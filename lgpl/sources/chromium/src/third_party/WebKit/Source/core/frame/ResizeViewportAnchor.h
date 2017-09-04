// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeViewportAnchor_h
#define ResizeViewportAnchor_h

#include "core/CoreExport.h"
#include "core/page/Page.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class FrameView;

// This class scrolls the viewports to compensate for bounds clamping caused by
// viewport size changes.
//
// It is needed when the layout viewport grows (causing its own scroll position
// to be clamped) and also when it shrinks (causing the visual viewport's scroll
// position to be clamped).
class CORE_EXPORT ResizeViewportAnchor final
    : public GarbageCollected<ResizeViewportAnchor> {
  WTF_MAKE_NONCOPYABLE(ResizeViewportAnchor);

 public:
  ResizeViewportAnchor(Page& page) : page_(page), scope_count_(0) {}

  class ResizeScope {
    STACK_ALLOCATED();

   public:
    explicit ResizeScope(ResizeViewportAnchor& anchor) : anchor_(anchor) {
      anchor_->BeginScope();
    }
    ~ResizeScope() { anchor_->EndScope(); }

   private:
    Member<ResizeViewportAnchor> anchor_;
  };

  void ResizeFrameView(IntSize);

  DEFINE_INLINE_TRACE() { visitor->Trace(page_); }

 private:
  void BeginScope() { scope_count_++; }
  void EndScope();
  FrameView* RootFrameView();

  // The amount of resize-induced clamping drift accumulated during the
  // ResizeScope.  Note that this should NOT include other kinds of scrolling
  // that may occur during layout, such as from ScrollAnchor.
  ScrollOffset drift_;
  Member<Page> page_;
  int scope_count_;
};

}  // namespace blink

#endif  // ResizeViewportAnchor_h
