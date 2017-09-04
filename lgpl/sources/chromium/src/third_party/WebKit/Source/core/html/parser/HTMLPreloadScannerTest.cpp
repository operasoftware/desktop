// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLPreloadScanner.h"

#include <memory>
#include "core/MediaTypeNames.h"
#include "core/css/MediaValuesCached.h"
#include "core/frame/Settings.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLResourcePreloader.h"
#include "core/html/parser/PreloadRequest.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

struct TestCase {
  const char* base_url;
  const char* input_html;
  const char* preloaded_url;  // Or nullptr if no preload is expected.
  const char* output_base_url;
  Resource::Type type;
  int resource_width;
  ClientHintsPreferences preferences;
};

struct PreconnectTestCase {
  const char* base_url;
  const char* input_html;
  const char* preconnected_host;
  CrossOriginAttributeValue cross_origin;
};

struct ReferrerPolicyTestCase {
  const char* base_url;
  const char* input_html;
  const char* preloaded_url;  // Or nullptr if no preload is expected.
  const char* output_base_url;
  Resource::Type type;
  int resource_width;
  ReferrerPolicy referrer_policy;
  // Expected referrer header of the preload request, or nullptr if the header
  // shouldn't be checked (and no network request should be created).
  const char* expected_referrer;
};

struct NonceTestCase {
  const char* base_url;
  const char* input_html;
  const char* nonce;
};

struct ContextTestCase {
  const char* base_url;
  const char* input_html;
  const char* preloaded_url;  // Or nullptr if no preload is expected.
  bool is_image_set;
};

class MockHTMLResourcePreloader : public ResourcePreloader {
 public:
  void PreloadRequestVerification(Resource::Type type,
                                  const char* url,
                                  const char* base_url,
                                  int width,
                                  const ClientHintsPreferences& preferences) {
    if (!url) {
      EXPECT_FALSE(preload_request_);
      return;
    }
    EXPECT_NE(nullptr, preload_request_.get());
    if (preload_request_) {
      EXPECT_FALSE(preload_request_->IsPreconnect());
      EXPECT_EQ(type, preload_request_->ResourceType());
      EXPECT_STREQ(url, preload_request_->ResourceURL().Ascii().data());
      EXPECT_STREQ(base_url,
                   preload_request_->BaseURL().GetString().Ascii().data());
      EXPECT_EQ(width, preload_request_->ResourceWidth());
      EXPECT_EQ(preferences.ShouldSendDPR(),
                preload_request_->Preferences().ShouldSendDPR());
      EXPECT_EQ(preferences.ShouldSendResourceWidth(),
                preload_request_->Preferences().ShouldSendResourceWidth());
      EXPECT_EQ(preferences.ShouldSendViewportWidth(),
                preload_request_->Preferences().ShouldSendViewportWidth());
    }
  }

  void PreloadRequestVerification(Resource::Type type,
                                  const char* url,
                                  const char* base_url,
                                  int width,
                                  ReferrerPolicy referrer_policy) {
    PreloadRequestVerification(type, url, base_url, width,
                               ClientHintsPreferences());
    EXPECT_EQ(referrer_policy, preload_request_->GetReferrerPolicy());
  }

  void PreloadRequestVerification(Resource::Type type,
                                  const char* url,
                                  const char* base_url,
                                  int width,
                                  ReferrerPolicy referrer_policy,
                                  Document* document,
                                  const char* expected_referrer) {
    PreloadRequestVerification(type, url, base_url, width, referrer_policy);
    Resource* resource = preload_request_->Start(document);
    ASSERT_TRUE(resource);
    EXPECT_EQ(expected_referrer, resource->GetResourceRequest().HttpReferrer());
  }

  void PreconnectRequestVerification(const String& host,
                                     CrossOriginAttributeValue cross_origin) {
    if (!host.IsNull()) {
      EXPECT_TRUE(preload_request_->IsPreconnect());
      EXPECT_STREQ(preload_request_->ResourceURL().Ascii().data(),
                   host.Ascii().data());
      EXPECT_EQ(preload_request_->CrossOrigin(), cross_origin);
    }
  }

  void NonceRequestVerification(const char* nonce) {
    ASSERT_TRUE(preload_request_.get());
    if (strlen(nonce))
      EXPECT_EQ(nonce, preload_request_->Nonce());
    else
      EXPECT_TRUE(preload_request_->Nonce().IsEmpty());
  }

  void ContextVerification(bool is_image_set) {
    ASSERT_TRUE(preload_request_.get());
    EXPECT_EQ(preload_request_->IsImageSetForTestingOnly(), is_image_set);
  }

