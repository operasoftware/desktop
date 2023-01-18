//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "third_party/blink/renderer/core/style/gpu_shader_resource.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/resource/text_resource.h"
#include "third_party/blink/renderer/core/style/gpu_shader_resource_client.h"
#include "third_party/blink/renderer/platform/graphics/gpu_shader.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

namespace blink {

GpuShaderResource::GpuShaderResource(const KURL& url) : url_(url) {}

GpuShaderResource::~GpuShaderResource() = default;

void GpuShaderResource::AddClient(GpuShaderResourceClient& client) {
  auto& refcount = clients_.insert(&client, 0).stored_value->value;
  refcount++;
}

void GpuShaderResource::RemoveClient(GpuShaderResourceClient& client) {
  auto it = clients_.find(&client);
  DCHECK(it != clients_.end());
  it->value--;
  if (it->value)
    return;
  clients_.erase(it);
}

bool GpuShaderResource::IsLoaded() const {
  return !!shader_.get();
}

const GpuShader* GpuShaderResource::GetGpuShader() const {
  DCHECK(shader_);
  return shader_.get();
}

void GpuShaderResource::NotifyContentChanged() {
  HeapVector<Member<GpuShaderResourceClient>> clients;
  CopyKeysToVector(clients_, clients);

  for (GpuShaderResourceClient* client : clients)
    client->ResourceContentChanged(this);
}

void GpuShaderResource::Load(Document& document) {
  if (shader_content_)
    return;

  document_ = &document;

  ExecutionContext* execution_context = document.GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(url_), options);
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  DCHECK_EQ(params.GetResourceRequest().GetRequestContext(),
            mojom::blink::RequestContextType::UNSPECIFIED);
  params.SetRequestContext(mojom::blink::RequestContextType::SUBRESOURCE);
  params.SetRequestDestination(network::mojom::RequestDestination::kEmpty);

  shader_content_ =
      TextResource::FetchGpuShaderDocument(params, document.Fetcher(), this);
  loader_ = shader_content_->Loader();
}

void GpuShaderResource::NotifyFinished(Resource*) {
  if (shader_content_->HasData()) {
    auto result = GpuShader::MakeFromSource(shader_content_->DecodedText());
    if (result.has_value()) {
      shader_ = std::move(result.value());
    } else if (!result.error().empty()) {
      document_->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::ConsoleMessageSource::kOther,
              mojom::ConsoleMessageLevel::kError,
              String("Error parsing shader source at: ") + url_.GetString() +
                  "\n" + result.error()),
          false);
    }
  }

  document_.Clear();
  loader_ = nullptr;
  NotifyContentChanged();
}

String GpuShaderResource::DebugName() const {
  return "GpuShaderResource";
}

void GpuShaderResource::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(shader_content_);
  visitor->Trace(clients_);
  visitor->Trace(loader_);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
