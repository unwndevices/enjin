#include "../../include/enjin2/ui/system.hpp"

namespace enjin2 {

// Initialize static system ID counter
SystemID SystemBase::nextSystemID = 1;

EntityManager::EntityManager() : freeCount(MAX_ENTITIES), entityCount(0) {
    // Initialize generation counters to 1 (0 means invalid)
    for (size_t i = 0; i < MAX_ENTITIES; ++i) {
        generations[i] = 1;
        freeList[i] = i;
    }
}

Entity EntityManager::createEntity() {
    if (freeCount == 0) {
        return Entity(); // Invalid entity
    }
    
    size_t index = freeList[--freeCount];
    entityCount++;
    
    return Entity(static_cast<uint32_t>(index), generations[index]);
}

void EntityManager::destroyEntity(Entity entity) {
    if (!isValid(entity)) return;
    
    size_t index = entity.id;
    
    // Increment generation to invalidate existing handles
    generations[index]++;
    if (generations[index] == 0) {
        generations[index] = 1; // Skip 0 (invalid)
    }
    
    // Add to free list
    freeList[freeCount++] = index;
    entityCount--;
}

bool EntityManager::isValid(Entity entity) const {
    if (entity.id >= MAX_ENTITIES) return false;
    return generations[entity.id] == entity.generation;
}

} // namespace enjin2