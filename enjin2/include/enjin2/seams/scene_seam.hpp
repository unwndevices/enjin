#pragma once

#include <enjin2/core/scene.hpp>
#include <enjin2/core/scene_state_machine.hpp>

namespace enjin2 {

/**
 * SceneSeam - Strangler Fig pattern seam for scene boundaries
 *
 * Allows enjin1 and enjin2 implementations to coexist during incremental migration,
 * with runtime switching capability between legacy and new scene management.
 */
class SceneSeam {
public:
    /// Backend type selector
    enum class Backend {
        ENJIN1,  ///< Use enjin1 legacy backend
        ENJIN2   ///< Use enjin2 new backend
    };

private:
    Backend currentBackend;            ///< Current backend type
    SceneStateMachine* enjin2SM;        ///< Pointer to enjin2 scene state machine
    void* enjin1SM;                     ///< Pointer to enjin1 scene manager (opaque)

public:
    /**
     * Construct scene seam with specified backend type
     * @param backend Which backend to use
     */
    explicit SceneSeam(Backend backend)
        : currentBackend(backend), enjin2SM(nullptr), enjin1SM(nullptr) {}

    /**
     * Initialize the scene system
     * Routes to appropriate implementation based on current backend
     */
    void initialize() {
        if (currentBackend == Backend::ENJIN2 && enjin2SM != nullptr) {
            enjin2SM->initialize();
        } else if (currentBackend == Backend::ENJIN1) {
            // TODO: Route to legacy implementation when enjin1 integration is ready
            // This is intentionally stubbed - legacy routing will be implemented
            // when enjin1 headers are integrated
        }
    }

    /**
     * Update scene state each frame
     * @param deltaTime Time since last frame (in milliseconds)
     */
    void update(uint16_t deltaTime) {
        if (currentBackend == Backend::ENJIN2 && enjin2SM != nullptr) {
            enjin2SM->update(deltaTime);
        } else if (currentBackend == Backend::ENJIN1) {
            // TODO: Route to legacy implementation when enjin1 integration is ready
        }
    }

    /**
     * Switch to enjin2 backend
     * @param newSM Pointer to enjin2 scene state machine to use
     */
    void switchToEnjin2(SceneStateMachine* newSM) {
        enjin2SM = newSM;
        currentBackend = Backend::ENJIN2;
    }

    /**
     * Render the current scene to a canvas
     * @tparam PixelType Pixel type for the canvas (e.g., uint8_t for 8-bit color)
     * @param canvas Canvas to render to
     */
    template<typename PixelType>
    void render(ICanvas<PixelType>& canvas) {
        if (currentBackend == Backend::ENJIN2 && enjin2SM != nullptr) {
            enjin2SM->render(canvas);
        } else if (currentBackend == Backend::ENJIN1) {
            // TODO: Route to legacy implementation when enjin1 integration is ready
        }
    }

    /**
     * Get current backend type
     * @return Current backend being used
     */
    Backend getBackend() const {
        return currentBackend;
    }
};

} // namespace enjin2
