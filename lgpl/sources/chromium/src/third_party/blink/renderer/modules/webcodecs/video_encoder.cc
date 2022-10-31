// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/cxx17_backports.h"
#include "base/format_macros.h"
#include "base/features/feature_utils.h"
#include "base/features/submodule_features.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/clamped_math.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/async_destroy_video_encoder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/mime_util.h"
#include "media/base/offloading_video_encoder.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#include "media/base/video_util.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/h264_level_limits.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "media/video/video_encoder_fallback.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_avc_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_svc_output_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_space_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/gpu_factories_retriever.h"
#include "third_party/blink/renderer/modules/webcodecs/video_color_space.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/openh264/openh264_buildflags.h"

#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_OPENH264)
#include "media/video/openh264_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/video/vpx_video_encoder.h"
#endif

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
#include "media/video/vt_video_encoder.h"
#elif BUILDFLAG(IS_WIN)
#include "media/video/wmf_video_encoder.h"
#endif
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

namespace WTF {

template <>
struct CrossThreadCopier<media::EncoderStatus>
    : public CrossThreadCopierPassThrough<media::EncoderStatus> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

constexpr const char kCategory[] = "media";
constexpr int kMaxActiveEncodes = 5;

// Use this function in cases when we can't immediately delete |ptr| because
// there might be its methods on the call stack.
template <typename T>
void DeleteLater(ScriptState* state, std::unique_ptr<T> ptr) {
  DCHECK(state->ContextIsValid());
  auto* context = ExecutionContext::From(state);
  auto runner = context->GetTaskRunner(TaskType::kInternalDefault);
  runner->DeleteSoon(FROM_HERE, std::move(ptr));
}

bool IsAcceleratedConfigurationSupported(
    media::VideoCodecProfile profile,
    const media::VideoEncoder::Options& options,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  if (!gpu_factories || !gpu_factories->IsGpuVideoEncodeAcceleratorEnabled())
    return false;

  auto supported_profiles =
      gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
          media::VideoEncodeAccelerator::SupportedProfiles());

  bool found_supported_profile = false;
  for (auto& supported_profile : supported_profiles) {
    if (supported_profile.profile != profile)
      continue;

    if (supported_profile.min_resolution.width() > options.frame_size.width() ||
        supported_profile.min_resolution.height() >
            options.frame_size.height()) {
      continue;
    }

    if (supported_profile.max_resolution.width() < options.frame_size.width() ||
        supported_profile.max_resolution.height() <
            options.frame_size.height()) {
      continue;
    }

    double max_supported_framerate =
        static_cast<double>(supported_profile.max_framerate_numerator) /
        supported_profile.max_framerate_denominator;
    if (options.framerate.has_value() &&
        options.framerate.value() > max_supported_framerate) {
      continue;
    }

    if (options.scalability_mode.has_value() &&
        !base::Contains(supported_profile.scalability_modes,
                        options.scalability_mode.value())) {
      continue;
    }

    found_supported_profile = true;
    break;
  }
  return found_supported_profile;
}

