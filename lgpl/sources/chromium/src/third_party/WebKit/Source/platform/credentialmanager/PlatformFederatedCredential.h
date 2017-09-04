// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformFederatedCredential_h
#define PlatformFederatedCredential_h

#include "platform/credentialmanager/PlatformCredential.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT PlatformFederatedCredential final
    : public PlatformCredential {
  WTF_MAKE_NONCOPYABLE(PlatformFederatedCredential);

 public:
  static PlatformFederatedCredential* Create(
      const String& id,
      PassRefPtr<SecurityOrigin> provider,
      const String& name,
      const KURL& icon_url);
  ~PlatformFederatedCredential() override;

  PassRefPtr<SecurityOrigin> Provider() const { return provider_; }

  bool IsFederated() override { return true; }

 private:
  PlatformFederatedCredential(const String& id,
                              PassRefPtr<SecurityOrigin> provider,
                              const String& name,
                              const KURL& icon_url);

  RefPtr<SecurityOrigin> provider_;
};

}  // namespace blink

#endif  // PlatformFederatedCredential_h
