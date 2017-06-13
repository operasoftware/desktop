// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGImageDecoder_h
#define SVGImageDecoder_h

#include "platform/image-decoders/ImageDecoder.h"

namespace blink {

typedef SkBitmap (*SVGDecodeFunction)(const unsigned char* data,
                                      size_t length,
                                      const IntSize& size);

// This class decodes the SVG image format.
class PLATFORM_EXPORT SVGImageDecoder final : public ImageDecoder {
 public:
  SVGImageDecoder(AlphaOption, const ColorBehavior&, size_t maxDecodedBytes);
  static void setDecodeFunction(SVGDecodeFunction function);
  String filenameExtension() const override { return "svg"; }
  void decodeSize() override;
  void decode(size_t) override;
  size_t decodeFrameCount() override;
  IntSize frameSizeAtIndex(size_t) const override;
};

}  // namespace blink

#endif  // SVGImageDecoder_h
