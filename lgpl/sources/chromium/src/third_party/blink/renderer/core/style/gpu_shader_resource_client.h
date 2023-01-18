//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GPU_SHADER_RESOURCE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GPU_SHADER_RESOURCE_CLIENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class GpuShaderResource;

class CORE_EXPORT GpuShaderResourceClient : public GarbageCollectedMixin {
 public:
  virtual ~GpuShaderResourceClient() = default;

  virtual void ResourceContentChanged(GpuShaderResource*) = 0;

 protected:
  GpuShaderResourceClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GPU_SHADER_RESOURCE_CLIENT_H_
