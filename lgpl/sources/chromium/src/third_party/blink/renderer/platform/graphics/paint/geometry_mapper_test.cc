// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/box_reflection.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class GeometryMapperTest : public testing::Test,
                           public PaintTestConfigurations {
 public:
  const GeometryMapperClipCache::ClipCacheEntry* GetCachedClip(
      const ClipPaintPropertyNode& descendant_clip,
      const PropertyTreeState& ancestor_property_tree_state) {
    GeometryMapperClipCache::ClipAndTransform clip_and_transform(
        &ancestor_property_tree_state.Clip(),
        &ancestor_property_tree_state.Transform(), kIgnoreOverlayScrollbarSize);
    return descendant_clip.GetClipCache().GetCachedClip(clip_and_transform);
  }

  void LocalToAncestorVisualRectInternal(
      const PropertyTreeStateOrAlias& local_state,
      const PropertyTreeStateOrAlias& ancestor_state,
      FloatClipRect& mapping_rect) {
    GeometryMapper::LocalToAncestorVisualRectInternalForTesting(
        local_state.Unalias(), ancestor_state.Unalias(), mapping_rect);
  }

  void LocalToAncestorVisualRectForCompositingOverlap(
      const PropertyTreeStateOrAlias& local_state,
      const PropertyTreeStateOrAlias& ancestor_state,
      FloatClipRect& mapping_rect) {
    GeometryMapper::
        LocalToAncestorVisualRectInternalForCompositingOverlapForTesting(
            local_state.Unalias(), ancestor_state.Unalias(), mapping_rect);
  }

  bool MightOverlapForCompositing(const gfx::RectF& rect1,
                                  const PropertyTreeState& state1,
                                  const gfx::RectF& rect2,
                                  const PropertyTreeState& state2) {
    bool result = GeometryMapper::MightOverlapForCompositing(rect1, state1,
                                                             rect2, state2);
    EXPECT_EQ(result, GeometryMapper::MightOverlapForCompositing(
                          rect2, state2, rect1, state1));
    return result;
  }

  // For any rect |r|, MightOverlapForCompositing(rect1, state1, r, state2) is
  // expected to be true iff |r| intersects |rect2| in |state2|.
  void CheckOverlap(const gfx::RectF& rect1,
                    const PropertyTreeState& state1,
                    const gfx::RectF& rect2,
                    const PropertyTreeState& state2);

  void CheckMappings();
  void CheckLocalToAncestorVisualRect();
  void CheckLocalToAncestorClipRect();
  void CheckSourceToDestinationRect();
  void CheckSourceToDestinationProjection();
  void CheckCachedClip();

  // Variables required by CheckMappings(). The tests should set these
  // variables with proper values before calling CheckMappings().
  PropertyTreeStateOrAlias local_state = PropertyTreeState::Root();
  PropertyTreeStateOrAlias ancestor_state = PropertyTreeState::Root();
  gfx::RectF input_rect;
  FloatClipRect expected_visual_rect;
  absl::optional<FloatClipRect> expected_visual_rect_expanded_for_compositing;
  gfx::Vector2dF expected_translation_2d;
  absl::optional<TransformationMatrix> expected_transform;
  FloatClipRect expected_clip;
  bool expected_clip_has_transform_animation = false;
  bool expected_clip_has_sticky_transform = false;
  gfx::RectF expected_transformed_rect;
};

INSTANTIATE_PAINT_TEST_SUITE_P(GeometryMapperTest);

