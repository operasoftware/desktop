// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/avif/avif_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreateAVIFDecoder() {
  // TODO(crbug.com/323934468): Initialize decoder settings dynamically using
  // fuzzer input.
  return std::make_unique<AVIFImageDecoder>(
      ImageDecoder::kAlphaPremultiplied, ImageDecoder::kDefaultBitDepth,
      ColorBehavior::kTransformToSRGB, ImageDecoder::kNoDecodedImageByteLimit,
      ImageDecoder::AnimationOption::kPreferAnimation);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support;
  auto buffer = SharedBuffer::Create(data, size);
  auto decoder = CreateAVIFDecoder();
  constexpr static bool kAllDataReceived = true;
  decoder->SetData(buffer.get(), kAllDataReceived);
  for (wtf_size_t frame = 0; frame < decoder->FrameCount(); ++frame) {
    decoder->DecodeFrameBufferAtIndex(frame);
    if (decoder->Failed()) {
      return 0;
    }
  }
  return 0;
}

}  // namespace blink