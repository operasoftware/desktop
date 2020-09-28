// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/extendable_message_event.h"

namespace blink {

ExtendableMessageEvent* ExtendableMessageEvent::Create(
    const AtomicString& type,
    const ExtendableMessageEventInit* initializer) {
  return MakeGarbageCollected<ExtendableMessageEvent>(type, initializer);
}

ExtendableMessageEvent* ExtendableMessageEvent::Create(
    scoped_refptr<SerializedScriptValue> data,
    const String& origin,
    MessagePortArray* ports,
    ServiceWorkerClient* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event = MakeGarbageCollected<ExtendableMessageEvent>(
      std::move(data), origin, ports, observer);
  event->source_as_client_ = source;
  return event;
}

ExtendableMessageEvent* ExtendableMessageEvent::Create(
    scoped_refptr<SerializedScriptValue> data,
    const String& origin,
    MessagePortArray* ports,
    ServiceWorker* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event = MakeGarbageCollected<ExtendableMessageEvent>(
      std::move(data), origin, ports, observer);
  event->source_as_service_worker_ = source;
  return event;
}

ExtendableMessageEvent* ExtendableMessageEvent::CreateError(
    const String& origin,
    MessagePortArray* ports,
    ServiceWorkerClient* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event =
      MakeGarbageCollected<ExtendableMessageEvent>(origin, ports, observer);
  event->source_as_client_ = source;
  return event;
}

ExtendableMessageEvent* ExtendableMessageEvent::CreateError(
    const String& origin,
    MessagePortArray* ports,
    ServiceWorker* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event =
      MakeGarbageCollected<ExtendableMessageEvent>(origin, ports, observer);
  event->source_as_service_worker_ = source;
  return event;
}

ScriptValue ExtendableMessageEvent::data(ScriptState* script_state) const {
  v8::Local<v8::Value> value;
  if (!data_.IsEmpty()) {
    value = data_.GetAcrossWorld(script_state);
  } else if (serialized_data_) {
    SerializedScriptValue::DeserializeOptions options;
    MessagePortArray message_ports = ports();
    options.message_ports = &message_ports;
    value = serialized_data_->Deserialize(script_state->GetIsolate(), options);
  } else {
    value = v8::Null(script_state->GetIsolate());
  }
  return ScriptValue(script_state->GetIsolate(), value);
}

void ExtendableMessageEvent::source(
    ClientOrServiceWorkerOrMessagePort& result) const {
  if (source_as_client_)
    result = ClientOrServiceWorkerOrMessagePort::FromClient(source_as_client_);
  else if (source_as_service_worker_)
    result = ClientOrServiceWorkerOrMessagePort::FromServiceWorker(
        source_as_service_worker_);
  else if (source_as_message_port_)
    result = ClientOrServiceWorkerOrMessagePort::FromMessagePort(
        source_as_message_port_);
  else
    result = ClientOrServiceWorkerOrMessagePort();
}

MessagePortArray ExtendableMessageEvent::ports() const {
  // TODO(bashi): Currently we return a copied array because the binding
  // layer could modify the content of the array while executing JS callbacks.
  // Avoid copying once we can make sure that the binding layer won't
  // modify the content.
  if (ports_) {
    return *ports_;
  }
  return MessagePortArray();
}

const AtomicString& ExtendableMessageEvent::InterfaceName() const {
  return event_interface_names::kExtendableMessageEvent;
}

void ExtendableMessageEvent::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  visitor->Trace(source_as_client_);
  visitor->Trace(source_as_service_worker_);
  visitor->Trace(source_as_message_port_);
  visitor->Trace(ports_);
  ExtendableEvent::Trace(visitor);
}

ExtendableMessageEvent::ExtendableMessageEvent(
    const AtomicString& type,
    const ExtendableMessageEventInit* initializer)
    : ExtendableMessageEvent(type, initializer, nullptr) {}

ExtendableMessageEvent::ExtendableMessageEvent(
    const AtomicString& type,
    const ExtendableMessageEventInit* initializer,
    WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer) {
  if (initializer->hasData()) {
    const ScriptValue& data = initializer->data();
    data_.Set(data.GetIsolate(), data.V8Value());
  }
  if (initializer->hasOrigin())
    origin_ = initializer->origin();
  if (initializer->hasLastEventId())
    last_event_id_ = initializer->lastEventId();
  if (initializer->hasSource()) {
    if (initializer->source().IsClient())
      source_as_client_ = initializer->source().GetAsClient();
    else if (initializer->source().IsServiceWorker())
      source_as_service_worker_ = initializer->source().GetAsServiceWorker();
    else if (initializer->source().IsMessagePort())
      source_as_message_port_ = initializer->source().GetAsMessagePort();
  }
  if (initializer->hasPorts())
    ports_ = MakeGarbageCollected<MessagePortArray>(initializer->ports());
}

ExtendableMessageEvent::ExtendableMessageEvent(
    scoped_refptr<SerializedScriptValue> data,
    const String& origin,
    MessagePortArray* ports,
    WaitUntilObserver* observer)
    : ExtendableEvent(event_type_names::kMessage,
                      ExtendableMessageEventInit::Create(),
                      observer),
      serialized_data_(std::move(data)),
      origin_(origin),
      last_event_id_(String()),
      ports_(ports) {
  if (serialized_data_)
    serialized_data_->RegisterMemoryAllocatedWithCurrentScriptContext();
}

ExtendableMessageEvent::ExtendableMessageEvent(const String& origin,
                                               MessagePortArray* ports,
                                               WaitUntilObserver* observer)
    : ExtendableEvent(event_type_names::kMessageerror,
                      ExtendableMessageEventInit::Create(),
                      observer),
      origin_(origin),
      last_event_id_(String()),
      ports_(ports) {}

}  // namespace blink
