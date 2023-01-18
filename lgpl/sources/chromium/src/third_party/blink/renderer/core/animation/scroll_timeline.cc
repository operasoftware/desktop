// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include <tuple>

#include "base/memory/values_equivalent.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_base.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

using ScrollTimelineSet =
    HeapHashMap<WeakMember<Node>,
                Member<HeapHashSet<WeakMember<ScrollTimeline>>>>;
ScrollTimelineSet& GetScrollTimelineSet() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollTimelineSet>, set,
                      (MakeGarbageCollected<ScrollTimelineSet>()));
  return *set;
}

ScrollOrientation ToPhysicalScrollOrientation(
    ScrollTimeline::ScrollDirection direction,
    const LayoutBox& source_box) {
  bool is_horizontal = source_box.IsHorizontalWritingMode();
  switch (direction) {
    case ScrollTimeline::ScrollDirection::kBlock:
      return is_horizontal ? kVerticalScroll : kHorizontalScroll;
    case ScrollTimeline::ScrollDirection::kInline:
      return is_horizontal ? kHorizontalScroll : kVerticalScroll;
    case ScrollTimeline::ScrollDirection::kHorizontal:
      return kHorizontalScroll;
    case ScrollTimeline::ScrollDirection::kVertical:
      return kVerticalScroll;
  }
}

Node* ResolveSource(Element* source) {
  if (source && source == source->GetDocument().ScrollingElementNoLayout()) {
    return &source->GetDocument();
  }
  return source;
}

}  // namespace

ScrollTimeline* ScrollTimeline::Create(Document& document,
                                       ScrollTimelineOptions* options,
                                       ExceptionState& exception_state) {
  absl::optional<Element*> source = options->hasSource()
                                        ? absl::make_optional(options->source())
                                        : absl::nullopt;

  // TODO(crbug.com/1060384): Update to axis in alignment with the spec rewrite.
  ScrollDirection orientation;
  if (!StringToScrollDirection(options->orientation(), orientation)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid orientation");
    return nullptr;
  }

  // The scrollingElement depends on style/layout-tree in quirks mode. Update
  // such that subsequent calls to ScrollingElementNoLayout returns up-to-date
  // information.
  if (document.InQuirksMode())
    document.UpdateStyleAndLayoutTree();

  return Create(&document, source.value_or(document.ScrollingElementNoLayout()),
                orientation);
}

ScrollTimeline* ScrollTimeline::Create(Document* document,
                                       Element* source,
                                       ScrollDirection orientation) {
  ScrollTimeline* scroll_timeline = MakeGarbageCollected<ScrollTimeline>(
      document, ReferenceType::kSource, source, orientation);
  scroll_timeline->SnapshotState();

  return scroll_timeline;
}

bool ScrollTimeline::StringToScrollDirection(
    String scroll_direction,
    ScrollTimeline::ScrollDirection& result) {
  if (scroll_direction == "block") {
    result = ScrollDirection::kBlock;
    return true;
  }
  if (scroll_direction == "inline") {
    result = ScrollDirection::kInline;
    return true;
  }
  if (scroll_direction == "horizontal") {
    result = ScrollDirection::kHorizontal;
    return true;
  }
  if (scroll_direction == "vertical") {
    result = ScrollDirection::kVertical;
    return true;
  }
  return false;
}

ScrollTimeline::ScrollTimeline(Document* document,
                               ReferenceType reference_type,
                               Element* reference,
                               ScrollDirection orientation)
    : AnimationTimeline(document),
      reference_type_(reference_type),
      reference_element_(reference),
      orientation_(orientation) {
  UpdateResolvedSource();
}

bool ScrollTimeline::IsActive() const {
  return timeline_state_snapshotted_.phase != TimelinePhase::kInactive;
}

void ScrollTimeline::Invalidate() {
  ScheduleNextServiceInternal(/* time_check = */ false);
}

bool ScrollTimeline::ComputeIsActive() const {
  LayoutBox* layout_box =
      resolved_source_ ? resolved_source_->GetLayoutBox() : nullptr;
  return layout_box && layout_box->IsScrollContainer();
}

absl::optional<ScrollOffsets> ScrollTimeline::GetResolvedScrollOffsets() const {
  return timeline_state_snapshotted_.scroll_offsets;
}