#define EXPECT_CLIP_RECT_EQ(expected, actual)                       \
  do {                                                              \
    SCOPED_TRACE("EXPECT_CLIP_RECT_EQ: " #expected " vs " #actual); \
    EXPECT_EQ((expected).IsInfinite(), (actual).IsInfinite());      \
    EXPECT_EQ((expected).HasRadius(), (actual).HasRadius());        \
    EXPECT_EQ((expected).IsTight(), (actual).IsTight());            \
    if (!(expected).IsInfinite())                                   \
      EXPECT_EQ((expected).Rect(), (actual).Rect());                \
  } while (false)

void GeometryMapperTest::CheckLocalToAncestorVisualRect() {
  FloatClipRect actual_visual_rect(input_rect);
  LocalToAncestorVisualRectInternal(local_state, ancestor_state,
                                    actual_visual_rect);
  EXPECT_CLIP_RECT_EQ(expected_visual_rect, actual_visual_rect);

  actual_visual_rect = FloatClipRect(input_rect);
  LocalToAncestorVisualRectForCompositingOverlap(local_state, ancestor_state,
                                                 actual_visual_rect);
  EXPECT_CLIP_RECT_EQ(expected_visual_rect_expanded_for_compositing
                          ? *expected_visual_rect_expanded_for_compositing
                          : expected_visual_rect,
                      actual_visual_rect);
}

void GeometryMapperTest::CheckLocalToAncestorClipRect() {
  FloatClipRect actual_clip_rect =
      GeometryMapper::LocalToAncestorClipRect(local_state, ancestor_state);
  EXPECT_CLIP_RECT_EQ(expected_clip, actual_clip_rect);
}

void GeometryMapperTest::CheckSourceToDestinationRect() {
  auto actual_transformed_rect = input_rect;
  GeometryMapper::SourceToDestinationRect(local_state.Transform(),
                                          ancestor_state.Transform(),
                                          actual_transformed_rect);
  EXPECT_EQ(expected_transformed_rect, actual_transformed_rect);
}

void GeometryMapperTest::CheckSourceToDestinationProjection() {
  const auto& actual_transform_to_ancestor =
      GeometryMapper::SourceToDestinationProjection(local_state.Transform(),
                                                    ancestor_state.Transform());
  if (expected_transform) {
    EXPECT_EQ(*expected_transform, actual_transform_to_ancestor.Matrix());
  } else {
    EXPECT_EQ(expected_translation_2d,
              actual_transform_to_ancestor.Translation2D());
  }
}

void GeometryMapperTest::CheckCachedClip() {
  if (&ancestor_state.Effect() != &local_state.Effect())
    return;
  const auto& local_clip = local_state.Clip().Unalias();
  const auto* cached_clip = GetCachedClip(local_clip, ancestor_state.Unalias());
  if (&ancestor_state.Clip() == &local_clip ||
      (&ancestor_state.Clip() == local_clip.Parent() &&
       &ancestor_state.Transform() == &local_clip.LocalTransformSpace())) {
    EXPECT_EQ(nullptr, cached_clip);
    return;
  }
  ASSERT_NE(nullptr, cached_clip);
  EXPECT_CLIP_RECT_EQ(expected_clip, cached_clip->clip_rect);
  EXPECT_EQ(expected_clip_has_transform_animation,
            cached_clip->has_transform_animation);
  if (RuntimeEnabledFeatures::ScrollUpdateOptimizationsEnabled()) {
    EXPECT_EQ(expected_clip_has_sticky_transform,
              cached_clip->has_sticky_transform);
  }
}

// See the data fields of GeometryMapperTest for variables that will be used in
// this function.
void GeometryMapperTest::CheckMappings() {
  CheckLocalToAncestorVisualRect();
  CheckLocalToAncestorClipRect();
  CheckSourceToDestinationRect();
  CheckSourceToDestinationProjection();
  {
    SCOPED_TRACE("Repeated check to test caching");
    CheckLocalToAncestorVisualRect();
    CheckLocalToAncestorClipRect();
    CheckSourceToDestinationRect();
    CheckSourceToDestinationProjection();
  }
  CheckCachedClip();
}

void GeometryMapperTest::CheckOverlap(const gfx::RectF& rect1,
                                      const PropertyTreeState& state1,
                                      const gfx::RectF& rect2,
                                      const PropertyTreeState& state2) {
  // How to debug: If anything fail, keep only the following line, and examine
  // whether the two visual rects from VisualRectForCompositingOverlap() are
  // equal. If not, examine the test data and GeometryMapper code.
  EXPECT_TRUE(MightOverlapForCompositing(rect1, state1, rect2, state2));

  // Test four 1x1 rects around each corner.
  gfx::RectF top_left(rect2.origin(), gfx::SizeF(1, 1));
  EXPECT_TRUE(MightOverlapForCompositing(rect1, state1, top_left, state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, top_left - gfx::Vector2dF(1, 0), state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, top_left - gfx::Vector2dF(1, 1), state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, top_left - gfx::Vector2dF(0, 1), state2));

  gfx::RectF top_right(rect2.top_right(), gfx::SizeF(1, 1));
  EXPECT_FALSE(MightOverlapForCompositing(rect1, state1, top_right, state2));
  EXPECT_TRUE(MightOverlapForCompositing(
      rect1, state1, top_right - gfx::Vector2dF(1, 0), state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, top_right - gfx::Vector2dF(1, 1), state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, top_right - gfx::Vector2dF(0, 1), state2));

  gfx::RectF bottom_right(rect2.bottom_right(), gfx::SizeF(1, 1));
  EXPECT_FALSE(MightOverlapForCompositing(rect1, state1, bottom_right, state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, bottom_right - gfx::Vector2dF(1, 0), state2));
  EXPECT_TRUE(MightOverlapForCompositing(
      rect1, state1, bottom_right - gfx::Vector2dF(1, 1), state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, bottom_right - gfx::Vector2dF(0, 1), state2));

  gfx::RectF bottom_left(rect2.bottom_left(), gfx::SizeF(1, 1));
  EXPECT_FALSE(MightOverlapForCompositing(rect1, state1, bottom_left, state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, bottom_left - gfx::Vector2dF(1, 0), state2));
  EXPECT_FALSE(MightOverlapForCompositing(
      rect1, state1, bottom_left - gfx::Vector2dF(1, 1), state2));
  EXPECT_TRUE(MightOverlapForCompositing(
      rect1, state1, bottom_left - gfx::Vector2dF(0, 1), state2));
}

TEST_P(GeometryMapperTest, Root) {
  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_transformed_rect = input_rect;
  CheckMappings();
}

TEST_P(GeometryMapperTest, IdentityTransform) {
  auto transform = Create2DTranslation(t0(), 0, 0);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_visual_rect = FloatClipRect(input_rect);
  CheckMappings();
}

TEST_P(GeometryMapperTest, TranslationTransform) {
  expected_translation_2d = gfx::Vector2dF(20, 10);
  auto transform = Create2DTranslation(t0(), 20, 10);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_transformed_rect.Offset(expected_translation_2d);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  CheckMappings();

  gfx::RectF rect = expected_transformed_rect;
  GeometryMapper::SourceToDestinationRect(t0(), local_state.Transform(), rect);
  EXPECT_EQ(input_rect, rect);
}

TEST_P(GeometryMapperTest, TranslationTransformWithAlias) {
  expected_translation_2d = gfx::Vector2dF(20, 10);
  auto real_transform = Create2DTranslation(t0(), 20, 10);
  auto transform = TransformPaintPropertyNodeAlias::Create(*real_transform);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_transformed_rect.Offset(expected_translation_2d);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  CheckMappings();

  gfx::RectF rect = expected_transformed_rect;
  GeometryMapper::SourceToDestinationRect(t0(), local_state.Transform(), rect);
  EXPECT_EQ(input_rect, rect);
}

TEST_P(GeometryMapperTest, RotationAndScaleTransform) {
  expected_transform = TransformationMatrix().Rotate(45).Scale(2);
  auto transform = CreateTransform(t0(), *expected_transform);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, RotationAndScaleTransformWithAlias) {
  expected_transform = TransformationMatrix().Rotate(45).Scale(2);
  auto real_transform = CreateTransform(t0(), *expected_transform);
  auto transform = TransformPaintPropertyNodeAlias::Create(*real_transform);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, RotationAndScaleTransformWithTransformOrigin) {
  expected_transform = TransformationMatrix().Rotate(45).Scale(2);
  auto transform =
      CreateTransform(t0(), *expected_transform, gfx::Point3F(50, 50, 0));
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transform->ApplyTransformOrigin(50, 50, 0);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, NestedTransforms) {
  auto rotate_transform = TransformationMatrix().Rotate(45);
  auto transform1 = CreateTransform(t0(), rotate_transform);

  auto scale_transform = TransformationMatrix().Scale(2);
  auto transform2 = CreateTransform(*transform1, scale_transform);
  local_state.SetTransform(*transform2);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transform = rotate_transform * scale_transform;
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, NestedTransformsFlattening) {
  auto rotate_transform = TransformationMatrix().Rotate3d(45, 0, 0);
  auto transform1 = CreateTransform(t0(), rotate_transform);

  auto inverse_rotate_transform = TransformationMatrix().Rotate3d(-45, 0, 0);
  TransformPaintPropertyNode::State inverse_state{inverse_rotate_transform};
  inverse_state.flags.flattens_inherited_transform = true;
  auto transform2 =
      TransformPaintPropertyNode::Create(*transform1, std::move(inverse_state));
  local_state.SetTransform(*transform2);

  input_rect = gfx::RectF(0, 0, 100, 100);
  rotate_transform.FlattenTo2d();
  expected_transform = rotate_transform * inverse_rotate_transform;
  expected_transform->FlattenTo2d();
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, NestedTransformsScaleAndTranslation) {
  auto scale_transform = TransformationMatrix().Scale(2);
  auto transform1 = CreateTransform(t0(), scale_transform);

  auto translate_transform = TransformationMatrix().Translate(100, 0);
  auto transform2 = CreateTransform(*transform1, translate_transform);
  local_state.SetTransform(*transform2);

  input_rect = gfx::RectF(0, 0, 100, 100);
  // Note: unlike NestedTransforms, the order of these transforms matters. This
  // tests correct order of matrix multiplication.
  expected_transform = scale_transform * translate_transform;
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, NestedTransformsIntermediateDestination) {
  auto translate_transform = TransformationMatrix().Translate(10, 20);
  auto transform1 = CreateTransform(t0(), translate_transform);

  auto scale_transform = TransformationMatrix().Scale(3);
  auto transform2 = CreateTransform(*transform1, scale_transform);

  local_state.SetTransform(*transform2);
  ancestor_state.SetTransform(*transform1);

  expected_transform = scale_transform;
  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, SimpleClip) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;  // not clipped.
  expected_clip = clip->LayoutClipRect();
  expected_visual_rect = expected_clip;
  CheckMappings();
}

