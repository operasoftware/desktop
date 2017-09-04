/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All
 * rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/dom/CharacterData.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MutationObserverInterestGroup.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/Text.h"
#include "core/editing/FrameSelection.h"
#include "core/events/MutationEvent.h"
#include "core/probe/CoreProbes.h"
#include "platform/wtf/CheckedNumeric.h"

namespace blink {

void CharacterData::Atomize() {
  data_ = AtomicString(data_);
}

void CharacterData::setData(const String& data) {
  const String& non_null_data = !data.IsNull() ? data : g_empty_string;
  unsigned old_length = length();

  SetDataAndUpdate(non_null_data, 0, old_length, non_null_data.length(),
                   kUpdateFromNonParser);
  GetDocument().DidRemoveText(*this, 0, old_length);
}

String CharacterData::substringData(unsigned offset,
                                    unsigned count,
                                    ExceptionState& exception_state) {
  if (offset > length()) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The offset " + String::Number(offset) +
                             " is greater than the node's length (" +
                             String::Number(length()) + ").");
    return String();
  }

  return data_.Substring(offset, count);
}

void CharacterData::ParserAppendData(const String& data) {
  String new_str = data_ + data;

  SetDataAndUpdate(new_str, data_.length(), 0, data.length(),
                   kUpdateFromParser);
}

void CharacterData::appendData(const String& data) {
  String new_str = data_ + data;

  SetDataAndUpdate(new_str, data_.length(), 0, data.length(),
                   kUpdateFromNonParser);

  // FIXME: Should we call textInserted here?
}

void CharacterData::insertData(unsigned offset,
                               const String& data,
                               ExceptionState& exception_state) {
  if (offset > length()) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The offset " + String::Number(offset) +
                             " is greater than the node's length (" +
                             String::Number(length()) + ").");
    return;
  }

  String new_str = data_;
  new_str.insert(data, offset);

  SetDataAndUpdate(new_str, offset, 0, data.length(), kUpdateFromNonParser);

  GetDocument().DidInsertText(*this, offset, data.length());
}

static bool ValidateOffsetCount(unsigned offset,
                                unsigned count,
                                unsigned length,
                                unsigned& real_count,
                                ExceptionState& exception_state) {
  if (offset > length) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The offset " + String::Number(offset) +
                             " is greater than the node's length (" +
                             String::Number(length) + ").");
    return false;
  }

  CheckedNumeric<unsigned> offset_count = offset;
  offset_count += count;

  if (!offset_count.IsValid() || offset + count > length)
    real_count = length - offset;
  else
    real_count = count;

  return true;
}

void CharacterData::deleteData(unsigned offset,
                               unsigned count,
                               ExceptionState& exception_state) {
  unsigned real_count = 0;
  if (!ValidateOffsetCount(offset, count, length(), real_count,
                           exception_state))
    return;

  String new_str = data_;
  new_str.Remove(offset, real_count);

  SetDataAndUpdate(new_str, offset, real_count, 0, kUpdateFromNonParser);

  GetDocument().DidRemoveText(*this, offset, real_count);
}

void CharacterData::replaceData(unsigned offset,
                                unsigned count,
                                const String& data,
                                ExceptionState& exception_state) {
  unsigned real_count = 0;
  if (!ValidateOffsetCount(offset, count, length(), real_count,
                           exception_state))
    return;

  String new_str = data_;
  new_str.Remove(offset, real_count);
  new_str.insert(data, offset);

  SetDataAndUpdate(new_str, offset, real_count, data.length(),
                   kUpdateFromNonParser);

  // update DOM ranges
  GetDocument().DidRemoveText(*this, offset, real_count);
  GetDocument().DidInsertText(*this, offset, data.length());
}

String CharacterData::nodeValue() const {
  return data_;
}

bool CharacterData::ContainsOnlyWhitespace() const {
  return data_.ContainsOnlyWhitespace();
}

void CharacterData::setNodeValue(const String& node_value) {
  setData(node_value);
}

void CharacterData::SetDataAndUpdate(const String& new_data,
                                     unsigned offset_of_replaced_data,
                                     unsigned old_length,
                                     unsigned new_length,
                                     UpdateSource source) {
  String old_data = data_;
  data_ = new_data;

  DCHECK(!GetLayoutObject() || IsTextNode());
  if (IsTextNode())
    ToText(this)->UpdateTextLayoutObject(offset_of_replaced_data, old_length);

  if (source != kUpdateFromParser) {
    if (getNodeType() == kProcessingInstructionNode)
      ToProcessingInstruction(this)->DidAttributeChanged();

    GetDocument().NotifyUpdateCharacterData(this, offset_of_replaced_data,
                                            old_length, new_length);
  }

  GetDocument().IncDOMTreeVersion();
  DidModifyData(old_data, source);
}

void CharacterData::DidModifyData(const String& old_data, UpdateSource source) {
  if (MutationObserverInterestGroup* mutation_recipients =
          MutationObserverInterestGroup::CreateForCharacterDataMutation(*this))
    mutation_recipients->EnqueueMutationRecord(
        MutationRecord::CreateCharacterData(this, old_data));

  if (parentNode()) {
    ContainerNode::ChildrenChange change = {
        ContainerNode::kTextChanged, this, previousSibling(), nextSibling(),
        ContainerNode::kChildrenChangeSourceAPI};
    parentNode()->ChildrenChanged(change);
  }

  // Skip DOM mutation events if the modification is from parser.
  // Note that mutation observer events will still fire.
  // Spec: https://html.spec.whatwg.org/multipage/syntax.html#insert-a-character
  if (source != kUpdateFromParser && !IsInShadowTree()) {
    if (GetDocument().HasListenerType(
            Document::kDOMCharacterDataModifiedListener))
      DispatchScopedEvent(
          MutationEvent::Create(EventTypeNames::DOMCharacterDataModified, true,
                                nullptr, old_data, data_));
    DispatchSubtreeModifiedEvent();
  }
  probe::characterDataModified(this);
}

int CharacterData::MaxCharacterOffset() const {
  return static_cast<int>(length());
}

}  // namespace blink
