// Copyright (c) 2017 Opera Software AS. All rights reserved.

#ifndef VRPlayerErrorEvent_h
#define VRPlayerErrorEvent_h

#include "core/dom/events/Event.h"

namespace blink {

class VRPlayerErrorEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum ErrorType {
    kOk,
    kNoApi,
    kNoHmd,
    kRendererError,
    kVrSessionError,
    kServiceConnectionError,
    kAcquireError,
    kVideoFrameError,
  };

  ~VRPlayerErrorEvent() override;

  static VRPlayerErrorEvent* Create(unsigned short type) {
    return new VRPlayerErrorEvent(type);
  }

  unsigned short errorType() const { return type_; }

  DECLARE_VIRTUAL_TRACE();

 private:
  VRPlayerErrorEvent(unsigned short type);

  const AtomicString& InterfaceName() const override;

  unsigned short type_;
};

}  // namespace blink

#endif  // VRPlayerErrorEvent_h
