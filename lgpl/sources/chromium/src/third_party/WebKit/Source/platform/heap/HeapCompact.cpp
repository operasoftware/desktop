// Copyright 2016 Opera Software AS. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/HeapCompact.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Heap.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

// For this interval tree, return the sub tree that covers the [address, address + size) range.
// Null if there is none.
//
// That smaller interval can then be used to iterate over for the Addresses set within
// it.
SparseHeapBitmap* SparseHeapBitmap::hasRange(Address address, size_t size)
{
    // Starts after entirely, |m_right| handles.
    if (address > end())
        return m_right ? m_right->hasRange(address, size) : nullptr;

    // Starts within, |this| is included in the resulting range.
    if (address >= base())
        return this;

    Address right = address + size - 1;
    // Starts before, but intersects with our range.
    if (right >= base())
        return this;

    // Is before entirely, for |m_left| to handle.
    return m_left ? m_left->hasRange(address, size) : nullptr;
}

bool SparseHeapBitmap::isSet(Address address)
{
    if (address > end())
        return m_right && m_right->isSet(address);
    if (address >= base()) {
        if (m_bitmap)
            return m_bitmap->test(address - base());
        return m_size == 1;
    }
    return m_left && m_left->isSet(address);
}

void SparseHeapBitmap::add(Address address)
{
    // |address| is beyond the maximum that this SparseHeapBitmap node can encompass.
    if (address >= maxEnd()) {
        if (!m_right) {
            m_right = SparseHeapBitmap::create(address);
            return;
        }
        m_right->add(address);
        return;
    }
    // Same on the other side.
    if (address < minStart()) {
        if (!m_left) {
            m_left = SparseHeapBitmap::create(address);
            return;
        }
        m_left->add(address);
        return;
    }
    if (address == base())
        return;
    // |address| can be encompassed by |this| by expanding its size.
    if (address > base()) {
        if (!m_bitmap)
            createBitmap();
        m_bitmap->set(address - base());
        return;
    }
    // Use |address| as the new base for this interval.
    Address oldBase = swapBase(address);
    createBitmap();
    m_bitmap->set(oldBase - address);
}

void SparseHeapBitmap::createBitmap()
{
    ASSERT(!m_bitmap && m_size == 1);
    m_bitmap = wrapUnique(new std::bitset<s_maxRange>(s_maxRange));
    m_size = s_maxRange;
    m_bitmap->set(0);
}

class HeapCompact::MovableObjectFixups {
public:
    static std::unique_ptr<MovableObjectFixups> create()
    {
        return wrapUnique(new MovableObjectFixups);
    }

    ~MovableObjectFixups()
    {
    }

    void addCompactablePage(BasePage* p)
    {
        // Add all pages belonging to arena to the set of relocatable pages.
        m_relocatablePages.add(p);
    }

    void add(void** reference)
    {
        void* table = *reference;
        BasePage* tablePage = pageFromObject(table);
        // Nothing to compact on a large object's page.
        if (tablePage->isLargeObjectPage())
            return;

#if ENABLE(ASSERT)
        auto it = m_fixups.find(table);
        ASSERT(it == m_fixups.end() || it->value == reinterpret_cast<void*>(reference));
#endif
        Address refAddress = reinterpret_cast<Address>(reference);
        BasePage* refPage = reinterpret_cast<BasePage*>(blinkPageAddress(refAddress) + blinkGuardPageSize);
        if (m_relocatablePages.contains(refPage)) {
            ASSERT(!refPage->isLargeObjectPage());
            // If this is an interior slot (interior to some other backing store),
            // record it as such. This entails:
            //
            //  - storing it in the interior map - mapping the slot to
            //    its (eventual) location. Initially 0.
            //  - mark it as being interior pointer within the page's
            //    "interior" bitmap. This bitmap is used when moving a backing
            //    store, checking if interior slots will have to be redirected
            //
            if (HeapCompact::isCompactingArena(refPage->arena()->arenaIndex()))
                addInteriorFixup(refAddress, reference);
        }
        m_fixups.add(table, reinterpret_cast<void*>(reference));
    }