VideoEncoderTraits::ParsedConfig* ParseConfigStatic(
    const VideoEncoderConfig* config,
    ExceptionState& exception_state) {
  constexpr int kMaxSupportedFrameSize = 8192;
  auto* result = MakeGarbageCollected<VideoEncoderTraits::ParsedConfig>();

  result->options.frame_size.set_height(config->height());
  if (config->height() == 0 || config->height() > kMaxSupportedFrameSize) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid height; expected range from %d to %d, received %d.", 1,
        kMaxSupportedFrameSize, config->height()));
    return nullptr;
  }

  result->options.frame_size.set_width(config->width());
  if (config->width() == 0 || config->width() > kMaxSupportedFrameSize) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid width; expected range from %d to %d, received %d.", 1,
        kMaxSupportedFrameSize, config->width()));
    return nullptr;
  }

  if (config->alpha() == "keep") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Alpha encoding is not currently supported.");
    return nullptr;
  }

  result->options.latency_mode =
      (config->latencyMode() == "quality")
          ? media::VideoEncoder::LatencyMode::Quality
          : media::VideoEncoder::LatencyMode::Realtime;

  if (config->hasBitrate()) {
    uint32_t bps = base::saturated_cast<uint32_t>(config->bitrate());
    if (bps == 0) {
      exception_state.ThrowTypeError("Zero is not a valid bitrate.");
      return nullptr;
    }
    if (config->hasBitrateMode() && config->bitrateMode() == "constant") {
      result->options.bitrate = media::Bitrate::ConstantBitrate(bps);
    } else {
      // VBR in media:Bitrate supports both target and peak bitrate.
      // Currently webcodecs doesn't expose peak bitrate
      // (assuming unconstrained VBR), here we just set peak as 10 times
      // target as a good enough way of expressing unconstrained VBR.
      result->options.bitrate = media::Bitrate::VariableBitrate(
          bps, base::ClampMul(bps, 10u).RawValue());
    }
  }

  if (config->hasDisplayWidth() && config->hasDisplayHeight()) {
    result->display_size.emplace(config->displayWidth(),
                                 config->displayHeight());
  }

  if (config->hasFramerate()) {
    constexpr double kMinFramerate = .0001;
    constexpr double kMaxFramerate = 1'000'000'000;
    if (std::isnan(config->framerate()) ||
        config->framerate() < kMinFramerate ||
        config->framerate() > kMaxFramerate) {
      exception_state.ThrowTypeError(String::Format(
          "Invalid framerate; expected range from %f to %f, received %f.",
          kMinFramerate, kMaxFramerate, config->framerate()));
      return nullptr;
    }
    result->options.framerate = config->framerate();
  } else {
    result->options.framerate =
        media::VideoEncodeAccelerator::kDefaultFramerate;
  }

  // https://w3c.github.io/webrtc-svc/
  if (config->hasScalabilityMode()) {
    if (config->scalabilityMode() == "L1T2") {
      result->options.scalability_mode = media::SVCScalabilityMode::kL1T2;
    } else if (config->scalabilityMode() == "L1T3") {
      result->options.scalability_mode = media::SVCScalabilityMode::kL1T3;
    } else {
      exception_state.ThrowTypeError("Unsupported scalabilityMode.");
      return nullptr;
    }
  }

  // The IDL defines a default value of "no-preference".
  DCHECK(config->hasHardwareAcceleration());

  result->hw_pref = StringToHardwarePreference(
      IDLEnumAsString(config->hardwareAcceleration()));

  bool is_codec_ambiguous = true;
  result->codec = media::VideoCodec::kUnknown;
  result->profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
  result->level = 0;
  result->codec_string = config->codec();

  // Some codec strings provide color space info, but for WebCodecs this is
  // ignored. Instead, the VideoFrames given to encode() are the source of truth
  // for input color space. Note also that the output color space is up to the
  // underlying codec impl. See https://github.com/w3c/webcodecs/issues/345.
  media::VideoColorSpace codec_string_color_space;

  bool parse_succeeded = media::ParseVideoCodecString(
      "", config->codec().Utf8(), &is_codec_ambiguous, &result->codec,
      &result->profile, &result->level, &codec_string_color_space);

  if (!parse_succeeded || is_codec_ambiguous) {
    exception_state.ThrowTypeError("Unknown codec.");
    return nullptr;
  }

  // We are done with the parsing.
  if (!config->hasAvc())
    return result;

  // We should only get here with H264 codecs.
  if (result->codec != media::VideoCodec::kH264) {
    exception_state.ThrowTypeError(
        "'avc' field can only be used with AVC codecs");
    return nullptr;
  }

  std::string avc_format = IDLEnumAsString(config->avc()->format()).Utf8();
  if (avc_format == "avc") {
    result->options.avc.produce_annexb = false;
  } else if (avc_format == "annexb") {
    result->options.avc.produce_annexb = true;
  } else {
    NOTREACHED();
  }

  return result;
}

bool VerifyCodecSupportStatic(VideoEncoderTraits::ParsedConfig* config,
                              ExceptionState* exception_state) {
  switch (config->codec) {
    case media::VideoCodec::kAV1:
      if (config->profile !=
          media::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN) {
        if (exception_state) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kNotSupportedError, "Unsupported av1 profile.");
        }
        return false;
      }
      break;

    case media::VideoCodec::kVP8:
      break;

    case media::VideoCodec::kVP9:
      if (config->profile == media::VideoCodecProfile::VP9PROFILE_PROFILE1 ||
          config->profile == media::VideoCodecProfile::VP9PROFILE_PROFILE3) {
        if (exception_state) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kNotSupportedError, "Unsupported vp9 profile.");
        }
        return false;
      }
      break;

    case media::VideoCodec::kH264: {
      if (config->options.frame_size.width() % 2 != 0 ||
          config->options.frame_size.height() % 2 != 0) {
        if (exception_state) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              "H264 only supports even sized frames.");
        }
        return false;
      }

      // Note: This calculation is incorrect for interlaced or MBAFF encoding;
      // but we don't support those and likely never will.
      gfx::Size coded_size(
          base::bits::AlignUp(config->options.frame_size.width(), 16),
          base::bits::AlignUp(config->options.frame_size.height(), 16));
      uint64_t coded_area = coded_size.Area64();
      uint64_t max_coded_area =
          media::H264LevelToMaxFS(config->level) * 16ull * 16ull;
      if (coded_area > max_coded_area) {
        if (exception_state) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              String::Format("The provided resolution (%s) has a coded area "
                             "(%d*%d=%" PRIu64
                             ") which exceeds the maximum coded area (%" PRIu64
                             ") supported by the AVC level (%1.1f) indicated "
                             "by the codec string (0x%02X). You must either "
                             "specify a lower resolution or higher AVC level.",
                             config->options.frame_size.ToString().c_str(),
                             coded_size.width(), coded_size.height(),
                             coded_area, max_coded_area, config->level / 10.0f,
                             config->level));
        }
        return false;
      }
      break;
    }

    default:
      if (exception_state) {
        exception_state->ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                           "Unsupported codec type.");
      }
      return false;
  }

  return true;
}

