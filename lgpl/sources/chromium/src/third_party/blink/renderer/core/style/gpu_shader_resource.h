//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_GPU_SHADER_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_GPU_SHADER_RESOURCE_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Document;
class GpuShader;
class GpuShaderResourceClient;
class TextResource;

// A class tracking a reference to an shader resource

class GpuShaderResource : public GarbageCollected<GpuShaderResource>,
                          public ResourceClient {
 public:
  explicit GpuShaderResource(const KURL&);
  GpuShaderResource(const GpuShaderResource&) = delete;
  GpuShaderResource& operator=(const GpuShaderResource&) = delete;
  ~GpuShaderResource() override;

  void Load(Document&);

  void AddClient(GpuShaderResourceClient&);
  void RemoveClient(GpuShaderResourceClient&);
  bool IsLoaded() const;
  KURL Url() const { return url_; }
  const GpuShader* GetGpuShader() const;

  void Trace(Visitor*) const override;

 private:
  // ResourceClient implementation
  void NotifyFinished(Resource*) override;
  String DebugName() const override;

  void NotifyContentChanged();
  Member<const Document> document_;
  KURL url_;
  Member<TextResource> shader_content_;
  Member<ResourceLoader> loader_;
  std::unique_ptr<GpuShader> shader_;
  mutable HeapHashMap<Member<GpuShaderResourceClient>, int> clients_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_GPU_SHADER_RESOURCE_H_
