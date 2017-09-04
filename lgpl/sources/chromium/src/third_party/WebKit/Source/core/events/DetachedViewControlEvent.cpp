// Copyright (c) 2016 Opera Software AS. All rights reserved.

#include "core/events/DetachedViewControlEvent.h"

namespace blink {

DetachedViewControlEvent::DetachedViewControlEvent()
{
}

DetachedViewControlEvent::DetachedViewControlEvent(const String& control_name)
    : Event(EventTypeNames::operadetachedviewcontrol, false, false)
    , control_name_(control_name)
{
}

DetachedViewControlEvent::~DetachedViewControlEvent()
{
}

const AtomicString& DetachedViewControlEvent::InterfaceName() const
{
    return EventNames::DetachedViewControlEvent;
}

DEFINE_TRACE(DetachedViewControlEvent)
{
    Event::Trace(visitor);
}

} // namespace blink
