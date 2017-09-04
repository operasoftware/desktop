/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "core/workers/SharedWorkerThread.h"

#include <memory>
#include "core/workers/SharedWorkerGlobalScope.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<SharedWorkerThread> SharedWorkerThread::Create(
    const String& name,
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    WorkerReportingProxy& worker_reporting_proxy) {
  return WTF::WrapUnique(new SharedWorkerThread(
      name, std::move(worker_loader_proxy), worker_reporting_proxy));
}

SharedWorkerThread::SharedWorkerThread(
    const String& name,
    PassRefPtr<WorkerLoaderProxy> worker_loader_proxy,
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(std::move(worker_loader_proxy), worker_reporting_proxy),
      worker_backing_thread_(
          WorkerBackingThread::Create("SharedWorker Thread")),
      name_(name.IsolatedCopy()) {}

SharedWorkerThread::~SharedWorkerThread() {}

void SharedWorkerThread::ClearWorkerBackingThread() {
  worker_backing_thread_ = nullptr;
}

WorkerOrWorkletGlobalScope* SharedWorkerThread::CreateWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startup_data) {
  return SharedWorkerGlobalScope::Create(name_, this, std::move(startup_data));
}

}  // namespace blink
