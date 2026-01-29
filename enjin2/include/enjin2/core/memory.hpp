#pragma once

#include <cstdint>
#include <cstddef>

namespace enjin2 {

// Static memory pool for embedded systems
template<typename T, size_t CAPACITY>
class StaticPool {
private:
    alignas(T) uint8_t storage[sizeof(T) * CAPACITY];
    bool used[CAPACITY];
    size_t count;
    
public:
    StaticPool() : count(0) {
        for (size_t i = 0; i < CAPACITY; ++i) {
            used[i] = false;
        }
    }
    
    T* allocate() {
        for (size_t i = 0; i < CAPACITY; ++i) {
            if (!used[i]) {
                used[i] = true;
                count++;
                return reinterpret_cast<T*>(&storage[i * sizeof(T)]);
            }
        }
        return nullptr; // Pool exhausted
    }
    
    void deallocate(T* ptr) {
        if (!ptr) return;
        
        uintptr_t offset = reinterpret_cast<uintptr_t>(ptr) - 
                          reinterpret_cast<uintptr_t>(storage);
        size_t index = offset / sizeof(T);
        
        if (index < CAPACITY && used[index]) {
            used[index] = false;
            count--;
            ptr->~T(); // Call destructor
        }
    }
    
    size_t size() const { return count; }
    size_t capacity() const { return CAPACITY; }
    bool empty() const { return count == 0; }
    bool full() const { return count == CAPACITY; }
};

// Handle-based system to avoid pointer issues
template<typename T>
struct Handle {
    uint16_t index;
    uint16_t generation;
    
    Handle() : index(0), generation(0) {}
    Handle(uint16_t i, uint16_t g) : index(i), generation(g) {}
    
    bool isValid() const { return generation != 0; }
    void invalidate() { generation = 0; }
    
    bool operator==(const Handle& other) const {
        return index == other.index && generation == other.generation;
    }
    
    bool operator!=(const Handle& other) const {
        return !(*this == other);
    }
};

template<typename T, size_t CAPACITY>
class HandlePool {
private:
    StaticPool<T, CAPACITY> pool;
    uint16_t generations[CAPACITY];
    T* objects[CAPACITY];
    
public:
    HandlePool() {
        for (size_t i = 0; i < CAPACITY; ++i) {
            generations[i] = 1; // Start at 1 so 0 means invalid
            objects[i] = nullptr;
        }
    }
    
    Handle<T> create() {
        T* obj = pool.allocate();
        if (!obj) return Handle<T>(); // Invalid handle
        
        // Find the slot
        for (size_t i = 0; i < CAPACITY; ++i) {
            if (!objects[i]) {
                objects[i] = obj;
                return Handle<T>(i, generations[i]);
            }
        }
        
        // This shouldn't happen if pool allocation succeeded
        pool.deallocate(obj);
        return Handle<T>();
    }
    
    void destroy(Handle<T> handle) {
        if (!isValid(handle)) return;
        
        T* obj = objects[handle.index];
        if (obj) {
            pool.deallocate(obj);
            objects[handle.index] = nullptr;
            generations[handle.index]++;
        }
    }
    
    T* get(Handle<T> handle) {
        if (!isValid(handle)) return nullptr;
        return objects[handle.index];
    }
    
    const T* get(Handle<T> handle) const {
        if (!isValid(handle)) return nullptr;
        return objects[handle.index];
    }
    
    bool isValid(Handle<T> handle) const {
        return handle.index < CAPACITY && 
               handle.generation == generations[handle.index] &&
               objects[handle.index] != nullptr;
    }
};

// Stack allocator for temporary allocations
class StackAllocator {
private:
    uint8_t* memory;
    size_t capacity;
    size_t top;
    
public:
    StackAllocator(uint8_t* mem, size_t cap) 
        : memory(mem), capacity(cap), top(0) {}
    
    void* allocate(size_t size, size_t alignment = sizeof(void*)) {
        // Align the allocation
        size_t aligned_top = (top + alignment - 1) & ~(alignment - 1);
        
        if (aligned_top + size > capacity) {
            return nullptr; // Out of memory
        }
        
        void* result = memory + aligned_top;
        top = aligned_top + size;
        return result;
    }
    
    void reset() { top = 0; }
    
    size_t getUsed() const { return top; }
    size_t getRemaining() const { return capacity - top; }
};

} // namespace enjin2