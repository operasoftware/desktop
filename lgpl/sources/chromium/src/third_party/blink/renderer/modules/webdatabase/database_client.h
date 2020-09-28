/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 *     * Neither the name of Google Inc. nor the names of its
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_CLIENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Database;
class ExecutionContext;
class InspectorDatabaseAgent;
class Page;

class MODULES_EXPORT DatabaseClient : public GarbageCollected<DatabaseClient>,
                                      public Supplement<Page> {
  USING_GARBAGE_COLLECTED_MIXIN(DatabaseClient);

 public:
  static const char kSupplementName[];

  DatabaseClient();

  void Trace(Visitor*) const override;

  bool AllowDatabase(ExecutionContext*);

  void DidOpenDatabase(Database*,
                       const String& domain,
                       const String& name,
                       const String& version);

  static DatabaseClient* FromPage(Page*);
  static DatabaseClient* From(ExecutionContext*);

  void SetInspectorAgent(InspectorDatabaseAgent*);

 private:
  Member<InspectorDatabaseAgent> inspector_agent_;

  DISALLOW_COPY_AND_ASSIGN(DatabaseClient);
};

MODULES_EXPORT void ProvideDatabaseClientTo(Page&, DatabaseClient*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_CLIENT_H_
