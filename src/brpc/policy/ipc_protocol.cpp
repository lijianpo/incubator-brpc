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

#include <google/protobuf/descriptor.h>         // MethodDescriptor
#include <google/protobuf/message.h>            // Message
#include <gflags/gflags.h>

#include "butil/time.h" 
#include "butil/iobuf.h"                         // butil::IOBuf

#include "brpc/controller.h"               // Controller
#include "brpc/socket.h"                   // Socket
#include "brpc/server.h"                   // Server
#include "brpc/span.h"
#include "brpc/details/server_private_accessor.h"
#include "brpc/details/controller_private_accessor.h"
#include "brpc/policy/most_common_message.h"
#include "brpc/details/usercode_backup_pool.h"
#include "brpc/policy/ipc_protocol.h"
#include "brpc/ipc_message.h"
#include "brpc/ipc_service.h"

namespace brpc {
namespace policy {

ParseResult ParseIpcMessage(
        butil::IOBuf* source,
        Socket*, 
        bool /*read_eof*/, 
        const void* /*arg*/) {

    IpcHead head;
    const size_t n = source->copy_to((char *)&head, sizeof(head));
    if (n < sizeof(head)) {
        return MakeParseError(PARSE_ERROR_NOT_ENOUGH_DATA);
    }

    uint32_t body_len = head.body_len;
    if (body_len > FLAGS_max_body_size) {
        return MakeParseError(PARSE_ERROR_TOO_BIG_DATA);
    } else if (source->length() < sizeof(head) + body_len) {
        return MakeParseError(PARSE_ERROR_NOT_ENOUGH_DATA);
    }

    policy::MostCommonMessage* msg = policy::MostCommonMessage::Get();
    source->cutn(&msg->meta, sizeof(head));
    source->cutn(&msg->payload, body_len);
    return MakeMessage(msg);
}

void SerializeIpcRequest(
        butil::IOBuf* request_buf, 
        Controller* cntl,
        const google::protobuf::Message* req_base) {

    if (req_base == NULL) {
        return cntl->SetFailed(EREQUEST, "request is NULL");
    }
    ControllerPrivateAccessor accessor(cntl);
    if (req_base->GetDescriptor() != IpcMessage::descriptor()) {
        return cntl->SetFailed(EINVAL, "Type of request must be IpcMessage");
    }
    if (cntl->response() != NULL &&
        cntl->response()->GetDescriptor() != IpcMessage::descriptor()) {
        return cntl->SetFailed(EINVAL, "Type of response must be IpcMessage");
    }
    const IpcMessage* req = (const IpcMessage*)req_base;

    IpcHead head = req->head;
    head.magic_num = 14694;
    head.body_len = req->body.size();

    request_buf->append(&head, sizeof(head));
    request_buf->append(req->body);
}

void PackIpcRequest(butil::IOBuf* packet_buf,
                    SocketMessage**,
                    uint64_t correlation_id,
                    const google::protobuf::MethodDescriptor* method,
                    Controller* cntl,
                    const butil::IOBuf& request,
                    const Authenticator* auth) {

    ControllerPrivateAccessor accessor(cntl);
    if (cntl->connection_type() == CONNECTION_TYPE_SINGLE) {
        return cntl->SetFailed(
            EINVAL, "ipc protocol can't work with CONNECTION_TYPE_SINGLE");
    }

    accessor.get_sending_socket()->set_correlation_id(correlation_id);
    Span* span = accessor.span();
    if (span) {
        span->set_request_size(request.length());
    }
    
    if (auth != NULL) {
        std::string auth_str;
        auth->GenerateCredential(&auth_str);
        //means first request in this connect, need to special head
        packet_buf->append(auth_str);
    }

    packet_buf->append(request);
}

inline void ProcessIpcRequestNoExcept(IpcService* service,
                                                   Controller* cntl,
                                                   IpcMessage* req,
                                                   IpcMessage* res,
                                                   IpcClosure* done) {
    service->ProcessIpcRequest(cntl, req, res, done);
    return;

    // NOTE: done is not actually run before ResumeRunning() is called so that
    // we can still set `cntl' in the catch branch.
    LOG(ERROR)<< "ProcessIpcRequestNoExcept in";
    done->SuspendRunning();
    try {
        service->ProcessIpcRequest(cntl, req, res, NULL);
    } catch (std::exception& e) {
        cntl->SetFailed(EINTERNAL, "Catched exception: %s", e.what());
    } catch (std::string& e) {
        cntl->SetFailed(EINTERNAL, "Catched std::string: %s", e.c_str());
    } catch (const char* e) {
        cntl->SetFailed(EINTERNAL, "Catched const char*: %s", e);
    } catch (...) {
        cntl->SetFailed(EINTERNAL, "Catched unknown exception");
    }
    done->ResumeRunning();
    LOG(ERROR)<< "ProcessIpcRequestNoExcept out";
}

struct CallMethodInBackupThreadArgs {
    IpcService* service;
    Controller* controller;
    IpcMessage* request;
    IpcMessage* response;
    IpcClosure* done;
};

static void CallMethodInBackupThread(void* void_args) {
    CallMethodInBackupThreadArgs* args = (CallMethodInBackupThreadArgs*)void_args;
    ProcessIpcRequestNoExcept(args->service,
                              args->controller,
                              args->request,
                              args->response,
                              args->done);
    delete args;
}

static void EndRunningCallMethodInPool(IpcService* service,
                                       Controller* controller,
                                       IpcMessage* request,
                                       IpcMessage* response,
                                       IpcClosure* done) {
    CallMethodInBackupThreadArgs* args = new CallMethodInBackupThreadArgs;
    args->service = service;
    args->controller = controller;
    args->request = request;
    args->response = response;
    args->done = done;
    return EndRunningUserCodeInPool(CallMethodInBackupThread, args);
};

void ProcessIpcRequest(InputMessageBase* msg_base) {
    const int64_t start_parse_us = butil::cpuwide_time_us();
    DestroyingPtr<MostCommonMessage> msg(static_cast<MostCommonMessage*>(msg_base));
    SocketUniquePtr socket_guard(msg->ReleaseSocket());
    Socket* socket = socket_guard.get();
    const Server* server = static_cast<const Server*>(msg_base->arg());
    ScopedNonServiceError non_service_error(server);
    
    //ControllerPrivateAccessor accessor(cntl);
    //Span* span = accessor.span();
    //if (span) {
    //    span->set_base_real_us(msg->base_real_us());
    //    span->set_received_us(msg->received_us());
    //    span->set_response_size(msg->payload.length());
    //    span->set_start_parse_us(start_parse_us);
    //}

    IpcClosure* ipc_done = new IpcClosure;
    //ClosureGuard done_guard(ipc_done);

    Controller* cntl = &(ipc_done->controller());
    IpcMessage* request = &(ipc_done->_request);
    IpcMessage* response = &(ipc_done->_response);
    ipc_done->_received_us = msg->received_us();

    ServerPrivateAccessor server_accessor(server);
    const bool security_mode = server->options().security_mode() &&
                               socket->user() == server_accessor.acceptor();

    ControllerPrivateAccessor accessor(cntl);

    accessor.set_server(server)
        .set_security_mode(security_mode)
        .set_peer_id(socket->id())
        .set_remote_side(socket->remote_side())
        .set_local_side(socket->local_side())
        .set_request_protocol(PROTOCOL_IPC)
        .set_begin_time_us(msg->received_us())
        .move_in_server_receiving_sock(socket_guard);

    IpcService* service = server->options().ipc_service;
    if (service == NULL) {
        LOG_EVERY_SECOND(ERROR)
            << "Received ipc request however the server does not set"
            " ServerOptions.ipc_service, close the connection.";
        return cntl->SetFailed(EINTERNAL, "ServerOptions.ipc_service is NULL");
    }

    // Switch to service-specific error.
    non_service_error.release();

    MethodStatus* method_status = service->_status;
    ipc_done->_received_us = msg->received_us();

    if (method_status) {
        if (!method_status->OnRequested()) {
            return cntl->SetFailed(ELIMIT, "Reached %s's max_concurrency=%d",
                   "IPC STATUS",
                    method_status->MaxConcurrency());
        }
    }

    //ServerPrivateAccessor server_accessor(server);
    //const Server::MethodProperty *sp =
    //         server_accessor.FindMethodPropertyByFullName("kuwo.RpcService");

    if (request!= NULL) {
        msg->meta.copy_to(&request->head, sizeof(IpcHead));
        msg->payload.swap(request->body);
    } // else just ignore the response.

    if (!server->IsRunning()) {
        return cntl->SetFailed(ELOGOFF, "Server is stopping");
    }

    // FIXME: enable to debug
    //EndRunningCallMethodInPool(service, cntl, request, response, ipc_done);
    //return;
    //

    //google::protobuf::Service* svc = sp->service;
    service->ProcessIpcRequest(cntl, request, response, NULL);

    // Unlocks correlation_id inside. Revert controller's
    // error code if it version check of `cid' fails
    //msg.reset();  // optional, just release resource ASAP

    //accessor.OnResponse(cid, saved_error);
    ipc_done->Run();
}

bool VerifyIpcRequest(const InputMessageBase* msg_base) {
    LOG(ERROR) << "VerifyIpcRequest";
    const MostCommonMessage* msg =
            static_cast<const MostCommonMessage*>(msg_base);
    const Server* server = static_cast<const Server*>(msg->arg());
    Socket* socket = msg->socket();

    const CallId cid = { static_cast<uint64_t>(msg->socket()->correlation_id()) };
    Controller* cntl = NULL;
    const int rc = bthread_id_lock(cid, (void**)&cntl);
    if (rc != 0) {
        LOG_IF(ERROR, rc != EINVAL && rc != EPERM)
            << "Fail to lock correlation_id=" << cid << ", " << berror(rc);
        return false;
    }

    ControllerPrivateAccessor accessor(cntl);
    Span* span = accessor.span();
    if (span) {
        span->set_base_real_us(msg->base_real_us());
        span->set_received_us(msg->received_us());
        span->set_response_size(msg->payload.length());
        //span->set_start_parse_us(start_parse_us);
    }

    // MUST be IpcMessage (checked in SerializeIpcRequest)
    IpcHead head;
    msg->meta.copy_to(&head, sizeof(IpcHead));
    LOG(ERROR) << "ProcessIpcRequest magic_num: " << head.magic_num;
    if (head.magic_num != 14694) {
        return false;
    }

    const Authenticator* auth = server->options().auth;
    if (NULL == auth) {
        // Fast pass (no authentication)
        return true;
    }

    //if (auth->VerifyCredential(
    //            meta.authentication_data(), socket->remote_side(),
    //            socket->mutable_auth_context()) != 0) {
    //    return false;
    //}
    return true;
}

void ProcessIpcResponse(InputMessageBase* msg_base) {
    const int64_t start_parse_us = butil::cpuwide_time_us();
    DestroyingPtr<MostCommonMessage> msg(static_cast<MostCommonMessage*>(msg_base));
    
    // Fetch correlation id that we saved before in `PackIpcRequest'
    const CallId cid = { static_cast<uint64_t>(msg->socket()->correlation_id()) };
    Controller* cntl = NULL;
    const int rc = bthread_id_lock(cid, (void**)&cntl);
    if (rc != 0) {
        LOG_IF(ERROR, rc != EINVAL && rc != EPERM)
            << "Fail to lock correlation_id=" << cid << ", " << berror(rc);
        return;
    }

    ControllerPrivateAccessor accessor(cntl);
    Span* span = accessor.span();
    if (span) {
        span->set_base_real_us(msg->base_real_us());
        span->set_received_us(msg->received_us());
        span->set_response_size(msg->payload.length());
        span->set_start_parse_us(start_parse_us);
    }
    // MUST be IpcMessage (checked in SerializeIpcRequest)
    IpcMessage* response = (IpcMessage*)cntl->response();
    const int saved_error = cntl->ErrorCode();

    if (response != NULL) {
        msg->meta.copy_to(&response->head, sizeof(IpcHead));
        msg->payload.swap(response->body);

        if (response->head.magic_num != 14694) {
            cntl->SetFailed(ENOENT, "ipc response head magic_num != 14694");
            LOG(WARNING) << "Server " << msg->socket()->remote_side() << " doesn't contain the right data";
        }
    } // else just ignore the response.

    // Unlocks correlation_id inside. Revert controller's
    // error code if it version check of `cid' fails
    //msg.reset();  // optional, just release resource ASAP
    accessor.OnResponse(cid, saved_error);
}

} // namespace policy
} // namespace brpc
