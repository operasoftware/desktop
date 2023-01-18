// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_image_fallback_helper.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static bool ElementRepresentsNothing(const Element& element) {
  const auto& html_element = To<HTMLElement>(element);
  // We source fallback content/alternative text from more than just the 'alt'
  // attribute, so consider the element to represent text in those cases as
  // well.
  bool alt_is_set = !html_element.AltText().IsNull();
  bool alt_is_empty = alt_is_set && html_element.AltText().empty();
  bool src_is_set = !element.getAttribute(html_names::kSrcAttr).empty();
  if (src_is_set && alt_is_empty)
    return true;
  return !src_is_set && (!alt_is_set || alt_is_empty);
}

static bool ImageSmallerThanAltImage(int pixels_for_alt_image,
                                     const Length& width,
                                     const Length& height) {
  // We don't have a layout tree so can't compute the size of an image
  // relative dimensions - so we just assume we should display the alt image.
  if (!width.IsFixed() && !height.IsFixed())
    return false;
  if (height.IsFixed() && height.Value() < pixels_for_alt_image)
    return true;
  return width.IsFixed() && width.Value() < pixels_for_alt_image;
}

namespace {

class ImageFallbackContentBuilder {
  STACK_ALLOCATED();

 public:
  ImageFallbackContentBuilder(const ShadowRoot& shadow_root)
      : place_holder_(shadow_root.getElementById("alttext-container")),
        broken_image_(shadow_root.getElementById("alttext-image")) {}

  bool HasContentElements() const { return place_holder_ && broken_image_; }

  void ShowBrokenImageIcon(bool is_ltr) {
    broken_image_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                          CSSValueID::kInline);
    // Make sure the broken image icon appears on the appropriate side of the
    // image for the element's writing direction.
    broken_image_->SetInlineStyleProperty(
        CSSPropertyID::kFloat, AtomicString(is_ltr ? "left" : "right"));
  }
  void HideBrokenImageIcon() {
    broken_image_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                          CSSValueID::kNone);
  }

  void ShowAsReplaced(const Length& width, const Length& height, float zoom) {
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kOverflow,
                                          CSSValueID::kHidden);
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                          CSSValueID::kInlineBlock);
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kPointerEvents,
                                          CSSValueID::kNone);
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kHeight,
                                          *CSSValue::Create(height, zoom));
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kWidth,
                                          *CSSValue::Create(width, zoom));
  }

  void ShowBorder() {
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kBorderWidth, 1,
                                          CSSPrimitiveValue::UnitType::kPixels);
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kBorderStyle,
                                          CSSValueID::kSolid);
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kBorderColor,
                                          CSSValueID::kSilver);
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kPadding, 1,
                                          CSSPrimitiveValue::UnitType::kPixels);
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kBoxSizing,
                                          CSSValueID::kBorderBox);
  }

  void AlignToBaseline() {
    place_holder_->SetInlineStyleProperty(CSSPropertyID::kVerticalAlign,
                                          CSSValueID::kBaseline);
  }

 private:
  Element* place_holder_;
  Element* broken_image_;
};

}  // namespace

void HTMLImageFallbackHelper::CreateAltTextShadowTree(Element& element) {
  Document& document = element.GetDocument();

  auto* container = MakeGarbageCollected<HTMLSpanElement>(document);
  container->setAttribute(html_names::kIdAttr,
                          AtomicString("alttext-container"));

  auto* broken_image = MakeGarbageCollected<HTMLImageElement>(document);
  broken_image->SetIsFallbackImage();
  broken_image->setAttribute(html_names::kIdAttr,
                             AtomicString("alttext-image"));
  broken_image->setAttribute(html_names::kWidthAttr, AtomicString("16"));
  broken_image->setAttribute(html_names::kHeightAttr, AtomicString("16"));
  broken_image->setAttribute(html_names::kAlignAttr, AtomicString("left"));
  broken_image->SetInlineStyleProperty(CSSPropertyID::kMargin, 0,
                                       CSSPrimitiveValue::UnitType::kPixels);
  container->AppendChild(broken_image);

  auto* alt_text = MakeGarbageCollected<HTMLSpanElement>(document);
  alt_text->setAttribute(html_names::kIdAttr, AtomicString("alttext"));

  auto* text = Text::Create(document, To<HTMLElement>(element).AltText());
  alt_text->AppendChild(text);
  container->AppendChild(alt_text);

  element.EnsureUserAgentShadowRoot().AppendChild(container);
}

