/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/document_lifecycle.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#endif

namespace blink {

static DocumentLifecycle::DeprecatedTransition* g_deprecated_transition_stack =
    nullptr;

// TODO(skyostil): Come up with a better way to store cross-frame lifecycle
// related data to avoid this being a global setting.
static unsigned g_allow_throttling_count = 0;

DocumentLifecycle::Scope::Scope(DocumentLifecycle& lifecycle,
                                LifecycleState final_state)
    : lifecycle_(lifecycle), final_state_(final_state) {}

DocumentLifecycle::Scope::~Scope() {
  lifecycle_.AdvanceTo(final_state_);
}

DocumentLifecycle::DeprecatedTransition::DeprecatedTransition(
    LifecycleState from,
    LifecycleState to)
    : previous_(g_deprecated_transition_stack), from_(from), to_(to) {
  g_deprecated_transition_stack = this;
}

DocumentLifecycle::DeprecatedTransition::~DeprecatedTransition() {
  g_deprecated_transition_stack = previous_;
}

DocumentLifecycle::AllowThrottlingScope::AllowThrottlingScope(
    DocumentLifecycle& lifecycle) {
  g_allow_throttling_count++;
}

DocumentLifecycle::AllowThrottlingScope::~AllowThrottlingScope() {
  DCHECK_GT(g_allow_throttling_count, 0u);
  g_allow_throttling_count--;
}

DocumentLifecycle::DisallowThrottlingScope::DisallowThrottlingScope(
    DocumentLifecycle& lifecycle) {
  saved_count_ = g_allow_throttling_count;
  g_allow_throttling_count = 0;
}

DocumentLifecycle::DisallowThrottlingScope::~DisallowThrottlingScope() {
  g_allow_throttling_count = saved_count_;
}

DocumentLifecycle::DocumentLifecycle()
    : state_(kUninitialized),
      detach_count_(0),
      disallow_transition_count_(0),
      check_no_transition_(false) {}

#if DCHECK_IS_ON()

bool DocumentLifecycle::CanAdvanceTo(LifecycleState next_state) const {
  if (StateTransitionDisallowed())
    return false;

  // We can stop from anywhere.
  if (next_state == kStopping)
    return true;

  switch (state_) {
    case kUninitialized:
      return next_state == kInactive;
    case kInactive:
      if (next_state == kStyleClean)
        return true;
      break;
    case kVisualUpdatePending:
      if (next_state == kInPreLayout)
        return true;
      if (next_state == kInStyleRecalc)
        return true;
      if (next_state == kInPerformLayout)
        return true;
      break;
    case kInStyleRecalc:
      return next_state == kStyleClean;
    case kStyleClean:
      // We can synchronously recalc style.
      if (next_state == kInStyleRecalc)
        return true;
      // We can notify layout objects that subtrees changed.
      if (next_state == kInLayoutSubtreeChange)
        return true;
      // We can synchronously perform layout.
      if (next_state == kInPreLayout)
        return true;
      if (next_state == kInPerformLayout)
        return true;
      // We can redundant arrive in the style clean state.
      if (next_state == kStyleClean)
        return true;
      if (next_state == kLayoutClean)
        return true;
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInCompositingUpdate)
        return true;
      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInPrePaint)
        return true;
      break;
    case kInLayoutSubtreeChange:
      return next_state == kLayoutSubtreeChangeClean;
    case kLayoutSubtreeChangeClean:
      // We can synchronously recalc style.
      if (next_state == kInStyleRecalc)
        return true;
      // We can synchronously perform layout.
      if (next_state == kInPreLayout)
        return true;
      if (next_state == kInPerformLayout)
        return true;
      // Can move back to style clean.
      if (next_state == kStyleClean)
        return true;
      if (next_state == kLayoutClean)
        return true;
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInCompositingUpdate)
        return true;
      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInPrePaint)
        return true;
      break;
    case kInPreLayout:
      if (next_state == kInStyleRecalc)
        return true;
      if (next_state == kStyleClean)
        return true;
      if (next_state == kInPreLayout)
        return true;
      break;
    case kInPerformLayout:
      return next_state == kAfterPerformLayout;
    case kAfterPerformLayout:
      // We can synchronously recompute layout in AfterPerformLayout.
      // FIXME: Ideally, we would unnest this recursion into a loop.
      if (next_state == kInPreLayout)
        return true;
      if (next_state == kLayoutClean)
        return true;
      break;
    case kLayoutClean:
      // We can synchronously recalc style.
      if (next_state == kInStyleRecalc)
        return true;
      // We can synchronously perform layout.
      if (next_state == kInPreLayout)
        return true;
      if (next_state == kInPerformLayout)
        return true;
      // We can redundantly arrive in the layout clean state. This situation
      // can happen when we call layout recursively and we unwind the stack.
      if (next_state == kLayoutClean)
        return true;
      if (next_state == kStyleClean)
        return true;
      // InAccessibility only runs if there is an ExistingAXObjectCache.
      if (next_state == kInAccessibility)
        return true;
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInCompositingUpdate)
        return true;
      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInPrePaint)
        return true;
      break;
    case kInAccessibility:
      if (next_state == kAccessibilityClean)
        return true;
      break;
    case kAccessibilityClean:
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInCompositingUpdate)
        return true;
      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInPrePaint)
        return true;
      break;
    case kInCompositingUpdate:
      DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
      // Once we are in the compositing update, we can either just clean the
      // inputs or do the whole of compositing.
      return next_state == kCompositingInputsClean ||
             next_state == kCompositingClean;
    case kCompositingInputsClean:
      DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
      // We can return to style re-calc, layout, or the start of compositing.
      if (next_state == kInStyleRecalc)
        return true;
      if (next_state == kInPreLayout)
        return true;
      if (next_state == kInCompositingUpdate)
        return true;
      if (next_state == kInAccessibility)
        return true;
      // Otherwise, we can continue onwards.
      if (next_state == kCompositingClean)
        return true;
      break;
    case kCompositingClean:
      DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
      if (next_state == kInStyleRecalc)
        return true;
      if (next_state == kInPreLayout)
        return true;
      if (next_state == kInCompositingUpdate)
        return true;
      if (next_state == kInPrePaint)
        return true;
      if (next_state == kInAccessibility)
        return true;
      break;
    case kInPrePaint:
      if (next_state == kPrePaintClean)
        return true;
      break;
    case kPrePaintClean:
      if (next_state == kInPaint)
        return true;
      if (next_state == kInStyleRecalc)
        return true;
      if (next_state == kInPreLayout)
        return true;
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInCompositingUpdate)
        return true;
      if (next_state == kInPrePaint)
        return true;
      if (next_state == kInAccessibility)
        return true;
      break;
    case kInPaint:
      if (next_state == kPaintClean)
        return true;
      break;
    case kPaintClean:
      if (next_state == kInStyleRecalc)
        return true;
      if (next_state == kInPreLayout)
        return true;
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          next_state == kInCompositingUpdate)
        return true;
      if (next_state == kInPrePaint)
        return true;
      if (next_state == kInPaint)
        return true;
      if (next_state == kInAccessibility)
        return true;
      break;
    case kStopping:
      return next_state == kStopped;
    case kStopped:
      return false;
  }
  return false;
}

