/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebDOMEvent_h
#define WebDOMEvent_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"

namespace blink {

class Event;

class WebDOMEvent {
 public:
  ~WebDOMEvent() { Reset(); }

  WebDOMEvent() {}
  WebDOMEvent(const WebDOMEvent& other) { Assign(other); }
  WebDOMEvent& operator=(const WebDOMEvent& e) {
    Assign(e);
    return *this;
  }

  BLINK_EXPORT void Reset();
  BLINK_EXPORT void Assign(const WebDOMEvent&);

  bool IsNull() const { return private_.IsNull(); }

#if BLINK_IMPLEMENTATION
  BLINK_EXPORT WebDOMEvent(Event*);
  BLINK_EXPORT operator Event*() const;
#endif

  template <typename T>
  T To() {
    T res;
    res.WebDOMEvent::assign(*this);
    return res;
  }

  template <typename T>
  const T ToConst() const {
    T res;
    res.WebDOMEvent::assign(*this);
    return res;
  }

 protected:
#if BLINK_IMPLEMENTATION
  void Assign(Event*);

  template <typename T>
  T* Unwrap() {
    return static_cast<T*>(private_.Get());
  }

  template <typename T>
  const T* ConstUnwrap() const {
    return static_cast<const T*>(private_.Get());
  }
#endif

  WebPrivatePtr<Event> private_;
};

}  // namespace blink

#endif