TEST_P(GeometryMapperTest, UsesLayoutClipRect) {
  auto clip = CreateClip(c0(), t0(), gfx::RectF(10, 10, 50.5, 50.5),
                         FloatRoundedRect(10, 10, 50, 51));
  local_state.SetClip(*clip);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;  // not clipped.

  // GeometryMapper uses the LayoutClipRect.
  expected_clip = clip->LayoutClipRect();
  expected_visual_rect = expected_clip;
  CheckMappings();
}

TEST_P(GeometryMapperTest, SimpleClipWithAlias) {
  auto real_clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  auto clip = ClipPaintPropertyNodeAlias::Create(*real_clip);
  local_state.SetClip(*clip);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;  // not clipped.
  expected_clip = clip->Unalias().LayoutClipRect();
  expected_visual_rect = expected_clip;
  CheckMappings();
}

TEST_P(GeometryMapperTest, SimpleClipOverlayScrollbars) {
  ClipPaintPropertyNode::State clip_state(&t0(), gfx::RectF(10, 10, 50, 50),
                                          FloatRoundedRect(10, 10, 50, 50));
  clip_state.layout_clip_rect_excluding_overlay_scrollbars =
      FloatClipRect(gfx::RectF(10, 10, 45, 43));
  auto clip = ClipPaintPropertyNode::Create(c0(), std::move(clip_state));
  local_state.SetClip(*clip);

  input_rect = gfx::RectF(0, 0, 100, 100);

  FloatClipRect actual_visual_rect(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_visual_rect,
      kExcludeOverlayScrollbarSizeForHitTesting);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(gfx::RectF(10, 10, 45, 43)),
                      actual_visual_rect);

  // Check that not passing kExcludeOverlayScrollbarSizeForHitTesting gives
  // a different result.
  actual_visual_rect = FloatClipRect(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(local_state, ancestor_state,
                                            actual_visual_rect,
                                            kIgnoreOverlayScrollbarSize);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(gfx::RectF(10, 10, 50, 50)),
                      actual_visual_rect);

  FloatClipRect actual_clip_rect = GeometryMapper::LocalToAncestorClipRect(
      local_state, ancestor_state, kExcludeOverlayScrollbarSizeForHitTesting);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(gfx::RectF(10, 10, 45, 43)),
                      actual_clip_rect);

  // Check that not passing kExcludeOverlayScrollbarSizeForHitTesting gives
  // a different result.
  actual_clip_rect = GeometryMapper::LocalToAncestorClipRect(
      local_state, ancestor_state, kIgnoreOverlayScrollbarSize);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(gfx::RectF(10, 10, 50, 50)),
                      actual_clip_rect);
}

TEST_P(GeometryMapperTest, SimpleClipInclusiveIntersect) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  FloatClipRect actual_clip_rect(gfx::RectF(60, 10, 10, 10));
  GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_clip_rect,
      kIgnoreOverlayScrollbarSize, kInclusiveIntersect);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(gfx::RectF(60, 10, 0, 10)),
                      actual_clip_rect);

  // Check that not passing kExcludeOverlayScrollbarSizeForHitTesting gives
  // a different result.
  actual_clip_rect.SetRect(gfx::RectF(60, 10, 10, 10));
  GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_clip_rect,
      kIgnoreOverlayScrollbarSize, kNonInclusiveIntersect);
  EXPECT_CLIP_RECT_EQ(FloatClipRect(gfx::RectF()), actual_clip_rect);
}

TEST_P(GeometryMapperTest, SimpleClipPlusOpacity) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  auto opacity = CreateOpacityEffect(e0(), 0.99);
  local_state.SetEffect(*opacity);

  FloatClipRect actual_clip_rect(gfx::RectF(60, 10, 10, 10));
  auto intersects = GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_clip_rect);

  EXPECT_TRUE(actual_clip_rect.Rect().IsEmpty());
  EXPECT_FALSE(intersects);
}

TEST_P(GeometryMapperTest, SimpleClipPlusOpacityInclusiveIntersect) {
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  auto opacity = CreateOpacityEffect(e0(), 0.99);
  local_state.SetEffect(*opacity);

  FloatClipRect actual_clip_rect(gfx::RectF(10, 10, 10, 0));
  auto intersects = GeometryMapper::LocalToAncestorVisualRect(
      local_state, ancestor_state, actual_clip_rect,
      kIgnoreOverlayScrollbarSize, kInclusiveIntersect);

  EXPECT_TRUE(actual_clip_rect.Rect().IsEmpty());
  EXPECT_TRUE(intersects);
}

TEST_P(GeometryMapperTest, RoundedClip) {
  FloatRoundedRect rect(gfx::RectF(10, 10, 50, 50),
                        FloatRoundedRect::Radii(gfx::SizeF(1, 1), gfx::SizeF(),
                                                gfx::SizeF(), gfx::SizeF()));
  auto clip = CreateClip(c0(), t0(), rect);
  local_state.SetClip(*clip);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_clip = clip->LayoutClipRect();
  EXPECT_TRUE(expected_clip.HasRadius());
  expected_visual_rect = expected_clip;
  CheckMappings();
}

