// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('Launched', function(response) {
  chrome.test.assertEq('reload', response);
  chrome.runtime.reload();
});