void HTMLImageFallbackHelper::CustomStyleForAltText(Element& element,
                                                    ComputedStyle& new_style) {
  // If we have an author shadow root or have not created the UA shadow root
  // yet, bail early. We can't use ensureUserAgentShadowRoot() here because that
  // would alter the DOM tree during style recalc.
  if (element.AuthorShadowRoot() || !element.UserAgentShadowRoot())
    return;

  ImageFallbackContentBuilder fallback(*element.UserAgentShadowRoot());
  // Input elements have a UA shadow root of their own. We may not have replaced
  // it with fallback content yet.
  if (!fallback.HasContentElements())
    return;

  // TODO(crbug.com/953707):
  // This method is called during style recalc, and it is generally not allowed
  // to mark nodes style dirty during recalc. The code below modifies inline
  // style in the UA shadow tree below based on the computed style for the image
  // element. As part of that we mark elements in the shadow tree style dirty.
  // The scope object here is to allow that and avoid DCHECK failures which
  // would otherwise have been triggered.
  StyleEngine::AllowMarkStyleDirtyFromRecalcScope scope(
      element.GetDocument().GetStyleEngine());

  if (element.GetDocument().InQuirksMode()) {
    // Mimic the behaviour of the image host by setting symmetric dimensions if
    // only one dimension is specified.
    if (!new_style.Width().IsAuto() && new_style.Height().IsAuto())
      new_style.SetHeight(new_style.Width());
    else if (!new_style.Height().IsAuto() && new_style.Width().IsAuto())
      new_style.SetWidth(new_style.Height());

    if (!new_style.Width().IsAuto() && !new_style.Height().IsAuto())
      fallback.AlignToBaseline();
  }

  bool has_intrinsic_dimensions =
      !new_style.Width().IsAuto() && !new_style.Height().IsAuto();
  bool has_dimensions_from_ar =
      !new_style.AspectRatio().IsAuto() &&
      (!new_style.Width().IsAuto() || !new_style.Height().IsAuto());
  bool has_no_alt_attribute =
      element.getAttribute(html_names::kAltAttr).empty();
  bool treat_as_replaced =
      (has_intrinsic_dimensions || has_dimensions_from_ar) &&
      (element.GetDocument().InQuirksMode() || has_no_alt_attribute);
  if (treat_as_replaced) {
    // https://html.spec.whatwg.org/C/#images-3:
    // "If the element does not represent an image, but the element already has
    // intrinsic dimensions (e.g. from the dimension attributes or CSS rules),
    // and either: the user agent has reason to believe that the image will
    // become available and be rendered in due course, or the element has no alt
    // attribute, or the Document is in quirks mode The user agent is expected
    // to treat the element as a replaced element whose content is the text that
    // the element represents, if any."
    fallback.ShowAsReplaced(new_style.Width(), new_style.Height(),
                            new_style.EffectiveZoom());

    // 16px for the image and 2px for its top/left border/padding offset.
    int pixels_for_alt_image = 18;
    if (ImageSmallerThanAltImage(pixels_for_alt_image, new_style.Width(),
                                 new_style.Height())) {
      fallback.HideBrokenImageIcon();
    } else {
      fallback.ShowBorder();
      fallback.ShowBrokenImageIcon(new_style.IsLeftToRightDirection());
    }
  } else {
    if (new_style.Display() == EDisplay::kInline) {
      new_style.SetWidth(Length());
      new_style.SetHeight(Length());
      new_style.SetAspectRatio(
          ComputedStyleInitialValues::InitialAspectRatio());
    }
    if (ElementRepresentsNothing(element)) {
      // "If the element is an img element that represents nothing and the user
      // agent does not expect this to change the user agent is expected to
      // treat the element as an empty inline element."
      //  - We achieve this by hiding the broken image so that the span is
      //  empty.
      fallback.HideBrokenImageIcon();
    } else {
      // "If the element is an img element that represents some text and the
      // user agent does not expect this to change the user agent is expected to
      // treat the element as a non-replaced phrasing element whose content is
      // the text, optionally with an icon indicating that an image is missing,
      // so that the user can request the image be displayed or investigate why
      // it is not rendering."
      fallback.ShowBrokenImageIcon(new_style.IsLeftToRightDirection());
    }
  }
}

}  // namespace blink
