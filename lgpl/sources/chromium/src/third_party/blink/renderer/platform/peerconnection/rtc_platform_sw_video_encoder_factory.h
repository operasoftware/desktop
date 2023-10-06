// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_ENCODER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_ENCODER_FACTORY_H_

#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace blink {

// Produces SW video eccoders that delegate to OS APIs for actual encoding.
class RTCPlatformSWVideoEncoderFactory final
    : public webrtc::VideoEncoderFactory {
 public:
  // webrtc::VideoDecoderFactory:
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
  CodecSupport QueryCodecSupport(
      const webrtc::SdpVideoFormat& format,
      absl::optional<std::string> scalability_mode) const override;
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_ENCODER_FACTORY_H_
