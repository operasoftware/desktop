// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.ContentSettings

var cs = chrome.contentSettings;
var default_content_settings = {
  "cookies": "session_only",
  "images": "allow",
  "javascript": "block",
  "popups": "block",
  "location": "ask",
  "notifications": "ask",
  "fullscreen": "ask",
  "mouselock": "ask",
  "microphone": "ask",
  "camera": "ask",
  "automaticDownloads": "ask"
};

var settings = {
  'cookies': 'block',
  'images': 'allow',
  'javascript': 'block',
  'popups': 'allow',
  'location': 'block',
  'notifications': 'block',
  'fullscreen': 'block',  // Should be ignored.
  'mouselock': 'block',   // Should be ignored.
  'plugins': 'block',     // Should be ignored.
  'microphone': 'block',
  'camera': 'block',
  'automaticDownloads': 'block'
};

// List of settings that are expected to return different values than were
// written, due to deprecation. For example, "fullscreen" is set to "block" but
// we expect this to be ignored, and so read back as "allow". Any setting
// omitted from this list is expected to read back whatever was written.
var deprecatedSettingsExpectations = {
  // Due to deprecation, these should be "allow", regardless of the setting.
  'fullscreen': 'allow',
  'mouselock': 'allow',
  'plugins': 'block'
};

Object.prototype.forEach = function(f) {
  var k;
  for (k in this) {
    if (this.hasOwnProperty(k))
      f(k, this[k]);
  }
};

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

function expectFalse(message) {
  return expect({
    "value": false,
    "levelOfControl": "controllable_by_this_extension"
  }, message);
}

chrome.test.runTests([
  function setDefaultContentSettings() {
    default_content_settings.forEach(function(type, setting) {
      cs[type].set({
        'primaryPattern': '<all_urls>',
        'secondaryPattern': '<all_urls>',
        'setting': setting
      }, chrome.test.callbackPass());
    });
  },
  function setContentSettings() {
    settings.forEach(function(type, setting) {
      cs[type].set({
        'primaryPattern': 'http://*.google.com/*',
        'secondaryPattern': 'http://*.google.com/*',
        'setting': setting
      }, chrome.test.callbackPass());
    });
  },
  function getContentSettings() {
    settings.forEach(function(type, setting) {
      setting = deprecatedSettingsExpectations[type] || setting;
      var message = "Setting for " + type + " should be " + setting;
      cs[type].get({
        'primaryUrl': 'http://www.google.com',
        'secondaryUrl': 'http://www.google.com'
      }, expect({'setting':setting}, message));
    });
  },
  function invalidSettings() {
    cs.cookies.get({
      'primaryUrl': 'moo'
    }, chrome.test.callbackFail("The URL \"moo\" is invalid."));
    cs.javascript.set({
      'primaryPattern': 'http://example.com/*',
      'secondaryPattern': 'http://example.com/path',
      'setting': 'block'
    }, chrome.test.callbackFail("Specific paths are not allowed."));
    cs.javascript.set({
      'primaryPattern': 'http://example.com/*',
      'secondaryPattern': 'file:///home/hansmoleman/*',
      'setting': 'allow'
    }, chrome.test.callbackFail(
        "Path wildcards in file URL patterns are not allowed."));
    var caught = false;
    try {
      cs.javascript.set({primaryPattern: '<all_urls>',
                         secondaryPattern: '<all_urls>',
                         setting: 'something radically fake'});
    } catch (e) {
      caught = true;
    }
    chrome.test.assertTrue(caught);
  },
  function testPluginsApi_Set() {
    cs['plugins'].set(
        {
          'primaryPattern': 'https://*.google.com:443/*',
          'secondaryPattern': '<all_urls>',
          'setting': 'allow'
        },
        () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  function testPluginsApi_Get() {
    cs['plugins'].get(
        {'primaryUrl': 'https://drive.google.com:443/*'}, (value) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq({setting: 'block'}, value);
          chrome.test.succeed();
        });
  },
  function testPluginsApi_Clear() {
    cs['plugins'].clear({}, () => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  }
]);
