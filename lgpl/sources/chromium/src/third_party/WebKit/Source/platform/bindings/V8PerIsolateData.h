/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8PerIsolateData_h
#define V8PerIsolateData_h

#include <memory>

#include "gin/public/isolate_holder.h"
#include "gin/public/v8_idle_task_runner.h"
#include "platform/PlatformExport.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

class ActiveScriptWrappableBase;
class DOMDataStore;
class StringCache;
class V8PrivateProperty;
class WebTaskRunner;
struct WrapperTypeInfo;

typedef WTF::Vector<DOMDataStore*> DOMDataStoreList;

// Used to hold data that is associated with a single v8::Isolate object, and
// has a 1:1 relationship with v8::Isolate.
class PLATFORM_EXPORT V8PerIsolateData {
  USING_FAST_MALLOC(V8PerIsolateData);
  WTF_MAKE_NONCOPYABLE(V8PerIsolateData);

 public:
  class EndOfScopeTask {
    USING_FAST_MALLOC(EndOfScopeTask);

   public:
    virtual ~EndOfScopeTask() {}
    virtual void Run() = 0;
  };

  // Disables the UseCounter.
  // UseCounter depends on the current context, but it's not available during
  // the initialization of v8::Context and the global object.  So we need to
  // disable the UseCounter while the initialization of the context and global
  // object.
  // TODO(yukishiino): Come up with an idea to remove this hack.
  class UseCounterDisabledScope {
    STACK_ALLOCATED();

   public:
    explicit UseCounterDisabledScope(V8PerIsolateData* per_isolate_data)
        : per_isolate_data_(per_isolate_data),
          original_use_counter_disabled_(
              per_isolate_data_->use_counter_disabled_) {
      per_isolate_data_->use_counter_disabled_ = true;
    }
    ~UseCounterDisabledScope() {
      per_isolate_data_->use_counter_disabled_ = original_use_counter_disabled_;
    }

   private:
    V8PerIsolateData* per_isolate_data_;
    const bool original_use_counter_disabled_;
  };

  // Use this class to abstract away types of members that are pointers to core/
  // objects, which are simply owned and released by V8PerIsolateData (see
  // m_threadDebugger for an example).
  class PLATFORM_EXPORT Data {
   public:
    virtual ~Data() = default;
  };

  static v8::Isolate* Initialize(WebTaskRunner*);

  static V8PerIsolateData* From(v8::Isolate* isolate) {
    DCHECK(isolate);
    DCHECK(isolate->GetData(gin::kEmbedderBlink));
    return static_cast<V8PerIsolateData*>(
        isolate->GetData(gin::kEmbedderBlink));
  }

  static void WillBeDestroyed(v8::Isolate*);
  static void Destroy(v8::Isolate*);
  static v8::Isolate* MainThreadIsolate();

  static void EnableIdleTasks(v8::Isolate*,
                              std::unique_ptr<gin::V8IdleTaskRunner>);

  v8::Isolate* GetIsolate() { return isolate_holder_.isolate(); }

  StringCache* GetStringCache() { return string_cache_.get(); }

  bool IsHandlingRecursionLevelError() const {
    return is_handling_recursion_level_error_;
  }
  void SetIsHandlingRecursionLevelError(bool value) {
    is_handling_recursion_level_error_ = value;
  }

  bool IsReportingException() const { return is_reporting_exception_; }
  void SetReportingException(bool value) { is_reporting_exception_ = value; }

  bool IsUseCounterDisabled() const { return use_counter_disabled_; }

  V8PrivateProperty* PrivateProperty() { return private_property_.get(); }

  // Accessors to the cache of interface templates.
  v8::Local<v8::FunctionTemplate> FindInterfaceTemplate(const DOMWrapperWorld&,
                                                        const void* key);
  void SetInterfaceTemplate(const DOMWrapperWorld&,
                            const void* key,
                            v8::Local<v8::FunctionTemplate>);

  // Accessor to the cache of cross-origin accessible operation's templates.
  // Created templates get automatically cached.
  v8::Local<v8::FunctionTemplate> FindOrCreateOperationTemplate(
      const DOMWrapperWorld&,
      const void* key,
      v8::FunctionCallback,
      v8::Local<v8::Value> data,
      v8::Local<v8::Signature>,
      int length);

  // Obtains a pointer to an array of names, given a lookup key. If it does not
  // yet exist, it is created from the given array of strings. Once created,
  // these live for as long as the isolate, so this is appropriate only for a
  // compile-time list of related names, such as IDL dictionary keys.
  const v8::Eternal<v8::Name>* FindOrCreateEternalNameCache(
      const void* lookup_key,
      const char* const names[],
      size_t count);

  bool HasInstance(const WrapperTypeInfo* untrusted, v8::Local<v8::Value>);
  v8::Local<v8::Object> FindInstanceInPrototypeChain(const WrapperTypeInfo*,
                                                     v8::Local<v8::Value>);

