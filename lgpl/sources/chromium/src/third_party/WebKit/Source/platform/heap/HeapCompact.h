// Copyright 2016 Opera Software AS. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeapCompact_h
#define HeapCompact_h

#include "platform/PlatformExport.h"
#include "platform/heap/BlinkGC.h"
#include "wtf/PtrUtil.h"

#include <bitset>
#include <memory>

// Global dev/debug switches:

// Set to 0 to prevent compaction GCs, disabling the heap compaction feature.
#define ENABLE_HEAP_COMPACTION 1

// Emit debug info during compaction.
#define DEBUG_HEAP_COMPACTION 0

// Emit stats on freelist occupancy.
// 0 - disabled, 1 - minimal, 2 - verbose.
#define DEBUG_HEAP_FREELIST 0

// Log the amount of time spent compacting.
#define DEBUG_LOG_HEAP_COMPACTION_RUNNING_TIME 0

// Set to 1 to also compact the vector backing store heaps (in addition to the hash table heap.)
#define HEAP_COMPACT_VECTOR_BACKING 1

// Compact during all idle + precise GCs; for debugging.
#define STRESS_TEST_HEAP_COMPACTION 0

#define LOG_HEAP_COMPACTION_INTERNAL(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)

#if DEBUG_HEAP_COMPACTION
#define LOG_HEAP_COMPACTION(msg, ...) LOG_HEAP_COMPACTION_INTERNAL(msg, ##__VA_ARGS__)
#else
#define LOG_HEAP_COMPACTION(msg, ...)
#endif

#if DEBUG_HEAP_FREELIST
#define LOG_HEAP_FREELIST(msg, ...) LOG_HEAP_COMPACTION_INTERNAL(msg, ##__VA_ARGS__)
#else
#define LOG_HEAP_FREELIST(msg, ...)
#endif

#if DEBUG_HEAP_FREELIST == 2
#define LOG_HEAP_FREELIST_VERBOSE(msg, ...) LOG_HEAP_COMPACTION_INTERNAL(msg, ##__VA_ARGS__)
#else
#define LOG_HEAP_FREELIST_VERBOSE(msg, ...)
#endif

namespace blink {

class NormalPageArena;
class BasePage;
class ThreadHeap;
class ThreadState;

class PLATFORM_EXPORT HeapCompact final {
public:
    static std::unique_ptr<HeapCompact> create()
    {
        return wrapUnique(new HeapCompact);
    }

    ~HeapCompact();

    // Check if a GC for the given type and reason should perform additional
    // heap compaction once it has run.
    //
    // If deemed worthy, heap compaction is implicitly initialized and set up.
    void checkIfCompacting(ThreadHeap*, Visitor*, BlinkGC::GCType, BlinkGC::GCReason);

    // Returns true if the ongoing GC will also compact.
    bool isCompacting();

    // Returns true if the ongoing GC will also compact the given arena/sub-heap.
    static bool isCompactingArena(int arenaIndex)
    {
#if HEAP_COMPACT_VECTOR_BACKING
        return arenaIndex >= BlinkGC::Vector1ArenaIndex && arenaIndex <= BlinkGC::HashTableArenaIndex;
#else
        return arenaIndex ==  BlinkGC::HashTableArenaIndex;
#endif
    }

    // Register |slot| as containing a reference to a movable backing store
    // object.
    //
    // When compaction moves the backing store object at |*slot| to |newAddress|,
    // |*slot| must be updated to hold |newAddress| instead.
    void registerMovingObjectReference(void** slot);

    // Register a callback to be invoked once the |backingStore| object is
    // moved; see MovingObjectCallback documentation for the arguments supplied
    // to the callback.
    //
    // This is needed to handle backing store objects containing intra-object
    // pointers, all of which must be relocated/rebased to be done wrt the
    // moved to location. For Blink, LinkedHashSet<> is the only abstraction
    // which relies on this feature.
    void registerMovingObjectCallback(void* backingStore, void* data, MovingObjectCallback);

    // Register |slot| as containing a reference to the interior of a movable object.
    //
    // registerMovingObjectReference() handles the common case of holding an external
    // reference to the backing store object. registerRelocation() handles the relocation
    // of external references into backing store objects - something very rarely done by
    // the Blink codebase, but a possibility.
    void registerRelocation(void** slot);

    // Signal that the compaction pass is being started, finished by some ThreadState.
    void startCompacting(ThreadState*);
    void finishedCompacting(ThreadState*);

    // Perform any relocation post-processing after having completed compacting the
    // given sub heap. Pass along the number of pages that were freed from the arena,
    // along with their total size.
    void finishedArenaCompaction(NormalPageArena*, size_t freedPages, size_t freedSize);

