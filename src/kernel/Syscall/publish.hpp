#pragma once
#include "kernel/common.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/stdlib/kHashMap.hpp"
#include "kernel/spinlock.hpp"

namespace Publish
{
    static constexpr uint32_t MAX_NAME_LEN = 128;
    static constexpr uint32_t MAX_DATA_LEN = 256;

    struct Key
    {
        char     bytes[MAX_NAME_LEN];
        uint32_t len;

        Key() : len(0) {}

        Key(const char* name, uint32_t nameLen)
        {
            len = (nameLen > MAX_NAME_LEN) ? MAX_NAME_LEN : nameLen;
            Memory::copy(bytes, name, len);
        }
    };

    struct Value
    {
        char     bytes[MAX_DATA_LEN];
        uint32_t len;

        Value() : len(0) {}

        Value(const char* data, uint32_t dataLen)
        {
            len = (dataLen > MAX_DATA_LEN) ? MAX_DATA_LEN : dataLen;
            Memory::copy(bytes, data, len);
        }
    };

    struct Hasher
    {
        static uint64_t hash(const Key &k)
        {
            
            uint64_t h = 14695981039346656037ULL;
            for (uint32_t i = 0; i < k.len; i++)
            {
                h ^= (uint8_t)k.bytes[i];
                h *= 1099511628211ULL;
            }
            return h;
        }

        static bool equals(const Key &a, const Key &b)
        {
            if (a.len != b.len)
                return false;
            return Memory::compare(a.bytes, b.bytes, a.len) == 0;
        }
    };

    class Table
    {
    public:
        void init(Memory::BuddyAllocator *phys)
        {
            m_map.init(phys, 64); 
        }

        bool publish(const char* name, uint32_t nameLen, const char* data, uint32_t dataLen)
        {
            if (nameLen == 0 || nameLen > MAX_NAME_LEN || dataLen > MAX_DATA_LEN)
                return false;

            Key key(name, nameLen);
            Value value(data, dataLen);

            m_lock.acquire();
            bool ok = m_map.insert(key, value);
            m_lock.release();
            return ok;
        }

        uint32_t lookup(const char* name, uint32_t nameLen, char* outBuf, uint32_t outCapacity)
        {
            if (nameLen == 0 || nameLen > MAX_NAME_LEN)
                return 0;

            Key key(name, nameLen);
            Value value;

            m_lock.acquire();
            bool found = m_map.find(key, value);
            m_lock.release();

            if (!found)
                return 0;

            uint32_t toCopy = (value.len < outCapacity) ? value.len : outCapacity;
            Memory::copy(outBuf, value.bytes, toCopy);
            return toCopy;
        }

    private:
        Memory::KHashMap<Key, Value, Hasher> m_map;
        Spinlock m_lock;
    };
}