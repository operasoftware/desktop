// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_supported_formats.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/video_codecs.h"
#include "third_party/webrtc/api/video_codecs/h264_profile_level_id.h"
#include "third_party/webrtc/media/base/media_constants.h"

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#include "media/base/platform_mime_util.h"
#if BUILDFLAG(IS_MAC)
#include "media/video/vt_video_encoder.h"
#endif  // BUILDFLAG(IS_MAC)
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

namespace blink {

namespace {

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
struct VideoCodecProfileAndWebRTCProfile {
  webrtc::H264Profile webrtc_profile;
  media::VideoCodecProfile media_profile;
};

constexpr VideoCodecProfileAndWebRTCProfile kSupportedProfiles[] = {
    {webrtc::H264Profile::kProfileBaseline, media::H264PROFILE_BASELINE},
    {webrtc::H264Profile::kProfileConstrainedBaseline,
     media::H264PROFILE_BASELINE},
    {webrtc::H264Profile::kProfileMain, media::H264PROFILE_MAIN},
    {webrtc::H264Profile::kProfileHigh, media::H264PROFILE_HIGH},
    {webrtc::H264Profile::kProfileConstrainedHigh, media::H264PROFILE_HIGH},
};

#if BUILDFLAG(IS_WIN)
constexpr webrtc::ScalabilityMode kSupportedScalabilityModes[] = {
    webrtc::ScalabilityMode::kL1T1,
    webrtc::ScalabilityMode::kL1T2,
    webrtc::ScalabilityMode::kL1T3,
};
#elif BUILDFLAG(IS_MAC)
constexpr webrtc::ScalabilityMode kSupportedScalabilityModes[] = {
    webrtc::ScalabilityMode::kL1T1,
};
#endif  // BUILDFLAG(IS_WIN)

webrtc::SdpVideoFormat H264ProfileToWebRtcFormat(
    webrtc::H264Profile profile,
    const std::string& packetization_mode) {
  // Level 5.1 is the maximum level supported by Media Foundation
  // (https://docs.microsoft.com/en-us/windows/win32/medfound/h-264-video-decoder).
  // Video Toolbox is likely not worse than that.
  //
  // For better compatibility, let's step this down to 3.1, which is the value
  // provided by Chromium when OpenH264 is used. In theory, some sites might
  // not try to use the higher levels even though they could, which isn't
  // ideal. On the other hand, the important thing is we don't provide a
  // _lower_ value than Chrome. And presently (08.2022) there is at least one
  // prominent site (Messenger) that specifically looks for level 3.1 and
  // chooses VP8 over H.264 if the UA says it supports a higher level.
  const webrtc::H264ProfileLevelId profile_level_id(
      profile, webrtc::H264Level::kLevel3_1);

  const absl::InlinedVector<webrtc::ScalabilityMode,
                            webrtc::kScalabilityModeCount>
      scalability_modes(std::begin(kSupportedScalabilityModes),
                        std::end(kSupportedScalabilityModes));
  webrtc::SdpVideoFormat format(
      cricket::kH264CodecName,
      {{cricket::kH264FmtpProfileLevelId,
        *webrtc::H264ProfileLevelIdToString(profile_level_id)},
       {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
       {cricket::kH264FmtpPacketizationMode, packetization_mode}},
       scalability_modes);
  return format;
}
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

}  // namespace

std::vector<webrtc::SdpVideoFormat> GetPlatformSWCodecSupportedFormats(
    bool encoder) {
  DVLOG(3) << __func__ << " encoder=" << encoder;
  std::vector<webrtc::SdpVideoFormat> supported_formats;
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  const bool has_platform_support =
      encoder ? media::platform_mime_util::IsPlatformVideoEncoderAvailable()
              : media::platform_mime_util::IsPlatformVideoDecoderAvailable();
  if (has_platform_support) {
    for (const char* packetization_mode : {"0", "1"}) {
      for (const auto profile : kSupportedProfiles) {
#if BUILDFLAG(IS_MAC)
        if (encoder && !media::VTVideoEncoder::IsNoDelayEncodingSupported(
                           profile.media_profile)) {
          continue;
        }
#endif
        const auto format = H264ProfileToWebRtcFormat(profile.webrtc_profile,
                                                      packetization_mode);
        DVLOG(3) << "  " << media::GetProfileName(profile.media_profile) << " ("
                 << format.ToString() << ")";
        supported_formats.push_back(std::move(format));
      }
    }
  }
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)
  return supported_formats;
}

}  // namespace blink
