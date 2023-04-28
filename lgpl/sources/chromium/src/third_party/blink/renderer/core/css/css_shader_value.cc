//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#include "third_party/blink/renderer/core/css/css_shader_value.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/style/gpu_shader_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSShaderValue::CSSShaderValue() : CSSValue(kShaderValueClass) {}

CSSShaderValue::CSSShaderValue(const AtomicString& raw_value,
                               const KURL& url,
                               const Referrer& referrer,
                               CSSValueList* args,
                               float animation_frame)
    : CSSValue(kShaderValueClass),
      referrer_(referrer),
      relative_url_(raw_value),
      absolute_url_(url.GetString()),
      args_(args),
      animation_frame_(animation_frame) {}

CSSShaderValue::~CSSShaderValue() = default;

GpuShaderResource* CSSShaderValue::CacheShader(const Document& document) const {
  if (!document.GetSettings() ||
      !document.GetSettings()->GetGpuShaderCssFiltersEnabled()) {
    return nullptr;
  }

  if (!resource_) {
    resource_ = document.GetStyleEngine().CacheStyleShader(absolute_url_);
  }

  return resource_.Get();
}

void CSSShaderValue::ReResolveUrl(const Document& document) const {
  KURL url = document.CompleteURL(relative_url_);
  AtomicString url_string(url.GetString());
  if (url_string == absolute_url_)
    return;
  absolute_url_ = url_string;
  resource_ = nullptr;
  animation_frame_ = 0;
  args_.Clear();
}

String CSSShaderValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("shader(");
  result.Append(SerializeURI(relative_url_));
  result.Append(" ");
  result.Append(args_->CssText());
  result.Append(")");
  return result.ReleaseString();
}

bool CSSShaderValue::Equals(const CSSShaderValue& other) const {
  bool urls_equal = false;
  if (absolute_url_.empty() && other.absolute_url_.empty())
    urls_equal = relative_url_ == other.relative_url_;
  else
    urls_equal = absolute_url_ == other.absolute_url_;
  return urls_equal && args_ == other.args_ &&
         animation_frame_ == other.animation_frame_;
}

void CSSShaderValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(resource_);
  visitor->Trace(args_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
