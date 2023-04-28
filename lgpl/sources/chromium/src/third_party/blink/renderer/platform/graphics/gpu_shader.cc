//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "third_party/blink/renderer/platform/graphics/gpu_shader.h"

#include <utility>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "cc/paint/gpu_shader_program.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {
String FormatUniformError(const cc::GpuShaderProgram::UniformError& error) {
  switch (error.type()) {
    case cc::GpuShaderProgram::UniformError::kErrorUnknownUniform:
      return String::Format(
          "Unexpected uniform: \x1B[41;93;4m%s\x1B[m.\n\nAllowed "
          "uniforms:\n-----------------\n%s",
          error.name().c_str(),
          base::JoinString(cc::GpuShaderProgram::GetAllSupportedUniforms(),
                           "\n")
              .c_str());
    case cc::GpuShaderProgram::UniformError::kTypeMismatch:
      return String::Format(
          "Invalid uniform type for \x1B[41;93;4m%s\x1B[m. Expected "
          "\x1B[42;93;4m%s\x1B[m but got "
          "\x1B[41;93;4m%s\x1B[m",
          error.name().c_str(), error.excpected_type().c_str(),
          error.actual_type().c_str());
    case cc::GpuShaderProgram::UniformError::kErrorNone:
      NOTREACHED();
      return String();
  }
}

}  // namespace

GpuShader::GpuShader(cc::GpuShaderSource source, uint32_t uniforms)
    : shader_source_(std::move(source)), uniforms_(uniforms) {}

GpuShader::~GpuShader() = default;

// static
base::expected<std::unique_ptr<GpuShader>, String> GpuShader::MakeFromSource(
    String source) {
  if (!SharedGpuContext::IsGpuCompositingEnabled()) {
    return base::unexpected(
        String("GpuShader filters are not supported when GPU compositing is "
               "disabled."));
  }

  auto shader_source = cc::GpuShaderSource(SkString(source.Utf8()));
  auto program = cc::GpuShaderProgram::Make(std::move(shader_source));

  if (program->error().has_value()) {
    const auto& error = program->error().value();
    switch (error.type()) {
      case cc::GpuShaderProgram::Error::kErrorParser:
        return base::unexpected(String(error.parser_error()));
      case cc::GpuShaderProgram::Error::kErrorUniforms:
        return base::unexpected(FormatUniformError(error.uniform_error()));
    }
  }

  return std::make_unique<GpuShader>(
      cc::GpuShaderSource(SkString(source.Utf8())), program->uniforms_flags());
}

bool GpuShader::NeedsCompositingLayer() const {
  // TODO(kubal): Re-enable this once blink painting issues are fixed
  //              See DNA-103201 - for now we always force compositing layer.
  // static constexpr cc::GpuShaderPaintFilter::ShaderUniforms
  //     kUniformForcesCompositingLayer =
  //         cc::GpuShaderPaintFilter::kUniformMousePosition |
  //         cc::GpuShaderPaintFilter::kUniformAnimationFrame;
  // return uniforms_ & kUniformForcesCompositingLayer;
  return true;
}

bool GpuShader::NeedsMouseInput() const {
  return uniforms_ & cc::GpuShaderProgram::kUniformMousePosition;
}

}  // namespace blink