 protected:
  void Preload(std::unique_ptr<PreloadRequest> preload_request,
               const NetworkHintsInterface&) override {
    preload_request_ = std::move(preload_request);
  }

 private:
  std::unique_ptr<PreloadRequest> preload_request_;
};

class HTMLPreloadScannerTest : public testing::Test {
 protected:
  enum ViewportState {
    kViewportEnabled,
    kViewportDisabled,
  };

  enum PreloadState {
    kPreloadEnabled,
    kPreloadDisabled,
  };

  HTMLPreloadScannerTest() : dummy_page_holder_(DummyPageHolder::Create()) {}

  MediaValuesCached::MediaValuesCachedData CreateMediaValuesData() {
    MediaValuesCached::MediaValuesCachedData data;
    data.viewport_width = 500;
    data.viewport_height = 600;
    data.device_width = 700;
    data.device_height = 800;
    data.device_pixel_ratio = 2.0;
    data.color_bits_per_component = 24;
    data.monochrome_bits_per_component = 0;
    data.primary_pointer_type = kPointerTypeFine;
    data.default_font_size = 16;
    data.three_d_enabled = true;
    data.media_type = MediaTypeNames::screen;
    data.strict_mode = true;
    data.display_mode = kWebDisplayModeBrowser;
    return data;
  }

  void RunSetUp(
      ViewportState viewport_state,
      PreloadState preload_state = kPreloadEnabled,
      ReferrerPolicy document_referrer_policy = kReferrerPolicyDefault) {
    HTMLParserOptions options(&dummy_page_holder_->GetDocument());
    KURL document_url(kParsedURLString, "http://whatever.test/");
    dummy_page_holder_->GetDocument().SetURL(document_url);
    dummy_page_holder_->GetDocument().SetSecurityOrigin(
        SecurityOrigin::Create(document_url));
    dummy_page_holder_->GetDocument().GetSettings()->SetViewportEnabled(
        viewport_state == kViewportEnabled);
    dummy_page_holder_->GetDocument().GetSettings()->SetViewportMetaEnabled(
        viewport_state == kViewportEnabled);
    dummy_page_holder_->GetDocument().GetSettings()->SetDoHtmlPreloadScanning(
        preload_state == kPreloadEnabled);
    dummy_page_holder_->GetDocument().SetReferrerPolicy(
        document_referrer_policy);
    scanner_ = HTMLPreloadScanner::Create(
        options, document_url,
        CachedDocumentParameters::Create(&dummy_page_holder_->GetDocument()),
        CreateMediaValuesData(),
        TokenPreloadScanner::ScannerType::kMainDocument);
  }

  void SetUp() override { RunSetUp(kViewportEnabled); }

