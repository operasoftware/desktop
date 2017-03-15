// Copyright 2016 Opera Software AS. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/HeapCompact.h"

#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

static const size_t chunkSize = SparseHeapBitmap::s_maxRange;

TEST(HeapCompactTest, Basic)
{
    Address base = reinterpret_cast<Address>(0x1000u);
    OwnPtr<SparseHeapBitmap> bitmap = SparseHeapBitmap::create(base);

    // 101010... starting at |base|.
    for (unsigned i = 0; i < 2 * chunkSize; i += 2)
        bitmap->add(base + i);

    // Check that hasRange() returns a bitmap subtree, if any, for a given address.
    EXPECT_TRUE(!!bitmap->hasRange(base, 1));
    EXPECT_TRUE(!!bitmap->hasRange(base + 1, 1));
    EXPECT_FALSE(!!bitmap->hasRange(base - 1, 1));

    // Test implementation details.. that each SparseHeapBitmap node maps |s_maxRange|
    // ranges only.
    EXPECT_EQ(bitmap->hasRange(base + 1, 1), bitmap->hasRange(base + 2, 1));
    EXPECT_NE(bitmap->hasRange(base, 1), bitmap->hasRange(base + chunkSize, 1));

    SparseHeapBitmap* start = bitmap->hasRange(base + 2, 20);
    EXPECT_TRUE(!!start);
    for (unsigned i = 2; i < chunkSize * 2; i += 2) {
        EXPECT_TRUE(start->isSet(base + i));
        EXPECT_FALSE(start->isSet(base + i + 1));
    }
}

TEST(HeapCompactTest, BasicSparse)
{
    Address base = reinterpret_cast<Address>(0x1000u);
    OwnPtr<SparseHeapBitmap> bitmap = SparseHeapBitmap::create(base);

    size_t doubleChunk = 2 * chunkSize;

    // Create a sparse bitmap tree,
    bitmap->add(base - doubleChunk);
    bitmap->add(base + doubleChunk);

    SparseHeapBitmap* start = bitmap->hasRange(base - doubleChunk - 2, 20);
    EXPECT_TRUE(!!start);
    EXPECT_TRUE(start->isSet(base - doubleChunk));
    EXPECT_FALSE(start->isSet(base - doubleChunk + 1));
    EXPECT_FALSE(start->isSet(base));
    EXPECT_FALSE(start->isSet(base + 1));
    EXPECT_FALSE(start->isSet(base + doubleChunk));
    EXPECT_FALSE(start->isSet(base + doubleChunk + 1));

    start = bitmap->hasRange(base - doubleChunk - 2, 2048);
    EXPECT_TRUE(!!start);
    EXPECT_TRUE(start->isSet(base - doubleChunk));
    EXPECT_FALSE(start->isSet(base - doubleChunk + 1));
    EXPECT_TRUE(start->isSet(base));
    EXPECT_FALSE(start->isSet(base + 1));
    EXPECT_TRUE(start->isSet(base + doubleChunk));
    EXPECT_FALSE(start->isSet(base + doubleChunk + 1));

    start = bitmap->hasRange(base, 20);
    EXPECT_TRUE(!!start);
    // Probing for values outside of hasRange() should be considered
    // undefined, but do it to exercise the (left) tree traversal.
    EXPECT_TRUE(start->isSet(base - doubleChunk));
    EXPECT_FALSE(start->isSet(base - doubleChunk + 1));
    EXPECT_TRUE(start->isSet(base));
    EXPECT_FALSE(start->isSet(base + 1));
    EXPECT_TRUE(start->isSet(base + doubleChunk));
    EXPECT_FALSE(start->isSet(base + doubleChunk + 1));

    start = bitmap->hasRange(base + chunkSize + 2, 2048);
    EXPECT_TRUE(!!start);
    // Probing for values outside of hasRange() should be considered
    // undefined, but do it to exercise the (left) tree traversal.
    EXPECT_FALSE(start->isSet(base - doubleChunk));
    EXPECT_FALSE(start->isSet(base - doubleChunk + 1));
    EXPECT_FALSE(start->isSet(base));
    EXPECT_FALSE(start->isSet(base + 1));
    EXPECT_FALSE(start->isSet(base + chunkSize));
    EXPECT_TRUE(start->isSet(base + doubleChunk));
    EXPECT_FALSE(start->isSet(base + doubleChunk + 1));
}

TEST(HeapCompactTest, LeftExtension)
{
    Address base = reinterpret_cast<Address>(0x1000u);
    OwnPtr<SparseHeapBitmap> bitmap = SparseHeapBitmap::create(base);

    SparseHeapBitmap* start = bitmap->hasRange(base, 1);
    ASSERT(start);

    // Verify that re-adding is a no-op.
    bitmap->add(base);
    EXPECT_EQ(start, bitmap->hasRange(base, 1));

    // Adding an Address |A| before a single-address SparseHeapBitmap node should
    // cause that node to  be "left extended" to use |A| as its new base.
    bitmap->add(base - 2);
    EXPECT_EQ(bitmap->hasRange(base, 1), bitmap->hasRange(base - 2, 1));

    // Reset.
    bitmap = SparseHeapBitmap::create(base);

    // If attempting same as above, but the Address |A| is outside the
    // chunk size of a node, a new SparseHeapBitmap node needs to be
    // created to the left of |bitmap|.
    bitmap->add(base - chunkSize);
    EXPECT_NE(bitmap->hasRange(base, 1), bitmap->hasRange(base - 2, 1));

    bitmap = SparseHeapBitmap::create(base);
    bitmap->add(base - chunkSize + 1);
    // This address is just inside the horizon and shouldn't create a new chunk.
    EXPECT_EQ(bitmap->hasRange(base, 1), bitmap->hasRange(base - 2, 1));
    // ..but this one should, like for the sub-test above.
    bitmap->add(base - chunkSize);
    EXPECT_EQ(bitmap->hasRange(base, 1), bitmap->hasRange(base - 2, 1));
    EXPECT_NE(bitmap->hasRange(base, 1), bitmap->hasRange(base - chunkSize, 1));
}

} // namespace blink
