// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"

#include <algorithm>
#include <limits>

#include "base/macros.h"
#include "base/numerics/clamped_math.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_delegate.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_init.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_delegate.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

namespace {

// Internal implementation of IntersectionObserverDelegate when using
// IntersectionObserver with an EventCallback.
class IntersectionObserverDelegateImpl final
    : public IntersectionObserverDelegate {

 public:
  IntersectionObserverDelegateImpl(
      ExecutionContext* context,
      IntersectionObserver::EventCallback callback,
      IntersectionObserver::DeliveryBehavior delivery_behavior)
      : context_(context),
        callback_(std::move(callback)),
        delivery_behavior_(delivery_behavior) {}

  IntersectionObserver::DeliveryBehavior GetDeliveryBehavior() const override {
    return delivery_behavior_;
  }

  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>& entries,
               IntersectionObserver& observer) override {
    callback_.Run(entries);
  }

  ExecutionContext* GetExecutionContext() const override { return context_; }

  void Trace(Visitor* visitor) const override {
    IntersectionObserverDelegate::Trace(visitor);
    visitor->Trace(context_);
  }

 private:
  WeakMember<ExecutionContext> context_;
  IntersectionObserver::EventCallback callback_;
  IntersectionObserver::DeliveryBehavior delivery_behavior_;
  DISALLOW_COPY_AND_ASSIGN(IntersectionObserverDelegateImpl);
};

void ParseMargin(String margin_parameter,
                 Vector<Length>& margin,
                 ExceptionState& exception_state) {
  // TODO(szager): Make sure this exact syntax and behavior is spec-ed
  // somewhere.

  // The root margin argument accepts syntax similar to that for CSS margin:
  //
  // "1px" = top/right/bottom/left
  // "1px 2px" = top/bottom left/right
  // "1px 2px 3px" = top left/right bottom
  // "1px 2px 3px 4px" = top left right bottom
  CSSTokenizer tokenizer(margin_parameter);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange token_range(tokens);
  while (token_range.Peek().GetType() != kEOFToken &&
         !exception_state.HadException()) {
    if (margin.size() == 4) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "Extra text found at the end of rootMargin.");
      break;
    }
    const CSSParserToken& token = token_range.ConsumeIncludingWhitespace();
    switch (token.GetType()) {
      case kPercentageToken:
        margin.push_back(Length::Percent(token.NumericValue()));
        break;
      case kDimensionToken:
        switch (token.GetUnitType()) {
          case CSSPrimitiveValue::UnitType::kPixels:
            margin.push_back(
                Length::Fixed(static_cast<int>(floor(token.NumericValue()))));
            break;
          case CSSPrimitiveValue::UnitType::kPercentage:
            margin.push_back(Length::Percent(token.NumericValue()));
            break;
          default:
            exception_state.ThrowDOMException(
                DOMExceptionCode::kSyntaxError,
                "rootMargin must be specified in pixels or percent.");
        }
        break;
      default:
        exception_state.ThrowDOMException(
            DOMExceptionCode::kSyntaxError,
            "rootMargin must be specified in pixels or percent.");
    }
  }
}

void ParseThresholds(const DoubleOrDoubleSequence& threshold_parameter,
                     Vector<float>& thresholds,
                     ExceptionState& exception_state) {
  if (threshold_parameter.IsDouble()) {
    thresholds.push_back(
        base::MakeClampedNum<float>(threshold_parameter.GetAsDouble()));
  } else {
    for (auto threshold_value : threshold_parameter.GetAsDoubleSequence())
      thresholds.push_back(base::MakeClampedNum<float>(threshold_value));
  }

  for (auto threshold_value : thresholds) {
    if (std::isnan(threshold_value) || threshold_value < 0.0 ||
        threshold_value > 1.0) {
      exception_state.ThrowRangeError(
          "Threshold values must be numbers between 0 and 1");
      break;
    }
  }

  std::sort(thresholds.begin(), thresholds.end());
}

}  // anonymous namespace

static bool throttle_delay_enabled = true;
const float IntersectionObserver::kMinimumThreshold =
    std::numeric_limits<float>::min();

void IntersectionObserver::SetThrottleDelayEnabledForTesting(bool enabled) {
  throttle_delay_enabled = enabled;
}

IntersectionObserver* IntersectionObserver::Create(
    const IntersectionObserverInit* observer_init,
    IntersectionObserverDelegate& delegate,
    ExceptionState& exception_state) {
  Node* root = nullptr;
  if (observer_init->root().IsElement()) {
    root = observer_init->root().GetAsElement();
  } else if (observer_init->root().IsDocument()) {
    root = observer_init->root().GetAsDocument();
  }

  DOMHighResTimeStamp delay = 0;
  bool track_visibility = false;
  delay = observer_init->delay();
  track_visibility = observer_init->trackVisibility();
  if (track_visibility && delay < 100) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "To enable the 'trackVisibility' option, you must also use a "
        "'delay' option with a value of at least 100. Visibility is more "
        "expensive to compute than the basic intersection; enabling this "
        "option may negatively affect your page's performance. Please make "
        "sure you *really* need visibility tracking before enabling the "
        "'trackVisibility' option.");
    return nullptr;
  }

  Vector<Length> margin;
  ParseMargin(observer_init->rootMargin(), margin, exception_state);
  if (exception_state.HadException())
    return nullptr;

  Vector<float> thresholds;
  ParseThresholds(observer_init->threshold(), thresholds, exception_state);
  if (exception_state.HadException())
    return nullptr;

  return MakeGarbageCollected<IntersectionObserver>(
      delegate, root, margin, thresholds, kFractionOfTarget, delay,
      track_visibility, false, kApplyMarginToRoot);
}

