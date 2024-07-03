// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/text_resource.h"

#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

#if BUILDFLAG(OPERA_FEATURE_BLINK_GPU_SHADER_CSS_FILTER)
class GpuShaderDocumentResourceFactory : public ResourceFactory {
 public:
  GpuShaderDocumentResourceFactory()
      : ResourceFactory(ResourceType::kGpuShader,
                        TextResourceDecoderOptions::kPlainTextContent) {}

  Resource* Create(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      const TextResourceDecoderOptions& decoder_options) const override {
    return MakeGarbageCollected<TextResource>(request, ResourceType::kGpuShader,
                                              options, decoder_options);
  }
};
#endif  // BUILDFLAG(OPERA_FEATURE_BLINK_GPU_SHADER_CSS_FILTER)

}  // namespace

#if BUILDFLAG(OPERA_FEATURE_BLINK_GPU_SHADER_CSS_FILTER)
TextResource* TextResource::FetchGpuShaderDocument(FetchParameters& params,
                                                   ResourceFetcher* fetcher,
                                                   ResourceClient* client) {
  return To<TextResource>(fetcher->RequestResource(
      params, GpuShaderDocumentResourceFactory(), client));
}
#endif  // BUILDFLAG(OPERA_FEATURE_BLINK_GPU_SHADER_CSS_FILTER)

TextResource::TextResource(const ResourceRequest& resource_request,
                           ResourceType type,
                           const ResourceLoaderOptions& options,
                           const TextResourceDecoderOptions& decoder_options)
    : Resource(resource_request, type, options),
      decoder_(std::make_unique<TextResourceDecoder>(decoder_options)) {}

TextResource::~TextResource() = default;

void TextResource::SetEncoding(const String& chs) {
  decoder_->SetEncoding(WTF::TextEncoding(chs),
                        TextResourceDecoder::kEncodingFromHTTPHeader);
}

WTF::TextEncoding TextResource::Encoding() const {
  return decoder_->Encoding();
}

String TextResource::DecodedText() const {
  DCHECK(Data());

  StringBuilder builder;
  for (const auto& span : *Data())
    builder.Append(decoder_->Decode(span.data(), span.size()));
  builder.Append(decoder_->Flush());
  return builder.ToString();
}

}  // namespace blink
