/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"

#include "base/optional.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_updater.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_layer_assigner.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_requirements_updater.h"
#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_builder.h"
#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_updater.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

PaintLayerCompositor::PaintLayerCompositor(LayoutView& layout_view)
    : layout_view_(layout_view) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
}

PaintLayerCompositor::~PaintLayerCompositor() = default;

void PaintLayerCompositor::CleanUp() {
  if (InCompositingMode())
    SetOwnerNeedsCompositingUpdate();
}

bool PaintLayerCompositor::InCompositingMode() const {
  // FIXME: This should assert that lifecycle is >= CompositingClean since
  // the last step of updateIfNeeded can set this bit to false.
  DCHECK(layout_view_.Layer()->IsAllowedToQueryCompositingState());
  return compositing_;
}

bool PaintLayerCompositor::StaleInCompositingMode() const {
  return compositing_;
}

void PaintLayerCompositor::SetCompositingModeEnabled(bool enable) {
  if (enable == compositing_)
    return;
  compositing_ = enable;
}

void PaintLayerCompositor::UpdateAcceleratedCompositingSettings() {
  if (auto* root_layer = RootLayer())
    root_layer->SetNeedsCompositingInputsUpdate();
}

void PaintLayerCompositor::UpdateIfNeededRecursive(
    DocumentLifecycle::LifecycleState target_state) {
  CompositingReasonsStats compositing_reasons_stats;
  UpdateIfNeededRecursiveInternal(target_state, compositing_reasons_stats);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Compositing.LayerPromotionCount.Overlap",
                              compositing_reasons_stats.overlap_layers, 1, 100,
                              5);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Compositing.LayerPromotionCount.ActiveAnimation",
      compositing_reasons_stats.active_animation_layers, 1, 100, 5);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Compositing.LayerPromotionCount.AssumedOverlap",
      compositing_reasons_stats.assumed_overlap_layers, 1, 1000, 5);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Compositing.LayerPromotionCount.IndirectComposited",
      compositing_reasons_stats.indirect_composited_layers, 1, 10000, 10);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Compositing.LayerPromotionCount.TotalComposited",
      compositing_reasons_stats.total_composited_layers, 1, 1000, 10);
}

void PaintLayerCompositor::UpdateIfNeededRecursiveInternal(
    DocumentLifecycle::LifecycleState target_state,
    CompositingReasonsStats& compositing_reasons_stats) {
  DCHECK(target_state >= DocumentLifecycle::kCompositingInputsClean);

  if (layout_view_.GetFrameView()->ShouldThrottleRendering())
    return;

  LocalFrameView* view = layout_view_.GetFrameView();
  view->ResetNeedsForcedCompositingUpdate();

  for (Frame* child =
           layout_view_.GetFrameView()->GetFrame().Tree().FirstChild();
       child; child = child->Tree().NextSibling()) {
    auto* local_frame = DynamicTo<LocalFrame>(child);
    if (!local_frame)
      continue;
    // It's possible for trusted Pepper plugins to force hit testing in
    // situations where the frame tree is in an inconsistent state, such as in
    // the middle of frame detach.
    // TODO(bbudge) Remove this check when trusted Pepper plugins are gone.
    if (local_frame->GetDocument()->IsActive() &&
        local_frame->ContentLayoutObject()) {
      local_frame->ContentLayoutObject()
          ->Compositor()
          ->UpdateIfNeededRecursiveInternal(target_state,
                                            compositing_reasons_stats);
    }
  }

  TRACE_EVENT0("blink,benchmark",
               "PaintLayerCompositor::updateIfNeededRecursive");

  DCHECK(!layout_view_.NeedsLayout());

  ScriptForbiddenScope forbid_script;

#if DCHECK_IS_ON()
  view->SetIsUpdatingDescendantDependentFlags(true);
#endif
  {
    TRACE_EVENT0("blink", "PaintLayer::UpdateDescendantDependentFlags");
    RootLayer()->UpdateDescendantDependentFlags();
  }
#if DCHECK_IS_ON()
  view->SetIsUpdatingDescendantDependentFlags(false);
#endif

  layout_view_.CommitPendingSelection();

  UpdateIfNeeded(target_state, compositing_reasons_stats);
  DCHECK(Lifecycle().GetState() == DocumentLifecycle::kCompositingInputsClean ||
         Lifecycle().GetState() == DocumentLifecycle::kCompositingClean);
  if (target_state == DocumentLifecycle::kCompositingInputsClean)
    return;

#if DCHECK_IS_ON()
  DCHECK_EQ(Lifecycle().GetState(), DocumentLifecycle::kCompositingClean);
  AssertNoUnresolvedDirtyBits();
  for (Frame* child =
           layout_view_.GetFrameView()->GetFrame().Tree().FirstChild();
       child; child = child->Tree().NextSibling()) {
    auto* local_frame = DynamicTo<LocalFrame>(child);
    if (!local_frame)
      continue;
    if (local_frame->ShouldThrottleRendering() ||
        !local_frame->ContentLayoutObject())
      continue;
    local_frame->ContentLayoutObject()
        ->Compositor()
        ->AssertNoUnresolvedDirtyBits();
  }
#endif
}

