//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_SHADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_SHADER_H_

#include <memory>
#include <string>

#include "base/types/expected.h"
#include "cc/paint/gpu_shader_source.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using ShaderUniforms = uint32_t;

class PLATFORM_EXPORT GpuShader {
 public:
  GpuShader(cc::GpuShaderSource source, uint32_t uniforms);
  GpuShader(const GpuShader&) = delete;
  GpuShader& operator=(const GpuShader&) = delete;
  ~GpuShader();

  static base::expected<std::unique_ptr<GpuShader>, String> MakeFromSource(
      String source);

  const cc::GpuShaderSource& source() const { return shader_source_; }
  bool NeedsCompositingLayer() const;
  bool NeedsMouseInput() const;

 private:
  cc::GpuShaderSource shader_source_;
  uint32_t uniforms_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_SHADER_H_
