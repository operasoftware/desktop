// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

#include <bitset>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_position_fallback_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_try_rule.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_property_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

// This may still consume tokens if it fails
AtomicString ConsumeStringOrURI(CSSParserTokenStream& stream) {
  const CSSParserToken& token = stream.Peek();

  if (token.GetType() == kStringToken || token.GetType() == kUrlToken)
    return stream.ConsumeIncludingWhitespace().Value().ToAtomicString();

  if (token.GetType() != kFunctionToken ||
      !EqualIgnoringASCIICase(token.Value(), "url"))
    return AtomicString();

  AtomicString result;
  {
    CSSParserTokenStream::BlockGuard guard(stream);
    const CSSParserToken& uri = stream.ConsumeIncludingWhitespace();
    if (uri.GetType() != kBadStringToken && stream.UncheckedAtEnd()) {
      DCHECK_EQ(uri.GetType(), kStringToken);
      result = uri.Value().ToAtomicString();
    }
  }
  stream.ConsumeWhitespace();
  return result;
}

// Finds the longest prefix of |range| that matches a <layer-name> and parses
// it. Returns an empty result with |range| unmodified if parsing fails.
StyleRuleBase::LayerName ConsumeCascadeLayerName(CSSParserTokenRange& range) {
  CSSParserTokenRange original_range = range;
  StyleRuleBase::LayerName name;
  while (!range.AtEnd() && range.Peek().GetType() == kIdentToken) {
    const CSSParserToken& name_part = range.Consume();
    name.emplace_back(name_part.Value().ToString());

    const bool has_next_part = range.Peek().GetType() == kDelimiterToken &&
                               range.Peek().Delimiter() == '.' &&
                               range.Peek(1).GetType() == kIdentToken;
    if (!has_next_part)
      break;
    range.Consume();
  }

  if (!name.size())
    original_range = range;
  else
    range.ConsumeWhitespace();

  return name;
}

StyleRule::RuleType RuleTypeForMutableDeclaration(
    MutableCSSPropertyValueSet* declaration) {
  switch (declaration->CssParserMode()) {
    case kCSSViewportRuleMode:
      return StyleRule::kViewport;
    case kCSSFontFaceRuleMode:
      return StyleRule::kFontFace;
    case kCSSKeyframeRuleMode:
      return StyleRule::kKeyframe;
    default:
      return StyleRule::kStyle;
  }
}

}  // namespace

CSSParserImpl::CSSParserImpl(const CSSParserContext* context,
                             StyleSheetContents* style_sheet)
    : context_(context),
      style_sheet_(style_sheet),
      observer_(nullptr),
      lazy_state_(nullptr) {}

MutableCSSPropertyValueSet::SetResult CSSParserImpl::ParseValue(
    MutableCSSPropertyValueSet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important,
    const CSSParserContext* context) {
  STACK_UNINITIALIZED CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = RuleTypeForMutableDeclaration(declaration);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  CSSTokenizedValue tokenized_value = ConsumeValue(stream);
  parser.ConsumeDeclarationValue(tokenized_value, unresolved_property,
                                 important, rule_type);
  if (parser.parsed_properties_.IsEmpty()) {
    return MutableCSSPropertyValueSet::kParseError;
  }
  return declaration->AddParsedProperties(parser.parsed_properties_);
}

MutableCSSPropertyValueSet::SetResult CSSParserImpl::ParseVariableValue(
    MutableCSSPropertyValueSet* declaration,
    const AtomicString& property_name,
    const String& value,
    bool important,
    const CSSParserContext* context,
    bool is_animation_tainted) {
  STACK_UNINITIALIZED CSSParserImpl parser(context);
  CSSTokenizer tokenizer(value);
  CSSParserTokenStream stream(tokenizer);
  CSSTokenizedValue tokenized_value = ConsumeValue(stream);
  parser.ConsumeVariableValue(tokenized_value, property_name, important,
                              is_animation_tainted);
  if (parser.parsed_properties_.IsEmpty()) {
    return MutableCSSPropertyValueSet::kParseError;
  } else {
    return declaration->AddParsedProperties(parser.parsed_properties_);
  }
}

static inline void FilterProperties(
    bool important,
    const HeapVector<CSSPropertyValue, 64>& input,
    HeapVector<CSSPropertyValue, 64>& output,
    wtf_size_t& unused_entries,
    std::bitset<kNumCSSProperties>& seen_properties,
    HashSet<AtomicString>& seen_custom_properties) {
  // Add properties in reverse order so that highest priority definitions are
  // reached first. Duplicate definitions can then be ignored when found.
  for (wtf_size_t i = input.size(); i--;) {
    const CSSPropertyValue& property = input[i];
    if (property.IsImportant() != important)
      continue;
    if (property.Id() == CSSPropertyID::kVariable) {
      AtomicString name = property.Name().ToAtomicString();
      if (seen_custom_properties.Contains(name))
        continue;
      seen_custom_properties.insert(name);
    } else {
      const unsigned property_id_index = GetCSSPropertyIDIndex(property.Id());
      if (seen_properties.test(property_id_index))
        continue;
      seen_properties.set(property_id_index);
    }
    output[--unused_entries] = property;
  }
}

static ImmutableCSSPropertyValueSet* CreateCSSPropertyValueSet(
    HeapVector<CSSPropertyValue, 64>& parsed_properties,
    CSSParserMode mode) {
  std::bitset<kNumCSSProperties> seen_properties;
  wtf_size_t unused_entries = parsed_properties.size();
  HeapVector<CSSPropertyValue, 64> results(unused_entries);
  HashSet<AtomicString> seen_custom_properties;

  FilterProperties(true, parsed_properties, results, unused_entries,
                   seen_properties, seen_custom_properties);
  FilterProperties(false, parsed_properties, results, unused_entries,
                   seen_properties, seen_custom_properties);

  ImmutableCSSPropertyValueSet* result = ImmutableCSSPropertyValueSet::Create(
      results.data() + unused_entries, results.size() - unused_entries, mode);
  parsed_properties.clear();
  return result;
}

ImmutableCSSPropertyValueSet* CSSParserImpl::ParseInlineStyleDeclaration(
    const String& string,
    Element* element) {
  Document& document = element->GetDocument();
  auto* context = MakeGarbageCollected<CSSParserContext>(
      document.ElementSheet().Contents()->ParserContext(), &document);
  CSSParserMode mode = element->IsHTMLElement() && !document.InQuirksMode()
                           ? kHTMLStandardMode
                           : kHTMLQuirksMode;
  context->SetMode(mode);
  CSSParserImpl parser(context, document.ElementSheet().Contents());
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, mode);
}

