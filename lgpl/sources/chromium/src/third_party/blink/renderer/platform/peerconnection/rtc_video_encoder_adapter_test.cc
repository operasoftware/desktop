// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_adapter.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/encoder_status.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video/video_bitrate_allocation.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

namespace blink {

namespace {

constexpr auto kRtpTicksPerSecond = 90000;
constexpr auto kRtpTicksPerMs = kRtpTicksPerSecond / 1000;
constexpr auto kFrameInterval25fps = base::Milliseconds(1000 / 25);

constexpr media::VideoCodecProfile kProfile = media::H264PROFILE_BASELINE;
constexpr gfx::Size kFrameSize1 = {320, 240};
constexpr gfx::Size kFrameSize2 = {480, 360};

const webrtc::VideoEncoder::Capabilities kVideoEncoderCapabilities(
    /*loss_notification=*/false);
const webrtc::VideoEncoder::Settings
    kVideoEncoderSettings(kVideoEncoderCapabilities, 1, 12345);

webrtc::VideoFrame CreateTestFrame(const gfx::Size& size,
                                   base::TimeDelta timestamp) {
  auto frame = media::VideoFrame::CreateColorFrame(size, 12, 13, 14, timestamp);
  auto buffer =
      rtc::make_ref_counted<WebRtcVideoFrameAdapter>(std::move(frame));
  return webrtc::VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_ntp_time_ms(timestamp.InMilliseconds())
      .set_timestamp_rtp(timestamp.InMilliseconds() * kRtpTicksPerMs)
      .build();
}

webrtc::VideoFrame CreateBlackFrame(const gfx::Size& size,
                                    base::TimeDelta timestamp) {
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(size.width(), size.height());
  webrtc::I420Buffer::SetBlack(buffer.get());
  return webrtc::VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_ntp_time_ms(timestamp.InMilliseconds())
      .set_timestamp_rtp(timestamp.InMilliseconds() * kRtpTicksPerMs)
      .build();
}

class TestEncoder final : public media::VideoEncoder {
 public:
  explicit TestEncoder(base::OnceClosure destroy_cb)
      : destroy_cb_(std::move(destroy_cb)) {}
  ~TestEncoder() override {
    std::move(destroy_cb_).Run();
  }

  media::VideoCodecProfile profile() const { return profile_; }
  const Options& options() const { return options_; }

  void set_status(media::EncoderStatus::Codes status) { status_ = status; }

  // All pending frames will finish encoding immediately.
  void ReturnAllFrames(bool produce_corrupt_output = false) {
    for (const PendingEncode& pending_encode : pending_encodes_) {
      media::VideoEncoderOutput output;
      if (produce_corrupt_output) {
        output.timestamp = base::TimeDelta::Max();
      } else {
        output.timestamp = pending_encode.frame->timestamp();
        output.key_frame = pending_encode.options.key_frame;
      }
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(output_cb_, std::move(output),
                                    /*codec_description=*/absl::nullopt));
    }
    pending_encodes_.clear();
  }

  // media::VideoDecoder:
  void Initialize(media::VideoCodecProfile profile,
                  const Options& options,
                  EncoderInfoCB info_cb,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override {
    ASSERT_FALSE(is_initialized());
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();

    profile_ = profile;
    options_ = options;
    output_cb_ = std::move(output_cb);
    initialized_ = status_ == media::EncoderStatus::Codes::kOk;
    RespondWithStatus(std::move(done_cb), status_);
  }
  void Encode(scoped_refptr<media::VideoFrame> frame,
              const EncodeOptions& options,
              EncoderStatusCB done_cb) override {
    ASSERT_TRUE(is_initialized());
    PendingEncode pending_encode;
    pending_encode.frame = std::move(frame);
    pending_encode.options = options;
    pending_encodes_.push_back(std::move(pending_encode));
    RespondWithStatus(std::move(done_cb), status_);
  }
  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     EncoderStatusCB done_cb) override {
    ASSERT_TRUE(is_initialized());
    ASSERT_TRUE(pending_encodes_.empty()) << "Expected Flush() first";
    options_ = options;
    if (output_cb)
      output_cb_ = std::move(output_cb);
    initialized_ = status_ == media::EncoderStatus::Codes::kOk;
    RespondWithStatus(std::move(done_cb), status_);
  }
  void Flush(EncoderStatusCB done_cb) override {
    ASSERT_TRUE(is_initialized());
    ReturnAllFrames();
    RespondWithStatus(std::move(done_cb), status_);
  }

 private:
  bool is_initialized() const { return initialized_; }