VideoEncoderConfig* CopyConfig(const VideoEncoderConfig& config) {
  auto* result = VideoEncoderConfig::Create();
  result->setCodec(config.codec());
  result->setWidth(config.width());
  result->setHeight(config.height());

  if (config.hasDisplayWidth())
    result->setDisplayWidth(config.displayWidth());

  if (config.hasDisplayHeight())
    result->setDisplayHeight(config.displayHeight());

  if (config.hasFramerate())
    result->setFramerate(config.framerate());

  if (config.hasBitrate())
    result->setBitrate(config.bitrate());

  if (config.hasScalabilityMode())
    result->setScalabilityMode(config.scalabilityMode());

  if (config.hasHardwareAcceleration())
    result->setHardwareAcceleration(config.hardwareAcceleration());

  if (config.hasAlpha())
    result->setAlpha(config.alpha());

  if (config.hasBitrateMode())
    result->setBitrateMode(config.bitrateMode());

  if (config.hasLatencyMode())
    result->setLatencyMode(config.latencyMode());

  if (config.hasAvc() && config.avc()->hasFormat()) {
    auto* avc = AvcEncoderConfig::Create();
    avc->setFormat(config.avc()->format());
    result->setAvc(avc);
  }

  return result;
}

bool CanUseGpuMemoryBufferReadback(media::VideoPixelFormat format,
                                   bool force_opaque) {
  // GMB readback only works with NV12, so only opaque buffers can be used.
  return (format == media::PIXEL_FORMAT_XBGR ||
          format == media::PIXEL_FORMAT_XRGB ||
          (force_opaque && (format == media::PIXEL_FORMAT_ABGR ||
                            format == media::PIXEL_FORMAT_ARGB))) &&
         WebGraphicsContext3DVideoFramePool::
             IsGpuMemoryBufferReadbackFromTextureEnabled();
}

}  // namespace

// static
const char* VideoEncoderTraits::GetName() {
  return "VideoEncoder";
}

// static
VideoEncoder* VideoEncoder::Create(ScriptState* script_state,
                                   const VideoEncoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<VideoEncoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

VideoEncoder::VideoEncoder(ScriptState* script_state,
                           const VideoEncoderInit* init,
                           ExceptionState& exception_state)
    : Base(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

VideoEncoder::~VideoEncoder() {
  if (codec_ == media::VideoCodec::kH264) {
    // Compare with ~EncoderBase().
    base::UmaHistogramSparse("Blink.WebCodecs.VideoEncoder.H264.FinalStatus",
                             static_cast<int>(logger_->status_code()));
  }
}

VideoEncoder::ParsedConfig* VideoEncoder::ParseConfig(
    const VideoEncoderConfig* config,
    ExceptionState& exception_state) {
  return ParseConfigStatic(config, exception_state);
}

bool VideoEncoder::VerifyCodecSupport(ParsedConfig* config,
                                      ExceptionState& exception_state) {
  return VerifyCodecSupportStatic(config, &exception_state);
}

void VideoEncoder::OnMediaEncoderCreated(std::string encoder_name,
                                         bool is_hw_accelerated) {
  if (is_hw_accelerated)
    ApplyCodecPressure();
  else
    ReleaseCodecPressure();

  // TODO(https://crbug.com/1139089) : Add encoder properties.
  media::MediaLog* log = logger_->log();

  log->SetProperty<media::MediaLogProperty::kVideoEncoderName>(encoder_name);
  log->SetProperty<media::MediaLogProperty::kIsPlatformVideoEncoder>(
      is_hw_accelerated);
}

std::unique_ptr<media::VideoEncoder>
VideoEncoder::CreateAcceleratedVideoEncoder(
    media::VideoCodecProfile profile,
    const media::VideoEncoder::Options& options,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  if (!IsAcceleratedConfigurationSupported(profile, options, gpu_factories))
    return nullptr;

  return std::make_unique<
      media::AsyncDestroyVideoEncoder<media::VideoEncodeAcceleratorAdapter>>(
      std::make_unique<media::VideoEncodeAcceleratorAdapter>(
          gpu_factories, logger_->log()->Clone(), callback_runner_));
}

std::unique_ptr<media::VideoEncoder> CreateAv1VideoEncoder() {
#if BUILDFLAG(ENABLE_LIBAOM)
  return std::make_unique<media::Av1VideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_LIBAOM)
}

std::unique_ptr<media::VideoEncoder> CreateVpxVideoEncoder() {
#if BUILDFLAG(ENABLE_LIBVPX)
  return std::make_unique<media::VpxVideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_LIBVPX)
}

std::unique_ptr<media::VideoEncoder> CreatePlatformH264VideoEncoder() {
  std::unique_ptr<media::VideoEncoder> encoder;
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#if BUILDFLAG(IS_MAC)
  if (base::IsFeatureEnabled(base::kFeaturePlatformSWH264EncoderWebCodecsMac))
    encoder = std::make_unique<media::VTVideoEncoder>();
#elif BUILDFLAG(IS_WIN)
  if (base::IsFeatureEnabled(base::kFeaturePlatformSWH264EncoderWebCodecsWin))
    encoder = std::make_unique<media::WMFVideoEncoder>();
#endif
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)
  return encoder;
}

std::unique_ptr<media::VideoEncoder>
VideoEncoder::CreateExternalH264VideoEncoder(
    media::VideoCodecProfile profile,
    const media::VideoEncoder::Options& options,
    media::GpuVideoAcceleratorFactories* external_software_factories) {
#if BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
  if (base::IsFeatureEnabled(base::kFeatureExternalOpenH264Encoder)) {
    // Not really accelerated, but using the VEA framework to encode (in SW)
    // in another process.
    return CreateAcceleratedVideoEncoder(profile, options,
                                         external_software_factories);
  }
#endif  // BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
  return nullptr;
}

