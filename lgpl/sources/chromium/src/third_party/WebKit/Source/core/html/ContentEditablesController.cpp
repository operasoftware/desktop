/*
 * Copyright (C) 2013 Opera Software ASA. All rights reserved.
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

#include "core/html/ContentEditablesController.h"

#include "core/html/HTMLElement.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

namespace {
const char kContentEditablesSavedContentsSignature[] = "Blink's contentEditables saved content";
const int kContentEditablesSavedContentsVersion = 1;
} // namespace

void ContentEditablesState::registerContentEditableElement(Element* element)
{
    m_contenteditablesWithPaths.add(element, element->getPath());
}

void ContentEditablesState::unregisterContentEditableElement(Element* element)
{
    m_contenteditablesWithPaths.remove(element);
}

bool ContentEditablesState::isRegistered(Element* element)
{
    return m_contenteditablesWithPaths.contains(element);
}

void ContentEditablesState::restoreContentsIn(Element* element)
{
    if (!m_contenteditablesWithPaths.contains(element))
        return;

    HTMLElement* htmlElement = toHTMLElement(element);
    ASSERT(htmlElement->contentEditable() == "true" || htmlElement->contentEditable() == "plaintext-only");
    String elementPath = htmlElement->getPath();

    if (m_contenteditablesWithPaths.get(htmlElement) == elementPath) {
        String content = m_savedContents.get(elementPath);
        if (!content.isEmpty())
            htmlElement->setInnerHTML(content, IGNORE_EXCEPTION);
    }
}

Vector<String> ContentEditablesState::toStateVector()
{
    Vector<String> result;
    if (m_contenteditablesWithPaths.size()) {
        result.reserveInitialCapacity(m_contenteditablesWithPaths.size() * 2 + 2);
        result.append(kContentEditablesSavedContentsSignature);
        result.append(String::number(kContentEditablesSavedContentsVersion, 0u));
        for (const auto& iter : m_contenteditablesWithPaths) {
            result.append(iter.value);
            result.append(toHTMLElement(iter.key)->innerHTML());
        }
    }
    return result;
}

void ContentEditablesState::setContentEditablesContent(const Vector<String>& contents)
{
    if (contents.size() && contents[0] == kContentEditablesSavedContentsSignature) {
        // i == 1 is version - unused for now.
        for (size_t idx = 2; idx < contents.size(); idx += 2) {
            m_savedContents.add(contents[idx], contents[idx + 1]);
        }
    }
}

ContentEditablesState::ContentEditablesState()
{
}

ContentEditablesState::~ContentEditablesState()
{
}

DEFINE_TRACE(ContentEditablesState)
{
#if ENABLE(OILPAN)
    visitor->trace(m_contenteditablesWithPaths);
#endif
}

ContentEditablesController::ContentEditablesController()
    : m_state(ContentEditablesState::create())
{
}

void ContentEditablesController::registerContentEditableElement(Element* element)
{
    if (!RuntimeEnabledFeatures::restoreContenteditablesStateEnabled())
        return;

    m_state->registerContentEditableElement(element);
}

void ContentEditablesController::unregisterContentEditableElement(Element* element)
{
    m_state->unregisterContentEditableElement(element);
}

bool ContentEditablesController::isRegistered(Element* element)
{
    return m_state->isRegistered(element);
}

void ContentEditablesController::restoreContentsIn(Element* element)
{
    m_state->restoreContentsIn(element);
}

ContentEditablesState* ContentEditablesController::getContentEditablesState()
{
    return m_state.get();
}

void ContentEditablesController::setContentEditablesContent(const Vector<String>& contents)
{
    m_state->setContentEditablesContent(contents);
}

DEFINE_TRACE(ContentEditablesController)
{
    visitor->trace(m_state);
}

} // namespace blink