IntersectionObserver* IntersectionObserver::Create(
    ScriptState* script_state,
    V8IntersectionObserverCallback* callback,
    const IntersectionObserverInit* observer_init,
    ExceptionState& exception_state) {
  V8IntersectionObserverDelegate* delegate =
      MakeGarbageCollected<V8IntersectionObserverDelegate>(callback,
                                                           script_state);
  if (observer_init && observer_init->trackVisibility()) {
    UseCounter::Count(delegate->GetExecutionContext(),
                      WebFeature::kIntersectionObserverV2);
  }
  return Create(observer_init, *delegate, exception_state);
}

IntersectionObserver* IntersectionObserver::Create(
    const Vector<Length>& margin,
    const Vector<float>& thresholds,
    Document* document,
    EventCallback callback,
    DeliveryBehavior behavior,
    ThresholdInterpretation semantics,
    DOMHighResTimeStamp delay,
    bool track_visibility,
    bool always_report_root_bounds,
    MarginTarget margin_target,
    ExceptionState& exception_state) {
  IntersectionObserverDelegateImpl* intersection_observer_delegate =
      MakeGarbageCollected<IntersectionObserverDelegateImpl>(
          document->GetExecutionContext(), std::move(callback), behavior);
  return MakeGarbageCollected<IntersectionObserver>(
      *intersection_observer_delegate, nullptr, margin, thresholds, semantics,
      delay, track_visibility, always_report_root_bounds, margin_target);
}

IntersectionObserver::IntersectionObserver(
    IntersectionObserverDelegate& delegate,
    Node* root,
    const Vector<Length>& margin,
    const Vector<float>& thresholds,
    ThresholdInterpretation semantics,
    DOMHighResTimeStamp delay,
    bool track_visibility,
    bool always_report_root_bounds,
    MarginTarget margin_target)
    : ExecutionContextClient(delegate.GetExecutionContext()),
      delegate_(&delegate),
      root_(root),
      thresholds_(thresholds),
      delay_(delay),
      margin_(4, Length::Fixed(0)),
      margin_target_(margin_target),
      root_is_implicit_(root ? 0 : 1),
      track_visibility_(track_visibility),
      track_fraction_of_root_(semantics == kFractionOfRoot),
      always_report_root_bounds_(always_report_root_bounds),
      needs_delivery_(0),
      can_use_cached_rects_(0) {
  switch (margin.size()) {
    case 0:
      break;
    case 1:
      margin_[0] = margin_[1] = margin_[2] = margin_[3] = margin[0];
      break;
    case 2:
      margin_[0] = margin_[2] = margin[0];
      margin_[1] = margin_[3] = margin[1];
      break;
    case 3:
      margin_[0] = margin[0];
      margin_[1] = margin_[3] = margin[1];
      margin_[2] = margin[2];
      break;
    case 4:
      margin_[0] = margin[0];
      margin_[1] = margin[1];
      margin_[2] = margin[2];
      margin_[3] = margin[3];
      break;
    default:
      NOTREACHED();
      break;
  }
  if (root) {
    if (root->IsDocumentNode()) {
      To<Document>(root)
          ->EnsureDocumentExplicitRootIntersectionObserverData()
          .AddObserver(*this);
    } else {
      DCHECK(root->IsElementNode());
      To<Element>(root)->EnsureIntersectionObserverData().AddObserver(*this);
    }
  }
}

void IntersectionObserver::ProcessCustomWeakness(const LivenessBroker& info) {
  // For explicit-root observers, if the root element disappears for any reason,
  // any remaining obsevations must be dismantled.
  if (root() && !info.IsHeapObjectAlive(root()))
    root_ = nullptr;
  if (!RootIsImplicit() && !root())
    disconnect();
}

bool IntersectionObserver::RootIsValid() const {
  return RootIsImplicit() || root();
}

