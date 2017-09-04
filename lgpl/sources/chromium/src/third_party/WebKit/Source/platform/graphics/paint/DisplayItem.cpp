// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

struct SameSizeAsDisplayItem {
  virtual ~SameSizeAsDisplayItem() {}  // Allocate vtable pointer.
  void* pointer;
  int i;
#ifndef NDEBUG
  WTF::String debug_string_;
#endif
};
static_assert(sizeof(DisplayItem) == sizeof(SameSizeAsDisplayItem),
              "DisplayItem should stay small");

#ifndef NDEBUG

static WTF::String PaintPhaseAsDebugString(int paint_phase) {
  // Must be kept in sync with PaintPhase.
  switch (paint_phase) {
    case 0:
      return "PaintPhaseBlockBackground";
    case 1:
      return "PaintPhaseSelfBlockBackground";
    case 2:
      return "PaintPhaseChildBlockBackgrounds";
    case 3:
      return "PaintPhaseFloat";
    case 4:
      return "PaintPhaseForeground";
    case 5:
      return "PaintPhaseOutline";
    case 6:
      return "PaintPhaseSelfOutline";
    case 7:
      return "PaintPhaseChildOutlines";
    case 8:
      return "PaintPhaseSelection";
    case 9:
      return "PaintPhaseTextClip";
    case 10:
      return "PaintPhaseMask";
    case DisplayItem::kPaintPhaseMax:
      return "PaintPhaseClippingMask";
    default:
      NOTREACHED();
      return "Unknown";
  }
}

