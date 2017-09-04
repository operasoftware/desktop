/*
 * Copyright (C) 2005, 2006, 2008, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/HistoryItem.h"

#include "core/html/ContentEditablesController.h"
#include "core/html/forms/FormController.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/text/CString.h"

namespace blink {

namespace {
const char kDocumentStateVersionMarker[] = "Version";
const char kDocumentStateVersion[] = "1";
}  // namespace

static long long GenerateSequenceNumber() {
  // Initialize to the current time to reduce the likelihood of generating
  // identifiers that overlap with those from past/future browser sessions.
  static long long next = static_cast<long long>(CurrentTime() * 1000000.0);
  return ++next;
}

HistoryItem::HistoryItem()
    : did_save_scroll_or_scale_state_(false),
      page_scale_factor_(0),
      item_sequence_number_(GenerateSequenceNumber()),
      document_sequence_number_(GenerateSequenceNumber()),
      scroll_restoration_type_(kScrollRestorationAuto) {}

HistoryItem::~HistoryItem() {}

const String& HistoryItem::UrlString() const {
  return url_string_;
}

KURL HistoryItem::Url() const {
  return KURL(kParsedURLString, url_string_);
}

const Referrer& HistoryItem::GetReferrer() const {
  return referrer_;
}

void HistoryItem::SetURLString(const String& url_string) {
  if (url_string_ != url_string)
    url_string_ = url_string;
}

void HistoryItem::SetURL(const KURL& url) {
  SetURLString(url.GetString());
}

void HistoryItem::SetReferrer(const Referrer& referrer) {
  // This should be a CHECK.
  referrer_ = SecurityPolicy::GenerateReferrer(referrer.referrer_policy, Url(),
                                               referrer.referrer);
}

const ScrollOffset& HistoryItem::VisualViewportScrollOffset() const {
  return visual_viewport_scroll_offset_;
}

void HistoryItem::SetVisualViewportScrollOffset(const ScrollOffset& offset) {
  visual_viewport_scroll_offset_ = offset;
  SetDidSaveScrollOrScaleState(true);
}

const ScrollOffset& HistoryItem::GetScrollOffset() const {
  return scroll_offset_;
}

void HistoryItem::SetScrollOffset(const ScrollOffset& offset) {
  scroll_offset_ = offset;
  SetDidSaveScrollOrScaleState(true);
}

float HistoryItem::PageScaleFactor() const {
  return page_scale_factor_;
}

void HistoryItem::SetPageScaleFactor(float scale_factor) {
  page_scale_factor_ = scale_factor;
  SetDidSaveScrollOrScaleState(true);
}

void HistoryItem::SetFormState(const Vector<String>& state) {
  form_state_ = state;
}

const Vector<String>& HistoryItem::FormState() {
  if (document_forms_state_)
    form_state_ = document_forms_state_->ToStateVector();
  return form_state_;
}

void HistoryItem::ClearFormState() {
  form_state_.clear();
  document_forms_state_.Clear();
}

void HistoryItem::SetContentEditablesState(ContentEditablesState* state) {
  content_editables_state_ = state;
}

const Vector<String>& HistoryItem::GetContentEditablesState() {
  if (content_editables_state_)
    content_editables_state_vector_ = content_editables_state_->ToStateVector();

  return content_editables_state_vector_;
}

void HistoryItem::ClearContentEditablesState() {
  content_editables_state_.Clear();
  content_editables_state_vector_.clear();
}

void HistoryItem::SetDocumentState(const Vector<String>& state) {
  if (state.size() > 1 && state[0] == kDocumentStateVersionMarker &&
      state[1].ToUInt() > 0) {
    size_t form_state_size = state[2].ToUInt();
    DCHECK_LT(form_state_size, state.size());
    size_t end_form_state_idx = 2 + form_state_size;
    size_t content_editables_state_size =
        state[end_form_state_idx + 1].ToUInt();
    DCHECK_EQ(form_state_size + content_editables_state_size + 4, state.size());
    form_state_.clear();
    if (form_state_size > 0)
      form_state_.AppendRange(&state[3], &state[end_form_state_idx + 1]);
    content_editables_state_.Clear();
    if (content_editables_state_size)
      content_editables_state_vector_.AppendRange(
          &state[end_form_state_idx + 2], state.end());
  } else {
    form_state_ = state;
  }
}

void HistoryItem::SetFormState(DocumentFormsState* state) {
  document_forms_state_ = state;
}

Vector<String> HistoryItem::GetReferencedFilePaths() {
  return FormController::GetReferencedFilePaths(FormState());
}

void HistoryItem::GetDocumentState(Vector<String>* state) {
  // Update states.
  FormState();
  GetContentEditablesState();

  state->clear();
  state->push_back(kDocumentStateVersionMarker);
  state->push_back(kDocumentStateVersion);
  state->push_back(String::Number(form_state_.size()));
  state->AppendVector(form_state_);
  state->push_back(String::Number(content_editables_state_vector_.size()));
  state->AppendVector(content_editables_state_vector_);
}

void HistoryItem::ClearDocumentState() {
  form_state_.clear();
  document_forms_state_.Clear();
}

void HistoryItem::SetStateObject(PassRefPtr<SerializedScriptValue> object) {
  state_object_ = std::move(object);
}

const AtomicString& HistoryItem::FormContentType() const {
  return form_content_type_;
}

void HistoryItem::SetFormInfoFromRequest(const ResourceRequest& request) {
  if (DeprecatedEqualIgnoringCase(request.HttpMethod(), "POST")) {
    // FIXME: Eventually we have to make this smart enough to handle the case
    // where we have a stream for the body to handle the "data interspersed with
    // files" feature.
    form_data_ = request.HttpBody();
    form_content_type_ = request.HttpContentType();
  } else {
    form_data_ = nullptr;
    form_content_type_ = g_null_atom;
  }
}

void HistoryItem::SetFormData(PassRefPtr<EncodedFormData> form_data) {
  form_data_ = std::move(form_data);
}

void HistoryItem::SetFormContentType(const AtomicString& form_content_type) {
  form_content_type_ = form_content_type;
}

EncodedFormData* HistoryItem::FormData() {
  return form_data_.Get();
}

ResourceRequest HistoryItem::GenerateResourceRequest(
    WebCachePolicy cache_policy) {
  ResourceRequest request(url_string_);
  request.SetHTTPReferrer(referrer_);
  request.SetCachePolicy(cache_policy);
  if (form_data_) {
    request.SetHTTPMethod(HTTPNames::POST);
    request.SetHTTPBody(form_data_);
    request.SetHTTPContentType(form_content_type_);
    request.AddHTTPOriginIfNeeded(referrer_.referrer);
  }
  return request;
}

DEFINE_TRACE(HistoryItem) {
  visitor->Trace(document_forms_state_);
  visitor->Trace(content_editables_state_);
}

}  // namespace blink