std::unique_ptr<media::VideoEncoder> CreateOpenH264VideoEncoder() {
#if BUILDFLAG(ENABLE_OPENH264)
  return std::make_unique<media::OpenH264VideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_OPENH264)
}

// This method is static and takes |self| in order to make it possible to use it
// with a weak |this|. It's needed in to avoid a persistent reference cycle.
std::unique_ptr<media::VideoEncoder> VideoEncoder::CreateSoftwareVideoEncoder(
    VideoEncoder* self,
    media::VideoCodec codec,
    media::VideoCodecProfile profile,
    const media::VideoEncoder::Options& options,
    media::GpuVideoAcceleratorFactories* external_software_factories) {
  if (!self)
    return nullptr;
  std::unique_ptr<media::VideoEncoder> result;
  switch (codec) {
    case media::VideoCodec::kAV1:
      result = CreateAv1VideoEncoder();
      if (result)
        self->OnMediaEncoderCreated("Av1VideoEncoder", false);
      break;
    case media::VideoCodec::kVP8:
    case media::VideoCodec::kVP9:
      result = CreateVpxVideoEncoder();
      if (result)
        self->OnMediaEncoderCreated("VpxVideoEncoder", false);
      break;
    case media::VideoCodec::kH264:
      result = self->CreateExternalH264VideoEncoder(
          profile, options, external_software_factories);
      if (result) {
        self->OnMediaEncoderCreated("ExternalOpenH264VideoEncoder", false);
        break;
      }
      result = CreatePlatformH264VideoEncoder();
      if (result) {
        self->OnMediaEncoderCreated("PlatformVideoEncoder", false);
        break;
      }
      result = CreateOpenH264VideoEncoder();
      if (result)
        self->OnMediaEncoderCreated("OpenH264VideoEncoder", false);
      break;
    default:
      break;
  }
  if (!result)
    return nullptr;
  return std::make_unique<media::OffloadingVideoEncoder>(std::move(result));
}

std::unique_ptr<media::VideoEncoder> VideoEncoder::CreateMediaVideoEncoder(
    const ParsedConfig& config,
    media::GpuVideoAcceleratorFactories* external_software_factories,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  codec_ = config.codec;

  switch (config.hw_pref) {
    case HardwarePreference::kPreferHardware: {
      auto result = CreateAcceleratedVideoEncoder(
          config.profile, config.options, gpu_factories);
      if (result)
        OnMediaEncoderCreated("AcceleratedVideoEncoder", true);
      return result;
    }
    case HardwarePreference::kNoPreference:
      if (auto result = CreateAcceleratedVideoEncoder(
              config.profile, config.options, gpu_factories)) {
        OnMediaEncoderCreated("AcceleratedVideoEncoder", true);
        auto fallback_encoder = CreateSoftwareVideoEncoder(
            this, config.codec, config.profile, config.options,
            external_software_factories);
        return std::make_unique<media::VideoEncoderFallback>(
            std::move(result),
            ConvertToBaseOnceCallback(CrossThreadBindOnce(
                [](std::unique_ptr<media::VideoEncoder> encoder) {
                  return encoder;
                },
                std::move(fallback_encoder))));
      }
      [[fallthrough]];
    case HardwarePreference::kPreferSoftware:
      return CreateSoftwareVideoEncoder(this, config.codec, config.profile,
                                        config.options,
                                        external_software_factories);

    default:
      NOTREACHED();
      return nullptr;
  }
}

void VideoEncoder::ContinueConfigureWithGpuFactories(
    Request* request,
    media::GpuVideoAcceleratorFactories* external_software_factories,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK(active_config_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_encoder_ = CreateMediaVideoEncoder(
      *active_config_, external_software_factories, gpu_factories);
  if (!media_encoder_) {
    HandleError(logger_->MakeException(
        "Encoder creation error.",
        media::EncoderStatus(
            media::EncoderStatus::Codes::kEncoderInitializationError,
            "Unable to create encoder (most likely unsupported "
            "codec/acceleration requirement combination)")));
    request->EndTracing();
    return;
  }

  auto output_cb = ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
      &VideoEncoder::CallOutputCallback, WrapCrossThreadWeakPersistent(this),
      // We can't use |active_config_| from |this| because it can change by
      // the time the callback is executed.
      WrapCrossThreadPersistent(active_config_.Get()), reset_count_));

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::VideoCodec codec,
                          media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(self->active_config_);

    if (!status.is_ok()) {
      self->HandleError(self->logger_->MakeException(
          "Encoder initialization error.", std::move(status)));
    } else {
      UMA_HISTOGRAM_ENUMERATION("Blink.WebCodecs.VideoEncoder.Codec", codec,
                                media::VideoCodec::kMaxValue);
    }
    req->EndTracing();

    self->blocking_request_in_progress_ = false;
    self->ProcessRequests();
  };

  media_encoder_->Initialize(
      active_config_->profile, active_config_->options, std::move(output_cb),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, WrapCrossThreadWeakPersistent(this),
          WrapCrossThreadPersistent(request), active_config_->codec)));
}

