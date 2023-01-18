//
// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.
//

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SHADER_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SHADER_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class KURL;
class GpuShaderResource;

class CORE_EXPORT CSSShaderValue : public CSSValue {
 public:
  CSSShaderValue(const AtomicString& raw_value,
                 const KURL&,
                 const Referrer&,
                 float);
  ~CSSShaderValue();

  GpuShaderResource* CacheShader(const Document& document) const;

  void ReResolveUrl(const Document&) const;

  String CustomCSSText() const;
  bool Equals(const CSSShaderValue&) const;

  CSSShaderValue* ValueWithURLMadeAbsolute() const {
    return MakeGarbageCollected<CSSShaderValue>(
        absolute_url_, KURL(absolute_url_), Referrer(), animation_frame_);
  }

  void TraceAfterDispatch(blink::Visitor*) const;

  const Referrer& GetReferrer() const { return referrer_; }
  const AtomicString& AbsoluteUrl() const { return absolute_url_; }
  const AtomicString& RelativeUrl() const { return relative_url_; }
  GpuShaderResource* Resource() const { return resource_; }
  float AnimationFrame() const { return animation_frame_; }

 private:
  Referrer referrer_;
  AtomicString relative_url_;
  mutable Member<GpuShaderResource> resource_;
  mutable AtomicString absolute_url_;
  // This is dummy frame id using for driving the shader animations.
  mutable float animation_frame_;
};

template <>
struct DowncastTraits<CSSShaderValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsShaderValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SHADER_VALUE_H_
