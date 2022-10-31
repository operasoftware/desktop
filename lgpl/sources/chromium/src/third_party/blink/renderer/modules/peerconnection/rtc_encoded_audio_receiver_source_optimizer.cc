#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_receiver_source_optimizer.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

RtcEncodedAudioReceiverSourceOptimizer::RtcEncodedAudioReceiverSourceOptimizer(
    UnderlyingSourceSetter set_underlying_source,
    WTF::CrossThreadOnceClosure disconnect_callback)
    : set_underlying_source_(std::move(set_underlying_source)),
      disconnect_callback_(std::move(disconnect_callback)) {}

UnderlyingSourceBase*
RtcEncodedAudioReceiverSourceOptimizer::PerformInProcessOptimization(
    ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  scoped_refptr<base::SingleThreadTaskRunner> current_runner =
      context->GetTaskRunner(TaskType::kInternalMediaRealTime);

  auto* new_source = MakeGarbageCollected<RTCEncodedAudioUnderlyingSource>(
      script_state, std::move(disconnect_callback_),
      /*is_receiver=*/true);

  set_underlying_source_.Run(WrapCrossThreadPersistent(new_source),
                             std::move(current_runner));

  return new_source;
}

}  // namespace blink
