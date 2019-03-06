// Copyright (c) 2016 Opera Software AS. All rights reserved.

#ifndef DetachedViewControlEvent_h
#define DetachedViewControlEvent_h

#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

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

    void Trace(Visitor*) override;

   private:
    DetachedViewControlEvent();
    DetachedViewControlEvent(const String& control_name);

    const AtomicString& InterfaceName() const override;

    String control_name_;
};

} // namespace blink

#endif // DetachedViewControlEvent_h