TEST_P(GeometryMapperTest, ClipPath) {
  FloatRoundedRect rect(gfx::RectF(10, 10, 50, 50),
                        FloatRoundedRect::Radii(gfx::SizeF(1, 1), gfx::SizeF(),
                                                gfx::SizeF(), gfx::SizeF()));
  auto clip = CreateClipPathClip(c0(), t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_clip = FloatClipRect(gfx::RectF(10, 10, 50, 50));
  expected_clip.ClearIsTight();
  expected_visual_rect = expected_clip;
  CheckMappings();
}

TEST_P(GeometryMapperTest, TwoClips) {
  FloatRoundedRect clip_rect1(
      gfx::RectF(10, 10, 30, 40),
      FloatRoundedRect::Radii(gfx::SizeF(1, 1), gfx::SizeF(), gfx::SizeF(),
                              gfx::SizeF()));

  auto clip1 = CreateClip(c0(), t0(), clip_rect1);
  auto clip2 = CreateClip(*clip1, t0(), FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip2);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_clip = clip1->LayoutClipRect();
  EXPECT_TRUE(expected_clip.HasRadius());
  expected_visual_rect = expected_clip;
  CheckMappings();

  ancestor_state.SetClip(*clip1);
  expected_clip = clip2->LayoutClipRect();
  expected_visual_rect = expected_clip;
  CheckMappings();
}

TEST_P(GeometryMapperTest, TwoClipsTransformAbove) {
  auto transform = Create2DTranslation(t0(), 0, 0);

  FloatRoundedRect clip_rect1(
      gfx::RectF(10, 10, 50, 50),
      FloatRoundedRect::Radii(gfx::SizeF(1, 1), gfx::SizeF(), gfx::SizeF(),
                              gfx::SizeF()));

  auto clip1 = CreateClip(c0(), *transform, clip_rect1);
  auto clip2 = CreateClip(*clip1, *transform, FloatRoundedRect(10, 10, 30, 40));
  local_state.SetClip(*clip2);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  expected_clip = clip2->LayoutClipRect();
  expected_clip.SetHasRadius();
  expected_visual_rect = expected_clip;
  CheckMappings();

  expected_clip = clip1->LayoutClipRect();
  EXPECT_TRUE(expected_clip.HasRadius());
  local_state.SetClip(*clip1);
  expected_visual_rect = expected_clip;
  CheckMappings();
}

TEST_P(GeometryMapperTest, ClipBeforeTransform) {
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateTransform(t0(), *expected_transform);
  auto clip = CreateClip(c0(), *transform, FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Intersect(clip->LayoutClipRect());
  expected_visual_rect.Map(*expected_transform);
  EXPECT_FALSE(expected_visual_rect.IsTight());
  expected_clip = clip->LayoutClipRect();
  expected_clip.Map(*expected_transform);
  EXPECT_FALSE(expected_clip.IsTight());
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  CheckMappings();
}

TEST_P(GeometryMapperTest, ExpandVisualRectWithClipBeforeAnimatingTransform) {
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateAnimatingTransform(t0(), *expected_transform);
  auto clip = CreateClip(c0(), *transform, FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Intersect(clip->LayoutClipRect());
  expected_visual_rect.Map(*expected_transform);
  // The clip has animating transform, so it doesn't apply to the visual rect.
  expected_visual_rect_expanded_for_compositing = InfiniteLooseFloatClipRect();
  EXPECT_FALSE(expected_visual_rect.IsTight());
  expected_clip = clip->LayoutClipRect();
  expected_clip.Map(*expected_transform);
  EXPECT_FALSE(expected_clip.IsTight());
  expected_clip_has_transform_animation = true;
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  CheckMappings();
}

TEST_P(GeometryMapperTest, ExpandVisualRectWithClipBeforeSticky) {
  expected_transform = TransformationMatrix().Translate(0, 100);
  auto transform = CreateTransform(t0(), *expected_transform, gfx::Point3F(),
                                   CompositingReason::kStickyPosition);
  auto clip = CreateClip(c0(), *transform, FloatRoundedRect(10, 10, 50, 50));
  local_state.SetClip(*clip);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Intersect(clip->LayoutClipRect());
  expected_visual_rect.Map(*expected_transform);
  // The clip has sticky transform, so it doesn't apply to the visual rect.
  if (RuntimeEnabledFeatures::ScrollUpdateOptimizationsEnabled()) {
    expected_visual_rect_expanded_for_compositing =
        InfiniteLooseFloatClipRect();
  }
  EXPECT_FALSE(expected_visual_rect.IsTight());
  expected_clip = clip->LayoutClipRect();
  expected_clip.Map(*expected_transform);
  EXPECT_FALSE(expected_clip.IsTight());
  expected_clip_has_sticky_transform = true;
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  CheckMappings();
}

TEST_P(GeometryMapperTest, ClipAfterTransform) {
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateTransform(t0(), *expected_transform);
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  local_state.SetClip(*clip);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Map(*expected_transform);
  expected_visual_rect.Intersect(clip->LayoutClipRect());
  EXPECT_FALSE(expected_visual_rect.IsTight());
  expected_clip = clip->LayoutClipRect();
  EXPECT_TRUE(expected_clip.IsTight());
  CheckMappings();
}

TEST_P(GeometryMapperTest, ExpandVisualRectWithClipAfterAnimatingTransform) {
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateAnimatingTransform(t0(), *expected_transform);
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  local_state.SetClip(*clip);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Map(*expected_transform);
  expected_visual_rect.Intersect(clip->LayoutClipRect());
  EXPECT_FALSE(expected_visual_rect.IsTight());
  expected_clip = clip->LayoutClipRect();
  EXPECT_TRUE(expected_clip.IsTight());
  // The visual rect is expanded first to infinity because of the transform
  // animation, then clipped by the clip.
  expected_visual_rect_expanded_for_compositing = expected_clip;
  expected_visual_rect_expanded_for_compositing->ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, ExpandVisualRectWithClipAfterSticky) {
  expected_transform = TransformationMatrix().Translate(0, 100);
  auto transform = CreateTransform(t0(), *expected_transform, gfx::Point3F(),
                                   CompositingReason::kStickyPosition);
  auto clip = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  local_state.SetClip(*clip);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Map(*expected_transform);
  expected_visual_rect.Intersect(clip->LayoutClipRect());
  EXPECT_FALSE(expected_visual_rect.IsTight());
  expected_clip = clip->LayoutClipRect();
  EXPECT_TRUE(expected_clip.IsTight());
  if (RuntimeEnabledFeatures::ScrollUpdateOptimizationsEnabled()) {
    // The visual rect is expanded first to infinity because of the sticky
    // transform, then clipped by the clip.
    expected_visual_rect_expanded_for_compositing = expected_clip;
    expected_visual_rect_expanded_for_compositing->ClearIsTight();
  }
  CheckMappings();
}

TEST_P(GeometryMapperTest, TwoClipsWithTransformBetween) {
  auto clip1 = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateTransform(t0(), *expected_transform);
  auto clip2 =
      CreateClip(*clip1, *transform, FloatRoundedRect(10, 10, 200, 200));
  local_state.SetClip(*clip2);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);

  expected_clip = clip2->LayoutClipRect();
  expected_clip.Map(*expected_transform);
  expected_clip.Intersect(clip1->LayoutClipRect());
  EXPECT_FALSE(expected_clip.IsTight());

  // All clips are performed in the space of the ancestor. In cases such as
  // this, this means the clip is not tight.
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Map(*expected_transform);
  // Intersect with all clips between local and ancestor, independently mapped
  // to ancestor space.
  expected_visual_rect.Intersect(expected_clip);
  EXPECT_FALSE(expected_visual_rect.IsTight());
  CheckMappings();
}

TEST_P(GeometryMapperTest,
       ExpandVisualRectWithTwoClipsWithAnimatingTransformBetween) {
  auto clip1 = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  expected_transform = TransformationMatrix().Rotate(45);
  auto transform = CreateAnimatingTransform(t0(), *expected_transform);
  auto clip2 =
      CreateClip(*clip1, *transform, FloatRoundedRect(10, 10, 200, 200));
  local_state.SetClip(*clip2);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);

  expected_clip = clip2->LayoutClipRect();
  expected_clip.Map(*expected_transform);
  expected_clip.Intersect(clip1->LayoutClipRect());
  EXPECT_FALSE(expected_clip.IsTight());
  expected_clip_has_transform_animation = true;
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Map(*expected_transform);
  expected_visual_rect.Intersect(expected_clip);
  EXPECT_FALSE(expected_visual_rect.IsTight());
  // The visual rect is expanded to infinity because of the transform animation,
  // then clipped by clip1. clip2 doesn't apply because it's below the animating
  // transform.
  expected_visual_rect_expanded_for_compositing = clip1->LayoutClipRect();
  expected_visual_rect_expanded_for_compositing->ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, ExpandVisualRectWithTwoClipsWithStickyBetween) {
  auto clip1 = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  expected_transform = TransformationMatrix().Translate(0, 100);
  auto transform = CreateTransform(t0(), *expected_transform, gfx::Point3F(),
                                   CompositingReason::kStickyPosition);
  auto clip2 =
      CreateClip(*clip1, *transform, FloatRoundedRect(10, 10, 200, 200));
  local_state.SetClip(*clip2);
  local_state.SetTransform(*transform);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = expected_transform->MapRect(input_rect);

  expected_clip = clip2->LayoutClipRect();
  expected_clip.Map(*expected_transform);
  expected_clip.Intersect(clip1->LayoutClipRect());
  EXPECT_FALSE(expected_clip.IsTight());
  expected_clip_has_sticky_transform = true;
  expected_visual_rect = FloatClipRect(input_rect);
  expected_visual_rect.Map(*expected_transform);
  expected_visual_rect.Intersect(expected_clip);
  EXPECT_FALSE(expected_visual_rect.IsTight());
  if (RuntimeEnabledFeatures::ScrollUpdateOptimizationsEnabled()) {
    // The visual rect is expanded to infinity because of the sticky transform,
    // then clipped by clip1. clip2 doesn't apply because it's below the sticky
    // transform.
    expected_visual_rect_expanded_for_compositing = clip1->LayoutClipRect();
    expected_visual_rect_expanded_for_compositing->ClearIsTight();
  }
  CheckMappings();
}

TEST_P(GeometryMapperTest, ExpandVisualRectForFixed) {
  // With ScrollUpdateOptimizations, we don't expand visual rect for fixed in
  // LocalToAncestorVisualRectInternal(), but check overlap before it.
  if (RuntimeEnabledFeatures::ScrollUpdateOptimizationsEnabled())
    return;

  auto above_viewport = CreateTransform(t0(), TransformationMatrix());
  auto viewport = CreateTransform(*above_viewport, TransformationMatrix());
  auto scroll_state = CreateCompositedScrollTranslationState(
      PropertyTreeState(*viewport, c0(), e0()), -100, -200,
      gfx::Rect(0, 0, 800, 600), gfx::Size(2400, 1800));

  auto fixed_transform = CreateFixedPositionTranslation(
      *viewport, 200, 200, scroll_state.Transform());
  auto child_of_fixed = Create2DTranslation(*fixed_transform, 50, 50);

  local_state.SetTransform(*child_of_fixed);
  ancestor_state.SetTransform(*viewport);

  const gfx::SizeF child_of_fixed_size(100, 100);
  input_rect = gfx::RectF(child_of_fixed_size);

  const gfx::Vector2dF descendant_offset(250, 250);
  expected_translation_2d = descendant_offset;
  expected_transformed_rect = gfx::RectF(
      gfx::PointAtOffsetFromOrigin(descendant_offset), child_of_fixed_size);
  expected_visual_rect = FloatClipRect(expected_transformed_rect);
  expected_visual_rect_expanded_for_compositing =
      FloatClipRect(gfx::RectF(150, 50, 1700, 1300));

  CheckMappings();

  // If we're not mapping to the viewport, the fixed rect should not be
  // expanded.
  ancestor_state.SetTransform(*above_viewport);
  expected_transform = TransformationMatrix().Translate(descendant_offset.x(),
                                                        descendant_offset.y());
  expected_visual_rect.ClearIsTight();
  expected_visual_rect_expanded_for_compositing = expected_visual_rect;
  CheckMappings();
}

TEST_P(GeometryMapperTest, SiblingTransforms) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  auto rotate_transform1 = TransformationMatrix().Rotate(45);
  auto transform1 = CreateTransform(t0(), rotate_transform1);

  auto rotate_transform2 = TransformationMatrix().Rotate(-45);
  auto transform2 = CreateTransform(t0(), rotate_transform2);

  auto transform1_state = PropertyTreeState::Root();
  transform1_state.SetTransform(*transform1);
  auto transform2_state = PropertyTreeState::Root();
  transform2_state.SetTransform(*transform2);

  input_rect = gfx::RectF(0, 0, 100, 100);
  FloatClipRect result_clip(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(transform1_state, transform2_state,
                                            result_clip);
  FloatClipRect expected_clip(gfx::RectF(-100, 0, 100, 100));
  // We convervatively treat any rotated clip rect as not tight, even if it's
  // rotated by 90 degrees.
  expected_clip.ClearIsTight();
  EXPECT_CLIP_RECT_EQ(expected_clip, result_clip);

  gfx::RectF result = input_rect;
  GeometryMapper::SourceToDestinationRect(*transform1, *transform2, result);
  EXPECT_EQ(gfx::RectF(-100, 0, 100, 100), result);

  result_clip = FloatClipRect(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(transform2_state, transform1_state,
                                            result_clip);
  expected_clip = FloatClipRect(gfx::RectF(0, -100, 100, 100));
  expected_clip.ClearIsTight();
  EXPECT_CLIP_RECT_EQ(expected_clip, result_clip);

  result = input_rect;
  GeometryMapper::SourceToDestinationRect(*transform2, *transform1, result);
  EXPECT_EQ(gfx::RectF(0, -100, 100, 100), result);
}

TEST_P(GeometryMapperTest, SiblingTransformsWithClip) {
  // These transforms are siblings. Thus mapping from one to the other requires
  // going through the root.
  auto rotate_transform1 = TransformationMatrix().Rotate(45);
  auto transform1 = CreateTransform(t0(), rotate_transform1);

  auto rotate_transform2 = TransformationMatrix().Rotate(-45);
  auto transform2 = CreateTransform(t0(), rotate_transform2);

  auto clip = CreateClip(c0(), *transform2, FloatRoundedRect(10, 20, 30, 40));

  auto transform1_state = PropertyTreeState::Root();
  transform1_state.SetTransform(*transform1);
  auto transform2_and_clip_state = PropertyTreeState::Root();
  transform2_and_clip_state.SetTransform(*transform2);
  transform2_and_clip_state.SetClip(*clip);

  input_rect = gfx::RectF(0, 0, 100, 100);
  FloatClipRect result(input_rect);
  LocalToAncestorVisualRectInternal(transform1_state, transform2_and_clip_state,
                                    result);
  // Because the clip of the destination state is not an ancestor of the clip
  // of the source state, no clips are applied.
  FloatClipRect expected(gfx::RectF(-100, 0, 100, 100));
  expected.ClearIsTight();
  EXPECT_CLIP_RECT_EQ(expected, result);

  result = FloatClipRect(input_rect);
  GeometryMapper::LocalToAncestorVisualRect(transform2_and_clip_state,
                                            transform1_state, result);
  expected = FloatClipRect(gfx::RectF(20, -40, 40, 30));
  // This is because the combined Rotate(45) and Rotate(-45) is not exactly a
  // translation-only transform due to calculation errors.
  expected.ClearIsTight();
  EXPECT_CLIP_RECT_EQ(expected, result);
}

TEST_P(GeometryMapperTest, FilterWithClipsAndTransforms) {
  auto transform_above_effect = Create2DTranslation(t0(), 40, 50);
  auto transform_below_effect =
      Create2DTranslation(*transform_above_effect, 20, 30);

  // This clip is between transformAboveEffect and the effect.
  auto clip_above_effect = CreateClip(c0(), *transform_above_effect,
                                      FloatRoundedRect(-100, -100, 200, 200));

  CompositorFilterOperations filters;
  filters.AppendBlurFilter(20);
  auto effect = CreateFilterEffect(e0(), *transform_above_effect,
                                   clip_above_effect.get(), filters);
  auto clip_expander =
      CreatePixelMovingFilterClipExpander(*clip_above_effect, *effect);

  // This clip is between the effect and transform_below_effect.
  auto clip_below_effect = CreateClip(*clip_expander, *transform_above_effect,
                                      FloatRoundedRect(10, 10, 100, 100));

  local_state =
      PropertyTreeState(*transform_below_effect, *clip_below_effect, *effect);

  input_rect = gfx::RectF(0, 0, 100, 100);
  // 1. transform_below_effect
  auto output = input_rect;
  output.Offset(transform_below_effect->Translation2D());
  // 2. clip_below_effect
  output.Intersect(clip_below_effect->LayoutClipRect().Rect());
  EXPECT_EQ(gfx::RectF(20, 30, 90, 80), output);
  // 3. effect (the outset is 3 times of blur amount).
  output = filters.MapRect(output);
  EXPECT_EQ(gfx::RectF(-40, -30, 210, 200), output);
  // 4. clip_above_effect
  output.Intersect(clip_above_effect->LayoutClipRect().Rect());
  EXPECT_EQ(gfx::RectF(-40, -30, 140, 130), output);
  // 5. transform_above_effect
  output.Offset(transform_above_effect->Translation2D());
  EXPECT_EQ(gfx::RectF(0, 20, 140, 130), output);

  expected_translation_2d = transform_above_effect->Translation2D() +
                            transform_below_effect->Translation2D();
  expected_transformed_rect = input_rect;
  expected_transformed_rect.Offset(expected_translation_2d);
  expected_visual_rect = FloatClipRect(output);
  expected_visual_rect.ClearIsTight();
  expected_clip = FloatClipRect(gfx::RectF(50, 60, 90, 90));
  expected_clip.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, FilterWithClipsAndTransformsWithAlias) {
  auto transform_above_effect = Create2DTranslation(t0(), 40, 50);
  auto transform_below_effect =
      Create2DTranslation(*transform_above_effect, 20, 30);

  // This clip is between transformAboveEffect and the effect.
  auto clip_above_effect = CreateClip(c0(), *transform_above_effect,
                                      FloatRoundedRect(-100, -100, 200, 200));

  CompositorFilterOperations filters;
  filters.AppendBlurFilter(20);
  auto real_effect = CreateFilterEffect(e0(), *transform_above_effect,
                                        clip_above_effect.get(), filters);
  auto clip_expander =
      CreatePixelMovingFilterClipExpander(*clip_above_effect, *real_effect);
  auto effect = EffectPaintPropertyNodeAlias::Create(*real_effect);

  // This clip is between the effect and transformBelowEffect.
  auto clip_below_effect = CreateClip(*clip_expander, *transform_above_effect,
                                      FloatRoundedRect(10, 10, 100, 100));

  local_state = PropertyTreeStateOrAlias(*transform_below_effect,
                                         *clip_below_effect, *effect);

  input_rect = gfx::RectF(0, 0, 100, 100);
  // 1. transformBelowEffect
  auto output = input_rect;
  output.Offset(transform_below_effect->Translation2D());
  // 2. clipBelowEffect
  output.Intersect(clip_below_effect->LayoutClipRect().Rect());
  EXPECT_EQ(gfx::RectF(20, 30, 90, 80), output);
  // 3. effect (the outset is 3 times of blur amount).
  output = filters.MapRect(output);
  EXPECT_EQ(gfx::RectF(-40, -30, 210, 200), output);
  // 4. clipAboveEffect
  output.Intersect(clip_above_effect->LayoutClipRect().Rect());
  EXPECT_EQ(gfx::RectF(-40, -30, 140, 130), output);
  // 5. transformAboveEffect
  output.Offset(transform_above_effect->Translation2D());
  EXPECT_EQ(gfx::RectF(0, 20, 140, 130), output);

  expected_translation_2d = transform_above_effect->Translation2D() +
                            transform_below_effect->Translation2D();
  expected_transformed_rect = input_rect;
  expected_transformed_rect.Offset(expected_translation_2d);
  expected_visual_rect = FloatClipRect(output);
  expected_visual_rect.ClearIsTight();
  expected_clip = FloatClipRect(gfx::RectF(50, 60, 90, 90));
  expected_clip.ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest,
       ExpandVisualRectWithTwoClipsWithAnimatingFilterBetween) {
  auto clip1 = CreateClip(c0(), t0(), FloatRoundedRect(10, 10, 200, 200));
  auto effect = CreateAnimatingFilterEffect(e0(), CompositorFilterOperations(),
                                            clip1.get());
  auto clip_expander = CreatePixelMovingFilterClipExpander(*clip1, *effect);

  auto clip2 =
      CreateClip(*clip_expander, t0(), FloatRoundedRect(50, 0, 200, 50));
  local_state.SetClip(*clip2);
  local_state.SetEffect(*effect);

  input_rect = gfx::RectF(0, 0, 100, 100);
  expected_transformed_rect = input_rect;
  auto output = input_rect;
  output.Intersect(clip2->LayoutClipRect().Rect());
  output.Intersect(clip1->LayoutClipRect().Rect());
  EXPECT_EQ(gfx::RectF(50, 10, 50, 40), output);
  expected_visual_rect = FloatClipRect(output);
  expected_visual_rect.ClearIsTight();
  expected_clip = clip2->LayoutClipRect();
  expected_clip.Intersect(clip1->LayoutClipRect());
  expected_clip.ClearIsTight();
  // The visual rect is expanded to infinity because of the filter animation,
  // the clipped by clip1. clip2 doesn't apply because it's below the animating
  // filter.
  expected_visual_rect_expanded_for_compositing = clip1->LayoutClipRect();
  expected_visual_rect_expanded_for_compositing->ClearIsTight();
  CheckMappings();
}

TEST_P(GeometryMapperTest, Reflection) {
  CompositorFilterOperations filters;
  filters.AppendReferenceFilter(paint_filter_builder::BuildBoxReflectFilter(
      BoxReflection(BoxReflection::kHorizontalReflection, 0), nullptr));
  auto effect = CreateFilterEffect(e0(), filters);
  auto clip_expander = CreatePixelMovingFilterClipExpander(c0(), *effect);

  local_state.SetEffect(*effect);
  local_state.SetClip(*clip_expander);

  input_rect = gfx::RectF(100, 100, 50, 50);
  expected_transformed_rect = input_rect;
  // Reflection is at (50, 100, 50, 50).
  expected_visual_rect = FloatClipRect(gfx::RectF(-150, 100, 300, 50));
  expected_visual_rect.ClearIsTight();
  expected_clip.ClearIsTight();

  CheckMappings();
}

TEST_P(GeometryMapperTest, Precision) {
  auto t1 = CreateTransform(t0(), TransformationMatrix().Scale(32767));
  auto t2 = CreateTransform(*t1, TransformationMatrix().Rotate(1));
  auto t3 = Create2DTranslation(*t2, 0, 0);
  auto t4 = Create2DTranslation(*t3, 0, 0);
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t4, *t4).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t3, *t4).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t2, *t4).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t3, *t2).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t4, *t2).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t4, *t3).IsIdentity());
  EXPECT_TRUE(
      GeometryMapper::SourceToDestinationProjection(*t2, *t3).IsIdentity());
}

