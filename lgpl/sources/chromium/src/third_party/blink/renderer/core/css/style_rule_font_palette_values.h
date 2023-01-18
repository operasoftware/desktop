// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_FONT_PALETTE_VALUES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_FONT_PALETTE_VALUES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptors.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"

namespace blink {

class CORE_EXPORT StyleRuleFontPaletteValues : public StyleRuleBase {
 public:
  StyleRuleFontPaletteValues(const AtomicString&, CSSPropertyValueSet*);
  StyleRuleFontPaletteValues(const StyleRuleFontPaletteValues&);
  ~StyleRuleFontPaletteValues();

  AtomicString GetName() const { return name_; }
  const CSSValue* GetFontFamily() const { return font_family_; }
  const CSSValue* GetBasePalette() const { return base_palette_; }
  const CSSValue* GetOverrideColors() const { return override_colors_; }

  AtomicString GetFontFamilyAsString() const;
  FontPalette::BasePaletteValue GetBasePaletteIndex() const;
  Vector<FontPalette::FontPaletteOverride> GetOverrideColorsAsVector() const;

  StyleRuleFontPaletteValues* Copy() const {
    return MakeGarbageCollected<StyleRuleFontPaletteValues>(*this);
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<const CSSValue>& GetDescriptorReference(AtRuleDescriptorID);

  AtomicString name_;
  Member<const CSSValue> font_family_;
  Member<const CSSValue> base_palette_;
  Member<const CSSValue> override_colors_;
};

template <>
struct DowncastTraits<StyleRuleFontPaletteValues> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsFontPaletteValuesRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_FONT_PALETTE_VALUES_H_
