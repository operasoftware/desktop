// Copyright (c) 2016 Opera Software AS. All rights reserved.

#ifndef DetachedViewControlEvent_h
#define DetachedViewControlEvent_h

#include "core/clipboard/DataTransfer.h"
#include "core/dom/events/Event.h"

namespace blink {

class DetachedViewControlEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();

public:
    ~DetachedViewControlEvent() override;

    static DetachedViewControlEvent* Create()
    {
        return new DetachedViewControlEvent();
    }

    static DetachedViewControlEvent* Create(const String& control_name)
    {
        return new DetachedViewControlEvent(control_name);
    }

    const String& controlName() const { return control_name_; }

    virtual void Trace(Visitor*);

   private:
    DetachedViewControlEvent();
    DetachedViewControlEvent(const String& control_name);

    const AtomicString& InterfaceName() const override;

    String control_name_;
};

} // namespace blink

#endif // DetachedViewControlEvent_h
