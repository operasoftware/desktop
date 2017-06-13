// Copyright (c) 2016 Opera Software AS. All rights reserved.

#ifndef DetachedViewControlEvent_h
#define DetachedViewControlEvent_h

#include "core/clipboard/DataTransfer.h"
#include "core/events/Event.h"

namespace blink {

class DetachedViewControlEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();

public:
    ~DetachedViewControlEvent() override;

    static DetachedViewControlEvent* create()
    {
        return new DetachedViewControlEvent();
    }

    static DetachedViewControlEvent* create(const String& controlName)
    {
        return new DetachedViewControlEvent(controlName);
    }

    const String& controlName() const { return m_controlName; }

    DECLARE_VIRTUAL_TRACE();

private:
    DetachedViewControlEvent();
    DetachedViewControlEvent(const String& controlName);

    const AtomicString& interfaceName() const override;

    String m_controlName;
};

} // namespace blink

#endif // DetachedViewControlEvent_h
