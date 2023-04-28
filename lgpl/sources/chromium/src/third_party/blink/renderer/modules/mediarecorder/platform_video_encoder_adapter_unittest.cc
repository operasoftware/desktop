// Copyright (C) 2022 Opera Norway AS. All rights reserved.
//
// This file is an original work developed by Opera.

#include "third_party/blink/renderer/modules/mediarecorder/platform_video_encoder_adapter.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/encoder_status.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

namespace blink {

namespace {

scoped_refptr<media::VideoFrame> MakeTestFrame() {
  return media::VideoFrame::CreateBlackFrame({320, 240});
}

class TestVideoEncoder final : public media::VideoEncoder {
 public:
  void set_status(media::EncoderStatus::Codes status) { status_ = status; }
  void set_encoded_data_size(size_t size) { encoded_data_size_ = size; }
  void set_key_frame(bool key_frame) { key_frame_ = key_frame; }
  const absl::optional<Options>& options() const { return options_; }

  // VideoDecoder implementation.
  void Initialize(media::VideoCodecProfile profile,
                  const Options& options,
                  EncoderInfoCB info_cb,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override {
    options_ = options;
    output_cb_ = std::move(output_cb);
    RespondWithStatus(std::move(done_cb));
  }
  void Encode(scoped_refptr<media::VideoFrame> frame,
              bool key_frame,
              EncoderStatusCB done_cb) override {
    RespondWithData();
    RespondWithStatus(std::move(done_cb));
  }
  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     EncoderStatusCB done_cb) override {
    FAIL() << "Unexpected call";
  }
  void Flush(EncoderStatusCB done_cb) override {
    FAIL() << "Unexpected call";
  }

 private:
  void RespondWithData(const base::Location& location = FROM_HERE) const {
    media::VideoEncoderOutput output;
    output.size = encoded_data_size_;
    output.data = std::make_unique<uint8_t[]>(output.size);
    output.key_frame = key_frame_;

    scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
        location, base::BindOnce(output_cb_, std::move(output), absl::nullopt));
  }

  void RespondWithStatus(EncoderStatusCB callback,
                         const base::Location& location = FROM_HERE) const {
    scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
        location, base::BindOnce(std::move(callback), status_));
  }

  absl::optional<Options> options_;
  OutputCB output_cb_;
  media::EncoderStatus status_;
  size_t encoded_data_size_ = 5;
  bool key_frame_ = false;
};

}  // namespace

class PlatformVideoEncoderAdapterTest : public testing::Test {
 public:
  PlatformVideoEncoderAdapterTest() {
    auto encoder = std::make_unique<TestVideoEncoder>();
    encoder_ = encoder.get();
    adapter_ = std::make_unique<PlatformVideoEncoderAdapter>(
        std::move(encoder),
        VideoTrackRecorder::CodecProfile(VideoTrackRecorder::CodecId::kH264),
        media::BindToCurrentLoop(base::BindRepeating(
            &PlatformVideoEncoderAdapterTest::OnEncodedVideo,
            base::Unretained(this))),
        media::BindToCurrentLoop(base::BindRepeating(
            &PlatformVideoEncoderAdapterTest::OnError, base::Unretained(this))),
        /*bits_per_second=*/0, MakeTestFrame()->visible_rect().size());
  }

  PlatformVideoEncoderAdapter& adapter() { return *adapter_; }
  TestVideoEncoder& encoder() { return *encoder_; }

  bool has_error() const { return has_error_; }
  bool last_frame_was_key_frame() const { return key_frame_; }
  std::string last_encoded_data() const { return encoded_data_; }

