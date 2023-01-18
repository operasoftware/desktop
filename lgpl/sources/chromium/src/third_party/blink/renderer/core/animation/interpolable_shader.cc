//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "third_party/blink/renderer/core/animation/interpolable_shader.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_shader_value.h"

namespace blink {

InterpolableShader::InterpolableShader(AtomicString relative_url,
                                       AtomicString absolute_url,
                                       const Referrer& referrer,
                                       GpuShaderResource* resource,
                                       float animation_frame)
    : referrer_(referrer),
      relative_url_(relative_url),
      absolute_url_(absolute_url),
      resource_(resource) {
  animation_frame_ = std::make_unique<InterpolableNumber>(animation_frame);
}

// static
std::unique_ptr<InterpolableShader> InterpolableShader::CreateNeutral() {
  return std::make_unique<InterpolableShader>(AtomicString(), AtomicString(),
                                              Referrer(), nullptr, 0);
}

// static
std::unique_ptr<InterpolableShader> InterpolableShader::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* shader = DynamicTo<CSSShaderValue>(value);
  if (!shader)
    return nullptr;

  return std::make_unique<InterpolableShader>(
      shader->RelativeUrl(), shader->AbsoluteUrl(), shader->GetReferrer(),
      shader->Resource(), shader->AnimationFrame());
}

InterpolableShader* InterpolableShader::RawClone() const {
  return new InterpolableShader(
      relative_url_, absolute_url_, referrer_, resource_,
      To<InterpolableNumber>(*animation_frame_->Clone()).Value());
}

InterpolableShader* InterpolableShader::RawCloneAndZero() const {
  return new InterpolableShader(
      relative_url_, absolute_url_, referrer_, resource_,
      To<InterpolableNumber>(*animation_frame_->CloneAndZero()).Value());
}

void InterpolableShader::Scale(double scale) {
  animation_frame_->Scale(scale);
}

void InterpolableShader::Add(const InterpolableValue& other) {
  const InterpolableShader& other_shader = To<InterpolableShader>(other);
  animation_frame_->Add(*other_shader.animation_frame_);
}

void InterpolableShader::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableShader& other_shader = To<InterpolableShader>(other);
  animation_frame_->AssertCanInterpolateWith(*other_shader.animation_frame_);
}

void InterpolableShader::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const InterpolableShader& to_shader = To<InterpolableShader>(to);
  InterpolableShader& result_shader = To<InterpolableShader>(result);

  animation_frame_->Interpolate(*to_shader.animation_frame_, progress,
                                *result_shader.animation_frame_);
}

}  // namespace blink
