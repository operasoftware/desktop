/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "modules/webgl/WebGLSharedObject.h"

#include "modules/webgl/WebGLContextGroup.h"
#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLSharedObject::WebGLSharedObject(WebGLRenderingContextBase* context)
    : WebGLObject(context), context_group_(this, context->ContextGroup()) {}

bool WebGLSharedObject::Validate(const WebGLContextGroup* context_group,
                                 const WebGLRenderingContextBase*) const {
  // The contexts and context groups no longer maintain references to all
  // the objects they ever created, so there's no way to invalidate them
  // eagerly during context loss. The invalidation is discovered lazily.
  return context_group == context_group_ &&
         CachedNumberOfContextLosses() ==
             context_group->NumberOfContextLosses();
}

uint32_t WebGLSharedObject::CurrentNumberOfContextLosses() const {
  return context_group_->NumberOfContextLosses();
}

gpu::gles2::GLES2Interface* WebGLSharedObject::GetAGLInterface() const {
  return context_group_->GetAGLInterface();
}

DEFINE_TRACE(WebGLSharedObject) {
  visitor->Trace(context_group_);
  WebGLObject::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(WebGLSharedObject) {
  visitor->TraceWrappers(context_group_);
  WebGLObject::TraceWrappers(visitor);
}

}  // namespace blink
