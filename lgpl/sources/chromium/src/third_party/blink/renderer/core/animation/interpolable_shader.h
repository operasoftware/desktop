//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SHADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SHADER_H_

#include <memory>
#include <vector>
#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/pairwise_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/style/gpu_shader_resource.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSValue;
class GpuShaderResource;
class StyleResolverState;
class UnderlyingValue;

// Represents a CSS shader value converted into a form that can be interpolated
// from/to.
class InterpolableShader : public InterpolableValue {
 public:
  InterpolableShader(AtomicString relative_url,
                     AtomicString absolute_url,
                     const Referrer& referrer,
                     GpuShaderResource* resource,
                     std::unique_ptr<InterpolableList> args,
                     float animation_frame);
  static std::unique_ptr<InterpolableShader> CreateNeutral();

  static std::unique_ptr<InterpolableShader> MaybeConvertCSSValue(
      const CSSValue&);

  const Referrer& referrer() const { return referrer_; }
  AtomicString relative_url() const { return relative_url_; }
  AtomicString absolute_url() const { return absolute_url_; }
  GpuShaderResource* resource() const { return resource_; }
  float animation_frame() const {
    return To<InterpolableNumber>(*animation_frame_).Value();
  }
  CSSValueList* CreateArgsList() const;

  // InterpolableValue implementation:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  bool IsShader() const final { return true; }
  bool Equals(const InterpolableValue& other) const final {
    NOTREACHED();
    return false;
  }
  void Scale(double scale) final;
  void Add(const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;
  bool IsCompatibleWith(const InterpolableShader& other) const;

 private:
  InterpolableShader* RawClone() const final;
  InterpolableShader* RawCloneAndZero() const final;

  Referrer referrer_;
  AtomicString relative_url_;
  AtomicString absolute_url_;
  Persistent<GpuShaderResource> resource_;

  // The interpolable components of a shader. These should all be non-null.
  std::unique_ptr<InterpolableList> args_;
  std::unique_ptr<InterpolableValue> animation_frame_;
};

template <>
struct DowncastTraits<InterpolableShader> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsShader();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SHADER_H_
