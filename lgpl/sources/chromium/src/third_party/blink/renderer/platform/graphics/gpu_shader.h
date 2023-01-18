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
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkString.h"

namespace blink {

using ShaderUniforms = uint32_t;

class PLATFORM_EXPORT GpuShader {
 public:
  GpuShader();
  GpuShader(const GpuShader&) = delete;
  GpuShader& operator=(const GpuShader&) = delete;
  ~GpuShader();

  static base::expected<std::unique_ptr<GpuShader>, String> MakeFromSource(
      String source);

  const std::string& source() const { return shader_source_; }
  bool NeedsCompositingLayer() const;
  bool NeedsMouseInput() const;

 private:
  std::string shader_source_;
  uint32_t uniforms_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_SHADER_H_
