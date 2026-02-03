#pragma once

#include <cstdint>
#include <cstddef>

namespace enjin2 {

/**
 * @file memory.hpp
 * @brief Memory management utilities for embedded systems
 *
 * Provides static pool, handle-based object management, and stack allocation
 * for systems with limited memory and no dynamic allocation.
 */

/**
 * @brief Static memory pool for embedded systems
 *
 * Pre-allocates storage for a fixed number of objects of type T.
 * All allocations come from the pre-allocated pool, avoiding dynamic memory.
 *
 * @tparam T Type of objects to allocate
 * @tparam CAPACITY Maximum number of objects that can be allocated
 */
template<typename T, size_t CAPACITY>
class StaticPool {
private:
    alignas(T) uint8_t storage[sizeof(T) * CAPACITY];
    bool used[CAPACITY];
    size_t count;
    
public:
    /**
     * @brief Constructor initializes empty pool
     */
    StaticPool() : count(0) {
        for (size_t i = 0; i < CAPACITY; ++i) {
            used[i] = false;
        }
    }

    /**
     * @brief Allocate memory for an object of type T
     * @return Pointer to allocated memory, or nullptr if pool is exhausted
     */
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
    
    /**
     * @brief Deallocate memory and call object destructor
     * @param ptr Pointer to object to deallocate (may be nullptr)
     */
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

    /**
     * @brief Get current number of allocated objects
     * @return Current allocation count
     */
    size_t size() const { return count; }

    /**
     * @brief Get maximum capacity of the pool
     * @return Total capacity (CAPACITY template parameter)
     */
    size_t capacity() const { return CAPACITY; }

    /**
     * @brief Check if pool has no allocations
     * @return true if pool is empty, false otherwise
     */
    bool empty() const { return count == 0; }

    /**
     * @brief Check if pool is at maximum capacity
     * @return true if pool is full, false otherwise
     */
    bool full() const { return count == CAPACITY; }
};

/**
 * @brief Handle-based reference to avoid dangling pointers
 *
 * Uses index + generation to safely reference objects in pools.
 * Generation counter prevents accessing stale or destroyed objects.
 *
 * @tparam T Type of referenced object
 */
template<typename T>
struct Handle {
    uint16_t index; ///< Index into object pool
    uint16_t generation; ///< Generation counter for validation

    /**
     * @brief Default constructor creates invalid handle
     */
    Handle() : index(0), generation(0) {}

    /**
     * @brief Constructor with index and generation
     * @param i Index into object pool
     * @param g Generation counter value
     */
    Handle(uint16_t i, uint16_t g) : index(i), generation(g) {}

    /**
     * @brief Check if handle is valid
     * @return true if handle refers to a valid object, false if invalid
     */
    bool isValid() const { return generation != 0; }

    /**
     * @brief Mark handle as invalid
     */
    void invalidate() { generation = 0; }

    /**
     * @brief Equality comparison operator
     * @param other Handle to compare with
     * @return true if handles refer to the same object
     */
    bool operator==(const Handle& other) const {
        return index == other.index && generation == other.generation;
    }

    /**
     * @brief Inequality comparison operator
     * @param other Handle to compare with
     * @return true if handles refer to different objects
     */
    bool operator!=(const Handle& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Handle-based object pool with generation tracking
 *
 * Combines StaticPool with handle-based access for safe object management.
 * Generation counters prevent use-after-free bugs.
 *
 * @tparam T Type of objects to manage
 * @tparam CAPACITY Maximum number of objects in pool
 */
template<typename T, size_t CAPACITY>
class HandlePool {
private:
    StaticPool<T, CAPACITY> pool;
    uint16_t generations[CAPACITY];
    T* objects[CAPACITY];

public:
    /**
     * @brief Constructor initializes empty pool with generation counters
     */
    HandlePool() {
        for (size_t i = 0; i < CAPACITY; ++i) {
            generations[i] = 1; // Start at 1 so 0 means invalid
            objects[i] = nullptr;
        }
    }

    /**
     * @brief Create a new object in the pool
     * @return Handle to newly created object, or invalid handle if pool is full
     */
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

    /**
     * @brief Destroy object referenced by handle
     * @param handle Handle to object to destroy (invalid handles are ignored)
     */
    void destroy(Handle<T> handle) {
        if (!isValid(handle)) return;
        
        T* obj = objects[handle.index];
        if (obj) {
            pool.deallocate(obj);
            objects[handle.index] = nullptr;
            generations[handle.index]++;
        }
    }

    /**
     * @brief Get pointer to object from handle
     * @param handle Handle to object
     * @return Pointer to object, or nullptr if handle is invalid
     */
    T* get(Handle<T> handle) {
        if (!isValid(handle)) return nullptr;
        return objects[handle.index];
    }

    /**
     * @brief Get const pointer to object from handle
     * @param handle Handle to object
     * @return Const pointer to object, or nullptr if handle is invalid
     */
    const T* get(Handle<T> handle) const {
        if (!isValid(handle)) return nullptr;
        return objects[handle.index];
    }

    /**
     * @brief Check if handle is valid
     * @param handle Handle to validate
     * @return true if handle is valid and refers to an existing object
     */
    bool isValid(Handle<T> handle) const {
        return handle.index < CAPACITY && 
               handle.generation == generations[handle.index] &&
               objects[handle.index] != nullptr;
    }
};

/**
 * @brief Stack-based allocator for temporary allocations
 *
 * Allocates memory linearly from a fixed buffer.
 * All allocations can be freed at once with reset(), ideal for frame-based allocations.
 */
class StackAllocator {
private:
    uint8_t* memory;
    size_t capacity;
    size_t top;

public:
    /**
     * @brief Construct stack allocator with provided buffer
     * @param mem Pointer to pre-allocated memory buffer
     * @param cap Total capacity of buffer in bytes
     */
    StackAllocator(uint8_t* mem, size_t cap)
        : memory(mem), capacity(cap), top(0) {}

    /**
     * @brief Allocate memory from stack
     * @param size Number of bytes to allocate
     * @param alignment Alignment requirement in bytes (default: pointer size)
     * @return Pointer to allocated memory, or nullptr if out of space
     */
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

    /**
     * @brief Reset allocator, freeing all allocations
     */
    void reset() { top = 0; }

    /**
     * @brief Get currently used memory
     * @return Number of bytes currently allocated
     */
    size_t getUsed() const { return top; }

    /**
     * @brief Get remaining free memory
     * @return Number of bytes available for allocation
     */
    size_t getRemaining() const { return capacity - top; }
};

} // namespace enjin2