// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/public/web/modules/mediasession/media_session_utils.h"

#include "base/check.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_session.h"
#include "third_party/blink/renderer/modules/mediasession/media_session.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-value.h"

namespace blink {

int GetMediaSessionActionHandlerId(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   const std::string& action) {
  blink::MediaSession* media_session =
      blink::V8MediaSession::ToWrappable(isolate, value);
  CHECK(media_session) << "MediaSession required as argument";
  return media_session->getActionHandlerId(WTF::String::FromUTF8(action));
}

}  // namespace blink
