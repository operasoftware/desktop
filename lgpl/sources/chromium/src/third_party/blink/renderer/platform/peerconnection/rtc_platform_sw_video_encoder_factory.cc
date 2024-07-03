// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_encoder_factory.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "media/base/video_encoder.h"
#include "media/media_buildflags.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_supported_formats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_adapter.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"

#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
#include "media/video/vt_video_encoder.h"
#elif BUILDFLAG(IS_WIN)
#include "media/video/wmf_video_encoder.h"
#endif
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)

namespace blink {

namespace {

bool IsScalabiltiyModeSupported(
    const std::string& scalability_mode,
    const absl::InlinedVector<webrtc::ScalabilityMode,
                              webrtc::kScalabilityModeCount>&
        supported_scalability_modes) {
  return base::ranges::find(supported_scalability_modes, scalability_mode,
                            [](webrtc::ScalabilityMode mode) {
                              return webrtc::ScalabilityModeToString(mode);
                            }) != supported_scalability_modes.end();
}

}  // namespace

std::vector<webrtc::SdpVideoFormat>
RTCPlatformSWVideoEncoderFactory::GetSupportedFormats() const {
  return GetPlatformSWCodecSupportedFormats(/*encoder=*/true);
}

// Must implement, because in the default QueryCodecSupport() implementation a
// non-null `scalability_mode` implies "unsupported".
webrtc::VideoEncoderFactory::CodecSupport
RTCPlatformSWVideoEncoderFactory::QueryCodecSupport(
    const webrtc::SdpVideoFormat& format,
    absl::optional<std::string> scalability_mode) const {
  for (const auto& supported_format : GetSupportedFormats()) {
    if (format.IsSameCodec(supported_format)) {
      if (!scalability_mode ||
          IsScalabiltiyModeSupported(*scalability_mode,
                                     supported_format.scalability_modes)) {
        return {/*is_supported=*/true, /*is_power_efficient=*/false};
      }
      break;
    }
  }
  return {/*is_supported=*/false, /*is_power_efficient=*/false};
}

std::unique_ptr<webrtc::VideoEncoder> RTCPlatformSWVideoEncoderFactory::Create(
    const webrtc::Environment& env,
    const webrtc::SdpVideoFormat& format) {
#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
  auto encoder = std::make_unique<media::VTVideoEncoder>();
#elif BUILDFLAG(IS_WIN)
  auto encoder = std::make_unique<media::WMFVideoEncoder>();
#endif
  return std::make_unique<RTCVideoEncoderAdapter>(
      WebRtcVideoFormatToMediaVideoCodecProfile(format), std::move(encoder));
#else
  return nullptr;
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
}

}  // namespace blink