  void RespondWithStatus(EncoderStatusCB callback,
                         media::EncoderStatus status,
                         const base::Location& location = FROM_HERE) {
    task_runner_->PostTask(
        location, base::BindOnce(std::move(callback), std::move(status)));
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::OnceClosure destroy_cb_;
  media::VideoCodecProfile profile_ =  media::VIDEO_CODEC_PROFILE_UNKNOWN;
  Options options_;
  OutputCB output_cb_;
  std::vector<PendingEncode> pending_encodes_;
  media::EncoderStatus::Codes status_ = media::EncoderStatus::Codes::kOk;
  bool initialized_ = false;
};

class TestEncodedImageCallback final : public webrtc::EncodedImageCallback {
 public:
  explicit TestEncodedImageCallback(int expected_image_count)
      : pending_image_count_(expected_image_count) {}

  // Waits until all expected images have arrived.
  std::vector<webrtc::EncodedImage> WaitAndGetImages() {
    run_loop_.Run();
    return images_;
  }

  bool has_dropped_frames() const { return has_dropped_frames_; }

  // webrtc::EncodedImageCallback:
  Result OnEncodedImage(
      const webrtc::EncodedImage& encoded_image,
      const webrtc::CodecSpecificInfo* codec_specific_info) override {
    images_.push_back(encoded_image);
    if (--pending_image_count_ == 0)
      run_loop_.Quit();
    return {Result::OK, encoded_image.RtpTimestamp()};
  }
  void OnDroppedFrame(DropReason reason) override {
    has_dropped_frames_ = true;
    if (--pending_image_count_ == 0)
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  int pending_image_count_;
  std::vector<webrtc::EncodedImage> images_;
  bool has_dropped_frames_ = false;
};

}  // namespace

class RTCVideoEncoderAdapterTest : public testing::Test {
 public:
  RTCVideoEncoderAdapterTest() {
    // The adapter can dispose of the encoder early (on error, etc.). We must
    // clear our encoder reference in this case to prevent it from dangling.
    auto test_encoder = std::make_unique<TestEncoder>(
        base::BindLambdaForTesting([this]() { test_encoder_ = nullptr; }));
    test_encoder_ = test_encoder.get();
    adapter_ = std::make_unique<RTCVideoEncoderAdapter>(
        kProfile, std::move(test_encoder));
  }

  void TearDown() override {
    ASSERT_EQ(adapter_->Release(), WEBRTC_VIDEO_CODEC_OK);
  }

  RTCVideoEncoderAdapter& adapter() { return *adapter_; }
  TestEncoder& test_encoder() {
    CHECK(test_encoder_) << "The adapter has disposed of the encoder already";
    return *test_encoder_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<RTCVideoEncoderAdapter> adapter_;
  base::raw_ptr<TestEncoder> test_encoder_;
};

TEST_F(RTCVideoEncoderAdapterTest, InitEncodeSuccess) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  EXPECT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(test_encoder().profile(), kProfile);
  EXPECT_EQ(test_encoder().options().frame_size, kFrameSize1);

  EXPECT_FALSE(test_encoder().options().scalability_mode.has_value());
  ASSERT_EQ(adapter().GetEncoderInfo().fps_allocation[0].size(), 1u);
  EXPECT_EQ(adapter().GetEncoderInfo().fps_allocation[0][0], 255u);
}

TEST_F(RTCVideoEncoderAdapterTest, InitEncodeFailure) {
  test_encoder().set_status(
      media::EncoderStatus::Codes::kEncoderInitializationError);
  webrtc::VideoCodec codec;
  EXPECT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_UNINITIALIZED);
}

TEST_F(RTCVideoEncoderAdapterTest, FpsAllocation) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  codec.SetScalabilityMode(webrtc::ScalabilityMode::kL1T2);
  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  ASSERT_TRUE(test_encoder().options().scalability_mode.has_value());
  EXPECT_EQ(test_encoder().options().scalability_mode.value(),
            media::SVCScalabilityMode::kL1T2);

  ASSERT_EQ(adapter().GetEncoderInfo().fps_allocation[0].size(), 2u);
  EXPECT_EQ(adapter().GetEncoderInfo().fps_allocation[0][0], 127u);
  EXPECT_EQ(adapter().GetEncoderInfo().fps_allocation[0][1], 255u);
}

