#pragma once

#include <enjin2/abstract/iscene.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/core/scene_state_machine.hpp>

namespace enjin2 {

/**
 * SceneSeam - Strangler Fig pattern seam for scene boundaries
 *
 * Allows enjin1 and enjin2 implementations to coexist during incremental migration.
 * Uses compile-time backend selection via USE_ENJIN1_BACKEND macro.
 *
 * @tparam PixelType Pixel type for rendering (e.g., Pixel4, uint8_t)
 */
template <typename PixelType>
class SceneSeam : public IScene<PixelType> {
public:
    /// Backend type selector (deprecated - kept for backward compatibility)
    [[deprecated("Use compile-time USE_ENJIN1_BACKEND macro instead")]]
    enum class Backend {
        ENJIN1,  ///< Use enjin1 legacy backend
        ENJIN2   ///< Use enjin2 new backend
    };

private:
    Backend currentBackend;      ///< Current backend type (deprecated)
    SceneStateMachine* enjin2SM; ///< Pointer to enjin2 scene state machine
    void* enjin1SM;             ///< Pointer to enjin1 scene manager (opaque)
    uint32_t id;               ///< Scene ID
    bool active;                ///< Scene active state
    bool initialized;           ///< Scene initialized state

public:
    /**
     * Construct scene seam with specified backend type
     * @param backend Which backend to use (deprecated)
     * @param sceneId Scene identifier
     */
    explicit SceneSeam(Backend backend = Backend::ENJIN2, uint32_t sceneId = 0)
        : currentBackend(backend), enjin2SM(nullptr), enjin1SM(nullptr),
          id(sceneId), active(false), initialized(false) {}

    /**
     * Called when scene is created
     */
    void onCreate() override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (currentBackend == Backend::ENJIN1 && enjin1SM != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        // Scene creation handled by scene state machine
        // This is a no-op for the seam itself
#endif
    }

    /**
     * Called when scene becomes active
     */
    void onActivate() override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (currentBackend == Backend::ENJIN1 && enjin1SM != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (currentBackend == Backend::ENJIN2 && enjin2SM != nullptr) {
            active = true;
        }
#endif
    }

    /**
     * Called when scene becomes inactive
     */
    void onDeactivate() override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (currentBackend == Backend::ENJIN1 && enjin1SM != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        active = false;
#endif
    }

    /**
     * Called when scene is destroyed
     */
    void onDestroy() override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (currentBackend == Backend::ENJIN1 && enjin1SM != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        // Scene cleanup handled by scene state machine
        // This is a no-op for the seam itself
#endif
    }

    /**
     * Update scene state each frame
     * @param deltaTime Time since last frame (in milliseconds)
     */
    void onUpdate(uint16_t deltaTime) override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (currentBackend == Backend::ENJIN1 && enjin1SM != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (currentBackend == Backend::ENJIN2 && enjin2SM != nullptr) {
            enjin2SM->update(deltaTime);
        }
#endif
    }

    /**
     * Render the current scene to a canvas
     * @param canvas Canvas to render to
     */
    void onRender(ICanvas<PixelType>& canvas) override {
#if USE_ENJIN1_BACKEND
        #error "enjin1 backend not yet integrated"
        // When enjin1 is integrated:
        // if (currentBackend == Backend::ENJIN1 && enjin1SM != nullptr) {
        //     // Route to enjin1 implementation
        // }
#else
        if (currentBackend == Backend::ENJIN2 && enjin2SM != nullptr) {
            enjin2SM->render(canvas);
        }
#endif
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
     * Switch to enjin2 backend (deprecated - kept for backward compatibility)
     * @param newSM Pointer to enjin2 scene state machine to use
     */
    [[deprecated("Use compile-time USE_ENJIN1_BACKEND macro for backend selection")]]
    void switchToEnjin2(SceneStateMachine* newSM) {
        enjin2SM = newSM;
        currentBackend = Backend::ENJIN2;
    }

    /**
     * Get current backend type (deprecated - kept for backward compatibility)
     * @return Current backend being used
     */
    [[deprecated("Use compile-time USE_ENJIN1_BACKEND macro for backend selection")]]
    Backend getBackend() const {
        return currentBackend;
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
