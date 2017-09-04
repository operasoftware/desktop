/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CompositorAnimations_h
#define CompositorAnimations_h

#include <memory>
#include "core/CoreExport.h"
#include "core/animation/EffectModel.h"
#include "core/animation/Timing.h"
#include "platform/animation/TimingFunction.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Animation;
class CompositorAnimation;
class Element;
class FloatBox;
class KeyframeEffectModelBase;

class CORE_EXPORT CompositorAnimations {
  STATIC_ONLY(CompositorAnimations);

 public:
  static bool IsCompositableProperty(CSSPropertyID);
  static const CSSPropertyID kCompositableProperties[7];

  static bool IsCandidateForAnimationOnCompositor(
      const Timing&,
      const Element&,
      const Animation*,
      const EffectModel&,
      double animation_playback_rate);
  static void CancelIncompatibleAnimationsOnCompositor(const Element&,
                                                       const Animation&,
                                                       const EffectModel&);
  static bool CanStartAnimationOnCompositor(const Element&);
  static void StartAnimationOnCompositor(const Element&,
                                         int group,
                                         double start_time,
                                         double time_offset,
                                         const Timing&,
                                         const Animation&,
                                         const EffectModel&,
                                         Vector<int>& started_animation_ids,
                                         double animation_playback_rate);
  static void CancelAnimationOnCompositor(const Element&,
                                          const Animation&,
                                          int id);
  static void PauseAnimationForTestingOnCompositor(const Element&,
                                                   const Animation&,
                                                   int id,
                                                   double pause_time);

  static void AttachCompositedLayers(Element&, const Animation&);

  static bool GetAnimatedBoundingBox(FloatBox&,
                                     const EffectModel&,
                                     double min_value,
                                     double max_value);

  struct CompositorTiming {
    Timing::PlaybackDirection direction;
    double scaled_duration;
    double scaled_time_offset;
    double adjusted_iteration_count;
    double playback_rate;
    Timing::FillMode fill_mode;
    double iteration_start;
  };

  static bool ConvertTimingForCompositor(const Timing&,
                                         double time_offset,
                                         CompositorTiming& out,
                                         double animation_playback_rate);

  static void GetAnimationOnCompositor(
      const Timing&,
      int group,
      double start_time,
      double time_offset,
      const KeyframeEffectModelBase&,
      Vector<std::unique_ptr<CompositorAnimation>>& animations,
      double animation_playback_rate);
};

}  // namespace blink

#endif
