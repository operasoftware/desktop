// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class TestCSSParserObserver : public CSSParserObserver {
 public:
  void StartRuleHeader(StyleRule::RuleType rule_type,
                       unsigned offset) override {
    rule_type_ = rule_type;
    rule_header_start_ = offset;
  }
  void EndRuleHeader(unsigned offset) override { rule_header_end_ = offset; }

  void ObserveSelector(unsigned start_offset, unsigned end_offset) override {}
  void StartRuleBody(unsigned offset) override { rule_body_start_ = offset; }
  void EndRuleBody(unsigned offset) override { rule_body_end_ = offset; }
  void ObserveProperty(unsigned start_offset,
                       unsigned end_offset,
                       bool is_important,
                       bool is_parsed) override {}
  void ObserveComment(unsigned start_offset, unsigned end_offset) override {}

  StyleRule::RuleType rule_type_ = StyleRule::RuleType::kStyle;
  unsigned rule_header_start_ = 0;
  unsigned rule_header_end_ = 0;
  unsigned rule_body_start_ = 0;
  unsigned rule_body_end_ = 0;
};

TEST(CSSParserImplTest, AtImportOffsets) {
  String sheet_text = "@import 'test.css';";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ImportRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_, StyleRule::RuleType::kImport);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 18u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 18u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 18u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 18u);
}

TEST(CSSParserImplTest, AtMediaOffsets) {
  String sheet_text = "@media screen { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_, StyleRule::RuleType::kMedia);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 7u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 14u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 15u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 16u);
}

TEST(CSSParserImplTest, AtSupportsOffsets) {
  String sheet_text = "@supports (display:none) { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kSupports);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 10u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 25u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 26u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 27u);
}

TEST(CSSParserImplTest, AtFontFaceOffsets) {
  String sheet_text = "@font-face { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kFontFace);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 11u);
}

TEST(CSSParserImplTest, AtKeyframesOffsets) {
  String sheet_text = "@keyframes test { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kKeyframes);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 16u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 17u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 18u);
}

TEST(CSSParserImplTest, AtPageOffsets) {
  String sheet_text = "@page :first { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_, StyleRule::RuleType::kPage);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 6u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 13u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 14u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 15u);
}

TEST(CSSParserImplTest, AtPropertyOffsets) {
  String sheet_text = "@property --test { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kProperty);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 10u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 17u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 18u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 19u);
}

TEST(CSSParserImplTest, AtCounterStyleOffsets) {
  String sheet_text = "@counter-style test { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kCounterStyle);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 15u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 20u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 21u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 22u);
}

TEST(CSSParserImplTest, AtContainerOffsets) {
  ScopedCSSContainerQueriesForTest scoped_feature(true);

  String sheet_text = "@container (max-width: 100px) { }";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kContainer);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 30u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 31u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 32u);
}

TEST(CSSParserImplTest, AtContainerDisabled) {
  String rule = "@container (max-width: 100px) { }";
  {
    ScopedCSSContainerQueriesForTest scoped_feature(true);
    Document* document = Document::CreateForTest();
    EXPECT_TRUE(css_test_helpers::ParseRule(*document, rule));
  }
  {
    ScopedCSSContainerQueriesForTest scoped_feature(false);
    Document* document = Document::CreateForTest();
    EXPECT_FALSE(css_test_helpers::ParseRule(*document, rule));
  }
}

TEST(CSSParserImplTest, RemoveImportantAnnotationIfPresent) {
  struct TestCase {
    String input;
    String expected_text;
    bool expected_is_important;
  };
  static const TestCase test_cases[] = {
      {"", "", false},
      {"!important", "", true},
      {" !important", " ", true},
      {"!", "!", false},
      {"1px", "1px", false},
      {"2px!important", "2px", true},
      {"3px !important", "3px ", true},
      {"4px ! important", "4px ", true},
      {"5px !important ", "5px ", true},
      {"6px !!important", "6px !", true},
      {"7px !important !important", "7px !important ", true},
      {"8px important", "8px important", false},
  };
  for (auto current_case : test_cases) {
    CSSTokenizer tokenizer(current_case.input);
    CSSParserTokenStream stream(tokenizer);
    CSSTokenizedValue tokenized_value = CSSParserImpl::ConsumeValue(stream);
    SCOPED_TRACE(current_case.input);
    bool is_important =
        CSSParserImpl::RemoveImportantAnnotationIfPresent(tokenized_value);
    EXPECT_EQ(is_important, current_case.expected_is_important);
    EXPECT_EQ(tokenized_value.text.ToString(), current_case.expected_text);
  }
}

