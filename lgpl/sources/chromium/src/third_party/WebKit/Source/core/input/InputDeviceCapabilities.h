// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InputDeviceCapabilities_h
#define InputDeviceCapabilities_h

#include "core/CoreExport.h"
#include "core/input/InputDeviceCapabilitiesInit.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CORE_EXPORT InputDeviceCapabilities final
    : public GarbageCollected<InputDeviceCapabilities>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static InputDeviceCapabilities* Create(bool fires_touch_events) {
    return new InputDeviceCapabilities(fires_touch_events);
  }

  static InputDeviceCapabilities* Create(
      const InputDeviceCapabilitiesInit& initializer) {
    return new InputDeviceCapabilities(initializer);
  }

  bool firesTouchEvents() const { return fires_touch_events_; }

  DEFINE_INLINE_TRACE() {}

 private:
  InputDeviceCapabilities(bool fires_touch_events);
  InputDeviceCapabilities(const InputDeviceCapabilitiesInit&);

  // Whether this device dispatches touch events. This mainly lets developers
  // avoid handling both touch and mouse events dispatched for a single user
  // action.
  bool fires_touch_events_;
};

// Grouping constant-valued InputDeviceCapabilities objects together,
// which is kept and used by each 'view' (DOMWindow) that dispatches
// events parameterized over InputDeviceCapabilities.
//
// TODO(sof): lazily instantiate InputDeviceCapabilities instances upon
// UIEvent access instead. This would allow internal tracking of such
// capabilities by value.
class InputDeviceCapabilitiesConstants final
    : public GarbageCollected<InputDeviceCapabilitiesConstants> {
 public:
  // Returns an InputDeviceCapabilities which has
  // |firesTouchEvents| set to value of |firesTouch|.
  InputDeviceCapabilities* FiresTouchEvents(bool fires_touch);

  DEFINE_INLINE_TRACE() {
    visitor->Trace(fires_touch_events_);
    visitor->Trace(doesnt_fire_touch_events_);
  }

 private:
  Member<InputDeviceCapabilities> fires_touch_events_;
  Member<InputDeviceCapabilities> doesnt_fire_touch_events_;
};

}  // namespace blink

#endif  // InputDeviceCapabilities_h
