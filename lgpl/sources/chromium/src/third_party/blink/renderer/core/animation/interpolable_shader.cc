//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "third_party/blink/renderer/core/animation/interpolable_shader.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_shader_value.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

InterpolableShader::InterpolableShader(AtomicString relative_url,
                                       AtomicString absolute_url,
                                       const Referrer& referrer,
                                       GpuShaderResource* resource,
                                       InterpolableList* args,
                                       float animation_frame)
    : referrer_(referrer),
      relative_url_(relative_url),
      absolute_url_(absolute_url),
      resource_(resource),
      args_(std::move(args)),
      animation_frame_(
          MakeGarbageCollected<InterpolableNumber>(animation_frame)) {}

InterpolableShader::~InterpolableShader() = default;

CSSValueList* InterpolableShader::CreateArgsList() const {
  auto* list = CSSValueList::CreateSpaceSeparated();
  for (wtf_size_t i = 0; i < args_->length(); i++) {
    list->Append(*CSSNumericLiteralValue::Create(
        To<InterpolableNumber>(args_->Get(i))->Value(),
        CSSPrimitiveValue::UnitType::kNumber));
  }

  return list;
}

// static
InterpolableShader* InterpolableShader::CreateNeutral() {
  return MakeGarbageCollected<InterpolableShader>(
      AtomicString(), AtomicString(), Referrer(), nullptr,
      MakeGarbageCollected<InterpolableList>(0), 0);
}

// static
InterpolableShader* InterpolableShader::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* shader = DynamicTo<CSSShaderValue>(value);
  if (!shader)
    return nullptr;

  const auto* args = shader->Args();
  auto* list = MakeGarbageCollected<InterpolableList>(args->length());

  for (wtf_size_t i = 0; i < args->length(); i++) {
    const auto* arg_value = DynamicTo<CSSPrimitiveValue>(args->Item(i));
    DCHECK(arg_value && arg_value->IsNumber());
    list->Set(i, MakeGarbageCollected<InterpolableNumber>(
                     arg_value->GetDoubleValue()));
  }

  return MakeGarbageCollected<InterpolableShader>(
      shader->RelativeUrl(), shader->AbsoluteUrl(), shader->GetReferrer(),
      shader->Resource(), std::move(list), shader->AnimationFrame());
}

InterpolableShader* InterpolableShader::RawClone() const {
  return MakeGarbageCollected<InterpolableShader>(
      relative_url_, absolute_url_, referrer_, resource_, args_->Clone(),
      animation_frame_->Value());
}

InterpolableShader* InterpolableShader::RawCloneAndZero() const {
  return MakeGarbageCollected<InterpolableShader>(relative_url_, absolute_url_,
                                                  referrer_, resource_,
                                                  args_->CloneAndZero(), 0);
}

void InterpolableShader::Scale(double scale) {
  args_->Scale(scale);
  animation_frame_->Scale(scale);
}

void InterpolableShader::Add(const InterpolableValue& other) {
  const InterpolableShader& other_shader = To<InterpolableShader>(other);
  args_->Add(*other_shader.args_);
  animation_frame_->Add(*other_shader.animation_frame_);
}

void InterpolableShader::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableShader& other_shader = To<InterpolableShader>(other);
  args_->AssertCanInterpolateWith(*other_shader.args_);
  animation_frame_->AssertCanInterpolateWith(*other_shader.animation_frame_);
}

void InterpolableShader::Trace(Visitor* v) const {
  InterpolableValue::Trace(v);
  v->Trace(resource_);
  v->Trace(args_);
  v->Trace(animation_frame_);
}

bool InterpolableShader::IsCompatibleWith(
    const InterpolableShader& other) const {
  return args_->length() == other.args_->length();
}

void InterpolableShader::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const InterpolableShader& to_shader = To<InterpolableShader>(to);
  InterpolableShader& result_shader = To<InterpolableShader>(result);

  args_->Interpolate(*to_shader.args_, progress, *result_shader.args_);
  animation_frame_->Interpolate(*to_shader.animation_frame_, progress,
                                *result_shader.animation_frame_);
}

}  // namespace blink
