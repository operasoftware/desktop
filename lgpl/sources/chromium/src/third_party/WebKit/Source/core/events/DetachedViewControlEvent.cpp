// Copyright (c) 2016 Opera Software AS. All rights reserved.

#include "core/events/DetachedViewControlEvent.h"

namespace blink {

DetachedViewControlEvent::DetachedViewControlEvent()
{
}

DetachedViewControlEvent::DetachedViewControlEvent(const String& controlName)
    : Event(EventTypeNames::operadetachedviewcontrol, false, false)
    , m_controlName(controlName)
{
}

DetachedViewControlEvent::~DetachedViewControlEvent()
{
}

const AtomicString& DetachedViewControlEvent::interfaceName() const
{
    return EventNames::DetachedViewControlEvent;
}

DEFINE_TRACE(DetachedViewControlEvent)
{
    Event::trace(visitor);
}

} // namespace blink