bool DocumentLifecycle::CanRewindTo(LifecycleState next_state) const {
  if (StateTransitionDisallowed())
    return false;

  // This transition is bogus, but we've allowed it anyway.
  if (g_deprecated_transition_stack &&
      state_ == g_deprecated_transition_stack->From() &&
      next_state == g_deprecated_transition_stack->To())
    return true;
  return state_ == kStyleClean || state_ == kLayoutSubtreeChangeClean ||
         state_ == kAfterPerformLayout || state_ == kLayoutClean ||
         state_ == kAccessibilityClean || state_ == kCompositingInputsClean ||
         state_ == kCompositingClean || state_ == kPrePaintClean ||
         state_ == kPaintClean;
}

#define DEBUG_STRING_CASE(StateName) \
  case DocumentLifecycle::StateName: \
    return #StateName

static WTF::String StateAsDebugString(
    const DocumentLifecycle::LifecycleState& state) {
  switch (state) {
    DEBUG_STRING_CASE(kUninitialized);
    DEBUG_STRING_CASE(kInactive);
    DEBUG_STRING_CASE(kVisualUpdatePending);
    DEBUG_STRING_CASE(kInStyleRecalc);
    DEBUG_STRING_CASE(kStyleClean);
    DEBUG_STRING_CASE(kInLayoutSubtreeChange);
    DEBUG_STRING_CASE(kLayoutSubtreeChangeClean);
    DEBUG_STRING_CASE(kInPreLayout);
    DEBUG_STRING_CASE(kInPerformLayout);
    DEBUG_STRING_CASE(kAfterPerformLayout);
    DEBUG_STRING_CASE(kLayoutClean);
    DEBUG_STRING_CASE(kInAccessibility);
    DEBUG_STRING_CASE(kAccessibilityClean);
    DEBUG_STRING_CASE(kInCompositingUpdate);
    DEBUG_STRING_CASE(kCompositingInputsClean);
    DEBUG_STRING_CASE(kCompositingClean);
    DEBUG_STRING_CASE(kInPrePaint);
    DEBUG_STRING_CASE(kPrePaintClean);
    DEBUG_STRING_CASE(kInPaint);
    DEBUG_STRING_CASE(kPaintClean);
    DEBUG_STRING_CASE(kStopping);
    DEBUG_STRING_CASE(kStopped);
  }

  NOTREACHED();
  return "Unknown";
}

WTF::String DocumentLifecycle::ToString() const {
  return StateAsDebugString(state_);
}
#endif

void DocumentLifecycle::AdvanceTo(LifecycleState next_state) {
#if DCHECK_IS_ON()
  DCHECK(CanAdvanceTo(next_state))
      << "Cannot advance document lifecycle from " << StateAsDebugString(state_)
      << " to " << StateAsDebugString(next_state) << ".";
#endif
  CHECK(state_ == next_state || !check_no_transition_);
  state_ = next_state;
}

void DocumentLifecycle::EnsureStateAtMost(LifecycleState state) {
  DCHECK(state == kVisualUpdatePending || state == kStyleClean ||
         state == kLayoutClean);
  if (state_ <= state)
    return;
#if DCHECK_IS_ON()
  DCHECK(CanRewindTo(state))
      << "Cannot rewind document lifecycle from " << StateAsDebugString(state_)
      << " to " << StateAsDebugString(state) << ".";
#endif
  CHECK(state_ == state || !check_no_transition_);
  state_ = state;
}

bool DocumentLifecycle::ThrottlingAllowed() const {
  return g_allow_throttling_count;
}

}  // namespace blink
