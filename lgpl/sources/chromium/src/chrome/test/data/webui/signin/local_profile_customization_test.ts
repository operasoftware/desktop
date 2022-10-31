// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/lazy_load.js';

import {LocalProfileCustomizationElement} from 'chrome://profile-picker/lazy_load.js';
import {AutogeneratedThemeColorInfo, ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isChildVisible, waitBeforeNextRender} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('LocalProfileCustomizationTest', function() {
  let customizeProfileElement: LocalProfileCustomizationElement;
  let browserProxy: TestManageProfilesBrowserProxy;
  const defaultAvatarIndex: number = 26;

  async function resetCustomizeProfileElement() {
    document.body.innerHTML = '';
    customizeProfileElement =
        document.createElement('local-profile-customization');
    customizeProfileElement.profileThemeInfo = browserProxy.profileThemeInfo;
    document.body.appendChild(customizeProfileElement);
    await browserProxy.whenCalled('getProfileThemeInfo');
    browserProxy.resetResolver('getProfileThemeInfo');
    await waitBeforeNextRender(customizeProfileElement);
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    return resetCustomizeProfileElement();
  });

  async function setProfileTheme(theme: AutogeneratedThemeColorInfo) {
    browserProxy.setProfileThemeInfo(theme);
    customizeProfileElement.shadowRoot!.querySelector('cr-customize-themes')!
        .selectedTheme = {
      type: 2,
      info: {
        chromeThemeId: browserProxy.profileThemeInfo.colorId,
        autogeneratedThemeColors: undefined,
        thirdPartyThemeInfo: undefined,
      },
      isForced: false,
    };
    await browserProxy.whenCalled('getProfileThemeInfo');
    browserProxy.resetResolver('getProfileThemeInfo');
  }

  async function verifyCreateProfileCalledWithParams(
      profileName: string, profileColor: number, avatarIndex: number,
      createShortcut: boolean) {
    const args = await browserProxy.whenCalled('createProfile');
    assertEquals(args[0], profileName);
    assertEquals(args[1], profileColor);
    assertEquals(args[2], avatarIndex);
    assertEquals(args[3], createShortcut);
    browserProxy.resetResolver('createProfile');
  }

  test('ProfileName', async function() {
    const profileNameInput = customizeProfileElement.$.nameInput;
    assertTrue(isChildVisible(customizeProfileElement, '#nameInput'));
    assertFalse(profileNameInput.invalid);
    assertTrue(customizeProfileElement.$.save.disabled);

    // Invalid profile name.
    profileNameInput.value = '\t';
    assertTrue(profileNameInput.invalid);
    profileNameInput.value = ' ';
    assertTrue(profileNameInput.invalid);
    assertTrue(customizeProfileElement.$.save.disabled);
    // Valid profil name.
    profileNameInput.value = 'Work';
    assertFalse(profileNameInput.invalid);
    assertFalse(customizeProfileElement.$.save.disabled);
    customizeProfileElement.$.save.click();
    await verifyCreateProfileCalledWithParams(
        'Work', browserProxy.profileThemeInfo.color, defaultAvatarIndex, false);
  });

  test('selectAvatarDialog', async function() {
    function getProfileAvatarSelectorIconUrl(item: HTMLElement) {
      return getComputedStyle(item).backgroundImage.split('/').pop()!.split(
          '"')[0];
    }

    function verifyAvatarSelected(item: HTMLElement) {
      flush();
      const avatarGrid = customizeProfileElement.shadowRoot!
                             .querySelector('cr-profile-avatar-selector')!
                             .shadowRoot!.querySelector('#avatar-grid')!;
      assertEquals(avatarGrid.querySelectorAll('.iron-selected').length, 1);
      assertTrue(item.classList.contains('iron-selected'));
      const displayedAvatarUrl =
          (customizeProfileElement.shadowRoot!.querySelector('img')!.src)
              .split('/')
              .pop();
      assertEquals(getProfileAvatarSelectorIconUrl(item), displayedAvatarUrl);
    }
    await browserProxy.whenCalled('getAvailableIcons');
    assertTrue(
        isChildVisible(customizeProfileElement, '#customizeAvatarIcon')!,
    );
    customizeProfileElement.shadowRoot!
        .querySelector<HTMLElement>('#customizeAvatarIcon')!.click();
    assertTrue(customizeProfileElement.$.selectAvatarDialog.open);
    flush();

    const items =
        customizeProfileElement.shadowRoot!
            .querySelector('cr-profile-avatar-selector')!.shadowRoot!
            .querySelector('#avatar-grid')!.querySelectorAll<HTMLElement>(
                '.avatar');
    assertEquals(items.length, 4);
    assertEquals(
        getProfileAvatarSelectorIconUrl(items[0]!),
        browserProxy.profileThemeInfo.themeGenericAvatar);
    verifyAvatarSelected(items[0]!);

    // Select custom avatar
    items[1]!.click();
    flush();
    verifyAvatarSelected(items[1]!);
    assertEquals(getProfileAvatarSelectorIconUrl(items[1]!), 'fake-icon-1.png');

    // Simulate theme changes with custom avatar selected.
    const themeInfo = Object.assign({}, browserProxy.profileThemeInfo);
    themeInfo.themeGenericAvatar = 'AvatarUrl-7';
    await setProfileTheme(themeInfo);
    assertEquals(
        getProfileAvatarSelectorIconUrl(items[0]!),
        themeInfo.themeGenericAvatar);
    verifyAvatarSelected(items[1]!);

    // Theme changes with generic avatar selected.
    items[0]!.click();
    flush();
    verifyAvatarSelected(items[0]!);
    themeInfo.themeGenericAvatar = 'AvatarUrl-8';
    await setProfileTheme(Object.assign({}, themeInfo));
    assertEquals(
        getProfileAvatarSelectorIconUrl(items[0]!),
        themeInfo.themeGenericAvatar);
    verifyAvatarSelected(items[0]!);

    // Create profile with custom avatar.
    items[3]!.click();
    flush();
    verifyAvatarSelected(items[3]!);
    assertEquals(getProfileAvatarSelectorIconUrl(items[3]!), 'fake-icon-3.png');

    // Close the dialog.
    assertTrue(!!customizeProfileElement.$.doneButton);
    customizeProfileElement.$.doneButton.click();
    assertFalse(customizeProfileElement.$.selectAvatarDialog.open);

    customizeProfileElement.$.nameInput.value = 'Work';
    assertFalse(customizeProfileElement.$.save.disabled);
    customizeProfileElement.$.save.click();
    await verifyCreateProfileCalledWithParams(
        'Work', browserProxy.profileThemeInfo.color, 3, false);
  });

  test('ThemeSelectionChanges', async function() {
    function verifyAppliedTheme() {
      assertEquals(
          getComputedStyle(customizeProfileElement.shadowRoot!.querySelector(
                               '#headerContainer')!)
              .backgroundColor,
          browserProxy.profileThemeInfo.themeFrameColor);
      assertEquals(
          getComputedStyle(customizeProfileElement.$.backButton)
              .getPropertyValue('--cr-icon-button-fill-color')
              .trim(),
          browserProxy.profileThemeInfo.themeFrameTextColor);
      assertEquals(
          getComputedStyle(
              customizeProfileElement.shadowRoot!.querySelector('#title')!)
              .color,
          browserProxy.profileThemeInfo.themeFrameTextColor);
      assertEquals(
          (customizeProfileElement.shadowRoot!.querySelector('img')!.src)
              .split('/')
              .pop(),
          browserProxy.profileThemeInfo.themeGenericAvatar);
    }
    assertTrue(isChildVisible(customizeProfileElement, '#colorPicker'));
    verifyAppliedTheme();
    await setProfileTheme({
      color: -3413569,
      colorId: 7,
      themeFrameColor: 'rgb(203, 233, 191)',
      themeFrameTextColor: 'rgb(32, 33, 36)',
      themeGenericAvatar: 'AvatarUrl-7',
      themeShapeColor: 'rgb(255, 255, 255)',
    });
    verifyAppliedTheme();
    assertTrue(customizeProfileElement.$.save.disabled);
    customizeProfileElement.$.nameInput.value = 'Personal';
    customizeProfileElement.$.save.click();
    await verifyCreateProfileCalledWithParams(
        'Personal', browserProxy.profileThemeInfo.color, defaultAvatarIndex,
        false);
  });

  test('createShortcut', async function() {
    let createShortcut =
        customizeProfileElement.shadowRoot!.querySelector('cr-checkbox')!;
    assertTrue(!!createShortcut);
    assertTrue(createShortcut!.hidden);
    loadTimeData.overrideValues({
      profileShortcutsEnabled: true,
    });
    await resetCustomizeProfileElement();
    createShortcut =
        customizeProfileElement.shadowRoot!.querySelector('cr-checkbox')!;
    assertTrue(isChildVisible(customizeProfileElement, '#nameInput'));
    assertFalse(createShortcut!.hidden);
    assertTrue(createShortcut!.checked);
    createShortcut!.click();
    assertFalse(createShortcut!.checked);
    customizeProfileElement.$.nameInput.value = 'Personal';
    customizeProfileElement.$.save.click();
    await verifyCreateProfileCalledWithParams(
        'Personal', browserProxy.profileThemeInfo.color, defaultAvatarIndex,
        false);
    // Profile creation in progress should disable the save button.
    assertTrue(customizeProfileElement.$.save.disabled);
    // Fire profile creation finished.
    webUIListenerCallback('create-profile-finished');
    flushTasks();
    assertFalse(customizeProfileElement.$.save.disabled);
    createShortcut!.click();
    assertTrue(createShortcut!.checked);
    customizeProfileElement.$.save.click();
    await verifyCreateProfileCalledWithParams(
        'Personal', browserProxy.profileThemeInfo.color, defaultAvatarIndex,
        true);
  });
});
