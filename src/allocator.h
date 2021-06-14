#pragma once

#include <stdint.h>
#include "debug.h"


template <typename T, size_t N>
class fixed_pool_allocator {
public:
    fixed_pool_allocator() 
        : pool { 0 }
        , current(pool)
    { }

    T* allocate(size_t size) noexcept {
        if ((current + sizeof(T)) > (pool + sizeof(pool))) {
            Debug.println("Out of pool!");
            // Debug.print("curr:"); Debug.print((size_t)current, 16);
            // Debug.print("pool:"); Debug.print((size_t)pool, 16);
            // Debug.print(" size:"); Debug.println(sizeof(pool), 16);
            return nullptr; // Out of memory
        }

        void* p = current;
        //Debug.print("alloc:"); Debug.println((size_t)p, 16);
        current += sizeof(T);
        return static_cast<T*>(p);
    }

    template <class ...A>
    T* make_new(A&& ...args) {
        void* p = allocate(sizeof(T));
        if (p != nullptr) {
            return new (p) T(std::forward<A>(args)...);
        }
        return nullptr;
    }

    void deallocate(T* p, size_t n) noexcept {
        // Not supported, can only mass-deallocate.
    }

    void deallocate_all() noexcept {
        memset(pool, 0, sizeof(pool));
        current = pool;
    }

private:
    alignas(T) uint8_t pool[sizeof(T) * N];
    uint8_t* current;
};

template <typename T, size_t N>
void* operator new(size_t size, fixed_pool_allocator<T,N>& allocator) noexcept {
    return allocator.allocate(size);
}
