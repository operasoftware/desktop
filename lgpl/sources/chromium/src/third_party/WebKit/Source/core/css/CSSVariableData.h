// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableData_h
#define CSSVariableData_h

#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSParserTokenRange;
class CSSSyntaxDescriptor;

class CORE_EXPORT CSSVariableData : public RefCounted<CSSVariableData> {
  WTF_MAKE_NONCOPYABLE(CSSVariableData);
  USING_FAST_MALLOC(CSSVariableData);

 public:
  static PassRefPtr<CSSVariableData> Create(const CSSParserTokenRange& range,
                                            bool is_animation_tainted,
                                            bool needs_variable_resolution) {
    return AdoptRef(new CSSVariableData(range, is_animation_tainted,
                                        needs_variable_resolution));
  }

  static PassRefPtr<CSSVariableData> CreateResolved(
      const Vector<CSSParserToken>& resolved_tokens,
      const CSSVariableData& unresolved_data,
      bool is_animation_tainted) {
    return AdoptRef(new CSSVariableData(resolved_tokens,
                                        unresolved_data.backing_string_,
                                        is_animation_tainted));
  }

  CSSParserTokenRange TokenRange() const { return tokens_; }

  const Vector<CSSParserToken>& Tokens() const { return tokens_; }

  bool operator==(const CSSVariableData& other) const;

  bool IsAnimationTainted() const { return is_animation_tainted_; }

  bool NeedsVariableResolution() const { return needs_variable_resolution_; }

  const CSSValue* ParseForSyntax(const CSSSyntaxDescriptor&) const;

  StylePropertySet* PropertySet();

 private:
  CSSVariableData(const CSSParserTokenRange&,
                  bool is_animation_tainted,
                  bool needs_variable_resolution);

  // We can safely copy the tokens (which have raw pointers to substrings)
  // because StylePropertySets contain references to
  // CSSCustomPropertyDeclarations, which point to the unresolved
  // CSSVariableData values that own the backing strings this will potentially
  // reference.
  CSSVariableData(const Vector<CSSParserToken>& resolved_tokens,
                  String backing_string,
                  bool is_animation_tainted)
      : backing_string_(backing_string),
        tokens_(resolved_tokens),
        is_animation_tainted_(is_animation_tainted),
        needs_variable_resolution_(false),
        cached_property_set_(false) {}

  void ConsumeAndUpdateTokens(const CSSParserTokenRange&);
  template <typename CharacterType>
  void UpdateTokens(const CSSParserTokenRange&);

  String backing_string_;
  Vector<CSSParserToken> tokens_;
  const bool is_animation_tainted_;
  const bool needs_variable_resolution_;

  // Parsed representation for @apply
  bool cached_property_set_;
  Persistent<StylePropertySet> property_set_;
};

}  // namespace blink

#endif  // CSSVariableData_h