TEST_P(GeometryMapperTest, MightOverlap) {
  auto t2 = Create2DTranslation(t0(), 99, 0);
  auto t3 = Create2DTranslation(t0(), 100, 0);
  auto t4 =
      CreateAnimatingTransform(t0(), TransformationMatrix().Translate(100, 0));

  gfx::RectF r(0, 0, 100, 100);
  PropertyTreeState s1 = PropertyTreeState::Root();
  PropertyTreeState s2(*t2, c0(), e0());
  PropertyTreeState s3(*t3, c0(), e0());
  PropertyTreeState s4(*t4, c0(), e0());

  EXPECT_TRUE(MightOverlapForCompositing(r, s1, r, s1));
  EXPECT_TRUE(MightOverlapForCompositing(r, s1, r, s2));
  EXPECT_FALSE(MightOverlapForCompositing(r, s1, r, s3));
  EXPECT_TRUE(MightOverlapForCompositing(r, s1, r, s4));
}

TEST_P(GeometryMapperTest, MightOverlapCommonClipAncestor) {
  auto common_clip = CreateClip(c0(), t0(), FloatRoundedRect(0, 0, 1, 1));
  auto c1 = CreateClip(*common_clip, t0(), FloatRoundedRect(0, 100, 100, 100));
  auto c2 = CreateClip(*common_clip, t0(), FloatRoundedRect(50, 100, 100, 100));
  auto c3 =
      CreateClip(*common_clip, t0(), FloatRoundedRect(100, 100, 100, 100));

  gfx::RectF r(0, 100, 200, 100);
  PropertyTreeState s1(t0(), *c1, e0());
  PropertyTreeState s2(t0(), *c2, e0());
  PropertyTreeState s3(t0(), *c3, e0());

  EXPECT_TRUE(MightOverlapForCompositing(r, s1, r, s2));
  EXPECT_FALSE(MightOverlapForCompositing(r, s1, r, s3));
  EXPECT_TRUE(MightOverlapForCompositing(r, s2, r, s3));
}

