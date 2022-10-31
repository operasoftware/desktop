// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_encoder_factory.h"

#include <utility>

#include "base/features/feature_utils.h"
#include "base/features/submodule_features.h"
#include "media/base/video_encoder.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_supported_formats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_adapter.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
#include "media/video/vt_video_encoder.h"
#elif BUILDFLAG(IS_WIN)
#include "media/video/wmf_video_encoder.h"
#endif
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

namespace blink {

std::vector<webrtc::SdpVideoFormat>
RTCPlatformSWVideoEncoderFactory::GetSupportedFormats() const {
  return GetPlatformSWCodecSupportedFormats(/*encoder=*/true);
}

std::unique_ptr<webrtc::VideoEncoder>
RTCPlatformSWVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format) {
  std::unique_ptr<media::VideoEncoder> encoder;
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
  if (base::IsFeatureEnabled(
          base::kFeaturePlatformSWH264EncoderDecoderWebRTCMac)) {
    encoder = std::make_unique<media::VTVideoEncoder>();
  }
#elif BUILDFLAG(IS_WIN)
  if (base::IsFeatureEnabled(
          base::kFeaturePlatformSWH264EncoderDecoderWebRTCWin)) {
    encoder = std::make_unique<media::WMFVideoEncoder>();
  }
#endif
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)
  if (!encoder)
    return nullptr;

  return std::make_unique<RTCVideoEncoderAdapter>(
      WebRtcVideoFormatToMediaVideoCodecProfile(format), std::move(encoder));
}

}  // namespace blink
