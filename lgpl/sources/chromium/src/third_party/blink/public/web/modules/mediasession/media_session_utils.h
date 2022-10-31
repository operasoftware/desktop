// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASESSION_MEDIA_SESSION_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASESSION_MEDIA_SESSION_UTILS_H_

#include <string>

#include "third_party/blink/public/platform/web_common.h"
#include "v8/include/v8-local-handle.h"

namespace v8 {
class Isolate;
class Value;
}  // namespace v8

namespace blink {

BLINK_MODULES_EXPORT int GetMediaSessionActionHandlerId(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const std::string& action);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASESSION_MEDIA_SESSION_UTILS_H_
