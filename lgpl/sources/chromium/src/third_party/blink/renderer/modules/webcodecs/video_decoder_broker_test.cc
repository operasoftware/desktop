// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/decode_status.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/fake_video_decoder.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/interface_factory_impl.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_video_decoder_service.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#include "third_party/blink/renderer/modules/webcodecs/video_decoder_broker.h"

using ::testing::_;
using ::testing::Return;

namespace blink {

namespace {

// Fake decoder intended to simulate platform specific hw accelerated decoders
// running in the GPU process.
// * Initialize() will succeed for any given config.
// * MakeVideoFrame() is overridden to create frames frame with a mailbox and
//   power_efficient flag. This simulates hw decoder output and satisfies
//   requirements of MojoVideoDecoder.
class FakeGpuVideoDecoder : public media::FakeVideoDecoder {
 public:
  FakeGpuVideoDecoder()
      : FakeVideoDecoder("FakeGpuVideoDecoder" /* display_name */,
                         0 /* decoding_delay */,
                         13 /* max_parallel_decoding_requests */,
                         media::BytesDecodedCB()) {}
  ~FakeGpuVideoDecoder() override = default;

  scoped_refptr<media::VideoFrame> MakeVideoFrame(
      const media::DecoderBuffer& buffer) override {
    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
    mailbox_holders[0].mailbox.name[0] = 1;
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapNativeTextures(
            media::PIXEL_FORMAT_ARGB, mailbox_holders,
            media::VideoFrame::ReleaseMailboxCB(), current_config_.coded_size(),
            current_config_.visible_rect(), current_config_.natural_size(),
            buffer.timestamp());
    frame->metadata()->power_efficient = true;
    return frame;
  }

  // Override these methods to provide non-default values for testing.
  bool IsPlatformDecoder() const override { return true; }
  bool NeedsBitstreamConversion() const override { return true; }
  bool CanReadWithoutStalling() const override { return false; }
};

// Client to MojoVideoDecoderService vended by FakeInterfaceFactory. Creates a
// FakeGpuVideoDecoder when requested.
class FakeMojoMediaClient : public media::MojoMediaClient {
 public:
  // MojoMediaClient implementation.
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      media::MediaLog* media_log,
      media::mojom::CommandBufferIdPtr command_buffer_id,
      media::VideoDecoderImplementation implementation,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override {
    return std::make_unique<FakeGpuVideoDecoder>();
  }
};

// Other end of remote InterfaceFactory requested by VideoDecoderBroker. Used
// to create our (fake) media::mojom::VideoDecoder.
class FakeInterfaceFactory : public media::mojom::InterfaceFactory {
 public:
  FakeInterfaceFactory() = default;
  ~FakeInterfaceFactory() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<media::mojom::InterfaceFactory>(
        std::move(handle)));
    receiver_.set_disconnect_handler(WTF::Bind(
        &FakeInterfaceFactory::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // Implement this one interface from mojom::InterfaceFactory. Using the real
  // MojoVideoDecoderService allows us to reuse buffer conversion code. The
  // FakeMojoMediaClient will create a FakeGpuVideoDecoder.
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) override {
    video_decoder_receivers_.Add(
        std::make_unique<media::MojoVideoDecoderService>(&mojo_media_client_,
                                                         &cdm_service_context_),
        std::move(receiver));
  }

  // Stub out other mojom::InterfaceFactory interfaces.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) override {}
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#endif
#if defined(OS_ANDROID)
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) override {}
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#endif  // defined(OS_ANDROID
  void CreateCdm(const std::string& key_system,
                 mojo::PendingReceiver<media::mojom::ContentDecryptionModule>
                     receiver) override {}

 private:
  media::MojoCdmServiceContext cdm_service_context_;
  FakeMojoMediaClient mojo_media_client_;
  mojo::Receiver<media::mojom::InterfaceFactory> receiver_{this};
  mojo::UniqueReceiverSet<media::mojom::VideoDecoder> video_decoder_receivers_;
};

}  // namespace

