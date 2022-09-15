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


#ifndef BRPC_IPC_SERVICE_H
#define BRPC_IPC_SERVICE_H

#include "brpc/controller.h"                 // Controller
#include "brpc/ipc_message.h"             // IpcMessage
#include "brpc/describable.h"


namespace brpc {

class Server;
class MethodStatus;
class StatusService;
namespace policy {
void ProcessIpcRequest(InputMessageBase* msg_base);
}

// The continuation of request processing. Namely send response back to client.
class IpcClosure : public google::protobuf::Closure {
public:
    explicit IpcClosure();
    ~IpcClosure();

    // [Required] Call this to send response back to the client.
    void Run() override;

    // Suspend/resume calling DoRun().
    void SuspendRunning();
    void ResumeRunning();

    Controller& controller();

public:
    void DoRun();
friend void ProcessIpcRequest(InputMessageBase* msg_base);

    butil::atomic<int> _run_counter;
    int64_t _received_us;
    IpcMessage _request;
    IpcMessage _response;
    Controller _controller;
};

// Inherit this class to let brpc server understands nshead requests.
class IpcService : public Describable {
public:
    IpcService();
    virtual ~IpcService();

    // Implement this method to handle nshead requests. Notice that this
    // method can be called with a failed Controller(something wrong with the
    // request before calling this method), in which case the implemenetation
    // shall send specific response with error information back to client.
    // Parameters:
    //   server      The server receiving the request.
    //   controller  per-rpc settings.
    //   request     The nshead request received.
    //   response    The nshead response that you should fill in.
    //   done        You must call done->Run() to end the processing.
    virtual void ProcessIpcRequest(Controller* controller,
                                      IpcMessage* request,
                                      IpcMessage* response,
                                      IpcClosure* done);

    // Put descriptions into the stream.
    void Describe(std::ostream &os, const DescribeOptions&) const;

private:
DISALLOW_COPY_AND_ASSIGN(IpcService);
friend class IpcClosure;
friend void policy::ProcessIpcRequest(InputMessageBase* msg_base);
friend class StatusService;
friend class Server;

private:
    void Expose(const butil::StringPiece& prefix);
    
    // Tracking status of non IpcPbService
    MethodStatus* _status;
    size_t _additional_space;
    std::string _cached_name;
};

} // namespace brpc


#endif // BRPC_IPC_SERVICE_H
