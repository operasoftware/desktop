// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/svg/SVGImageDecoder.h"

#include "third_party/skia/include/core/SkData.h"

namespace blink {

static SVGDecodeFunction s_decodeFunction = nullptr;

static const int imageSizes[] = {128, 64, 32, 16};

void Decode(ImageFrame* frame, int size, SkData* data) {
  SkBitmap bitmap =
      s_decodeFunction(data->bytes(), data->size(), IntSize(size, size));
  ImageFrame tmpFrame(bitmap);
  frame->CopyBitmapData(tmpFrame);
  frame->SetStatus(ImageFrame::kFrameComplete);
}

SVGImageDecoder::SVGImageDecoder(AlphaOption alphaOption,
                                 const ColorBehavior& colorBehavior,
                                 size_t maxDecodedBytes)
    : ImageDecoder(alphaOption, colorBehavior, maxDecodedBytes) {
  SetSize(imageSizes[0], imageSizes[0]);
}

void SVGImageDecoder::SetDecodeFunction(SVGDecodeFunction function) {
  s_decodeFunction = function;
}

void SVGImageDecoder::DecodeSize() {
  DCHECK(IsSizeAvailable());
}

void SVGImageDecoder::Decode(size_t idx) {
  DCHECK(IsSizeAvailable());
  DCHECK(frame_buffer_cache_.size() == 4);
  DCHECK(s_decodeFunction);

  sk_sp<SkData> image_data = data_->GetAsSkData();
  blink::Decode(&frame_buffer_cache_[idx], imageSizes[idx], image_data.get());
}

size_t SVGImageDecoder::DecodeFrameCount() {
  return arraysize(imageSizes);
}

IntSize SVGImageDecoder::FrameSizeAtIndex(size_t idx) const {
  return IntSize(imageSizes[idx], imageSizes[idx]);
}

}  // namespace blink