// TODO(crbug.com/1336260): Since phase can only be kActive or kInactive and
// currentTime  can only be null if phase is inactive or before the first
// snapshot we can probably drop phase.
AnimationTimeline::PhaseAndTime ScrollTimeline::CurrentPhaseAndTime() {
  return {timeline_state_snapshotted_.phase,
          timeline_state_snapshotted_.current_time};
}

V8CSSNumberish* ScrollTimeline::ConvertTimeToProgress(
    AnimationTimeDelta time) const {
  return MakeGarbageCollected<V8CSSNumberish>(
      CSSUnitValues::percent((time / GetDuration().value()) * 100));
}

V8CSSNumberish* ScrollTimeline::currentTime() {
  // Compute time as a percentage based on the relative scroll position, where
  // the start offset corresponds to 0% and the end to 100%.
  auto current_time = timeline_state_snapshotted_.current_time;

  if (current_time) {
    return ConvertTimeToProgress(AnimationTimeDelta(current_time.value()));
  }
  return nullptr;
}

V8CSSNumberish* ScrollTimeline::duration() {
  return MakeGarbageCollected<V8CSSNumberish>(CSSUnitValues::percent(100));
}

// TODO(crbug.com/1060384): This section is missing from the spec rewrite.
// Resolved to remove the before and after phases in
// https://github.com/w3c/csswg-drafts/issues/7240.
// https://drafts.csswg.org/scroll-animations-1/#current-time-algorithm
ScrollTimeline::TimelineState ScrollTimeline::ComputeTimelineState() {
  UpdateResolvedSource();

  // 1. If scroll timeline is inactive, return an unresolved time value.
  // https://github.com/WICG/scroll-animations/issues/31
  // https://wicg.github.io/scroll-animations/#current-time-algorithm
  if (!ComputeIsActive()) {
    return {TimelinePhase::kInactive, /*current_time*/ absl::nullopt,
            /* scroll_offsets */ absl::nullopt};
  }
  DCHECK(resolved_source_);
  LayoutBox* layout_box = resolved_source_->GetLayoutBox();

  // Layout box and scrollable area must exist since the timeline is active.
  DCHECK(layout_box);
  DCHECK(layout_box->GetScrollableArea());

  // Depending on the writing-mode and direction, the scroll origin shifts and
  // the scroll offset may be negative. The easiest way to deal with this is to
  // use only the magnitude of the scroll offset, and compare it to (max_offset
  // - min_offset).
  PaintLayerScrollableArea* scrollable_area = layout_box->GetScrollableArea();

  // Using the absolute value of the scroll offset only makes sense if either
  // the max or min scroll offset for a given axis is 0. This should be
  // guaranteed by the scroll origin code, but these DCHECKs ensure that.
  DCHECK(scrollable_area->MaximumScrollOffset().y() == 0 ||
         scrollable_area->MinimumScrollOffset().y() == 0);
  DCHECK(scrollable_area->MaximumScrollOffset().x() == 0 ||
         scrollable_area->MinimumScrollOffset().x() == 0);

  ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
  auto physical_orientation =
      ToPhysicalScrollOrientation(orientation_, *layout_box);
  double current_offset = (physical_orientation == kHorizontalScroll)
                              ? scroll_offset.x()
                              : scroll_offset.y();
  // When using a rtl direction, current_offset grows correctly from 0 to
  // max_offset, but is negative. Since our offsets are all just deltas along
  // the orientation direction, we can just take the absolute current_offset and
  // use that everywhere.
  current_offset = std::abs(current_offset);

  absl::optional<ScrollOffsets> scroll_offsets =
      CalculateOffsets(scrollable_area, physical_orientation);
  DCHECK(scroll_offsets);

  // TODO(crbug.com/1338167): Update once
  // github.com/w3c/csswg-drafts/issues/7401 is resolved.
  double progress = (scroll_offsets->start == scroll_offsets->end)
                        ? 1
                        : (current_offset - scroll_offsets->start) /
                              (scroll_offsets->end - scroll_offsets->start);

  base::TimeDelta duration = base::Seconds(GetDuration()->InSecondsF());
  absl::optional<base::TimeDelta> calculated_current_time =
      base::Milliseconds(progress * duration.InMillisecondsF());

  return {TimelinePhase::kActive, calculated_current_time, scroll_offsets};
}

