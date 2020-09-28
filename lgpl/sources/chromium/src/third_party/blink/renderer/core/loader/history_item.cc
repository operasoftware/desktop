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

#include "third_party/blink/renderer/core/loader/history_item.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/content_editables_controller.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {
const char kDocumentStateVersionMarker[] = "Version";
const char kDocumentStateVersion[] = "1";
}  // namespace

static int64_t GenerateSequenceNumber() {
  // Initialize to the current time to reduce the likelihood of generating
  // identifiers that overlap with those from past/future browser sessions.
  static int64_t next =
      static_cast<int64_t>(base::Time::Now().ToDoubleT() * 1000000.0);
  return ++next;
}

HistoryItem::HistoryItem()
    : item_sequence_number_(GenerateSequenceNumber()),
      document_sequence_number_(GenerateSequenceNumber()),
      scroll_restoration_type_(kScrollRestorationAuto) {}

HistoryItem::~HistoryItem() = default;

const String& HistoryItem::UrlString() const {
  return url_string_;
}

KURL HistoryItem::Url() const {
  return KURL(url_string_);
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

void HistoryItem::SetVisualViewportScrollOffset(const ScrollOffset& offset) {
  if (!view_state_)
    view_state_ = base::make_optional<ViewState>();
  view_state_->visual_viewport_scroll_offset_ = offset;
}

void HistoryItem::SetScrollOffset(const ScrollOffset& offset) {
  if (!view_state_)
    view_state_ = base::make_optional<ViewState>();
  view_state_->scroll_offset_ = offset;
}

void HistoryItem::SetPageScaleFactor(float scale_factor) {
  if (!view_state_)
    view_state_ = base::make_optional<ViewState>();
  view_state_->page_scale_factor_ = scale_factor;
}

void HistoryItem::SetScrollAnchorData(
    const ScrollAnchorData& scroll_anchor_data) {
  if (!view_state_)
    view_state_ = base::make_optional<ViewState>();
  view_state_->scroll_anchor_data_ = scroll_anchor_data;
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
    unsigned int form_state_size = state[2].ToUInt();
    DCHECK_LT(form_state_size, state.size());
    unsigned int end_form_state_idx = 2 + form_state_size;
    unsigned int content_editables_state_size =
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

void HistoryItem::SetStateObject(scoped_refptr<SerializedScriptValue> object) {
  state_object_ = std::move(object);
}

const AtomicString& HistoryItem::FormContentType() const {
  return form_content_type_;
}

void HistoryItem::SetFormData(scoped_refptr<EncodedFormData> form_data) {
  form_data_ = std::move(form_data);
}

void HistoryItem::SetFormContentType(const AtomicString& form_content_type) {
  form_content_type_ = form_content_type;
}

EncodedFormData* HistoryItem::FormData() {
  return form_data_.get();
}

ResourceRequest HistoryItem::GenerateResourceRequest(
    mojom::FetchCacheMode cache_mode) {
  ResourceRequest request(url_string_);
  request.SetReferrerString(referrer_.referrer);
  request.SetReferrerPolicy(referrer_.referrer_policy);
  request.SetCacheMode(cache_mode);
  if (form_data_) {
    request.SetHttpMethod(http_names::kPOST);
    request.SetHttpBody(form_data_);
    request.SetHTTPContentType(form_content_type_);
    request.SetHTTPOriginToMatchReferrerIfNeeded();
  }
  return request;
}

void HistoryItem::Trace(Visitor* visitor) const {
  visitor->Trace(document_forms_state_);
  visitor->Trace(content_editables_state_);
}

}  // namespace blink
