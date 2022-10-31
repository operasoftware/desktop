/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/script/script_runner.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace {

void PostTaskWithLowPriorityUntilTimeout(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta timeout,
    scoped_refptr<base::SingleThreadTaskRunner> lower_priority_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> normal_priority_task_runner) {
  using RefCountedOnceClosure = base::RefCountedData<base::OnceClosure>;
  scoped_refptr<RefCountedOnceClosure> ref_counted_task =
      base::MakeRefCounted<RefCountedOnceClosure>(std::move(task));

  // |run_task_once| runs on both of |lower_priority_task_runner| and
  // |normal_priority_task_runner|. |run_task_once| guarantees that the given
  // |task| doesn't run more than once. |task| runs on either of
  // |lower_priority_task_runner| and |normal_priority_task_runner| whichever
  // comes first.
  auto run_task_once =
      [](scoped_refptr<RefCountedOnceClosure> ref_counted_task) {
        if (!ref_counted_task->data.is_null())
          std::move(ref_counted_task->data).Run();
      };

  lower_priority_task_runner->PostTask(
      from_here, WTF::Bind(run_task_once, ref_counted_task));

  normal_priority_task_runner->PostDelayedTask(
      from_here, WTF::Bind(run_task_once, ref_counted_task), timeout);
}

}  // namespace

namespace blink {

void PostTaskWithLowPriorityUntilTimeoutForTesting(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta timeout,
    scoped_refptr<base::SingleThreadTaskRunner> lower_priority_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> normal_priority_task_runner) {
  PostTaskWithLowPriorityUntilTimeout(from_here, std::move(task), timeout,
                                      std::move(lower_priority_task_runner),
                                      std::move(normal_priority_task_runner));
}

ScriptRunner::ScriptRunner(Document* document)
    : document_(document),
      task_runner_(document->GetTaskRunner(TaskType::kNetworking)),
      low_priority_task_runner_(
          document->GetTaskRunner(TaskType::kLowPriorityScriptExecution)) {
  DCHECK(document);
}

ScriptRunner::DelayReasons ScriptRunner::DetermineDelayReasonsToWait(
    PendingScript* pending_script) {
  DelayReasons reasons = static_cast<DelayReasons>(DelayReason::kLoad);

  if (pending_script->IsEligibleForDelay() &&
      (active_delay_reasons_ &
       static_cast<DelayReasons>(DelayReason::kMilestone))) {
    reasons |= static_cast<DelayReasons>(DelayReason::kMilestone);
  }

  if (base::FeatureList::IsEnabled(features::kForceDeferScriptIntervention)) {
    if (active_delay_reasons_ &
        static_cast<DelayReasons>(DelayReason::kForceDefer)) {
      reasons |= static_cast<DelayReasons>(DelayReason::kForceDefer);
    }
  }

  return reasons;
}

void ScriptRunner::QueueScriptForExecution(
    PendingScript* pending_script,
    absl::optional<DelayReasons> delay_reasons_override_for_test) {
  DCHECK(pending_script);
  document_->IncrementLoadEventDelayCount();
  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      pending_async_scripts_.insert(
          pending_script, delay_reasons_override_for_test
                              ? *delay_reasons_override_for_test
                              : DetermineDelayReasonsToWait(pending_script));
      break;

    case ScriptSchedulingType::kInOrder:
      pending_in_order_scripts_.push_back(pending_script);
      break;

    case ScriptSchedulingType::kForceInOrder:
      pending_force_in_order_scripts_.push_back(pending_script);
      pending_force_in_order_scripts_count_ += 1;
      break;

    default:
      NOTREACHED();
      break;
  }

  // Note that WatchForLoad() can immediately call PendingScriptFinished().
  pending_script->WatchForLoad(this);
}

void ScriptRunner::AddDelayReason(DelayReason delay_reason) {
  DCHECK(!(active_delay_reasons_ & static_cast<DelayReasons>(delay_reason)));
  active_delay_reasons_ |= static_cast<DelayReasons>(delay_reason);
}

void ScriptRunner::RemoveDelayReason(DelayReason delay_reason) {
  DCHECK(active_delay_reasons_ & static_cast<DelayReasons>(delay_reason));
  active_delay_reasons_ &= ~static_cast<DelayReasons>(delay_reason);

  HeapVector<Member<PendingScript>> pending_async_scripts;
  CopyKeysToVector(pending_async_scripts_, pending_async_scripts);
  for (PendingScript* pending_script : pending_async_scripts) {
    RemoveDelayReasonFromScript(pending_script, delay_reason);
  }
}