    // Record the main thread's freelist residency (in bytes.) This is done
    // after the decision has been made on whether or not to compact for the
    // current GC. If compacting, the size sampling will be ignored and
    // the internal counter is reset.
    void setFreeListAllocations(size_t);

    // Register the heap page as containing live objects that will all be
    // compacted. When the GC is compacting, that is.
    void addCompactablePage(BasePage*);

    // Notify heap compaction that object at |from| has been moved to
    // |to|.
    void movedObject(Address from, Address to);

private:
    class MovableObjectFixups;

    HeapCompact();

    // Parameters controlling when compaction should be done:

    // Number of GCs that must have passed since last compaction GC.
    static const int kCompactIntervalThreshold = 10;

    // Freelist size threshold that must be exceeded before compaction
    // should be considered.
    static const size_t kFreeThreshold = 512 * 1024;

    MovableObjectFixups& fixups();

    std::unique_ptr<MovableObjectFixups> m_fixups;

    bool m_doCompact;
    size_t m_gcCount;
    int m_threadCount;
    size_t m_freeListAllocations;
    size_t m_freedPages;
    size_t m_freedSize;
#if DEBUG_LOG_HEAP_COMPACTION_RUNNING_TIME
    double m_startCompaction;
#endif
};

// A sparse bitmap of heap addresses where the (very few) addresses that are
// set are likely to be in small clusters. The abstraction is tailored to
// support heap compaction, assuming the following:
//
//   - Addresses will be bitmap-marked from lower to higher addresses.
//   - Bitmap lookups are performed for each object that is compacted
//     and moved to some new location, supplying the (base, size)
//     pair of the object's heap allocation.
//   - If the sparse bitmap has any marked addresses in that range, it
//     returns a value that can be quickly iterated over to check which
//     addresses within the range are actually set.
//   - The bitmap is needed to support something that is very rarely done
//     by the current Blink codebase, which is to have nested collection
//     part objects. Consequently, it is safe to assume sparseness.
//
// Support the above by having a sparse bitmap organized as a binary
// tree with nodes covering fixed size ranges via a simple bitmap/set.
// That is, each SparseHeapBitmap node will contain a bitmap/set for
// some fixed size range, along with pointers to SparseHeapBitmaps
// for addresses outside of its range.
//
// The bitmap tree isn't kept balanced across the Address additions
// made.
class SparseHeapBitmap {
public:
    static std::unique_ptr<SparseHeapBitmap> create(Address base)
    {
        return wrapUnique(new SparseHeapBitmap(base));
    }

    ~SparseHeapBitmap()
    {
    }

    // Return the sparse bitmap subtree that covers the [address, address + size)
    // range, if any.
    //
    // The returned SparseHeapBitmap can be used to quickly lookup what
    // addresses in that range are set or not.
    SparseHeapBitmap* hasRange(Address, size_t);

    // True iff |address| is set for this SparseHeapBitmap tree.
    bool isSet(Address);

    // Mark |address| as present/set.
    void add(Address);

    // Partition the sparse bitmap into 256 Address chunks; a SparseHeapBitmap either
    // contains a single Address or a bitmap recording the mapping for [base, base + s_maxRange).
    static const size_t s_maxRange = 256;

private:
    SparseHeapBitmap(Address base)
        : m_base(base)
        , m_size(1)
    {
    }

    Address base() const { return m_base; }
    size_t size() const { return m_size; }
    Address end() const { return m_base + (m_size - 1); }

    Address maxEnd() const
    {
        return m_base + s_maxRange;
    }

    Address minStart() const
    {
        // If this bitmap node represents the sparse [m_base, s_maxRange) range,
        // do not allow it to be "left extended" as that would entail having
        // to shift down the contents of the std::bitset somehow.
        //
        // This shouldn't be a real problem as any clusters of set addresses
        // will be marked while iterating from lower to higher addresses, hence
        // "left extension" like this is unlikely to be common.
        if (m_bitmap)
            return m_base;
        return (m_base > reinterpret_cast<Address>(s_maxRange)) ? (m_base - s_maxRange + 1) : nullptr;
    }

    Address swapBase(Address address)
    {
        Address oldBase = m_base;
        m_base = address;
        return oldBase;
    }

    void createBitmap();

    Address m_base;
    // Either 1 or |s_maxRange|.
    size_t m_size;

    // If non-null, contains a bitmap for addresses within [m_base, m_size)
    std::unique_ptr<std::bitset<s_maxRange>> m_bitmap;

    std::unique_ptr<SparseHeapBitmap> m_left;
    std::unique_ptr<SparseHeapBitmap> m_right;
};

} // namespace blink

#endif // HeapCompact_h