ImmutableCSSPropertyValueSet* CSSParserImpl::ParseInlineStyleDeclaration(
    const String& string,
    CSSParserMode parser_mode,
    SecureContextMode secure_context_mode) {
  auto* context =
      MakeGarbageCollected<CSSParserContext>(parser_mode, secure_context_mode);
  CSSParserImpl parser(context);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, parser_mode);
}

bool CSSParserImpl::ParseDeclarationList(
    MutableCSSPropertyValueSet* declaration,
    const String& string,
    const CSSParserContext* context) {
  CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = RuleTypeForMutableDeclaration(declaration);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, rule_type);
  if (parser.parsed_properties_.IsEmpty())
    return false;

  std::bitset<kNumCSSProperties> seen_properties;
  wtf_size_t unused_entries = parser.parsed_properties_.size();
  HeapVector<CSSPropertyValue, 64> results(unused_entries);
  HashSet<AtomicString> seen_custom_properties;
  FilterProperties(true, parser.parsed_properties_, results, unused_entries,
                   seen_properties, seen_custom_properties);
  FilterProperties(false, parser.parsed_properties_, results, unused_entries,
                   seen_properties, seen_custom_properties);
  if (unused_entries)
    results.EraseAt(0, unused_entries);
  return declaration->AddParsedProperties(results);
}

StyleRuleBase* CSSParserImpl::ParseRule(const String& string,
                                        const CSSParserContext* context,
                                        StyleSheetContents* style_sheet,
                                        AllowedRulesType allowed_rules) {
  CSSParserImpl parser(context, style_sheet);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  stream.ConsumeWhitespace();
  if (stream.UncheckedAtEnd())
    return nullptr;  // Parse error, empty rule
  StyleRuleBase* rule;
  if (stream.UncheckedPeek().GetType() == kAtKeywordToken)
    rule = parser.ConsumeAtRule(stream, allowed_rules);
  else
    rule = parser.ConsumeQualifiedRule(stream, allowed_rules);
  if (!rule)
    return nullptr;  // Parse error, failed to consume rule
  stream.ConsumeWhitespace();
  if (!rule || !stream.UncheckedAtEnd())
    return nullptr;  // Parse error, trailing garbage
  return rule;
}

ParseSheetResult CSSParserImpl::ParseStyleSheet(
    const String& string,
    const CSSParserContext* context,
    StyleSheetContents* style_sheet,
    CSSDeferPropertyParsing defer_property_parsing,
    bool allow_import_rules,
    std::unique_ptr<CachedCSSTokenizer> cached_tokenizer) {
  absl::optional<LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer> timer;
  if (context->GetDocument() && context->GetDocument()->View()) {
    timer.emplace(
        context->GetDocument()->View()->EnsureUkmAggregator().GetScopedTimer(
            static_cast<size_t>(LocalFrameUkmAggregator::kParseStyleSheet)));
  }
  TRACE_EVENT_BEGIN2("blink,blink_style", "CSSParserImpl::parseStyleSheet",
                     "baseUrl", context->BaseURL().GetString().Utf8(), "mode",
                     context->Mode());

  TRACE_EVENT_BEGIN0("blink,blink_style",
                     "CSSParserImpl::parseStyleSheet.parse");
  absl::optional<CSSTokenizerWrapper> tokenizer;
  absl::optional<CSSTokenizer> uncached_tokenizer;
  if (cached_tokenizer) {
    tokenizer.emplace(*cached_tokenizer);
  } else {
    uncached_tokenizer.emplace(string);
    tokenizer.emplace(*uncached_tokenizer);
  }
  CSSParserTokenStream stream(*tokenizer);
  CSSParserImpl parser(context, style_sheet);
  if (defer_property_parsing == CSSDeferPropertyParsing::kYes) {
    parser.lazy_state_ = MakeGarbageCollected<CSSLazyParsingState>(
        context, string, parser.style_sheet_);
  }
  ParseSheetResult result = ParseSheetResult::kSucceeded;
  bool first_rule_valid = parser.ConsumeRuleList(
      stream, kTopLevelRuleList,
      [&style_sheet, &result, allow_import_rules,
       context](StyleRuleBase* rule) {
        if (rule->IsCharsetRule())
          return;
        if (rule->IsImportRule()) {
          if (!allow_import_rules || context->IsForMarkupSanitization()) {
            result = ParseSheetResult::kHasUnallowedImportRule;
            return;
          }
        }
        style_sheet->ParserAppendRule(rule);
      });
  style_sheet->SetHasSyntacticallyValidCSSHeader(first_rule_valid);
  TRACE_EVENT_END0("blink,blink_style", "CSSParserImpl::parseStyleSheet.parse");

  TRACE_EVENT_END2("blink,blink_style", "CSSParserImpl::parseStyleSheet",
                   "tokenCount", tokenizer->TokenCount(), "length",
                   string.length());
  return result;
}

// static
CSSSelectorList CSSParserImpl::ParsePageSelector(
    CSSParserTokenRange range,
    StyleSheetContents* style_sheet,
    const CSSParserContext& context) {
  // We only support a small subset of the css-page spec.
  range.ConsumeWhitespace();
  AtomicString type_selector;
  if (range.Peek().GetType() == kIdentToken)
    type_selector = range.Consume().Value().ToAtomicString();

  AtomicString pseudo;
  if (range.Peek().GetType() == kColonToken) {
    range.Consume();
    if (range.Peek().GetType() != kIdentToken)
      return CSSSelectorList();
    pseudo = range.Consume().Value().ToAtomicString();
  }

  range.ConsumeWhitespace();
  if (!range.AtEnd())
    return CSSSelectorList();  // Parse error; extra tokens in @page selector

  std::unique_ptr<CSSParserSelector> selector;
  if (!type_selector.IsNull() && pseudo.IsNull()) {
    selector = std::make_unique<CSSParserSelector>(
        QualifiedName(g_null_atom, type_selector, g_star_atom));
  } else {
    selector = std::make_unique<CSSParserSelector>();
    if (!pseudo.IsNull()) {
      selector->SetMatch(CSSSelector::kPagePseudoClass);
      selector->UpdatePseudoPage(pseudo.LowerASCII(), context.GetDocument());
      if (selector->GetPseudoType() == CSSSelector::kPseudoUnknown)
        return CSSSelectorList();
    }
    if (!type_selector.IsNull()) {
      selector->PrependTagSelector(
          QualifiedName(g_null_atom, type_selector, g_star_atom));
    }
  }

  selector->SetForPage();
  Vector<std::unique_ptr<CSSParserSelector>> selector_vector;
  selector_vector.push_back(std::move(selector));
  CSSSelectorList selector_list =
      CSSSelectorList::AdoptSelectorVector(selector_vector);
  return selector_list;
}

