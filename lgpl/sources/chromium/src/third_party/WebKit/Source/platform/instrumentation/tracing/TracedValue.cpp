// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/instrumentation/tracing/TracedValue.h"

#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"

namespace blink {

std::unique_ptr<TracedValue> TracedValue::Create() {
  return WTF::WrapUnique(new TracedValue());
}

TracedValue::TracedValue() {}

TracedValue::~TracedValue() {}

void TracedValue::SetInteger(const char* name, int value) {
  traced_value_.SetIntegerWithCopiedName(name, value);
}

void TracedValue::SetDouble(const char* name, double value) {
  traced_value_.SetDoubleWithCopiedName(name, value);
}

void TracedValue::SetBoolean(const char* name, bool value) {
  traced_value_.SetBooleanWithCopiedName(name, value);
}

void TracedValue::SetString(const char* name, const String& value) {
  StringUTF8Adaptor adaptor(value);
  traced_value_.SetStringWithCopiedName(name, adaptor.AsStringPiece());
}

void TracedValue::BeginDictionary(const char* name) {
  traced_value_.BeginDictionaryWithCopiedName(name);
}

void TracedValue::BeginArray(const char* name) {
  traced_value_.BeginArrayWithCopiedName(name);
}

void TracedValue::EndDictionary() {
  traced_value_.EndDictionary();
}

void TracedValue::PushInteger(int value) {
  traced_value_.AppendInteger(value);
}

void TracedValue::PushDouble(double value) {
  traced_value_.AppendDouble(value);
}

void TracedValue::PushBoolean(bool value) {
  traced_value_.AppendBoolean(value);
}

void TracedValue::PushString(const String& value) {
  StringUTF8Adaptor adaptor(value);
  traced_value_.AppendString(adaptor.AsStringPiece());
}

void TracedValue::BeginArray() {
  traced_value_.BeginArray();
}

void TracedValue::BeginDictionary() {
  traced_value_.BeginDictionary();
}

void TracedValue::EndArray() {
  traced_value_.EndArray();
}

String TracedValue::ToString() const {
  return String(traced_value_.ToString().c_str());
}

void TracedValue::AppendAsTraceFormat(std::string* out) const {
  traced_value_.AppendAsTraceFormat(out);
}

void TracedValue::EstimateTraceMemoryOverhead(
    base::trace_event::TraceEventMemoryOverhead* overhead) {
  traced_value_.EstimateTraceMemoryOverhead(overhead);
}

}  // namespace blink
