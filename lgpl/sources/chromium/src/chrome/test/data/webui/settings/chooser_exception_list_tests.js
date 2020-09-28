// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';
import 'chrome://test/cr_elements/cr_policy_strings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ChooserType,ContentSettingsTypes,SiteSettingSource,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair,createRawChooserException,createRawSiteException,createSiteSettingsPrefs} from 'chrome://test/settings/test_util.js';
// clang-format on

/** @fileoverview Suite of tests for chooser-exception-list. */

/**
 * An example pref that does not contain any entries.
 * @type {SiteSettingsPref}
 */
let prefsEmpty;

/**
 * An example pref with only user granted USB exception.
 * @type {SiteSettingsPref}
 */
let prefsUserProvider;

/**
 * An example pref with only policy granted USB exception.
 * @type {SiteSettingsPref}
 */
let prefsPolicyProvider;

/**
 * An example pref with 3 USB exception items. The first item will have a user
 * granted site exception and a policy granted site exception. The second item
 * will only have a policy granted site exception. The last item will only have
 * a user granted site exception.
 * @type {SiteSettingsPref}
 */
let prefsUsb;

/**
 * Creates all the test
 */
function populateTestExceptions() {
  prefsEmpty = createSiteSettingsPrefs(
      [] /* defaultsList */, [] /* exceptionsList */,
      [] /* chooserExceptionsList */);

  prefsUserProvider = createSiteSettingsPrefs(
      [] /* defaultsList */, [] /* exceptionsList */,
      [createContentSettingTypeToValuePair(ContentSettingsTypes.USB_DEVICES, [
        createRawChooserException(
            ChooserType.USB_DEVICES,
            [createRawSiteException('https://foo.com')])
      ])] /* chooserExceptionsList */);

  prefsPolicyProvider = createSiteSettingsPrefs(
      [] /* defaultsList */, [] /* exceptionsList */,
      [createContentSettingTypeToValuePair(ContentSettingsTypes.USB_DEVICES, [
        createRawChooserException(
            ChooserType.USB_DEVICES,
            [createRawSiteException(
                'https://foo.com', {source: SiteSettingSource.POLICY})])
      ])] /* chooserExceptionsList */);

  prefsUsb =
      createSiteSettingsPrefs([] /* defaultsList */, [] /* exceptionsList */, [
        createContentSettingTypeToValuePair(
            ContentSettingsTypes.USB_DEVICES,
            [
              createRawChooserException(
                  ChooserType.USB_DEVICES,
                  [
                    createRawSiteException(
                        'https://foo-policy.com',
                        {source: SiteSettingSource.POLICY}),
                    createRawSiteException('https://foo-user.com'),
                  ],
                  {
                    displayName: 'Gadget',
                  }),
              createRawChooserException(
                  ChooserType.USB_DEVICES,
                  [createRawSiteException('https://bar-policy.com', {
                    source: SiteSettingSource.POLICY,
                  })],
                  {
                    displayName: 'Gizmo',
                  }),
              createRawChooserException(
                  ChooserType.USB_DEVICES,
                  [createRawSiteException('https://baz-user.com')],
                  {displayName: 'Widget'})
            ]),
      ] /* chooserExceptionsList */);
}

