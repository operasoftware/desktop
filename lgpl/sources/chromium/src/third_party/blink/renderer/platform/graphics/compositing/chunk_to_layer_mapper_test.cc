// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/chunk_to_layer_mapper.h"

#include <utility>
#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"

namespace blink {

class ChunkToLayerMapperTest : public testing::Test {
 protected:
  static PaintChunk Chunk(const PropertyTreeState& state) {
    DEFINE_STATIC_LOCAL(FakeDisplayItemClient, fake_client, ());
    DEFINE_STATIC_LOCAL(
        base::Optional<PaintChunk::Id>, id,
        (PaintChunk::Id(fake_client, DisplayItem::kDrawingFirst)));
    PaintChunk chunk(0, 1, *id, state);
    return chunk;
  }

  // A state containing arbitrary values which should not affect test results
  // if the state is used as a layer state.
  PropertyTreeState LayerState() {
    if (!layer_transform_) {
      layer_transform_ =
          CreateTransform(t0(), TransformationMatrix().Translate(123, 456),
                          FloatPoint3D(1, 2, 3));
      layer_clip_ =
          CreateClip(c0(), *layer_transform_, FloatRoundedRect(12, 34, 56, 78));
      layer_effect_ = EffectPaintPropertyNode::Create(
          e0(), EffectPaintPropertyNode::State{
                    layer_transform_, layer_clip_, kColorFilterLuminanceToAlpha,
                    CompositorFilterOperations(), 0.789f,
                    CompositorFilterOperations(), base::Optional<gfx::RRectF>(),
                    SkBlendMode::kSrcIn});
    }
    return PropertyTreeState(*layer_transform_, *layer_clip_, *layer_effect_);
  }

  bool HasFilterThatMovesPixels(const ChunkToLayerMapper& mapper) {
    return mapper.has_filter_that_moves_pixels_;
  }

