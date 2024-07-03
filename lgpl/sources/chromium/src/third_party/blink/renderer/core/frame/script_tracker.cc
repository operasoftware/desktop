// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/script_tracker.h"

#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

namespace {

String FindNonEmptyUrlOnStack(
    const HeapVector<Member<const ScriptInfo>>& stack) {
  auto ordered_stack = base::Reversed(stack);
  auto it = base::ranges::find_if(ordered_stack, [](auto info) {
    return !info->url.empty();
  });
  if (it != ordered_stack.end())
    return (*it)->url;
  return String();
}

String ScriptUrlForFunction(const probe::CallFunction& probe) {
  v8::Local<v8::Value> resource_name =
      probe.function->GetScriptOrigin().ResourceName();
  if (resource_name.IsEmpty())
    return String();
  v8::MaybeLocal<v8::String> resource_name_string = resource_name->ToString(
      probe.context->GetIsolate()->GetCurrentContext());
  if (resource_name_string.IsEmpty())
    return String();
  return ToCoreString(probe.context->GetIsolate(),
                      resource_name_string.ToLocalChecked());
}

}  // namespace

ScriptInfo::ScriptInfo(String url, int id) : url(url), id(id) {}

ScriptInfo::~ScriptInfo() = default;

ScriptTracker::ScriptTracker(LocalFrame& local_frame) : frame_(local_frame) {
  frame_->GetProbeSink()->AddScriptTracker(this);
}

ScriptTracker::~ScriptTracker() = default;

void ScriptTracker::Will(const probe::ExecuteScript& probe) {
  if (probe.context != frame_->DomWindow())
    return;
  // Will we need/want world id? probe.context->GetCurrentWorld()->GetWorldId()
  // will yield it (at the cost of some ref churn).
  active_scripts_.push_back(MakeGarbageCollected<ScriptInfo>(
      probe.script_url, probe.script_id));
}

void ScriptTracker::Did(const probe::ExecuteScript& probe) {
  if (probe.context != frame_->DomWindow())
    return;
  active_scripts_.pop_back();
}

void ScriptTracker::Will(const probe::CallFunction& probe) {
  if (probe.depth)
    return;
  if (probe.context != frame_->DomWindow())
    return;
  active_scripts_.push_back(MakeGarbageCollected<ScriptInfo>(
      ScriptUrlForFunction(probe), probe.function->ScriptId()));
}

void ScriptTracker::Did(const probe::CallFunction& probe) {
  if (probe.depth)
    return;
  if (probe.context != frame_->DomWindow())
    return;
  active_scripts_.pop_back();
}

void ScriptTracker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(active_scripts_);
  visitor->Trace(generating_scripts_);
}

const ScriptInfo* ScriptTracker::GetActiveScriptInfo() const {
  return active_scripts_.empty() ? nullptr : active_scripts_.front();
}

String ScriptTracker::FindIndirectScriptSourceUrl() const {
  String nonempty_active_script_url = FindNonEmptyUrlOnStack(active_scripts_);
  if (nonempty_active_script_url)
    return nonempty_active_script_url;
  return FindNonEmptyUrlOnStack(generating_scripts_);
}

void ScriptTracker::PushGeneratingScript(const ScriptInfo& script_info) {
  generating_scripts_.push_back(script_info);
}

void ScriptTracker::PopGeneratingScript() {
  generating_scripts_.pop_back();
}

void ScriptTracker::Shutdown() {
  if (frame_)
    return;
  frame_->GetProbeSink()->RemoveScriptTracker(this);
  frame_ = nullptr;
}

GeneratingScriptScope::GeneratingScriptScope(LocalFrame* frame,
                                             const ScriptInfo& script_info)
    : script_tracker_(frame->GetScriptTracker()) {
  script_tracker_->PushGeneratingScript(script_info);
}

GeneratingScriptScope::~GeneratingScriptScope() {
  script_tracker_->PopGeneratingScript();
}

}  // namespace blink