  v8::Local<v8::Context> EnsureScriptRegexpContext();
  void ClearScriptRegexpContext();

  // EndOfScopeTasks are run when control is returning
  // to C++ from script, after executing a script task (e.g. callback,
  // event) or microtasks (e.g. promise). This is explicitly needed for
  // Indexed DB transactions per spec, but should in general be avoided.
  void AddEndOfScopeTask(std::unique_ptr<EndOfScopeTask>);
  void RunEndOfScopeTasks();
  void ClearEndOfScopeTasks();

  void SetThreadDebugger(std::unique_ptr<Data>);
  Data* ThreadDebugger();

  using ActiveScriptWrappableSet =
      HeapHashSet<WeakMember<ActiveScriptWrappableBase>>;
  void AddActiveScriptWrappable(ActiveScriptWrappableBase*);
  const ActiveScriptWrappableSet* ActiveScriptWrappables() const {
    return active_script_wrappables_.Get();
  }

  class PLATFORM_EXPORT TemporaryScriptWrappableVisitorScope {
    WTF_MAKE_NONCOPYABLE(TemporaryScriptWrappableVisitorScope);
    STACK_ALLOCATED();

   public:
    TemporaryScriptWrappableVisitorScope(
        v8::Isolate* isolate,
        std::unique_ptr<ScriptWrappableVisitor> visitor)
        : isolate_(isolate), saved_visitor_(std::move(visitor)) {
      SwapWithV8PerIsolateDataVisitor(saved_visitor_);
    }
    ~TemporaryScriptWrappableVisitorScope() {
      SwapWithV8PerIsolateDataVisitor(saved_visitor_);
    }

    inline ScriptWrappableVisitor* CurrentVisitor() {
      return V8PerIsolateData::From(isolate_)->GetScriptWrappableVisitor();
    }

   private:
    void SwapWithV8PerIsolateDataVisitor(
        std::unique_ptr<ScriptWrappableVisitor>&);

    v8::Isolate* isolate_;
    std::unique_ptr<ScriptWrappableVisitor> saved_visitor_;
  };

  void SetScriptWrappableVisitor(
      std::unique_ptr<ScriptWrappableVisitor> visitor) {
    script_wrappable_visitor_ = std::move(visitor);
  }
  ScriptWrappableVisitor* GetScriptWrappableVisitor() {
    return script_wrappable_visitor_.get();
  }

 private:
  explicit V8PerIsolateData(WebTaskRunner*);
  ~V8PerIsolateData();

  typedef HashMap<const void*, v8::Eternal<v8::FunctionTemplate>>
      V8FunctionTemplateMap;
  V8FunctionTemplateMap& SelectInterfaceTemplateMap(const DOMWrapperWorld&);
  V8FunctionTemplateMap& SelectOperationTemplateMap(const DOMWrapperWorld&);
  bool HasInstance(const WrapperTypeInfo* untrusted,
                   v8::Local<v8::Value>,
                   V8FunctionTemplateMap&);
  v8::Local<v8::Object> FindInstanceInPrototypeChain(const WrapperTypeInfo*,
                                                     v8::Local<v8::Value>,
                                                     V8FunctionTemplateMap&);

  gin::IsolateHolder isolate_holder_;

  // m_interfaceTemplateMapFor{,Non}MainWorld holds function templates for
  // the inerface objects.
  V8FunctionTemplateMap interface_template_map_for_main_world_;
  V8FunctionTemplateMap interface_template_map_for_non_main_world_;
  // m_operationTemplateMapFor{,Non}MainWorld holds function templates for
  // the cross-origin accessible DOM operations.
  V8FunctionTemplateMap operation_template_map_for_main_world_;
  V8FunctionTemplateMap operation_template_map_for_non_main_world_;

  // Contains lists of eternal names, such as dictionary keys.
  HashMap<const void*, Vector<v8::Eternal<v8::Name>>> eternal_name_cache_;

  std::unique_ptr<StringCache> string_cache_;
  std::unique_ptr<V8PrivateProperty> private_property_;
  RefPtr<ScriptState> script_regexp_script_state_;

  bool constructor_mode_;
  friend class ConstructorMode;

  bool use_counter_disabled_;
  friend class UseCounterDisabledScope;

  bool is_handling_recursion_level_error_;
  bool is_reporting_exception_;

  Vector<std::unique_ptr<EndOfScopeTask>> end_of_scope_tasks_;
  std::unique_ptr<Data> thread_debugger_;

  Persistent<ActiveScriptWrappableSet> active_script_wrappables_;
  std::unique_ptr<ScriptWrappableVisitor> script_wrappable_visitor_;
};

}  // namespace blink

#endif  // V8PerIsolateData_h
