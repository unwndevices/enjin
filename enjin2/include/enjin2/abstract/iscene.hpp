#pragma once

#include <cstdint>

namespace enjin2 {

// Forward declaration
template <typename TPixel>
class ICanvas;

/**
 * @brief Abstract scene interface for scene lifecycle
 * @tparam PixelType Pixel type for rendering (e.g., Pixel4, uint8_t)
 *
 * Both enjin1 and enjin2 can implement this interface for compile-time polymorphism.
 * Provides standard scene lifecycle methods (onCreate, onUpdate, onRender, etc.).
 */
template <typename PixelType>
class IScene {
public:
    /**
     * @brief Virtual destructor for proper cleanup through base pointer
     */
    virtual ~IScene() = default;

    // ===== Scene lifecycle methods =====

    /**
     * @brief Called when scene is created
     *
     * Called once when the scene is first created.
     * Override to set up initial objects and state.
     */
    virtual void onCreate() = 0;

    /**
     * @brief Called when scene becomes active
     *
     * Called when the scene becomes the active scene.
     * Use this to resume animations, start background processes, etc.
     */
    virtual void onActivate() = 0;

    /**
     * @brief Called when scene becomes inactive
     *
     * Called when the scene is no longer active.
     * Use this to pause animations, stop background processes, etc.
     */
    virtual void onDeactivate() = 0;

    /**
     * @brief Called when scene is destroyed
     *
     * Use this to clean up scene-specific resources.
     */
    virtual void onDestroy() = 0;

    /**
     * @brief Called every frame during update
     * @param deltaTime Time since last frame in milliseconds
     *
     * Use this for scene-specific update logic that should happen
     * before object updates.
     */
    virtual void onUpdate(uint16_t deltaTime) = 0;

    /**
     * @brief Called during rendering
     * @param canvas Target canvas for rendering
     *
     * Use this for scene-specific rendering like backgrounds or UI overlays.
     */
    virtual void onRender(ICanvas<PixelType>& canvas) = 0;

    // ===== Scene state queries =====

    /**
     * @brief Get scene ID
     * @return Scene identifier
     */
    virtual uint32_t getId() const = 0;

    /**
     * @brief Check if scene is active
     * @return True if scene is active
     */
    virtual bool isActive() const = 0;

    /**
     * @brief Check if scene is initialized
     * @return True if scene is initialized
     */
    virtual bool isInitialized() const = 0;
};

} // namespace enjin2