bool VideoEncoder::CanReconfigure(ParsedConfig& original_config,
                                  ParsedConfig& new_config) {
  // Reconfigure is intended for things that don't require changing underlying
  // codec implementation and can be changed on the fly.
  return original_config.codec == new_config.codec &&
         original_config.profile == new_config.profile &&
         original_config.level == new_config.level &&
         original_config.hw_pref == new_config.hw_pref;
}

const AtomicString& VideoEncoder::InterfaceName() const {
  return event_target_names::kVideoEncoder;
}

bool VideoEncoder::HasPendingActivity() const {
  return (active_encodes_ > 0) || Base::HasPendingActivity();
}

bool VideoEncoder::ReadyToProcessNextRequest() {
  if (active_encodes_ >= kMaxActiveEncodes)
    return false;

  return Base::ReadyToProcessNextRequest();
}

void VideoEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kEncode);
  DCHECK_GT(requested_encodes_, 0u);

  auto frame = request->input->frame();
  bool keyframe = request->encodeOpts->hasKeyFrameNonNull() &&
                  request->encodeOpts->keyFrameNonNull();
  active_encodes_++;
  request->StartTracingVideoEncode(keyframe, frame->timestamp());

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);

    self->active_encodes_--;
    if (!status.is_ok()) {
      self->HandleError(
          self->logger_->MakeException("Encoding error.", std::move(status)));
    }
    req->EndTracing();
    self->ProcessRequests();
  };

  // Currently underlying encoders can't handle frame backed by textures,
  // so let's readback pixel data to CPU memory.
  // TODO(crbug.com/1229845): We shouldn't be reading back frames here.
  if (frame->HasTextures() && !frame->HasGpuMemoryBuffer()) {
    // TODO(crbug.com/1195433): Once support for alpha channel encoding is
    // implemented, |force_opaque| must be set based on the VideoEncoderConfig.
    const bool can_use_gmb =
        !disable_accelerated_frame_pool_ &&
        CanUseGpuMemoryBufferReadback(frame->format(), /*force_opaque=*/true);
    if (can_use_gmb && !accelerated_frame_pool_) {
      if (auto wrapper = SharedGpuContext::ContextProviderWrapper()) {
        accelerated_frame_pool_ =
            std::make_unique<WebGraphicsContext3DVideoFramePool>(wrapper);
      }
    }
    if (can_use_gmb && accelerated_frame_pool_) {
      // This will execute shortly after CopyRGBATextureToVideoFrame()
      // completes. |blocking_request_in_progress_| = true will ensure that
      // HasPendingActivity() keeps the VideoEncoder alive long enough.
      auto blit_done_callback =
          [](VideoEncoder* self, bool keyframe, uint32_t reset_count,
             base::TimeDelta timestamp, media::VideoFrameMetadata metadata,
             media::VideoEncoder::EncoderStatusCB done_callback,
             scoped_refptr<media::VideoFrame> frame) {
            TRACE_EVENT_NESTABLE_ASYNC_END0(
                "media", "CopyRGBATextureToVideoFrame", self);
            if (!self || self->reset_count_ != reset_count || !frame)
              return;

            // CopyRGBATextureToVideoFrame() operates on mailboxes and not
            // frames, so we must manually copy over properties relevant to the
            // encoder.
            frame->set_timestamp(timestamp);
            frame->set_metadata(metadata);

            DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
            --self->requested_encodes_;
            self->ScheduleDequeueEvent();
            self->blocking_request_in_progress_ = false;
            self->media_encoder_->Encode(std::move(frame), keyframe,
                                         std::move(done_callback));
            self->ProcessRequests();
          };

      auto origin = frame->metadata().texture_origin_is_top_left
                        ? kTopLeft_GrSurfaceOrigin
                        : kBottomLeft_GrSurfaceOrigin;

      // TODO(crbug.com/1224279): This assumes that all frames are 8-bit sRGB.
      // Expose the color space and pixel format that is backing
      // `image->GetMailboxHolder()`, or, alternatively, expose an accelerated
      // SkImage.
      auto format = (frame->format() == media::PIXEL_FORMAT_XBGR ||
                     frame->format() == media::PIXEL_FORMAT_ABGR)
                        ? viz::ResourceFormat::RGBA_8888
                        : viz::ResourceFormat::BGRA_8888;

      // Stall request processing while we wait for the copy to complete. It'd
      // be nice to not have to do this, but currently the request processing
      // loop must execute synchronously or flush() will miss frames.
      blocking_request_in_progress_ = true;
      // When doing RGBA to YUVA conversion using `accelerated_frame_pool_`, use
      // sRGB primaries and the 601 YUV matrix. Note that this is subtly
      // different from the 601 gfx::ColorSpace because the 601 gfx::ColorSpace
      // has different (non-sRGB) primaries.
      // https://crbug.com/1258245
      constexpr gfx::ColorSpace dst_color_space(
          gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::SRGB,
          gfx::ColorSpace::MatrixID::SMPTE170M,
          gfx::ColorSpace::RangeID::LIMITED);

      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "CopyRGBATextureToVideoFrame",
                                        this, "timestamp", frame->timestamp());
      if (accelerated_frame_pool_->CopyRGBATextureToVideoFrame(
              format, frame->coded_size(), frame->ColorSpace(), origin,
              frame->mailbox_holder(0), dst_color_space,
              WTF::Bind(blit_done_callback, WrapWeakPersistent(this), keyframe,
                        reset_count_, frame->timestamp(), frame->metadata(),
                        ConvertToBaseOnceCallback(CrossThreadBindOnce(
                            done_callback, WrapCrossThreadWeakPersistent(this),
                            WrapCrossThreadPersistent(request)))))) {
        request->input->close();
        return;
      }

      TRACE_EVENT_NESTABLE_ASYNC_END0("media", "CopyRGBATextureToVideoFrame",
                                      this);

      // Error occurred, fall through to normal readback path below.
      blocking_request_in_progress_ = false;
      disable_accelerated_frame_pool_ = true;
      accelerated_frame_pool_.reset();
    }

    auto wrapper = SharedGpuContext::ContextProviderWrapper();
    scoped_refptr<viz::RasterContextProvider> raster_provider;
    if (wrapper && wrapper->ContextProvider())
      raster_provider = wrapper->ContextProvider()->RasterContextProvider();
    if (raster_provider) {
      auto* ri = raster_provider->RasterInterface();
      auto* gr_context = raster_provider->GrContext();

      frame = ReadbackTextureBackedFrameToMemorySync(*frame, ri, gr_context,
                                                     &readback_frame_pool_);
    } else {
      frame.reset();
    }
  }

  if (!frame) {
    callback_runner_->PostTask(
        FROM_HERE, ConvertToBaseOnceCallback(CrossThreadBindOnce(
                       done_callback, WrapCrossThreadWeakPersistent(this),
                       WrapCrossThreadPersistent(request),
                       media::EncoderStatus(
                           media::EncoderStatus::Codes::kEncoderFailedEncode,
                           "Can't readback frame textures."))));
    return;
  }

  // Currently underlying encoders can't handle alpha channel, so let's
  // wrap a frame with an alpha channel into a frame without it.
  // For example such frames can come from 2D canvas context with alpha = true.
  if (frame->storage_type() == media::VideoFrame::STORAGE_OWNED_MEMORY &&
      frame->format() == media::PIXEL_FORMAT_I420A) {
    frame = media::WrapAsI420VideoFrame(std::move(frame));
  }

  --requested_encodes_;
  ScheduleDequeueEvent();
  media_encoder_->Encode(frame, keyframe,
                         ConvertToBaseOnceCallback(CrossThreadBindOnce(
                             done_callback, WrapCrossThreadWeakPersistent(this),
                             WrapCrossThreadPersistent(request))));

  // We passed a copy of frame() above, so this should be safe to close here.
  request->input->close();
}

