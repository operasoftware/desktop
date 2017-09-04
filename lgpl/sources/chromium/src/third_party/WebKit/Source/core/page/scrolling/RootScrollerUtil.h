// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RootScrollerUtil_h
#define RootScrollerUtil_h

namespace blink {

class Node;
class PaintLayer;
class ScrollableArea;

namespace RootScrollerUtil {

// Returns the ScrollableArea that's associated with the root scroller Node.
// For the <html> element and document Node this will be the FrameView or root
// PaintLayerScrollableArea.
ScrollableArea* ScrollableAreaForRootScroller(const Node*);

// Returns the PaintLayer that'll be used as the root scrolling layer. For the
// <html> element and document Node, this returns the LayoutView's PaintLayer
// rather than <html>'s since scrolling is handled by LayoutView.
PaintLayer* PaintLayerForRootScroller(const Node*);

}  // namespace RootScrollerUtil

}  // namespace blink

#endif  // RootScrollerUtil_h
