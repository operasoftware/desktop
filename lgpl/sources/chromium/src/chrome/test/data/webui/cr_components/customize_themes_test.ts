// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://resources/cr_components/customize_themes/theme_icon.js';
import 'chrome://resources/cr_components/customize_themes/customize_themes.js';

import {CustomizeThemesBrowserProxy, CustomizeThemesBrowserProxyImpl} from 'chrome://resources/cr_components/customize_themes/browser_proxy.js';
import {CustomizeThemesElement} from 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import {ChromeTheme, CustomizeThemesClientCallbackRouter, CustomizeThemesHandlerInterface, ThemeType} from 'chrome://resources/cr_components/customize_themes/customize_themes.mojom-webui.js';
import {ThemeIconElement} from 'chrome://resources/cr_components/customize_themes/theme_icon.js';

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

/**
 * Asserts the computed style value for an element.
 * @param name The name of the style to assert.
 * @param expected The expected style value.
 */
function assertStyle(element: Element, name: string, expected: string) {
  assertTrue(!!element);
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual);
}

class TestCustomizeThemesHandler extends TestBrowserProxy implements
    CustomizeThemesHandlerInterface {
  private chromeThemes_: ChromeTheme[] = [];

  constructor() {
    super([
      'applyAutogeneratedTheme',
      'applyChromeTheme',
      'applyDefaultTheme',
      'initializeTheme',
      'getChromeThemes',
      'confirmThemeChanges',
      'revertThemeChanges',
    ]);
  }

  setChromeThemes(chromeThemes: ChromeTheme[]) {
    this.chromeThemes_ = chromeThemes;
  }

  applyAutogeneratedTheme(frameColor: SkColor) {
    this.methodCalled('applyAutogeneratedTheme', frameColor);
  }

  applyChromeTheme(id: number) {
    this.methodCalled('applyChromeTheme', id);
  }

  applyDefaultTheme() {
    this.methodCalled('applyDefaultTheme');
  }

  initializeTheme() {
    this.methodCalled('initializeTheme');
  }

  getChromeThemes() {
    this.methodCalled('getChromeThemes');
    return Promise.resolve({chromeThemes: this.chromeThemes_});
  }

  confirmThemeChanges() {
    this.methodCalled('confirmThemeChanges');
  }

  revertThemeChanges() {
    this.methodCalled('revertThemeChanges');
  }
}

class TestCustomizeThemesBrowserProxy extends TestBrowserProxy implements
    CustomizeThemesBrowserProxy {
  testHandler = new TestCustomizeThemesHandler();
  private callbackRouter_: CustomizeThemesClientCallbackRouter;

  constructor() {
    super(['open']);

    this.testHandler = new TestCustomizeThemesHandler();
    this.callbackRouter_ = new CustomizeThemesClientCallbackRouter();
  }

  handler() {
    return this.testHandler;
  }

  callbackRouter() {
    return this.callbackRouter_;
  }

  open(url: string) {
    this.methodCalled('open', url);
  }
}