TEST_F(RTCVideoEncoderAdapterTest, Encode) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  constexpr auto kFrameCount = 4u;
  TestEncodedImageCallback encoded_image_callback(kFrameCount);
  ASSERT_EQ(adapter().RegisterEncodeCompleteCallback(&encoded_image_callback),
            WEBRTC_VIDEO_CODEC_OK);

  // Encode some frames, requesting every second one to be a key frame.
  for (size_t i = 0; i < kFrameCount; ++i) {
    std::vector<webrtc::VideoFrameType> frame_types = {
        i % 2 == 0 ? webrtc::VideoFrameType::kVideoFrameKey
                   : webrtc::VideoFrameType::kVideoFrameDelta};
    EXPECT_EQ(
        adapter().Encode(CreateTestFrame(kFrameSize1, kFrameInterval25fps * i),
                         &frame_types),
        WEBRTC_VIDEO_CODEC_OK);
  }

  test_encoder().ReturnAllFrames();
  const auto images = encoded_image_callback.WaitAndGetImages();

  ASSERT_EQ(images.size(), kFrameCount);
  for (size_t i = 0; i < kFrameCount; ++i) {
    EXPECT_EQ(gfx::Size(images[i]._encodedWidth, images[i]._encodedHeight),
              kFrameSize1);
    EXPECT_EQ(base::Milliseconds(images[i].RtpTimestamp() / kRtpTicksPerMs),
              kFrameInterval25fps * i);
    EXPECT_EQ(images[i]._frameType,
              i % 2 == 0 ? webrtc::VideoFrameType::kVideoFrameKey
                         : webrtc::VideoFrameType::kVideoFrameDelta);
  }
}

TEST_F(RTCVideoEncoderAdapterTest, ReInitEncode) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  constexpr auto kFrameCount = 2u;

  // Enqueue a couple of frames.
  TestEncodedImageCallback encoded_image_callback1(kFrameCount);
  ASSERT_EQ(adapter().RegisterEncodeCompleteCallback(&encoded_image_callback1),
            WEBRTC_VIDEO_CODEC_OK);
  for (size_t i = 0; i < kFrameCount; ++i) {
    ASSERT_EQ(
        adapter().Encode(CreateTestFrame(kFrameSize1, kFrameInterval25fps * i),
                         /*frame_types=*/nullptr),
        WEBRTC_VIDEO_CODEC_OK);
  }

  // Re-initialization must be preceded by a Release(), which flushes any
  // pending encodes.
  EXPECT_EQ(adapter().Release(), WEBRTC_VIDEO_CODEC_OK);
  auto images = encoded_image_callback1.WaitAndGetImages();

  ASSERT_EQ(images.size(), kFrameCount);
  for (size_t i = 0; i < kFrameCount; ++i) {
    EXPECT_EQ(gfx::Size(images[i]._encodedWidth, images[i]._encodedHeight),
              kFrameSize1);
  }

  codec.width = kFrameSize2.width();
  codec.height = kFrameSize2.height();
  EXPECT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(test_encoder().options().frame_size, kFrameSize2);

  // Enqueue frames of a different size.
  TestEncodedImageCallback encoded_image_callback2(kFrameCount);
  ASSERT_EQ(adapter().RegisterEncodeCompleteCallback(&encoded_image_callback2),
            WEBRTC_VIDEO_CODEC_OK);
  for (size_t i = 0; i < kFrameCount; ++i) {
    ASSERT_EQ(
        adapter().Encode(CreateTestFrame(kFrameSize2, kFrameInterval25fps * i),
                         /*frame_types=*/nullptr),
        WEBRTC_VIDEO_CODEC_OK);
  }

  test_encoder().ReturnAllFrames();
  images = encoded_image_callback2.WaitAndGetImages();

  ASSERT_EQ(images.size(), kFrameCount);
  for (size_t i = 0; i < kFrameCount; ++i) {
    EXPECT_EQ(gfx::Size(images[i]._encodedWidth, images[i]._encodedHeight),
              kFrameSize2);
  }
}

TEST_F(RTCVideoEncoderAdapterTest, EncodeNonNativeFrame) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  TestEncodedImageCallback encoded_image_callback(1);
  ASSERT_EQ(adapter().RegisterEncodeCompleteCallback(&encoded_image_callback),
            WEBRTC_VIDEO_CODEC_OK);

  std::vector<webrtc::VideoFrameType> frame_types = {
      webrtc::VideoFrameType::kVideoFrameKey};
  EXPECT_EQ(adapter().Encode(CreateBlackFrame(kFrameSize1, base::TimeDelta()),
                             &frame_types),
            WEBRTC_VIDEO_CODEC_OK);

  test_encoder().ReturnAllFrames();
  const auto images = encoded_image_callback.WaitAndGetImages();
  EXPECT_EQ(images.size(), 1u);
}

