// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setFullscreen(checkbox) {
  if (checkbox.checked) {
    document.body.webkitRequestFullScreen();
  } else {
    document.webkitCancelFullScreen();
  }
}
function onFullscreenChange() {
  finishJavaScriptStep();
}
document.addEventListener('webkitfullscreenchange', onFullscreenChange, false);
window.addEventListener('load', finishJavaScriptStep, false);