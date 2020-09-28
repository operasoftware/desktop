// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_audio_element_capturer_source.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_parameters.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/media_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/webaudiosourceprovider_impl.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

static const int kNumChannelsForTest = 1;
static const int kBufferDurationMs = 10;

static const int kAudioTrackSampleRate = 48000;
static const int kAudioTrackSamplesPerBuffer =
    kAudioTrackSampleRate * kBufferDurationMs /
    base::Time::kMillisecondsPerSecond;

class MockMediaStreamAudioSink final : public blink::WebMediaStreamAudioSink {
 public:
  MockMediaStreamAudioSink() : blink::WebMediaStreamAudioSink() {}
  ~MockMediaStreamAudioSink() override = default;

  MOCK_METHOD1(OnSetFormat, void(const media::AudioParameters& params));
  MOCK_METHOD2(OnData,
               void(const media::AudioBus& audio_bus,
                    base::TimeTicks estimated_capture_time));

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamAudioSink);
};

// This test needs to bundle together plenty of objects, namely:
// - a WebAudioSourceProviderImpl, which in turn needs an Audio Sink, in this
//  case a NullAudioSink. This is needed to plug HTMLAudioElementCapturerSource
//  and inject audio.
// - a MediaStreamSource, that owns the HTMLAudioElementCapturerSource under
//  test, and a MediaStreamComponent, that the class under test needs to
//  connect to in order to operate correctly. This class has an inner content
//  MediaStreamAudioTrack.
// - finally, a MockMediaStreamAudioSink to observe captured audio frames, and
//  that plugs into the former MediaStreamAudioTrack.
class HTMLAudioElementCapturerSourceTest : public testing::Test {
 public:
  HTMLAudioElementCapturerSourceTest()
      : fake_callback_(0.1, kAudioTrackSampleRate),
        audio_source_(new blink::WebAudioSourceProviderImpl(
            new media::NullAudioSink(
                blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
            &media_log_)) {}

  void SetUp() final {
    SetUpAudioTrack();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    media_stream_component_ = nullptr;
    media_stream_source_ = nullptr;
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  HtmlAudioElementCapturerSource* source() const {
    return static_cast<HtmlAudioElementCapturerSource*>(
        MediaStreamAudioSource::From(media_stream_source_.Get()));
  }

  blink::MediaStreamAudioTrack* track() const {
    return blink::MediaStreamAudioTrack::From(media_stream_component_);
  }

  int InjectAudio(media::AudioBus* audio_bus) {
    return audio_source_->RenderForTesting(audio_bus);
  }

 protected:
  void SetUpAudioTrack() {
    const media::AudioParameters params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::GuessChannelLayout(kNumChannelsForTest),
        kAudioTrackSampleRate /* sample_rate */,
        kAudioTrackSamplesPerBuffer /* frames_per_buffer */);
    audio_source_->Initialize(params, &fake_callback_);

    media_stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("audio_id"), MediaStreamSource::kTypeAudio,
        String::FromUTF8("audio_track"), false /* remote */);
    media_stream_component_ = MakeGarbageCollected<MediaStreamComponent>(
        media_stream_source_->Id(), media_stream_source_);

    // |media_stream_source_| takes wnership of
    // HtmlAudioElementCapturerSource.
    auto capture_source = std::make_unique<HtmlAudioElementCapturerSource>(
        audio_source_, blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    capture_source->SetOwner(media_stream_source_.Get());
    media_stream_source_->SetPlatformSource(std::move(capture_source));
    ASSERT_TRUE(source()->ConnectToTrack(media_stream_component_.Get()));
  }

  Persistent<MediaStreamSource> media_stream_source_;
  Persistent<MediaStreamComponent> media_stream_component_;

  media::NullMediaLog media_log_;
  media::FakeAudioRenderCallback fake_callback_;
  scoped_refptr<blink::WebAudioSourceProviderImpl> audio_source_;
};

// Constructs and destructs all objects. This is a non trivial sequence.
TEST_F(HTMLAudioElementCapturerSourceTest, ConstructAndDestruct) {}

// This test verifies that Audio can be properly captured when injected in the
// WebAudioSourceProviderImpl.
TEST_F(HTMLAudioElementCapturerSourceTest, CaptureAudio) {
  testing::InSequence s;

  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();

  MockMediaStreamAudioSink sink;
  track()->AddSink(&sink);
  EXPECT_CALL(sink, OnSetFormat(testing::_)).Times(1);
  EXPECT_CALL(sink, OnData(testing::AllOf(
                               testing::Property(&media::AudioBus::channels,
                                                 kNumChannelsForTest),
                               testing::Property(&media::AudioBus::frames,
                                                 kAudioTrackSamplesPerBuffer)),
                           testing::_))
      .Times(1)
      .WillOnce([&](const auto&, auto) { std::move(quit_closure).Run(); });

  std::unique_ptr<media::AudioBus> bus =
      media::AudioBus::Create(kNumChannelsForTest, kAudioTrackSamplesPerBuffer);
  InjectAudio(bus.get());
  run_loop.Run();

  track()->Stop();
  track()->RemoveSink(&sink);
}

// When a new source is created and started, it is stopped in the same task
// when cross-origin data is detected. This test checks that no data is
// delivered in this case.
TEST_F(HTMLAudioElementCapturerSourceTest,
       StartAndStopInSameTaskCapturesZeroFrames) {
  testing::InSequence s;

  // Stop the original track and start a new one so that it can be stopped in
  // in the same task.
  track()->Stop();
  base::RunLoop().RunUntilIdle();
  SetUpAudioTrack();

  MockMediaStreamAudioSink sink;
  track()->AddSink(&sink);
  EXPECT_CALL(sink, OnData(testing::AllOf(
                               testing::Property(&media::AudioBus::channels,
                                                 kNumChannelsForTest),
                               testing::Property(&media::AudioBus::frames,
                                                 kAudioTrackSamplesPerBuffer)),
                           testing::_))
      .Times(0);

  std::unique_ptr<media::AudioBus> bus =
      media::AudioBus::Create(kNumChannelsForTest, kAudioTrackSamplesPerBuffer);
  InjectAudio(bus.get());

  track()->Stop();
  base::RunLoop().RunUntilIdle();
  track()->RemoveSink(&sink);
}

}  // namespace blink