TEST(CSSParserImplTest, InvalidLayerRules) {
  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();

  // At most one layer name in an @layer block rule
  EXPECT_FALSE(ParseRule(*document, "@layer foo, bar { }"));

  // Layers must be named in an @layer statement rule
  EXPECT_FALSE(ParseRule(*document, "@layer ;"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo, , bar;"));

  // Invalid layer names
  EXPECT_FALSE(ParseRule(*document, "@layer foo.bar. { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo.bar.;"));
  EXPECT_FALSE(ParseRule(*document, "@layer .foo.bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer .foo.bar;"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo. bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo. bar;"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo bar;"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo/bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo/bar;"));
}

TEST(CSSParserImplTest, ValidLayerBlockRule) {
  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();

  // Basic named layer
  {
    String rule = "@layer foo { }";
    auto* parsed = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(1u, parsed->GetName().size());
    EXPECT_EQ("foo", parsed->GetName()[0]);
  }

  // Unnamed layer
  {
    String rule = "@layer { }";
    auto* parsed = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(1u, parsed->GetName().size());
    EXPECT_EQ(g_empty_atom, parsed->GetName()[0]);
  }

  // Sub-layer declared directly
  {
    String rule = "@layer foo.bar { }";
    auto* parsed = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2u, parsed->GetName().size());
    EXPECT_EQ("foo", parsed->GetName()[0]);
    EXPECT_EQ("bar", parsed->GetName()[1]);
  }
}

TEST(CSSParserImplTest, ValidLayerStatementRule) {
  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();

  {
    String rule = "@layer foo;";
    auto* parsed =
        DynamicTo<StyleRuleLayerStatement>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(1u, parsed->GetNames().size());
    ASSERT_EQ(1u, parsed->GetNames()[0].size());
    EXPECT_EQ("foo", parsed->GetNames()[0][0]);
  }

  {
    String rule = "@layer foo, bar;";
    auto* parsed =
        DynamicTo<StyleRuleLayerStatement>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2u, parsed->GetNames().size());
    ASSERT_EQ(1u, parsed->GetNames()[0].size());
    EXPECT_EQ("foo", parsed->GetNames()[0][0]);
    ASSERT_EQ(1u, parsed->GetNames()[1].size());
    EXPECT_EQ("bar", parsed->GetNames()[1][0]);
  }

  {
    String rule = "@layer foo, bar.baz;";
    auto* parsed =
        DynamicTo<StyleRuleLayerStatement>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2u, parsed->GetNames().size());
    ASSERT_EQ(1u, parsed->GetNames()[0].size());
    EXPECT_EQ("foo", parsed->GetNames()[0][0]);
    ASSERT_EQ(2u, parsed->GetNames()[1].size());
    EXPECT_EQ("bar", parsed->GetNames()[1][0]);
    EXPECT_EQ("baz", parsed->GetNames()[1][1]);
  }
}

TEST(CSSParserImplTest, NestedLayerRules) {
  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();

  // Block rule as a child rule.
  {
    String rule = "@layer foo { @layer bar { } }";
    auto* foo = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(foo);
    ASSERT_EQ(1u, foo->GetName().size());
    EXPECT_EQ("foo", foo->GetName()[0]);
    ASSERT_EQ(1u, foo->ChildRules().size());

    auto* bar = DynamicTo<StyleRuleLayerBlock>(foo->ChildRules()[0].Get());
    ASSERT_TRUE(bar);
    ASSERT_EQ(1u, bar->GetName().size());
    EXPECT_EQ("bar", bar->GetName()[0]);
  }

  // Statement rule as a child rule.
  {
    String rule = "@layer foo { @layer bar, baz; }";
    auto* foo = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(foo);
    ASSERT_EQ(1u, foo->GetName().size());
    EXPECT_EQ("foo", foo->GetName()[0]);
    ASSERT_EQ(1u, foo->ChildRules().size());

    auto* barbaz =
        DynamicTo<StyleRuleLayerStatement>(foo->ChildRules()[0].Get());
    ASSERT_TRUE(barbaz);
    ASSERT_EQ(2u, barbaz->GetNames().size());
    ASSERT_EQ(1u, barbaz->GetNames()[0].size());
    EXPECT_EQ("bar", barbaz->GetNames()[0][0]);
    ASSERT_EQ(1u, barbaz->GetNames()[1].size());
    EXPECT_EQ("baz", barbaz->GetNames()[1][0]);
  }

  // Nested in an unnamed layer.
  {
    String rule = "@layer { @layer foo; @layer bar { } }";
    auto* parent = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(parent);
    ASSERT_EQ(1u, parent->GetName().size());
    EXPECT_EQ(g_empty_atom, parent->GetName()[0]);
    ASSERT_EQ(2u, parent->ChildRules().size());

    auto* foo =
        DynamicTo<StyleRuleLayerStatement>(parent->ChildRules()[0].Get());
    ASSERT_TRUE(foo);
    ASSERT_EQ(1u, foo->GetNames().size());
    ASSERT_EQ(1u, foo->GetNames()[0].size());
    EXPECT_EQ("foo", foo->GetNames()[0][0]);

    auto* bar = DynamicTo<StyleRuleLayerBlock>(parent->ChildRules()[1].Get());
    ASSERT_TRUE(bar);
    ASSERT_EQ(1u, bar->GetName().size());
    EXPECT_EQ("bar", bar->GetName()[0]);
  }
}

TEST(CSSParserImplTest, LayeredImportRules) {
  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();

  {
    String rule = "@import url(foo.css) layer;";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ(g_empty_atom, parsed->GetLayerName()[0]);
  }

  {
    String rule = "@import url(foo.css) layer(bar);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ("bar", parsed->GetLayerName()[0]);
  }

  {
    String rule = "@import url(foo.css) layer(bar.baz);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(2u, parsed->GetLayerName().size());
    EXPECT_EQ("bar", parsed->GetLayerName()[0]);
    EXPECT_EQ("baz", parsed->GetLayerName()[1]);
  }
}

TEST(CSSParserImplTest, LayeredImportRulesInvalid) {
  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();

  // Invalid layer declarations in @import rules should not make the entire rule
  // invalid. They should be parsed as <general-enclosed> and have no effect.

  {
    String rule = "@import url(foo.css) layer();";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->IsLayered());
    EXPECT_TRUE(parsed->MediaQueries()->HasUnknown());
  }

  {
    String rule = "@import url(foo.css) layer(bar, baz);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->IsLayered());
    EXPECT_TRUE(parsed->MediaQueries()->HasUnknown());
  }

  {
    String rule = "@import url(foo.css) layer(bar.baz.);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->IsLayered());
    EXPECT_TRUE(parsed->MediaQueries()->HasUnknown());
  }
}

