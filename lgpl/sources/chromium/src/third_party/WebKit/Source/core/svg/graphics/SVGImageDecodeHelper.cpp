// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/graphics/SVGImageDecodeHelper.h"

#include "core/svg/graphics/SVGImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"
#include "platform/SharedBuffer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

SkBitmap decodeSVGImage(const unsigned char* data,
                        size_t length,
                        const IntSize& size) {
  RefPtr<SVGImage> svgImage = SVGImage::create(nullptr);
  RefPtr<SharedBuffer> buffer = SharedBuffer::create(data, length);
  svgImage->setData(buffer, true);
  RefPtr<Image> svgContainer =
      SVGImageForContainer::create(svgImage.get(), size, 1, KURL());
  sk_sp<SkImage> skImage =
      svgContainer->imageForCurrentFrame(ColorBehavior::ignore());
  SkBitmap bitmap;
  skImage->asLegacyBitmap(&bitmap, SkImage::kRO_LegacyBitmapMode);
  return bitmap;
}

}  // namespace blink
