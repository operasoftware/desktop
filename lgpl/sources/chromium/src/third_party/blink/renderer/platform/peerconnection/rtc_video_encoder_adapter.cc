// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_adapter.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "media/base/encoder_status.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/video/video_encoder_info.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

namespace blink {

namespace {

constexpr auto kRtpTicksPerSecond = 90000;
constexpr auto kUsPerRtpTick = 1000.0 * 1000.0 / kRtpTicksPerSecond;

}  // namespace

RTCVideoEncoderAdapter::RTCVideoEncoderAdapter(
    media::VideoCodecProfile profile,
    std::unique_ptr<media::VideoEncoder> encoder)
    : profile_(profile),
      encoder_(std::move(encoder)),
      encoder_implementation_name_("platform SW encoder"),
      // The Windows platform SW encoder may need to load a system library,
      // hence MayBlock(). This doesn't hurt since we wait for tasks to finish
      // on this runner anyway.
      encoder_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING, base::MayBlock()})) {
  DVLOG(1) << __func__ << " " << profile_;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

RTCVideoEncoderAdapter::~RTCVideoEncoderAdapter() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  encoder_task_runner_->DeleteSoon(FROM_HERE, std::move(encoder_));
}

int RTCVideoEncoderAdapter::InitEncode(const webrtc::VideoCodec* codec_settings,
                                       const VideoEncoder::Settings& settings) {
  DVLOG(1) << __func__ << " encoder_initialized_=" << encoder_initialized_;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  encoder_options_.frame_size = {codec_settings->width, codec_settings->height};

  if (codec_settings->startBitrate != 0) {
    encoder_options_.bitrate =
        media::Bitrate::ConstantBitrate(codec_settings->startBitrate);
  } else {
    encoder_options_.bitrate.reset();
  }

  if (codec_settings->maxFramerate != 0)
    encoder_options_.framerate = codec_settings->maxFramerate;
  else
    encoder_options_.framerate.reset();

  encoder_options_.avc.produce_annexb = true;
  encoder_options_.latency_mode = media::VideoEncoder::LatencyMode::Realtime;

  if (codec_settings->GetScalabilityMode().has_value()) {
    switch (codec_settings->GetScalabilityMode().value()) {
      case webrtc::ScalabilityMode::kL1T1:
        encoder_options_.scalability_mode = media::SVCScalabilityMode::kL1T1;
        break;
      case webrtc::ScalabilityMode::kL1T2:
        encoder_options_.scalability_mode = media::SVCScalabilityMode::kL1T2;
        break;
      case webrtc::ScalabilityMode::kL1T3:
        encoder_options_.scalability_mode = media::SVCScalabilityMode::kL1T3;
        break;
      default:
        DVLOG(1) << "Unsupported scalability mode: "
                 << webrtc::ScalabilityModeToString(
                        codec_settings->GetScalabilityMode().value());
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
  }

  auto info_callback = base::BindPostTaskToCurrentDefault(
      base::BindRepeating(&RTCVideoEncoderAdapter::OnEncoderInfoUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
auto output_callback = base::BindPostTaskToCurrentDefault(
      base::BindRepeating(&RTCVideoEncoderAdapter::OnEncodedFrameReady,
                          weak_ptr_factory_.GetWeakPtr()));

  const bool needs_initialization = !encoder_initialized_;
  encoder_initialized_ = false;
  const media::EncoderStatus result = RunOnEncoderTaskRunnerSync(
      needs_initialization
          ? base::BindOnce(&media::VideoEncoder::Initialize,
                           base::Unretained(encoder_.get()), profile_,
                           encoder_options_, std::move(info_callback),
                           std::move(output_callback))
          : base::BindOnce(&media::VideoEncoder::ChangeOptions,
                           base::Unretained(encoder_.get()), encoder_options_,
                           std::move(output_callback)));

  if (!result.is_ok())
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

  encoder_initialized_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoderAdapter::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  encoded_image_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoEncoderAdapter::Release() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_initialized_)
    return WEBRTC_VIDEO_CODEC_OK;

  const media::EncoderStatus result = RunOnEncoderTaskRunnerSync(base::BindOnce(
      &media::VideoEncoder::Flush, base::Unretained(encoder_.get())));

  return result.is_ok() ? WEBRTC_VIDEO_CODEC_OK
                        : WEBRTC_VIDEO_CODEC_ENCODER_FAILURE;
}

int32_t RTCVideoEncoderAdapter::Encode(
    const webrtc::VideoFrame& webrtc_frame,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DVLOG(3) << __func__
           << " webrtc_frame.timestamp()=" << webrtc_frame.timestamp();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  input_frames_.push_back(webrtc_frame);

  // Normally RTCVideoEncoderAdapter is used with
  // `webrtc::VideoFrameBuffer::Type::kNative` frames produced by
  // blink::WebRTCVideoTrackSource, except when a video track is disabled and
  // webrtc::VideoBroadcaster inserts non-native black frames.
  const scoped_refptr<media::VideoFrame> frame =
      webrtc_frame.video_frame_buffer()->type() ==
              webrtc::VideoFrameBuffer::Type::kNative
          ? static_cast<const WebRtcVideoFrameAdapter*>(
                webrtc_frame.video_frame_buffer().get())
                ->getMediaVideoFrame()
          : media::VideoFrame::CreateBlackFrame(
                {webrtc_frame.width(), webrtc_frame.height()});
  frame->set_timestamp(
      base::Microseconds(webrtc_frame.timestamp() * kUsPerRtpTick));

  const bool is_key_frame =
      frame_types && !frame_types->empty() &&
      frame_types->front() == webrtc::VideoFrameType::kVideoFrameKey;

  const media::EncoderStatus result = RunOnEncoderTaskRunnerSync(base::BindOnce(
      &media::VideoEncoder::Encode, base::Unretained(encoder_.get()),
      std::move(frame), media::VideoEncoder::EncodeOptions(is_key_frame)));

  return result.is_ok() ? WEBRTC_VIDEO_CODEC_OK
                        : WEBRTC_VIDEO_CODEC_ENCODER_FAILURE;
}

void RTCVideoEncoderAdapter::SetRates(const RateControlParameters& parameters) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!encoder_initialized_) {
    DVLOG(1) << "Encoder not initialized";
    return;
  }

  if (parameters.bitrate.get_sum_bps() != 0) {
    encoder_options_.bitrate =
        media::Bitrate::ConstantBitrate(parameters.bitrate.get_sum_bps());
  } else {
    encoder_options_.bitrate.reset();
  }

  if (parameters.framerate_fps != 0)
    encoder_options_.framerate = parameters.framerate_fps;
  else
    encoder_options_.framerate.reset();

  media::EncoderStatus result = RunOnEncoderTaskRunnerSync(base::BindOnce(
      &media::VideoEncoder::Flush, base::Unretained(encoder_.get())));
  if (result.is_ok()) {
    result = RunOnEncoderTaskRunnerSync(base::BindOnce(
        &media::VideoEncoder::ChangeOptions, base::Unretained(encoder_.get()),
        encoder_options_, base::NullCallback()));
  }

  DVLOG_IF(1, !result.is_ok())
      << "Failed to set encoder rates: " << static_cast<int>(result.code())
      << " " << result.message();
}

