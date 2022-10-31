// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_DECODER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_DECODER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {

// Produces SW video decoders that delegate to OS APIs for actual decoding.
class RTCPlatformSWVideoDecoderFactory final
    : public webrtc::VideoDecoderFactory {
 public:
  explicit RTCPlatformSWVideoDecoderFactory(
      scoped_refptr<base::SequencedTaskRunner> media_task_runner);
  ~RTCPlatformSWVideoDecoderFactory() override;

  RTCPlatformSWVideoDecoderFactory(const RTCPlatformSWVideoDecoderFactory&) =
      delete;
  RTCPlatformSWVideoDecoderFactory& operator=(
      const RTCPlatformSWVideoDecoderFactory&) = delete;

  // webrtc::VideoDecoderFactory
  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override;
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

 private:
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_DECODER_FACTORY_H_
