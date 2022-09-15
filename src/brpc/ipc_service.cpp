// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.


#include "butil/class_name.h"
#include "brpc/ipc_service.h"
#include "brpc/details/method_status.h"
#include "brpc/details/server_private_accessor.h"
#include "brpc/details/controller_private_accessor.h"

namespace brpc {

IpcClosure::IpcClosure()
    : _run_counter(1), _received_us(0) {
}

IpcClosure::~IpcClosure() {
    LogErrorTextAndDelete(false)(&_controller);
}

void IpcClosure::SuspendRunning() {
    int x = _run_counter.fetch_add(1, butil::memory_order_relaxed);
    LOG(ERROR) << "SuspendRunning: _run_counter.fetch_add" << x;
}

void IpcClosure::ResumeRunning() {
    int x = _run_counter.fetch_sub(1, butil::memory_order_relaxed);
    LOG(ERROR) << "ResumeRunning: _run_counter.fetch_sub " << x;
    //if (_run_counter.fetch_sub(1, butil::memory_order_relaxed) == 1) {
    if (x == 1) {
        DoRun();
    }
}

Controller& IpcClosure::controller() {
    return _controller;
}

void IpcClosure::Run() {
    DoRun();
    return;
    //if (_run_counter.fetch_sub(1, butil::memory_order_relaxed) == 1) {
    int x = _run_counter.fetch_sub(1, butil::memory_order_relaxed);
    LOG(ERROR) << "Run() _run_counter.fetch_sub" << x;
    if (x == 1) {
        DoRun();
    }
}

void IpcClosure::DoRun() {
    std::unique_ptr<IpcClosure> recycle_ctx(this);
    const Server* server = _controller.server();
    ControllerPrivateAccessor accessor(&_controller);
    Span* span = accessor.span();
    //if (span) {
    //    span->set_start_send_us(butil::cpuwide_time_us());
    //}
    Socket* sock = accessor.get_sending_socket();
    if (sock == NULL) {
        LOG(ERROR) << "Fail to get sending socket.";
	return;
    }

    MethodStatus* method_status = server->options().ipc_service->_status;
    ConcurrencyRemover concurrency_remover(method_status, &_controller, _received_us);

    butil::IOBuf write_buf;
    write_buf.append(&_response.head, sizeof(IpcHead));
    write_buf.append(_response.body.movable());

    Socket::WriteOptions wopt;
    wopt.ignore_eovercrowded = true;
    if (sock->Write(&write_buf, &wopt) != 0) {
        const int errcode = errno;
        PLOG_IF(WARNING, errcode != EPIPE) << "Fail to write into " << *sock;
        _controller.SetFailed(errcode, "Fail to write into %s",
                              sock->description().c_str());
        return;
    }
    //if (span) {
        // TODO: this is not sent
     //   span->set_sent_us(butil::cpuwide_time_us());
    //}

}

IpcService::IpcService() : _additional_space(0) {
    _status = new (std::nothrow) MethodStatus;
    LOG_IF(FATAL, _status == NULL) << "Fail to new MethodStatus";
}

IpcService::~IpcService() {
    delete _status;
    _status = NULL;
}

void IpcService::ProcessIpcRequest(Controller* controller,
                                      IpcMessage* request,
                                      IpcMessage* response,
                                      IpcClosure* done) {
    brpc::ClosureGuard done_guard(done);

    LOG(ERROR) << "SHOULD implement IpcService::ProcessIpcRequest in child class.";

    butil::IOBufBuilder os;
    os << "{ \"query\": {}, \"msg\": { \"status\": \"ok\"}";
    os.move_to(response->body);
    response->head.magic_num = 14694;
    response->head.body_len = response->body.length();
}

void IpcService::Describe(std::ostream &os, const DescribeOptions&) const {
    os << butil::class_name_str(*this);
}

void IpcService::Expose(const butil::StringPiece& prefix) {
    _cached_name = butil::class_name_str(*this);
    if (_status == NULL) {
        return;
    }
    std::string s;
    s.reserve(prefix.size() + 1 + _cached_name.size());
    s.append(prefix.data(), prefix.size());
    s.push_back('_');
    s.append(_cached_name);
    _status->Expose(s);
}

} // namespace brpc

