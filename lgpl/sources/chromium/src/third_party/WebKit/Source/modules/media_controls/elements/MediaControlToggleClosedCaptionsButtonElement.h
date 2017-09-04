// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlToggleClosedCaptionsButtonElement_h
#define MediaControlToggleClosedCaptionsButtonElement_h

#include "core/html/shadow/MediaControlElementTypes.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlToggleClosedCaptionsButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlToggleClosedCaptionsButtonElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  bool WillRespondToMouseClickEvents() override;
  void UpdateDisplayType() override;
  WebLocalizedString::Name GetOverflowStringName() override;
  bool HasOverflowButton() override;

 private:
  void DefaultEventHandler(Event*) override;
};

}  // namespace blink

#endif  // MediaControlToggleClosedCaptionsButtonElement_h
