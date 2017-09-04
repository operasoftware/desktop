/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#include "platform/loader/fetch/FetchParameters.h"

#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/Suborigin.h"

namespace blink {

FetchParameters::FetchParameters(const ResourceRequest& resource_request,
                                 const AtomicString& initiator,
                                 const String& charset)
    : resource_request_(resource_request),
      charset_(charset),
      options_(ResourceFetcher::DefaultResourceOptions()),
      speculative_preload_type_(SpeculativePreloadType::kNotSpeculative),
      preload_discovery_time_(0.0),
      defer_(kNoDefer),
      origin_restriction_(kUseDefaultOriginRestrictionForType),
      placeholder_image_request_type_(kDisallowPlaceholder) {
  options_.initiator_info.name = initiator;
}

FetchParameters::FetchParameters(const ResourceRequest& resource_request,
                                 const AtomicString& initiator,
                                 const ResourceLoaderOptions& options)
    : resource_request_(resource_request),
      options_(options),
      speculative_preload_type_(SpeculativePreloadType::kNotSpeculative),
      preload_discovery_time_(0.0),
      defer_(kNoDefer),
      origin_restriction_(kUseDefaultOriginRestrictionForType),
      placeholder_image_request_type_(
          PlaceholderImageRequestType::kDisallowPlaceholder) {
  options_.initiator_info.name = initiator;
}

FetchParameters::FetchParameters(const ResourceRequest& resource_request,
                                 const FetchInitiatorInfo& initiator)
    : resource_request_(resource_request),
      options_(ResourceFetcher::DefaultResourceOptions()),
      speculative_preload_type_(SpeculativePreloadType::kNotSpeculative),
      preload_discovery_time_(0.0),
      defer_(kNoDefer),
      origin_restriction_(kUseDefaultOriginRestrictionForType),
      placeholder_image_request_type_(
          PlaceholderImageRequestType::kDisallowPlaceholder) {
  options_.initiator_info = initiator;
}

FetchParameters::~FetchParameters() {}

void FetchParameters::SetCrossOriginAccessControl(
    SecurityOrigin* origin,
    CrossOriginAttributeValue cross_origin) {
  DCHECK_NE(cross_origin, kCrossOriginAttributeNotSet);
  // Per https://w3c.github.io/webappsec-suborigins/#security-model-opt-outs,
  // credentials are forced when credentials mode is "same-origin", the
  // 'unsafe-credentials' option is set, and the request's physical origin is
  // the same as the URL's.
  const bool suborigin_policy_forces_credentials =
      origin->HasSuborigin() &&
      origin->GetSuborigin()->PolicyContains(
          Suborigin::SuboriginPolicyOptions::kUnsafeCredentials) &&
      SecurityOrigin::Create(Url())->IsSameSchemeHostPort(origin);
  const bool use_credentials =
      cross_origin == kCrossOriginAttributeUseCredentials ||
      suborigin_policy_forces_credentials;
  const bool is_same_origin_request =
      origin && origin->CanRequestNoSuborigin(resource_request_.Url());

  // Currently FetchParametersMode and FetchCredentialsMode are only used when
  // the request goes to Service Worker.
  resource_request_.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
  resource_request_.SetFetchCredentialsMode(
      use_credentials ? WebURLRequest::kFetchCredentialsModeInclude
                      : WebURLRequest::kFetchCredentialsModeSameOrigin);

  if (is_same_origin_request || use_credentials) {
    options_.allow_credentials = kAllowStoredCredentials;
    resource_request_.SetAllowStoredCredentials(true);
  } else {
    options_.allow_credentials = kDoNotAllowStoredCredentials;
    resource_request_.SetAllowStoredCredentials(false);
  }
  options_.cors_enabled = kIsCORSEnabled;
  options_.security_origin = origin;
  options_.credentials_requested = use_credentials
                                       ? kClientRequestedCredentials
                                       : kClientDidNotRequestCredentials;

  // TODO: Credentials should be removed only when the request is cross origin.
  resource_request_.RemoveUserAndPassFromURL();

  if (origin)
    resource_request_.SetHTTPOrigin(origin);
}

void FetchParameters::SetResourceWidth(ResourceWidth resource_width) {
  if (resource_width.is_set) {
    resource_width_.width = resource_width.width;
    resource_width_.is_set = true;
  }
}

void FetchParameters::SetSpeculativePreloadType(
    SpeculativePreloadType speculative_preload_type,
    double discovery_time) {
  speculative_preload_type_ = speculative_preload_type;
  preload_discovery_time_ = discovery_time;
}

void FetchParameters::MakeSynchronous() {
  // Synchronous requests should always be max priority, lest they hang the
  // renderer.
  resource_request_.SetPriority(kResourceLoadPriorityHighest);
  resource_request_.SetTimeoutInterval(10);
  options_.synchronous_policy = kRequestSynchronously;
}

void FetchParameters::SetAllowImagePlaceholder() {
  DCHECK_EQ(kDisallowPlaceholder, placeholder_image_request_type_);
  if (!resource_request_.Url().ProtocolIsInHTTPFamily() ||
      resource_request_.HttpMethod() != "GET" ||
      !resource_request_.HttpHeaderField("range").IsNull()) {
    // Make sure that the request isn't marked as using Client Lo-Fi, since
    // without loading an image placeholder, Client Lo-Fi isn't really in use.
    resource_request_.SetPreviewsState(resource_request_.GetPreviewsState() &
                                       ~(WebURLRequest::kClientLoFiOn));
    return;
  }

  placeholder_image_request_type_ = kAllowPlaceholder;

  // Fetch the first few bytes of the image. This number is tuned to both (a)
  // likely capture the entire image for small images and (b) likely contain
  // the dimensions for larger images.
  // TODO(sclittle): Calculate the optimal value for this number.
  resource_request_.SetHTTPHeaderField("range", "bytes=0-2047");

  // TODO(sclittle): Indicate somehow (e.g. through a new request bit) to the
  // embedder that it should return the full resource if the entire resource is
  // fresh in the cache.
}

}  // namespace blink
