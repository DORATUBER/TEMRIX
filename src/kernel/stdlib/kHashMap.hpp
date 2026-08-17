#pragma once
#include "kernel/common.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"
#include "kernel/spinlock.hpp"

namespace Memory
{
    template <typename K, typename V, typename Hasher>
    class KHashMap
    {
    public:
        void init(BuddyAllocator *phys, uint64_t initialBuckets = 16)
        {
            m_phys = phys;
            m_buckets = nullptr;
            m_bucketCount = 0;
            m_entries = nullptr;
            m_entryCap = 0;
            m_entryCount = 0;
            m_freeHead = -1;

            resizeBuckets(initialBuckets);
        }

        bool insert(const K &key, const V &value)
        {
            int64_t existing = findSlot(key);
            if (existing >= 0)
            {
                m_entries[existing].value = value;
                return true;
            }

            if (m_entryCount * 4 >= m_bucketCount * 3) 
            {
                if (!resizeBuckets(m_bucketCount * 2))
                    return false;
            }

            int64_t slot = allocEntry();
            if (slot < 0)
                return false;

            uint64_t h = Hasher::hash(key) % m_bucketCount;
            m_entries[slot].key = key;
            m_entries[slot].value = value;
            m_entries[slot].used = true;
            m_entries[slot].next = m_buckets[h];
            m_buckets[h] = slot;
            m_entryCount++;
            return true;
        }

        bool find(const K &key, V &outValue) const
        {
            int64_t slot = findSlot(key);
            if (slot < 0)
                return false;
            outValue = m_entries[slot].value;
            return true;
        }

        bool remove(const K &key)
        {
            uint64_t h = Hasher::hash(key) % m_bucketCount;
            int64_t prev = -1;
            int64_t cur = m_buckets[h];

            while (cur >= 0)
            {
                if (m_entries[cur].used && Hasher::equals(m_entries[cur].key, key))
                {
                    if (prev < 0)
                        m_buckets[h] = m_entries[cur].next;
                    else
                        m_entries[prev].next = m_entries[cur].next;

                    m_entries[cur].used = false;
                    m_entries[cur].next = m_freeHead;
                    m_freeHead = cur;
                    m_entryCount--;
                    return true;
                }
                prev = cur;
                cur = m_entries[cur].next;
            }
            return false;
        }

        void destroy()
        {
            if (m_buckets)
                m_phys->free(m_buckets);
            if (m_entries)
                m_phys->free(m_entries);
            m_buckets = nullptr;
            m_entries = nullptr;
            m_bucketCount = 0;
            m_entryCap = 0;
            m_entryCount = 0;
            m_nextFreeIndex = 0;
        }

        uint64_t size() const { return m_entryCount; }

    private:
        struct Slot
        {
            K key;
            V value;
            bool used;
            int64_t next;
        };

        BuddyAllocator *m_phys;
        int64_t *m_buckets;     
        uint64_t m_bucketCount;
        Slot *m_entries;    
        uint64_t m_entryCap;
        uint64_t m_entryCount;
        int64_t m_freeHead; 
        uint64_t m_nextFreeIndex;

        int64_t findSlot(const K &key) const
        {
            if (m_bucketCount == 0)
                return -1;

            uint64_t h = Hasher::hash(key) % m_bucketCount;
            int64_t cur = m_buckets[h];
            while (cur >= 0)
            {
                if (m_entries[cur].used && Hasher::equals(m_entries[cur].key, key))
                    return cur;
                cur = m_entries[cur].next;
            }
            return -1;
        }

        int64_t allocEntry()
        {
            if (m_freeHead >= 0)
            {
                int64_t slot = m_freeHead;
                m_freeHead = m_entries[slot].next;
                return slot;
            }

            if (m_nextFreeIndex >= m_entryCap)
            {
                uint64_t newCap = m_entryCap == 0 ? 16 : m_entryCap * 2;
                Slot *newEntries = (Slot*)m_phys->malloc(newCap * sizeof(Slot));
                if (!newEntries)
                    return -1;

                if (m_entries)
                {
                    for (uint64_t i = 0; i < m_entryCap; i++)
                        newEntries[i] = m_entries[i];
                    m_phys->free(m_entries);
                }

                m_entries = newEntries;
                m_entryCap = newCap;
            }
            return (int64_t)(m_nextFreeIndex++);
        }

        bool resizeBuckets(uint64_t newCount)
        {
            int64_t *newBuckets = (int64_t*)m_phys->malloc(newCount * sizeof(int64_t));
            if (!newBuckets)
                return false;

            for (uint64_t i = 0; i < newCount; i++)
                newBuckets[i] = -1;

            if (m_buckets)
            {
                for (uint64_t i = 0; i < m_entryCap; i++)
                {
                    if (!m_entries[i].used)
                        continue;
                    uint64_t h = Hasher::hash(m_entries[i].key) % newCount;
                    m_entries[i].next = newBuckets[h];
                    newBuckets[h] = (int64_t)i;
                }
                m_phys->free(m_buckets);
            }

            m_buckets = newBuckets;
            m_bucketCount = newCount;
            return true;
        }
    };
}