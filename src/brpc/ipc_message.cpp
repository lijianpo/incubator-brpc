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

#include "ipc_message.h"

#include <google/protobuf/reflection_ops.h>     // ReflectionOps::Merge
#include <google/protobuf/wire_format.h>        // WireFormatLite::GetTagWireType

namespace brpc {

IpcMessage::IpcMessage()
    : ::google::protobuf::Message() {
    SharedCtor();
}

IpcMessage::IpcMessage(const IpcMessage& from)
    : ::google::protobuf::Message() {
    SharedCtor();
    MergeFrom(from);
}

void IpcMessage::SharedCtor() {
    memset(&head, 0, sizeof(head));
}

IpcMessage::~IpcMessage() {
    SharedDtor();
}

void IpcMessage::SharedDtor() {
}

const ::google::protobuf::Descriptor* IpcMessage::descriptor() {
    return IpcMessageBase::descriptor();
}

IpcMessage* IpcMessage::New() const {
    return new IpcMessage;
}

#if GOOGLE_PROTOBUF_VERSION >= 3006000
IpcMessage* IpcMessage::New(::google::protobuf::Arena* arena) const {
    return CreateMaybeMessage<IpcMessage>(arena);
}
#endif

void IpcMessage::Clear() {
    head.body_len = 0;
    body.clear();
}

bool IpcMessage::MergePartialFromCodedStream(
        ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!(EXPRESSION)) return false
    ::google::protobuf::uint32 tag;

    while ((tag = input->ReadTag()) != 0) {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
                ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
            return true;
        }
    }
    return true;
#undef DO_
}

void IpcMessage::SerializeWithCachedSizes(
        ::google::protobuf::io::CodedOutputStream*) const {
}

::google::protobuf::uint8* IpcMessage::SerializeWithCachedSizesToArray(
        ::google::protobuf::uint8* target) const {
    return target;
}

int IpcMessage::ByteSize() const {
    return sizeof(head) + body.size();
}

void IpcMessage::MergeFrom(const ::google::protobuf::Message& from) {
    GOOGLE_CHECK_NE(&from, this);
    const IpcMessage* source = dynamic_cast<const IpcMessage*>(&from);
    if (source == NULL) {
        ::google::protobuf::internal::ReflectionOps::Merge(from, this);
    } else {
        MergeFrom(*source);
    }
}

void IpcMessage::MergeFrom(const IpcMessage& from) {
    GOOGLE_CHECK_NE(&from, this);
    head = from.head;
    body = from.body;
}

void IpcMessage::CopyFrom(const ::google::protobuf::Message& from) {
    if (&from == this) {
        return;
    }

    Clear();
    MergeFrom(from);
}

void IpcMessage::CopyFrom(const IpcMessage& from) {
    if (&from == this) {
        return;
    }

    Clear();
    MergeFrom(from);
}

bool IpcMessage::IsInitialized() const {
    return true;
}

void IpcMessage::Swap(IpcMessage* other) {
    if (other != this) {
        const IpcHead tmp = other->head;
        other->head = head;
        head = tmp;
        body.swap(other->body);
    }
}

::google::protobuf::Metadata IpcMessage::GetMetadata() const {
    ::google::protobuf::Metadata metadata;
    metadata.descriptor = IpcMessage::descriptor();
    metadata.reflection = NULL;
    return metadata;
}

} // namespace brpc