RTCVideoEncoderAdapter::EncoderInfo RTCVideoEncoderAdapter::GetEncoderInfo()
    const {
  DVLOG(1) << __func__;
  // This can be called on more than one task runner.

  EncoderInfo info;
  info.scaling_settings = ScalingSettings::kOff;
  info.requested_resolution_alignment = 2;
  info.supports_native_handle = true;
  info.implementation_name = encoder_implementation_name_;
  info.is_hardware_accelerated = false;
  info.supports_simulcast = false;
  info.preferred_pixel_formats = {webrtc::VideoFrameBuffer::Type::kNative};

  if (encoder_options_.scalability_mode.has_value()) {
    size_t num_temporal_layers = 1;
    switch (encoder_options_.scalability_mode.value()) {
      case media::SVCScalabilityMode::kL1T1:
        num_temporal_layers = 1;
        break;
      case media::SVCScalabilityMode::kL1T2:
        num_temporal_layers = 2;
        break;
      case media::SVCScalabilityMode::kL1T3:
        num_temporal_layers = 3;
        break;
      default:
        NOTREACHED() << "Unsupported SVC: "
                     << media::GetScalabilityModeName(
                            encoder_options_.scalability_mode.value());
    }
    // Assume there's just one spatial layer -- no way to communicate anything
    // else via media::VideoEncoder::Options anyway.
    constexpr size_t spatial_layer_index = 0u;
    const auto fps_allocation = media::GetFpsAllocation(num_temporal_layers);
    info.fps_allocation[spatial_layer_index] = {fps_allocation.begin(),
                                                fps_allocation.end()};
  }

  return info;
}

