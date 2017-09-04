// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/interface.cpp.tmpl

// clang-format off
#include "V8TestIntegerIndexedGlobal.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8Node.h"
#include "core/dom/ExecutionContext.h"
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
const WrapperTypeInfo V8TestIntegerIndexedGlobal::wrapperTypeInfo = { gin::kEmbedderBlink, V8TestIntegerIndexedGlobal::domTemplate, V8TestIntegerIndexedGlobal::Trace, V8TestIntegerIndexedGlobal::TraceWrappers, nullptr, "TestIntegerIndexedGlobal", 0, WrapperTypeInfo::kWrapperTypeObjectPrototype, WrapperTypeInfo::kObjectClassId, WrapperTypeInfo::kNotInheritFromActiveScriptWrappable, WrapperTypeInfo::kIndependent };
#if defined(COMPONENT_BUILD) && defined(WIN32) && COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestIntegerIndexedGlobal.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestIntegerIndexedGlobal::wrapper_type_info_ = V8TestIntegerIndexedGlobal::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestIntegerIndexedGlobal>::value,
    "TestIntegerIndexedGlobal inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestIntegerIndexedGlobal::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestIntegerIndexedGlobal is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace TestIntegerIndexedGlobalV8Internal {

static void lengthAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestIntegerIndexedGlobal* impl = V8TestIntegerIndexedGlobal::toImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->length()));
}

static void lengthAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestIntegerIndexedGlobal* impl = V8TestIntegerIndexedGlobal::toImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestIntegerIndexedGlobal", "length");

  // Prepare the value to be set.
  uint64_t cppValue = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState, kNormalConversion);
  if (exceptionState.HadException())
    return;

  impl->setLength(cppValue);
}

static void voidMethodDocumentMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestIntegerIndexedGlobal* impl = V8TestIntegerIndexedGlobal::toImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocument", "TestIntegerIndexedGlobal", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Document* document;
  document = V8Document::toImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!document) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocument", "TestIntegerIndexedGlobal", "parameter 1 is not of type 'Document'."));

    return;
  }

  impl->voidMethodDocument(document);
}

} // namespace TestIntegerIndexedGlobalV8Internal

void V8TestIntegerIndexedGlobal::lengthAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestIntegerIndexedGlobalV8Internal::lengthAttributeGetter(info);
}

void V8TestIntegerIndexedGlobal::lengthAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> v8Value = info[0];

  TestIntegerIndexedGlobalV8Internal::lengthAttributeSetter(v8Value, info);
}

void V8TestIntegerIndexedGlobal::voidMethodDocumentMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestIntegerIndexedGlobalV8Internal::voidMethodDocumentMethod(info);
}

void V8TestIntegerIndexedGlobal::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestIntegerIndexedGlobal::namedPropertyGetterCustom(propertyName, info);
}

void V8TestIntegerIndexedGlobal::namedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestIntegerIndexedGlobal::namedPropertySetterCustom(propertyName, v8Value, info);
}

void V8TestIntegerIndexedGlobal::namedPropertyDeleterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestIntegerIndexedGlobal::namedPropertyDeleterCustom(propertyName, info);
}

void V8TestIntegerIndexedGlobal::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestIntegerIndexedGlobal::namedPropertyQueryCustom(propertyName, info);
}

void V8TestIntegerIndexedGlobal::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  V8TestIntegerIndexedGlobal::namedPropertyEnumeratorCustom(info);
}

void V8TestIntegerIndexedGlobal::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  V8TestIntegerIndexedGlobal::indexedPropertyGetterCustom(index, info);
}

void V8TestIntegerIndexedGlobal::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  V8TestIntegerIndexedGlobal::indexedPropertySetterCustom(index, v8Value, info);
}

void V8TestIntegerIndexedGlobal::indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  V8TestIntegerIndexedGlobal::indexedPropertyDeleterCustom(index, info);
}

static const V8DOMConfiguration::AccessorConfiguration V8TestIntegerIndexedGlobalAccessors[] = {
      { "length", V8TestIntegerIndexedGlobal::lengthAttributeGetterCallback, V8TestIntegerIndexedGlobal::lengthAttributeSetterCallback, nullptr, nullptr, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds }
    ,
};