  scoped_refptr<TransformPaintPropertyNode> layer_transform_;
  scoped_refptr<ClipPaintPropertyNode> layer_clip_;
  scoped_refptr<EffectPaintPropertyNode> layer_effect_;
};

TEST_F(ChunkToLayerMapperTest, OneChunkUsingLayerState) {
  ChunkToLayerMapper mapper(LayerState(), gfx::Vector2dF(10, 20));
  auto chunk = Chunk(LayerState());
  mapper.SwitchToChunk(chunk);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(SkMatrix::Translate(-10, -20), mapper.Transform());
  EXPECT_EQ(FloatClipRect(), mapper.ClipRect());
  EXPECT_EQ(IntRect(20, 10, 88, 99),
            mapper.MapVisualRect(IntRect(30, 30, 88, 99)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));
}

TEST_F(ChunkToLayerMapperTest, TwoChunkUsingLayerState) {
  ChunkToLayerMapper mapper(LayerState(), gfx::Vector2dF(10, 20));
  auto chunk1 = Chunk(LayerState());
  auto chunk2 = Chunk(LayerState());

  mapper.SwitchToChunk(chunk1);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(SkMatrix::Translate(-10, -20), mapper.Transform());
  EXPECT_EQ(FloatClipRect(), mapper.ClipRect());
  EXPECT_EQ(IntRect(20, 10, 88, 99),
            mapper.MapVisualRect(IntRect(30, 30, 88, 99)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));

  mapper.SwitchToChunk(chunk2);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(SkMatrix::Translate(-10, -20), mapper.Transform());
  EXPECT_EQ(FloatClipRect(), mapper.ClipRect());
  EXPECT_EQ(IntRect(20, 10, 88, 99),
            mapper.MapVisualRect(IntRect(30, 30, 88, 99)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));
}

TEST_F(ChunkToLayerMapperTest, TwoChunkSameState) {
  ChunkToLayerMapper mapper(LayerState(), gfx::Vector2dF(10, 20));
  auto transform = CreateTransform(LayerState().Transform(),
                                   TransformationMatrix().Scale(2));
  auto clip = CreateClip(LayerState().Clip(), LayerState().Transform(),
                         FloatRoundedRect(10, 10, 100, 100));
  auto& effect = LayerState().Effect();
  auto chunk1 = Chunk(PropertyTreeState(*transform, *clip, effect));
  auto chunk2 = Chunk(PropertyTreeState(*transform, *clip, effect));

  mapper.SwitchToChunk(chunk1);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  SkMatrix expected_transform = SkMatrix::Translate(-10, -20);
  expected_transform.preScale(2, 2);
  EXPECT_EQ(expected_transform, mapper.Transform());
  EXPECT_EQ(FloatRect(0, -10, 100, 100), mapper.ClipRect().Rect());
  EXPECT_TRUE(mapper.ClipRect().IsTight());
  EXPECT_EQ(IntRect(50, 40, 50, 50),
            mapper.MapVisualRect(IntRect(30, 30, 88, 99)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));

  mapper.SwitchToChunk(chunk2);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(expected_transform, mapper.Transform());
  EXPECT_EQ(FloatRect(0, -10, 100, 100), mapper.ClipRect().Rect());
  EXPECT_TRUE(mapper.ClipRect().IsTight());
  EXPECT_EQ(IntRect(50, 40, 50, 50),
            mapper.MapVisualRect(IntRect(30, 30, 88, 99)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));
}

TEST_F(ChunkToLayerMapperTest, TwoChunkDifferentState) {
  ChunkToLayerMapper mapper(LayerState(), gfx::Vector2dF(10, 20));
  auto transform1 = CreateTransform(LayerState().Transform(),
                                    TransformationMatrix().Scale(2));
  auto clip1 = CreateClip(LayerState().Clip(), LayerState().Transform(),
                          FloatRoundedRect(10, 10, 100, 100));
  auto& effect = LayerState().Effect();
  auto chunk1 = Chunk(PropertyTreeState(*transform1, *clip1, effect));

  auto transform2 = Create2DTranslation(*transform1, 20, 30);
  auto clip2 = CreateClip(LayerState().Clip(), *transform2,
                          FloatRoundedRect(0, 0, 20, 20));
  auto chunk2 = Chunk(PropertyTreeState(*transform2, *clip2, effect));

  mapper.SwitchToChunk(chunk1);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  SkMatrix expected_transform = SkMatrix::Translate(-10, -20);
  expected_transform.preScale(2, 2);
  EXPECT_EQ(expected_transform, mapper.Transform());
  EXPECT_EQ(FloatRect(0, -10, 100, 100), mapper.ClipRect().Rect());
  EXPECT_TRUE(mapper.ClipRect().IsTight());
  EXPECT_EQ(IntRect(50, 40, 50, 50),
            mapper.MapVisualRect(IntRect(30, 30, 88, 99)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));

  mapper.SwitchToChunk(chunk2);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  expected_transform.preTranslate(20, 30);
  EXPECT_EQ(expected_transform, mapper.Transform());
  EXPECT_EQ(FloatRect(30, 40, 40, 40), mapper.ClipRect().Rect());
  EXPECT_FALSE(mapper.ClipRect().IsTight());
  EXPECT_EQ(IntRect(30, 40, 40, 40),
            mapper.MapVisualRect(IntRect(0, 0, 200, 200)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));
}

TEST_F(ChunkToLayerMapperTest, SlowPath) {
  ChunkToLayerMapper mapper(LayerState(), gfx::Vector2dF(10, 20));
  auto chunk1 = Chunk(LayerState());

  // Chunk2 has a blur filter. Should use the slow path.
  CompositorFilterOperations filter2;
  filter2.AppendBlurFilter(20);
  auto effect2 = CreateFilterEffect(LayerState().Effect(), std::move(filter2));
  auto chunk2 = Chunk(PropertyTreeState(LayerState().Transform(),
                                        LayerState().Clip(), *effect2));

  // Chunk3 has a different effect which inherits from chunk2's effect.
  // Should use the slow path.
  auto effect3 = CreateOpacityEffect(*effect2, 1.f);
  auto chunk3 = Chunk(PropertyTreeState(LayerState().Transform(),
                                        LayerState().Clip(), *effect3));

  // Chunk4 has an opacity filter effect which inherits from the layer's effect.
  // Should use the fast path.
  CompositorFilterOperations filter4;
  filter4.AppendOpacityFilter(0.5);
  auto effect4 = CreateFilterEffect(LayerState().Effect(), std::move(filter4));
  auto chunk4 = Chunk(PropertyTreeState(LayerState().Transform(),
                                        LayerState().Clip(), *effect4));

  // Chunk5 goes back to the layer state.
  auto chunk5 = Chunk(LayerState());

  mapper.SwitchToChunk(chunk1);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(SkMatrix::Translate(-10, -20), mapper.Transform());
  EXPECT_EQ(FloatClipRect(), mapper.ClipRect());

  mapper.SwitchToChunk(chunk2);
  EXPECT_TRUE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(SkMatrix::Translate(-10, -20), mapper.Transform());
  EXPECT_TRUE(mapper.ClipRect().IsInfinite());
  EXPECT_EQ(IntRect(-40, -50, 208, 219),
            mapper.MapVisualRect(IntRect(30, 30, 88, 99)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));

  mapper.SwitchToChunk(chunk3);
  EXPECT_TRUE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(SkMatrix::Translate(-10, -20), mapper.Transform());
  EXPECT_TRUE(mapper.ClipRect().IsInfinite());
  EXPECT_EQ(IntRect(-40, -50, 208, 219),
            mapper.MapVisualRect(IntRect(30, 30, 88, 99)));
  EXPECT_EQ(IntRect(), mapper.MapVisualRect(IntRect()));

  mapper.SwitchToChunk(chunk4);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(SkMatrix::Translate(-10, -20), mapper.Transform());
  EXPECT_EQ(FloatClipRect(), mapper.ClipRect());

  mapper.SwitchToChunk(chunk5);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
  EXPECT_EQ(SkMatrix::Translate(-10, -20), mapper.Transform());
  EXPECT_EQ(FloatClipRect(), mapper.ClipRect());
}

TEST_F(ChunkToLayerMapperTest, SwitchToSiblingEffect) {
  auto effect1 = CreateOpacityEffect(LayerState().Effect(), 0.5f);
  auto chunk1 = Chunk(PropertyTreeState(LayerState().Transform(),
                                        LayerState().Clip(), *effect1));
  auto effect2 = CreateOpacityEffect(LayerState().Effect(), 0.5f);
  auto chunk2 = Chunk(PropertyTreeState(LayerState().Transform(),
                                        LayerState().Clip(), *effect2));

  ChunkToLayerMapper mapper(chunk1.properties.GetPropertyTreeState(),
                            gfx::Vector2dF(10, 20));
  mapper.SwitchToChunk(chunk2);
  EXPECT_FALSE(HasFilterThatMovesPixels(mapper));
}

}  // namespace blink