void ScriptRunner::RemoveDelayReasonFromScript(PendingScript* pending_script,
                                               DelayReason delay_reason) {
  auto it = pending_async_scripts_.find(pending_script);

  if (it == pending_async_scripts_.end())
    return;

  if (it->value &= ~static_cast<DelayReasons>(delay_reason)) {
    // Still to be delayed.
    return;
  }

  // Script is really ready to evaluate.
  pending_async_scripts_.erase(it);
  base::OnceClosure task =
      WTF::Bind(&ScriptRunner::ExecutePendingScript, WrapWeakPersistent(this),
                WrapPersistent(pending_script));
  if (base::FeatureList::IsEnabled(
          features::kLowPriorityAsyncScriptExecution)) {
    PostTaskWithLowPriorityUntilTimeout(
        FROM_HERE, std::move(task),
        features::kTimeoutForLowPriorityAsyncScriptExecution.Get(),
        low_priority_task_runner_, task_runner_);
  } else {
    task_runner_->PostTask(FROM_HERE, std::move(task));
  }
}

void ScriptRunner::ExecuteForceInOrderPendingScript(
    PendingScript* pending_script) {
  DCHECK_GT(pending_force_in_order_scripts_count_, 0u);
  ExecutePendingScript(pending_script);
  pending_force_in_order_scripts_count_ -= 1;
}

void ScriptRunner::ExecuteParserBlockingScriptsBlockedByForceInOrder() {
  ScriptableDocumentParser* parser = document_->GetScriptableDocumentParser();
  if (parser && document_->IsScriptExecutionReady()) {
    parser->ExecuteScriptsWaitingForResources();
  }
}

void ScriptRunner::PendingScriptFinished(PendingScript* pending_script) {
  pending_script->StopWatchingForLoad();

  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      CHECK(pending_async_scripts_.Contains(pending_script));
      RemoveDelayReasonFromScript(pending_script, DelayReason::kLoad);
      break;

    case ScriptSchedulingType::kInOrder:
      while (!pending_in_order_scripts_.IsEmpty() &&
             pending_in_order_scripts_.front()->IsReady()) {
        PendingScript* pending_in_order = pending_in_order_scripts_.TakeFirst();
        task_runner_->PostTask(FROM_HERE,
                               WTF::Bind(&ScriptRunner::ExecutePendingScript,
                                         WrapWeakPersistent(this),
                                         WrapPersistent(pending_in_order)));
      }
      break;

    case ScriptSchedulingType::kForceInOrder:
      while (!pending_force_in_order_scripts_.IsEmpty() &&
             pending_force_in_order_scripts_.front()->IsReady()) {
        PendingScript* pending_in_order =
            pending_force_in_order_scripts_.TakeFirst();
        task_runner_->PostTask(
            FROM_HERE,
            WTF::Bind(&ScriptRunner::ExecuteForceInOrderPendingScript,
                      WrapWeakPersistent(this),
                      WrapPersistent(pending_in_order)));
      }
      if (pending_force_in_order_scripts_.IsEmpty()) {
        task_runner_->PostTask(
            FROM_HERE,
            WTF::Bind(&ScriptRunner::
                          ExecuteParserBlockingScriptsBlockedByForceInOrder,
                      WrapWeakPersistent(this)));
      }
      break;

    default:
      NOTREACHED();
      break;
  }
}

void ScriptRunner::ExecutePendingScript(PendingScript* pending_script) {
  TRACE_EVENT("blink", "ScriptRunner::ExecutePendingScript");

  DCHECK(!document_->domWindow() || !document_->domWindow()->IsContextPaused());
  DCHECK(pending_script);

  pending_script->ExecuteScriptBlock();

  document_->DecrementLoadEventDelayCount();
}

void ScriptRunner::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(pending_in_order_scripts_);
  visitor->Trace(pending_async_scripts_);
  visitor->Trace(pending_force_in_order_scripts_);
  PendingScriptClient::Trace(visitor);
}

ScriptRunnerDelayer::ScriptRunnerDelayer(ScriptRunner* script_runner,
                                         ScriptRunner::DelayReason delay_reason)
    : script_runner_(script_runner), delay_reason_(delay_reason) {}

void ScriptRunnerDelayer::Activate() {
  if (activated_)
    return;
  activated_ = true;
  if (script_runner_)
    script_runner_->AddDelayReason(delay_reason_);
}

void ScriptRunnerDelayer::Deactivate() {
  if (!activated_)
    return;
  activated_ = false;
  if (script_runner_)
    script_runner_->RemoveDelayReason(delay_reason_);
}

void ScriptRunnerDelayer::Trace(Visitor* visitor) const {
  visitor->Trace(script_runner_);
}

}  // namespace blink
