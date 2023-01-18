/*
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/resolver/viewport_style_resolver.h"

#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

ViewportStyleResolver::ViewportStyleResolver(Document& document)
    : document_(document) {
  DCHECK(document.GetFrame());
}

void ViewportStyleResolver::Reset() {
  needs_update_ = false;
}

float ViewportStyleResolver::Zoom() const {
  return document_->GetStyleResolver().InitialZoom();
}

ViewportDescription ViewportStyleResolver::ResolveViewportDescription(
    mojom::blink::ViewportStyle viewport_style) {
  ViewportDescription description(ViewportDescription::kUserAgentStyleSheet);

  if (document_->IsMobileDocument()) {
    description.min_zoom = 0.25;
    description.max_zoom = 5.0;
    return description;
  }

  switch (viewport_style) {
    case mojom::blink::ViewportStyle::kDefault:
      return description;
    case mojom::blink::ViewportStyle::kMobile: {
      description.min_width = Length::Fixed(980.0 * Zoom());
      return description;
    }
    case mojom::blink::ViewportStyle::kTelevision: {
      description.min_width = Length::Fixed(1280 * Zoom());
      return description;
    }
  }
}

void ViewportStyleResolver::Resolve() {
  mojom::blink::ViewportStyle viewport_style =
      document_->GetSettings() ? document_->GetSettings()->GetViewportStyle()
                               : mojom::blink::ViewportStyle::kDefault;
  document_->GetViewportData().SetViewportDescription(
      ResolveViewportDescription(viewport_style));
}
void ViewportStyleResolver::SetNeedsUpdate() {
  needs_update_ = true;
  document_->ScheduleLayoutTreeUpdateIfNeeded();
}

void ViewportStyleResolver::UpdateViewport() {
  if (!needs_update_)
    return;
  Reset();
  Resolve();
  needs_update_ = false;
}

void ViewportStyleResolver::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

}  // namespace blink