TEST(CSSParserImplTest, LayeredImportRulesMultipleLayers) {
  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();

  // If an @import rule has more than one layer keyword/function, only the first
  // one is parsed as layer, and the remaining ones are parsed as
  // <general-enclosed> and hence have no effect.

  {
    String rule = "@import url(foo.css) layer layer;";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ(g_empty_atom, parsed->GetLayerName()[0]);
    EXPECT_EQ("not all", parsed->MediaQueries()->MediaText());
  }

  {
    String rule = "@import url(foo.css) layer layer(bar);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ(g_empty_atom, parsed->GetLayerName()[0]);
    EXPECT_TRUE(parsed->MediaQueries()->HasUnknown());
  }

  {
    String rule = "@import url(foo.css) layer(bar) layer;";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ("bar", parsed->GetLayerName()[0]);
    EXPECT_EQ("not all", parsed->MediaQueries()->MediaText());
  }
}

TEST(CSSParserImplTest, CorrectAtRuleOrderingWithLayers) {
  String sheet_text = R"CSS(
    @layer foo;
    @import url(bar.css) layer(bar);
    @namespace url(http://www.w3.org/1999/xhtml);
    @layer baz;
    @layer qux { }
  )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);

  // All rules should parse successfully.
  EXPECT_EQ(1u, sheet->PreImportLayerStatementRules().size());
  EXPECT_EQ(1u, sheet->ImportRules().size());
  EXPECT_EQ(1u, sheet->NamespaceRules().size());
  EXPECT_EQ(2u, sheet->ChildRules().size());
}

