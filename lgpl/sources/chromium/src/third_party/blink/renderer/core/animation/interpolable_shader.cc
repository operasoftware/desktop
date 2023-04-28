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
                                       std::unique_ptr<InterpolableList> args,
                                       float animation_frame)
    : referrer_(referrer),
      relative_url_(relative_url),
      absolute_url_(absolute_url),
      resource_(resource),
      args_(std::move(args)) {
  animation_frame_ = std::make_unique<InterpolableNumber>(animation_frame);
}

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
std::unique_ptr<InterpolableShader> InterpolableShader::CreateNeutral() {
  return std::make_unique<InterpolableShader>(
      AtomicString(), AtomicString(), Referrer(), nullptr,
      std::make_unique<InterpolableList>(0), 0);
}

// static
std::unique_ptr<InterpolableShader> InterpolableShader::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* shader = DynamicTo<CSSShaderValue>(value);
  if (!shader)
    return nullptr;

  const auto* args = shader->Args();
  auto list = std::make_unique<InterpolableList>(args->length());

  for (wtf_size_t i = 0; i < args->length(); i++) {
    const auto* arg_value = DynamicTo<CSSPrimitiveValue>(args->Item(i));
    DCHECK(arg_value && arg_value->IsNumber());
    list->Set(
        i, std::make_unique<InterpolableNumber>(arg_value->GetDoubleValue()));
  }

  return std::make_unique<InterpolableShader>(
      shader->RelativeUrl(), shader->AbsoluteUrl(), shader->GetReferrer(),
      shader->Resource(), std::move(list), shader->AnimationFrame());
}

InterpolableShader* InterpolableShader::RawClone() const {
  std::unique_ptr<InterpolableList> args(
      DynamicTo<InterpolableList>(args_->Clone().release()));
  return new InterpolableShader(
      relative_url_, absolute_url_, referrer_, resource_, std::move(args),
      To<InterpolableNumber>(*animation_frame_->Clone()).Value());
}

InterpolableShader* InterpolableShader::RawCloneAndZero() const {
  std::unique_ptr<InterpolableList> args(
      DynamicTo<InterpolableList>(args_->CloneAndZero().release()));
  return new InterpolableShader(
      relative_url_, absolute_url_, referrer_, resource_, std::move(args),
      To<InterpolableNumber>(*animation_frame_->CloneAndZero()).Value());
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
