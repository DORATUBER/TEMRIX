#pragma once
#include "kernel/common.hpp"
#include "kernel/MemoryCommon.hpp"
#include "kernel/BuddyAllocator.hpp"
#include "kernel/PageTable.hpp"

namespace Memory
{
    template <typename T>
    class KVector
    {
    public:
        void init(BuddyAllocator *phys)
        {
            m_phys = phys;
            m_data = nullptr;
            m_size = 0;
            m_cap = 0;
        }

        bool push(const T &val)
        {
            if (m_size == m_cap)
            {
                uint64_t newCap = m_cap == 0 ? 4 : m_cap * 2;
                uint64_t newBytes = newCap * sizeof(T);
                void *newData = m_phys->malloc(newBytes);
                if (!newData)
                    return false;

                
                uint8_t *src = reinterpret_cast<uint8_t *>(m_data);
                uint8_t *dst = reinterpret_cast<uint8_t *>(newData);
                for (uint64_t i = 0; i < m_size * sizeof(T); i++)
                    dst[i] = src[i];

                if (m_data)
                    m_phys->free(m_data);
                m_data = reinterpret_cast<T *>(newData);
                m_cap = newCap;
            }
            m_data[m_size++] = val;
            return true;
        }

        void remove(uint64_t idx)
        {
            if (idx >= m_size)
                return;
            for (uint64_t i = idx; i < m_size - 1; i++)
                m_data[i] = m_data[i + 1];
            m_size--;
        }

        void destroy()
        {
            if (m_data)
            {
                m_phys->free(m_data);
                m_data = nullptr;
            }
            m_size = 0;
            m_cap = 0;
        }

        T &operator[](uint64_t i) { return m_data[i]; }
        const T &operator[](uint64_t i) const { return m_data[i]; }
        uint64_t size() const { return m_size; }
        uint64_t capacity() const { return m_cap; }

    private:
        BuddyAllocator *m_phys;
        T *m_data;
        uint64_t m_size;
        uint64_t m_cap;
    };
}