    void addCallback(void* backingStore, void* data, MovingObjectCallback callback)
    {
        ASSERT(!m_fixupCallbacks.contains(backingStore));
        m_fixupCallbacks.add(backingStore, std::pair<void*, MovingObjectCallback>(data, callback));
    }

    size_t size()
    {
        return m_fixups.size();
    }

    void relocateInteriorFixups(Address from, Address to, size_t size)
    {
        SparseHeapBitmap* range = m_interiors->hasRange(from, size);
        if (!range)
            return;

        // Scan through the payload, looking for interior pointer slots
        // to adjust. If the backing store of such an interior slot hasn't
        // been moved already, update the slot -> real location mapping.
        // When the backing store is eventually moved, it'll use that location.
        //
        for (size_t i = 0; i < size; i += sizeof(void*)) {
            if (!range->isSet(from + i))
                continue;
            auto it = m_interiorFixups.find(from + i);
            if (it == m_interiorFixups.end())
                continue;

            // If |slot|'s mapping is set, then the slot has been adjusted already.
            if (it->value)
                continue;
            LOG_HEAP_COMPACTION("range interior fixup: %p %p %p\n", from + i, it->value, to + i);
            void* fixup = reinterpret_cast<void*>(to + i);
            // Fill in the relocated location of the original slot |from + i|;
            // when the backing store corresponding to |from + i| is eventually
            // moved/compacted, it'll update |to + i| with a pointer to the
            // moved backing store.
            m_interiorFixups.set(reinterpret_cast<void*>(from + i), fixup);
        }
    }

    void relocate(Address from, Address to)
    {
        auto it = m_fixups.find(from);
        ASSERT(it != m_fixups.end());
        void** slot = reinterpret_cast<void**>(it->value);
        auto interior = m_interiorFixups.find(reinterpret_cast<void*>(slot));
        if (interior != m_interiorFixups.end()) {
            void** slotLocation = reinterpret_cast<void**>(interior->value);
            if (!slotLocation) {
                m_interiorFixups.set(slot, to);
            } else {
                LOG_HEAP_COMPACTION("Redirected slot: %p => %p\n", slot, slotLocation);
                slot = slotLocation;
            }
        }
        // If the slot has subsequently been updated, a prefinalizer or
        // a destructor having mutated and expanded/shrunk the collection,
        // do not update and relocate the slot -- |from| is no longer valid
        // and referenced.
        //
        // The slot's contents may also have been cleared during weak processing;
        // no work to be done in that case either.
        if (*slot != from) {
            LOG_HEAP_COMPACTION("No relocation: slot = %p, *slot = %p, from = %p, to = %p\n", slot, *slot, from , to);
            return;
        }
        *slot = to;

        size_t size = HeapObjectHeader::fromPayload(to)->payloadSize();
        auto callback = m_fixupCallbacks.find(from);
        if (callback != m_fixupCallbacks.end())
            callback->value.second(callback->value.first, from, to, size);

        if (!m_interiors)
            return;

        relocateInteriorFixups(from, to, size);
    }

    void addInteriorFixup(Address interior, void** slot)
    {
        ASSERT(!m_interiorFixups.contains(reinterpret_cast<void*>(slot)));
        m_interiorFixups.add(reinterpret_cast<void*>(slot), nullptr);
        addInteriorMapping(interior);
    }

    void addInteriorMapping(Address interior)
    {
        LOG_HEAP_COMPACTION("Interior: %p\n", interior);
        if (!m_interiors) {
            m_interiors = SparseHeapBitmap::create(interior);
            return;
        }
        m_interiors->add(interior);
    }

    void addRelocation(void** slot)
    {
        void* heapObject = *slot;

        // Record the interior pointer.
        if (!m_fixups.contains(*slot))
            addInteriorFixup(reinterpret_cast<Address>(heapObject), slot);

        BasePage* heapPage = pageFromObject(heapObject);
        ASSERT(heapPage);
        ASSERT(!heapPage->isLargeObjectPage());
        // For now, the heap objects we're adding relocations for are assumed
        // to be residing in a compactable heap. There's no reason why it must be
        // so, just a sanity checking assert while phasing in this extra set of
        // relocations.
        ASSERT(m_relocatablePages.contains(heapPage));

        NormalPage* normalPage = static_cast<NormalPage*>(heapPage);
        auto perHeap = m_externalRelocations.find(normalPage->arenaForNormalPage());
        if (perHeap == m_externalRelocations.end()) {
            Vector<void**> relocations;
            relocations.append(slot);
            HashMap<void*, Vector<void**>> table;
            table.add(*slot, relocations);
            m_externalRelocations.add(normalPage->arenaForNormalPage(), table);
            return;
        }
        auto entry = perHeap->value.find(*slot);
        if (entry == perHeap->value.end()) {
            Vector<void**> relocations;
            relocations.append(slot);
            perHeap->value.add(*slot, relocations);
            return;
        }
        entry->value.append(slot);
    }

