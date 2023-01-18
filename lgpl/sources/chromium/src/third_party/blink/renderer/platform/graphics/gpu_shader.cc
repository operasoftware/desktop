//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "third_party/blink/renderer/platform/graphics/gpu_shader.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "cc/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {
String FormatUniformError(const cc::GpuShaderPaintFilter::UniformError& error) {
  switch (error.type()) {
    case cc::GpuShaderPaintFilter::UniformError::kErrorUnknownUniform:
      return String::Format(
          "Unexpected uniform: \x1B[41;93;4m%s\x1B[m.\n\nAllowed "
          "uniforms:\n-----------------\n%s",
          error.name().c_str(),
          base::JoinString(cc::GpuShaderPaintFilter::GetAllSupportedUniforms(),
                           "\n")
              .c_str());
    case cc::GpuShaderPaintFilter::UniformError::kTypeMismatch:
      return String::Format(
          "Invalid uniform type for \x1B[41;93;4m%s\x1B[m. Expected "
          "\x1B[42;93;4m%s\x1B[m but got "
          "\x1B[41;93;4m%s\x1B[m",
          error.name().c_str(), error.excpected_type().c_str(),
          error.actual_type().c_str());
    case cc::GpuShaderPaintFilter::UniformError::kErrorNone:
      NOTREACHED();
      return String();
  }
}

}  // namespace

GpuShader::GpuShader() = default;

GpuShader::~GpuShader() = default;

// static
base::expected<std::unique_ptr<GpuShader>, String> GpuShader::MakeFromSource(
    String source) {
  if (!SharedGpuContext::IsGpuCompositingEnabled()) {
    return base::unexpected(
        String("GpuShader filters are not supported when GPU compositing is "
               "disabled."));
  }

  auto shader_source = source.Utf8();
  cc::GpuShaderPaintFilter filter(shader_source);
  auto status = filter.CompileAndBindUniforms();

  if (!status.IsSuccess()) {
    switch (status.status()) {
      case cc::GpuShaderPaintFilter::CompilationStatus::kErrorParser:
        return base::unexpected(String(status.parser_error()));
      case cc::GpuShaderPaintFilter::CompilationStatus::kErrorUniforms:
        return base::unexpected(FormatUniformError(status.uniform_error()));
      case cc::GpuShaderPaintFilter::CompilationStatus::kSuccess:
        NOTREACHED();
        break;
    }
  }

  DCHECK(filter.valid());

  auto shader = std::make_unique<GpuShader>();
  shader->shader_source_ = std::move(shader_source);
  shader->uniforms_ = filter.uniforms();
  return std::move(shader);
}

bool GpuShader::NeedsCompositingLayer() const {
  static constexpr cc::GpuShaderPaintFilter::ShaderUniforms
      kUniformForcesCompositingLayer =
          cc::GpuShaderPaintFilter::kUniformMousePosition |
          cc::GpuShaderPaintFilter::kUniformAnimationFrame;
  return uniforms_ & kUniformForcesCompositingLayer;
}

bool GpuShader::NeedsMouseInput() const {
  return uniforms_ & cc::GpuShaderPaintFilter::kUniformMousePosition;
}

}  // namespace blink