media::EncoderStatus RTCVideoEncoderAdapter::RunOnEncoderTaskRunnerSync(
    EncoderTask task,
    const base::Location& location) {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  base::WaitableEvent waiter;

  media::EncoderStatus result;
  encoder_task_runner_->PostTask(
      location,
      base::BindOnce(std::move(task), base::BindOnce(
                                          [](base::WaitableEvent* waiter,
                                             media::EncoderStatus* result,
                                             media::EncoderStatus status) {
                                            *result = status;
                                            waiter->Signal();
                                          },
                                          &waiter, &result)));
  waiter.Wait();

  return result;
}

void RTCVideoEncoderAdapter::OnEncoderInfoUpdated(
    const media::VideoEncoderInfo& encoder_info) {
  DVLOG(1) << __func__
           << " implementation_name=" << encoder_info.implementation_name;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  encoder_implementation_name_ = encoder_info.implementation_name;
}

void RTCVideoEncoderAdapter::OnEncodedFrameReady(
    media::VideoEncoderOutput output,
    absl::optional<
        media::VideoEncoder::CodecDescription> /*codec_description*/) {
  DVLOG(3) << __func__ << " output.timestamp=" << output.timestamp
           << " output.size=" << output.size;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(encoded_image_callback_)
      << "Expected RegisterEncodeCompleteCallback() first";

  auto* it = base::ranges::find(
      input_frames_, output.timestamp,
      [](const webrtc::VideoFrame& input_frame) {
        return base::Microseconds(input_frame.timestamp() * kUsPerRtpTick);
      });
  if (it == base::ranges::end(input_frames_)) {
    DVLOG(1) << __func__ << " could not match output timestamp "
             << output.timestamp << " to input frame";
    // This should not happen with a valid media::VideoEncoder implementation.
    // Dropping the encode instead of asserting because a system library can be
    // at fault.
    encoded_image_callback_->OnDroppedFrame(
        webrtc::EncodedImageCallback::DropReason::kDroppedByEncoder);
    return;
  }
  webrtc::VideoFrame input_frame = std::move(*it);
  input_frames_.erase(it);

  webrtc::EncodedImage encoded_image;
  encoded_image.SetTimestamp(input_frame.timestamp());
  encoded_image.SetEncodedData(
      webrtc::EncodedImageBuffer::Create(output.data.get(), output.size));
  encoded_image._encodedWidth = input_frame.width();
  encoded_image._encodedHeight = input_frame.height();
  encoded_image._frameType = output.key_frame
                                 ? webrtc::VideoFrameType::kVideoFrameKey
                                 : webrtc::VideoFrameType::kVideoFrameDelta;

  webrtc::CodecSpecificInfo codec_specific_info;
  codec_specific_info.codecType = webrtc::kVideoCodecH264;
  webrtc::CodecSpecificInfoH264& h264 = codec_specific_info.codecSpecific.H264;
  h264.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
  h264.idr_frame = output.key_frame;
  h264.temporal_idx = webrtc::kNoTemporalIdx;
  h264.base_layer_sync = false;

  encoded_image_callback_->OnEncodedImage(encoded_image, &codec_specific_info);
}

}  // namespace blink