TEST_P(GeometryMapperTest, MightOverlapFixed) {
  auto viewport = CreateTransform(t0(), TransformationMatrix());
  auto scroll_state1 = CreateScrollTranslationState(
      PropertyTreeState(*viewport, c0(), e0()), -1234, -567,
      gfx::Rect(0, 0, 800, 600), gfx::Size(2400, 1800));
  auto fixed_transform = CreateFixedPositionTranslation(
      *viewport, 100, 200, scroll_state1.Transform());
  PropertyTreeState fixed_state(*fixed_transform, scroll_state1.Clip(), e0());

  // A visual rect (0, 0, 100, 100) under fixed_transform (with a (100, 200)
  // 2d translation) is expanded to (100, 200, 100 + 2400 -800, 100 + 1800 -600)
  // which is (100, 200, 1700, 1300) in the scrolling space.
  {
    SCOPED_TRACE("fixed_state and scroll_state1");
    CheckOverlap(gfx::RectF(0, 0, 100, 100), fixed_state,
                 gfx::RectF(100, 200, 1700, 1300),
                 scroll_state1.GetPropertyTreeState());
  }

  if (RuntimeEnabledFeatures::ScrollUpdateOptimizationsEnabled()) {
    {
      SCOPED_TRACE("fixed_state and scroll_state1");
      auto scroll_state2 = CreateScrollTranslationState(
          scroll_state1.GetPropertyTreeState(), -2345, -678,
          gfx::Rect(20, 10, 200, 100), gfx::Size(3000, 2000));
      // The result is false because the container rect of scroll_state2 doesn't
      // intersect with the expanded fixed-position rect in scroll_state1.
      EXPECT_FALSE(MightOverlapForCompositing(
          gfx::RectF(0, 0, 100, 100), fixed_state, gfx::RectF(1, 2, 3, 4),
          scroll_state2.GetPropertyTreeState()));
    }
    {
      SCOPED_TRACE("fixed_state and scroll_state1");
      auto scroll_state3 = CreateScrollTranslationState(
          scroll_state1.GetPropertyTreeState(), -234, -567,
          gfx::Rect(0, 300, 500, 500), gfx::Size(1000, 2000));
      EXPECT_TRUE(MightOverlapForCompositing(
          gfx::RectF(0, 0, 100, 100), fixed_state, gfx::RectF(1, 2, 3, 4),
          scroll_state3.GetPropertyTreeState()));
    }
  }
}