#if DCHECK_IS_ON()
void PaintLayerCompositor::AssertNoUnresolvedDirtyBits() {
  DCHECK_EQ(pending_update_type_, kCompositingUpdateNone);
}
#endif

void PaintLayerCompositor::SetNeedsCompositingUpdate(
    CompositingUpdateType update_type) {
  DCHECK_NE(update_type, kCompositingUpdateNone);
  pending_update_type_ = std::max(pending_update_type_, update_type);
  if (Page* page = GetPage())
    page->Animator().ScheduleVisualUpdate(layout_view_.GetFrame());

  if (layout_view_.DocumentBeingDestroyed())
    return;

  Lifecycle().EnsureStateAtMost(DocumentLifecycle::kLayoutClean);
}

void PaintLayerCompositor::UpdateWithoutAcceleratedCompositing(
    CompositingUpdateType update_type) {
  DCHECK(!layout_view_.GetDocument()
              .GetSettings()
              ->GetAcceleratedCompositingEnabled());

  if (update_type >= kCompositingUpdateAfterCompositingInputChange) {
    CompositingInputsUpdater(RootLayer(), GetCompositingInputsRoot()).Update();
  }

#if DCHECK_IS_ON()
  CompositingInputsUpdater::AssertNeedsCompositingInputsUpdateBitsCleared(
      RootLayer());
#endif
}

void PaintLayerCompositor::
    ForceRecomputeVisualRectsIncludingNonCompositingDescendants(
        LayoutObject& layout_object) {
  // We clear the previous visual rect as it's wrong (paint invalidation
  // container changed, ...). Forcing a full invalidation will make us recompute
  // it. Also we are not changing the previous position from our paint
  // invalidation container, which is fine as we want a full paint invalidation
  // anyway.
  layout_object.ClearPreviousVisualRects();

  for (LayoutObject* child = layout_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsPaintInvalidationContainer())
      ForceRecomputeVisualRectsIncludingNonCompositingDescendants(*child);
  }
}

#if DCHECK_IS_ON()
static void AssertWholeTreeNotComposited(const PaintLayer& paint_layer) {
  DCHECK(paint_layer.GetCompositingState() == kNotComposited);
  for (PaintLayer* child = paint_layer.FirstChild(); child;
       child = child->NextSibling()) {
    AssertWholeTreeNotComposited(*child);
  }
}
#endif

