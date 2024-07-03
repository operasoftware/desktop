/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/async_audio_decoder.h"

#include <memory>

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_log.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
#include "media/base/media_util.h"
#include "media/mojo/clients/mojo_audio_decoder.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)

namespace blink {

void AsyncAudioDecoder::DecodeAsync(
    DOMArrayBuffer* audio_data,
    float sample_rate,
    V8DecodeSuccessCallback* success_callback,
    V8DecodeErrorCallback* error_callback,
    ScriptPromiseResolver<AudioBuffer>* resolver,
    BaseAudioContext* context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DCHECK(audio_data);

  scoped_refptr<base::SequencedTaskRunner> external_decoder_task_runner =
      Platform::Current()->MediaThreadTaskRunner();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context->GetExecutionContext()->GetTaskRunner(
          blink::TaskType::kInternalMedia);

  PostCrossThreadTask(
      *external_decoder_task_runner, FROM_HERE,
      CrossThreadBindOnce(&AsyncAudioDecoder::CreateAudioDecoder,
                          WrapCrossThreadPersistent(audio_data), sample_rate,
                          MakeCrossThreadHandle(success_callback),
                          MakeCrossThreadHandle(error_callback),
                          MakeCrossThreadHandle(resolver),
                          MakeCrossThreadHandle(context),
                          external_decoder_task_runner, std::move(task_runner),
                          exception_state.GetContext()));
}

void AsyncAudioDecoder::CreateAudioDecoder(
    DOMArrayBuffer* audio_data,
    float sample_rate,
    CrossThreadHandle<V8DecodeSuccessCallback> success_callback,
    CrossThreadHandle<V8DecodeErrorCallback> error_callback,
    CrossThreadHandle<ScriptPromiseResolver<AudioBuffer>> resolver,
    CrossThreadHandle<BaseAudioContext> context,
    scoped_refptr<base::SequencedTaskRunner> external_decoder_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const ExceptionContext& exception_context) {
  DCHECK(!IsMainThread());

  std::unique_ptr<media::MediaLog> media_log;
  std::unique_ptr<media::AudioDecoder> external_decoder;
#if BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)
  media_log = std::make_unique<media::NullMediaLog>();
  mojo::PendingRemote<media::mojom::InterfaceFactory> pending_interface_factory;
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      pending_interface_factory.InitWithNewPipeAndPassReceiver());
  interface_factory.Bind(std::move(pending_interface_factory));

  mojo::PendingRemote<media::mojom::AudioDecoder> decoder_remote;
  interface_factory->CreateAudioDecoder(
      decoder_remote.InitWithNewPipeAndPassReceiver());
  external_decoder = std::make_unique<media::MojoAudioDecoder>(
      external_decoder_task_runner, media_log.get(), std::move(decoder_remote));
#endif  // BUILDFLAG(USE_SYSTEM_PROPRIETARY_CODECS)

  // ArrayBufferContents is a thread-safe smart pointer around the backing
  // store.
  ArrayBufferContents audio_data_contents = *audio_data->Content();

  worker_pool::PostTask(
      FROM_HERE,
      CrossThreadBindOnce(
          &AsyncAudioDecoder::DecodeOnBackgroundThread,
          std::move(audio_data_contents), sample_rate,
          std::move(success_callback), std::move(error_callback),
          std::move(resolver), std::move(context), std::move(media_log),
          std::move(external_decoder), std::move(external_decoder_task_runner),
          std::move(task_runner), exception_context));
}

void AsyncAudioDecoder::DecodeOnBackgroundThread(
    ArrayBufferContents audio_data_contents,
    float sample_rate,
    CrossThreadHandle<V8DecodeSuccessCallback> success_callback,
    CrossThreadHandle<V8DecodeErrorCallback> error_callback,
    CrossThreadHandle<ScriptPromiseResolver<AudioBuffer>> resolver,
    CrossThreadHandle<BaseAudioContext> context,
    std::unique_ptr<media::MediaLog> media_log,
    std::unique_ptr<media::AudioDecoder> external_decoder,
    scoped_refptr<base::SequencedTaskRunner> external_decoder_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const ExceptionContext& exception_context) {
  DCHECK(!IsMainThread());
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      audio_data_contents.Data(), audio_data_contents.DataLength(), false,
      sample_rate, external_decoder.get(), external_decoder_task_runner.get());

  external_decoder_task_runner->DeleteSoon(FROM_HERE,
                                           std::move(external_decoder));

  // A reference to `bus` is retained by base::OnceCallback and will be removed
  // after `NotifyComplete()` is done.
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &AsyncAudioDecoder::NotifyComplete, std::move(audio_data_contents),
          MakeUnwrappingCrossThreadHandle(success_callback),
          MakeUnwrappingCrossThreadHandle(error_callback),
          WTF::RetainedRef(std::move(bus)),
          MakeUnwrappingCrossThreadHandle(resolver),
          MakeUnwrappingCrossThreadHandle(context), exception_context));
}

void AsyncAudioDecoder::NotifyComplete(
    ArrayBufferContents,
    V8DecodeSuccessCallback* success_callback,
    V8DecodeErrorCallback* error_callback,
    AudioBus* audio_bus,
    ScriptPromiseResolver<AudioBuffer>* resolver,
    BaseAudioContext* context,
    const ExceptionContext& exception_context) {
  DCHECK(IsMainThread());

  AudioBuffer* audio_buffer = AudioBuffer::CreateFromAudioBus(audio_bus);

  // If the context is available, let the context finish the notification.
  if (context) {
    context->HandleDecodeAudioData(audio_buffer, resolver, success_callback,
                                   error_callback, exception_context);
  }
}

}  // namespace blink
