// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('main.html', {
    bounds: { width: 400, height: 200 }
  }, function() {});
});