  void WaitUntilError() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void WaitUntilEncodeDone(int frame_count) {
    expected_encode_result_count_ = frame_count;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void OnEncodedVideo(const media::WebmMuxer::VideoParameters& params,
                      std::string encoded_data,
                      std::string encoded_alpha,
                      base::TimeTicks capture_timestamp,
                      bool is_key_frame) {
    encoded_data_ = std::move(encoded_data);
    key_frame_ = is_key_frame;

    if (expected_encode_result_count_.has_value() &&
        --expected_encode_result_count_.value() == 0) {
      run_loop_->Quit();
      expected_encode_result_count_.reset();
    }
  }

  void OnError() {
    has_error_ = true;
    run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  std::unique_ptr<PlatformVideoEncoderAdapter> adapter_;
  base::raw_ptr<TestVideoEncoder> encoder_;

  absl::optional<int> expected_encode_result_count_;
  bool has_error_ = false;
  std::string encoded_data_;
  bool key_frame_ = false;
};

TEST_F(PlatformVideoEncoderAdapterTest, InitializePlatformEncoder) {
  const auto test_frame = MakeTestFrame();

  adapter().StartFrameEncode(test_frame, /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  WaitUntilEncodeDone(1);

  EXPECT_FALSE(has_error());

  const auto options = encoder().options();
  ASSERT_TRUE(options.has_value());
  EXPECT_EQ(options->frame_size, test_frame->visible_rect().size());
  EXPECT_GT(options->bitrate.value_or(media::Bitrate::ConstantBitrate(0u))
                .target_bps(),
            0u)
      << "Must constrain the bitrate in media::VideoEncoder even if it's not "
         "constrained in VideoTrackRecorder";
  EXPECT_TRUE(options->avc.produce_annexb);
}

TEST_F(PlatformVideoEncoderAdapterTest, InitializationError) {
  encoder().set_status(
      media::EncoderStatus::Codes::kEncoderInitializationError);

  adapter().StartFrameEncode(MakeTestFrame(), /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  WaitUntilError();

  EXPECT_TRUE(has_error());
}

TEST_F(PlatformVideoEncoderAdapterTest, EncodeFrame) {
  constexpr size_t kDataSize = 12;
  encoder().set_encoded_data_size(kDataSize);

  const auto test_frame = MakeTestFrame();

  encoder().set_key_frame(true);
  adapter().StartFrameEncode(test_frame, /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  WaitUntilEncodeDone(1);

  EXPECT_FALSE(has_error());
  EXPECT_EQ(last_encoded_data().size(), kDataSize);
  EXPECT_TRUE(last_frame_was_key_frame());

  encoder().set_key_frame(false);
  adapter().StartFrameEncode(test_frame, /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  WaitUntilEncodeDone(1);

  EXPECT_FALSE(has_error());
  EXPECT_EQ(last_encoded_data().size(), kDataSize);
  EXPECT_FALSE(last_frame_was_key_frame());
}

TEST_F(PlatformVideoEncoderAdapterTest, EncodeError) {
  const auto test_frame = MakeTestFrame();

  adapter().StartFrameEncode(test_frame, /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  WaitUntilEncodeDone(1);

  EXPECT_FALSE(has_error());

  encoder().set_status(media::EncoderStatus::Codes::kEncoderFailedEncode);
  adapter().StartFrameEncode(test_frame, /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  WaitUntilError();

  EXPECT_TRUE(has_error());
}

TEST_F(PlatformVideoEncoderAdapterTest, FrameQueue) {
  constexpr size_t kDataSize = 12;
  encoder().set_encoded_data_size(kDataSize);

  encoder().set_key_frame(true);
  adapter().StartFrameEncode(MakeTestFrame(), /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  encoder().set_key_frame(false);
  adapter().StartFrameEncode(MakeTestFrame(), /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  adapter().StartFrameEncode(MakeTestFrame(), /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  WaitUntilEncodeDone(2);
  adapter().StartFrameEncode(MakeTestFrame(), /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  adapter().StartFrameEncode(MakeTestFrame(), /*scaled_video_frames=*/{},
                             base::TimeTicks::Now());
  WaitUntilEncodeDone(3);

  EXPECT_FALSE(has_error());
}

}  // namespace blink