absl::optional<ScrollOffsets> ScrollTimeline::CalculateOffsets(
    PaintLayerScrollableArea* scrollable_area,
    ScrollOrientation physical_orientation) const {
  ScrollOffset scroll_dimensions = scrollable_area->MaximumScrollOffset() -
                                   scrollable_area->MinimumScrollOffset();
  double end_offset = physical_orientation == kHorizontalScroll
                          ? scroll_dimensions.x()
                          : scroll_dimensions.y();
  return absl::make_optional<ScrollOffsets>(0, end_offset);
}

// Scroll-linked animations are initialized with the start time of zero.
absl::optional<base::TimeDelta>
ScrollTimeline::InitialStartTimeForAnimations() {
  return base::TimeDelta();
}

AnimationTimeDelta ScrollTimeline::CalculateIntrinsicIterationDuration(
    const Timing& timing) {
  absl::optional<AnimationTimeDelta> duration = GetDuration();

  // Only run calculation for progress based scroll timelines
  if (duration) {
    // if iteration_duration == "auto" and iterations > 0
    if (!timing.iteration_duration && timing.iteration_count > 0) {
      // duration represents 100% so we divide it by iteration count to
      // calculate the iteration duration. TODO: (crbug.com/1216527) Once
      // delays can be percentages we will include them in the calculation:
      // ((100% - start_delay% - end_delay%) / iterations) * duration
      return duration.value() / timing.iteration_count;
    }
  }
  return AnimationTimeDelta();
}

void ScrollTimeline::ServiceAnimations(TimingUpdateReason reason) {
  // Snapshot timeline state once at top of animation frame.
  if (reason == kTimingUpdateForAnimationFrame)
    SnapshotState();

  // When scroll timeline goes from inactive to active the animations may need
  // to be started and possibly composited.
  bool was_active =
      last_current_phase_and_time_ &&
      last_current_phase_and_time_.value().phase == TimelinePhase::kActive;
  if (!was_active && IsActive())
    MarkAnimationsCompositorPending();

  AnimationTimeline::ServiceAnimations(reason);
}

void ScrollTimeline::ScheduleNextServiceInternal(bool time_check) {
  if (AnimationsNeedingUpdateCount() == 0)
    return;

  if (time_check) {
    auto state = ComputeTimelineState();
    PhaseAndTime current_phase_and_time{state.phase, state.current_time};
    if (current_phase_and_time == last_current_phase_and_time_)
      return;
  }
  ScheduleServiceOnNextFrame();
}

void ScrollTimeline::ScheduleNextService() {
  ScheduleNextServiceInternal(/* time_check = */ true);
}

void ScrollTimeline::SnapshotState() {
  timeline_state_snapshotted_ = ComputeTimelineState();
}

Element* ScrollTimeline::source() const {
  if (reference_type_ == ReferenceType::kNearestAncestor)
    GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  return SourceInternal();
}

Element* ScrollTimeline::SourceInternal() const {
  if (reference_type_ == ReferenceType::kSource)
    return reference_element_.Get();

  // ReferenceType::kNearestAncestor
  if (!reference_element_)
    return nullptr;

  LayoutBox* layout_box = reference_element_->GetLayoutBox();
  if (!layout_box)
    return nullptr;

  const LayoutBox* scroll_container = layout_box->ContainingScrollContainer();
  if (!scroll_container)
    return scroll_container->GetDocument().ScrollingElementNoLayout();

  Node* node = scroll_container->GetNode();
  if (node->IsElementNode())
    return DynamicTo<Element>(node);
  if (node->IsDocumentNode())
    return DynamicTo<Document>(node)->ScrollingElementNoLayout();

  NOTREACHED();
  return nullptr;
}

String ScrollTimeline::orientation() {
  switch (orientation_) {
    case ScrollDirection::kBlock:
      return "block";
    case ScrollDirection::kInline:
      return "inline";
    case ScrollDirection::kHorizontal:
      return "horizontal";
    case ScrollDirection::kVertical:
      return "vertical";
    default:
      NOTREACHED();
      return "";
  }
}