void IntersectionObserver::observe(Element* target,
                                   ExceptionState& exception_state) {
  if (!RootIsValid())
    return;

  if (!target || root() == target)
    return;

  if (target->EnsureIntersectionObserverData().GetObservationFor(*this))
    return;

  IntersectionObservation* observation =
      MakeGarbageCollected<IntersectionObservation>(*this, *target);
  target->EnsureIntersectionObserverData().AddObservation(*observation);
  observations_.insert(observation);
  if (root() && root()->isConnected()) {
    root()
        ->GetDocument()
        .EnsureIntersectionObserverController()
        .AddTrackedObserver(*this);
  }
  if (target->isConnected()) {
    target->GetDocument()
        .EnsureIntersectionObserverController()
        .AddTrackedObservation(*observation);
    if (LocalFrameView* frame_view = target->GetDocument().View()) {
      // The IntersectionObsever spec requires that at least one observation
      // be recorded after observe() is called, even if the frame is throttled.
      frame_view->SetIntersectionObservationState(LocalFrameView::kRequired);
      frame_view->ScheduleAnimation();
    }
  } else {
    // The IntersectionObsever spec requires that at least one observation
    // be recorded after observe() is called, even if the target is detached.
    observation->ComputeIntersection(
        IntersectionObservation::kImplicitRootObserversNeedUpdate |
        IntersectionObservation::kExplicitRootObserversNeedUpdate |
        IntersectionObservation::kIgnoreDelay);
  }
}

void IntersectionObserver::unobserve(Element* target,
                                     ExceptionState& exception_state) {
  if (!target || !target->IntersectionObserverData())
    return;

  IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*this);
  if (!observation)
    return;

  observation->Disconnect();
  observations_.erase(observation);
  if (root() && root()->isConnected() && observations_.IsEmpty()) {
    root()
        ->GetDocument()
        .EnsureIntersectionObserverController()
        .RemoveTrackedObserver(*this);
  }
}

void IntersectionObserver::disconnect(ExceptionState& exception_state) {
  for (auto& observation : observations_)
    observation->Disconnect();
  observations_.clear();
  if (root() && root()->isConnected()) {
    root()
        ->GetDocument()
        .EnsureIntersectionObserverController()
        .RemoveTrackedObserver(*this);
  }
}

HeapVector<Member<IntersectionObserverEntry>> IntersectionObserver::takeRecords(
    ExceptionState& exception_state) {
  needs_delivery_ = 0;
  HeapVector<Member<IntersectionObserverEntry>> entries;
  for (auto& observation : observations_)
    observation->TakeRecords(entries);
  return entries;
}

static void AppendLength(StringBuilder& string_builder, const Length& length) {
  string_builder.AppendNumber(length.IntValue());
  if (length.IsPercent())
    string_builder.Append('%');
  else
    string_builder.Append("px", 2);
}

String IntersectionObserver::rootMargin() const {
  StringBuilder string_builder;
  const auto& margin = RootMargin();
  if (margin.IsEmpty()) {
    string_builder.Append("0px 0px 0px 0px");
  } else {
    DCHECK_EQ(margin.size(), 4u);
    AppendLength(string_builder, margin[0]);
    string_builder.Append(' ');
    AppendLength(string_builder, margin[1]);
    string_builder.Append(' ');
    AppendLength(string_builder, margin[2]);
    string_builder.Append(' ');
    AppendLength(string_builder, margin[3]);
  }
  return string_builder.ToString();
}

DOMHighResTimeStamp IntersectionObserver::GetEffectiveDelay() const {
  return throttle_delay_enabled ? delay_ : 0;
}

DOMHighResTimeStamp IntersectionObserver::GetTimeStamp() const {
  return DOMWindowPerformance::performance(
             *To<LocalDOMWindow>(delegate_->GetExecutionContext()))
      ->now();
}

bool IntersectionObserver::ComputeIntersections(unsigned flags) {
  DCHECK(!RootIsImplicit());
  if (!RootIsValid() || !GetExecutionContext() || observations_.IsEmpty())
    return false;
  IntersectionGeometry::RootGeometry root_geometry(
      IntersectionGeometry::GetRootLayoutObjectForTarget(root(), nullptr,
                                                         false),
      RootMargin());
  HeapVector<Member<IntersectionObservation>> observations_to_process;
  // TODO(szager): Is this copy necessary?
  CopyToVector(observations_, observations_to_process);
  for (auto& observation : observations_to_process) {
    observation->ComputeIntersection(root_geometry, flags);
  }
  can_use_cached_rects_ = 1;
  return trackVisibility();
}

void IntersectionObserver::SetNeedsDelivery() {
  if (needs_delivery_)
    return;
  needs_delivery_ = 1;
  To<LocalDOMWindow>(GetExecutionContext())
      ->document()
      ->EnsureIntersectionObserverController()
      .ScheduleIntersectionObserverForDelivery(*this);
}

IntersectionObserver::DeliveryBehavior
IntersectionObserver::GetDeliveryBehavior() const {
  return delegate_->GetDeliveryBehavior();
}

void IntersectionObserver::Deliver() {
  if (!needs_delivery_)
    return;
  needs_delivery_ = 0;
  HeapVector<Member<IntersectionObserverEntry>> entries;
  for (auto& observation : observations_)
    observation->TakeRecords(entries);
  if (entries.size())
    delegate_->Deliver(entries, *this);
}

bool IntersectionObserver::HasPendingActivity() const {
  return !observations_.IsEmpty();
}

void IntersectionObserver::Trace(Visitor* visitor) const {
  visitor->template RegisterWeakCallbackMethod<
      IntersectionObserver, &IntersectionObserver::ProcessCustomWeakness>(this);
  visitor->Trace(delegate_);
  visitor->Trace(observations_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
