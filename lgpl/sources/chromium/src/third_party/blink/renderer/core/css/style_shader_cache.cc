//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "third_party/blink/renderer/core/css/style_shader_cache.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/gpu_shader_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

namespace blink {

// TODO(kubal): StyleImageCache respects is_ad_related. Consider checking it
// too.
GpuShaderResource* StyleShaderCache::CacheStyleShader(const String& url) {
  auto result = fetched_shader_map_.insert(url, nullptr);
  if (result.is_new_entry || !result.stored_value->value) {
    auto* resource = MakeGarbageCollected<GpuShaderResource>(KURL(url));
    resource->AddClient(*this);
    result.stored_value->value = resource;
  }
  return result.stored_value->value;
}

void StyleShaderCache::ResourceContentChanged(GpuShaderResource* resource) {
  resource->RemoveClient(*this);
}

void StyleShaderCache::Trace(Visitor* visitor) const {
  GpuShaderResourceClient::Trace(visitor);
  visitor->Trace(fetched_shader_map_);
}

}  // namespace blink