void ScrollTimeline::GetCurrentAndMaxOffset(const LayoutBox* layout_box,
                                            double& current_offset,
                                            double& max_offset) const {
  DCHECK(layout_box);
  DCHECK(layout_box->GetScrollableArea());

  // Depending on the writing-mode and direction, the scroll origin shifts and
  // the scroll offset may be negative. The easiest way to deal with this is to
  // use only the magnitude of the scroll offset, and compare it to (max_offset
  // - min_offset).
  PaintLayerScrollableArea* scrollable_area = layout_box->GetScrollableArea();

  // Using the absolute value of the scroll offset only makes sense if either
  // the max or min scroll offset for a given axis is 0. This should be
  // guaranteed by the scroll origin code, but these DCHECKs ensure that.
  DCHECK(scrollable_area->MaximumScrollOffset().y() == 0 ||
         scrollable_area->MinimumScrollOffset().y() == 0);
  DCHECK(scrollable_area->MaximumScrollOffset().x() == 0 ||
         scrollable_area->MinimumScrollOffset().x() == 0);
  ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
  ScrollOffset scroll_dimensions = scrollable_area->MaximumScrollOffset() -
                                   scrollable_area->MinimumScrollOffset();

  auto physical_orientation =
      ToPhysicalScrollOrientation(orientation_, *layout_box);

  if (physical_orientation == kHorizontalScroll) {
    current_offset = scroll_offset.x();
    max_offset = scroll_dimensions.x();
  } else {
    current_offset = scroll_offset.y();
    max_offset = scroll_dimensions.y();
  }
  // When using a rtl direction, current_offset grows correctly from 0 to
  // max_offset, but is negative. Since our offsets are all just deltas along
  // the orientation direction, we can just take the absolute current_offset and
  // use that everywhere.
  current_offset = std::abs(current_offset);
}

void ScrollTimeline::AnimationAttached(Animation* animation) {
  if (resolved_source_ && !HasAnimations())
    resolved_source_->RegisterScrollTimeline(this);

  AnimationTimeline::AnimationAttached(animation);
}

void ScrollTimeline::AnimationDetached(Animation* animation) {
  AnimationTimeline::AnimationDetached(animation);

  if (resolved_source_ && !HasAnimations())
    resolved_source_->UnregisterScrollTimeline(this);
}

void ScrollTimeline::WorkletAnimationAttached(WorkletAnimationBase* worklet) {
  if (!resolved_source_)
    return;
  attached_worklet_animations_.insert(worklet);
}

void ScrollTimeline::UpdateResolvedSource() {
  if (reference_type_ == ReferenceType::kSource && resolved_source_)
    return;

  Node* old_resolved_source = resolved_source_.Get();
  resolved_source_ = ResolveSource(SourceInternal());
  if (old_resolved_source == resolved_source_.Get())
    return;

  ScrollTimelineSet& set = GetScrollTimelineSet();

  if (old_resolved_source) {
    if (HasAnimations())
      old_resolved_source->UnregisterScrollTimeline(this);

    auto it = set.find(old_resolved_source);
    DCHECK(it != set.end());
    it->value->erase(this);
  }

  if (resolved_source_) {
    if (HasAnimations())
      resolved_source_->RegisterScrollTimeline(this);

    auto add_result = set.insert(
        resolved_source_,
        MakeGarbageCollected<HeapHashSet<WeakMember<ScrollTimeline>>>());
    add_result.stored_value->value->insert(this);
  }
}

void ScrollTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(reference_element_);
  visitor->Trace(resolved_source_);
  visitor->Trace(attached_worklet_animations_);
  AnimationTimeline::Trace(visitor);
}

void ScrollTimeline::Invalidate(Node* node) {
  ScrollTimelineSet& set = GetScrollTimelineSet();
  auto it = set.find(node);

  if (it == set.end())
    return;

  for (auto& timeline : *it->value) {
    timeline->Invalidate();
  }
}

void ScrollTimeline::InvalidateEffectTargetStyle() {
  for (Animation* animation : GetAnimations())
    animation->InvalidateEffectTargetStyle();
}

bool ScrollTimeline::ValidateState() {
  auto state = ComputeTimelineState();
  if (timeline_state_snapshotted_ == state)
    return true;
  timeline_state_snapshotted_ = state;
  InvalidateEffectTargetStyle();
  return false;
}

cc::AnimationTimeline* ScrollTimeline::EnsureCompositorTimeline() {
  if (compositor_timeline_)
    return compositor_timeline_.get();

  compositor_timeline_ = scroll_timeline_util::ToCompositorScrollTimeline(this);
  return compositor_timeline_.get();
}

void ScrollTimeline::UpdateCompositorTimeline() {
  if (!compositor_timeline_)
    return;

  ToScrollTimeline(compositor_timeline_.get())
      ->UpdateScrollerIdAndScrollOffsets(
          scroll_timeline_util::GetCompositorScrollElementId(resolved_source_),
          GetResolvedScrollOffsets());
}

}  // namespace blink
