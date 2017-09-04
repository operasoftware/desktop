// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/union_container.cpp.tmpl

// clang-format off
#include "ByteStringSequenceSequenceOrByteStringByteStringRecord.h"

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ToV8ForCore.h"

namespace blink {

ByteStringSequenceSequenceOrByteStringByteStringRecord::ByteStringSequenceSequenceOrByteStringByteStringRecord() : m_type(SpecificTypeNone) {}

const Vector<std::pair<String, String>>& ByteStringSequenceSequenceOrByteStringByteStringRecord::getAsByteStringByteStringRecord() const {
  DCHECK(isByteStringByteStringRecord());
  return m_byteStringByteStringRecord;
}

void ByteStringSequenceSequenceOrByteStringByteStringRecord::setByteStringByteStringRecord(const Vector<std::pair<String, String>>& value) {
  DCHECK(isNull());
  m_byteStringByteStringRecord = value;
  m_type = SpecificTypeByteStringByteStringRecord;
}

ByteStringSequenceSequenceOrByteStringByteStringRecord ByteStringSequenceSequenceOrByteStringByteStringRecord::fromByteStringByteStringRecord(const Vector<std::pair<String, String>>& value) {
  ByteStringSequenceSequenceOrByteStringByteStringRecord container;
  container.setByteStringByteStringRecord(value);
  return container;
}

const Vector<Vector<String>>& ByteStringSequenceSequenceOrByteStringByteStringRecord::getAsByteStringSequenceSequence() const {
  DCHECK(isByteStringSequenceSequence());
  return m_byteStringSequenceSequence;
}

void ByteStringSequenceSequenceOrByteStringByteStringRecord::setByteStringSequenceSequence(const Vector<Vector<String>>& value) {
  DCHECK(isNull());
  m_byteStringSequenceSequence = value;
  m_type = SpecificTypeByteStringSequenceSequence;
}

ByteStringSequenceSequenceOrByteStringByteStringRecord ByteStringSequenceSequenceOrByteStringByteStringRecord::fromByteStringSequenceSequence(const Vector<Vector<String>>& value) {
  ByteStringSequenceSequenceOrByteStringByteStringRecord container;
  container.setByteStringSequenceSequence(value);
  return container;
}

ByteStringSequenceSequenceOrByteStringByteStringRecord::ByteStringSequenceSequenceOrByteStringByteStringRecord(const ByteStringSequenceSequenceOrByteStringByteStringRecord&) = default;
ByteStringSequenceSequenceOrByteStringByteStringRecord::~ByteStringSequenceSequenceOrByteStringByteStringRecord() = default;
ByteStringSequenceSequenceOrByteStringByteStringRecord& ByteStringSequenceSequenceOrByteStringByteStringRecord::operator=(const ByteStringSequenceSequenceOrByteStringByteStringRecord&) = default;

DEFINE_TRACE(ByteStringSequenceSequenceOrByteStringByteStringRecord) {
}

void V8ByteStringSequenceSequenceOrByteStringByteStringRecord::toImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, ByteStringSequenceSequenceOrByteStringByteStringRecord& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<Vector<String>> cppValue = NativeValueTraits<IDLSequence<IDLSequence<IDLByteString>>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setByteStringSequenceSequence(cppValue);
    return;
  }

  if (v8Value->IsObject()) {
    Vector<std::pair<String, String>> cppValue = NativeValueTraits<IDLRecord<IDLByteString, IDLByteString>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setByteStringByteStringRecord(cppValue);
    return;
  }

  exceptionState.ThrowTypeError("The provided value is not of type '(sequence<sequence<ByteString>> or record<ByteString, ByteString>)'");
}

v8::Local<v8::Value> ToV8(const ByteStringSequenceSequenceOrByteStringByteStringRecord& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.m_type) {
    case ByteStringSequenceSequenceOrByteStringByteStringRecord::SpecificTypeNone:
      return v8::Null(isolate);
    case ByteStringSequenceSequenceOrByteStringByteStringRecord::SpecificTypeByteStringByteStringRecord:
      return ToV8(impl.getAsByteStringByteStringRecord(), creationContext, isolate);
    case ByteStringSequenceSequenceOrByteStringByteStringRecord::SpecificTypeByteStringSequenceSequence:
      return ToV8(impl.getAsByteStringSequenceSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

ByteStringSequenceSequenceOrByteStringByteStringRecord NativeValueTraits<ByteStringSequenceSequenceOrByteStringByteStringRecord>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  ByteStringSequenceSequenceOrByteStringByteStringRecord impl;
  V8ByteStringSequenceSequenceOrByteStringByteStringRecord::toImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