class VideoDecoderBrokerTest : public testing::Test {
 public:
  VideoDecoderBrokerTest() = default;
  ~VideoDecoderBrokerTest() override {
    if (media_thread_)
      media_thread_->Stop();
  }

  void OnInitWithClosure(base::RepeatingClosure done_cb, media::Status status) {
    OnInit(status);
    done_cb.Run();
  }
  void OnDecodeDoneWithClosure(base::RepeatingClosure done_cb,
                               media::DecodeStatus status) {
    OnDecodeDone(status);
    done_cb.Run();
  }

  void OnResetDoneWithClosure(base::RepeatingClosure done_cb) {
    OnResetDone();
    done_cb.Run();
  }

  MOCK_METHOD1(OnInit, void(media::Status status));
  MOCK_METHOD1(OnDecodeDone, void(media::DecodeStatus));
  MOCK_METHOD0(OnResetDone, void());

  void OnOutput(scoped_refptr<media::VideoFrame> frame) {
    output_frames_.push_back(std::move(frame));
  }

  void SetupMojo(ExecutionContext& execution_context) {
    // Register FakeInterfaceFactory as impl for media::mojom::InterfaceFactory
    // required by MojoVideoDecoder. The factory will vend FakeGpuVideoDecoders
    // that simulate gpu-accelerated decode.
    interface_factory_ = std::make_unique<FakeInterfaceFactory>();
    EXPECT_TRUE(
        execution_context.GetBrowserInterfaceBroker().SetBinderForTesting(
            media::mojom::InterfaceFactory::Name_,
            WTF::BindRepeating(&FakeInterfaceFactory::BindRequest,
                               base::Unretained(interface_factory_.get()))));

    // |gpu_factories_| requires API calls be made using it's GetTaskRunner().
    // We use a separate |media_thread_| (as opposed to a separate task runner
    // on the main thread) to simulate cross-thread production behavior.
    media_thread_ = std::make_unique<base::Thread>("media_thread");
    media_thread_->Start();

    // |gpu_factories_| is a dependency of MojoVideoDecoder (and associated code
    // paths). Setup |gpu_factories_| to say "yes" to any decoder config to
    // ensure MojoVideoDecoder will be selected as the underlying decoder upon
    // VideoDecoderBroker::Initialize(). The
    gpu_factories_ =
        std::make_unique<media::MockGpuVideoAcceleratorFactories>(nullptr);
    EXPECT_CALL(*gpu_factories_, GetTaskRunner())
        .WillRepeatedly(Return(media_thread_->task_runner()));
    EXPECT_CALL(*gpu_factories_, IsDecoderConfigSupported(_, _))
        .WillRepeatedly(
            Return(media::GpuVideoAcceleratorFactories::Supported::kTrue));
  }

  void ConstructDecoder(ExecutionContext& execution_context) {
    decoder_broker_ = std::make_unique<VideoDecoderBroker>(
        execution_context, gpu_factories_.get());
  }

