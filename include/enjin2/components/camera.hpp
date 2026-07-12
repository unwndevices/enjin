/**
 * @file camera.hpp
 * @brief 2D Camera component for viewport control
 *
 * Provides smooth follow (lerp), screen shake (sin oscillation with decay),
 * and viewport bounds clamping. The camera position is the world point
 * at the top-left of the screen. Screen offset = -(camera pos + shake offset).
 *
 * Phase 44: CAM-01..CAM-06
 */
#pragma once
#include "../core/component.hpp"
#include "../core/types.hpp"
#include <cmath>
#include <algorithm>

namespace enjin2 {

/**
 * @brief 2D camera component with smooth follow, shake, and bounds clamping.
 *
 * Camera position convention: the camera position is the world-space point
 * that appears at the top-left (0,0) of the screen. Therefore:
 *   screen_pos = world_pos - camera_pos
 *   screen_offset = -camera_pos
 *
 * Usage:
 *   1. Add C_Camera to an object in the scene (first active C_Camera is used).
 *   2. Call setPosition() or lookAt() to control the viewport.
 *   3. Scene::renderObjects() picks up getScreenOffset() and applies it via
 *      C_Drawable::drawWithOffset() for all world-space drawables.
 */
class C_Camera : public Component {
public:
    /**
     * @brief Construct a new camera with default position (0,0)
     * @param owner Owning Object (required by Component)
     */
    explicit C_Camera(Object* owner);

    ~C_Camera() override = default;

    /**
     * @brief Update lerp and shake each frame
     * @param dt Delta time in seconds
     */
    void update(float dt) override;

    // ----------------------------------------------------------------
    // Position control
    // ----------------------------------------------------------------

    /**
     * @brief Teleport camera to (x, y) with no lerp residual
     * @param x World X coordinate of top-left screen corner
     * @param y World Y coordinate of top-left screen corner
     */
    void setPosition(float x, float y);

    /**
     * @brief Get current camera position (float precision)
     * @return Vec2 camera position
     */
    Vec2 getPosition() const { return m_pos; }

    /**
     * @brief Set follow target and lerp speed
     *
     * If lerpSpeed >= 1.0, position snaps to target immediately (no lerp).
     * lerpSpeed < 1.0 provides smooth follow; smaller = smoother but slower.
     *
     * @param x Target world X
     * @param y Target world Y
     * @param lerpSpeed Lerp coefficient (0 = no movement, 1.0 = snap)
     */
    void lookAt(float x, float y, float lerpSpeed = 1.0f);

    // ----------------------------------------------------------------
    // Shake
    // ----------------------------------------------------------------

    /**
     * @brief Apply screen shake with sin oscillation and linear decay
     * @param intensity Shake amplitude in pixels
     * @param duration Shake duration in seconds
     */
    void shake(float intensity, float duration);

    // ----------------------------------------------------------------
    // Screen offset (for render pipeline)
    // ----------------------------------------------------------------

    /**
     * @brief Compute integer screen offset: -(position + shakeOffset)
     *
     * Returns the pixel offset that Scene::renderObjects() adds to each
     * world-space drawable via C_Drawable::drawWithOffset().
     *
     * @return Point offset (negative of camera position + shake)
     */
    Point getScreenOffset() const;

    // ----------------------------------------------------------------
    // Viewport clamping
    // ----------------------------------------------------------------

    /**
     * @brief Set axis-aligned bounds that clamp camera position
     * @param minX Minimum world X for camera left edge
     * @param minY Minimum world Y for camera top edge
     * @param maxX Maximum world X for camera left edge
     * @param maxY Maximum world Y for camera top edge
     */
    void setBounds(float minX, float minY, float maxX, float maxY);

    /**
     * @brief Remove viewport clamping
     */
    void clearBounds();

    /**
     * @brief Check if bounds are currently set
     * @return True if bounds are active
     */
    bool hasBounds() const { return m_hasBounds; }

    // ----------------------------------------------------------------
    // Canvas size (used for offset calculation metadata)
    // ----------------------------------------------------------------

    /**
     * @brief Inject canvas dimensions (defaults to 128x128)
     * @param w Canvas width in pixels
     * @param h Canvas height in pixels
     */
    void setCanvasSize(uint16_t w, uint16_t h) { m_canvasW = w; m_canvasH = h; }

    /// @brief Get canvas width
    /// @return Canvas width in pixels
    uint16_t getCanvasWidth()  const { return m_canvasW; }
    /// @brief Get canvas height
    /// @return Canvas height in pixels
    uint16_t getCanvasHeight() const { return m_canvasH; }

private:
    // Position
    Vec2  m_pos{0.f, 0.f};      ///< Current camera position (float for smooth sub-pixel)
    Vec2  m_target{0.f, 0.f};   ///< Lerp target position
    float m_lerpSpeed{1.0f};    ///< Lerp coefficient (1.0 = instant snap)

    // Shake
    float m_shakeIntensity{0.f};   ///< Shake amplitude in pixels
    float m_shakeDuration{0.f};    ///< Total shake duration in seconds
    float m_shakeElapsed{0.f};     ///< Elapsed shake time in seconds
    Vec2  m_shakeOffset{0.f, 0.f}; ///< Current shake displacement

    // Bounds
    bool  m_hasBounds{false};
    float m_boundsMinX{0.f};
    float m_boundsMinY{0.f};
    float m_boundsMaxX{0.f};
    float m_boundsMaxY{0.f};

    // Canvas dimensions for metadata
    uint16_t m_canvasW{128};
    uint16_t m_canvasH{128};

    /// @brief Clamp m_pos to [min, max] if bounds are active
    void clampPosition();
};

} // namespace enjin2
