// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/interface.h.tmpl

// clang-format off
#ifndef V8DataView_h
#define V8DataView_h

#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/tests/idls/core/TestDataView.h"
#include "core/CoreExport.h"
#include "core/dom/ArrayBufferViewHelpers.h"
#include "core/dom/FlexibleArrayBufferView.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/Handle.h"

namespace blink {

class V8DataView {
  STATIC_ONLY(V8DataView);
 public:
  CORE_EXPORT static TestDataView* toImpl(v8::Local<v8::Object> object);
  CORE_EXPORT static TestDataView* toImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
  CORE_EXPORT static const WrapperTypeInfo wrapperTypeInfo;
  static void Trace(Visitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->Trace(scriptWrappable->ToImpl<TestDataView>());
  }
  static void TraceWrappers(WrapperVisitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->TraceWrappers(scriptWrappable->ToImpl<TestDataView>());
  }
  static const int internalFieldCount = kV8DefaultWrapperInternalFieldCount + 0;

  // Callback functions

  CORE_EXPORT static void getUint8MethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void getFloat64MethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void setUint8MethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void setFloat64MethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
};

template <>
struct NativeValueTraits<TestDataView> : public NativeValueTraitsBase<TestDataView> {
  CORE_EXPORT static TestDataView* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestDataView> {
  typedef V8DataView Type;
};

}  // namespace blink

#endif  // V8DataView_h