  void Test(TestCase test_case) {
    MockHTMLResourcePreloader preloader;
    KURL base_url(kParsedURLString, test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests = scanner_->Scan(base_url, nullptr);
    preloader.TakeAndPreload(requests);

    preloader.PreloadRequestVerification(
        test_case.type, test_case.preloaded_url, test_case.output_base_url,
        test_case.resource_width, test_case.preferences);
  }

  void Test(PreconnectTestCase test_case) {
    MockHTMLResourcePreloader preloader;
    KURL base_url(kParsedURLString, test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests = scanner_->Scan(base_url, nullptr);
    preloader.TakeAndPreload(requests);
    preloader.PreconnectRequestVerification(test_case.preconnected_host,
                                            test_case.cross_origin);
  }

  void Test(ReferrerPolicyTestCase test_case) {
    MockHTMLResourcePreloader preloader;
    KURL base_url(kParsedURLString, test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests = scanner_->Scan(base_url, nullptr);
    preloader.TakeAndPreload(requests);

    if (test_case.expected_referrer) {
      preloader.PreloadRequestVerification(
          test_case.type, test_case.preloaded_url, test_case.output_base_url,
          test_case.resource_width, test_case.referrer_policy,
          &dummy_page_holder_->GetDocument(), test_case.expected_referrer);
    } else {
      preloader.PreloadRequestVerification(
          test_case.type, test_case.preloaded_url, test_case.output_base_url,
          test_case.resource_width, test_case.referrer_policy);
    }
  }

  void Test(NonceTestCase test_case) {
    MockHTMLResourcePreloader preloader;
    KURL base_url(kParsedURLString, test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests = scanner_->Scan(base_url, nullptr);
    preloader.TakeAndPreload(requests);

    preloader.NonceRequestVerification(test_case.nonce);
  }

  void Test(ContextTestCase test_case) {
    MockHTMLResourcePreloader preloader;
    KURL base_url(kParsedURLString, test_case.base_url);
    scanner_->AppendToEnd(String(test_case.input_html));
    PreloadRequestStream requests = scanner_->Scan(base_url, nullptr);
    preloader.TakeAndPreload(requests);

    preloader.ContextVerification(test_case.is_image_set);
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  std::unique_ptr<HTMLPreloadScanner> scanner_;
};

TEST_F(HTMLPreloadScannerTest, testImages) {
  TestCase test_cases[] = {
      {"http://example.test", "<img src='bla.gif'>", "bla.gif",
       "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif",
       "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif",
       "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif",
       "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w'>", "bla2.gif",
       "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w'>",
       "bla3.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif "
       "500w' sizes='50vw'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif' sizes='50vw'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif "
       "500w'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewport) {
  TestCase test_cases[] = {
      {"http://example.test",
       "<meta name=viewport content='width=160'><img srcset='bla.gif 320w, "
       "blabla.gif 640w'>",
       "bla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<img src='bla.gif'>", "bla.gif",
       "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif",
       "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif",
       "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif",
       "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w'>", "bla2.gif",
       "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w'>",
       "bla2.gif", "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla2.gif", "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif "
       "500w' sizes='50vw'>",
       "bla2.gif", "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla2.gif", "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif'>",
       "bla2.gif", "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif' sizes='50vw'>",
       "bla2.gif", "http://example.test/", Resource::kImage, 80},
      {"http://example.test",
       "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' "
       "src='bla.gif'>",
       "bla2.gif", "http://example.test/", Resource::kImage, 80},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewportDeviceWidth) {
  TestCase test_cases[] = {
      {"http://example.test",
       "<meta name=viewport content='width=device-width'><img srcset='bla.gif "
       "320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<img src='bla.gif'>", "bla.gif",
       "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif",
       "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif",
       "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif",
       "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w'>", "bla2.gif",
       "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w'>",
       "bla3.gif", "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif "
       "500w' sizes='50vw'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif' sizes='50vw'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 350},
      {"http://example.test",
       "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 350},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewportDisabled) {
  RunSetUp(kViewportDisabled);
  TestCase test_cases[] = {
      {"http://example.test",
       "<meta name=viewport content='width=160'><img src='bla.gif'>", "bla.gif",
       "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif",
       "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif",
       "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif",
       "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w'>", "bla2.gif",
       "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w'>",
       "bla3.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif "
       "500w' sizes='50vw'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, "
       "bla4.gif 500w'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' "
       "src='bla.gif' sizes='50vw'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' "
       "src='bla.gif'>",
       "bla4.gif", "http://example.test/", Resource::kImage, 250},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testViewportNoContent) {
  TestCase test_cases[] = {
      {"http://example.test",
       "<meta name=viewport><img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<meta name=viewport content=sdkbsdkjnejjha><img srcset='bla.gif 320w, "
       "blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testMetaAcceptCH) {
  ClientHintsPreferences dpr;
  ClientHintsPreferences resource_width;
  ClientHintsPreferences all;
  ClientHintsPreferences viewport_width;
  dpr.SetShouldSendDPR(true);
  all.SetShouldSendDPR(true);
  resource_width.SetShouldSendResourceWidth(true);
  all.SetShouldSendResourceWidth(true);
  viewport_width.SetShouldSendViewportWidth(true);
  all.SetShouldSendViewportWidth(true);
  TestCase test_cases[] = {
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='bla'><img srcset='bla.gif 320w, "
       "blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='dprw'><img srcset='bla.gif 320w, "
       "blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<meta http-equiv='accept-ch'><img srcset='bla.gif 320w, blabla.gif "
       "640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='dpr \t'><img srcset='bla.gif "
       "320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0, dpr},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='bla,dpr \t'><img srcset='bla.gif "
       "320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0, dpr},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  width  '><img sizes='100vw' "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 500,
       resource_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  width  , wutever'><img "
       "sizes='300px' srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 300,
       resource_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  '><img "
       "srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0,
       viewport_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  , "
       "wutever'><img srcset='bla.gif 320w, blabla.gif 640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 0,
       viewport_width},
      {"http://example.test",
       "<meta http-equiv='accept-ch' content='  viewport-width  ,width, "
       "wutever, dpr \t'><img sizes='90vw' srcset='bla.gif 320w, blabla.gif "
       "640w'>",
       "blabla.gif", "http://example.test/", Resource::kImage, 450, all},
  };

  for (const auto& test_case : test_cases) {
    RunSetUp(kViewportDisabled);
    Test(test_case);
  }
}

TEST_F(HTMLPreloadScannerTest, testPreconnect) {
  PreconnectTestCase test_cases[] = {
      {"http://example.test", "<link rel=preconnect href=http://example2.test>",
       "http://example2.test", kCrossOriginAttributeNotSet},
      {"http://example.test",
       "<link rel=preconnect href=http://example2.test crossorigin=anonymous>",
       "http://example2.test", kCrossOriginAttributeAnonymous},
      {"http://example.test",
       "<link rel=preconnect href=http://example2.test "
       "crossorigin='use-credentials'>",
       "http://example2.test", kCrossOriginAttributeUseCredentials},
      {"http://example.test",
       "<link rel=preconnected href=http://example2.test "
       "crossorigin='use-credentials'>",
       nullptr, kCrossOriginAttributeNotSet},
      {"http://example.test",
       "<link rel=preconnect href=ws://example2.test "
       "crossorigin='use-credentials'>",
       nullptr, kCrossOriginAttributeNotSet},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testDisables) {
  RunSetUp(kViewportEnabled, kPreloadDisabled);

  TestCase test_cases[] = {
      {"http://example.test", "<img src='bla.gif'>"},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testPicture) {
  TestCase test_cases[] = {
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif'><img src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<picture><source sizes='50vw' srcset='srcset_bla.gif'><img "
       "src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<picture><source sizes='50vw' srcset='srcset_bla.gif'><img "
       "sizes='50vw' src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif' sizes='50vw'><img "
       "sizes='50vw' src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif'><img sizes='50vw' "
       "src='bla.gif'></picture>",
       "srcset_bla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<picture><source media='(max-width: 900px)' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "srcset_bla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<picture><source media='(max-width: 400px)' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<picture><source type='image/webp' srcset='srcset_bla.gif'><img "
       "sizes='50vw' srcset='bla.gif 500w'></picture>",
       "srcset_bla.gif", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<picture><source type='image/jp2' srcset='srcset_bla.gif'><img "
       "sizes='50vw' srcset='bla.gif 500w'></picture>",
       "bla.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<picture><source media='(max-width: 900px)' type='image/jp2' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<picture><source type='image/webp' media='(max-width: 400px)' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<picture><source type='image/jp2' media='(max-width: 900px)' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", Resource::kImage, 250},
      {"http://example.test",
       "<picture><source media='(max-width: 400px)' type='image/webp' "
       "srcset='srcset_bla.gif'><img sizes='50vw' srcset='bla.gif "
       "500w'></picture>",
       "bla.gif", "http://example.test/", Resource::kImage, 250},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testContext) {
  ContextTestCase test_cases[] = {
      {"http://example.test",
       "<picture><source srcset='srcset_bla.gif'><img src='bla.gif'></picture>",
       "srcset_bla.gif", true},
      {"http://example.test", "<img src='bla.gif'>", "bla.gif", false},
      {"http://example.test", "<img srcset='bla.gif'>", "bla.gif", true},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testReferrerPolicy) {
  ReferrerPolicyTestCase test_cases[] = {
      {"http://example.test", "<img src='bla.gif'/>", "bla.gif",
       "http://example.test/", Resource::kImage, 0, kReferrerPolicyDefault},
      {"http://example.test", "<img referrerpolicy='origin' src='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyOrigin, nullptr},
      {"http://example.test",
       "<meta name='referrer' content='not-a-valid-policy'><img "
       "src='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyDefault, nullptr},
      {"http://example.test",
       "<img referrerpolicy='origin' referrerpolicy='origin-when-cross-origin' "
       "src='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyOrigin, nullptr},
      {"http://example.test",
       "<img referrerpolicy='not-a-valid-policy' src='bla.gif'/>", "bla.gif",
       "http://example.test/", Resource::kImage, 0, kReferrerPolicyDefault,
       nullptr},
      {"http://example.test",
       "<link rel=preload as=image referrerpolicy='origin-when-cross-origin' "
       "href='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyOriginWhenCrossOrigin, nullptr},
      {"http://example.test",
       "<link rel='stylesheet' href='sheet.css' type='text/css'>", "sheet.css",
       "http://example.test/", Resource::kCSSStyleSheet, 0,
       kReferrerPolicyDefault, nullptr},
      {"http://example.test",
       "<link rel=preload as=image referrerpolicy='origin' "
       "referrerpolicy='origin-when-cross-origin' href='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyOrigin, nullptr},
      {"http://example.test",
       "<meta name='referrer' content='no-referrer'><img "
       "referrerpolicy='origin' src='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyOrigin, nullptr},
      // The scanner's state is not reset between test cases, so all subsequent
      // test cases have a document referrer policy of no-referrer.
      {"http://example.test",
       "<link rel=preload as=image referrerpolicy='not-a-valid-policy' "
       "href='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyNever, nullptr},
      {"http://example.test",
       "<img referrerpolicy='not-a-valid-policy' src='bla.gif'/>", "bla.gif",
       "http://example.test/", Resource::kImage, 0, kReferrerPolicyNever,
       nullptr},
      {"http://example.test", "<img src='bla.gif'/>", "bla.gif",
       "http://example.test/", Resource::kImage, 0, kReferrerPolicyNever,
       nullptr}};

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testNonce) {
  NonceTestCase test_cases[] = {
      {"http://example.test", "<script src='/script'></script>", ""},
      {"http://example.test", "<script src='/script' nonce=''></script>", ""},
      {"http://example.test", "<script src='/script' nonce='abc'></script>",
       "abc"},
      {"http://example.test", "<link rel='import' href='/import'>", ""},
      {"http://example.test", "<link rel='import' href='/import' nonce=''>",
       ""},
      {"http://example.test", "<link rel='import' href='/import' nonce='abc'>",
       "abc"},
      {"http://example.test", "<link rel='stylesheet' href='/style'>", ""},
      {"http://example.test", "<link rel='stylesheet' href='/style' nonce=''>",
       ""},
      {"http://example.test",
       "<link rel='stylesheet' href='/style' nonce='abc'>", "abc"},

      // <img> doesn't support nonces:
      {"http://example.test", "<img src='/image'>", ""},
      {"http://example.test", "<img src='/image' nonce=''>", ""},
      {"http://example.test", "<img src='/image' nonce='abc'>", ""},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.input_html);
    Test(test_case);
  }
}

// Tests that a document-level referrer policy (e.g. one set by HTTP header) is
// applied for preload requests.
TEST_F(HTMLPreloadScannerTest, testReferrerPolicyOnDocument) {
  RunSetUp(kViewportEnabled, kPreloadEnabled, kReferrerPolicyOrigin);
  ReferrerPolicyTestCase test_cases[] = {
      {"http://example.test", "<img src='blah.gif'/>", "blah.gif",
       "http://example.test/", Resource::kImage, 0, kReferrerPolicyOrigin,
       nullptr},
      {"http://example.test", "<style>@import url('blah.css');</style>",
       "blah.css", "http://example.test/", Resource::kCSSStyleSheet, 0,
       kReferrerPolicyOrigin, nullptr},
      // Tests that a meta-delivered referrer policy with an unrecognized policy
      // value does not override the document's referrer policy.
      {"http://example.test",
       "<meta name='referrer' content='not-a-valid-policy'><img "
       "src='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyOrigin, nullptr},
      // Tests that a meta-delivered referrer policy with a valid policy value
      // does override the document's referrer policy.
      {"http://example.test",
       "<meta name='referrer' content='unsafe-url'><img src='bla.gif'/>",
       "bla.gif", "http://example.test/", Resource::kImage, 0,
       kReferrerPolicyAlways, nullptr},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testLinkRelPreload) {
  TestCase test_cases[] = {
      {"http://example.test", "<link rel=preload href=bla>", "bla",
       "http://example.test/", Resource::kRaw, 0},
      {"http://example.test", "<link rel=preload href=bla as=script>", "bla",
       "http://example.test/", Resource::kScript, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=script type='script/foo'>", "bla",
       "http://example.test/", Resource::kScript, 0},
      {"http://example.test", "<link rel=preload href=bla as=style>", "bla",
       "http://example.test/", Resource::kCSSStyleSheet, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=style type='text/css'>", "bla",
       "http://example.test/", Resource::kCSSStyleSheet, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=style type='text/bla'>", nullptr,
       "http://example.test/", Resource::kCSSStyleSheet, 0},
      {"http://example.test", "<link rel=preload href=bla as=image>", "bla",
       "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=image type='image/webp'>", "bla",
       "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=image type='image/bla'>", nullptr,
       "http://example.test/", Resource::kImage, 0},
      {"http://example.test", "<link rel=preload href=bla as=font>", "bla",
       "http://example.test/", Resource::kFont, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=font type='font/woff2'>", "bla",
       "http://example.test/", Resource::kFont, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=font type='font/bla'>", nullptr,
       "http://example.test/", Resource::kFont, 0},
      {"http://example.test", "<link rel=preload href=bla as=video>", "bla",
       "http://example.test/", Resource::kMedia, 0},
      {"http://example.test", "<link rel=preload href=bla as=track>", "bla",
       "http://example.test/", Resource::kTextTrack, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=image media=\"(max-width: 800px)\">",
       "bla", "http://example.test/", Resource::kImage, 0},
      {"http://example.test",
       "<link rel=preload href=bla as=image media=\"(max-width: 400px)\">",
       nullptr, "http://example.test/", Resource::kImage, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, testNoDataUrls) {
  TestCase test_cases[] = {
      {"http://example.test",
       "<link rel=preload href='data:text/html,<p>data</data>'>", nullptr,
       "http://example.test/", Resource::kRaw, 0},
      {"http://example.test", "<img src='data:text/html,<p>data</data>'>",
       nullptr, "http://example.test/", Resource::kImage, 0},
      {"data:text/html,<a>anchor</a>", "<img src='#anchor'>", nullptr,
       "http://example.test/", Resource::kImage, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

// The preload scanner should follow the same policy that the ScriptLoader does
// with regard to the type and language attribute.
TEST_F(HTMLPreloadScannerTest, testScriptTypeAndLanguage) {
  TestCase test_cases[] = {
      // Allow empty src and language attributes.
      {"http://example.test", "<script src='test.js'></script>", "test.js",
       "http://example.test/", Resource::kScript, 0},
      {"http://example.test",
       "<script type='' language='' src='test.js'></script>", "test.js",
       "http://example.test/", Resource::kScript, 0},
      // Allow standard language and type attributes.
      {"http://example.test",
       "<script type='text/javascript' src='test.js'></script>", "test.js",
       "http://example.test/", Resource::kScript, 0},
      {"http://example.test",
       "<script type='text/javascript' language='javascript' "
       "src='test.js'></script>",
       "test.js", "http://example.test/", Resource::kScript, 0},
      // Allow legacy languages in the "language" attribute with an empty
      // type.
      {"http://example.test",
       "<script language='javascript1.1' src='test.js'></script>", "test.js",
       "http://example.test/", Resource::kScript, 0},
      // Allow legacy languages in the "type" attribute.
      {"http://example.test",
       "<script type='javascript' src='test.js'></script>", "test.js",
       "http://example.test/", Resource::kScript, 0},
      {"http://example.test",
       "<script type='javascript1.7' src='test.js'></script>", "test.js",
       "http://example.test/", Resource::kScript, 0},
      // Do not allow invalid types in the "type" attribute.
      {"http://example.test", "<script type='invalid' src='test.js'></script>",
       nullptr, "http://example.test/", Resource::kScript, 0},
      {"http://example.test", "<script type='asdf' src='test.js'></script>",
       nullptr, "http://example.test/", Resource::kScript, 0},
      // Do not allow invalid languages.
      {"http://example.test",
       "<script language='french' src='test.js'></script>", nullptr,
       "http://example.test/", Resource::kScript, 0},
      {"http://example.test",
       "<script language='python' src='test.js'></script>", nullptr,
       "http://example.test/", Resource::kScript, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

// Regression test for crbug.com/664744.
TEST_F(HTMLPreloadScannerTest, testUppercaseAsValues) {
  TestCase test_cases[] = {
      {"http://example.test", "<link rel=preload href=bla as=SCRIPT>", "bla",
       "http://example.test/", Resource::kScript, 0},
      {"http://example.test", "<link rel=preload href=bla as=fOnT>", "bla",
       "http://example.test/", Resource::kFont, 0},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

TEST_F(HTMLPreloadScannerTest, ReferrerHeader) {
  RunSetUp(kViewportEnabled, kPreloadEnabled, kReferrerPolicyAlways);

  KURL preload_url(kParsedURLString, "http://example.test/sheet.css");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      preload_url, WrappedResourceResponse(ResourceResponse()), "");

  ReferrerPolicyTestCase test_case = {
      "http://example.test",
      "<link rel='stylesheet' href='sheet.css' type='text/css'>",
      "sheet.css",
      "http://example.test/",
      Resource::kCSSStyleSheet,
      0,
      kReferrerPolicyAlways,
      "http://whatever.test/"};
  Test(test_case);
}

}  // namespace blink
