/**
 * @file camera.cpp
 * @brief C_Camera component implementation
 *
 * Phase 44: CAM-01..CAM-06
 */
#include "enjin2/components/camera.hpp"
#include <algorithm>
#include <cmath>

namespace enjin2 {

C_Camera::C_Camera(Object* owner)
    : Component(owner)
    // All other members initialised via in-class defaults in the header
{}

void C_Camera::setPosition(float x, float y) {
    m_pos.x = x;
    m_pos.y = y;
    m_target.x = x;
    m_target.y = y;
    m_lerpSpeed = 1.0f;  // Reset to instant — no lerp residual
    clampPosition();
}

void C_Camera::lookAt(float x, float y, float lerpSpeed) {
    m_target.x = x;
    m_target.y = y;
    m_lerpSpeed = lerpSpeed;
    if (lerpSpeed >= 1.0f) {
        // Snap immediately
        m_pos.x = m_target.x;
        m_pos.y = m_target.y;
        clampPosition();
    }
}

void C_Camera::shake(float intensity, float duration) {
    m_shakeIntensity = intensity;
    m_shakeDuration  = duration;
    m_shakeElapsed   = 0.f;
}

void C_Camera::update(float dt) {
    // --- Lerp position toward target ---
    // Scale factor: lerpSpeed * 10 gives visually smooth tracking for typical
    // values (lerpSpeed=0.1 → factor=1.0 per second at dt=1.0).
    float factor = m_lerpSpeed * dt * 10.0f;
    if (factor > 1.0f) factor = 1.0f;

    m_pos.x += (m_target.x - m_pos.x) * factor;
    m_pos.y += (m_target.y - m_pos.y) * factor;

    clampPosition();

    // --- Shake update ---
    if (m_shakeElapsed < m_shakeDuration) {
        m_shakeElapsed += dt;
        // Clamp elapsed to duration for final frame
        float elapsed = m_shakeElapsed < m_shakeDuration ? m_shakeElapsed : m_shakeDuration;
        float progress = elapsed / m_shakeDuration;
        float decay = 1.0f - progress;
        m_shakeOffset.x = std::sin(elapsed * 40.0f) * m_shakeIntensity * decay;
        m_shakeOffset.y = std::sin(elapsed * 30.0f) * m_shakeIntensity * decay * 0.7f;
    } else {
        m_shakeOffset.x = 0.f;
        m_shakeOffset.y = 0.f;
    }
}

Point C_Camera::getScreenOffset() const {
    return Point(
        static_cast<int16_t>(-(m_pos.x + m_shakeOffset.x)),
        static_cast<int16_t>(-(m_pos.y + m_shakeOffset.y))
    );
}

void C_Camera::setBounds(float minX, float minY, float maxX, float maxY) {
    m_boundsMinX = minX;
    m_boundsMinY = minY;
    m_boundsMaxX = maxX;
    m_boundsMaxY = maxY;
    m_hasBounds  = true;
    clampPosition();
}

void C_Camera::clearBounds() {
    m_hasBounds = false;
}

void C_Camera::clampPosition() {
    if (!m_hasBounds) return;
    m_pos.x = std::max(m_boundsMinX, std::min(m_pos.x, m_boundsMaxX));
    m_pos.y = std::max(m_boundsMinY, std::min(m_pos.y, m_boundsMaxY));
}

} // namespace enjin2
