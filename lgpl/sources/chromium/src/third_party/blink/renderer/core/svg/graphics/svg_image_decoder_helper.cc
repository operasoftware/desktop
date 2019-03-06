// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/graphics/svg_image_decoder_helper.h"

#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

SkBitmap DecodeSVGImage(const unsigned char* data,
                        size_t length,
                        const IntSize& size) {
  scoped_refptr<SVGImage> svg_image = SVGImage::Create(nullptr);
  scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create(data, length);
  svg_image->SetData(buffer, true);
  scoped_refptr<Image> svg_container =
      SVGImageForContainer::Create(svg_image.get(), FloatSize(size), 1, KURL());
  sk_sp<SkImage> sk_image =
      svg_container->PaintImageForCurrentFrame().GetSkImage();
  SkBitmap bitmap;
  sk_image->asLegacyBitmap(&bitmap, SkImage::kRO_LegacyBitmapMode);
  return bitmap;
}

}  // namespace blink
