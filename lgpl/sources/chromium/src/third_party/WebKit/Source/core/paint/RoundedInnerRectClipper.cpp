// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RoundedInnerRectClipper.h"

#include "core/layout/LayoutObject.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

RoundedInnerRectClipper::RoundedInnerRectClipper(
    const LayoutObject& layout_object,
    const PaintInfo& paint_info,
    const LayoutRect& rect,
    const FloatRoundedRect& clip_rect,
    RoundedInnerRectClipperBehavior behavior)
    : layout_object_(layout_object),
      paint_info_(paint_info),
      use_paint_controller_(behavior == kApplyToDisplayList),
      clip_type_(use_paint_controller_
                     ? paint_info_.DisplayItemTypeForClipping()
                     : DisplayItem::kClipBoxPaintPhaseFirst) {
  Vector<FloatRoundedRect> rounded_rect_clips;
  if (clip_rect.IsRenderable()) {
    rounded_rect_clips.push_back(clip_rect);
  } else {
    // We create a rounded rect for each of the corners and clip it, while
    // making sure we clip opposing corners together.
    if (!clip_rect.GetRadii().TopLeft().IsEmpty() ||
        !clip_rect.GetRadii().BottomRight().IsEmpty()) {
      FloatRect top_corner(clip_rect.Rect().X(), clip_rect.Rect().Y(),
                           rect.MaxX() - clip_rect.Rect().X(),
                           rect.MaxY() - clip_rect.Rect().Y());
      FloatRoundedRect::Radii top_corner_radii;
      top_corner_radii.SetTopLeft(clip_rect.GetRadii().TopLeft());
      rounded_rect_clips.push_back(
          FloatRoundedRect(top_corner, top_corner_radii));

      FloatRect bottom_corner(rect.X().ToFloat(), rect.Y().ToFloat(),
                              clip_rect.Rect().MaxX() - rect.X().ToFloat(),
                              clip_rect.Rect().MaxY() - rect.Y().ToFloat());
      FloatRoundedRect::Radii bottom_corner_radii;
      bottom_corner_radii.SetBottomRight(clip_rect.GetRadii().BottomRight());
      rounded_rect_clips.push_back(
          FloatRoundedRect(bottom_corner, bottom_corner_radii));
    }

    if (!clip_rect.GetRadii().TopRight().IsEmpty() ||
        !clip_rect.GetRadii().BottomLeft().IsEmpty()) {
      FloatRect top_corner(rect.X().ToFloat(), clip_rect.Rect().Y(),
                           clip_rect.Rect().MaxX() - rect.X().ToFloat(),
                           rect.MaxY() - clip_rect.Rect().Y());
      FloatRoundedRect::Radii top_corner_radii;
      top_corner_radii.SetTopRight(clip_rect.GetRadii().TopRight());
      rounded_rect_clips.push_back(
          FloatRoundedRect(top_corner, top_corner_radii));

      FloatRect bottom_corner(clip_rect.Rect().X(), rect.Y().ToFloat(),
                              rect.MaxX() - clip_rect.Rect().X(),
                              clip_rect.Rect().MaxY() - rect.Y().ToFloat());
      FloatRoundedRect::Radii bottom_corner_radii;
      bottom_corner_radii.SetBottomLeft(clip_rect.GetRadii().BottomLeft());
      rounded_rect_clips.push_back(
          FloatRoundedRect(bottom_corner, bottom_corner_radii));
    }
  }

  if (use_paint_controller_) {
    paint_info_.context.GetPaintController().CreateAndAppend<ClipDisplayItem>(
        layout_object, clip_type_, LayoutRect::InfiniteIntRect(),
        rounded_rect_clips);
  } else {
    ClipDisplayItem clip_display_item(layout_object, clip_type_,
                                      LayoutRect::InfiniteIntRect(),
                                      rounded_rect_clips);
    clip_display_item.Replay(paint_info.context);
  }
}

RoundedInnerRectClipper::~RoundedInnerRectClipper() {
  DisplayItem::Type end_type = DisplayItem::ClipTypeToEndClipType(clip_type_);
  if (use_paint_controller_) {
    paint_info_.context.GetPaintController().EndItem<EndClipDisplayItem>(
        layout_object_, end_type);
  } else {
    EndClipDisplayItem end_clip_display_item(layout_object_, end_type);
    end_clip_display_item.Replay(paint_info_.context);
  }
}

}  // namespace blink