suite('CrComponentsCustomizeThemesTest', () => {
  let testProxy: TestCustomizeThemesBrowserProxy;

  function createCustomizeThemesElement(): CustomizeThemesElement {
    const customizeThemesElement =
        document.createElement('cr-customize-themes');
    document.body.append(customizeThemesElement);
    return customizeThemesElement;
  }

  function createCustomizeThemesElementWithThemes(themes: ChromeTheme[]):
      CustomizeThemesElement {
    testProxy.testHandler.setChromeThemes(themes);
    return createCustomizeThemesElement();
  }

  setup(() => {
    document.body.innerHTML = '';
    testProxy = new TestCustomizeThemesBrowserProxy();
    CustomizeThemesBrowserProxyImpl.setInstance(testProxy);
  });

  test('creating element shows theme tiles', async () => {
    // Arrange.
    const themes: ChromeTheme[] = [
      {
        id: 0,
        label: 'theme_0',
        colors: {
          frame: {value: 0xff000000},          // white.
          activeTab: {value: 0xff0000ff},      // blue.
          activeTabText: {value: 0xff000000},  // white.
        },
      },
      {
        id: 1,
        label: 'theme_1',
        colors: {
          frame: {value: 0xffff0000},          // red.
          activeTab: {value: 0xff00ff00},      // green.
          activeTabText: {value: 0xff000000},  // white.
        },
      },
    ];

    // Act.
    const customizeThemesElement =
        createCustomizeThemesElementWithThemes(themes);
    assertEquals(1, testProxy.testHandler.getCallCount('initializeTheme'));
    assertEquals(1, testProxy.testHandler.getCallCount('getChromeThemes'));
    await flushTasks();

    // Assert.
    const tilesWrapper = customizeThemesElement.shadowRoot!.querySelectorAll(
        'div.chrome-theme-wrapper');
    assertEquals(tilesWrapper.length, themes.length);
    tilesWrapper.forEach(function(tileWrapper, i) {
      assertEquals(tileWrapper.getAttribute('aria-label'), themes[i]!.label);
    });

    const tiles =
        customizeThemesElement.shadowRoot!.querySelectorAll('cr-theme-icon');
    assertEquals(tiles.length, 4);
    assertStyle(tiles[2]!, '--cr-theme-icon-frame-color', 'rgba(0, 0, 0, 1)');
    assertStyle(
        tiles[2]!, '--cr-theme-icon-active-tab-color', 'rgba(0, 0, 255, 1)');
    assertStyle(tiles[3]!, '--cr-theme-icon-frame-color', 'rgba(255, 0, 0, 1)');
    assertStyle(
        tiles[3]!, '--cr-theme-icon-active-tab-color', 'rgba(0, 255, 0, 1)');
  });

  [true, false].forEach(autoConfirm => {
    test(
        `clicking default theme with autoConfirm="${
            autoConfirm}" applies default theme`,
        async () => {
          // Arrange.
          const customizeThemesElement = createCustomizeThemesElement();
          customizeThemesElement.autoConfirmThemeChanges = autoConfirm;
          await flushTasks();

          // Act.
          customizeThemesElement.$.defaultTheme.click();

          // Assert.
          assertEquals(
              1, testProxy.testHandler.getCallCount('applyDefaultTheme'));
          assertEquals(
              autoConfirm ? 1 : 0,
              testProxy.testHandler.getCallCount('confirmThemeChanges'));
        });

    test(
        `selecting color with autoConfirm="${
            autoConfirm}" applies autogenerated theme`,
        async () => {
          // Arrange.
          const customizeThemesElement = createCustomizeThemesElement();
          customizeThemesElement.autoConfirmThemeChanges = autoConfirm;
          const applyAutogeneratedThemeCalled =
              testProxy.testHandler.whenCalled('applyAutogeneratedTheme');

          // Act.
          customizeThemesElement.$.colorPicker.value = '#ff0000';
          customizeThemesElement.$.colorPicker.dispatchEvent(
              new Event('change'));

          // Assert.
          const {value} = await applyAutogeneratedThemeCalled;
          assertEquals(value, 0xffff0000);
          assertEquals(
              autoConfirm ? 1 : 0,
              testProxy.testHandler.getCallCount('confirmThemeChanges'));
        });
  });

  function assertSingleThemeIconIsFocusableWithTabKey(
      customizeThemesElement: CustomizeThemesElement) {
    const numberOfthemeIcons =
        customizeThemesElement.shadowRoot!.querySelectorAll('div cr-theme-icon')
            .length;
    const numberOfNoneFocusableThemeIcons =
        customizeThemesElement.shadowRoot!
            .querySelectorAll('div[tabindex="-1"] cr-theme-icon')
            .length;
    assertEquals(numberOfNoneFocusableThemeIcons, numberOfthemeIcons - 1);
    assertEquals(
        customizeThemesElement.shadowRoot!
            .querySelectorAll('div[tabindex="0"] cr-theme-icon')
            .length,
        1);
  }

  test('No theme selected', () => {
    const customizeThemesElement = createCustomizeThemesElement();
    // First item of the grid has tabindex 0.
    const focusableIcons = customizeThemesElement.shadowRoot!.querySelectorAll(
        'div[tabindex="0"] cr-theme-icon');
    assertEquals(focusableIcons.length, 1);
    assertEquals(
        focusableIcons[0],
        customizeThemesElement.shadowRoot!.querySelector(
            'div[id="autogeneratedThemeContainer"] cr-theme-icon'));
    assertSingleThemeIconIsFocusableWithTabKey(customizeThemesElement);
  });

  test('setting autogenerated theme selects and updates icon', async () => {
    // Arrange.
    const customizeThemesElement = createCustomizeThemesElement();

    // Act.
    customizeThemesElement.selectedTheme = {
      type: ThemeType.kAutogenerated,
      info: {
        autogeneratedThemeColors: {
          frame: {value: 0xffff0000},
          activeTab: {value: 0xff0000ff},
          activeTabText: {value: 0xff00ff00},
        },
        chromeThemeId: undefined,
        thirdPartyThemeInfo: undefined,
      },
      isForced: false,
    };
    await flushTasks();

    // Assert.
    const selectedIcons = customizeThemesElement.shadowRoot!.querySelectorAll(
        'cr-theme-icon[selected]');
    assertEquals(selectedIcons.length, 1);
    assertEquals(
        selectedIcons[0],
        customizeThemesElement.shadowRoot!.querySelector(
            '#autogeneratedTheme'));
    assertStyle(
        selectedIcons[0]!, '--cr-theme-icon-frame-color', 'rgba(255, 0, 0, 1)');
    assertStyle(
        selectedIcons[0]!, '--cr-theme-icon-active-tab-color',
        'rgba(0, 0, 255, 1)');
    assertDeepEquals(
        selectedIcons,
        customizeThemesElement.shadowRoot!.querySelectorAll(
            'div[aria-checked="true"] cr-theme-icon'));
    assertDeepEquals(
        selectedIcons,
        customizeThemesElement.shadowRoot!.querySelectorAll(
            'div[tabindex="0"] cr-theme-icon'));
    assertSingleThemeIconIsFocusableWithTabKey(customizeThemesElement);
  });

  test('setting default theme selects and updates icon', async () => {
    // Arrange.
    const customizeThemesElement = createCustomizeThemesElement();

    // Act.
    customizeThemesElement.selectedTheme = {
      type: ThemeType.kDefault,
      info: {
        chromeThemeId: 0,
        autogeneratedThemeColors: undefined,
        thirdPartyThemeInfo: undefined,
      },
      isForced: false,
    };
    await flushTasks();

    // Assert.
    const selectedIcons = customizeThemesElement.shadowRoot!.querySelectorAll(
        'cr-theme-icon[selected]');
    assertEquals(selectedIcons.length, 1);
    assertEquals(selectedIcons[0], customizeThemesElement.$.defaultTheme);
    assertDeepEquals(
        selectedIcons,
        customizeThemesElement.shadowRoot!.querySelectorAll(
            'div[aria-checked="true"] cr-theme-icon'));
    assertDeepEquals(
        selectedIcons,
        customizeThemesElement.shadowRoot!.querySelectorAll(
            'div[tabindex="0"] cr-theme-icon'));
    assertSingleThemeIconIsFocusableWithTabKey(customizeThemesElement);
  });

  test('setting Chrome theme selects and updates icon', async () => {
    // Arrange.
    const themes = [
      {
        id: 0,
        label: 'foo',
        colors: {
          frame: {value: 0xff000000},
          activeTab: {value: 0xff0000ff},
          activeTabText: {value: 0xff00ff00},
        },
      },
    ];
    const customizeThemesElement =
        createCustomizeThemesElementWithThemes(themes);

    // Act.
    customizeThemesElement.selectedTheme = {
      type: ThemeType.kChrome,
      info: {
        chromeThemeId: 0,
        autogeneratedThemeColors: undefined,
        thirdPartyThemeInfo: undefined,
      },
      isForced: false,
    };
    await flushTasks();

    // Assert.
    const selectedIcons = customizeThemesElement.shadowRoot!.querySelectorAll(
        'cr-theme-icon[selected]');
    assertEquals(selectedIcons.length, 1);
    const selectedIconWrapper =
        customizeThemesElement.shadowRoot!.querySelector(
            'div.chrome-theme-wrapper');
    assertTrue(!!selectedIconWrapper);
    assertTrue(!!selectedIconWrapper.querySelector('cr-theme-icon[selected]'));
    assertEquals(selectedIconWrapper.getAttribute('aria-label'), 'foo');
    assertDeepEquals(
        selectedIcons,
        customizeThemesElement.shadowRoot!.querySelectorAll(
            'div[aria-checked="true"] cr-theme-icon'));
    assertDeepEquals(
        selectedIcons,
        customizeThemesElement.shadowRoot!.querySelectorAll(
            'div[tabindex="0"] cr-theme-icon'));
    assertSingleThemeIconIsFocusableWithTabKey(customizeThemesElement);
  });

  test('setting policy theme', async () => {
    // Arrange.
    const customizeThemesElement = createCustomizeThemesElement();

    // Act.
    customizeThemesElement.selectedTheme = {
      type: ThemeType.kAutogenerated,
      info: {
        autogeneratedThemeColors: {
          frame: {value: 0xff0000ff},
          activeTab: {value: 0xff00ff00},
          activeTabText: {value: 0xffff0000},
        },
        chromeThemeId: undefined,
        thirdPartyThemeInfo: undefined,
      },
      isForced: true,
    };
    await flushTasks();

    // Assert.
    // Theme icon is selected and updated, and color picker icon is hidden.
    const selectedIcons = customizeThemesElement.shadowRoot!.querySelectorAll(
        'cr-theme-icon[selected]');
    assertEquals(selectedIcons.length, 1);
    assertEquals(
        selectedIcons[0],
        customizeThemesElement.shadowRoot!.querySelector(
            '#autogeneratedTheme'));
    assertStyle(
        selectedIcons[0]!, '--cr-theme-icon-frame-color', 'rgba(0, 0, 255, 1)');
    assertStyle(
        selectedIcons[0]!, '--cr-theme-icon-active-tab-color',
        'rgba(0, 255, 0, 1)');
    assertDeepEquals(
        selectedIcons,
        customizeThemesElement.shadowRoot!.querySelectorAll(
            'div[aria-checked="true"] cr-theme-icon'));
    assertDeepEquals(
        selectedIcons,
        customizeThemesElement.shadowRoot!.querySelectorAll(
            'div[tabindex="0"] cr-theme-icon'));
    assertSingleThemeIconIsFocusableWithTabKey(customizeThemesElement);
    assertTrue(customizeThemesElement.$.colorPickerIcon.hidden);

    // Managed dialog appears on theme click when policy theme is applied.
    assertEquals(
        null,
        customizeThemesElement.shadowRoot!.querySelector('managed-dialog'));
    customizeThemesElement.$.defaultTheme.click();
    await flushTasks();  // Allow managed-dialog to be added to DOM.
    assertFalse(
        customizeThemesElement.shadowRoot!.querySelector(
                                              'managed-dialog')!.hidden);
  });

  test('setting third-party theme shows uninstall UI', async () => {
    // Arrange.
    const customizeThemesElement = createCustomizeThemesElement();

    // Act.
    customizeThemesElement.selectedTheme = {
      type: ThemeType.kThirdParty,
      info: {
        thirdPartyThemeInfo: {
          id: 'foo',
          name: 'bar',
        },
        chromeThemeId: undefined,
        autogeneratedThemeColors: undefined,
      },
      isForced: false,
    };

    // Assert.
    assertStyle(
        customizeThemesElement.shadowRoot!.querySelector(
            '#thirdPartyThemeContainer')!,
        'display', 'block');
    assertEquals(
        customizeThemesElement.shadowRoot!
            .querySelector('#thirdPartyThemeName')!.textContent!.trim(),
        'bar');

    // First item of the grid has tabindex 0.
    const focusableIcons = customizeThemesElement.shadowRoot!.querySelectorAll(
        'div[tabindex="0"] cr-theme-icon');
    assertEquals(focusableIcons.length, 1);
    assertEquals(
        focusableIcons[0],
        customizeThemesElement.shadowRoot!.querySelector(
            'div[id="autogeneratedThemeContainer"] cr-theme-icon'));
    assertSingleThemeIconIsFocusableWithTabKey(customizeThemesElement);
  });

  test('clicking third-party link opens theme page', async () => {
    // Arrange.
    const customizeThemesElement = createCustomizeThemesElement();
    customizeThemesElement.selectedTheme = {
      type: ThemeType.kThirdParty,
      info: {
        thirdPartyThemeInfo: {
          id: 'foo',
          name: 'bar',
        },
        chromeThemeId: undefined,
        autogeneratedThemeColors: undefined,
      },
      isForced: false,
    };

    // Act.
    customizeThemesElement.shadowRoot!
        .querySelector<HTMLElement>('#thirdPartyLink')!.click();

    // Assert.
    const link = await testProxy.whenCalled('open');
    assertEquals('https://chrome.google.com/webstore/detail/foo', link);
  });

  test('setting non-third-party theme hides uninstall UI', async () => {
    // Arrange.
    const customizeThemesElement = createCustomizeThemesElement();

    // Act.
    customizeThemesElement.selectedTheme = {
      type: ThemeType.kDefault,
      info: {
        chromeThemeId: 0,
        thirdPartyThemeInfo: undefined,
        autogeneratedThemeColors: undefined,
      },
      isForced: false,
    };

    // Assert.
    assertStyle(
        customizeThemesElement.shadowRoot!.querySelector(
            '#thirdPartyThemeContainer')!,
        'display', 'none');
  });

  test('uninstalling third-party theme sets default theme', async () => {
    // Arrange.
    const customizeThemesElement = createCustomizeThemesElement();
    customizeThemesElement.selectedTheme = {
      type: ThemeType.kThirdParty,
      info: {
        thirdPartyThemeInfo: {
          id: 'foo',
          name: 'bar',
        },
        chromeThemeId: undefined,
        autogeneratedThemeColors: undefined,
      },
      isForced: false,
    };
    // Act.
    customizeThemesElement.shadowRoot!
        .querySelector<HTMLElement>('#uninstallThirdPartyButton')!.click();

    // Assert.
    assertEquals(1, testProxy.testHandler.getCallCount('applyDefaultTheme'));
    assertEquals(1, testProxy.testHandler.getCallCount('confirmThemeChanges'));
  });
});

