/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "platform/wtf/text/AtomicString.h"

#include "platform/wtf/text/AtomicStringTable.h"

#if OS(MACOSX)

#include "platform/wtf/text/CString.h"
#include <CoreFoundation/CoreFoundation.h>

namespace WTF {

PassRefPtr<StringImpl> AtomicString::Add(CFStringRef string) {
  if (!string)
    return nullptr;

  CFIndex length = CFStringGetLength(string);

  if (const LChar* ptr = reinterpret_cast<const LChar*>(
          CFStringGetCStringPtr(string, kCFStringEncodingISOLatin1)))
    return AtomicStringTable::Instance().Add(ptr, length);

  if (const UniChar* ptr = CFStringGetCharactersPtr(string))
    return AtomicStringTable::Instance().Add(
        reinterpret_cast<const UChar*>(ptr), length);

  Vector<UniChar, 1024> uchar_buffer(length);
  CFStringGetCharacters(string, CFRangeMake(0, length), uchar_buffer.data());
  return AtomicStringTable::Instance().Add(
      reinterpret_cast<const UChar*>(uchar_buffer.data()), length);
}

}  // namespace WTF

#endif  // OS(MACOSX)
