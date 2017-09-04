// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/interface.h.tmpl

// clang-format off
#ifndef V8TestTypedefs_h
#define V8TestTypedefs_h

#include "bindings/core/v8/ByteStringSequenceSequenceOrByteStringByteStringRecord.h"
#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord.h"
#include "bindings/core/v8/StringOrDouble.h"
#include "bindings/core/v8/TestInterfaceOrTestInterfaceEmpty.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/UnsignedLongLongOrBooleanOrTestCallbackInterface.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/tests/idls/core/TestTypedefs.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/Handle.h"

namespace blink {

class V8TestTypedefs {
  STATIC_ONLY(V8TestTypedefs);
 public:
  CORE_EXPORT static bool hasInstance(v8::Local<v8::Value>, v8::Isolate*);
  static v8::Local<v8::Object> findInstanceInPrototypeChain(v8::Local<v8::Value>, v8::Isolate*);
  CORE_EXPORT static v8::Local<v8::FunctionTemplate> domTemplate(v8::Isolate*, const DOMWrapperWorld&);
  static TestTypedefs* toImpl(v8::Local<v8::Object> object) {
    return ToScriptWrappable(object)->ToImpl<TestTypedefs>();
  }
  CORE_EXPORT static TestTypedefs* toImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
  CORE_EXPORT static const WrapperTypeInfo wrapperTypeInfo;
  static void Trace(Visitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->Trace(scriptWrappable->ToImpl<TestTypedefs>());
  }
  static void TraceWrappers(WrapperVisitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->TraceWrappers(scriptWrappable->ToImpl<TestTypedefs>());
  }
  static const int internalFieldCount = kV8DefaultWrapperInternalFieldCount + 0;

  // Callback functions
  CORE_EXPORT static void constructorCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  CORE_EXPORT static void uLongLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void uLongLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void domStringOrDoubleOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void domStringOrDoubleOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  CORE_EXPORT static void voidMethodArrayOfLongsArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void voidMethodFloatArgStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void voidMethodTestCallbackInterfaceTypeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void uLongLongMethodTestInterfaceEmptyTypeSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void testInterfaceOrTestInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void domStringOrDoubleMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void arrayOfStringsMethodArrayOfStringsArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void stringArrayMethodStringArrayArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void methodTakingRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void methodTakingOilpanValueRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void unionWithRecordMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void methodThatReturnsRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void voidMethodNestedUnionTypeMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void voidMethodUnionWithTypedefMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
};

template <>
struct NativeValueTraits<TestTypedefs> : public NativeValueTraitsBase<TestTypedefs> {
  CORE_EXPORT static TestTypedefs* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestTypedefs> {
  typedef V8TestTypedefs Type;
};

}  // namespace blink

#endif  // V8TestTypedefs_h
