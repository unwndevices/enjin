#pragma once

#include <enjin2/abstract/iscene.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/core/scene_state_machine.hpp>

namespace enjin2 {

/**
 * SceneSeam - Scene boundary seam for enjin2
 *
 * Provides clean interface between scene system and enjin2 implementation.
 * All enjin1 backend infrastructure removed - enjin2-only implementation.
 *
 * @tparam PixelType Pixel type for rendering (e.g., Pixel4, uint8_t)
 */
template <typename PixelType>
class SceneSeam : public IScene<PixelType> {
private:
    SceneStateMachine* enjin2SM; ///< Pointer to enjin2 scene state machine
    uint32_t id;                ///< Scene ID
    bool active;                 ///< Scene active state
    bool initialized;            ///< Scene initialized state

public:
    /**
     * Construct scene seam
     * @param sceneId Scene identifier
     */
    explicit SceneSeam(uint32_t sceneId = 0)
        : enjin2SM(nullptr), id(sceneId), active(false), initialized(false) {}

    /**
     * Called when scene is created
     */
    void onCreate() override {
        // Scene creation handled by scene state machine
        // This is a no-op for the seam itself
    }

    /**
     * Called when scene becomes active
     */
    void onActivate() override {
        active = true;
    }

    /**
     * Called when scene becomes inactive
     */
    void onDeactivate() override {
        active = false;
    }

    /**
     * Called when scene is destroyed
     */
    void onDestroy() override {
        // Scene cleanup handled by scene state machine
        // This is a no-op for the seam itself
    }

    /**
     * Update scene state each frame
     * @param deltaTime Time since last frame (in milliseconds)
     */
    void onUpdate(uint16_t deltaTime) override {
        if (enjin2SM != nullptr) {
            enjin2SM->update(deltaTime);
        }
    }

    /**
     * Render the current scene to a canvas
     * @param canvas Canvas to render to
     */
    void onRender(ICanvas<PixelType>& canvas) override {
        if (enjin2SM != nullptr) {
            enjin2SM->render(canvas);
        }
    }

    /**
     * Get scene ID
     * @return Scene identifier
     */
    uint32_t getId() const override {
        return id;
    }

    /**
     * Check if scene is active
     * @return True if scene is active
     */
    bool isActive() const override {
        return active;
    }

    /**
     * Check if scene is initialized
     * @return True if scene is initialized
     */
    bool isInitialized() const override {
        return initialized;
    }

    /**
     * Initialize the scene system (legacy method, kept for compatibility)
     * Deprecated: Scene initialization now happens via onCreate/onActivate lifecycle
     */
    [[deprecated("Use onCreate/onActivate lifecycle instead")]]
    void initialize() {
        onCreate();
        initialized = true;
    }

    /**
     * Update scene state each frame (legacy method, kept for compatibility)
     * Deprecated: Use onUpdate instead
     */
    [[deprecated("Use onUpdate instead")]]
    void update(uint16_t deltaTime) {
        onUpdate(deltaTime);
    }

    /**
     * Render the current scene to a canvas (legacy method, kept for compatibility)
     * Deprecated: Use onRender instead
     * @param canvas Canvas to render to
     */
    [[deprecated("Use onRender instead")]]
    void render(ICanvas<PixelType>& canvas) {
        onRender(canvas);
    }
};

} // namespace enjin2
