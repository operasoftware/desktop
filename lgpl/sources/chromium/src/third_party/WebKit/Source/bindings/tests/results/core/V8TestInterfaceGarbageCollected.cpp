// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/interface.cpp.tmpl

// clang-format off
#include "V8TestInterfaceGarbageCollected.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "bindings/core/v8/V8Iterator.h"
#include "bindings/core/v8/V8TestInterfaceGarbageCollected.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/wtf/GetPtr.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceGarbageCollected::wrapperTypeInfo = { gin::kEmbedderBlink, V8TestInterfaceGarbageCollected::domTemplate, V8TestInterfaceGarbageCollected::Trace, V8TestInterfaceGarbageCollected::TraceWrappers, nullptr, "TestInterfaceGarbageCollected", &V8EventTarget::wrapperTypeInfo, WrapperTypeInfo::kWrapperTypeObjectPrototype, WrapperTypeInfo::kObjectClassId, WrapperTypeInfo::kNotInheritFromActiveScriptWrappable, WrapperTypeInfo::kIndependent };
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceGarbageCollected.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceGarbageCollected::wrapper_type_info_ = V8TestInterfaceGarbageCollected::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceGarbageCollected>::value,
    "TestInterfaceGarbageCollected inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceGarbageCollected::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceGarbageCollected is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace TestInterfaceGarbageCollectedV8Internal {

static void attr1AttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->attr1()), impl);
}

static void attr1AttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceGarbageCollected", "attr1");

  // Prepare the value to be set.
  TestInterfaceGarbageCollected* cppValue = V8TestInterfaceGarbageCollected::toImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterfaceGarbageCollected'.");
    return;
  }

  impl->setAttr1(cppValue);
}

static void sizeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(holder);

  V8SetReturnValueUnsigned(info, impl->size());
}

static void funcMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("func", "TestInterfaceGarbageCollected", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceGarbageCollected* arg;
  arg = V8TestInterfaceGarbageCollected::toImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!arg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("func", "TestInterfaceGarbageCollected", "parameter 1 is not of type 'TestInterfaceGarbageCollected'."));

    return;
  }

  impl->func(arg);
}

static void keysMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceGarbageCollected", "keys");

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForReceiverObject(info);

  Iterator* result = impl->keysForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void entriesMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceGarbageCollected", "entries");

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForReceiverObject(info);

  Iterator* result = impl->entriesForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void forEachMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceGarbageCollected", "forEach");

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForReceiverObject(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ScriptValue callback;
  ScriptValue thisArg;
  if (!(info[0]->IsObject() && v8::Local<v8::Object>::Cast(info[0])->IsCallable())) {
    exceptionState.ThrowTypeError("The callback provided as parameter 1 is not a function.");

    return;
  }
  callback = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);

  thisArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[1]);

  impl->forEachForBinding(scriptState, ScriptValue(scriptState, info.Holder()), callback, thisArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void hasMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceGarbageCollected", "has");

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForReceiverObject(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  bool result = impl->hasForBinding(scriptState, value, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void addMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceGarbageCollected", "add");

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForReceiverObject(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfaceGarbageCollected* result = impl->addForBinding(scriptState, value, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void clearMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceGarbageCollected", "clear");

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForReceiverObject(info);

  impl->clearForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void deleteMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceGarbageCollected", "delete");

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForReceiverObject(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  bool result = impl->deleteForBinding(scriptState, value, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValueBool(info, result);
}

static void iteratorMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceGarbageCollected", "iterator");

  TestInterfaceGarbageCollected* impl = V8TestInterfaceGarbageCollected::toImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForReceiverObject(info);

  Iterator* result = impl->GetIterator(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToConstruct("TestInterfaceGarbageCollected", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<> str;
  str = info[0];
  if (!str.Prepare())
    return;

  TestInterfaceGarbageCollected* impl = TestInterfaceGarbageCollected::Create(str);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceGarbageCollected::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

} // namespace TestInterfaceGarbageCollectedV8Internal

void V8TestInterfaceGarbageCollected::attr1AttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::attr1AttributeGetter(info);
}

void V8TestInterfaceGarbageCollected::attr1AttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceGarbageCollectedV8Internal::attr1AttributeSetter(v8Value, info);
}

void V8TestInterfaceGarbageCollected::sizeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::sizeAttributeGetter(info);
}

void V8TestInterfaceGarbageCollected::funcMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::funcMethod(info);
}

void V8TestInterfaceGarbageCollected::keysMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::keysMethod(info);
}

void V8TestInterfaceGarbageCollected::entriesMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::entriesMethod(info);
}

void V8TestInterfaceGarbageCollected::forEachMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::forEachMethod(info);
}

