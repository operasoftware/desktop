// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid.h"

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_device_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_device_request_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/hid/hid_connection_event.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

const char kContextGone[] = "Script context has shut down.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"hid\" is disallowed by permissions policy.";

// Carries out basic checks for the web-exposed APIs, to make sure the minimum
// requirements for them to be served are met. Returns true if any conditions
// fail to be met, generating an appropriate exception as well. Otherwise,
// returns false to indicate the call should be allowed.
bool ShouldBlockHidServiceCall(ExecutionContext* context,
                               ExceptionState& exception_state) {
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
  } else if (!context->IsFeatureEnabled(
                 mojom::blink::PermissionsPolicyFeature::kHid,
                 ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
  }

  return exception_state.HadException();
}

void RejectWithTypeError(const String& message,
                         ScriptPromiseResolver* resolver) {
  ScriptState::Scope scope(resolver->GetScriptState());
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  resolver->Reject(V8ThrowException::CreateTypeError(isolate, message));
}

}  // namespace

const char HID::kSupplementName[] = "HID";

HID* HID::hid(NavigatorBase& navigator) {
  HID* hid = Supplement<NavigatorBase>::From<HID>(navigator);
  if (!hid) {
    hid = MakeGarbageCollected<HID>(navigator);
    ProvideTo(navigator, hid);
  }
  return hid;
}

HID::HID(NavigatorBase& navigator)
    : ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      Supplement<NavigatorBase>(navigator),
      service_(navigator.GetExecutionContext()) {
  auto* context = GetExecutionContext();
  if (context) {
    feature_handle_for_scheduler_ = context->GetScheduler()->RegisterFeature(
        SchedulingPolicy::Feature::kWebHID,
        {SchedulingPolicy::DisableBackForwardCache()});
  }
}

HID::~HID() {
  DCHECK(get_devices_promises_.IsEmpty());
  DCHECK(request_device_promises_.IsEmpty());
}

ExecutionContext* HID::GetExecutionContext() const {
  return GetSupplementable()->GetExecutionContext();
}

const AtomicString& HID::InterfaceName() const {
  return event_target_names::kHID;
}

void HID::ContextDestroyed() {
  CloseServiceConnection();
}

void HID::AddedEventListener(const AtomicString& event_type,
                             RegisteredEventListener& listener) {
  EventTargetWithInlineData::AddedEventListener(event_type, listener);

  if (event_type != event_type_names::kConnect &&
      event_type != event_type_names::kDisconnect) {
    return;
  }

  auto* context = GetExecutionContext();
  if (!context ||
      !context->IsFeatureEnabled(mojom::blink::PermissionsPolicyFeature::kHid,
                                 ReportOptions::kDoNotReport)) {
    return;
  }

  EnsureServiceConnection();
}

void HID::DeviceAdded(device::mojom::blink::HidDeviceInfoPtr device_info) {
  auto* device = GetOrCreateDevice(std::move(device_info));

  DispatchEvent(*MakeGarbageCollected<HIDConnectionEvent>(
      event_type_names::kConnect, device));
}

void HID::DeviceRemoved(device::mojom::blink::HidDeviceInfoPtr device_info) {
  auto* device = GetOrCreateDevice(std::move(device_info));

  DispatchEvent(*MakeGarbageCollected<HIDConnectionEvent>(
      event_type_names::kDisconnect, device));
}

void HID::DeviceChanged(device::mojom::blink::HidDeviceInfoPtr device_info) {
  auto it = device_cache_.find(device_info->guid);
  if (it != device_cache_.end()) {
    it->value->UpdateDeviceInfo(std::move(device_info));
    return;
  }

  // If the GUID is not in the |device_cache_| then this is the first time we
  // have been notified for this device.
  DeviceAdded(std::move(device_info));
}

