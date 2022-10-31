// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {runTests} from './test_module.js';

chrome.test.sendMessage('ready', reply => {
  if (reply === 'go')
    runTests();
});