void V8TestInterfaceGarbageCollected::hasMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::hasMethod(info);
}

void V8TestInterfaceGarbageCollected::addMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::addMethod(info);
}

void V8TestInterfaceGarbageCollected::clearMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::clearMethod(info);
}

void V8TestInterfaceGarbageCollected::deleteMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::deleteMethod(info);
}

void V8TestInterfaceGarbageCollected::iteratorMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceGarbageCollectedV8Internal::iteratorMethod(info);
}

static const V8DOMConfiguration::AccessorConfiguration V8TestInterfaceGarbageCollectedAccessors[] = {
      { "attr1", V8TestInterfaceGarbageCollected::attr1AttributeGetterCallback, V8TestInterfaceGarbageCollected::attr1AttributeSetterCallback, nullptr, nullptr, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds }
    ,

      { "size", V8TestInterfaceGarbageCollected::sizeAttributeGetterCallback, nullptr, nullptr, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum | v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds }
    ,
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterfaceGarbageCollectedMethods[] = {
    {"func", V8TestInterfaceGarbageCollected::funcMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
    {"keys", V8TestInterfaceGarbageCollected::keysMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
    {"entries", V8TestInterfaceGarbageCollected::entriesMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
    {"forEach", V8TestInterfaceGarbageCollected::forEachMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
    {"has", V8TestInterfaceGarbageCollected::hasMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
    {"add", V8TestInterfaceGarbageCollected::addMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
    {"clear", V8TestInterfaceGarbageCollected::clearMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
    {"delete", V8TestInterfaceGarbageCollected::deleteMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
};

void V8TestInterfaceGarbageCollected::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("TestInterfaceGarbageCollected"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  TestInterfaceGarbageCollectedV8Internal::constructor(info);
}

static void installV8TestInterfaceGarbageCollectedTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceGarbageCollected::wrapperTypeInfo.interface_name, V8EventTarget::domTemplate(isolate, world), V8TestInterfaceGarbageCollected::internalFieldCount);
  interfaceTemplate->SetCallHandler(V8TestInterfaceGarbageCollected::constructorCallback);
  interfaceTemplate->SetLength(1);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register DOM constants, attributes and operations.
  V8DOMConfiguration::InstallAccessors(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestInterfaceGarbageCollectedAccessors, WTF_ARRAY_LENGTH(V8TestInterfaceGarbageCollectedAccessors));
  V8DOMConfiguration::InstallMethods(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestInterfaceGarbageCollectedMethods, WTF_ARRAY_LENGTH(V8TestInterfaceGarbageCollectedMethods));

  // Iterator (@@iterator)
  static const V8DOMConfiguration::SymbolKeyedMethodConfiguration symbolKeyedIteratorConfiguration = { v8::Symbol::GetIterator, "values", V8TestInterfaceGarbageCollected::iteratorMethodCallback, 0, v8::DontEnum, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess };
  V8DOMConfiguration::InstallMethod(isolate, world, prototypeTemplate, signature, symbolKeyedIteratorConfiguration);
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceGarbageCollected::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceGarbageCollectedTemplate);
}

bool V8TestInterfaceGarbageCollected::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceGarbageCollected::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceGarbageCollected* V8TestInterfaceGarbageCollected::toImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? toImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceGarbageCollected* NativeValueTraits<TestInterfaceGarbageCollected>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceGarbageCollected* nativeValue = V8TestInterfaceGarbageCollected::toImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceGarbageCollected"));
  }
  return nativeValue;
}

}  // namespace blink
