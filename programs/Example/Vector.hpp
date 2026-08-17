#pragma once
#include <temrixstd.h>

template<typename T>
class Vector {
    T*                      data      = nullptr;
    uint32_t                len       = 0;
    uint32_t                cap       = 0;

    void grow(uint32_t needed) {
        if (needed <= cap) return;
        uint32_t newCap = cap ? cap * 2 : 8;
        while (newCap < needed) newCap *= 2;
 
        T* newData = (T*)Syscall::Memory::Map(newCap * sizeof(T));
 
        for (uint32_t i = 0; i < len; i++) {
            newData[i] = data[i];
        }
 
        if (data) Syscall::Memory::Unmap((uint64_t)data, cap * sizeof(T));
        data = newData;
        cap  = newCap;
    }
 
public:
    Vector() {}
 
    Vector(uint32_t initialCap = 8)
    {
        grow(initialCap);
    }
 
    void init(uint32_t initialCap = 8) {
        grow(initialCap);
    }
 
    Vector(const Vector& o) {
        if (!o.data) return;
        grow(o.len);
        for (uint32_t i = 0; i < o.len; i++)
            data[i] = o.data[i];
        len = o.len;
    }
 
    Vector& operator=(const Vector& o) {
        if (this == &o) return *this;
        clear();
        if (!o.data) return *this;
        grow(o.len);
        for (uint32_t i = 0; i < o.len; i++)
            data[i] = o.data[i];
        len = o.len;
        return *this;
    }
 
    T&       operator[](uint32_t i)       { return data[i]; }
    const T& operator[](uint32_t i) const { return data[i]; }
 
    T*       begin()       { return data; }
    T*       end()         { return data + len; }
    const T* begin() const { return data; }
    const T* end()   const { return data + len; }
 
    uint32_t size()     const { return len; }
    uint32_t capacity() const { return cap; }
    bool     empty()    const { return len == 0; }
 
    T&       front()       { return data[0]; }
    const T& front() const { return data[0]; }
    T&       back()        { return data[len - 1]; }
    const T& back()  const { return data[len - 1]; }
 
    void push(const T& val) {
        grow(len + 1);
        data[len++] = val;
    }
 
    T pop() {
        T val = data[--len];
        return val;
    }
 
    void insert(uint32_t idx, const T& val) {
        grow(len + 1);
        for (uint32_t i = len; i > idx; i--)
            data[i] = data[i - 1];
        data[idx] = val;
        len++;
    }
 
    void remove(uint32_t idx) {
        for (uint32_t i = idx; i + 1 < len; i++)
            data[i] = data[i + 1];
        len--;
    }
 
    void clear() {
        len = 0;
    }
 
    void reserve(uint32_t newCap) {
        grow(newCap);
    }
 
    void resize(uint32_t newLen, const T& val = T{}) {
        grow(newLen);
        for (uint32_t i = len; i < newLen; i++)
            data[i] = val;
        len = newLen;
    }
 
    int find(const T& val) const {
        for (uint32_t i = 0; i < len; i++)
            if (data[i] == val) return (int)i;
        return -1;
    }
 
    bool contains(const T& val) const {
        return find(val) != -1;
    }
 
    ~Vector() {
        if (data) Syscall::Memory::Unmap((uint64_t)data, cap * sizeof(T));
    }
};