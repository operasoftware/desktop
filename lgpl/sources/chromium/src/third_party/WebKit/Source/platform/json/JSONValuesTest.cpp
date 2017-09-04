// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/json/JSONValues.h"

#include "platform/wtf/text/StringBuilder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class JSONValueDeletionVerifier : public JSONValue {
 public:
  JSONValueDeletionVerifier(int& counter) : counter_(counter) {}

  ~JSONValueDeletionVerifier() override { ++counter_; }

 private:
  int& counter_;
};

}  // namespace

TEST(JSONValuesTest, ArrayCastDoesNotLeak) {
  int deletion_count = 0;
  std::unique_ptr<JSONValueDeletionVerifier> not_an_array(
      new JSONValueDeletionVerifier(deletion_count));
  EXPECT_EQ(nullptr, JSONArray::From(std::move(not_an_array)));
  EXPECT_EQ(1, deletion_count);
}

TEST(JSONValuesTest, ObjectCastDoesNotLeak) {
  int deletion_count = 0;
  std::unique_ptr<JSONValueDeletionVerifier> not_an_object(
      new JSONValueDeletionVerifier(deletion_count));
  EXPECT_EQ(nullptr, JSONArray::From(std::move(not_an_object)));
  EXPECT_EQ(1, deletion_count);
}

}  // namespace blink
