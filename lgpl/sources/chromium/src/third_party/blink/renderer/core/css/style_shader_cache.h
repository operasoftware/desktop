//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SHADER_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SHADER_CACHE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/gpu_shader_resource_client.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class GpuShaderResource;
class FetchParameters;

// A per-StyleEngine cache for StyleShader. A CSSShaderValue points to a
// StyleShader, but different CSSShaderValue objects with the same URL would not
// have shared the same StyleShader without this cache.
class CORE_EXPORT StyleShaderCache : public GpuShaderResourceClient {
  DISALLOW_NEW();

 public:
  StyleShaderCache() = default;

  // Look up an existing GpuShaderResource in the cache, or create a new one,
  // add it to the cache. The fetch will be started later.
  GpuShaderResource* CacheStyleShader(const String& url);

  void Trace(Visitor*) const override;

 private:
  void ResourceContentChanged(GpuShaderResource*) override;

  // Map from URL to style shader. A weak reference makes sure the entry is
  // removed when no style declarations nor computed styles have a reference to
  // the shader.
  HeapHashMap<String, WeakMember<GpuShaderResource>> fetched_shader_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SHADER_CACHE_H_