void PaintLayerCompositor::UpdateIfNeeded(
    DocumentLifecycle::LifecycleState target_state,
    CompositingReasonsStats& compositing_reasons_stats) {
  DCHECK(target_state >= DocumentLifecycle::kCompositingInputsClean);

  Lifecycle().AdvanceTo(DocumentLifecycle::kInCompositingUpdate);

  if (pending_update_type_ < kCompositingUpdateAfterCompositingInputChange &&
      target_state == DocumentLifecycle::kCompositingInputsClean) {
    // The compositing inputs are already clean and that is our target state.
    // Early-exit here without clearing the pending update type since we haven't
    // handled e.g. geometry updates.
    Lifecycle().AdvanceTo(DocumentLifecycle::kCompositingInputsClean);
    return;
  }

  CompositingUpdateType update_type = pending_update_type_;
  pending_update_type_ = kCompositingUpdateNone;

  if (!layout_view_.GetDocument()
           .GetSettings()
           ->GetAcceleratedCompositingEnabled()) {
    UpdateWithoutAcceleratedCompositing(update_type);
    Lifecycle().AdvanceTo(
        std::min(DocumentLifecycle::kCompositingClean, target_state));
    return;
  }

  if (update_type == kCompositingUpdateNone) {
    Lifecycle().AdvanceTo(
        std::min(DocumentLifecycle::kCompositingClean, target_state));
    return;
  }

  PaintLayer* update_root = RootLayer();

  Vector<PaintLayer*> layers_needing_paint_invalidation;

  if (update_type >= kCompositingUpdateAfterCompositingInputChange) {
    CompositingInputsUpdater(RootLayer(), GetCompositingInputsRoot()).Update();

#if DCHECK_IS_ON()
    // FIXME: Move this check to the end of the compositing update.
    CompositingInputsUpdater::AssertNeedsCompositingInputsUpdateBitsCleared(
        update_root);
#endif

    // In the case where we only want to make compositing inputs clean, we
    // early-exit here. Because we have not handled the other implications of
    // |pending_update_type_| > kCompositingUpdateNone, we must restore the
    // pending update type for a future call.
    if (target_state == DocumentLifecycle::kCompositingInputsClean) {
      pending_update_type_ = update_type;
      Lifecycle().AdvanceTo(DocumentLifecycle::kCompositingInputsClean);
      return;
    }

    CompositingRequirementsUpdater(layout_view_)
        .Update(update_root, compositing_reasons_stats);

    CompositingLayerAssigner layer_assigner(this);
    layer_assigner.Assign(update_root, layers_needing_paint_invalidation);

    if (layer_assigner.LayersChanged()) {
      update_type = std::max(update_type, kCompositingUpdateRebuildTree);
      if (ScrollingCoordinator* scrolling_coordinator =
              GetScrollingCoordinator()) {
        LocalFrameView* frame_view = layout_view_.GetFrameView();
        scrolling_coordinator->NotifyGeometryChanged(frame_view);
      }
    }
  }

  GraphicsLayer* current_parent = nullptr;
  // Save off our current parent. We need this in subframes, because our
  // parent attached us to itself via AttachFrameContentLayersToIframeLayer().
  if (!IsMainFrame() && update_root->GetCompositedLayerMapping()) {
    current_parent =
        update_root->GetCompositedLayerMapping()->MainGraphicsLayer()->Parent();
  }

#if DCHECK_IS_ON()
  if (update_root->GetCompositingState() != kPaintsIntoOwnBacking) {
    AssertWholeTreeNotComposited(*update_root);
  }
#endif

  GraphicsLayerUpdater updater;
  updater.Update(*update_root, layers_needing_paint_invalidation);

  if (updater.NeedsRebuildTree())
    update_type = std::max(update_type, kCompositingUpdateRebuildTree);

#if DCHECK_IS_ON()
  // FIXME: Move this check to the end of the compositing update.
  GraphicsLayerUpdater::AssertNeedsToUpdateGraphicsLayerBitsCleared(
      *update_root);
#endif

  if (update_type >= kCompositingUpdateRebuildTree) {
    GraphicsLayerVector child_list;
    {
      TRACE_EVENT0("blink", "GraphicsLayerTreeBuilder::rebuild");
      GraphicsLayerTreeBuilder().Rebuild(*update_root, child_list);
    }

    if (!child_list.IsEmpty()) {
      CHECK(compositing_);
      DCHECK_EQ(1u, child_list.size());
      // Schedule an update in the parent frame so the <iframe>'s layer in the
      // owner document matches the compositing state here.
      SetOwnerNeedsCompositingUpdate();
      root_layer_attachment_dirty_ = true;
    }
  }

  for (unsigned i = 0; i < layers_needing_paint_invalidation.size(); i++) {
    ForceRecomputeVisualRectsIncludingNonCompositingDescendants(
        layers_needing_paint_invalidation[i]->GetLayoutObject());
  }

  Lifecycle().AdvanceTo(DocumentLifecycle::kCompositingClean);
}

static void RestartAnimationOnCompositor(const LayoutObject& layout_object) {
  ElementAnimations* element_animations = nullptr;
  if (auto* element = DynamicTo<Element>(layout_object.GetNode()))
    element_animations = element->GetElementAnimations();
  if (element_animations)
    element_animations->RestartAnimationOnCompositor();
}

