// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_ADAPTER_H_

#include <memory>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"

namespace webrtc {
class VideoFrame;
}  // namespace webrtc

namespace blink {

// Wraps a media::VideoEncoder and adapts it to the webrtc::VideoEncoder
// interface so it can be re-used for WebRTC.
class PLATFORM_EXPORT RTCVideoEncoderAdapter final
    : public webrtc::VideoEncoder {
 public:
  RTCVideoEncoderAdapter(media::VideoCodecProfile profile,
                         std::unique_ptr<media::VideoEncoder> encoder);
  ~RTCVideoEncoderAdapter() override;

  RTCVideoEncoderAdapter(const RTCVideoEncoderAdapter&) = delete;
  RTCVideoEncoderAdapter& operator=(const RTCVideoEncoderAdapter&) = delete;

  // webrtc::VideoEncoder:
  int InitEncode(const webrtc::VideoCodec* codec_settings,
                 const VideoEncoder::Settings& settings) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t Encode(
      const webrtc::VideoFrame& frame,
      const std::vector<webrtc::VideoFrameType>* frame_types) override;
  void SetRates(const RateControlParameters& parameters) override;
  EncoderInfo GetEncoderInfo() const override;

 private:
  using EncoderTask =
      base::OnceCallback<void(media::VideoEncoder::EncoderStatusCB done_cb)>;

  // Every call to `encoder_` goes through this helper. The purpose is to adapt
  // the asynchronous media::VideoEncoder API to the synchronous
  // webrtc::VideoEncoder API.
  media::EncoderStatus RunOnEncoderTaskRunnerSync(
      EncoderTask task,
      const base::Location& location = FROM_HERE);

  void OnEncodedFrameReady(
      media::VideoEncoderOutput output,
      absl::optional<media::VideoEncoder::CodecDescription> codec_description);

  const media::VideoCodecProfile profile_;

  // Performs the actual video encoding.
  std::unique_ptr<media::VideoEncoder> encoder_;
  bool encoder_initialized_ = false;

  // Where all media::VideoEncoder API calls happen.
  const scoped_refptr<base::SequencedTaskRunner> encoder_task_runner_;

  media::VideoEncoder::Options encoder_options_;

  WTF::Vector<webrtc::VideoFrame> input_frames_;

  // The sink for encoded video data.
  base::raw_ptr<webrtc::EncodedImageCallback> encoded_image_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RTCVideoEncoderAdapter> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_ADAPTER_H_