    void fixupExternalRelocations(NormalPageArena* arena)
    {
        auto perHeap = m_externalRelocations.find(arena);
        if (perHeap == m_externalRelocations.end())
            return;
        for (const auto& entry : perHeap->value) {
            void* heapObject = entry.key;
            // The |heapObject| will either be in m_fixups or have been recorded as
            // an internal fixup.
            auto heapEntry = m_fixups.find(heapObject);
            if (heapEntry != m_fixups.end()) {
                for (auto slot : entry.value)
                    *slot = heapEntry->value;
                continue;
            }
            // The movement of the containing object will have moved the
            // interior slot.
            auto it = m_interiorFixups.find(heapObject);
            ASSERT(it != m_interiorFixups.end());
            for (auto slot : entry.value)
                *slot = it->value;
        }
    }

private:
    MovableObjectFixups()
    {
    }

    // For now, keep a hash map which for each movable object, records
    // the slot that points to it. Upon moving, that slot needs to
    // be updated.
    //
    // TODO: consider in-place updating schemes.
    HashMap<void*, void*> m_fixups;

    // Map from (old) table to callbacks that need to be invoked
    // when it has moved.
    HashMap<void*, std::pair<void*, MovingObjectCallback>> m_fixupCallbacks;

    // slot => relocated slot/final backing.
    HashMap<void*, void*> m_interiorFixups;

    HashSet<BasePage*> m_relocatablePages;

    std::unique_ptr<SparseHeapBitmap> m_interiors;

    // Each heap/arena may have some additional non-backing store slot references
    // into it that needs to be fixed up & relocated after compaction has happened.
    //
    // (This is currently not needed for Blink, but functionality is kept
    // around to be able to support this should the need arise.)
    HashMap<NormalPageArena*, HashMap<void*, Vector<void**>>> m_externalRelocations;
};

HeapCompact::HeapCompact()
    : m_doCompact(false)
    , m_gcCount(0)
    , m_threadCount(0)
    , m_freeListAllocations(0)
    , m_freedPages(0)
    , m_freedSize(0)
#if DEBUG_LOG_HEAP_COMPACTION_RUNNING_TIME
    , m_startCompaction(0)
#endif
{
}

HeapCompact::~HeapCompact()
{
}

HeapCompact::MovableObjectFixups& HeapCompact::fixups()
{
    if (!m_fixups)
        m_fixups = MovableObjectFixups::create();
    return *m_fixups;
}

