// Copyright (c) 2017 Opera Software AS. All rights reserved.

#include "core/events/VRPlayerErrorEvent.h"

namespace blink {

VRPlayerErrorEvent::VRPlayerErrorEvent(unsigned short type)
    : Event(EventTypeNames::operavrplayererror, false, false), type_(type) {}

VRPlayerErrorEvent::~VRPlayerErrorEvent() {}

const AtomicString& VRPlayerErrorEvent::InterfaceName() const {
  return EventNames::VRPlayerErrorEvent;
}

void VRPlayerErrorEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
}

}  // namespace blink