ScriptPromise HID::getDevices(ScriptState* script_state,
                              ExceptionState& exception_state) {
  if (ShouldBlockHidServiceCall(GetExecutionContext(), exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  get_devices_promises_.insert(resolver);

  EnsureServiceConnection();
  service_->GetDevices(WTF::Bind(&HID::FinishGetDevices, WrapPersistent(this),
                                 WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise HID::requestDevice(ScriptState* script_state,
                                 const HIDDeviceRequestOptions* options,
                                 ExceptionState& exception_state) {
  // requestDevice requires a window to satisfy the user activation requirement
  // and to show a chooser dialog.
  const auto* window = GetSupplementable()->DomWindow();
  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
    return ScriptPromise();
  }

  if (ShouldBlockHidServiceCall(GetExecutionContext(), exception_state)) {
    return ScriptPromise();
  }

  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a permission request.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  request_device_promises_.insert(resolver);

  Vector<mojom::blink::HidDeviceFilterPtr> mojo_filters;
  if (options->hasFilters()) {
    mojo_filters.ReserveCapacity(options->filters().size());
    for (const auto& filter : options->filters()) {
      String error_message = CheckDeviceFilterValidity(*filter);
      if (error_message) {
        RejectWithTypeError(error_message, resolver);
        return promise;
      }
      mojo_filters.push_back(ConvertDeviceFilter(*filter));
    }
  }
  DCHECK_EQ(options->filters().size(), mojo_filters.size());

  Vector<mojom::blink::HidDeviceFilterPtr> mojo_exclusion_filters;
  if (options->hasExclusionFilters()) {
    if (options->exclusionFilters().size() == 0) {
      exception_state.ThrowTypeError(
          "'exclusionFilters', if present, must contain at least one filter.");
      return ScriptPromise();
    }
    mojo_exclusion_filters.ReserveCapacity(options->exclusionFilters().size());
    for (const auto& exclusion_filter : options->exclusionFilters()) {
      String error_message = CheckDeviceFilterValidity(*exclusion_filter);
      if (error_message) {
        RejectWithTypeError(error_message, resolver);
        return promise;
      }
      mojo_exclusion_filters.push_back(ConvertDeviceFilter(*exclusion_filter));
    }
    DCHECK_EQ(options->exclusionFilters().size(),
              mojo_exclusion_filters.size());
  }

  EnsureServiceConnection();
  service_->RequestDevice(
      std::move(mojo_filters), std::move(mojo_exclusion_filters),
      WTF::Bind(&HID::FinishRequestDevice, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void HID::Connect(
    const String& device_guid,
    mojo::PendingRemote<device::mojom::blink::HidConnectionClient> client,
    device::mojom::blink::HidManager::ConnectCallback callback) {
  EnsureServiceConnection();
  service_->Connect(device_guid, std::move(client), std::move(callback));
}

void HID::Forget(device::mojom::blink::HidDeviceInfoPtr device_info,
                 mojom::blink::HidService::ForgetCallback callback) {
  EnsureServiceConnection();
  service_->Forget(std::move(device_info), std::move(callback));
}

HIDDevice* HID::GetOrCreateDevice(device::mojom::blink::HidDeviceInfoPtr info) {
  auto it = device_cache_.find(info->guid);
  if (it != device_cache_.end()) {
    return it->value;
  }

  const String guid = info->guid;
  HIDDevice* device = MakeGarbageCollected<HIDDevice>(this, std::move(info),
                                                      GetExecutionContext());
  device_cache_.insert(guid, device);
  return device;
}

void HID::FinishGetDevices(
    ScriptPromiseResolver* resolver,
    Vector<device::mojom::blink::HidDeviceInfoPtr> device_infos) {
  DCHECK(get_devices_promises_.Contains(resolver));
  get_devices_promises_.erase(resolver);

  HeapVector<Member<HIDDevice>> devices;
  for (auto& device_info : device_infos)
    devices.push_back(GetOrCreateDevice(std::move(device_info)));

  resolver->Resolve(devices);
}

void HID::FinishRequestDevice(
    ScriptPromiseResolver* resolver,
    Vector<device::mojom::blink::HidDeviceInfoPtr> device_infos) {
  DCHECK(request_device_promises_.Contains(resolver));
  request_device_promises_.erase(resolver);

  HeapVector<Member<HIDDevice>> devices;
  for (auto& device_info : device_infos)
    devices.push_back(GetOrCreateDevice(std::move(device_info)));

  resolver->Resolve(devices);
}

void HID::EnsureServiceConnection() {
  DCHECK(GetExecutionContext());

  if (service_.is_bound())
    return;

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(
      WTF::Bind(&HID::CloseServiceConnection, WrapWeakPersistent(this)));
  DCHECK(!receiver_.is_bound());
  service_->RegisterClient(receiver_.BindNewEndpointAndPassRemote());
}

void HID::CloseServiceConnection() {
  service_.reset();
  receiver_.reset();

  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> get_devices_promises;
  get_devices_promises_.swap(get_devices_promises);
  for (ScriptPromiseResolver* resolver : get_devices_promises)
    resolver->Resolve(HeapVector<Member<HIDDevice>>());

  HeapHashSet<Member<ScriptPromiseResolver>> request_device_promises;
  request_device_promises_.swap(request_device_promises);
  for (ScriptPromiseResolver* resolver : request_device_promises)
    resolver->Resolve(HeapVector<Member<HIDDevice>>());
}

mojom::blink::HidDeviceFilterPtr HID::ConvertDeviceFilter(
    const HIDDeviceFilter& filter) {
  DCHECK(!CheckDeviceFilterValidity(filter));

  auto mojo_filter = mojom::blink::HidDeviceFilter::New();
  if (filter.hasVendorId()) {
    if (filter.hasProductId()) {
      mojo_filter->device_ids =
          mojom::blink::DeviceIdFilter::NewVendorAndProduct(
              mojom::blink::VendorAndProduct::New(filter.vendorId(),
                                                  filter.productId()));
    } else {
      mojo_filter->device_ids =
          mojom::blink::DeviceIdFilter::NewVendor(filter.vendorId());
    }
  }
  if (filter.hasUsagePage()) {
    if (filter.hasUsage()) {
      mojo_filter->usage = mojom::blink::UsageFilter::NewUsageAndPage(
          device::mojom::blink::HidUsageAndPage::New(filter.usage(),
                                                     filter.usagePage()));
    } else {
      mojo_filter->usage =
          mojom::blink::UsageFilter::NewPage(filter.usagePage());
    }
  }
  return mojo_filter;
}

String HID::CheckDeviceFilterValidity(const HIDDeviceFilter& filter) {
  if (!filter.hasVendorId() && !filter.hasProductId() &&
      !filter.hasUsagePage() && !filter.hasUsage()) {
    return "A filter must provide a property to filter by.";
  }

  if (filter.hasProductId() && !filter.hasVendorId()) {
    return "A filter containing a productId must also contain a vendorId.";
  }

  if (filter.hasUsage() && !filter.hasUsagePage()) {
    return "A filter containing a usage must also contain a usagePage.";
  }

  return String();
}

void HID::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(get_devices_promises_);
  visitor->Trace(request_device_promises_);
  visitor->Trace(device_cache_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

}  // namespace blink