TEST_P(GeometryMapperTest, MightOverlapScroll) {
  // This test applies only if ScrollUpdateOptimizationsEnabled.
  if (!RuntimeEnabledFeatures::ScrollUpdateOptimizationsEnabled())
    return;

  auto viewport = CreateTransform(t0(), TransformationMatrix());
  auto scroll_state1 = CreateScrollTranslationState(
      PropertyTreeState(*viewport, c0(), e0()), -1234, -567,
      gfx::Rect(10, 20, 100, 200), gfx::Size(2400, 1800));
  auto scroll_state2 = CreateScrollTranslationState(
      scroll_state1.GetPropertyTreeState(), -2345, -678,
      gfx::Rect(20, 10, 200, 100), gfx::Size(3000, 2000));

  auto transform_outside = Create2DTranslation(*viewport, 100, 200);
  PropertyTreeState state_outside(*transform_outside, c0(), e0());

  auto transform_under_scroll1 =
      Create2DTranslation(scroll_state1.Transform(), 34, 56);
  PropertyTreeState state_under_scroll1(
      *transform_under_scroll1, scroll_state1.Clip(), scroll_state1.Effect());

  auto transform_under_scroll2 =
      Create2DTranslation(scroll_state2.Transform(), 45, 67);
  PropertyTreeState state_under_scroll2(
      *transform_under_scroll2, scroll_state2.Clip(), scroll_state2.Effect());

  // For any rect directly or indirectly under scroll_state1, we should use
  // the outer scroller's container rect to check overlap with any rect outside
  // of the scroll_state1.
  gfx::RectF outer_container_rect1_in_state_outside(-90, -180, 100, 200);
  {
    SCOPED_TRACE("scroll_state1 and state_outside");
    CheckOverlap(gfx::RectF(1, 2, 3, 4), scroll_state1.GetPropertyTreeState(),
                 outer_container_rect1_in_state_outside, state_outside);
  }
  {
    SCOPED_TRACE("state_under_scroll1 and state_outside");
    CheckOverlap(gfx::RectF(1, 2, 3, 4), state_under_scroll1,
                 outer_container_rect1_in_state_outside, state_outside);
  }
  {
    SCOPED_TRACE("scroll_state2 and state_outside");
    CheckOverlap(gfx::RectF(3, 4, 5, 6), scroll_state2.GetPropertyTreeState(),
                 outer_container_rect1_in_state_outside, state_outside);
  }
  {
    SCOPED_TRACE("state_under_scroll2 and state_outside");
    CheckOverlap(gfx::RectF(3, 4, 5, 6), state_under_scroll2,
                 outer_container_rect1_in_state_outside, state_outside);
  }

  // For any rect under scroll_state2, we should use the inner scroller's
  // container rect to check overlap with any rect between scroll_state1 and
  // scroll_state2.
  {
    SCOPED_TRACE("scroll_state2 and scroll_state1");
    CheckOverlap(gfx::RectF(5, 6, 7, 8), scroll_state2.GetPropertyTreeState(),
                 gfx::RectF(20, 10, 200, 100),
                 scroll_state1.GetPropertyTreeState());
  }
  {
    SCOPED_TRACE("state_under_scroll2 and scroll_state1");
    CheckOverlap(gfx::RectF(5, 6, 7, 8), state_under_scroll2,
                 gfx::RectF(20, 10, 200, 100),
                 scroll_state1.GetPropertyTreeState());
  }
  {
    SCOPED_TRACE("state_under_scroll2 and state_under_scroll1");
    CheckOverlap(gfx::RectF(7, 8, 9, 10), state_under_scroll2,
                 gfx::RectF(-14, -46, 200, 100), state_under_scroll1);
  }
}

}  // namespace blink