std::unique_ptr<Vector<double>> CSSParserImpl::ParseKeyframeKeyList(
    const String& key_list) {
  CSSTokenizer tokenizer(key_list);
  // TODO(crbug.com/661854): Use streams instead of ranges
  return ConsumeKeyframeKeyList(CSSParserTokenRange(tokenizer.TokenizeToEOF()));
}

bool CSSParserImpl::ConsumeSupportsDeclaration(CSSParserTokenStream& stream) {
  DCHECK(parsed_properties_.IsEmpty());
  // Even though we might use an observer here, this is just to test if we
  // successfully parse the range, so we can temporarily remove the observer.
  CSSParserObserver* observer_copy = observer_;
  observer_ = nullptr;
  ConsumeDeclaration(stream, StyleRule::kStyle);
  observer_ = observer_copy;

  bool result = !parsed_properties_.IsEmpty();
  parsed_properties_.clear();
  return result;
}

void CSSParserImpl::ParseDeclarationListForInspector(
    const String& declaration,
    const CSSParserContext* context,
    CSSParserObserver& observer) {
  CSSParserImpl parser(context);
  parser.observer_ = &observer;
  CSSTokenizer tokenizer(declaration);
  observer.StartRuleHeader(StyleRule::kStyle, 0);
  observer.EndRuleHeader(1);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
}

void CSSParserImpl::ParseStyleSheetForInspector(const String& string,
                                                const CSSParserContext* context,
                                                StyleSheetContents* style_sheet,
                                                CSSParserObserver& observer) {
  CSSParserImpl parser(context, style_sheet);
  parser.observer_ = &observer;
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  bool first_rule_valid = parser.ConsumeRuleList(
      stream, kTopLevelRuleList, [&style_sheet](StyleRuleBase* rule) {
        if (rule->IsCharsetRule())
          return;
        style_sheet->ParserAppendRule(rule);
      });
  style_sheet->SetHasSyntacticallyValidCSSHeader(first_rule_valid);
}

CSSPropertyValueSet* CSSParserImpl::ParseDeclarationListForLazyStyle(
    const String& string,
    wtf_size_t offset,
    const CSSParserContext* context) {
  CSSTokenizer tokenizer(string, offset);
  CSSParserTokenStream stream(tokenizer);
  CSSParserTokenStream::BlockGuard guard(stream);
  CSSParserImpl parser(context);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, context->Mode());
}

static CSSParserImpl::AllowedRulesType ComputeNewAllowedRules(
    CSSParserImpl::AllowedRulesType allowed_rules,
    StyleRuleBase* rule) {
  if (!rule || allowed_rules == CSSParserImpl::kKeyframeRules ||
      allowed_rules == CSSParserImpl::kFontFeatureRules ||
      allowed_rules == CSSParserImpl::kTryRules ||
      allowed_rules == CSSParserImpl::kNoRules)
    return allowed_rules;
  DCHECK_LE(allowed_rules, CSSParserImpl::kRegularRules);
  if (rule->IsCharsetRule()) {
    return CSSParserImpl::kAllowLayerStatementRules;
  }
  if (rule->IsLayerStatementRule()) {
    if (allowed_rules <= CSSParserImpl::kAllowLayerStatementRules)
      return CSSParserImpl::kAllowLayerStatementRules;
    return CSSParserImpl::kRegularRules;
  }
  if (rule->IsImportRule())
    return CSSParserImpl::kAllowImportRules;
  if (rule->IsNamespaceRule())
    return CSSParserImpl::kAllowNamespaceRules;
  return CSSParserImpl::kRegularRules;
}

template <typename T>
bool CSSParserImpl::ConsumeRuleList(CSSParserTokenStream& stream,
                                    RuleListType rule_list_type,
                                    const T callback) {
  AllowedRulesType allowed_rules = kRegularRules;
  switch (rule_list_type) {
    case kTopLevelRuleList:
      allowed_rules = kAllowCharsetRules;
      break;
    case kRegularRuleList:
      allowed_rules = kRegularRules;
      break;
    case kKeyframesRuleList:
      allowed_rules = kKeyframeRules;
      break;
    case kFontFeatureRuleList:
      allowed_rules = kFontFeatureRules;
      break;
    case kPositionFallbackRuleList:
      allowed_rules = kTryRules;
      break;
    default:
      NOTREACHED();
  }

  bool seen_rule = false;
  bool first_rule_valid = false;
  while (!stream.AtEnd()) {
    StyleRuleBase* rule;
    switch (stream.UncheckedPeek().GetType()) {
      case kWhitespaceToken:
        stream.UncheckedConsume();
        continue;
      case kAtKeywordToken:
        rule = ConsumeAtRule(stream, allowed_rules);
        break;
      case kCDOToken:
      case kCDCToken:
        if (rule_list_type == kTopLevelRuleList) {
          stream.UncheckedConsume();
          continue;
        }
        [[fallthrough]];
      default:
        rule = ConsumeQualifiedRule(stream, allowed_rules);
        break;
    }
    if (!seen_rule) {
      seen_rule = true;
      first_rule_valid = rule;
    }
    if (rule) {
      allowed_rules = ComputeNewAllowedRules(allowed_rules, rule);
      callback(rule);
    }
  }

  return first_rule_valid;
}

CSSParserTokenRange ConsumeAtRulePrelude(CSSParserTokenStream& stream) {
  return stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken, kSemicolonToken>();
}

bool ConsumeEndOfPreludeForAtRuleWithoutBlock(CSSParserTokenStream& stream) {
  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    if (!stream.UncheckedAtEnd())
      stream.UncheckedConsume();  // kSemicolonToken
    return true;
  }

  // Consume the erroneous block.
  CSSParserTokenStream::BlockGuard guard(stream);
  return false;  // Parse error, we expected no block.
}