bool PaintLayerCompositor::AllocateOrClearCompositedLayerMapping(
    PaintLayer* layer,
    const CompositingStateTransitionType composited_layer_update) {
  bool composited_layer_mapping_changed = false;

  // FIXME: It would be nice to directly use the layer's compositing reason,
  // but allocateOrClearCompositedLayerMapping also gets called without having
  // updated compositing requirements fully.
  switch (composited_layer_update) {
    case kAllocateOwnCompositedLayerMapping:
      DCHECK(!layer->HasCompositedLayerMapping());
      SetCompositingModeEnabled(true);

      // If we need to issue paint invalidations, do so before allocating the
      // compositedLayerMapping and clearing out the groupedMapping.
      PaintInvalidationOnCompositingChange(layer);

      // If this layer was previously squashed, we need to remove its reference
      // to a groupedMapping right away, so that computing paint invalidation
      // rects will know the layer's correct compositingState.
      // FIXME: do we need to also remove the layer from it's location in the
      // squashing list of its groupedMapping?  Need to create a test where a
      // squashed layer pops into compositing. And also to cover all other sorts
      // of compositingState transitions.
      layer->SetLostGroupedMapping(false);
      layer->SetGroupedMapping(
          nullptr, PaintLayer::kInvalidateLayerAndRemoveFromMapping);

      layer->EnsureCompositedLayerMapping();
      composited_layer_mapping_changed = true;

      RestartAnimationOnCompositor(layer->GetLayoutObject());

      // At this time, the ScrollingCoordinator only supports the top-level
      // frame.
      if (layer->IsRootLayer() && layout_view_.GetFrame()->IsLocalRoot()) {
        if (ScrollingCoordinator* scrolling_coordinator =
                GetScrollingCoordinator()) {
          scrolling_coordinator->FrameViewRootLayerDidChange(
              layout_view_.GetFrameView());
        }
      }
      break;
    case kRemoveOwnCompositedLayerMapping:
    // PutInSquashingLayer means you might have to remove the composited layer
    // mapping first.
    case kPutInSquashingLayer:
      if (layer->HasCompositedLayerMapping()) {
        layer->ClearCompositedLayerMapping();
        composited_layer_mapping_changed = true;
      }

      break;
    case kRemoveFromSquashingLayer:
    case kNoCompositingStateChange:
      // Do nothing.
      break;
  }

  if (!composited_layer_mapping_changed)
    return false;

  layer->ClearClipRects(kPaintingClipRects);

  // Compositing state affects whether to create paint offset translation of
  // this layer, and amount of paint offset translation of descendants.
  layer->GetLayoutObject().SetNeedsPaintPropertyUpdate();

  return true;
}

void PaintLayerCompositor::PaintInvalidationOnCompositingChange(
    PaintLayer* layer) {
  // If the layoutObject is not attached yet, no need to issue paint
  // invalidations.
  if (&layer->GetLayoutObject() != &layout_view_ &&
      !layer->GetLayoutObject().Parent())
    return;

  // For querying Layer::compositingState()
  // Eager invalidation here is correct, since we are invalidating with respect
  // to the previous frame's compositing state when changing the compositing
  // backing of the layer.
  DisableCompositingQueryAsserts disabler;
  ObjectPaintInvalidator(layer->GetLayoutObject())
      .InvalidatePaintIncludingNonCompositingDescendants();
}

PaintLayerCompositor* PaintLayerCompositor::FrameContentsCompositor(
    const LayoutEmbeddedContent& layout_object) {
  const auto* element =
      DynamicTo<HTMLFrameOwnerElement>(layout_object.GetNode());
  if (!element)
    return nullptr;

  if (Document* content_document = element->contentDocument()) {
    if (auto* view = content_document->GetLayoutView())
      return view->Compositor();
  }
  return nullptr;
}

static void FullyInvalidatePaintRecursive(PaintLayer* layer) {
  if (layer->GetCompositingState() == kPaintsIntoOwnBacking)
    layer->GetCompositedLayerMapping()->SetAllLayersNeedDisplay();

  for (PaintLayer* child = layer->FirstChild(); child;
       child = child->NextSibling())
    FullyInvalidatePaintRecursive(child);
}

void PaintLayerCompositor::FullyInvalidatePaint() {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  // We're walking all compositing layers and invalidating them, so there's
  // no need to have up-to-date compositing state.
  DisableCompositingQueryAsserts disabler;
  FullyInvalidatePaintRecursive(RootLayer());
}

PaintLayer* PaintLayerCompositor::RootLayer() const {
  return layout_view_.Layer();
}

