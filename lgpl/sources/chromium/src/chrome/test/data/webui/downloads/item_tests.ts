// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {BrowserProxy, CrToastManagerElement, DangerType, DownloadsItemElement, IconLoaderImpl, loadTimeData, States} from 'chrome://downloads/downloads.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createDownload, TestDownloadsProxy, TestIconLoader} from './test_support.js';

suite('item tests', function() {
  let item: DownloadsItemElement;
  let testIconLoader: TestIconLoader;
  let toastManager: CrToastManagerElement;

  setup(function() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;

    // This isn't strictly necessary, but is a probably good idea.
    BrowserProxy.setInstance(new TestDownloadsProxy());

    testIconLoader = new TestIconLoader();
    IconLoaderImpl.setInstance(testIconLoader);

    item = document.createElement('downloads-item');
    document.body.appendChild(item);

    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('dangerous downloads aren\'t linkable', () => {
    item.set('data', createDownload({
               dangerType: DangerType.DANGEROUS_FILE,
               fileExternallyRemoved: false,
               hideDate: true,
               state: States.DANGEROUS,
               url: 'http://evil.com',
             }));
    flush();

    assertTrue(item.$['file-link'].hidden);
    assertFalse(item.$.url.hasAttribute('href'));
  });

  test('icon loads successfully', async () => {
    testIconLoader.setShouldIconsLoad(true);
    item.set('data', createDownload({filePath: 'unique1', hideDate: false}));
    const loadedPath = await testIconLoader.whenCalled('loadIcon');
    assertEquals(loadedPath, 'unique1');
    flush();
    assertFalse(item.getFileIcon().hidden);
  });

  test('icon fails to load', async () => {
    testIconLoader.setShouldIconsLoad(false);
    item.set('data', createDownload({filePath: 'unique2', hideDate: false}));
    item.set('data', createDownload({hideDate: false}));
    const loadedPath = await testIconLoader.whenCalled('loadIcon');
    assertEquals(loadedPath, 'unique2');
    flush();
    assertTrue(item.getFileIcon().hidden);
  });

  test('icon overridden by danger type', async () => {
    testIconLoader.setShouldIconsLoad(true);
    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               dangerType: DangerType.SENSITIVE_CONTENT_BLOCK,
             }));

    assertEquals('cr:error', item.shadowRoot!.querySelector('iron-icon')!.icon);
    assertTrue(item.$['file-icon'].hidden);

    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               dangerType: DangerType.BLOCKED_TOO_LARGE,
             }));

    assertEquals('cr:error', item.shadowRoot!.querySelector('iron-icon')!.icon);
    assertTrue(item.$['file-icon'].hidden);

    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               dangerType: DangerType.BLOCKED_PASSWORD_PROTECTED,
             }));

    assertEquals('cr:error', item.shadowRoot!.querySelector('iron-icon')!.icon);
    assertTrue(item.$['file-icon'].hidden);
  });

  test('open now button controlled by load time data', async () => {
    loadTimeData.overrideValues({'allowOpenNow': true});
    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               state: States.ASYNC_SCANNING,
             }));
    flush();
    assertNotEquals(item.shadowRoot!.querySelector('#openNow'), null);

    loadTimeData.overrideValues({'allowOpenNow': false});
    item.set('data', createDownload({
               filePath: 'unique1',
               hideDate: false,
               state: States.ASYNC_SCANNING,
             }));
    flush();
    assertEquals(item.shadowRoot!.querySelector('#openNow'), null);
  });

  test('undo is shown in toast', () => {
    item.data = createDownload({hideDate: false});
    toastManager.show('', /* hideSlotted= */ true);
    assertTrue(toastManager.slottedHidden);
    item.$.remove.click();
    assertFalse(toastManager.slottedHidden);
  });

  test('undo is not shown in toast when item is dangerous', () => {
    item.data = createDownload({hideDate: false, isDangerous: true});
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    item.$.remove.click();
    assertTrue(toastManager.slottedHidden);
  });

  test('undo is not shown in toast when item is mixed content', () => {
    item.data = createDownload({hideDate: false, isMixedContent: true});
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    item.$.remove.click();
    assertTrue(toastManager.slottedHidden);
  });
});
