// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_SUPPORTED_FORMATS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_SUPPORTED_FORMATS_H_

#include <vector>

namespace webrtc {
struct SdpVideoFormat;
}  // namespace webrtc

namespace blink {

std::vector<webrtc::SdpVideoFormat> GetPlatformSWCodecSupportedFormats(
    bool encoder);

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_PLATFORM_SW_VIDEO_SUPPORTED_FORMATS_H_
