// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl

// clang-format off
#ifndef NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord_h
#define NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ByteStringOrNodeList;
class Event;
class Node;
class XMLHttpRequest;

class CORE_EXPORT NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord();
  bool isNull() const { return m_type == SpecificTypeNone; }

  bool isEvent() const { return m_type == SpecificTypeEvent; }
  Event* getAsEvent() const;
  void setEvent(Event*);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord fromEvent(Event*);

  bool isLongSequence() const { return m_type == SpecificTypeLongSequence; }
  const Vector<int32_t>& getAsLongSequence() const;
  void setLongSequence(const Vector<int32_t>&);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord fromLongSequence(const Vector<int32_t>&);

  bool isNode() const { return m_type == SpecificTypeNode; }
  Node* getAsNode() const;
  void setNode(Node*);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord fromNode(Node*);

  bool isString() const { return m_type == SpecificTypeString; }
  const String& getAsString() const;
  void setString(const String&);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord fromString(const String&);

  bool isStringByteStringOrNodeListRecord() const { return m_type == SpecificTypeStringByteStringOrNodeListRecord; }
  const HeapVector<std::pair<String, ByteStringOrNodeList>>& getAsStringByteStringOrNodeListRecord() const;
  void setStringByteStringOrNodeListRecord(const HeapVector<std::pair<String, ByteStringOrNodeList>>&);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord fromStringByteStringOrNodeListRecord(const HeapVector<std::pair<String, ByteStringOrNodeList>>&);

  bool isXMLHttpRequest() const { return m_type == SpecificTypeXMLHttpRequest; }
  XMLHttpRequest* getAsXMLHttpRequest() const;
  void setXMLHttpRequest(XMLHttpRequest*);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord fromXMLHttpRequest(XMLHttpRequest*);

  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&);
  ~NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord();
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& operator=(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&);
  DECLARE_TRACE();

 private:
  enum SpecificTypes {
    SpecificTypeNone,
    SpecificTypeEvent,
    SpecificTypeLongSequence,
    SpecificTypeNode,
    SpecificTypeString,
    SpecificTypeStringByteStringOrNodeListRecord,
    SpecificTypeXMLHttpRequest,
  };
  SpecificTypes m_type;

  Member<Event> m_event;
  Vector<int32_t> m_longSequence;
  Member<Node> m_node;
  String m_string;
  HeapVector<std::pair<String, ByteStringOrNodeList>> m_stringByteStringOrNodeListRecord;
  Member<XMLHttpRequest> m_xmlHttpRequest;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord final {
 public:
  CORE_EXPORT static void toImpl(v8::Isolate*, v8::Local<v8::Value>, NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord> : public NativeValueTraitsBase<NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord> {
  CORE_EXPORT static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord> {
  typedef V8NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord);

#endif  // NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord_h