TEST_F(RTCVideoEncoderAdapterTest, CorruptEncoderOutput) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  TestEncodedImageCallback encoded_image_callback(1);
  ASSERT_EQ(adapter().RegisterEncodeCompleteCallback(&encoded_image_callback),
            WEBRTC_VIDEO_CODEC_OK);

  std::vector<webrtc::VideoFrameType> frame_types = {
      webrtc::VideoFrameType::kVideoFrameKey};
  EXPECT_EQ(adapter().Encode(CreateTestFrame(kFrameSize1, base::TimeDelta()),
                             &frame_types),
            WEBRTC_VIDEO_CODEC_OK);

  test_encoder().ReturnAllFrames(/*produce_corrupt_output=*/true);
  const auto images = encoded_image_callback.WaitAndGetImages();

  EXPECT_TRUE(images.empty());
  EXPECT_TRUE(encoded_image_callback.has_dropped_frames());
}

TEST_F(RTCVideoEncoderAdapterTest, Rates) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  codec.maxFramerate = 60;
  codec.startBitrate = 9000;

  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);
  EXPECT_EQ(test_encoder().options().framerate.value_or(0), codec.maxFramerate);
  EXPECT_EQ(
      test_encoder().options().bitrate.value_or(media::Bitrate()).target_bps(),
      codec.startBitrate);

  TestEncodedImageCallback encoded_image_callback(1);
  ASSERT_EQ(adapter().RegisterEncodeCompleteCallback(&encoded_image_callback),
            WEBRTC_VIDEO_CODEC_OK);
  std::vector<webrtc::VideoFrameType> frame_types = {
      webrtc::VideoFrameType::kVideoFrameKey};
  ASSERT_EQ(adapter().Encode(CreateTestFrame(kFrameSize1, {}), &frame_types),
            WEBRTC_VIDEO_CODEC_OK);

  webrtc::VideoBitrateAllocation new_bitrate;
  new_bitrate.SetBitrate(0, 0, 18000);
  const auto new_framerate = 30;
  adapter().SetRates({new_bitrate, new_framerate});

  EXPECT_EQ(test_encoder().options().framerate.value_or(0), new_framerate);
  EXPECT_EQ(
      test_encoder().options().bitrate.value_or(media::Bitrate()).target_bps(),
      new_bitrate.GetBitrate(0, 0));
}

TEST_F(RTCVideoEncoderAdapterTest, SetRatesUninitialized) {
  // Since we didn't initialize the encoder, SetRates() should do
  // nothing (if it does, TestEncoder will fail assertions).
  webrtc::VideoBitrateAllocation new_bitrate;
  new_bitrate.SetBitrate(0, 0, 18000);
  const auto new_framerate = 30;
  adapter().SetRates({new_bitrate, new_framerate});
}

TEST_F(RTCVideoEncoderAdapterTest, SetRatesFailure) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  codec.maxFramerate = 60;
  codec.startBitrate = 9000;

  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);
  std::vector<webrtc::VideoFrameType> frame_types = {
      webrtc::VideoFrameType::kVideoFrameKey};
  ASSERT_EQ(adapter().Encode(CreateTestFrame(kFrameSize1, {}), &frame_types),
            WEBRTC_VIDEO_CODEC_OK);

  webrtc::VideoBitrateAllocation new_bitrate;
  new_bitrate.SetBitrate(0, 0, 18000);
  constexpr auto new_framerate = 30;
  test_encoder().set_status(
      media::EncoderStatus::Codes::kEncoderInitializationError);
  adapter().SetRates({new_bitrate, new_framerate});

  // The encoder should reject encodes following a failure to set the rates.
  ASSERT_EQ(adapter().Encode(CreateTestFrame(kFrameSize1, {}), &frame_types),
            WEBRTC_VIDEO_CODEC_UNINITIALIZED);
}

TEST_F(RTCVideoEncoderAdapterTest, ReleaseUninitialized) {
  // Since we didn't initialize the encoder, Release() should do
  // nothing (if it does, TestEncoder will fail assertions).
  EXPECT_EQ(adapter().Release(), WEBRTC_VIDEO_CODEC_OK);
}

TEST_F(RTCVideoEncoderAdapterTest, ReleaseAfterFailedReInit) {
  webrtc::VideoCodec codec;
  codec.width = kFrameSize1.width();
  codec.height = kFrameSize1.height();
  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  // Re-initialization must be preceded by a Release().
  ASSERT_EQ(adapter().Release(), WEBRTC_VIDEO_CODEC_OK);

  codec.width = kFrameSize2.width();
  codec.height = kFrameSize2.height();
  test_encoder().set_status(
      media::EncoderStatus::Codes::kEncoderInitializationError);
  ASSERT_EQ(adapter().InitEncode(&codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_UNINITIALIZED);

  // Since we failed to re-initialize the encoder, Release() should do
  // nothing (verified in TearDown()).
}

}  // namespace blink
