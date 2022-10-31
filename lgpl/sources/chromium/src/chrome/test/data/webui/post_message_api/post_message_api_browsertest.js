// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

function PostMessageAPIModuleTest() {}

PostMessageAPIModuleTest.prototype = {

  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload:
      'chrome://chrome-signin/test_loader.html?module=post_message_api/post_message_api_test.js&host=test',

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],

  isAsync: true,
};

// Test the postmessage api server and client by defining server methods, and
// passing a series of postmessages.
TEST_F('PostMessageAPIModuleTest', 'PostMessageCommTest', function() {
  runMochaTest('PostMessageAPIModuleTest', 'PostMessageCommTest');
});
