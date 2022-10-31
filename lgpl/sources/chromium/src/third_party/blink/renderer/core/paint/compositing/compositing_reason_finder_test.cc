// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class CompositingReasonFinderTest : public RenderingTest,
                                    public PaintTestConfigurations {
 public:
  CompositingReasonFinderTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  void CheckCompositingReasonsForAnimation(bool supports_transform_animation);

  static CompositingReasons DirectReasonsForPaintProperties(
      const LayoutObject& object) {
    // We expect that the scrollable area's composited scrolling status has been
    // updated.
    return CompositingReasonFinder::DirectReasonsForPaintProperties(
        object,
        CompositingReasonFinder::DirectReasonsForPaintPropertiesExceptScrolling(
            object));
  }
};

#define EXPECT_REASONS(expect, actual)                        \
  EXPECT_EQ(expect, actual)                                   \
      << " expected: " << CompositingReason::ToString(expect) \
      << " actual: " << CompositingReason::ToString(actual)

INSTANTIATE_PAINT_TEST_SUITE_P(CompositingReasonFinderTest);

TEST_P(CompositingReasonFinderTest, PromoteTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::kTrivial3DTransform,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, PromoteNonTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(1px)'></div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::k3DTransform,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

class CompositingReasonFinderTestLowEndPlatform
    : public TestingPlatformSupport {
 public:
  bool IsLowEndDevice() override { return true; }
};

TEST_P(CompositingReasonFinderTest, DontPromoteTrivial3DWithLowEndDevice) {
  ScopedTestingPlatformSupport<CompositingReasonFinderTestLowEndPlatform>
      platform;
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::kNone,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, FixedElementShouldHaveCompositingReason) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .fixedDivStyle {
      position: fixed;
      width: 100px;
      height: 100px;
      border: 1px solid;
    }
    </style>
    <body style="background-image: linear-gradient(grey, yellow);">
      <div id="fixedDiv" class='fixedDivStyle'></div>
    </body>
  )HTML");

  ScopedFixedElementsDontOverscrollForTest fixed_elements_dont_overscroll(true);
  EXPECT_REASONS(
      CompositingReason::kFixedPosition | CompositingReason::kFixedToViewport,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("fixedDiv")));
}

TEST_P(CompositingReasonFinderTest, OnlyAnchoredStickyPositionPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .scroller {contain: paint; width: 400px; height: 400px; overflow: auto;
    will-change: transform;}
    .sticky { position: sticky; width: 10px; height: 10px;}</style>
    <div class='scroller'>
      <div id='sticky-top' class='sticky' style='top: 0px;'></div>
      <div id='sticky-no-anchor' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
  )HTML");

  EXPECT_REASONS(CompositingReason::kStickyPosition,
                 DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("sticky-top")));
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("sticky-no-anchor")));
}

TEST_P(CompositingReasonFinderTest, OnlyScrollingStickyPositionPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .scroller {
        width: 400px;
        height: 400px;
        overflow: auto;
        will-change: transform;
      }
      .sticky {
        position: sticky;
        top: 0;
        width: 10px;
        height: 10px;
      }
      .overflow-hidden {
        width: 400px;
        height: 400px;
        overflow: hidden;
        will-change: transform;
      }
    </style>
    <div class='scroller'>
      <div id='sticky-scrolling' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
    <div class='scroller'>
      <div id='sticky-no-scrolling' class='sticky'></div>
    </div>
    <div class='overflow-hidden'>
      <div id='overflow-hidden-scrolling' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
    <div class='overflow-hidden'>
      <div id='overflow-hidden-no-scrolling' class='sticky'></div>
    </div>
    <div style="position: fixed">
      <div id='under-fixed' class='sticky'></div>
    </div>
    < div style='height: 2000px;"></div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::kStickyPosition,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *GetPaintLayerByElementId("sticky-scrolling")));

  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *GetPaintLayerByElementId("sticky-no-scrolling")));

  EXPECT_REASONS(
      CompositingReason::kStickyPosition,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *GetPaintLayerByElementId("overflow-hidden-scrolling")));

  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *GetPaintLayerByElementId("overflow-hidden-no-scrolling")));

  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *GetPaintLayerByElementId("under-fixed")));
}