static const V8DOMConfiguration::MethodConfiguration V8TestIntegerIndexedGlobalMethods[] = {
    {"voidMethodDocument", V8TestIntegerIndexedGlobal::voidMethodDocumentMethodCallback, 1, v8::None, V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestIntegerIndexedGlobalTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestIntegerIndexedGlobal::wrapperTypeInfo.interface_name, V8TestIntegerIndexedGlobal::domTemplateForNamedPropertiesObject(isolate, world), V8TestIntegerIndexedGlobal::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Global object prototype chain consists of Immutable Prototype Exotic Objects
  prototypeTemplate->SetImmutableProto();

  // Global objects are Immutable Prototype Exotic Objects
  instanceTemplate->SetImmutableProto();

  // Register DOM constants, attributes and operations.
  V8DOMConfiguration::InstallAccessors(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestIntegerIndexedGlobalAccessors, WTF_ARRAY_LENGTH(V8TestIntegerIndexedGlobalAccessors));
  V8DOMConfiguration::InstallMethods(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestIntegerIndexedGlobalMethods, WTF_ARRAY_LENGTH(V8TestIntegerIndexedGlobalMethods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(V8TestIntegerIndexedGlobal::indexedPropertyGetterCallback, V8TestIntegerIndexedGlobal::indexedPropertySetterCallback, nullptr, V8TestIntegerIndexedGlobal::indexedPropertyDeleterCallback, IndexedPropertyEnumerator<TestIntegerIndexedGlobal>, v8::Local<v8::Value>(), v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);

  // Array iterator (@@iterator)
  instanceTemplate->SetIntrinsicDataProperty(v8::Symbol::GetIterator(isolate), v8::kArrayProto_values, v8::DontEnum);
}

v8::Local<v8::FunctionTemplate> V8TestIntegerIndexedGlobal::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestIntegerIndexedGlobalTemplate);
}

v8::Local<v8::FunctionTemplate> V8TestIntegerIndexedGlobal::domTemplateForNamedPropertiesObject(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  v8::Local<v8::FunctionTemplate> parentTemplate = V8None::domTemplate(isolate, world);

  v8::Local<v8::FunctionTemplate> namedPropertiesObjectFunctionTemplate = v8::FunctionTemplate::New(isolate, V8ObjectConstructor::IsValidConstructorMode);
  namedPropertiesObjectFunctionTemplate->SetClassName(V8AtomicString(isolate, "TestIntegerIndexedGlobalProperties"));
  namedPropertiesObjectFunctionTemplate->Inherit(parentTemplate);

  v8::Local<v8::ObjectTemplate> namedPropertiesObjectTemplate = namedPropertiesObjectFunctionTemplate->PrototypeTemplate();
  namedPropertiesObjectTemplate->SetInternalFieldCount(V8TestIntegerIndexedGlobal::internalFieldCount);
  // Named Properties object has SetPrototype method of Immutable Prototype Exotic Objects
  namedPropertiesObjectTemplate->SetImmutableProto();
  V8DOMConfiguration::SetClassString(isolate, namedPropertiesObjectTemplate, "TestIntegerIndexedGlobalProperties");
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestIntegerIndexedGlobal::namedPropertyGetterCallback, V8TestIntegerIndexedGlobal::namedPropertySetterCallback, V8TestIntegerIndexedGlobal::namedPropertyQueryCallback, V8TestIntegerIndexedGlobal::namedPropertyDeleterCallback, V8TestIntegerIndexedGlobal::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  namedPropertiesObjectTemplate->SetHandler(namedPropertyHandlerConfig);

  return namedPropertiesObjectFunctionTemplate;
}

bool V8TestIntegerIndexedGlobal::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestIntegerIndexedGlobal::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestIntegerIndexedGlobal* V8TestIntegerIndexedGlobal::toImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? toImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestIntegerIndexedGlobal* NativeValueTraits<TestIntegerIndexedGlobal>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestIntegerIndexedGlobal* nativeValue = V8TestIntegerIndexedGlobal::toImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestIntegerIndexedGlobal"));
  }
  return nativeValue;
}

}  // namespace blink
