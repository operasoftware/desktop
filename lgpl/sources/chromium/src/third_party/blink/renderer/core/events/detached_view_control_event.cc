// Copyright (c) 2016 Opera Software AS. All rights reserved.

#include "third_party/blink/renderer/core/events/detached_view_control_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// static
DetachedViewControlEvent* DetachedViewControlEvent::Create() {
  return MakeGarbageCollected<DetachedViewControlEvent>();
}

// static
DetachedViewControlEvent* DetachedViewControlEvent::Create(
    const String& control_name) {
  return MakeGarbageCollected<DetachedViewControlEvent>(control_name);
}

DetachedViewControlEvent::DetachedViewControlEvent() {}

DetachedViewControlEvent::DetachedViewControlEvent(const String& control_name)
    : Event(event_type_names::kOperadetachedviewcontrol,
            Event::Bubbles::kNo,
            Event::Cancelable::kNo),
      control_name_(control_name) {}

DetachedViewControlEvent::~DetachedViewControlEvent() {}

const AtomicString& DetachedViewControlEvent::InterfaceName() const {
  return event_interface_names::kDetachedViewControlEvent;
}

void DetachedViewControlEvent::Trace(Visitor* visitor) {
  Event::Trace(visitor);
}

} // namespace blink