suite('ChooserExceptionList', function() {
  /**
   * A site list element created before each test.
   * @type {ChooserExceptionList}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  // Initialize a chooser-exception-list before each test.
  setup(function() {
    populateTestExceptions();

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('chooser-exception-list');
    document.body.appendChild(testElement);
  });

  /**
   * Configures the test element for a particular category.
   * @param {ChooserType} chooserType The chooser type to set up the
   *     element for.
   * @param {Array<dictionary>} prefs The prefs to use.
   */
  function setUpChooserType(contentType, chooserType, prefs) {
    browserProxy.setPrefs(prefs);
    testElement.category = contentType;
    testElement.chooserType = chooserType;
  }

  function assertSiteOriginsEquals(site, actualSite) {
    assertEquals(site.origin, actualSite.origin);
    assertEquals(site.embeddingOrigin, actualSite.embeddingOrigin);
  }

  function assertChooserExceptionEquals(exception, actualException) {
    assertEquals(exception.displayName, actualException.displayName);
    assertEquals(exception.chooserType, actualException.chooserType);
    assertDeepEquals(exception.object, actualException.object);

    const sites = exception.sites;
    const actualSites = actualException.sites;
    assertEquals(sites.length, actualSites.length);
    for (let i = 0; i < sites.length; ++i) {
      assertSiteOriginsEquals(sites[i], actualSites[i]);
    }
  }

  test('getChooserExceptionList API used', function() {
    setUpChooserType(
        ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES, prefsUsb);
    assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
    assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
    return browserProxy.whenCalled('getChooserExceptionList')
        .then(function(chooserType) {
          assertEquals(ChooserType.USB_DEVICES, chooserType);

          // Flush the container to ensure that the container is populated.
          flush();

          // Ensure that each chooser exception is rendered with a
          // chooser-exception-list-entry.
          const chooserExceptionListEntries =
              testElement.root.querySelectorAll('chooser-exception-list-entry');
          assertEquals(3, chooserExceptionListEntries.length);
          for (let i = 0; i < chooserExceptionListEntries.length; ++i) {
            assertChooserExceptionEquals(
                prefsUsb.chooserExceptions[ContentSettingsTypes.USB_DEVICES][i],
                chooserExceptionListEntries[i].exception);
          }

          // The first chooser exception should render two site exceptions with
          // site-list-entry elements.
          const firstSiteListEntries =
              chooserExceptionListEntries[0].root.querySelectorAll(
                  'site-list-entry');
          assertEquals(2, firstSiteListEntries.length);

          // The second chooser exception should render one site exception with
          // a site-list-entry element.
          const secondSiteListEntries =
              chooserExceptionListEntries[1].root.querySelectorAll(
                  'site-list-entry');
          assertEquals(1, secondSiteListEntries.length);

          // The last chooser exception should render one site exception with a
          // site-list-entry element.
          const thirdSiteListEntries =
              chooserExceptionListEntries[2].root.querySelectorAll(
                  'site-list-entry');
          assertEquals(1, secondSiteListEntries.length);
        });
  });

  test(
      'User granted chooser exceptions should show the reset button',
      function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsUserProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        return browserProxy.whenCalled('getChooserExceptionList')
            .then(function(chooserType) {
              // Flush the container to ensure that the container is populated.
              flush();

              const chooserExceptionListEntry =
                  testElement.$$('chooser-exception-list-entry');
              assertTrue(!!chooserExceptionListEntry);

              const siteListEntry =
                  chooserExceptionListEntry.$$('site-list-entry');
              assertTrue(!!siteListEntry);

              // Ensure that the action menu container is hidden.
              const dotsMenu = siteListEntry.$$('#actionMenuButton');
              assertTrue(!!dotsMenu);
              assertTrue(dotsMenu.hidden);

              // Ensure that the reset button is not hidden.
              const resetButton = siteListEntry.$$('#resetSite');
              assertTrue(!!resetButton);
              assertFalse(resetButton.hidden);

              // Ensure that the policy enforced indicator is hidden.
              const policyIndicator =
                  siteListEntry.$$('cr-policy-pref-indicator');
              assertFalse(!!policyIndicator);
            });
      });

  test(
      'Policy granted chooser exceptions should show the policy indicator icon',
      function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsPolicyProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        return browserProxy.whenCalled('getChooserExceptionList')
            .then(function(chooserType) {
              // Flush the container to ensure that the container is populated.
              flush();

              const chooserExceptionListEntry =
                  testElement.$$('chooser-exception-list-entry');
              assertTrue(!!chooserExceptionListEntry);

              const siteListEntry =
                  chooserExceptionListEntry.$$('site-list-entry');
              assertTrue(!!siteListEntry);

              // Ensure that the action menu container is hidden.
              const dotsMenu = siteListEntry.$$('#actionMenuButton');
              assertTrue(!!dotsMenu);
              assertTrue(dotsMenu.hidden);

              // Ensure that the reset button is hidden.
              const resetButton = siteListEntry.$$('#resetSite');
              assertTrue(!!resetButton);
              assertTrue(resetButton.hidden);

              // Ensure that the policy enforced indicator not is hidden.
              const policyIndicator =
                  siteListEntry.$$('cr-policy-pref-indicator');
              assertTrue(!!policyIndicator);
            });
      });

  test(
      'Site exceptions from mixed sources should display properly', function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsUsb);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        return browserProxy.whenCalled('getChooserExceptionList')
            .then(function(chooserType) {
              // Flush the container to ensure that the container is populated.
              flush();

              const chooserExceptionListEntries =
                  testElement.root.querySelectorAll(
                      'chooser-exception-list-entry');
              assertEquals(3, chooserExceptionListEntries.length);

              // The first chooser exception contains mixed provider site
              // exceptions.
              const siteListEntries =
                  chooserExceptionListEntries[0].root.querySelectorAll(
                      'site-list-entry');
              assertEquals(2, siteListEntries.length);

              // The first site exception is a policy provided exception, so
              // only the policy indicator should be visible;
              const policyProvidedDotsMenu =
                  siteListEntries[0].$$('#actionMenuButton');
              assertTrue(!!policyProvidedDotsMenu);
              assertTrue(policyProvidedDotsMenu.hidden);

              const policyProvidedResetButton =
                  siteListEntries[0].$$('#resetSite');
              assertTrue(!!policyProvidedResetButton);
              assertTrue(policyProvidedResetButton.hidden);

              const policyProvidedPolicyIndicator =
                  siteListEntries[0].$$('cr-policy-pref-indicator');
              assertTrue(!!policyProvidedPolicyIndicator);

              // The second site exception is a user provided exception, so only
              // the reset button should be visible.
              const userProvidedDotsMenu =
                  siteListEntries[1].$$('#actionMenuButton');
              assertTrue(!!userProvidedDotsMenu);
              assertTrue(userProvidedDotsMenu.hidden);

              const userProvidedResetButton =
                  siteListEntries[1].$$('#resetSite');
              assertTrue(!!userProvidedResetButton);
              assertFalse(userProvidedResetButton.hidden);

              const userProvidedPolicyIndicator =
                  siteListEntries[1].$$('cr-policy-pref-indicator');
              assertFalse(!!userProvidedPolicyIndicator);
            });
      });

  test('Empty list', function() {
    setUpChooserType(
        ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES, prefsEmpty);
    assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
    assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
    return browserProxy.whenCalled('getChooserExceptionList')
        .then(function(chooserType) {
          assertEquals(ChooserType.USB_DEVICES, chooserType);
          assertEquals(0, testElement.chooserExceptions.length);
          const emptyListMessage = testElement.$$('#empty-list-message');
          assertFalse(emptyListMessage.hidden);
          assertEquals(
              'No USB devices found', emptyListMessage.textContent.trim());
        });
  });

  test('resetChooserExceptionForSite API used', function() {
    setUpChooserType(
        ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
        prefsUserProvider);
    assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
    assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
    return browserProxy.whenCalled('getChooserExceptionList')
        .then(function(chooserType) {
          assertEquals(ChooserType.USB_DEVICES, chooserType);
          assertEquals(1, testElement.chooserExceptions.length);

          assertChooserExceptionEquals(
              prefsUserProvider
                  .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0],
              testElement.chooserExceptions[0]);

          // Flush the container to ensure that the container is populated.
          flush();

          const chooserExceptionListEntry =
              testElement.$$('chooser-exception-list-entry');
          assertTrue(!!chooserExceptionListEntry);

          const siteListEntry = chooserExceptionListEntry.$$('site-list-entry');
          assertTrue(!!siteListEntry);

          // Assert that the action button is hidden.
          const dotsMenu = siteListEntry.$$('#actionMenuButton');
          assertTrue(!!dotsMenu);
          assertTrue(dotsMenu.hidden);

          // Assert that the reset button is visible.
          const resetButton = siteListEntry.$$('#resetSite');
          assertTrue(!!resetButton);
          assertFalse(resetButton.hidden);

          resetButton.click();
          return browserProxy.whenCalled('resetChooserExceptionForSite');
        })
        .then(function(args) {
          assertEquals(ChooserType.USB_DEVICES, args[0]);
          assertEquals('https://foo.com', args[1]);
          assertEquals('https://foo.com', args[2]);
          assertDeepEquals({}, args[3]);
        });
  });

  test(
      'The show-tooltip event is fired when mouse hovers over policy ' +
          'indicator and the common tooltip is shown',
      function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsPolicyProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        return browserProxy.whenCalled('getChooserExceptionList')
            .then(function(chooserType) {
              assertEquals(ChooserType.USB_DEVICES, chooserType);
              assertEquals(1, testElement.chooserExceptions.length);

              assertChooserExceptionEquals(
                  prefsPolicyProvider
                      .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0],
                  testElement.chooserExceptions[0]);

              // Flush the container to ensure that the container is populated.
              flush();

              const chooserExceptionListEntry =
                  testElement.$$('chooser-exception-list-entry');
              assertTrue(!!chooserExceptionListEntry);

              const siteListEntry =
                  chooserExceptionListEntry.$$('site-list-entry');
              assertTrue(!!siteListEntry);

              const tooltip = testElement.$.tooltip;
              assertTrue(!!tooltip);

              const innerTooltip = tooltip.$.tooltip;
              assertTrue(!!innerTooltip);

              /**
               * Create an array of test parameters objects. The parameter
               * properties are the following:
               * |text| Tooltip text to display.
               * |el| Event target element.
               * |eventType| The event type to dispatch to |el|.
               * @type {Array<{text: string, el: !Element, eventType: string}>}
               */
              const testsParams = [
                {text: 'a', el: testElement, eventType: 'mouseleave'},
                {text: 'b', el: testElement, eventType: 'tap'},
                {text: 'c', el: testElement, eventType: 'blur'},
                {text: 'd', el: tooltip, eventType: 'mouseenter'},
              ];
              testsParams.forEach(params => {
                const text = params.text;
                const eventTarget = params.el;

                siteListEntry.fire('show-tooltip', {target: testElement, text});
                assertFalse(innerTooltip.classList.contains('hidden'));
                assertEquals(text, tooltip.innerHTML.trim());

                eventTarget.dispatchEvent(new MouseEvent(params.eventType));
                assertTrue(innerTooltip.classList.contains('hidden'));
              });
            });
      });

  test('The exception list is updated when the prefs are modified', function() {
    setUpChooserType(
        ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
        prefsUserProvider);
    assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
    assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
    return browserProxy.whenCalled('getChooserExceptionList')
        .then(function(chooserType) {
          assertEquals(ChooserType.USB_DEVICES, chooserType);
          assertEquals(1, testElement.chooserExceptions.length);

          assertChooserExceptionEquals(
              prefsUserProvider
                  .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0],
              testElement.chooserExceptions[0]);

          browserProxy.resetResolver('getChooserExceptionList');

          // Simulate a change in preferences.
          setUpChooserType(
              ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
              prefsPolicyProvider);
          assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
          assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);

          webUIListenerCallback(
              'contentSettingChooserPermissionChanged',
              ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES);
          return browserProxy.whenCalled('getChooserExceptionList');
        })
        .then(function(chooserType) {
          assertEquals(ChooserType.USB_DEVICES, chooserType);
          assertEquals(1, testElement.chooserExceptions.length);

          assertChooserExceptionEquals(
              prefsPolicyProvider
                  .chooserExceptions[ContentSettingsTypes.USB_DEVICES][0],
              testElement.chooserExceptions[0]);
        });
  });

  test(
      'The exception list is updated when incognito status is changed',
      function() {
        setUpChooserType(
            ContentSettingsTypes.USB_DEVICES, ChooserType.USB_DEVICES,
            prefsPolicyProvider);
        assertEquals(ContentSettingsTypes.USB_DEVICES, testElement.category);
        assertEquals(ChooserType.USB_DEVICES, testElement.chooserType);
        return browserProxy.whenCalled('getChooserExceptionList')
            .then(function(chooserType) {
              // Flush the container to ensure that the container is populated.
              flush();

              const chooserExceptionListEntry =
                  testElement.$$('chooser-exception-list-entry');
              assertTrue(!!chooserExceptionListEntry);

              const siteListEntry =
                  chooserExceptionListEntry.$$('site-list-entry');
              assertTrue(!!siteListEntry);
              // Ensure that the incognito tooltip is hidden.
              const incognitoTooltip = siteListEntry.$$('#incognitoTooltip');
              assertFalse(!!incognitoTooltip);

              // Simulate an incognito session being created.
              browserProxy.resetResolver('getChooserExceptionList');
              browserProxy.setIncognito(true);
              return browserProxy.whenCalled('getChooserExceptionList');
            })
            .then(function(chooserType) {
              // Flush the container to ensure that the container is populated.
              flush();

              const chooserExceptionListEntry =
                  testElement.$$('chooser-exception-list-entry');
              assertTrue(!!chooserExceptionListEntry);
              assertTrue(chooserExceptionListEntry.$.listContainer
                             .querySelector('iron-list')
                             .items.some(item => item.incognito));

              const siteListEntries =
                  chooserExceptionListEntry.root.querySelectorAll(
                      'site-list-entry');
              assertEquals(2, siteListEntries.length);
              assertTrue(Array.from(siteListEntries)
                             .some(entry => entry.model.incognito));

              const tooltip = testElement.$.tooltip;
              assertTrue(!!tooltip);
              const innerTooltip = tooltip.$.tooltip;
              assertTrue(!!innerTooltip);
              const text =
                  'This exception will be automatically removed after you ' +
                  'exit the current Incognito session';
              // This filtered array should be non-empty due to above test that
              // checks for incognito exception.
              Array.from(siteListEntries)
                  .filter(entry => entry.model.incognito)
                  .forEach(entry => {
                    const incognitoTooltip = entry.$$('#incognitoTooltip');
                    // Make sure it is not hidden if it is an incognito
                    // exception
                    assertTrue(!!incognitoTooltip);
                    // Trigger mouse enter and check tooltip text
                    incognitoTooltip.dispatchEvent(
                        new MouseEvent('mouseenter'));
                    assertFalse(innerTooltip.classList.contains('hidden'));
                    assertEquals(text, tooltip.innerHTML.trim());
                  });
            });
      });
});