  void InitializeDecoder(media::VideoDecoderConfig config) {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnInit(media::SameStatusCode(media::OkStatus())));
    decoder_broker_->Initialize(
        config, false /*low_delay*/, nullptr /* cdm_context */,
        WTF::Bind(&VideoDecoderBrokerTest::OnInitWithClosure,
                  WTF::Unretained(this), run_loop.QuitClosure()),
        WTF::BindRepeating(&VideoDecoderBrokerTest::OnOutput,
                           WTF::Unretained(this)),
        media::WaitingCB());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void DecodeBuffer(
      scoped_refptr<media::DecoderBuffer> buffer,
      media::DecodeStatus expected_status = media::DecodeStatus::OK) {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnDecodeDone(expected_status));
    decoder_broker_->Decode(
        buffer, WTF::Bind(&VideoDecoderBrokerTest::OnDecodeDoneWithClosure,
                          WTF::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void ResetDecoder() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnResetDone());
    decoder_broker_->Reset(
        WTF::Bind(&VideoDecoderBrokerTest::OnResetDoneWithClosure,
                  WTF::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

  std::string GetDisplayName() { return decoder_broker_->GetDisplayName(); }

  bool IsPlatformDecoder() { return decoder_broker_->IsPlatformDecoder(); }

  bool NeedsBitstreamConversion() {
    return decoder_broker_->NeedsBitstreamConversion();
  }

  bool CanReadWithoutStalling() {
    return decoder_broker_->CanReadWithoutStalling();
  }

  int GetMaxDecodeRequests() { return decoder_broker_->GetMaxDecodeRequests(); }

 protected:
  std::unique_ptr<VideoDecoderBroker> decoder_broker_;
  std::vector<scoped_refptr<media::VideoFrame>> output_frames_;
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> gpu_factories_;
  std::unique_ptr<FakeInterfaceFactory> interface_factory_;
  std::unique_ptr<base::Thread> media_thread_;
};

TEST_F(VideoDecoderBrokerTest, Decode_Uninitialized) {
  V8TestingScope v8_scope;

  ConstructDecoder(*v8_scope.GetExecutionContext());
  EXPECT_EQ(GetDisplayName(), "EmptyWebCodecsVideoDecoder");

  // No call to Initialize. Other APIs should fail gracefully.

  DecodeBuffer(media::ReadTestDataFile("vp8-I-frame-320x120"),
               media::DecodeStatus::DECODE_ERROR);
  DecodeBuffer(media::DecoderBuffer::CreateEOSBuffer(),
               media::DecodeStatus::DECODE_ERROR);
  ASSERT_EQ(0U, output_frames_.size());

  ResetDecoder();
}

TEST_F(VideoDecoderBrokerTest, Decode_NoMojoDecoder) {
  V8TestingScope v8_scope;

  ConstructDecoder(*v8_scope.GetExecutionContext());
  EXPECT_EQ(GetDisplayName(), "EmptyWebCodecsVideoDecoder");

  InitializeDecoder(media::TestVideoConfig::Normal());
  EXPECT_NE(GetDisplayName(), "EmptyWebCodecsVideoDecoder");

  DecodeBuffer(media::ReadTestDataFile("vp8-I-frame-320x120"));
  DecodeBuffer(media::DecoderBuffer::CreateEOSBuffer());
  ASSERT_EQ(1U, output_frames_.size());

  ResetDecoder();

  DecodeBuffer(media::ReadTestDataFile("vp8-I-frame-320x120"));
  DecodeBuffer(media::DecoderBuffer::CreateEOSBuffer());
  ASSERT_EQ(2U, output_frames_.size());

  ResetDecoder();
}

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
TEST_F(VideoDecoderBrokerTest, Decode_WithMojoDecoder) {
  V8TestingScope v8_scope;
  ExecutionContext* execution_context = v8_scope.GetExecutionContext();

  SetupMojo(*execution_context);
  ConstructDecoder(*execution_context);
  EXPECT_EQ(GetDisplayName(), "EmptyWebCodecsVideoDecoder");

  media::VideoDecoderConfig config = media::TestVideoConfig::Normal();
  InitializeDecoder(config);
  EXPECT_EQ(GetDisplayName(), "MojoVideoDecoder");

  DecodeBuffer(media::CreateFakeVideoBufferForTest(
      config, base::TimeDelta(), base::TimeDelta::FromMilliseconds(33)));
  DecodeBuffer(media::DecoderBuffer::CreateEOSBuffer());
  ASSERT_EQ(1U, output_frames_.size());

  // Backing FakeVideoDecoder will return interesting values for these APIs.
  EXPECT_TRUE(IsPlatformDecoder());
  EXPECT_TRUE(NeedsBitstreamConversion());
  EXPECT_FALSE(CanReadWithoutStalling());
  EXPECT_EQ(GetMaxDecodeRequests(), 13);

  ResetDecoder();
}
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

}  // namespace blink
