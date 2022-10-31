// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCRIPT_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCRIPT_TRACKER_H_

#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

#if BUILDFLAG(OPERA_BLINK_FEATURE_SCRIPT_TRACKER)
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#endif  // BUILDFLAG(OPERA_BLINK_FEATURE_SCRIPT_TRACKER)

namespace blink {

class LocalFrame;

namespace probe {
class CallFunction;
class ExecuteScript;
}  // namespace probe

#if BUILDFLAG(OPERA_BLINK_FEATURE_SCRIPT_TRACKER)
struct ScriptInfo : public GarbageCollected<ScriptInfo> {
  String url;
  int id;

  ScriptInfo(String url, int id);
  ~ScriptInfo();

  void Trace(Visitor*) const {}
};

// Tracker for scripts being executed in a certain frame.
class ScriptTracker : public GarbageCollected<ScriptTracker> {
 public:
  explicit ScriptTracker(LocalFrame&);
  ScriptTracker(const ScriptTracker&) = delete;
  ScriptTracker& operator=(const ScriptTracker&) = delete;

  virtual ~ScriptTracker();

  // Instrumenting methods.
  // Called when a script module or script gets executed from native code.
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);

  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);

  virtual void Trace(Visitor*) const;

  const ScriptInfo* GetActiveScriptInfo() const;
  String FindIndirectScriptSourceUrl() const;

  void Shutdown();

 private:
  friend class GeneratingScriptScope;

  void PushGeneratingScript(const ScriptInfo&);
  void PopGeneratingScript();

  Member<LocalFrame> frame_;

  HeapVector<Member<const ScriptInfo>> active_scripts_;
  HeapVector<Member<const ScriptInfo>> generating_scripts_;
};

class GeneratingScriptScope {
  STACK_ALLOCATED();

 public:
  GeneratingScriptScope(LocalFrame*, const ScriptInfo&);
  ~GeneratingScriptScope();

 private:
  ScriptTracker* script_tracker_;
};
#else
// Though the tracker is needed only when OPERA_BLINK_FEATURE_SCRIPT_TRACKER
// flag is enabled the ScriptTracker class has to be always defined. The reason
// for that is that regardless of the flag state it is referenced by the file
// used by the v8_context_snapshot_generator (there are no means of conditional
// inclusion there) and always shows up in generated code.
class ScriptTracker : public GarbageCollected<ScriptTracker> {
public:
  virtual ~ScriptTracker() = default;

  // Instrumenting methods.
  // Called when a script module or script gets executed from native code.
  void Will(const probe::ExecuteScript&) {}
  void Did(const probe::ExecuteScript&) {}

  void Will(const probe::CallFunction&) {}
  void Did(const probe::CallFunction&) {}

  virtual void Trace(Visitor*) const {}
};
#endif  // BUILDFLAG(OPERA_BLINK_FEATURE_SCRIPT_TRACKER)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCRIPT_TRACKER_H_