bool ConsumeEndOfPreludeForAtRuleWithBlock(CSSParserTokenStream& stream) {
  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    if (!stream.UncheckedAtEnd())
      stream.UncheckedConsume();  // kSemicolonToken
    return false;                 // Parse error, we expected a block.
  }

  return true;
}

void ConsumeErroneousAtRule(CSSParserTokenStream& stream) {
  // Consume the prelude and block if present.
  ConsumeAtRulePrelude(stream);
  if (!stream.AtEnd()) {
    if (stream.UncheckedPeek().GetType() == kLeftBraceToken)
      CSSParserTokenStream::BlockGuard guard(stream);
    else
      stream.UncheckedConsume();  // kSemicolonToken
  }
}

StyleRuleBase* CSSParserImpl::ConsumeAtRule(CSSParserTokenStream& stream,
                                            AllowedRulesType allowed_rules) {
  DCHECK_EQ(stream.Peek().GetType(), kAtKeywordToken);
  const StringView name = stream.ConsumeIncludingWhitespace().Value();
  const CSSAtRuleID id = CssAtRuleID(name);

  // @import rules have a URI component that is not technically part of the
  // prelude.
  AtomicString import_prelude_uri;
  if (allowed_rules <= kAllowImportRules && id == kCSSAtRuleImport)
    import_prelude_uri = ConsumeStringOrURI(stream);

  if (id != kCSSAtRuleInvalid && context_->IsUseCounterRecordingEnabled())
    CountAtRule(context_, id);

  if (allowed_rules == kKeyframeRules || allowed_rules == kFontFeatureRules ||
      allowed_rules == kNoRules) {
    // Parse error, no at-rules supported inside @keyframes,
    // @font-feature-values, or blocks supported inside declaration lists.
    ConsumeErroneousAtRule(stream);
    return nullptr;
  }

  stream.EnsureLookAhead();
  if (allowed_rules == kAllowCharsetRules && id == kCSSAtRuleCharset) {
    return ConsumeCharsetRule(stream);
  } else if (allowed_rules <= kAllowImportRules && id == kCSSAtRuleImport) {
    return ConsumeImportRule(std::move(import_prelude_uri), stream);
  } else if (allowed_rules <= kAllowNamespaceRules &&
             id == kCSSAtRuleNamespace) {
    return ConsumeNamespaceRule(stream);
  } else if (allowed_rules == kTryRules) {
    if (id == kCSSAtRuleTry)
      return ConsumeTryRule(stream);
    ConsumeErroneousAtRule(stream);
    return nullptr;
  } else {
    DCHECK_LE(allowed_rules, kRegularRules);

    switch (id) {
      case kCSSAtRuleContainer:
        return ConsumeContainerRule(stream);
      case kCSSAtRuleMedia:
        return ConsumeMediaRule(stream);
      case kCSSAtRuleSupports:
        return ConsumeSupportsRule(stream);
      case kCSSAtRuleViewport:
        return ConsumeViewportRule(stream);
      case kCSSAtRuleFontFace:
        return ConsumeFontFaceRule(stream);
      case kCSSAtRuleFontPaletteValues:
        return ConsumeFontPaletteValuesRule(stream);
      case kCSSAtRuleWebkitKeyframes:
        return ConsumeKeyframesRule(true, stream);
      case kCSSAtRuleKeyframes:
        return ConsumeKeyframesRule(false, stream);
      case kCSSAtRuleLayer:
        return ConsumeLayerRule(stream);
      case kCSSAtRulePage:
        return ConsumePageRule(stream);
      case kCSSAtRuleProperty:
        return ConsumePropertyRule(stream);
      case kCSSAtRuleScrollTimeline:
        return ConsumeScrollTimelineRule(stream);
      case kCSSAtRuleScope:
        return ConsumeScopeRule(stream);
      case kCSSAtRuleCounterStyle:
        return ConsumeCounterStyleRule(stream);
      case kCSSAtRulePositionFallback:
        return ConsumePositionFallbackRule(stream);
      default:
        ConsumeErroneousAtRule(stream);
        return nullptr;  // Parse error, unrecognised or not-allowed at-rule
    }
  }
}

