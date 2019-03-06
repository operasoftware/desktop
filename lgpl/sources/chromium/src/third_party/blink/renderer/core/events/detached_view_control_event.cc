// Copyright (c) 2016 Opera Software AS. All rights reserved.

#include "third_party/blink/renderer/core/events/detached_view_control_event.h"

namespace blink {

DetachedViewControlEvent::DetachedViewControlEvent()
{
}

DetachedViewControlEvent::DetachedViewControlEvent(const String& control_name)
  : Event(EventTypeNames::operadetachedviewcontrol,
          Event::Bubbles::kNo,
          Event::Cancelable::kNo),
    control_name_(control_name) {}
DetachedViewControlEvent::~DetachedViewControlEvent()
{
}

const AtomicString& DetachedViewControlEvent::InterfaceName() const
{
    return EventNames::DetachedViewControlEvent;
}

void DetachedViewControlEvent::Trace(Visitor* visitor) {
  Event::Trace(visitor);
}

} // namespace blink
