// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGImageDecodeHelper_h
#define SVGImageDecodeHelper_h

#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class IntSize;

SkBitmap decodeSVGImage(const unsigned char* data,
                        size_t length,
                        const IntSize& size);

}  // namespace blink

#endif  // SVGImageDecodeHelper_h
