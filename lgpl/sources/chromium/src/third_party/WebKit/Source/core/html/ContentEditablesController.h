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

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {
class Element;

class ContentEditablesState final : public GarbageCollectedFinalized<ContentEditablesState> {
public:
    static ContentEditablesState* create()
    {
        return new ContentEditablesState;
    }

    ~ContentEditablesState();

    Vector<String> toStateVector();
    void registerContentEditableElement(Element*);
    void restoreContentsIn(Element*);
    void unregisterContentEditableElement(Element*);
    void setContentEditablesContent(const Vector<String>&);
    bool isRegistered(Element*);

    DECLARE_TRACE();

private:
    ContentEditablesState();

    HeapHashMap<Member<Element>, String> m_contenteditablesWithPaths;
    HashMap<String, String> m_savedContents;
};

class ContentEditablesController final : public GarbageCollected<ContentEditablesController>,
                                         public TraceWrapperBase {
public:
    static ContentEditablesController* create()
    {
        return new ContentEditablesController;
    }

    void registerContentEditableElement(Element*);
    void restoreContentsIn(Element*);
    void unregisterContentEditableElement(Element*);
    ContentEditablesState* getContentEditablesState();
    void setContentEditablesContent(const Vector<String>&);
    bool isRegistered(Element*);

    DECLARE_TRACE();

private:
    ContentEditablesController();

    Member<ContentEditablesState> m_state;
};

} // namespace blink

#endif // ContentEditablesController_h