StyleRuleBase* CSSParserImpl::ConsumeQualifiedRule(
    CSSParserTokenStream& stream,
    AllowedRulesType allowed_rules) {
  if (allowed_rules <= kRegularRules) {
    return ConsumeStyleRule(stream);
  }

  if (allowed_rules == kKeyframeRules) {
    stream.EnsureLookAhead();
    const wtf_size_t prelude_offset_start = stream.LookAheadOffset();
    const CSSParserTokenRange prelude =
        stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken>();
    const RangeOffset prelude_offset(prelude_offset_start,
                                     stream.LookAheadOffset());

    if (stream.AtEnd())
      return nullptr;  // Parse error, EOF instead of qualified rule block

    CSSParserTokenStream::BlockGuard guard(stream);
    StyleRuleKeyframe* keyframe_style_rule =
        ConsumeKeyframeStyleRule(prelude, prelude_offset, stream);
    if (keyframe_style_rule)
      context_->ReportLayoutAnimationsViolationIfNeeded(*keyframe_style_rule);
    return keyframe_style_rule;
  }
  if (allowed_rules == kFontFeatureRules) {
    stream.ConsumeWhitespace();
    if (stream.AtEnd())
      return nullptr;  // Parse error, EOF instead of qualified rule block
    bool prelude_invalid = false;
    stream.EnsureLookAhead();
    if (stream.UncheckedPeek().GetType() != kLeftBraceToken) {
      prelude_invalid = true;
      while (!stream.AtEnd() &&
             stream.UncheckedPeek().GetType() != kLeftBraceToken)
        stream.UncheckedConsumeComponentValue();
      if (stream.AtEnd())
        return nullptr;
    }

    CSSParserTokenStream::BlockGuard guard(stream);
    if (prelude_invalid)
      return nullptr;
    ConsumeDeclarationList(stream, StyleRule::kFontFace);
    return MakeGarbageCollected<StyleRuleFontFace>(
        CreateCSSPropertyValueSet(parsed_properties_, kCSSFontFaceRuleMode));
  }
  if (allowed_rules == kTryRules) {
    // We reach here only when there's a parse error. Treat everything before
    // the first block we reach as a bad prelude, then skip this block.
    stream.EnsureLookAhead();
    stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken>();
    if (!stream.AtEnd()) {
      CSSParserTokenStream::BlockGuard guard(stream);
    }
    return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

// This may still consume tokens if it fails
static AtomicString ConsumeStringOrURI(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();

  if (token.GetType() == kStringToken || token.GetType() == kUrlToken)
    return range.ConsumeIncludingWhitespace().Value().ToAtomicString();

  if (token.GetType() != kFunctionToken ||
      !EqualIgnoringASCIICase(token.Value(), "url"))
    return AtomicString();

  CSSParserTokenRange contents = range.ConsumeBlock();
  const CSSParserToken& uri = contents.ConsumeIncludingWhitespace();
  if (uri.GetType() == kBadStringToken || !contents.AtEnd())
    return AtomicString();
  DCHECK_EQ(uri.GetType(), kStringToken);
  return uri.Value().ToAtomicString();
}

StyleRuleCharset* CSSParserImpl::ConsumeCharsetRule(
    CSSParserTokenStream& stream) {
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(stream))
    return nullptr;

  const CSSParserToken& string = prelude.ConsumeIncludingWhitespace();
  if (string.GetType() != kStringToken || !prelude.AtEnd())
    return nullptr;  // Parse error, expected a single string
  return MakeGarbageCollected<StyleRuleCharset>();
}

StyleRuleImport* CSSParserImpl::ConsumeImportRule(
    const AtomicString& uri,
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(stream))
    return nullptr;

  if (uri.IsNull())
    return nullptr;  // Parse error, expected string or URI

  StyleRuleBase::LayerName layer;
  if (prelude.Peek().GetType() == kIdentToken &&
      prelude.Peek().Id() == CSSValueID::kLayer) {
    prelude.ConsumeIncludingWhitespace();
    layer = StyleRuleBase::LayerName({g_empty_atom});
  } else if (prelude.Peek().GetType() == kFunctionToken &&
             prelude.Peek().FunctionId() == CSSValueID::kLayer) {
    CSSParserTokenRange original_prelude = prelude;
    CSSParserTokenRange name_range =
        css_parsing_utils::ConsumeFunction(prelude);
    StyleRuleBase::LayerName name = ConsumeCascadeLayerName(name_range);
    if (!name.size() || !name_range.AtEnd()) {
      // Invalid layer() function can still be parsed as <general-enclosed>
      prelude = original_prelude;
    } else {
      layer = std::move(name);
    }
  }
  if (layer.size())
    context_->Count(WebFeature::kCSSCascadeLayers);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kImport, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(prelude_offset_end);
    observer_->EndRuleBody(prelude_offset_end);
  }

  return MakeGarbageCollected<StyleRuleImport>(
      uri, std::move(layer),
      MediaQueryParser::ParseMediaQuerySet(prelude,
                                           context_->GetExecutionContext()),
      context_->IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse);
}

StyleRuleNamespace* CSSParserImpl::ConsumeNamespaceRule(
    CSSParserTokenStream& stream) {
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(stream))
    return nullptr;

  AtomicString namespace_prefix;
  if (prelude.Peek().GetType() == kIdentToken)
    namespace_prefix =
        prelude.ConsumeIncludingWhitespace().Value().ToAtomicString();

  AtomicString uri(ConsumeStringOrURI(prelude));
  if (uri.IsNull() || !prelude.AtEnd())
    return nullptr;  // Parse error, expected string or URI

  return MakeGarbageCollected<StyleRuleNamespace>(namespace_prefix, uri);
}

StyleRuleMedia* CSSParserImpl::ConsumeMediaRule(CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  HeapVector<Member<StyleRuleBase>> rules;

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kMedia, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  if (style_sheet_)
    style_sheet_->SetHasMediaQueries();

  String prelude_string =
      stream
          .StringRangeAt(prelude_offset_start,
                         prelude_offset_end - prelude_offset_start)
          .ToString();
  const MediaQuerySet* media = CachedMediaQuerySet(prelude_string, prelude);
  DCHECK(media);

  ConsumeRuleList(stream, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_)
    observer_->EndRuleBody(stream.Offset());

  return MakeGarbageCollected<StyleRuleMedia>(media, rules);
}

StyleRuleSupports* CSSParserImpl::ConsumeSupportsRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSSupportsParser::Result supported =
      CSSSupportsParser::ConsumeSupportsCondition(stream, *this);
  // Check whether the entire prelude was consumed. If it wasn't, ensure we
  // consume any leftovers plus the block before returning a parse error.
  stream.ConsumeWhitespace();
  CSSParserTokenRange prelude_remainder = ConsumeAtRulePrelude(stream);
  if (!prelude_remainder.AtEnd())
    supported = CSSSupportsParser::Result::kParseFailure;
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  if (supported == CSSSupportsParser::Result::kParseFailure)
    return nullptr;  // Parse error, invalid @supports condition

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kSupports, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  const auto prelude_serialized =
      stream
          .StringRangeAt(prelude_offset_start,
                         prelude_offset_end - prelude_offset_start)
          .ToString()
          .SimplifyWhiteSpace();

  HeapVector<Member<StyleRuleBase>> rules;
  ConsumeRuleList(stream, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_)
    observer_->EndRuleBody(stream.Offset());

  return MakeGarbageCollected<StyleRuleSupports>(
      prelude_serialized, supported == CSSSupportsParser::Result::kSupported,
      rules);
}

StyleRuleViewport* CSSParserImpl::ConsumeViewportRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  const CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  // Allow @viewport rules from UA stylesheets only.
  if (!IsUASheetBehavior(context_->Mode()))
    return nullptr;

  if (!prelude.AtEnd())
    return nullptr;  // Parser error; @viewport prelude should be empty

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kViewport, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(prelude_offset_end);
    observer_->EndRuleBody(prelude_offset_end);
  }

  if (style_sheet_)
    style_sheet_->SetHasViewportRule();

  ConsumeDeclarationList(stream, StyleRule::kViewport);
  return MakeGarbageCollected<StyleRuleViewport>(
      CreateCSSPropertyValueSet(parsed_properties_, kCSSViewportRuleMode));
}

