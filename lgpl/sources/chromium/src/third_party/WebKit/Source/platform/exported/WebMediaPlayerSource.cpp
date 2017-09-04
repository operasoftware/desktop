// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMediaPlayerSource.h"

namespace blink {

WebMediaPlayerSource::WebMediaPlayerSource() {}

WebMediaPlayerSource::WebMediaPlayerSource(const WebURL& url) : url_(url) {}

WebMediaPlayerSource::WebMediaPlayerSource(const WebMediaStream& media_stream)
    : media_stream_(media_stream) {}

WebMediaPlayerSource::~WebMediaPlayerSource() {
  media_stream_.Reset();
}

bool WebMediaPlayerSource::IsURL() const {
  return !url_.IsEmpty();
}

WebURL WebMediaPlayerSource::GetAsURL() const {
  return url_;
}

bool WebMediaPlayerSource::IsMediaStream() const {
  return !media_stream_.IsNull();
}

WebMediaStream WebMediaPlayerSource::GetAsMediaStream() const {
  return media_stream_;
}

}  // namespace blink