void CompositingReasonFinderTest::CheckCompositingReasonsForAnimation(
    bool supports_transform_animation) {
  auto* object = GetLayoutObjectByElementId("target");
  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().CreateComputedStyle();

  style->SetSubtreeWillChangeContents(false);
  style->SetHasCurrentTransformAnimation(false);
  style->SetHasCurrentScaleAnimation(false);
  style->SetHasCurrentRotateAnimation(false);
  style->SetHasCurrentTranslateAnimation(false);
  style->SetHasCurrentOpacityAnimation(false);
  style->SetHasCurrentFilterAnimation(false);
  style->SetHasCurrentBackdropFilterAnimation(false);
  object->SetStyle(style);

  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  CompositingReasons expected_reason = CompositingReason::kNone;

  style->SetHasCurrentTransformAnimation(true);
  if (supports_transform_animation)
    expected_reason |= CompositingReason::kActiveTransformAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentScaleAnimation(true);
  if (supports_transform_animation)
    expected_reason |= CompositingReason::kActiveScaleAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentRotateAnimation(true);
  if (supports_transform_animation)
    expected_reason |= CompositingReason::kActiveRotateAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentTranslateAnimation(true);
  if (supports_transform_animation)
    expected_reason |= CompositingReason::kActiveTranslateAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentOpacityAnimation(true);
  expected_reason |= CompositingReason::kActiveOpacityAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentFilterAnimation(true);
  expected_reason |= CompositingReason::kActiveFilterAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentBackdropFilterAnimation(true);
  expected_reason |= CompositingReason::kActiveBackdropFilterAnimation;
  EXPECT_EQ(expected_reason,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));
}

TEST_P(CompositingReasonFinderTest, CompositingReasonsForAnimationBox) {
  SetBodyInnerHTML("<div id='target'>Target</div>");
  CheckCompositingReasonsForAnimation(/*supports_transform_animation*/ true);
}

TEST_P(CompositingReasonFinderTest, CompositingReasonsForAnimationInline) {
  SetBodyInnerHTML("<span id='target'>Target</span>");
  CheckCompositingReasonsForAnimation(/*supports_transform_animation*/ false);
}

TEST_P(CompositingReasonFinderTest, DontPromoteEmptyIframe) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe style="width:0; height:0; border: 0;" srcdoc="<!DOCTYPE html>"></iframe>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  LocalFrameView* child_frame_view = child_frame->View();
  ASSERT_TRUE(child_frame_view);
  EXPECT_FALSE(child_frame_view->CanThrottleRendering());
}

