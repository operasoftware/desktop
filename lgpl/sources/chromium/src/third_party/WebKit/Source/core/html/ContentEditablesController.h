/*
 * Copyright (C) 2013 Opera Software AS. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Opera ASA nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ContentEditablesController_h
#define ContentEditablesController_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashFunctions.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {
class Element;

class ContentEditablesState final
    : public GarbageCollectedFinalized<ContentEditablesState> {
 public:
  static ContentEditablesState* Create() { return new ContentEditablesState; }

  ~ContentEditablesState();

  Vector<String> ToStateVector();
  void RegisterContentEditableElement(Element*);
  void RestoreContentsIn(Element*);
  void UnregisterContentEditableElement(Element*);
  void SetContentEditablesContent(const Vector<String>&);
  bool IsRegistered(Element*);

  DECLARE_TRACE();

 private:
  ContentEditablesState();

  HeapHashMap<Member<Element>, String> content_editables_with_paths_;
  HashMap<String, String> saved_contents_;
};

class ContentEditablesController final
    : public GarbageCollected<ContentEditablesController>,
      public TraceWrapperBase {
 public:
  static ContentEditablesController* Create() {
    return new ContentEditablesController;
  }

  void RegisterContentEditableElement(Element*);
  void RestoreContentsIn(Element*);
  void UnregisterContentEditableElement(Element*);
  ContentEditablesState* GetContentEditablesState();
  void SetContentEditablesContent(const Vector<String>&);
  bool IsRegistered(Element*);

  DECLARE_TRACE();

 private:
  ContentEditablesController();

  Member<ContentEditablesState> state_;
};

}  // namespace blink

#endif  // ContentEditablesController_h
