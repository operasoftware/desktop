// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_PLATFORM_VIDEO_ENCODER_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_PLATFORM_VIDEO_ENCODER_ADAPTER_H_

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

namespace blink {

// An adapter from media::VideoEncoder to blink::VideoTrackRecorder::Encoder.
// It allows us to re-use our platform-based video encoders in MediaRecorder.
class MODULES_EXPORT PlatformVideoEncoderAdapter final
    : public VideoTrackRecorder::Encoder {
 public:
  PlatformVideoEncoderAdapter(
      std::unique_ptr<media::VideoEncoder> encoder,
      VideoTrackRecorder::CodecProfile codec_profile,
      const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
      const VideoTrackRecorder::OnErrorCB& on_error_cb,
      uint32_t bits_per_second,
      const gfx::Size& frame_size);

  PlatformVideoEncoderAdapter(const PlatformVideoEncoderAdapter&) = delete;
  PlatformVideoEncoderAdapter& operator=(const PlatformVideoEncoderAdapter&) =
      delete;

  ~PlatformVideoEncoderAdapter() override;

  base::WeakPtr<Encoder> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  // A frame that hasn't been (fully) encoded yet.
  struct PendingFrame {
    scoped_refptr<media::VideoFrame> frame;
    base::TimeTicks capture_timestamp;
  };

  // VideoTrackRecorder::Encoder implementation:
  void EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                   base::TimeTicks capture_timestamp) override;

  // Initializes or reinitializes the encoder. The latter happens on frame size
  // change.
  void InitializeEncoder(const gfx::Size& frame_size);

  void MaybeEncodePendingFrame();

  void OnEncodeOutputReady(
      media::VideoEncoderOutput output,
      absl::optional<media::VideoEncoder::CodecDescription>);

  // Used as the completion callback for media::VideoEncoder calls. If the
  // encoder task was successful, runs `next_task`.
  void OnEncoderTaskComplete(base::OnceClosure next_task,
                             media::EncoderStatus status);

  // A helper that returns OnEncoderTaskComplete() as a callback.
  media::VideoEncoder::EncoderStatusCB OnSuccessRun(
      base::OnceClosure next_task);

  bool is_encoder_initialized() const { return frame_size_.has_value(); }
  void set_configure_done() { configuring_ = false; }

  const media::VideoCodecProfile profile_;
  const VideoTrackRecorder::OnErrorCB on_error_cb_;
  std::unique_ptr<media::VideoEncoder> encoder_;

  // True while we are initializing or reinitializing `encoder_`. Allows us to
  // know when it's okay to take frames to encode from the `pending_frames_`
  // queue.
  bool configuring_ = false;

  // Frames that haven't started encoding yet.
  base::queue<PendingFrame> pending_frames_;

  // Frames handed over to `encoder_` that are still being encoded.
  base::queue<PendingFrame> frames_in_encoder_;

  // The last frame size configured in `encoder_`, if any.
  absl::optional<gfx::Size> frame_size_;

  base::WeakPtrFactory<PlatformVideoEncoderAdapter> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_PLATFORM_VIDEO_ENCODER_ADAPTER_H_
