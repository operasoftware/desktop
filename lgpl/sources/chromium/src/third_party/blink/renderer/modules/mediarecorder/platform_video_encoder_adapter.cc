// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/modules/mediarecorder/platform_video_encoder_adapter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/bitrate.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/muxers/webm_muxer.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// Common VideoFrame pixel formats use 12 bits per pixel. Thus, at the typical
// frame rate of 30 each pixel produces a bitrate of (12 bits * 30 1/s) = 360
// bps. Let's assume a 1/180 compression ratio. A 1280x720@30 video will have a
// bitrate of ~1.8 Mbps.
constexpr auto kDefaultBitratePerPixel = 2;

}  // namespace

PlatformVideoEncoderAdapter::PlatformVideoEncoderAdapter(
    std::unique_ptr<media::VideoEncoder> encoder,
    VideoTrackRecorder::CodecProfile codec_profile,
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
    const VideoTrackRecorder::OnErrorCB& error_callback,
    uint32_t bits_per_second,
    const gfx::Size& frame_size,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner)
    : Encoder(on_encoded_video_cb,
              bits_per_second > 0
                  ? bits_per_second
                  : frame_size.GetArea() * kDefaultBitratePerPixel,
              std::move(main_task_runner)),
      profile_(codec_profile.profile.value_or(media::H264PROFILE_BASELINE)),
      on_error_cb_(error_callback),
      encoder_(std::move(encoder)) {
  DVLOG(1) << __func__;
  DCHECK_EQ(codec_profile.codec_id, VideoTrackRecorder::CodecId::kH264);
}

PlatformVideoEncoderAdapter::~PlatformVideoEncoderAdapter() = default;

void PlatformVideoEncoderAdapter::InitializeEncoder(const gfx::Size& frame_size) {
  DVLOG(1) << __func__ << " frame_size=" << frame_size.ToString()
           << " bits_per_second_=" << bits_per_second_;
  DCHECK(encoding_task_runner_->RunsTasksInCurrentSequence());

  const bool was_initialized = is_encoder_initialized();
  frame_size_ = frame_size;
  configuring_ = true;

  media::VideoEncoder::Options options;
  options.frame_size = frame_size_.value();
  options.avc.produce_annexb = true;
  options.bitrate = media::Bitrate::ConstantBitrate(bits_per_second_);
  options.keyframe_interval = 100;

  auto output_cb = base::BindRepeating(
      &PlatformVideoEncoderAdapter::OnEncodeOutputReady, this);

  if (!was_initialized) {
    encoder_->Initialize(
        profile_, options, std::move(output_cb),
        OnSuccessRun(
            base::BindOnce(&PlatformVideoEncoderAdapter::set_configure_done,
                           this)
                .Then(base::BindOnce(
                    &PlatformVideoEncoderAdapter::MaybeEncodePendingFrame,
                    this))));
  } else {
    // Unretained(encoder_.get()) is safe, because this runs synchronously from
    // the callback returned by OnSuccessRun(), which is safely bound to
    // refcounted `this`.
    encoder_->Flush(OnSuccessRun(base::BindOnce(
        &media::VideoEncoder::ChangeOptions, base::Unretained(encoder_.get()),
        options, std::move(output_cb),
        OnSuccessRun(
            base::BindOnce(&PlatformVideoEncoderAdapter::set_configure_done,
                           this)
                .Then(base::BindOnce(
                    &PlatformVideoEncoderAdapter::MaybeEncodePendingFrame,
                    this))))));
  }
}

void PlatformVideoEncoderAdapter::EncodeOnEncodingTaskRunner(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks capture_timestamp) {
  DVLOG(3) << __func__ << " " << frame->AsHumanReadableString();
  DCHECK(encoding_task_runner_->RunsTasksInCurrentSequence());

  if (!frame) {
    DVLOG(1) << "No frame";
    on_error_cb_.Run();
    return;
  }

  pending_frames_.push({std::move(frame), capture_timestamp});

  MaybeEncodePendingFrame();
}

void PlatformVideoEncoderAdapter::MaybeEncodePendingFrame() {
  DVLOG(3) << __func__ << " pending_frames_.size()=" << pending_frames_.size();
  DCHECK(encoding_task_runner_->RunsTasksInCurrentSequence());

  if (configuring_ || pending_frames_.empty())
    return;

  auto pending_frame = pending_frames_.front();
  if (!is_encoder_initialized() ||
      pending_frame.frame->visible_rect().size() != frame_size_) {
    InitializeEncoder(pending_frame.frame->visible_rect().size());
    return;
  }

  pending_frames_.pop();

  encoder_->Encode(
      pending_frame.frame, /*key_frame=*/false,
      OnSuccessRun(base::BindOnce(
          &PlatformVideoEncoderAdapter::MaybeEncodePendingFrame, this)));

  frames_in_encoder_.push(std::move(pending_frame));
}

void PlatformVideoEncoderAdapter::OnEncodeOutputReady(
      media::VideoEncoderOutput output,
      absl::optional<media::VideoEncoder::CodecDescription>) {
  DVLOG(3) << __func__;
  DCHECK(encoding_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!frames_in_encoder_.empty());

  auto input_frame = std::move(frames_in_encoder_.front());
  frames_in_encoder_.pop();

  std::string data(output.data.get(), output.data.get() + output.size);

  PostCrossThreadTask(
      *origin_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          &OnFrameEncodeCompleted,
          CrossThreadBindRepeating(on_encoded_video_cb_),
          media::WebmMuxer::VideoParameters(std::move(input_frame.frame)),
          std::move(data), /*alpha_data=*/std::string(),
          input_frame.capture_timestamp, output.key_frame));
}

media::VideoEncoder::EncoderStatusCB PlatformVideoEncoderAdapter::OnSuccessRun(
    base::OnceClosure next_task) {
  DCHECK(encoding_task_runner_->RunsTasksInCurrentSequence());
  return base::BindOnce(&PlatformVideoEncoderAdapter::OnEncoderTaskComplete,
                        this, std::move(next_task));
}

void PlatformVideoEncoderAdapter::OnEncoderTaskComplete(
    base::OnceClosure next_task,
    media::EncoderStatus status) {
  DVLOG(3) << __func__ << " status.code()=" << static_cast<int>(status.code());
  DCHECK(encoding_task_runner_->RunsTasksInCurrentSequence());

  if (status.is_ok())
    std::move(next_task).Run();
  else
    on_error_cb_.Run();
}

}  // namespace blink