StyleRuleFontFace* CSSParserImpl::ConsumeFontFaceRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  if (!prelude.AtEnd())
    return nullptr;  // Parse error; @font-face prelude should be empty

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontFace, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(prelude_offset_end);
    observer_->EndRuleBody(prelude_offset_end);
  }

  if (style_sheet_)
    style_sheet_->SetHasFontFaceRule();

  ConsumeDeclarationList(stream, StyleRule::kFontFace);
  return MakeGarbageCollected<StyleRuleFontFace>(
      CreateCSSPropertyValueSet(parsed_properties_, kCSSFontFaceRuleMode));
}

StyleRuleKeyframes* CSSParserImpl::ConsumeKeyframesRule(
    bool webkit_prefixed,
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd())
    return nullptr;  // Parse error; expected single non-whitespace token in
                     // @keyframes header

  String name;
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToString();
  } else if (name_token.GetType() == kStringToken && webkit_prefixed) {
    context_->Count(WebFeature::kQuotedKeyframesRule);
    name = name_token.Value().ToString();
  } else {
    return nullptr;  // Parse error; expected ident token in @keyframes header
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kKeyframes, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  auto* keyframe_rule = MakeGarbageCollected<StyleRuleKeyframes>();
  ConsumeRuleList(
      stream, kKeyframesRuleList, [keyframe_rule](StyleRuleBase* keyframe) {
        keyframe_rule->ParserAppendKeyframe(To<StyleRuleKeyframe>(keyframe));
      });
  keyframe_rule->SetName(name);
  keyframe_rule->SetVendorPrefixed(webkit_prefixed);

  if (observer_)
    observer_->EndRuleBody(stream.Offset());

  return keyframe_rule;
}

StyleRulePage* CSSParserImpl::ConsumePageRule(CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  CSSSelectorList selector_list =
      ParsePageSelector(prelude, style_sheet_, *context_);
  if (!selector_list.IsValid())
    return nullptr;  // Parse error, invalid @page selector

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kPage, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kStyle);

  return MakeGarbageCollected<StyleRulePage>(
      std::move(selector_list),
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleProperty* CSSParserImpl::ConsumePropertyRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd())
    return nullptr;
  if (!CSSVariableParser::IsValidVariableName(name_token))
    return nullptr;
  String name = name_token.Value().ToString();

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kProperty, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kProperty);
  return MakeGarbageCollected<StyleRuleProperty>(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleCounterStyle* CSSParserImpl::ConsumeCounterStyleRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  AtomicString name = css_parsing_utils::ConsumeCounterStyleNameInPrelude(
      prelude, *GetContext());
  if (!name)
    return nullptr;

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kCounterStyle, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kCounterStyle);
  return MakeGarbageCollected<StyleRuleCounterStyle>(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleFontPaletteValues* CSSParserImpl::ConsumeFontPaletteValuesRule(
    CSSParserTokenStream& stream) {
  DCHECK(RuntimeEnabledFeatures::FontPaletteEnabled());

  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd())
    return nullptr;

  if (!css_parsing_utils::IsDashedIdent(name_token))
    return nullptr;
  AtomicString name = name_token.Value().ToAtomicString();
  if (!name)
    return nullptr;

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontPaletteValues,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kFontPaletteValues);
  return MakeGarbageCollected<StyleRuleFontPaletteValues>(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleScrollTimeline* CSSParserImpl::ConsumeScrollTimelineRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  if (!RuntimeEnabledFeatures::CSSScrollTimelineEnabled())
    return nullptr;

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd())
    return nullptr;
  if (!css_parsing_utils::IsTimelineName(name_token))
    return nullptr;
  String name = name_token.Value().ToString();

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kScrollTimeline,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kScrollTimeline);
  return MakeGarbageCollected<StyleRuleScrollTimeline>(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleBase* CSSParserImpl::ConsumeScopeRule(CSSParserTokenStream& stream) {
  DCHECK(RuntimeEnabledFeatures::CSSScopeEnabled());

  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kScope, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  auto* style_scope = StyleScope::Parse(prelude, context_, style_sheet_);
  if (!style_scope)
    return nullptr;

  if (observer_)
    observer_->StartRuleBody(stream.Offset());

  HeapVector<Member<StyleRuleBase>> rules;
  ConsumeRuleList(stream, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_)
    observer_->EndRuleBody(stream.Offset());

  return MakeGarbageCollected<StyleRuleScope>(*style_scope, rules);
}

StyleRuleContainer* CSSParserImpl::ConsumeContainerRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kContainer, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ContainerQueryParser query_parser(*context_);

  // <container-name>
  AtomicString name;
  if (prelude.Peek().GetType() == kIdentToken) {
    auto* ident = DynamicTo<CSSCustomIdentValue>(
        css_parsing_utils::ConsumeSingleContainerName(prelude, *context_));
    if (ident)
      name = ident->Value();
  }

  const MediaQueryExpNode* query = query_parser.ParseCondition(prelude);
  if (!query)
    return nullptr;
  ContainerQuery* container_query = MakeGarbageCollected<ContainerQuery>(
      ContainerSelector(std::move(name), *query), query);

  if (observer_)
    observer_->StartRuleBody(stream.Offset());

  HeapVector<Member<StyleRuleBase>> rules;
  ConsumeRuleList(stream, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_)
    observer_->EndRuleBody(stream.Offset());

  return MakeGarbageCollected<StyleRuleContainer>(*container_query, rules);
}

StyleRuleBase* CSSParserImpl::ConsumeLayerRule(CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();

  // @layer statement rule without style declarations.
  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(stream))
      return nullptr;

    Vector<StyleRuleBase::LayerName> names;
    while (!prelude.AtEnd()) {
      if (names.size()) {
        if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(prelude))
          return nullptr;
      }
      StyleRuleBase::LayerName name = ConsumeCascadeLayerName(prelude);
      if (!name.size())
        return nullptr;
      names.push_back(std::move(name));
    }
    if (!names.size())
      return nullptr;

    if (observer_) {
      observer_->StartRuleHeader(StyleRule::kLayerStatement,
                                 prelude_offset_start);
      observer_->EndRuleHeader(prelude_offset_end);
      observer_->StartRuleBody(prelude_offset_end);
      observer_->EndRuleBody(prelude_offset_end);
    }

    return MakeGarbageCollected<StyleRuleLayerStatement>(std::move(names));
  }

  // @layer block rule with style declarations.
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  StyleRuleBase::LayerName name;
  prelude.ConsumeWhitespace();
  if (prelude.AtEnd()) {
    name.push_back(g_empty_atom);
  } else {
    name = ConsumeCascadeLayerName(prelude);
    if (!name.size() || !prelude.AtEnd())
      return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kLayerBlock, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  HeapVector<Member<StyleRuleBase>> rules;
  ConsumeRuleList(stream, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_)
    observer_->EndRuleBody(stream.Offset());

  return MakeGarbageCollected<StyleRuleLayerBlock>(std::move(name), rules);
}