TEST(CSSParserImplTest, EmptyLayerStatementsAtWrongPositions) {
  {
    // @layer interleaving with @import rules
    String sheet_text = R"CSS(
      @layer foo;
      @import url(bar.css) layer(bar);
      @layer baz;
      @import url(qux.css);
    )CSS";
    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
    CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);

    EXPECT_EQ(1u, sheet->PreImportLayerStatementRules().size());
    EXPECT_EQ(1u, sheet->ChildRules().size());

    // After parsing @layer baz, @import rules are no longer allowed, so the
    // second @import rule should be ignored.
    ASSERT_EQ(1u, sheet->ImportRules().size());
    EXPECT_TRUE(sheet->ImportRules()[0]->IsLayered());
  }

  {
    // @layer between @import and @namespace rules
    String sheet_text = R"CSS(
      @layer foo;
      @import url(bar.css) layer(bar);
      @layer baz;
      @namespace url(http://www.w3.org/1999/xhtml);
    )CSS";
    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
    CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);

    EXPECT_EQ(1u, sheet->PreImportLayerStatementRules().size());
    EXPECT_EQ(1u, sheet->ImportRules().size());
    EXPECT_EQ(1u, sheet->ChildRules().size());

    // After parsing @layer baz, @namespace rules are no longer allowed.
    EXPECT_EQ(0u, sheet->NamespaceRules().size());
  }
}

TEST(CSSParserImplTest, EmptyLayerStatementAfterRegularRule) {
  // Empty @layer statements after regular rules are parsed as regular rules.

  String sheet_text = R"CSS(
    .element { color: green; }
    @layer foo, bar;
  )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);

  EXPECT_EQ(0u, sheet->PreImportLayerStatementRules().size());
  EXPECT_EQ(2u, sheet->ChildRules().size());
  EXPECT_TRUE(sheet->ChildRules()[0]->IsStyleRule());
  EXPECT_TRUE(sheet->ChildRules()[1]->IsLayerStatementRule());
}

TEST(CSSParserImplTest, FontPaletteValuesDisabled) {
  ScopedFontPaletteForTest disabled_scope(false);

  // @font-palette-values rules should be ignored when the feature is disabled.

  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();
  EXPECT_FALSE(ParseRule(*document, "@font-palette-values foo;"));
  EXPECT_FALSE(ParseRule(*document, "@font-palette-values foo { }"));
  EXPECT_FALSE(ParseRule(*document, "@font-palette-values foo.bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@font-palette-values { }"));
}

TEST(CSSParserImplTest, FontPaletteValuesBasicRuleParsing) {
  ScopedFontPaletteForTest enabled_scope(true);
  using css_test_helpers::ParseRule;
  Document* document = Document::CreateForTest();
  String rule = R"CSS(@font-palette-values --myTestPalette {
    font-family: testFamily;
    base-palette: 0;
    override-colors: 0 red, 1 blue;
  })CSS";
  auto* parsed =
      DynamicTo<StyleRuleFontPaletteValues>(ParseRule(*document, rule));
  ASSERT_TRUE(parsed);
  ASSERT_EQ("--myTestPalette", parsed->GetName());
  ASSERT_EQ("testFamily",
            DynamicTo<CSSFontFamilyValue>(parsed->GetFontFamily())->Value());
  ASSERT_EQ(
      0, DynamicTo<CSSPrimitiveValue>(parsed->GetBasePalette())->GetIntValue());
  ASSERT_TRUE(parsed->GetOverrideColors()->IsValueList());
  ASSERT_EQ(2u, DynamicTo<CSSValueList>(parsed->GetOverrideColors())->length());
}

}  // namespace blink