void VideoEncoder::ProcessConfigure(Request* request) {
  DCHECK_NE(state_.AsEnum(), V8CodecState::Enum::kClosed);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(active_config_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request->StartTracing();

  blocking_request_in_progress_ = true;

#if BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
  if (base::IsFeatureEnabled(base::kFeatureExternalOpenH264Encoder)) {
    RetrieveExternalSoftwareFactoriesWithKnownEncoderSupport(
        CrossThreadBindOnce(
            &VideoEncoder::ContinueConfigureWithExternalSoftwareFactories,
            WrapCrossThreadWeakPersistent(this),
            WrapCrossThreadPersistent(request)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
  ContinueConfigureWithExternalSoftwareFactories(request, nullptr);
}

void VideoEncoder::ContinueConfigureWithExternalSoftwareFactories(
    Request* request,
    media::GpuVideoAcceleratorFactories* external_software_factories) {
  DCHECK(active_config_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (active_config_->hw_pref == HardwarePreference::kPreferSoftware) {
    ContinueConfigureWithGpuFactories(request, external_software_factories,
                                      nullptr);
    return;
  }

  RetrieveGpuFactoriesWithKnownEncoderSupport(CrossThreadBindOnce(
      &VideoEncoder::ContinueConfigureWithGpuFactories,
      WrapCrossThreadWeakPersistent(this), WrapCrossThreadPersistent(request),
      CrossThreadUnretained(external_software_factories)));
}

void VideoEncoder::ProcessReconfigure(Request* request) {
  DCHECK_EQ(state_.AsEnum(), V8CodecState::Enum::kConfigured);
  DCHECK_EQ(request->type, Request::Type::kReconfigure);
  DCHECK(active_config_);
  DCHECK(media_encoder_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request->StartTracing();

  auto reconf_done_callback = [](VideoEncoder* self, Request* req,
                                 media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(self->active_config_);

    req->EndTracing();

    if (status.is_ok()) {
      self->blocking_request_in_progress_ = false;
      self->ProcessRequests();
    } else {
      // Reconfiguration failed. Either encoder doesn't support changing options
      // or it didn't like this particular change. Let's try to configure it
      // from scratch.
      req->type = Request::Type::kConfigure;
      self->ProcessConfigure(req);
    }
  };

  auto flush_done_callback = [](VideoEncoder* self, Request* req,
                                decltype(reconf_done_callback) reconf_callback,
                                media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError(self->logger_->MakeException(
          "Encoder initialization error.", std::move(status)));
      self->blocking_request_in_progress_ = false;
      req->EndTracing();
      return;
    }

    auto output_cb =
        ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
            &VideoEncoder::CallOutputCallback,
            WrapCrossThreadWeakPersistent(self),
            // We can't use |active_config_| from |this| because it can change
            // by the time the callback is executed.
            WrapCrossThreadPersistent(self->active_config_.Get()),
            self->reset_count_));

    self->first_output_after_configure_ = true;
    self->media_encoder_->ChangeOptions(
        self->active_config_->options, std::move(output_cb),
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            reconf_callback, WrapCrossThreadWeakPersistent(self),
            WrapCrossThreadPersistent(req))));
  };

  blocking_request_in_progress_ = true;
  media_encoder_->Flush(WTF::Bind(
      flush_done_callback, WrapCrossThreadWeakPersistent(this),
      WrapCrossThreadPersistent(request), std::move(reconf_done_callback)));
}

void VideoEncoder::CallOutputCallback(
    ParsedConfig* active_config,
    uint32_t reset_count,
    media::VideoEncoderOutput output,
    absl::optional<media::VideoEncoder::CodecDescription> codec_desc) {
  DCHECK(active_config);
  if (!script_state_->ContextIsValid() || !output_callback_ ||
      state_.AsEnum() != V8CodecState::Enum::kConfigured ||
      reset_count != reset_count_) {
    return;
  }

  MarkCodecActive();

  auto buffer =
      media::DecoderBuffer::FromArray(std::move(output.data), output.size);
  buffer->set_timestamp(output.timestamp);
  buffer->set_is_key_frame(output.key_frame);
  auto* chunk = MakeGarbageCollected<EncodedVideoChunk>(std::move(buffer));

  auto* metadata = EncodedVideoChunkMetadata::Create();
  if (active_config->options.scalability_mode.has_value()) {
    auto* svc_metadata = SvcOutputMetadata::Create();
    svc_metadata->setTemporalLayerId(output.temporal_id);
    metadata->setSvc(svc_metadata);
  }

  // TODO(https://crbug.com/1241448): All encoders should output color space.
  // For now, fallback to 601 since that is correct most often.
  gfx::ColorSpace output_color_space = output.color_space.IsValid()
                                           ? output.color_space
                                           : gfx::ColorSpace::CreateREC601();

  if (first_output_after_configure_ || codec_desc.has_value() ||
      output_color_space != last_output_color_space_) {
    first_output_after_configure_ = false;

    if (output_color_space != last_output_color_space_) {
// TODO(crbug.com/1241448): Make Android obey the contract below. For now
// Android VEA only _eventually_ gives a key frame when color space changes.
#if !BUILDFLAG(IS_ANDROID)
      DCHECK(output.key_frame) << "Encoders should generate a keyframe when "
                               << "changing color space";
#endif
      last_output_color_space_ = output_color_space;
    }

    auto* decoder_config = VideoDecoderConfig::Create();
    decoder_config->setCodec(active_config->codec_string);
    decoder_config->setCodedHeight(active_config->options.frame_size.height());
    decoder_config->setCodedWidth(active_config->options.frame_size.width());

    if (active_config->display_size.has_value()) {
      decoder_config->setDisplayAspectHeight(
          active_config->display_size.value().height());
      decoder_config->setDisplayAspectWidth(
          active_config->display_size.value().width());
    }

    VideoColorSpace* color_space =
        MakeGarbageCollected<VideoColorSpace>(output_color_space);
    decoder_config->setColorSpace(color_space->toJSON());

    if (codec_desc.has_value()) {
      auto* desc_array_buf = DOMArrayBuffer::Create(codec_desc.value().data(),
                                                    codec_desc.value().size());
      decoder_config->setDescription(
          MakeGarbageCollected<AllowSharedBufferSource>(desc_array_buf));
    }
    metadata->setDecoderConfig(decoder_config);
  }

  TRACE_EVENT_BEGIN1(kCategory, GetTraceNames()->output.c_str(), "timestamp",
                     chunk->timestamp());

  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, metadata);

  TRACE_EVENT_END0(kCategory, GetTraceNames()->output.c_str());
}