StyleRulePositionFallback* CSSParserImpl::ConsumePositionFallbackRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd())
    return nullptr;

  String name;  // <dashed-ident>
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToString();
    if (!name.StartsWith("--"))
      return nullptr;
  } else {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kPositionFallback,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  auto* position_fallback_rule =
      MakeGarbageCollected<StyleRulePositionFallback>(AtomicString(name));
  ConsumeRuleList(
      stream, kPositionFallbackRuleList,
      [position_fallback_rule](StyleRuleBase* try_rule) {
        position_fallback_rule->ParserAppendTryRule(To<StyleRuleTry>(try_rule));
      });

  if (observer_)
    observer_->EndRuleBody(stream.Offset());

  return position_fallback_rule;
}

StyleRuleTry* CSSParserImpl::ConsumeTryRule(CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream))
    return nullptr;
  CSSParserTokenStream::BlockGuard guard(stream);

  prelude.ConsumeWhitespace();
  if (!prelude.AtEnd())
    return nullptr;

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kTry, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kTry);
  return MakeGarbageCollected<StyleRuleTry>(
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleKeyframe* CSSParserImpl::ConsumeKeyframeStyleRule(
    const CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  std::unique_ptr<Vector<double>> key_list = ConsumeKeyframeKeyList(prelude);
  if (!key_list)
    return nullptr;

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kKeyframe, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
  }

  ConsumeDeclarationList(block, StyleRule::kKeyframe);

  return MakeGarbageCollected<StyleRuleKeyframe>(
      std::move(key_list),
      CreateCSSPropertyValueSet(parsed_properties_, kCSSKeyframeRuleMode));
}

