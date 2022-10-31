// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkType, RoutineType} from 'chrome://diagnostics/diagnostics_types.js';
import {convertKibToGibDecimalString, getNetworkCardTitle, getRoutineGroups, getSignalStrength, getSubnetMaskFromRoutingPrefix, setDisplayStateInTitleForTesting} from 'chrome://diagnostics/diagnostics_utils.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

export function diagnosticsUtilsTestSuite() {
  test('ProperlyConvertsKibToGib', () => {
    assertEquals('0', convertKibToGibDecimalString(0, 0));
    assertEquals('0.00', convertKibToGibDecimalString(0, 2));
    assertEquals('0.000000', convertKibToGibDecimalString(0, 6));
    assertEquals('0', convertKibToGibDecimalString(1, 0));
    assertEquals('5.72', convertKibToGibDecimalString(6000000, 2));
    assertEquals('5.722046', convertKibToGibDecimalString(6000000, 6));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20, 2));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20 + 1, 2));
    assertEquals('1.00', convertKibToGibDecimalString(2 ** 20 - 1, 2));
    assertEquals('0.999999', convertKibToGibDecimalString(2 ** 20 - 1, 6));
  });

  test('ConvertRoutingPrefixToSubnetMask', () => {
    // '0' indicates an unset value.
    assertEquals(getSubnetMaskFromRoutingPrefix(0), '');
    assertEquals(getSubnetMaskFromRoutingPrefix(1), '128.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(2), '192.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(3), '224.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(4), '240.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(5), '248.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(6), '252.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(7), '254.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(8), '255.0.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(9), '255.128.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(10), '255.192.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(11), '255.224.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(12), '255.240.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(13), '255.248.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(14), '255.252.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(15), '255.254.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(16), '255.255.0.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(17), '255.255.128.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(18), '255.255.192.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(19), '255.255.224.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(20), '255.255.240.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(21), '255.255.248.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(22), '255.255.252.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(23), '255.255.254.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(24), '255.255.255.0');
    assertEquals(getSubnetMaskFromRoutingPrefix(25), '255.255.255.128');
    assertEquals(getSubnetMaskFromRoutingPrefix(26), '255.255.255.192');
    assertEquals(getSubnetMaskFromRoutingPrefix(27), '255.255.255.224');
    assertEquals(getSubnetMaskFromRoutingPrefix(28), '255.255.255.240');
    assertEquals(getSubnetMaskFromRoutingPrefix(29), '255.255.255.248');
    assertEquals(getSubnetMaskFromRoutingPrefix(30), '255.255.255.252');
    assertEquals(getSubnetMaskFromRoutingPrefix(31), '255.255.255.254');
    assertEquals(getSubnetMaskFromRoutingPrefix(32), '255.255.255.255');
  });

  test('AllRoutineGroupsPresent', () => {
    loadTimeData.overrideValues({enableArcNetworkDiagnostics: true});
    const isArcEnabled = loadTimeData.getBoolean('enableArcNetworkDiagnostics');
    const routineGroups = getRoutineGroups(NetworkType.kWiFi, isArcEnabled);
    const [
      localNetworkGroup,
       nameResolutionGroup,
       wifiGroup,
       internetConnectivityGroup,
      ]
      = routineGroups;

    // All groups should be present.
    assertEquals(routineGroups.length, 4);

    // WiFi group should exist and all three WiFi routines should be present.
    assertEquals(wifiGroup.routines.length, 3);
    assertEquals(wifiGroup.groupName, 'wifiGroupLabel');

    // ARC routines should be present in their categories.
    assertTrue(
        nameResolutionGroup.routines.includes(RoutineType.kArcDnsResolution));

    assertTrue(localNetworkGroup.routines.includes(RoutineType.kArcPing));
    assertTrue(
        internetConnectivityGroup.routines.includes(RoutineType.kArcHttp));
  });

  test('NetworkTypeIsNotWiFi', () => {
    const isArcEnabled = loadTimeData.getBoolean('enableArcNetworkDiagnostics');
    const routineGroups = getRoutineGroups(NetworkType.kEthernet, isArcEnabled);
    // WiFi group should be missing.
    assertEquals(routineGroups.length, 3);
    const groupNames = routineGroups.map(group => group.groupName);
    assertFalse(groupNames.includes('wifiGroupLabel'));
  });

  test('ArcRoutinesDisabled', () => {
    loadTimeData.overrideValues({enableArcNetworkDiagnostics: false});
    const isArcEnabled = loadTimeData.getBoolean('enableArcNetworkDiagnostics');
    const routineGroups = getRoutineGroups(NetworkType.kEthernet, isArcEnabled);
    const [localNetworkGroup, nameResolutionGroup, internetConnectivityGroup] =
        routineGroups;
    assertFalse(
        nameResolutionGroup.routines.includes(RoutineType.kArcDnsResolution));

    assertFalse(localNetworkGroup.routines.includes(RoutineType.kArcPing));
    assertFalse(
        internetConnectivityGroup.routines.includes(RoutineType.kArcHttp));
  });

  test('GetNetworkCardTitle', () => {
    // Force connection state into title by setting displayStateInTitle to true.
    setDisplayStateInTitleForTesting(true);
    assertEquals(
        'Ethernet (Online)', getNetworkCardTitle('Ethernet', 'Online'));

    // Default state is to not display connection details in title.
    setDisplayStateInTitleForTesting(false);
    assertEquals('Ethernet', getNetworkCardTitle('Ethernet', 'Online'));
  });

  test('GetSignalStrength', () => {
    assertEquals(getSignalStrength(0), '');
    assertEquals(getSignalStrength(1), '');
    assertEquals(getSignalStrength(14), 'Weak (14)');
    assertEquals(getSignalStrength(33), 'Average (33)');
    assertEquals(getSignalStrength(63), 'Good (63)');
    assertEquals(getSignalStrength(98), 'Excellent (98)');
  });
}
