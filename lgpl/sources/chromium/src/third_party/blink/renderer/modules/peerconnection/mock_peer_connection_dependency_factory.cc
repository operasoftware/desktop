// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"

#include <stddef.h>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"

using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackVector;
using webrtc::IceCandidateCollection;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::ObserverInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackSourceInterface;
using webrtc::VideoTrackVector;

namespace blink {

template <class V>
static typename V::iterator FindTrack(V* vector, const std::string& track_id) {
  auto it = vector->begin();
  for (; it != vector->end(); ++it) {
    if ((*it)->id() == track_id) {
      break;
    }
  }
  return it;
}

MockWebRtcAudioSource::MockWebRtcAudioSource(bool is_remote)
    : is_remote_(is_remote) {}
void MockWebRtcAudioSource::RegisterObserver(ObserverInterface* observer) {}
void MockWebRtcAudioSource::UnregisterObserver(ObserverInterface* observer) {}

MockWebRtcAudioSource::SourceState MockWebRtcAudioSource::state() const {
  return SourceState::kLive;
}

bool MockWebRtcAudioSource::remote() const {
  return is_remote_;
}

MockMediaStream::MockMediaStream(const std::string& id) : id_(id) {}

bool MockMediaStream::AddTrack(AudioTrackInterface* track) {
  audio_track_vector_.push_back(track);
  NotifyObservers();
  return true;
}

bool MockMediaStream::AddTrack(VideoTrackInterface* track) {
  video_track_vector_.push_back(track);
  NotifyObservers();
  return true;
}

bool MockMediaStream::RemoveTrack(AudioTrackInterface* track) {
  auto it = FindTrack(&audio_track_vector_, track->id());
  if (it == audio_track_vector_.end())
    return false;
  audio_track_vector_.erase(it);
  NotifyObservers();
  return true;
}

bool MockMediaStream::RemoveTrack(VideoTrackInterface* track) {
  auto it = FindTrack(&video_track_vector_, track->id());
  if (it == video_track_vector_.end())
    return false;
  video_track_vector_.erase(it);
  NotifyObservers();
  return true;
}

std::string MockMediaStream::id() const {
  return id_;
}

AudioTrackVector MockMediaStream::GetAudioTracks() {
  return audio_track_vector_;
}

VideoTrackVector MockMediaStream::GetVideoTracks() {
  return video_track_vector_;
}

rtc::scoped_refptr<AudioTrackInterface> MockMediaStream::FindAudioTrack(
    const std::string& track_id) {
  auto it = FindTrack(&audio_track_vector_, track_id);
  return it == audio_track_vector_.end() ? nullptr : *it;
}

rtc::scoped_refptr<VideoTrackInterface> MockMediaStream::FindVideoTrack(
    const std::string& track_id) {
  auto it = FindTrack(&video_track_vector_, track_id);
  return it == video_track_vector_.end() ? nullptr : *it;
}

void MockMediaStream::RegisterObserver(ObserverInterface* observer) {
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void MockMediaStream::UnregisterObserver(ObserverInterface* observer) {
  auto it = observers_.find(observer);
  DCHECK(it != observers_.end());
  observers_.erase(it);
}

void MockMediaStream::NotifyObservers() {
  for (auto it = observers_.begin(); it != observers_.end(); ++it) {
    (*it)->OnChanged();
  }
}

MockMediaStream::~MockMediaStream() {}

scoped_refptr<MockWebRtcAudioTrack> MockWebRtcAudioTrack::Create(
    const std::string& id) {
  return new rtc::RefCountedObject<MockWebRtcAudioTrack>(id);
}

MockWebRtcAudioTrack::MockWebRtcAudioTrack(const std::string& id)
    : id_(id),
      source_(new rtc::RefCountedObject<MockWebRtcAudioSource>(true)),
      enabled_(true),
      state_(webrtc::MediaStreamTrackInterface::kLive) {}

MockWebRtcAudioTrack::~MockWebRtcAudioTrack() {}

std::string MockWebRtcAudioTrack::kind() const {
  return kAudioKind;
}

webrtc::AudioSourceInterface* MockWebRtcAudioTrack::GetSource() const {
  return source_.get();
}

std::string MockWebRtcAudioTrack::id() const {
  return id_;
}

bool MockWebRtcAudioTrack::enabled() const {
  return enabled_;
}

MockWebRtcVideoTrack::TrackState MockWebRtcAudioTrack::state() const {
  return state_;
}

bool MockWebRtcAudioTrack::set_enabled(bool enable) {
  enabled_ = enable;
  return true;
}

void MockWebRtcAudioTrack::RegisterObserver(ObserverInterface* observer) {
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void MockWebRtcAudioTrack::UnregisterObserver(ObserverInterface* observer) {
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

void MockWebRtcAudioTrack::SetEnded() {
  DCHECK_EQ(webrtc::MediaStreamTrackInterface::kLive, state_);
  state_ = webrtc::MediaStreamTrackInterface::kEnded;
  for (auto* o : observers_)
    o->OnChanged();
}

MockWebRtcVideoTrack::MockWebRtcVideoTrack(
    const std::string& id,
    webrtc::VideoTrackSourceInterface* source)
    : id_(id),
      source_(source),
      enabled_(true),
      state_(webrtc::MediaStreamTrackInterface::kLive),
      sink_(nullptr) {}

MockWebRtcVideoTrack::~MockWebRtcVideoTrack() {}

scoped_refptr<MockWebRtcVideoTrack> MockWebRtcVideoTrack::Create(
    const std::string& id,
    scoped_refptr<webrtc::VideoTrackSourceInterface> source) {
  return new rtc::RefCountedObject<MockWebRtcVideoTrack>(id, source.get());
}

void MockWebRtcVideoTrack::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  DCHECK(!sink_);
  sink_ = sink;
}

void MockWebRtcVideoTrack::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  DCHECK(sink_ == sink);
  sink_ = nullptr;
}

VideoTrackSourceInterface* MockWebRtcVideoTrack::GetSource() const {
  return source_.get();
}

std::string MockWebRtcVideoTrack::kind() const {
  return kVideoKind;
}

std::string MockWebRtcVideoTrack::id() const {
  return id_;
}

bool MockWebRtcVideoTrack::enabled() const {
  return enabled_;
}

MockWebRtcVideoTrack::TrackState MockWebRtcVideoTrack::state() const {
  return state_;
}

bool MockWebRtcVideoTrack::set_enabled(bool enable) {
  enabled_ = enable;
  return true;
}

void MockWebRtcVideoTrack::RegisterObserver(ObserverInterface* observer) {
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void MockWebRtcVideoTrack::UnregisterObserver(ObserverInterface* observer) {
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

void MockWebRtcVideoTrack::SetEnded() {
  DCHECK_EQ(webrtc::MediaStreamTrackInterface::kLive, state_);
  state_ = webrtc::MediaStreamTrackInterface::kEnded;
  for (auto* o : observers_)
    o->OnChanged();
}

scoped_refptr<MockWebRtcVideoTrackSource> MockWebRtcVideoTrackSource::Create(
    bool supports_encoded_output) {
  return new rtc::RefCountedObject<MockWebRtcVideoTrackSource>(
      supports_encoded_output);
}

MockWebRtcVideoTrackSource::MockWebRtcVideoTrackSource(
    bool supports_encoded_output)
    : supports_encoded_output_(supports_encoded_output) {}

bool MockWebRtcVideoTrackSource::is_screencast() const {
  return false;
}

absl::optional<bool> MockWebRtcVideoTrackSource::needs_denoising() const {
  return absl::nullopt;
}

bool MockWebRtcVideoTrackSource::GetStats(Stats* stats) {
  return false;
}

bool MockWebRtcVideoTrackSource::SupportsEncodedOutput() const {
  return supports_encoded_output_;
}

void MockWebRtcVideoTrackSource::GenerateKeyFrame() {}

void MockWebRtcVideoTrackSource::AddEncodedSink(
    rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) {}

void MockWebRtcVideoTrackSource::RemoveEncodedSink(
    rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) {}

void MockWebRtcVideoTrackSource::RegisterObserver(
    webrtc::ObserverInterface* observer) {}

void MockWebRtcVideoTrackSource::UnregisterObserver(
    webrtc::ObserverInterface* observer) {}

webrtc::MediaSourceInterface::SourceState MockWebRtcVideoTrackSource::state()
    const {
  return webrtc::MediaSourceInterface::kLive;
}

bool MockWebRtcVideoTrackSource::remote() const {
  return supports_encoded_output_;
}

void MockWebRtcVideoTrackSource::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {}

void MockWebRtcVideoTrackSource::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {}

class MockSessionDescription : public SessionDescriptionInterface {
 public:
  MockSessionDescription(const std::string& type, const std::string& sdp)
      : type_(type), sdp_(sdp) {}
  ~MockSessionDescription() override {}
  cricket::SessionDescription* description() override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  const cricket::SessionDescription* description() const override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  std::string session_id() const override {
    NOTIMPLEMENTED();
    return std::string();
  }
  std::string session_version() const override {
    NOTIMPLEMENTED();
    return std::string();
  }
  std::string type() const override { return type_; }
  bool AddCandidate(const IceCandidateInterface* candidate) override {
    NOTIMPLEMENTED();
    return false;
  }
  size_t number_of_mediasections() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  const IceCandidateCollection* candidates(
      size_t mediasection_index) const override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  bool ToString(std::string* out) const override {
    *out = sdp_;
    return true;
  }

 private:
  std::string type_;
  std::string sdp_;
};

class MockIceCandidate : public IceCandidateInterface {
 public:
  MockIceCandidate(const std::string& sdp_mid,
                   int sdp_mline_index,
                   const std::string& sdp)
      : sdp_mid_(sdp_mid), sdp_mline_index_(sdp_mline_index), sdp_(sdp) {
    // Assign an valid address to |candidate_| to pass assert in code.
    candidate_.set_address(rtc::SocketAddress("127.0.0.1", 5000));
  }
  ~MockIceCandidate() override {}
  std::string sdp_mid() const override { return sdp_mid_; }
  int sdp_mline_index() const override { return sdp_mline_index_; }
  const cricket::Candidate& candidate() const override { return candidate_; }
  bool ToString(std::string* out) const override {
    *out = sdp_;
    return true;
  }

 private:
  std::string sdp_mid_;
  int sdp_mline_index_;
  std::string sdp_;
  cricket::Candidate candidate_;
};

MockPeerConnectionDependencyFactory::MockPeerConnectionDependencyFactory()
    : blink::PeerConnectionDependencyFactory(
          /*create_p2p_socket_dispatcher =*/false),
      signaling_thread_("MockPCFactory WebRtc Signaling Thread") {
  EnsureWebRtcAudioDeviceImpl();
  CHECK(signaling_thread_.Start());
}

MockPeerConnectionDependencyFactory::~MockPeerConnectionDependencyFactory() {}

scoped_refptr<webrtc::PeerConnectionInterface>
MockPeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    blink::WebLocalFrame* frame,
    webrtc::PeerConnectionObserver* observer) {
  return new rtc::RefCountedObject<MockPeerConnectionImpl>(this, observer);
}

scoped_refptr<webrtc::VideoTrackSourceInterface>
MockPeerConnectionDependencyFactory::CreateVideoTrackSourceProxy(
    webrtc::VideoTrackSourceInterface* source) {
  return nullptr;
}
scoped_refptr<webrtc::MediaStreamInterface>
MockPeerConnectionDependencyFactory::CreateLocalMediaStream(
    const String& label) {
  return new rtc::RefCountedObject<MockMediaStream>(label.Utf8());
}

scoped_refptr<webrtc::VideoTrackInterface>
MockPeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const String& id,
    webrtc::VideoTrackSourceInterface* source) {
  scoped_refptr<webrtc::VideoTrackInterface> track(
      new rtc::RefCountedObject<MockWebRtcVideoTrack>(id.Utf8(), source));
  return track;
}

SessionDescriptionInterface*
MockPeerConnectionDependencyFactory::CreateSessionDescription(
    const String& type,
    const String& sdp,
    webrtc::SdpParseError* error) {
  if (fail_to_create_session_description_)
    return nullptr;
  return new MockSessionDescription(type.Utf8(), sdp.Utf8());
}

webrtc::IceCandidateInterface*
MockPeerConnectionDependencyFactory::CreateIceCandidate(const String& sdp_mid,
                                                        int sdp_mline_index,
                                                        const String& sdp) {
  return new MockIceCandidate(sdp_mid.Utf8(), sdp_mline_index, sdp.Utf8());
}

scoped_refptr<base::SingleThreadTaskRunner>
MockPeerConnectionDependencyFactory::GetWebRtcSignalingTaskRunner() {
  return signaling_thread_.task_runner();
}

void MockPeerConnectionDependencyFactory::SetFailToCreateSessionDescription(
    bool fail) {
  fail_to_create_session_description_ = fail;
}

}  // namespace blink