void HeapCompact::checkIfCompacting(ThreadHeap* heap, Visitor* visitor, BlinkGC::GCType gcType, BlinkGC::GCReason reason)
{
    // Called when GC is being attempted initiated (by ThreadHeap::collectGarbage()),
    // checking if there's sufficient reason to do a compaction pass on completion
    // of the GC (but before lazy sweeping.)
    //
    // TODO(sof): reconsider what is an effective policy for when compaction
    // is required. Both in terms of frequency and freelist residency.
#if ENABLE_HEAP_COMPACTION
    if (!RuntimeEnabledFeatures::heapCompactionEnabled())
        return;

    m_doCompact = false;
    LOG_HEAP_COMPACTION("check if compacting: gc=%s count=%lu free=%lu\n", ThreadHeap::gcReasonString(reason), m_gcCount, m_freeListAllocations);
    m_gcCount++;
    // It is only safe to compact during non-conservative GCs.
    if (reason != BlinkGC::IdleGC && reason != BlinkGC::PreciseGC)
        return;

    // If any of the participating threads require a stack scan,
    // do not compact.
    //
    // Why? Should the stack contain an iterator pointing into its
    // associated backing store, its references wouldn't be
    // correctly relocated.
    for (ThreadState* state : heap->threads())
        if (state->stackState() == BlinkGC::HeapPointersOnStack)
            return;

    m_freedPages = 0;
    m_freedSize = 0;

#if STRESS_TEST_HEAP_COMPACTION
    // Exercise the handling of object movement by compacting as
    // often as possible.
    m_doCompact = true;
    m_threadCount = heap->threads().size();
    visitor->setMarkCompactionMode();
    if (m_gcCount) {
        m_fixups.clear();
        return;
    }
#endif

    // Compact enable rules:
    //  - a while since last.
    //  - considerable amount of heap bound up in freelist
    //    allocations. For the moment, use a fixed limit
    //    irrespective of heap size.
    //    TODO: switch to a lower bound + compute free/total
    //    ratio.
    //
    // As this isn't compacting all heaps/arenas, the cost of doing compaction
    // isn't a worry.
    m_doCompact = m_gcCount > kCompactIntervalThreshold && m_freeListAllocations > kFreeThreshold;
    if (m_doCompact) {
        LOG_HEAP_COMPACTION("Compacting: free=%lu\n", m_freeListAllocations);
        m_threadCount = heap->threads().size();
        visitor->setMarkCompactionMode();
        m_fixups.reset();
        m_gcCount = 0;
        return;
    }
#endif // ENABLE_HEAP_COMPACTION
}

void HeapCompact::registerMovingObjectReference(void** reference)
{
    if (!m_doCompact)
        return;

    fixups().add(reference);
}

void HeapCompact::registerMovingObjectCallback(void* backingStore, void* data, MovingObjectCallback callback)
{
    if (!m_doCompact)
        return;

    fixups().addCallback(backingStore, data, callback);
}

void HeapCompact::registerRelocation(void** slot)
{
    if (!m_doCompact)
        return;

    if (!*slot)
        return;

    fixups().addRelocation(slot);
}

bool HeapCompact::isCompacting()
{
    return m_doCompact;
}

void HeapCompact::setFreeListAllocations(size_t freeSize)
{
    LOG_HEAP_FREELIST("Freelist size: %lu\n", freeSize);
    if (m_doCompact) {
        // Reset the total freelist allocation if we're about to compact.
        m_freeListAllocations = 0;
        return;
    }

    // TODO(sof): decide on how to smooth the samplings, if at all.
    m_freeListAllocations = freeSize;
}

void HeapCompact::finishedArenaCompaction(NormalPageArena* arena, size_t freedPages, size_t freedSize)
{
    // TODO(sof): no risk of thread interference?
    if (!m_doCompact)
        return;

    fixups().fixupExternalRelocations(arena);
    m_freedPages += freedPages;
    m_freedSize += freedSize;
}

void HeapCompact::movedObject(Address from, Address to)
{
    ASSERT(m_fixups);
    m_fixups->relocate(from, to);
}

void HeapCompact::startCompacting(ThreadState*)
{
#if DEBUG_LOG_HEAP_COMPACTION_RUNNING_TIME
    // TODO: avoid write race.
    if (!m_startCompaction)
        m_startCompaction = WTF::currentTimeMS();
#endif
}

void HeapCompact::finishedCompacting(ThreadState*)
{
    if (!m_doCompact)
        return;

    if (!atomicDecrement(&m_threadCount)) {
        // Final one clears out.
        m_fixups.reset();
        m_doCompact = false;
#if DEBUG_LOG_HEAP_COMPACTION_RUNNING_TIME
        double end = WTF::currentTimeMS();
        LOG_HEAP_COMPACTION_INTERNAL("Compaction stats: time=%gms, pages=%lu, size=%lu\n", end - m_startCompaction, m_freedPages, m_freedSize);
        m_startCompaction = 0;
#else
        LOG_HEAP_COMPACTION("Compaction stats: freed pages=%lu size=%lu\n", m_freedPages, m_freedSize);
#endif
    }
}

void HeapCompact::addCompactablePage(BasePage* p)
{
    if (!m_doCompact)
        return;
    fixups().addCompactablePage(p);
}

} // namespace blink