GraphicsLayer* PaintLayerCompositor::RootGraphicsLayer() const {
  if (CompositedLayerMapping* clm = RootLayer()->GetCompositedLayerMapping())
    return clm->MainGraphicsLayer();
  return nullptr;
}

GraphicsLayer* PaintLayerCompositor::PaintRootGraphicsLayer() const {
  if (layout_view_.GetDocument().GetPage()->GetChromeClient().IsPopup() ||
      !IsMainFrame())
    return RootGraphicsLayer();

  // Start from the full screen overlay layer if exists. Other layers will be
  // skipped during painting.
  if (PaintLayer* layer =
          layout_view_.GetFrameView()->GetFullScreenOverlayLayer()) {
    if (layer->HasCompositedLayerMapping())
      return layer->GetCompositedLayerMapping()->MainGraphicsLayer();
  }

  return RootGraphicsLayer();
}

void PaintLayerCompositor::UpdatePotentialCompositingReasonsFromStyle(
    PaintLayer& layer) {
  auto reasons = CompositingReasonFinder::PotentialCompositingReasonsFromStyle(
      layer.GetLayoutObject());
  layer.SetPotentialCompositingReasonsFromStyle(reasons);
}

bool PaintLayerCompositor::CanBeComposited(const PaintLayer* layer) const {
  LocalFrameView* frame_view = layer->GetLayoutObject().GetFrameView();
  // Elements within an invisible frame must not be composited because they are
  // not drawn.
  if (frame_view && !frame_view->IsVisible())
    return false;

  const bool has_compositor_animation =
      CompositingReasonFinder::CompositingReasonsForAnimation(
          layer->GetLayoutObject()) != CompositingReason::kNone;
  return layout_view_.GetDocument()
             .GetSettings()
             ->GetAcceleratedCompositingEnabled() &&
         (has_compositor_animation || !layer->SubtreeIsInvisible()) &&
         layer->IsSelfPaintingLayer() &&
         !layer->GetLayoutObject().IsLayoutFlowThread() &&
         // Don't composite <foreignObject> for the moment, to reduce
         // instances of the "fundamental compositing bug" breaking content.
         !layer->GetLayoutObject().IsSVGForeignObject();
}

// If an element has composited negative z-index children, those children paint
// in front of the layer background, so we need an extra 'contents' layer for
// the foreground of the layer object.
bool PaintLayerCompositor::NeedsContentsCompositingLayer(
    const PaintLayer* layer) const {
  if (!layer->HasCompositingDescendant())
    return false;
  return layer->IsStackingContextWithNegativeZOrderChildren();
}

static void UpdateTrackingRasterInvalidationsRecursive(
    GraphicsLayer* graphics_layer) {
  if (!graphics_layer)
    return;

  graphics_layer->UpdateTrackingRasterInvalidations();

  for (wtf_size_t i = 0; i < graphics_layer->Children().size(); ++i)
    UpdateTrackingRasterInvalidationsRecursive(graphics_layer->Children()[i]);
}

void PaintLayerCompositor::UpdateTrackingRasterInvalidations() {
#if DCHECK_IS_ON()
  DCHECK(Lifecycle().GetState() == DocumentLifecycle::kPaintClean ||
         layout_view_.GetFrameView()->ShouldThrottleRendering());
#endif

  if (GraphicsLayer* root_layer = PaintRootGraphicsLayer())
    UpdateTrackingRasterInvalidationsRecursive(root_layer);
}

void PaintLayerCompositor::SetOwnerNeedsCompositingUpdate() {
  if (HTMLFrameOwnerElement* owner_element =
          layout_view_.GetDocument().LocalOwner()) {
    owner_element->SetNeedsCompositingUpdate();
  }
}

ScrollingCoordinator* PaintLayerCompositor::GetScrollingCoordinator() const {
  if (Page* page = GetPage())
    return page->GetScrollingCoordinator();

  return nullptr;
}

Page* PaintLayerCompositor::GetPage() const {
  return layout_view_.GetFrameView()->GetFrame().GetPage();
}

DocumentLifecycle& PaintLayerCompositor::Lifecycle() const {
  return layout_view_.GetDocument().Lifecycle();
}

bool PaintLayerCompositor::IsMainFrame() const {
  return layout_view_.GetFrame()->IsMainFrame();
}

}  // namespace blink
