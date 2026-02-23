/**
 * @file scene.hpp
 * @brief Compatibility layer for scene lifecycle methods
 *
 * Provides PascalCase wrapper functions for enjin1 compatibility.
 * Deprecated after enjin1 deletion.
 */
#pragma once

// Migration support - deprecated after enjin1 deletion

#include "enjin2/core/scene.hpp"

namespace enjin {

/// @brief Scene alias mapping to enjin2::Scene
using Scene = enjin2::Scene;

/**
 * @brief Wrapper for enjin1 OnCreate lifecycle method
 *
 * In enjin1, scenes used PascalCase: OnCreate()
 * In enjin2, scenes use camelCase: onCreate()
 *
 * @param scene Pointer to scene to call onCreate on
 */
inline void OnCreate(enjin2::Scene* scene) {
    // Note: enjin2's onCreate is protected/virtual and called by initialize()
    // This wrapper is for calling logic that would have been in enjin1's OnCreate
    if (scene) scene->initialize();
}

/**
 * @brief Wrapper for enjin1 OnDestroy lifecycle method
 *
 * In enjin1, scenes used PascalCase: OnDestroy()
 * In enjin2, scenes use camelCase: onDestroy()
 *
 * @param scene Pointer to scene to call onDestroy on
 */
inline void OnDestroy(enjin2::Scene* scene) {
    // Note: enjin2's onDestroy is called automatically in destructor
    // This wrapper is for calling logic that would have been in enjin1's OnDestroy
    if (scene) {
        scene->deactivate();
        // Destructor will call onDestroy
    }
}

/**
 * @brief Wrapper for enjin1 OnActivate lifecycle method
 *
 * In enjin1, scenes used PascalCase: OnActivate()
 * In enjin2, scenes use camelCase: onActivate()
 *
 * @param scene Pointer to scene to call onActivate on
 */
inline void OnActivate(enjin2::Scene* scene) {
    if (scene) scene->activate();
}

/**
 * @brief Wrapper for enjin1 OnDeactivate lifecycle method
 *
 * In enjin1, scenes used PascalCase: OnDeactivate()
 * In enjin2, scenes use camelCase: onDeactivate()
 *
 * @param scene Pointer to scene to call onDeactivate on
 */
inline void OnDeactivate(enjin2::Scene* scene) {
    if (scene) scene->deactivate();
}

/**
 * @brief Wrapper for enjin1 Update lifecycle method
 *
 * In enjin1, scenes used PascalCase: Update()
 * In enjin2, scenes use camelCase: update()
 *
 * @param scene Pointer to scene to call update on
 * @param deltaTime Time since last frame in milliseconds
 */
inline void Update(enjin2::Scene* scene, uint16_t deltaTime) {
    if (scene) scene->update(deltaTime);
}

} // namespace enjin
