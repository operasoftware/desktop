// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_TASK_ATTRIBUTION_TRACKER_IMPL_H_

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink::scheduler {

class TaskAttributionTrackerTest;

// This class is used to keep track of tasks posted on the main thread and their
// ancestry. It assigns an incerementing ID per task, and gets notified when a
// task is posted, started or ended, and using that, it keeps track of which
// task is the parent of the current task, and stores that info for later. It
// then enables callers to determine if a certain task ID is an ancestor of the
// current task.
class MODULES_EXPORT TaskAttributionTrackerImpl
    : public TaskAttributionTracker {
  friend class TaskAttributionTrackerTest;

 public:
  TaskAttributionTrackerImpl();

  absl::optional<TaskAttributionId> RunningTaskAttributionId(
      ScriptState*) const override;

  AncestorStatus IsAncestor(ScriptState*, TaskAttributionId parent_id) override;
  AncestorStatus HasAncestorInSet(
      ScriptState*,
      const WTF::HashSet<scheduler::TaskAttributionIdType>&) override;

  std::unique_ptr<TaskScope> CreateTaskScope(
      ScriptState* script_state,
      absl::optional<TaskAttributionId> parent_task_id) override;

  // The vector size limits the amount of tasks we keep track of. Setting this
  // value too small can result in calls to `IsAncestor` returning an `Unknown`
  // ancestor status. If this happens a lot in realistic scenarios, we'd need to
  // increase this value (at the expense of memory dedicated to task tracking).
  static constexpr size_t kVectorSize = 1024;

  void SetRunningTaskAttributionId(absl::optional<TaskAttributionId> id) {
    running_task_id_ = id;
  }

  void TaskScopeCompleted(ScriptState*, TaskAttributionId);

  void RegisterObserver(TaskAttributionTracker::Observer* observer) override {
    DCHECK(!observer_ || observer == observer_);
    observer_ = observer;
  }

  void UnregisterObserver() override { observer_.Clear(); }

 private:
  struct TaskAttributionIdPair {
    TaskAttributionIdPair() = default;
    TaskAttributionIdPair(absl::optional<TaskAttributionId> parent_id,
                          absl::optional<TaskAttributionId> current_id)
        : parent(parent_id), current(current_id) {}

    explicit operator bool() const { return parent.has_value(); }
    absl::optional<TaskAttributionId> parent;
    absl::optional<TaskAttributionId> current;
  };

  template <typename F>
  AncestorStatus IsAncestorInternal(ScriptState*, F callback);

  class TaskScopeImpl : public TaskScope {
   public:
    TaskScopeImpl(ScriptState*,
                  TaskAttributionTrackerImpl*,
                  TaskAttributionId scope_task_id,
                  absl::optional<TaskAttributionId> previous_task_id,
                  absl::optional<TaskAttributionId> previous_v8_task_id);
    ~TaskScopeImpl() override;
    TaskScopeImpl(const TaskScopeImpl&) = delete;
    TaskScopeImpl& operator=(const TaskScopeImpl&) = delete;

    TaskAttributionId GetTaskId() const { return scope_task_id_; }
    absl::optional<TaskAttributionId> PreviousTaskAttributionId() const {
      return previous_task_id_;
    }
    absl::optional<TaskAttributionId> PreviousV8TaskAttributionId() const {
      return previous_v8_task_id_;
    }
    ScriptState* GetScriptState() const { return script_state_; }

   private:
    TaskAttributionTrackerImpl* task_tracker_;
    TaskAttributionId scope_task_id_;
    absl::optional<TaskAttributionId> previous_task_id_;
    absl::optional<TaskAttributionId> previous_v8_task_id_;
    Persistent<ScriptState> script_state_;
  };

  class MODULES_EXPORT V8Adapter {
   public:
    virtual absl::optional<TaskAttributionId> GetValue(ScriptState*);
    virtual void SetValue(ScriptState*, absl::optional<TaskAttributionId>);
    virtual ~V8Adapter() = default;
  };

  void TaskScopeCompleted(const TaskScopeImpl&);
  TaskAttributionIdPair& GetTaskAttributionIdPairFromTaskContainer(
      TaskAttributionId);
  void InsertTaskAttributionIdPair(
      TaskAttributionId task_id,
      absl::optional<TaskAttributionId> parent_task_id);
  void SaveTaskIdStateInV8(ScriptState*, absl::optional<TaskAttributionId>);

  void SetV8AdapterForTesting(std::unique_ptr<V8Adapter> adapter) {
    v8_adapter_.swap(adapter);
  }

  TaskAttributionId next_task_id_;
  absl::optional<TaskAttributionId> running_task_id_;

  std::unique_ptr<V8Adapter> v8_adapter_;

  // The task container is a vector of optional TaskAttributionIdPairs where its
  // indexes are TaskAttributionId hashes, and its values are the TaskId of the
  // parent task for the TaskAttributionId that is resulted in the index. We're
  // using this vector as a circular array, where in order to find if task A is
  // an ancestor of task B, we look up the value at B's taskAttributionId hash
  // position, get its parent, and repeat that process until we either find A in
  // the ancestor chain, get no parent task (indicating that a task has no
  // parent, so wasn't initiated by another JS task), or reach a parent that
  // doesn't have the current ID its child though it should have, which
  // indicates that the parent was overwritten by a newer task, indicating that
  // we went "full circle".
  WTF::Vector<TaskAttributionIdPair> task_container_ =
      WTF::Vector<TaskAttributionIdPair>(kVectorSize);

  WeakPersistent<TaskAttributionTracker::Observer> observer_;
};

}  // namespace blink::scheduler

#endif
