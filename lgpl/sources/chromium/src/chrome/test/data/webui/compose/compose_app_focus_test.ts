// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://compose/app.js';

import type {ComposeAppElement} from 'chrome-untrusted://compose/app.js';
import { StyleModifier, UserFeedback } from 'chrome-untrusted://compose/compose.mojom-webui.js';
import {ComposeApiProxyImpl} from 'chrome-untrusted://compose/compose_api_proxy.js';
import {ComposeStatus} from 'chrome-untrusted://compose/compose_enums.mojom-webui.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {TestComposeApiProxy} from './test_compose_api_proxy.js';

suite('ComposeApp', function() {
  let testProxy: TestComposeApiProxy;

  async function createApp(): Promise<ComposeAppElement> {
    const app = document.createElement('compose-app');
    document.body.appendChild(app);

    await testProxy.whenCalled('requestInitialState');
    await flushTasks();
    return app;
  }

  setup(async () => {
    testProxy = new TestComposeApiProxy();
    ComposeApiProxyImpl.setInstance(testProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function mockResponse(triggeredFromModifier: boolean = false): Promise<void> {
    testProxy.remote.responseReceived({
      status: ComposeStatus.kOk,
      result: 'some response',
      undoAvailable: false,
      redoAvailable: false,
      providedByUser: false,
      onDeviceEvaluationUsed: false,
      triggeredFromModifier,
    });
    return testProxy.remote.$.flushForTesting();
  }

  test('RefocusesInputOnInvalidate', async () => {
    const app = await createApp();
    app.$.textarea.value = 'short';
    app.$.textarea.dispatchEvent(new CustomEvent('value-changed'));
    app.$.submitButton.focus();
    app.$.submitButton.click();
    await flushTasks();
    assertEquals(app.$.textarea, app.shadowRoot!.activeElement);
  });

  test('FocusesEditInput', async () => {
    testProxy.setOpenMetadata({}, {
      webuiState: JSON.stringify({
        input: 'some input',
        isEditingSubmittedInput: true,
      }),
    });
    const app = await createApp();
    assertEquals(app.$.editTextarea, app.shadowRoot!.activeElement);
  });

  test('FocusesRefreshButtonAfterRefreshRewrite', async () => {
    const app = await createApp();
    app.$.textarea.value = 'test value one';
    app.$.submitButton.click();
    await mockResponse();

    app.$.refreshButton.click();
    await testProxy.whenCalled('rewrite');
    await mockResponse(true);

    assertEquals(app.$.refreshButton, app.shadowRoot!.activeElement);
  });

  test('FocusesEditInputAfterSubmitInput', async () => {
    const app = await createApp();
    app.$.textarea.value = 'test value one';
    app.$.submitButton.click();

    await testProxy.whenCalled('compose');
    await mockResponse();

    assertEquals(app.$.textarea, app.shadowRoot!.activeElement);
  });

  test('FocusesLengthMenuAfterLengthRewrite', async () => {
    const app = await createApp();
    app.$.textarea.value = 'test value';
    app.$.submitButton.click();
    await mockResponse();

    app.$.lengthMenu.value = `${StyleModifier.kLonger}`;
    app.$.lengthMenu.dispatchEvent(new CustomEvent('change'));

    await testProxy.whenCalled('rewrite');
    await mockResponse(true);

    assertEquals(app.$.lengthMenu, app.shadowRoot!.activeElement);
  });

  test('FocusesToneMenuAfterToneRewrite', async () => {
    const app = await createApp();
    app.$.textarea.value = 'test value';
    app.$.submitButton.click();
    await mockResponse();

    app.$.toneMenu.value = `${StyleModifier.kCasual}`;
    app.$.toneMenu.dispatchEvent(new CustomEvent('change'));

    await testProxy.whenCalled('rewrite');
    await mockResponse(true);

    assertEquals(app.$.toneMenu, app.shadowRoot!.activeElement);
  });

  test('FocusesUndoButtonAfterUndoClick', async () => {
    // Set up initial state to show undo button and mock up a previous state.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy.setOpenMetadata({}, {
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: true,
        redoAvailable: false,
        providedByUser: false,
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
    });
    testProxy.setUndoResponse({
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        redoAvailable: false,
        providedByUser: false,
        result: 'some undone result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
      webuiState: JSON.stringify({
        input: 'my old input',
        selectedLength: Number(StyleModifier.kUnset),
        selectedTone: Number(StyleModifier.kUnset),
      }),
      feedback: UserFeedback.kUserFeedbackUnspecified,
    });
    const appWithUndo = document.createElement('compose-app');
    document.body.appendChild(appWithUndo);
    await testProxy.whenCalled('requestInitialState');

    // CLick undo.
    appWithUndo.$.undoButton.click();
    await testProxy.whenCalled('undo');

    assertEquals(
        appWithUndo.$.undoButton, appWithUndo.shadowRoot!.activeElement);
  });

});