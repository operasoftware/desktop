// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of Personalization SearchHandler for
 * testing.
 */

/**
 * @implements {ash.personalizationApp.mojom.SearchHandlerInterface}
 */
export class FakePersonalizationSearchHandler {
  constructor() {
    /** @private {!Array<ash.personalizationApp.mojom.SearchResult>} */
    this.fakeResults_ = [];

    /**
     * @private {!ash.personalizationApp.mojom.SearchResultsObserverInterface}
     */
    this.observer_;
  }

  /**
   * @param {!Array<ash.personalizationApp.mojom.SearchResult>} results Fake
   *     results that will be returned when Search() is called.
   */
  setFakeResults(results) {
    this.fakeResults_ = results;
  }

  /** override */
  async search(query, maxNumResults) {
    return {results: this.fakeResults_};
  }

  /** override */
  addObserver(observer) {
    this.observer_ = observer;
  }

  simulateSearchResultsChanged() {
    if (this.observer_) {
      this.observer_.onSearchResultsChanged();
    }
  }
}
