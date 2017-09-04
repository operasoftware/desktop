// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ClassicScript.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/network/mime/MIMETypeRegistry.h"

namespace blink {

namespace {

void LogScriptMIMEType(LocalFrame* frame,
                       ScriptResource* resource,
                       const String& mime_type,
                       const SecurityOrigin* security_origin) {
  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type))
    return;
  bool is_text = mime_type.StartsWith("text/", kTextCaseASCIIInsensitive);
  if (is_text && MIMETypeRegistry::IsLegacySupportedJavaScriptLanguage(
                     mime_type.Substring(5)))
    return;
  bool is_same_origin = security_origin->CanRequest(resource->Url());
  bool is_application =
      !is_text &&
      mime_type.StartsWith("application/", kTextCaseASCIIInsensitive);

  UseCounter::Feature feature =
      is_same_origin
          ? (is_text ? UseCounter::kSameOriginTextScript
                     : is_application ? UseCounter::kSameOriginApplicationScript
                                      : UseCounter::kSameOriginOtherScript)
          : (is_text
                 ? UseCounter::kCrossOriginTextScript
                 : is_application ? UseCounter::kCrossOriginApplicationScript
                                  : UseCounter::kCrossOriginOtherScript);

  UseCounter::Count(frame, feature);
}

}  // namespace

DEFINE_TRACE(ClassicScript) {
  Script::Trace(visitor);
  visitor->Trace(script_source_code_);
}

bool ClassicScript::IsEmpty() const {
  return GetScriptSourceCode().IsEmpty();
}

bool ClassicScript::CheckMIMETypeBeforeRunScript(
    Document* context_document,
    const SecurityOrigin* security_origin) const {
  ScriptResource* resource = GetScriptSourceCode().GetResource();
  CHECK(resource);

  if (!ScriptResource::MimeTypeAllowedByNosniff(resource->GetResponse())) {
    context_document->AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Refused to execute script from '" + resource->Url().ElidedString() +
            "' because its MIME type ('" + resource->HttpContentType() +
            "') is not executable, and "
            "strict MIME type checking is "
            "enabled."));
    return false;
  }

  String mime_type = resource->HttpContentType();
  LocalFrame* frame = context_document->GetFrame();
  if (mime_type.StartsWith("image/") || mime_type == "text/csv" ||
      mime_type.StartsWith("audio/") || mime_type.StartsWith("video/")) {
    context_document->AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Refused to execute script from '" + resource->Url().ElidedString() +
            "' because its MIME type ('" + mime_type +
            "') is not executable."));
    if (mime_type.StartsWith("image/"))
      UseCounter::Count(frame, UseCounter::kBlockedSniffingImageToScript);
    else if (mime_type.StartsWith("audio/"))
      UseCounter::Count(frame, UseCounter::kBlockedSniffingAudioToScript);
    else if (mime_type.StartsWith("video/"))
      UseCounter::Count(frame, UseCounter::kBlockedSniffingVideoToScript);
    else if (mime_type == "text/csv")
      UseCounter::Count(frame, UseCounter::kBlockedSniffingCSVToScript);
    return false;
  }

  LogScriptMIMEType(frame, resource, mime_type, security_origin);

  return true;
}

void ClassicScript::RunScript(LocalFrame* frame,
                              const SecurityOrigin* security_origin) const {
  const bool is_external_script = GetScriptSourceCode().GetResource();

  AccessControlStatus access_control_status = kNotSharableCrossOrigin;
  if (!is_external_script) {
    access_control_status = kSharableCrossOrigin;
  } else {
    CHECK(GetScriptSourceCode().GetResource());
    access_control_status =
        GetScriptSourceCode().GetResource()->CalculateAccessControlStatus(
            security_origin);
  }

  frame->GetScriptController().ExecuteScriptInMainWorld(GetScriptSourceCode(),
                                                        access_control_status);
}

}  // namespace blink
