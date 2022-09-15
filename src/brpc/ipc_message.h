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

#ifndef BRPC_IPC_MESSAGE_H
#define BRPC_IPC_MESSAGE_H

#include <string>

#include <google/protobuf/message.h>
#include <google/protobuf/generated_message_reflection.h>   // dynamic_cast_if_available
#include <google/protobuf/reflection_ops.h>     // ReflectionOps::Merge

#include "brpc/ipc_head.h"
#include "butil/iobuf.h"       
#include "brpc/proto_base.pb.h"

namespace brpc {

class IpcMessage : public ::google::protobuf::Message {
public:
    IpcHead head;
    butil::IOBuf body;

public:
    IpcMessage();
    virtual ~IpcMessage();

    IpcMessage(const IpcMessage& from);

    inline IpcMessage& operator=(const IpcMessage& from) {
        CopyFrom(from);
        return *this;
    }

    static const ::google::protobuf::Descriptor* descriptor();
    static const IpcMessage& default_instance();

    void Swap(IpcMessage* other);

    // implements Message ----------------------------------------------

    IpcMessage* New() const;
#if GOOGLE_PROTOBUF_VERSION >= 3006000
    IpcMessage* New(::google::protobuf::Arena* arena) const override;
#endif
    void CopyFrom(const ::google::protobuf::Message& from);
    void MergeFrom(const ::google::protobuf::Message& from);
    void CopyFrom(const IpcMessage& from);
    void MergeFrom(const IpcMessage& from);
    void Clear();
    bool IsInitialized() const;

    int ByteSize() const;
    bool MergePartialFromCodedStream(
            ::google::protobuf::io::CodedInputStream* input);
    void SerializeWithCachedSizes(
            ::google::protobuf::io::CodedOutputStream* output) const;
    ::google::protobuf::uint8* SerializeWithCachedSizesToArray(
            ::google::protobuf::uint8* output) const;
    int GetCachedSize() const { return ByteSize(); }

protected:
    ::google::protobuf::Metadata GetMetadata() const override;

private:
    void SharedCtor();
    void SharedDtor();
};

} // namespace brpc

#endif  // BRPC_ESP_MESSAGE_H