#define PAINT_PHASE_BASED_DEBUG_STRINGS(Category)          \
  if (type >= DisplayItem::k##Category##PaintPhaseFirst && \
      type <= DisplayItem::k##Category##PaintPhaseLast)    \
    return #Category + PaintPhaseAsDebugString(            \
                           type - DisplayItem::k##Category##PaintPhaseFirst);

#define DEBUG_STRING_CASE(DisplayItemName) \
  case DisplayItem::k##DisplayItemName:    \
    return #DisplayItemName

#define DEFAULT_CASE \
  default:           \
    NOTREACHED();    \
    return "Unknown"

static WTF::String SpecialDrawingTypeAsDebugString(DisplayItem::Type type) {
  if (type >= DisplayItem::kTableCollapsedBorderUnalignedBase) {
    if (type <= DisplayItem::kTableCollapsedBorderBase)
      return "TableCollapsedBorderAlignment";
    if (type <= DisplayItem::kTableCollapsedBorderLast) {
      StringBuilder sb;
      sb.Append("TableCollapsedBorder");
      if (type & DisplayItem::kTableCollapsedBorderTop)
        sb.Append("Top");
      if (type & DisplayItem::kTableCollapsedBorderRight)
        sb.Append("Right");
      if (type & DisplayItem::kTableCollapsedBorderBottom)
        sb.Append("Bottom");
      if (type & DisplayItem::kTableCollapsedBorderLeft)
        sb.Append("Left");
      return sb.ToString();
    }
  }
  switch (type) {
    DEBUG_STRING_CASE(BoxDecorationBackground);
    DEBUG_STRING_CASE(Caret);
    DEBUG_STRING_CASE(ColumnRules);
    DEBUG_STRING_CASE(DebugDrawing);
    DEBUG_STRING_CASE(DocumentBackground);
    DEBUG_STRING_CASE(DragImage);
    DEBUG_STRING_CASE(DragCaret);
    DEBUG_STRING_CASE(SVGImage);
    DEBUG_STRING_CASE(LinkHighlight);
    DEBUG_STRING_CASE(ImageAreaFocusRing);
    DEBUG_STRING_CASE(PageOverlay);
    DEBUG_STRING_CASE(PageWidgetDelegateBackgroundFallback);
    DEBUG_STRING_CASE(PopupContainerBorder);
    DEBUG_STRING_CASE(PopupListBoxBackground);
    DEBUG_STRING_CASE(PopupListBoxRow);
    DEBUG_STRING_CASE(PrintedContentDestinationLocations);
    DEBUG_STRING_CASE(PrintedContentPDFURLRect);
    DEBUG_STRING_CASE(Resizer);
    DEBUG_STRING_CASE(SVGClip);
    DEBUG_STRING_CASE(SVGFilter);
    DEBUG_STRING_CASE(SVGMask);
    DEBUG_STRING_CASE(ScrollbarBackButtonEnd);
    DEBUG_STRING_CASE(ScrollbarBackButtonStart);
    DEBUG_STRING_CASE(ScrollbarBackground);
    DEBUG_STRING_CASE(ScrollbarBackTrack);
    DEBUG_STRING_CASE(ScrollbarCorner);
    DEBUG_STRING_CASE(ScrollbarForwardButtonEnd);
    DEBUG_STRING_CASE(ScrollbarForwardButtonStart);
    DEBUG_STRING_CASE(ScrollbarForwardTrack);
    DEBUG_STRING_CASE(ScrollbarThumb);
    DEBUG_STRING_CASE(ScrollbarTickmarks);
    DEBUG_STRING_CASE(ScrollbarTrackBackground);
    DEBUG_STRING_CASE(ScrollbarCompositedScrollbar);
    DEBUG_STRING_CASE(SelectionTint);
    DEBUG_STRING_CASE(VideoBitmap);
    DEBUG_STRING_CASE(WebPlugin);
    DEBUG_STRING_CASE(WebFont);
    DEBUG_STRING_CASE(ReflectionMask);

    DEFAULT_CASE;
  }
}

static WTF::String DrawingTypeAsDebugString(DisplayItem::Type type) {
  PAINT_PHASE_BASED_DEBUG_STRINGS(Drawing);
  return "Drawing" + SpecialDrawingTypeAsDebugString(type);
}

static String ForeignLayerTypeAsDebugString(DisplayItem::Type type) {
  switch (type) {
    DEBUG_STRING_CASE(ForeignLayerCanvas);
    DEBUG_STRING_CASE(ForeignLayerPlugin);
    DEBUG_STRING_CASE(ForeignLayerVideo);
    DEFAULT_CASE;
  }
}

static WTF::String ClipTypeAsDebugString(DisplayItem::Type type) {
  PAINT_PHASE_BASED_DEBUG_STRINGS(ClipBox);
  PAINT_PHASE_BASED_DEBUG_STRINGS(ClipColumnBounds);
  PAINT_PHASE_BASED_DEBUG_STRINGS(ClipLayerFragment);

  switch (type) {
    DEBUG_STRING_CASE(ClipFileUploadControlRect);
    DEBUG_STRING_CASE(ClipFrameToVisibleContentRect);
    DEBUG_STRING_CASE(ClipFrameScrollbars);
    DEBUG_STRING_CASE(ClipLayerBackground);
    DEBUG_STRING_CASE(ClipLayerColumnBounds);
    DEBUG_STRING_CASE(ClipLayerFilter);
    DEBUG_STRING_CASE(ClipLayerForeground);
    DEBUG_STRING_CASE(ClipLayerParent);
    DEBUG_STRING_CASE(ClipLayerOverflowControls);
    DEBUG_STRING_CASE(ClipPopupListBoxFrame);
    DEBUG_STRING_CASE(ClipScrollbarsToBoxBounds);
    DEBUG_STRING_CASE(ClipSelectionImage);
    DEBUG_STRING_CASE(PageWidgetDelegateClip);
    DEFAULT_CASE;
  }
}

static String ScrollTypeAsDebugString(DisplayItem::Type type) {
  PAINT_PHASE_BASED_DEBUG_STRINGS(Scroll);
  switch (type) {
    DEBUG_STRING_CASE(ScrollOverflowControls);
    DEFAULT_CASE;
  }
}

static String Transform3DTypeAsDebugString(DisplayItem::Type type) {
  switch (type) {
    DEBUG_STRING_CASE(Transform3DElementTransform);
    DEFAULT_CASE;
  }
}

WTF::String DisplayItem::TypeAsDebugString(Type type) {
  if (IsDrawingType(type))
    return DrawingTypeAsDebugString(type);

  if (IsForeignLayerType(type))
    return ForeignLayerTypeAsDebugString(type);

  if (IsClipType(type))
    return ClipTypeAsDebugString(type);
  if (IsEndClipType(type))
    return "End" + ClipTypeAsDebugString(endClipTypeToClipType(type));

  PAINT_PHASE_BASED_DEBUG_STRINGS(FloatClip);
  if (IsEndFloatClipType(type))
    return "End" + TypeAsDebugString(endFloatClipTypeToFloatClipType(type));

  if (IsScrollType(type))
    return ScrollTypeAsDebugString(type);
  if (IsEndScrollType(type))
    return "End" + ScrollTypeAsDebugString(endScrollTypeToScrollType(type));

  if (IsTransform3DType(type))
    return Transform3DTypeAsDebugString(type);
  if (IsEndTransform3DType(type))
    return "End" + Transform3DTypeAsDebugString(
                       endTransform3DTypeToTransform3DType(type));

  switch (type) {
    DEBUG_STRING_CASE(BeginFilter);
    DEBUG_STRING_CASE(EndFilter);
    DEBUG_STRING_CASE(BeginCompositing);
    DEBUG_STRING_CASE(EndCompositing);
    DEBUG_STRING_CASE(BeginTransform);
    DEBUG_STRING_CASE(EndTransform);
    DEBUG_STRING_CASE(BeginClipPath);
    DEBUG_STRING_CASE(EndClipPath);
    DEBUG_STRING_CASE(UninitializedType);
    DEFAULT_CASE;
  }
}

WTF::String DisplayItem::AsDebugString() const {
  WTF::StringBuilder string_builder;
  string_builder.Append('{');
  DumpPropertiesAsDebugString(string_builder);
  string_builder.Append('}');
  return string_builder.ToString();
}

void DisplayItem::DumpPropertiesAsDebugString(
    WTF::StringBuilder& string_builder) const {
  if (!HasValidClient()) {
    string_builder.Append("validClient: false, originalDebugString: ");
    // This is the original debug string which is in json format.
    string_builder.Append(ClientDebugString());
    return;
  }

  string_builder.Append(String::Format("client: \"%p", &Client()));
  if (!ClientDebugString().IsEmpty()) {
    string_builder.Append(' ');
    string_builder.Append(ClientDebugString());
  }
  string_builder.Append("\", type: \"");
  string_builder.Append(TypeAsDebugString(GetType()));
  string_builder.Append('"');
  if (skipped_cache_)
    string_builder.Append(", skippedCache: true");
}

#endif

}  // namespace blink
