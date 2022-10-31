// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = '_test_resources/api_test/webnavigation/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  var getURL = chrome.extension.getURL;
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});

  chrome.test.runTests([
    // Navigates to a.html which includes b.html as an iframe. b.html
    // redirects to c.html.
    function iframe() {
      expect([
        { label: "a-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('a.html') }},
        { label: "a-onCommitted",
          event: "onCommitted",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('a.html') }},
        { label: "a-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('a.html') }},
        { label: "a-onCompleted",
          event: "onCompleted",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('a.html') }},
        { label: "b-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b-onCommitted",
          event: "onCommitted",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "auto_subframe",
                     url: getURL('b.html') }},
        { label: "b-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b-onCompleted",
          event: "onCompleted",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "c-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('c.html') }},
        { label: "c-onCommitted",
          event: "onCommitted",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "auto_subframe",
                     url: getURL('c.html') }},
        { label: "c-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('c.html') }},
        { label: "c-onCompleted",
          event: "onCompleted",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('c.html') }}],
        [ navigationOrder("a-"),
          navigationOrder("b-"),
          navigationOrder("c-"),
          isIFrameOf("b-", "a-"),
          isLoadedBy("c-", "b-")]);
      chrome.tabs.update(tab.id, { url: getURL('a.html') });
    },

    // Navigates to d.html which includes e.html and f.html as iframes. To be
    // able to predict which iframe has which id, the iframe for f.html is
    // created by javascript. f.html then navigates to g.html.
    function iframeMultiple() {
      expect([
        { label: "d-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('d.html') }},
        { label: "d-onCommitted",
          event: "onCommitted",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('d.html') }},
        { label: "d-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('d.html') }},
        { label: "d-onCompleted",
          event: "onCompleted",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('d.html') }},
        { label: "e-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('e.html') }},
        { label: "e-onCommitted",
          event: "onCommitted",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "auto_subframe",
                     url: getURL('e.html') }},
        { label: "e-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('e.html') }},
        { label: "e-onCompleted",
          event: "onCompleted",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('e.html') }},
        { label: "f-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 2,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('f.html') }},
        { label: "f-onCommitted",
          event: "onCommitted",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "auto_subframe",
                     url: getURL('f.html') }},
        { label: "f-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('f.html') }},
        { label: "f-onCompleted",
          event: "onCompleted",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('f.html') }},
        { label: "g-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 2,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('g.html') }},
        { label: "g-onCommitted",
          event: "onCommitted",
          details: { documentId: 4,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "auto_subframe",
                     url: getURL('g.html') }},
        { label: "g-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 4,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('g.html') }},
        { label: "g-onCompleted",
          event: "onCompleted",
          details: { documentId: 4,
                     documentLifecycle: "active",
                     frameId: 2,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('g.html') }}],
        [ navigationOrder("d-"),
          navigationOrder("e-"),
          navigationOrder("f-"),
          navigationOrder("g-"),
          isIFrameOf("e-", "d-"),
          ["d-onDOMContentLoaded", "f-onBeforeNavigate", "f-onCompleted",
           "d-onCompleted"],
          isLoadedBy("g-", "f-")]);
      chrome.tabs.update(tab.id, { url: getURL('d.html') });
    },

    // Navigates to h.html which includes i.html that triggers a navigation
    // on the main frame.
    function iframeNavigate() {
      expect([
        { label: "h-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('h.html') }},
        { label: "h-onCommitted",
          event: "onCommitted",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('h.html') }},
        { label: "h-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('h.html') }},
        { label: "h-onCompleted",
          event: "onCompleted",
          details: { documentId: 1,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('h.html') }},
        { label: "i-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('i.html') }},
        { label: "i-onCommitted",
          event: "onCommitted",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "auto_subframe",
                     url: getURL('i.html') }},
        { label: "i-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('i.html') }},
        { label: "i-onCompleted",
          event: "onCompleted",
          details: { documentId: 2,
                     documentLifecycle: "active",
                     frameId: 1,
                     frameType: "sub_frame",
                     parentDocumentId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('i.html') }},
        { label: "c-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('c.html') }},
        { label: "c-onCommitted",
          event: "onCommitted",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: ['maybe_client_redirect'],
                     transitionType: "link",
                     url: getURL('c.html') }},
        { label: "c-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('c.html') }},
        { label: "c-onCompleted",
          event: "onCompleted",
          details: { documentId: 3,
                     documentLifecycle: "active",
                     frameId: 0,
                     frameType: "outermost_frame",
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('c.html') }}],
        [ navigationOrder("h-"),
          navigationOrder("i-"),
          navigationOrder("c-"),
          isIFrameOf("i-", "h-"),
          isLoadedBy("c-", "i-") ]);
      chrome.tabs.update(tab.id, { url: getURL('h.html') });
    },
  ]);
});
