// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguageHelper, LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType, SettingsLanguagesPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestLanguageSettingsMetricsProxy} from './test_languages_settings_metrics_proxy.js';

suite('LanguagesPageMetricsBrowser', function() {
  let languageHelper: LanguageHelper;
  let languagesPage: SettingsLanguagesPageElement;
  let browserProxy: TestLanguagesBrowserProxy;
  let languageSettingsMetricsProxy: TestLanguageSettingsMetricsProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableDesktopDetailedLanguageSettings: false,
    });
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    document.body.innerHTML = '';
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs()) as
        unknown as typeof chrome.settingsPrivate;
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);
    return CrSettingsPrefs.initialized.then(function() {
      // Sets up test browser proxy.
      browserProxy = new TestLanguagesBrowserProxy();
      LanguagesBrowserProxyImpl.setInstance(browserProxy);

      // Sets up test browser proxy.
      languageSettingsMetricsProxy = new TestLanguageSettingsMetricsProxy();
      LanguageSettingsMetricsProxyImpl.setInstance(
          languageSettingsMetricsProxy);

      // Sets up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      (languageSettingsPrivate as unknown as FakeLanguageSettingsPrivate)
          .setSettingsPrefs(settingsPrefs);

      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      languagesPage = document.createElement('settings-languages-page');

      // Prefs would normally be data-bound to settings-languages-page.
      languagesPage.prefs = settingsLanguages.prefs;
      fakeDataBind(settingsLanguages, languagesPage, 'prefs');

      languagesPage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, languagesPage, 'language-helper');

      languagesPage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, languagesPage, 'languages');

      document.body.appendChild(languagesPage);
      languageHelper = languagesPage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  test('records when adding languages', async () => {
    languagesPage.shadowRoot!.querySelector<HTMLElement>(
        '#addLanguages')!.click();
    flush();

    assertEquals(
        LanguageSettingsPageImpressionType.ADD_LANGUAGE,
        await languageSettingsMetricsProxy.whenCalled(
            'recordPageImpressionMetric'));
  });

  test('records when disabling translate.enable toggle', async () => {
    languagesPage.setPrefValue('translate.enabled', true);
    languagesPage.shadowRoot!
        .querySelector<HTMLElement>('#offerTranslateOtherLanguages')!.click();
    flush();

    assertEquals(
        LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test('records when enabling translate.enable toggle', async () => {
    languagesPage.setPrefValue('translate.enabled', false);
    languagesPage.shadowRoot!
        .querySelector<HTMLElement>('#offerTranslateOtherLanguages')!.click();
    flush();

    assertEquals(
        LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test('records when three-dot menu is opened', async () => {
    const menuButtons =
        languagesPage.shadowRoot!.querySelector('#languagesSection')!
            .querySelectorAll<HTMLElement>(
                '.list-item cr-icon-button.icon-more-vert');

    menuButtons[0]!.click();
    assertEquals(
        LanguageSettingsPageImpressionType.LANGUAGE_OVERFLOW_MENU_OPENED,
        await languageSettingsMetricsProxy.whenCalled(
            'recordPageImpressionMetric'));
  });

  test('records when ticking translate checkbox', async () => {
    const menuButtons =
        languagesPage.shadowRoot!.querySelector('#languagesSection')!
            .querySelectorAll<HTMLElement>(
                '.list-item cr-icon-button.icon-more-vert');

    // Chooses the second language to change translate checkbox
    // as first language is the language used for translation.
    menuButtons[1]!.click();
    flush();
    const actionMenu = languagesPage.$.menu.get();
    assertTrue(actionMenu.open);
    const item = actionMenu.querySelector<HTMLElement>('#offerTranslations');
    assertTrue(!!item);

    item!.click();
    assertEquals(
        LanguageSettingsActionType.DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));

    languageSettingsMetricsProxy.resetResolver('recordSettingsMetric');
    item!.click();
    assertEquals(
        LanguageSettingsActionType.ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  // <if expr="is_win">
  test('records when chrome language is changed', async () => {
    // Adding language with supportsUI = true in
    // fake_language_settings_private.ts
    languageHelper.enableLanguage('sw');
    // Testing the 'Change Chrome Language' button with 'sw'
    const languagesSection =
        languagesPage.shadowRoot!.querySelector('#languagesSection');
    assertTrue(!!languagesSection);
    const menuButton = languagesSection.querySelector<HTMLElement>(
        '.list-item cr-icon-button#more-sw');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();
    const actionMenu = languagesPage.$.menu.get();
    assertTrue(actionMenu.open);
    const item = actionMenu.querySelector<HTMLElement>('#uiLanguageItem');
    assertTrue(!!item);
    item.click();
    assertEquals(
        LanguageSettingsActionType.CHANGE_CHROME_LANGUAGE,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });
  // </if>

  test('records on language list reorder', async () => {
    // Add several languages.
    for (const language of ['en-CA', 'en-US', 'tk', 'no']) {
      languageHelper.enableLanguage(language);
    }

    flush();

    const menuButtons =
        languagesPage.shadowRoot!.querySelector('#languagesSection')!
            .querySelectorAll<HTMLElement>(
                '.list-item cr-icon-button.icon-more-vert');

    menuButtons[1]!.click();
    const actionMenu = languagesPage.$.menu.get();
    assertTrue(actionMenu.open);

    function getMenuItem(i18nKey: string): HTMLElement {
      const i18nString = loadTimeData.getString(i18nKey);
      assertTrue(!!i18nString);
      const menuItems =
          actionMenu.querySelectorAll<HTMLElement>('.dropdown-item');
      const menuItem = Array.from(menuItems).find(
          item => item.textContent!.trim() === i18nString);
      assertTrue(!!menuItem, 'Menu item "' + i18nKey + '" not found');
      return menuItem!;
    }

    let moveButton = getMenuItem('moveUp');
    moveButton.click();
    moveButton = getMenuItem('moveDown');
    moveButton.click();
    moveButton = getMenuItem('moveToTop');
    moveButton.click();
    assertEquals(
        3, languageSettingsMetricsProxy.getCallCount('recordSettingsMetric'));
  });
});