StyleRule* CSSParserImpl::ConsumeStyleRule(CSSParserTokenStream& stream) {
  if (observer_)
    observer_->StartRuleHeader(StyleRule::kStyle, stream.LookAheadOffset());

  // Parse the prelude of the style rule
  CSSSelectorVector selector_vector = CSSSelectorParser::ConsumeSelector(
      stream, context_, style_sheet_, observer_);

  if (selector_vector.IsEmpty()) {
    // Read the rest of the prelude if there was an error
    stream.EnsureLookAhead();
    while (!stream.UncheckedAtEnd() &&
           stream.UncheckedPeek().GetType() != kLeftBraceToken)
      stream.UncheckedConsumeComponentValue();
  }

  if (observer_)
    observer_->EndRuleHeader(stream.LookAheadOffset());

  if (stream.AtEnd())
    return nullptr;  // Parse error, EOF instead of qualified rule block

  DCHECK_EQ(stream.Peek().GetType(), kLeftBraceToken);
  CSSParserTokenStream::BlockGuard guard(stream);

  if (selector_vector.IsEmpty())
    return nullptr;  // Parse error, invalid selector list

  // TODO(csharrison): How should we lazily parse css that needs the observer?
  if (!observer_ && lazy_state_) {
    DCHECK(style_sheet_);
    return StyleRule::Create(selector_vector,
                             MakeGarbageCollected<CSSLazyPropertyParserImpl>(
                                 stream.Offset() - 1, lazy_state_));
  }
  ConsumeDeclarationList(stream, StyleRule::kStyle);

  return StyleRule::Create(
      selector_vector,
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

void CSSParserImpl::ConsumeDeclarationList(CSSParserTokenStream& stream,
                                           StyleRule::RuleType rule_type) {
  DCHECK(parsed_properties_.IsEmpty());

  bool is_observer_rule_type =
      rule_type == StyleRule::kStyle || rule_type == StyleRule::kProperty ||
      rule_type == StyleRule::kContainer ||
      rule_type == StyleRule::kCounterStyle ||
      rule_type == StyleRule::kFontPaletteValues ||
      rule_type == StyleRule::kScrollTimeline ||
      rule_type == StyleRule::kKeyframe || rule_type == StyleRule::kScope ||
      rule_type == StyleRule::kTry;
  bool use_observer = observer_ && is_observer_rule_type;
  if (use_observer) {
    observer_->StartRuleBody(stream.Offset());
  }

  while (true) {
    // Having a lookahead may skip comments, which are used by the observer.
    DCHECK(!stream.HasLookAhead() || stream.AtEnd());

    if (use_observer && !stream.HasLookAhead()) {
      while (true) {
        wtf_size_t start_offset = stream.Offset();
        if (!stream.ConsumeCommentOrNothing())
          break;
        observer_->ObserveComment(start_offset, stream.Offset());
      }
    }

    if (stream.AtEnd())
      break;

    switch (stream.UncheckedPeek().GetType()) {
      case kWhitespaceToken:
      case kSemicolonToken:
        stream.UncheckedConsume();
        break;
      case kIdentToken: {
        {
          CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);
          ConsumeDeclaration(stream, rule_type);
          // Consume the remainder of the declaration (if any) for error
          // recovery.
          stream.ConsumeUntilPeekedTypeIs<>();
        }

        if (!stream.AtEnd())
          stream.UncheckedConsume();  // kSemicolonToken

        break;
      }
      default:
        while (!stream.UncheckedAtEnd() &&
               stream.UncheckedPeek().GetType() != kSemicolonToken) {
          stream.UncheckedConsumeComponentValue();
        }

        if (!stream.UncheckedAtEnd())
          stream.UncheckedConsume();  // kSemicolonToken

        break;
    }
  }

  if (use_observer)
    observer_->EndRuleBody(stream.LookAheadOffset());
}

void CSSParserImpl::ConsumeDeclaration(CSSParserTokenStream& stream,
                                       StyleRule::RuleType rule_type) {
  const wtf_size_t decl_offset_start = stream.Offset();

  DCHECK_EQ(stream.Peek().GetType(), kIdentToken);
  const CSSParserToken& lhs = stream.ConsumeIncludingWhitespace();
  if (stream.Peek().GetType() != kColonToken)
    return;  // Parse error.

  stream.UncheckedConsume();  // kColonToken

  CSSTokenizedValue tokenized_value = ConsumeValue(stream);

  bool important = RemoveImportantAnnotationIfPresent(tokenized_value);

  size_t properties_count = parsed_properties_.size();

  CSSPropertyID unresolved_property = CSSPropertyID::kInvalid;
  AtRuleDescriptorID atrule_id = AtRuleDescriptorID::Invalid;
  if (rule_type == StyleRule::kFontFace ||
      rule_type == StyleRule::kFontPaletteValues ||
      rule_type == StyleRule::kProperty ||
      rule_type == StyleRule::kCounterStyle ||
      rule_type == StyleRule::kScrollTimeline) {
    if (important)  // Invalid
      return;
    atrule_id = lhs.ParseAsAtRuleDescriptorID();
    AtRuleDescriptorParser::ParseAtRule(rule_type, atrule_id, tokenized_value,
                                        *context_, parsed_properties_);
  } else {
    unresolved_property = lhs.ParseAsUnresolvedCSSPropertyID(
        context_->GetExecutionContext(), context_->Mode());
  }

  // @rules other than FontFace still handled with legacy code.
  if (important &&
      (rule_type == StyleRule::kKeyframe || rule_type == StyleRule::kTry)) {
    return;
  }

  if (unresolved_property == CSSPropertyID::kVariable) {
    if (rule_type != StyleRule::kStyle && rule_type != StyleRule::kKeyframe)
      return;
    AtomicString variable_name = lhs.Value().ToAtomicString();
    bool is_animation_tainted = rule_type == StyleRule::kKeyframe;
    ConsumeVariableValue(tokenized_value, variable_name, important,
                         is_animation_tainted);
  } else if (unresolved_property != CSSPropertyID::kInvalid) {
    ConsumeDeclarationValue(tokenized_value, unresolved_property, important,
                            rule_type);
  }

  if (observer_ &&
      (rule_type == StyleRule::kStyle || rule_type == StyleRule::kKeyframe)) {
    // The end offset is the offset of the terminating token, which is peeked
    // but not yet consumed.
    observer_->ObserveProperty(decl_offset_start, stream.LookAheadOffset(),
                               important,
                               parsed_properties_.size() != properties_count);
  }
}

void CSSParserImpl::ConsumeVariableValue(
    const CSSTokenizedValue& tokenized_value,
    const AtomicString& variable_name,
    bool important,
    bool is_animation_tainted) {
  if (CSSCustomPropertyDeclaration* value =
          CSSVariableParser::ParseDeclarationValue(
              tokenized_value, is_animation_tainted, *context_)) {
    parsed_properties_.push_back(
        CSSPropertyValue(CSSPropertyName(variable_name), *value, important));
    context_->Count(context_->Mode(), CSSPropertyID::kVariable);
  }
}

void CSSParserImpl::ConsumeDeclarationValue(
    const CSSTokenizedValue& tokenized_value,
    CSSPropertyID unresolved_property,
    bool important,
    StyleRule::RuleType rule_type) {
  CSSPropertyParser::ParseValue(unresolved_property, important,
                                tokenized_value.range, context_,
                                parsed_properties_, rule_type);
}

CSSTokenizedValue CSSParserImpl::ConsumeValue(CSSParserTokenStream& stream) {
  stream.EnsureLookAhead();
  wtf_size_t value_start_offset = stream.LookAheadOffset();
  CSSParserTokenRange range = stream.ConsumeUntilPeekedTypeIs<>();
  wtf_size_t value_end_offset = stream.LookAheadOffset();

  return {range, stream.StringRangeAt(value_start_offset,
                                      value_end_offset - value_start_offset)};
}

bool CSSParserImpl::RemoveImportantAnnotationIfPresent(
    CSSTokenizedValue& tokenized_value) {
  const CSSParserToken* first = tokenized_value.range.begin();
  const CSSParserToken* last = tokenized_value.range.end() - 1;
  while (last >= first && last->GetType() == kWhitespaceToken)
    --last;
  if (last >= first && last->GetType() == kIdentToken &&
      EqualIgnoringASCIICase(last->Value(), "important")) {
    --last;
    while (last >= first && last->GetType() == kWhitespaceToken)
      --last;
    if (last >= first && last->GetType() == kDelimiterToken &&
        last->Delimiter() == '!') {
      tokenized_value.range = tokenized_value.range.MakeSubRange(first, last);

      // Truncate the text to remove the delimiter and everything after it.
      if (!tokenized_value.text.IsEmpty()) {
        DCHECK_NE(tokenized_value.text.ToString().find('!'), kNotFound);
        unsigned truncated_length = tokenized_value.text.length() - 1;
        while (tokenized_value.text[truncated_length] != '!')
          --truncated_length;
        tokenized_value.text =
            StringView(tokenized_value.text, 0, truncated_length);
      }
      return true;
    }
  }

  return false;
}

std::unique_ptr<Vector<double>> CSSParserImpl::ConsumeKeyframeKeyList(
    CSSParserTokenRange range) {
  std::unique_ptr<Vector<double>> result = std::make_unique<Vector<double>>();
  while (true) {
    range.ConsumeWhitespace();
    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (token.GetType() == kPercentageToken && token.NumericValue() >= 0 &&
        token.NumericValue() <= 100)
      result->push_back(token.NumericValue() / 100);
    else if (token.GetType() == kIdentToken &&
             EqualIgnoringASCIICase(token.Value(), "from"))
      result->push_back(0);
    else if (token.GetType() == kIdentToken &&
             EqualIgnoringASCIICase(token.Value(), "to"))
      result->push_back(1);
    else
      return nullptr;  // Parser error, invalid value in keyframe selector
    if (range.AtEnd())
      return result;
    if (range.Consume().GetType() != kCommaToken)
      return nullptr;  // Parser error
  }
}

const MediaQuerySet* CSSParserImpl::CachedMediaQuerySet(
    String prelude_string,
    CSSParserTokenRange prelude) {
  Member<const MediaQuerySet>& media =
      media_query_cache_.insert(prelude_string, nullptr).stored_value->value;
  if (!media) {
    media = MediaQueryParser::ParseMediaQuerySet(
        prelude, context_->GetExecutionContext());
  }
  DCHECK(media);
  return media.Get();
}

}  // namespace blink
