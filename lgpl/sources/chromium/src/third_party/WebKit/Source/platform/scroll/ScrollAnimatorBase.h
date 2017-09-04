/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollAnimatorBase_h
#define ScrollAnimatorBase_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollAnimatorCompositorCoordinator.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/Forward.h"

namespace blink {

class CompositorAnimationTimeline;
class ScrollableArea;
class Scrollbar;

class PLATFORM_EXPORT ScrollAnimatorBase
    : public ScrollAnimatorCompositorCoordinator {
 public:
  static ScrollAnimatorBase* Create(ScrollableArea*);

  virtual ~ScrollAnimatorBase();

  virtual void Dispose() {}

  // A possibly animated scroll. The base class implementation always scrolls
  // immediately, never animates. If the scroll is animated and currently the
  // animator has an in-progress animation, the ScrollResult will always return
  // no unusedDelta and didScroll=true, i.e. fully consuming the scroll request.
  // This makes animations latch to a single scroller. Note, the semantics are
  // currently somewhat different on Mac - see ScrollAnimatorMac.mm.
  virtual ScrollResult UserScroll(ScrollGranularity, const ScrollOffset& delta);

  virtual void ScrollToOffsetWithoutAnimation(const ScrollOffset&);

  void SetCurrentOffset(const ScrollOffset&);
  ScrollOffset CurrentOffset() const;
  virtual ScrollOffset DesiredTargetOffset() const { return CurrentOffset(); }

  // Returns how much of pixelDelta will be used by the underlying scrollable
  // area.
  virtual ScrollOffset ComputeDeltaToConsume(const ScrollOffset& delta) const;

  // ScrollAnimatorCompositorCoordinator implementation.
  ScrollableArea* GetScrollableArea() const override {
    return scrollable_area_;
  }
  void TickAnimation(double monotonic_time) override{};
  void CancelAnimation() override {}
  void TakeOverCompositorAnimation() override {}
  void UpdateCompositorAnimations() override{};
  void NotifyCompositorAnimationFinished(int group_id) override{};
  void NotifyCompositorAnimationAborted(int group_id) override{};
  void LayerForCompositedScrollingDidChange(
      CompositorAnimationTimeline*) override{};

  virtual void ContentAreaWillPaint() const {}
  virtual void MouseEnteredContentArea() const {}
  virtual void MouseExitedContentArea() const {}
  virtual void MouseMovedInContentArea() const {}
  virtual void MouseEnteredScrollbar(Scrollbar&) const {}
  virtual void MouseExitedScrollbar(Scrollbar&) const {}
  virtual void ContentsResized() const {}
  virtual void ContentAreaDidShow() const {}
  virtual void ContentAreaDidHide() const {}

  virtual void FinishCurrentScrollAnimations() {}

  virtual void DidAddVerticalScrollbar(Scrollbar&) {}
  virtual void WillRemoveVerticalScrollbar(Scrollbar&) {}
  virtual void DidAddHorizontalScrollbar(Scrollbar&) {}
  virtual void WillRemoveHorizontalScrollbar(Scrollbar&) {}

  virtual void NotifyContentAreaScrolled(const ScrollOffset&, ScrollType) {}

  virtual bool SetScrollbarsVisibleForTesting(bool) { return false; }

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit ScrollAnimatorBase(ScrollableArea*);

  virtual void NotifyOffsetChanged();

  Member<ScrollableArea> scrollable_area_;

  ScrollOffset current_offset_;
};

}  // namespace blink

#endif  // ScrollAnimatorBase_h
