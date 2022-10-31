// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_CONTENT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_CONTENT_ELEMENT_H_

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/shared_element_resource_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"

namespace blink {

// This class implements the functionality to display a live or cached snapshot
// of an element created using content:element(id).
// The element function is described at
// https://developer.mozilla.org/en-US/docs/Web/CSS/element().
class CORE_EXPORT DocumentTransitionContentElement
    : public DocumentTransitionPseudoElementBase {
 public:
  explicit DocumentTransitionContentElement(
      Element* parent,
      PseudoId,
      const AtomicString& document_transition_tag,
      viz::SharedElementResourceId,
      bool is_live_content_element,
      const DocumentTransitionStyleTracker* style_tracker);
  ~DocumentTransitionContentElement() override;

  void SetIntrinsicSize(const LayoutSize& intrinsic_size);
  const LayoutSize& intrinsic_size() const { return intrinsic_size_; }
  const viz::SharedElementResourceId& resource_id() const {
    return resource_id_;
  }
  bool is_live_content_element() const { return is_live_content_element_; }

 private:
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;

  // |resource_id_| is used to generate a foreign layer to substitute this
  // element with a render pass generated by the compositor.
  const viz::SharedElementResourceId resource_id_;

  // This indicates whether the element represents a live or a cached content.
  const bool is_live_content_element_;

  // The size of the element's texture generated by the compositor.
  LayoutSize intrinsic_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_CONTENT_ELEMENT_H_