void VideoEncoder::ResetInternal() {
  Base::ResetInternal();
  active_encodes_ = 0;
}

static void isConfigSupportedWithSoftwareOnly(
    ScriptPromiseResolver* resolver,
    VideoEncoderSupport* support,
    VideoEncoderTraits::ParsedConfig* config) {
  std::unique_ptr<media::VideoEncoder> software_encoder;
  switch (config->codec) {
    case media::VideoCodec::kAV1:
      software_encoder = CreateAv1VideoEncoder();
      break;
    case media::VideoCodec::kVP8:
    case media::VideoCodec::kVP9:
      software_encoder = CreateVpxVideoEncoder();
      break;
    case media::VideoCodec::kH264:
      software_encoder = CreatePlatformH264VideoEncoder();
      if (!software_encoder)
        software_encoder = CreateOpenH264VideoEncoder();
      break;
    default:
      break;
  }
  if (!software_encoder) {
    support->setSupported(false);
    resolver->Resolve(support);
    return;
  }

  auto done_callback = [](std::unique_ptr<media::VideoEncoder> sw_encoder,
                          ScriptPromiseResolver* resolver,
                          VideoEncoderSupport* support,
                          media::EncoderStatus status) {
    support->setSupported(status.is_ok());
    resolver->Resolve(support);
    DeleteLater(resolver->GetScriptState(), std::move(sw_encoder));
  };

  auto* software_encoder_raw = software_encoder.get();
  software_encoder_raw->Initialize(
      config->profile, config->options, base::DoNothing(),
      ConvertToBaseOnceCallback(
          CrossThreadBindOnce(done_callback, std::move(software_encoder),
                              WrapCrossThreadPersistent(resolver),
                              WrapCrossThreadPersistent(support))));
}

