// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_decoder_factory.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/video_decoder.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_platform_sw_video_supported_formats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"

#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)

namespace blink {

namespace {

// Copied from rtc_video_decoder_factory.cc.
//
// The sole purpose is destroying the wrapped RTCVideoDecoderAdapter adapter on
// the correct sequence. Without ScopedVideoDecoder, ~RCTVideoDecoderAdapter
// tries to destroy media::VideoDecoder and WeakPtrs on a sequence other than
// where these were created. This is really lame, but I'm not going to fight
// this.
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
    media::mojom::InterfaceFactory* media_interface_factory,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const gfx::ColorSpace& render_color_space)
    : media_interface_factory_(media_interface_factory),
      media_task_runner_(std::move(media_task_runner)),
      render_color_space_(render_color_space) {}

RTCPlatformSWVideoDecoderFactory::~RTCPlatformSWVideoDecoderFactory() = default;

std::unique_ptr<webrtc::VideoDecoder> RTCPlatformSWVideoDecoderFactory::Create(
    const webrtc::Environment& env,
    const webrtc::SdpVideoFormat& format) {
  DVLOG(2) << __func__;

  std::unique_ptr<media::VideoDecoder> platform_decoder;
#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
  mojo::PendingRemote<media::mojom::VideoDecoder> video_decoder_remote;
  media_interface_factory_->CreateVideoDecoder(
      video_decoder_remote.InitWithNewPipeAndPassReceiver(),
      /*dst_video_decoder=*/{});

  platform_decoder = std::make_unique<media::MojoVideoDecoder>(
      media_task_runner_, /*gpu_factories=*/nullptr, &media_log_,
      std::move(video_decoder_remote),
      /*request_overlay_info_cb=*/base::DoNothing(), render_color_space_);
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)

  if (platform_decoder) {
    if (auto adapter = RTCVideoDecoderAdapter::Create(
            std::move(platform_decoder), media_task_runner_, format)) {
      return std::make_unique<ScopedVideoDecoder>(
          base::SequencedTaskRunner::GetCurrentDefault(), std::move(adapter));
    }
  }

  return nullptr;
}

std::vector<webrtc::SdpVideoFormat>
RTCPlatformSWVideoDecoderFactory::GetSupportedFormats() const {
  return GetPlatformSWCodecSupportedFormats(/*encoder=*/false);
}

}  // namespace blink
