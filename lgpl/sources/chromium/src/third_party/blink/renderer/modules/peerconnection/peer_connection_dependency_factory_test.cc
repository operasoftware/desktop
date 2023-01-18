// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/features/scoped_test_feature_override.h"
#include "base/features/submodule_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"

namespace blink {

class PeerConnectionDependencyFactoryTest
    : public ::testing::TestWithParam<bool> {
 public:
  void EnsureDependencyFactory(ExecutionContext& context) {
    dependency_factory_ = &PeerConnectionDependencyFactory::From(context);
    ASSERT_TRUE(dependency_factory_);
  }

  std::unique_ptr<RTCPeerConnectionHandler> CreateRTCPeerConnectionHandler() {
    std::unique_ptr<RTCPeerConnectionHandler> handler =
        dependency_factory_->CreateRTCPeerConnectionHandler(
            &mock_client_,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*encoded_insertable_streams=*/false);
    DummyExceptionStateForTesting exception_state;
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    handler->InitializeForTest(config,
                               /*peer_connection_tracker=*/nullptr,
                               exception_state);
    return handler;
  }

 protected:
  base::ScopedTestFeatureOverride enable_external_openh264_{
      base::kFeatureExternalOpenH264Encoder, GetParam()};
  base::ScopedTestFeatureOverride enable_platform_sw_codec_mac_{
      base::kFeaturePlatformSWH264EncoderDecoderWebRTCMac, GetParam()};
  base::ScopedTestFeatureOverride enable_platform_sw_codec_win_{
      base::kFeaturePlatformSWH264EncoderDecoderWebRTCWin, GetParam()};
  Persistent<PeerConnectionDependencyFactory> dependency_factory_;
  MockRTCPeerConnectionHandlerClient mock_client_;
};

INSTANTIATE_TEST_CASE_P(, PeerConnectionDependencyFactoryTest, testing::Bool());

TEST_P(PeerConnectionDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  V8TestingScope scope;
  EnsureDependencyFactory(*scope.GetExecutionContext());

  std::unique_ptr<RTCPeerConnectionHandler> pc_handler =
      CreateRTCPeerConnectionHandler();
  EXPECT_TRUE(pc_handler);
}

}  // namespace blink
