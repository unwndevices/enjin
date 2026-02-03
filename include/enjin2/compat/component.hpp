/**
 * @file component.hpp
 * @brief Compatibility layer for component lifecycle methods
 *
 * Provides PascalCase wrapper functions for enjin1 compatibility.
 * Deprecated after enjin1 deletion.
 */
#pragma once

// Migration support - deprecated after enjin1 deletion

#include "enjin2/core/component.hpp"

namespace enjin {

// Type alias for enjin1 compatibility
using Component = enjin2::Component;

/**
 * @brief Wrapper for enjin1 Awake lifecycle method
 *
 * In enjin1, components used PascalCase: Awake()
 * In enjin2, components use camelCase: awake()
 *
 * @param comp Pointer to component to call awake on
 */
inline void Awake(enjin2::Component* comp) {
    if (comp) comp->awake();
}

/**
 * @brief Wrapper for enjin1 Start lifecycle method
 *
 * In enjin1, components used PascalCase: Start()
 * In enjin2, components use camelCase: start()
 *
 * @param comp Pointer to component to call start on
 */
inline void Start(enjin2::Component* comp) {
    if (comp) comp->start();
}

/**
 * @brief Wrapper for enjin1 Update lifecycle method
 *
 * In enjin1, components used PascalCase: Update()
 * In enjin2, components use camelCase: update()
 *
 * @param comp Pointer to component to call update on
 * @param deltaTime Time since last frame in milliseconds
 */
inline void Update(enjin2::Component* comp, uint16_t deltaTime) {
    if (comp) comp->update(deltaTime);
}

/**
 * @brief Wrapper for enjin1 LateUpdate lifecycle method
 *
 * In enjin1, components used PascalCase: LateUpdate()
 * In enjin2, components use camelCase: lateUpdate()
 *
 * @param comp Pointer to component to call lateUpdate on
 * @param deltaTime Time since last frame in milliseconds
 */
inline void LateUpdate(enjin2::Component* comp, uint16_t deltaTime) {
    if (comp) comp->lateUpdate(deltaTime);
}

} // namespace enjin