suite('ThemeIconTest', () => {
  let themeIcon: ThemeIconElement;

  function queryAll(selector: string) {
    return themeIcon.shadowRoot!.querySelectorAll(selector);
  }

  function query(selector: string) {
    return themeIcon.shadowRoot!.querySelector(selector);
  }

  setup(() => {
    document.body.innerHTML = '';

    themeIcon = document.createElement('cr-theme-icon');
    document.body.appendChild(themeIcon);
  });

  test('setting frame color sets stroke and gradient', () => {
    // Act.
    themeIcon.style.setProperty('--cr-theme-icon-frame-color', 'red');

    // Assert.
    assertStyle(query('#circle')!, 'stroke', 'rgb(255, 0, 0)');
    assertStyle(
        queryAll('#gradient > stop')[1]!, 'stop-color', 'rgb(255, 0, 0)');
  });

  test('setting active tab color sets gradient', async () => {
    // Act.
    themeIcon.style.setProperty('--cr-theme-icon-active-tab-color', 'red');

    // Assert.
    assertStyle(
        queryAll('#gradient > stop')[0]!, 'stop-color', 'rgb(255, 0, 0)');
  });

  test('setting explicit stroke color sets different stroke', async () => {
    // Act.
    themeIcon.style.setProperty('--cr-theme-icon-frame-color', 'red');
    themeIcon.style.setProperty('--cr-theme-icon-stroke-color', 'blue');

    // Assert.
    assertStyle(query('#circle')!, 'stroke', 'rgb(0, 0, 255)');
    assertStyle(
        queryAll('#gradient > stop')[1]!, 'stop-color', 'rgb(255, 0, 0)');
  });

  test('selecting icon shows ring and check mark', async () => {
    // Act.
    themeIcon.toggleAttribute('selected', true);

    // Assert.
    assertStyle(query('#ring')!, 'visibility', 'visible');
    assertStyle(query('#checkMark')!, 'visibility', 'visible');
  });
});