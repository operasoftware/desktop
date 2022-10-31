// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_decoder_factory.h"

#include <memory>
#include <utility>

#include "base/features/feature_utils.h"
#include "base/features/submodule_features.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/video_decoder.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_supported_formats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
#include "media/filters/vt_video_decoder.h"
#elif BUILDFLAG(IS_WIN)
#include "media/filters/wmf_video_decoder.h"
#endif
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

namespace blink {

namespace {

// Copied from rtc_video_decoder_factory.cc.
//
// The sole purpose is destroying the wrapped RTCVideoDecoderAdapter adapter on
// the correct sequence. Without ScopedVideoDecoder, ~RCTVideoDecoderAdapter
// tries to destroy media::VideoDecoder and WeakPtrs on a sequence other than
// where these were created (the media thread). This is really lame, but I'm
// not going to fight this.
class ScopedVideoDecoder : public webrtc::VideoDecoder {
 public:
  ScopedVideoDecoder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     std::unique_ptr<webrtc::VideoDecoder> decoder)
      : task_runner_(std::move(task_runner)), decoder_(std::move(decoder)) {}

  ~ScopedVideoDecoder() override {
    task_runner_->DeleteSoon(FROM_HERE, std::move(decoder_));
  }

  // webrtc::VideoDecoder:
  bool Configure(const Settings& settings) override {
    return decoder_->Configure(settings);
  }
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    return decoder_->RegisterDecodeCompleteCallback(callback);
  }
  int32_t Release() override { return decoder_->Release(); }
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    return decoder_->Decode(input_image, missing_frames, render_time_ms);
  }
  DecoderInfo GetDecoderInfo() const override {
    return decoder_->GetDecoderInfo();
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<webrtc::VideoDecoder> decoder_;
};

}  // namespace

RTCPlatformSWVideoDecoderFactory::RTCPlatformSWVideoDecoderFactory(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)) {}

RTCPlatformSWVideoDecoderFactory::~RTCPlatformSWVideoDecoderFactory() = default;

std::unique_ptr<webrtc::VideoDecoder>
RTCPlatformSWVideoDecoderFactory::CreateVideoDecoder(
    const webrtc::SdpVideoFormat& format) {
  DVLOG(2) << __func__;

  std::unique_ptr<media::VideoDecoder> platform_decoder;
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
  if (base::IsFeatureEnabled(
          base::kFeaturePlatformSWH264EncoderDecoderWebRTCMac)) {
    platform_decoder =
        std::make_unique<media::VTVideoDecoder>(media_task_runner_);
  }
#elif BUILDFLAG(IS_WIN)
  if (base::IsFeatureEnabled(
          base::kFeaturePlatformSWH264EncoderDecoderWebRTCWin)) {
    platform_decoder =
        std::make_unique<media::WMFVideoDecoder>(media_task_runner_);
  }
#endif
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

  if (platform_decoder) {
    if (auto adapter = RTCVideoDecoderAdapter::Create(
            std::move(platform_decoder), media_task_runner_, format)) {
      return std::make_unique<ScopedVideoDecoder>(media_task_runner_,
                                                  std::move(adapter));
    }
  }

  return nullptr;
}

std::vector<webrtc::SdpVideoFormat>
RTCPlatformSWVideoDecoderFactory::GetSupportedFormats() const {
  return GetPlatformSWCodecSupportedFormats(/*encoder=*/false);
}

}  // namespace blink