static void isConfigSupportedWithHardwareOnly(
    ScriptPromiseResolver* resolver,
    VideoEncoderSupport* support,
    VideoEncoderTraits::ParsedConfig* config,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  bool supported = IsAcceleratedConfigurationSupported(
      config->profile, config->options, gpu_factories);
  support->setSupported(supported);
  resolver->Resolve(support);
}

class FindAnySupported final : public ScriptFunction::Callable {
 public:
  ScriptValue Call(ScriptState* state, ScriptValue value) override {
    ExceptionContext context(
        ExceptionContext::Context::kConstructorOperationInvoke,
        "VideoEncoderSupport");
    ExceptionState exception_state(state->GetIsolate(), context);
    HeapVector<Member<VideoEncoderSupport>> supports =
        NativeValueTraits<IDLSequence<VideoEncoderSupport>>::NativeValue(
            state->GetIsolate(), value.V8Value(), exception_state);

    VideoEncoderSupport* result = nullptr;
    // We don't really expect exceptions here, but if isConfigSupported() is
    // given a VideoEncoderConfig with uint64 values above max JS int (2^53 - 1)
    // creation of |supports| vector will fail. This can happen during fuzzing.
    if (!exception_state.HadException()) {
      for (auto& support : supports) {
        result = support;
        if (result->supported())
          break;
      }
    }
    return ScriptValue::From(state, result);
  }
};

// static
ScriptPromise VideoEncoder::isConfigSupported(ScriptState* script_state,
                                              const VideoEncoderConfig* config,
                                              ExceptionState& exception_state) {
  auto* parsed_config = ParseConfigStatic(config, exception_state);
  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return ScriptPromise();
  }
  auto* config_copy = CopyConfig(*config);

  // Run very basic coarse synchronous validation
  if (!VerifyCodecSupportStatic(parsed_config, nullptr)) {
    auto* support = VideoEncoderSupport::Create();
    support->setConfig(config_copy);
    support->setSupported(false);

    return ScriptPromise::Cast(
        script_state,
        ToV8Traits<VideoEncoderSupport>::ToV8(script_state, support)
            .ToLocalChecked());
  }

  // Create promises for resolving hardware and software encoding support and
  // put them into |promises|. Simultaneously run both versions of
  // isConfigSupported(), each version fulfills its own promise.
  HeapVector<ScriptPromise> promises;
  if (parsed_config->hw_pref != HardwarePreference::kPreferSoftware) {
    // Hardware support not denied, detect support by hardware encoders.
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    promises.push_back(resolver->Promise());
    auto* support = VideoEncoderSupport::Create();
    support->setConfig(config_copy);
    auto gpu_retrieved_callback = CrossThreadBindOnce(
        isConfigSupportedWithHardwareOnly, WrapCrossThreadPersistent(resolver),
        WrapCrossThreadPersistent(support),
        WrapCrossThreadPersistent(parsed_config));
    RetrieveGpuFactoriesWithKnownEncoderSupport(
        std::move(gpu_retrieved_callback));
  }

  if (parsed_config->hw_pref != HardwarePreference::kPreferHardware) {
    // Hardware support not required, detect support by software encoders.
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    promises.push_back(resolver->Promise());
    auto* support = VideoEncoderSupport::Create();
    support->setConfig(config_copy);
    isConfigSupportedWithSoftwareOnly(resolver, support, parsed_config);
  }

#if BUILDFLAG(ENABLE_EXTERNAL_OPENH264)
  if (base::IsFeatureEnabled(base::kFeatureExternalOpenH264Encoder) &&
      parsed_config->hw_pref != HardwarePreference::kPreferHardware) {
    // Hardware support not required, detect support by the external software
    // encoder.
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    promises.push_back(resolver->Promise());
    auto* support = VideoEncoderSupport::Create();
    support->setConfig(config_copy);
    // Don't be mislead by isConfigSupportedWithHardwareOnly() below: We feed
    // it the external software factories.
    RetrieveExternalSoftwareFactoriesWithKnownEncoderSupport(
        CrossThreadBindOnce(isConfigSupportedWithHardwareOnly,
                            WrapCrossThreadPersistent(resolver),
                            WrapCrossThreadPersistent(support),
                            WrapCrossThreadPersistent(parsed_config)));
  }
#endif  // BUILDFLAG(ENABLE_EXTERNAL_OPENH264)

  // Wait for all |promises| to resolve and check if any of them have
  // support=true.
  auto* find_any_supported = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<FindAnySupported>());

  return ScriptPromise::All(script_state, promises).Then(find_any_supported);
}

}  // namespace blink