TEST_P(CompositingReasonFinderTest, PromoteCrossOriginIframe) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe></iframe>
  )HTML");

  HTMLFrameOwnerElement* iframe =
      To<HTMLFrameOwnerElement>(GetDocument().getElementById("iframe"));
  ASSERT_TRUE(iframe);
  iframe->contentDocument()->OverrideIsInitialEmptyDocument();
  To<LocalFrame>(iframe->ContentFrame())->View()->BeginLifecycleUpdates();
  ASSERT_FALSE(iframe->ContentFrame()->IsCrossOriginToNearestMainFrame());
  UpdateAllLifecyclePhasesForTest();
  LayoutView* iframe_layout_view =
      To<LocalFrame>(iframe->ContentFrame())->ContentLayoutObject();
  ASSERT_TRUE(iframe_layout_view);
  PaintLayer* iframe_layer = iframe_layout_view->Layer();
  ASSERT_TRUE(iframe_layer);
  EXPECT_FALSE(iframe_layer->GetScrollableArea()->NeedsCompositedScrolling());
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*iframe_layout_view));

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe sandbox></iframe>
  )HTML");
  iframe = To<HTMLFrameOwnerElement>(GetDocument().getElementById("iframe"));
  iframe->contentDocument()->OverrideIsInitialEmptyDocument();
  To<LocalFrame>(iframe->ContentFrame())->View()->BeginLifecycleUpdates();
  UpdateAllLifecyclePhasesForTest();
  iframe_layout_view =
      To<LocalFrame>(iframe->ContentFrame())->ContentLayoutObject();
  iframe_layer = iframe_layout_view->Layer();
  ASSERT_TRUE(iframe_layer);
  ASSERT_TRUE(iframe->ContentFrame()->IsCrossOriginToNearestMainFrame());
  EXPECT_FALSE(iframe_layer->GetScrollableArea()->NeedsCompositedScrolling());
  EXPECT_REASONS(CompositingReason::kIFrame,
                 DirectReasonsForPaintProperties(*iframe_layout_view));

  // Make the iframe contents scrollable.
  iframe->contentDocument()->body()->setAttribute(html_names::kStyleAttr,
                                                  "height: 2000px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(iframe_layer->GetScrollableArea()->NeedsCompositedScrolling());
  EXPECT_REASONS(
      CompositingReason::kIFrame | CompositingReason::kOverflowScrolling,
      DirectReasonsForPaintProperties(*iframe_layout_view));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3DAncestor) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=target></div>
    </div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::kBackfaceInvisibility3DAncestor |
          CompositingReason::kTransform3DSceneLeaf,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3D) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=target style="transform-style: preserve-3d"></div>
    </div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::kBackfaceInvisibility3DAncestor,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3DWithInterveningDiv) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div>
        <div id=target style="position: relative"></div>
      </div>
    </div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::kBackfaceInvisibility3DAncestor,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorWithInterveningStackingDiv) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=intermediate style="isolation: isolate">
        <div id=target style="position: relative"></div>
      </div>
    </div>
  )HTML");

  EXPECT_REASONS(CompositingReason::kBackfaceInvisibility3DAncestor |
                     CompositingReason::kTransform3DSceneLeaf,
                 DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("intermediate")));
  EXPECT_REASONS(
      CompositingReason::kNone,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndFlattening) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden;">
      <div id=target></div>
    </div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::kNone,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, CompositeWithBackfaceVisibility) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div id=target style="backface-visibility: hidden;">
      <div></div>
    </div>
  )HTML");

  EXPECT_REASONS(
      CompositingReason::kNone,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, CompositedSVGText) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <text id="text" style="will-change: opacity">Text</text>
    </svg>
  )HTML");

  auto* svg_text = GetLayoutObjectByElementId("text");
  EXPECT_EQ(CompositingReason::kWillChangeOpacity,
            DirectReasonsForPaintProperties(*svg_text));
  auto* text = svg_text->SlowFirstChild();
  ASSERT_TRUE(text->IsText());
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*text));
}

TEST_P(CompositingReasonFinderTest, NotSupportedTransformAnimationsOnSVG) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { animation: transformKeyframes 1s infinite; }
      @keyframes transformKeyframes {
        0% { transform: rotate(-5deg); }
        100% { transform: rotate(5deg); }
      }
    </style>
    <svg>
      <defs id="defs" />
      <text id="text">text content
        <tspan id="tspan">tspan content</tspan>
      </text>
      <filter>
        <feBlend id="feBlend"></feBlend>
      </filter>
    </svg>
  )HTML");

  auto* defs = GetLayoutObjectByElementId("defs");
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*defs));

  auto* text = GetLayoutObjectByElementId("text");
  EXPECT_REASONS(CompositingReason::kActiveTransformAnimation,
                 DirectReasonsForPaintProperties(*text));

  auto* text_content = text->SlowFirstChild();
  ASSERT_TRUE(text_content->IsText());
  EXPECT_EQ(CompositingReason::kNone,
            DirectReasonsForPaintProperties(*text_content));

  auto* tspan = GetLayoutObjectByElementId("tspan");
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*tspan));

  auto* tspan_content = tspan->SlowFirstChild();
  ASSERT_TRUE(tspan_content->IsText());
  EXPECT_EQ(CompositingReason::kNone,
            DirectReasonsForPaintProperties(*tspan_content));

  auto* feBlend = GetLayoutObjectByElementId("feBlend");
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*feBlend));
}

TEST_P(CompositingReasonFinderTest, WillChangeScrollPosition) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 100px; overflow: scroll;
                            will-change: scroll-position">
      <div style="height: 2000px"></div>
    </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  EXPECT_TRUE(CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
      *target, CompositingReason::kNone));
  EXPECT_REASONS(CompositingReason::kOverflowScrolling,
                 DirectReasonsForPaintProperties(*target));

  GetDocument().getElementById("target")->RemoveInlineStyleProperty(
      CSSPropertyID::kWillChange);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
      *target, CompositingReason::kNone));
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*target));
}

}  // namespace blink
