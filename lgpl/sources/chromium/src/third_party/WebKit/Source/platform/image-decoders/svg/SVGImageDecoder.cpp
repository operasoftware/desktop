// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/svg/SVGImageDecoder.h"

namespace blink {

static SVGDecodeFunction s_decodeFunction = nullptr;

static const int imageSizes[] = {128, 64, 32, 16};

void Decode(ImageFrame* frame, int size, SkData* data) {
  SkBitmap bitmap =
      s_decodeFunction(data->bytes(), data->size(), IntSize(size, size));
  ImageFrame tmpFrame(bitmap);
  frame->copyBitmapData(tmpFrame);
  frame->setStatus(ImageFrame::FrameComplete);
}

SVGImageDecoder::SVGImageDecoder(AlphaOption alphaOption,
                                 const ColorBehavior& colorBehavior,
                                 size_t maxDecodedBytes)
    : ImageDecoder(alphaOption, colorBehavior, maxDecodedBytes) {
  setSize(imageSizes[0], imageSizes[0]);
}

void SVGImageDecoder::setDecodeFunction(SVGDecodeFunction function) {
  s_decodeFunction = function;
}

void SVGImageDecoder::decodeSize() {
  DCHECK(isSizeAvailable());
}

void SVGImageDecoder::decode(size_t idx) {
  DCHECK(isSizeAvailable());
  DCHECK(m_frameBufferCache.size() == 4);
  DCHECK(s_decodeFunction);

  sk_sp<SkData> image_data = m_data->getAsSkData();
  Decode(&m_frameBufferCache[idx], imageSizes[idx], image_data.get());
}

size_t SVGImageDecoder::decodeFrameCount() {
  return arraysize(imageSizes);
}

IntSize SVGImageDecoder::frameSizeAtIndex(size_t idx) const {
  return IntSize(imageSizes[idx], imageSizes[idx]);
}

}  // namespace blink
