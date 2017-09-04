/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** Invokes PaymentRequest with a very long request identifier. */
function buy() { // eslint-disable-line no-unused-vars
  const foo = Object.freeze({
    supportedMethods: ['basic-card']
  });
  const defaultMethods = Object.freeze([foo]);
  const defaultDetails = Object.freeze({
    total: {
      label: 'Label',
      amount: {
        currency: 'USD',
        value: '1.00',
      },
    },
  });
  const newDetails = Object.assign({}, defaultDetails, {
    id: ''.padStart(100000000, '\n very long id \t \n '),
  });
  try {
    const request = new PaymentRequest(defaultMethods, newDetails);
  } catch (error) {
    print(error);
  }
}
