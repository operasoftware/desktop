// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/ConstantSourceNode.h"

#include <algorithm>
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/ConstantSourceOptions.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

ConstantSourceHandler::ConstantSourceHandler(AudioNode& node,
                                             float sample_rate,
                                             AudioParamHandler& offset)
    : AudioScheduledSourceHandler(kNodeTypeConstantSource, node, sample_rate),
      offset_(offset),
      sample_accurate_values_(AudioUtilities::kRenderQuantumFrames) {
  // A ConstantSource is always mono.
  AddOutput(1);

  Initialize();
}

PassRefPtr<ConstantSourceHandler> ConstantSourceHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& offset) {
  return AdoptRef(new ConstantSourceHandler(node, sample_rate, offset));
}

ConstantSourceHandler::~ConstantSourceHandler() {
  Uninitialize();
}

void ConstantSourceHandler::Process(size_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();
  DCHECK(output_bus);

  if (!IsInitialized() || !output_bus->NumberOfChannels()) {
    output_bus->Zero();
    return;
  }

  // The audio thread can't block on this lock, so we call tryLock() instead.
  MutexTryLocker try_locker(process_lock_);
  if (!try_locker.Locked()) {
    // Too bad - the tryLock() failed.
    output_bus->Zero();
    return;
  }

  size_t quantum_frame_offset;
  size_t non_silent_frames_to_process;
  double start_frame_offset;

  // Figure out where in the current rendering quantum that the source is
  // active and for how many frames.
  UpdateSchedulingInfo(frames_to_process, output_bus, quantum_frame_offset,
                       non_silent_frames_to_process, start_frame_offset);

  if (!non_silent_frames_to_process) {
    output_bus->Zero();
    return;
  }

  if (offset_->HasSampleAccurateValues()) {
    DCHECK_LE(frames_to_process, sample_accurate_values_.size());
    if (frames_to_process <= sample_accurate_values_.size()) {
      float* offsets = sample_accurate_values_.Data();
      offset_->CalculateSampleAccurateValues(offsets, frames_to_process);
      if (non_silent_frames_to_process > 0) {
        memcpy(output_bus->Channel(0)->MutableData() + quantum_frame_offset,
               offsets + quantum_frame_offset,
               non_silent_frames_to_process * sizeof(*offsets));
        output_bus->ClearSilentFlag();
      } else {
        output_bus->Zero();
      }
    }
  } else {
    float value = offset_->Value();

    if (value == 0) {
      output_bus->Zero();
    } else {
      float* dest = output_bus->Channel(0)->MutableData();
      dest += quantum_frame_offset;
      for (unsigned k = 0; k < non_silent_frames_to_process; ++k) {
        dest[k] = value;
      }
      output_bus->ClearSilentFlag();
    }
  }
}

bool ConstantSourceHandler::PropagatesSilence() const {
  return !IsPlayingOrScheduled() || HasFinished();
}

// ----------------------------------------------------------------
ConstantSourceNode::ConstantSourceNode(BaseAudioContext& context)
    : AudioScheduledSourceNode(context),
      offset_(AudioParam::Create(context, kParamTypeConstantSourceValue, 1)) {
  SetHandler(ConstantSourceHandler::Create(*this, context.sampleRate(),
                                           offset_->Handler()));
}

ConstantSourceNode* ConstantSourceNode::Create(
    BaseAudioContext& context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context.IsContextClosed()) {
    context.ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  return new ConstantSourceNode(context);
}

ConstantSourceNode* ConstantSourceNode::Create(
    BaseAudioContext* context,
    const ConstantSourceOptions& options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  ConstantSourceNode* node = Create(*context, exception_state);

  if (!node)
    return nullptr;

  node->offset()->setValue(options.offset());

  return node;
}

DEFINE_TRACE(ConstantSourceNode) {
  visitor->Trace(offset_);
  AudioScheduledSourceNode::Trace(visitor);
}

ConstantSourceHandler& ConstantSourceNode::GetConstantSourceHandler() const {
  return static_cast<ConstantSourceHandler&>(Handler());
}

AudioParam* ConstantSourceNode::offset() {
  return offset_;
}

}  // namespace blink
