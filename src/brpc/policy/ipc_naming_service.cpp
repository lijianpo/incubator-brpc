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


#include <gflags/gflags.h>
#include <string>                                       // std::string
#include <set>                                          // std::set
#include "butil/string_printf.h"
#include "butil/third_party/rapidjson/document.h"
#include "butil/third_party/rapidjson/stringbuffer.h"
#include "butil/third_party/rapidjson/prettywriter.h"
#include "butil/time/time.h"
#include "bthread/bthread.h"
#include "brpc/log.h"
#include "brpc/channel.h"
#include "brpc/policy/file_naming_service.h"
#include "brpc/policy/ipc_naming_service.h"
#include "brpc/ipc_message.h"
#include "brpc/policy/rcm_protocol.h"


namespace brpc {
namespace policy {

DEFINE_string(ipc_center_addr, "http://10.1.5.11:18888",
              "The query string of request  for discovering service.");

DEFINE_int32(ipc_connect_timeout_ms, 200,
             "Timeout for creating connections to  in milliseconds");
DEFINE_int32(ipc_blocking_query_wait_secs, 60,
             "Maximum duration for the blocking request in secs.");
DEFINE_bool(ipc_enable_degrade_to_file_naming_service, false,
            "Use local backup file when  cannot connect");
DEFINE_string(ipc_file_naming_service_dir, "",
    "When it degraded to file naming service, the file with name of the "
    "service name will be searched in this dir to use.");

DEFINE_int32(ipc_retry_interval_ms, 1000000,
             "Wait so many milliseconds before retry when error happens");

DEFINE_int32(ipc_interval_ms, 3000000,
             "Wait so many milliseconds before retry when error happens");

int IpcNamingService::DegradeToOtherServiceIfNeeded(const char* service_name,
                                                       std::vector<ServerNode>* servers) {
    LOG(ERROR) << "IpcNamingService::DegradeToOtherServiceIfNeeded";
    if (FLAGS_ipc_enable_degrade_to_file_naming_service && !_backup_file_loaded) {
        _backup_file_loaded = true;
        const std::string file(FLAGS_ipc_file_naming_service_dir + service_name);
        LOG(INFO) << "Load server list from " << file;
        FileNamingService fns;
        return fns.GetServers(file.c_str(), servers);
    }
    return -1;
}

int IpcNamingService::GetServers(const char* service_name,
              std::vector<ServerNode>* servers) {
    brpc::ChannelOptions options;
    options.protocol = "ipc";
    options.connect_timeout_ms = FLAGS_ipc_connect_timeout_ms;
    options.timeout_ms = (FLAGS_ipc_blocking_query_wait_secs + 10) * butil::Time::kMillisecondsPerSecond;
    options.max_retry = 5;

    if (!_connected) {
        if (_channel.Init("10.1.5.11:18888", "", &options) != 0) {
            LOG(ERROR) << "Fail to init channel to  at " << FLAGS_ipc_center_addr;
            return DegradeToOtherServiceIfNeeded(service_name, servers);
        }
        _connected = true;
    }

    RcmProtocol rcm_request;
    rcm_request.set("query_cmd", "get_server_list_by_server_type_req");
    rcm_request.set("pid", 9999);
    rcm_request.set("ip", "10.1.29.2");
    rcm_request.set("remote_servers", service_name);
    rcm_request.set("model_file", "-");
    rcm_request.set("model_class", "-");

    std::string request_msg;
    rcm_request.toString(request_msg);

    brpc::IpcMessage request;
    brpc::IpcMessage response;
    request.head.body_len = request_msg.size();
    request.body = request_msg;

    brpc::Controller cntl;
    _channel.CallMethod(NULL, &cntl, &request, &response, NULL);

    servers->clear();
    if (cntl.Failed()) {
        LOG(ERROR) << "Fail to access " << service_name << ": " << cntl.ErrorText();
        return DegradeToOtherServiceIfNeeded(service_name, servers);
    }

    RcmProtocol rcm_response(STORAGE_TYPE_JSON, response.body.to_string());
    std::string str_value;
    rcm_response.get("query_cmd", str_value);
    if (str_value != "get_server_list_by_server_type_ret") {
        LOG(ERROR) << "get serves from register center failed.";
        return -1;
    }

    int items_size = rcm_response.get_items_size(); 
    for (int i = 0; i < items_size; ++i) {
        Item item = rcm_response.get_item(i);
        std::string ip = item.get("ip");
        int port = stol(item.get("port"));

        butil::EndPoint end_point;
        if (str2endpoint(ip.c_str(), port, &end_point) != 0) {
            LOG(ERROR) << "Service with illegal address or port: ";
            continue;
        }

        ServerNode node;
        node.addr = end_point;
        servers->push_back(node);
    }

    return 0;
}

int IpcNamingService::RunNamingService(const char* service_name,
                                          NamingServiceActions* actions) {
    std::vector<ServerNode> servers;
    bool ever_reset = false;
    for (;;) {
        servers.clear();
        const int rc = GetServers(service_name, &servers);
        if (bthread_stopped(bthread_self())) {
            RPC_VLOG << "Quit NamingServiceThread=" << bthread_self();
            return 0;
        }
        if (rc == 0) {
            ever_reset = true;
            actions->ResetServers(servers);
            bthread_usleep(FLAGS_ipc_interval_ms);
        } else {
            if (!ever_reset) {
                // ResetServers must be called at first time even if GetServers
                // failed, to wake up callers to `WaitForFirstBatchOfServers'
                ever_reset = true;
                servers.clear();
                actions->ResetServers(servers);
            }
	    LOG(ERROR)<< "usleep: " << std::max(FLAGS_ipc_retry_interval_ms, 1);

            if (bthread_usleep(std::max(FLAGS_ipc_retry_interval_ms, 1) * butil::Time::kMicrosecondsPerMillisecond) < 0) {
                if (errno == ESTOP) {
                    RPC_VLOG << "Quit NamingServiceThread=" << bthread_self();
                    return 0;
                }
                PLOG(FATAL) << "Fail to sleep";
                return -1;
            }
        }
    }
    CHECK(false);
    return -1;
}


void IpcNamingService::Describe(std::ostream& os,
                                   const DescribeOptions&) const {
    os << "ipc";
    return;
}

NamingService* IpcNamingService::New() const {
    return new IpcNamingService;
}

void IpcNamingService::Destroy() {
    delete this;
}

}  // namespace policy
} // namespace brpc
