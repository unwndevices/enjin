/**
 * @file animation.hpp
 * @brief Animation component for object animations
 *
 * Manages multiple animation tracks for position, scale, rotation,
 * color, and other properties with signal-based updates.
 */
#pragma once

#include "../core/component.hpp"
#include "../animation/animation_track.hpp"
#include "../components/position.hpp"
#include "../components/drawable.hpp"
#include <array>
#include <memory>

namespace enjin2 {

/**
 * @brief Animation component for object animations
 * 
 * Manages multiple animation tracks for different properties
 * (position, scale, rotation, color, etc.) and applies them
 * to the owning object's components.
 */
class C_Animation : public Component {
private:
    static constexpr size_t MAX_TRACKS = 8;  ///< Maximum animation tracks
    
    // Animation tracks for different properties
    PositionTrack positionTrack;
    FloatTrack scaleTrack;
    FloatTrack rotationTrack;
    ColorTrack colorTrack;
    
    // Track usage flags
    bool hasPositionTrack;
    bool hasScaleTrack;
    bool hasRotationTrack;
    bool hasColorTrack;
    
    // Animation state
    bool autoStart;         ///< Auto-start animations when component starts
    bool updatePosition;    ///< Update position component from animation
    bool updateColor;       ///< Update drawable color from animation
    
    // Signal connections for animation updates
    std::unique_ptr<SignalConnection<Point>> positionConnection;
    std::unique_ptr<SignalConnection<float>> scaleConnection;
    std::unique_ptr<SignalConnection<float>> rotationConnection;
    std::unique_ptr<SignalConnection<Pixel4>> colorConnection;
    
public:
    /**
     * @brief Constructor
     * @param owner Owner object
     */
    C_Animation(Object* owner) 
        : Component(owner), hasPositionTrack(false), hasScaleTrack(false),
          hasRotationTrack(false), hasColorTrack(false), autoStart(true),
          updatePosition(true), updateColor(true) {}
    
    /**
     * @brief Destructor
     */
    virtual ~C_Animation() = default;
    
    /**
     * @brief Component start - set up animation connections
     */
    void start() override {
        Component::start();
        
        // Connect position track to position component
        if (hasPositionTrack && updatePosition) {
            positionConnection = std::make_unique<SignalConnection<Point>>(
                positionTrack.connectOnUpdate([this](Point pos) {
                    auto position = owner->getComponent<C_Position>();
                    if (position) {
                        position->setPosition(pos.x, pos.y);
                    }
                })
            );
        }
        
        // Connect color track to drawable component
        if (hasColorTrack && updateColor) {
            colorConnection = std::make_unique<SignalConnection<Pixel4>>(
                colorTrack.connectOnUpdate([this](Pixel4 color) {
                    // Find first drawable component and update its color if possible
                    auto drawable = owner->getComponent<C_Drawable>();
                    if (drawable) {
                        // Try to cast to RectangleDrawable or other color-supporting drawable
                        // This would need to be extended for specific drawable types
                    }
                })
            );
        }
        
        // Auto-start animations if enabled
        if (autoStart) {
            playAll();
        }
    }
    
    /**
     * @brief Component update - update all active tracks
     * @param dt Time elapsed since last update in seconds
     */
    void update(float dt) override {
        Component::update(dt);

        if (hasPositionTrack) {
            positionTrack.update(dt);
        }

        if (hasScaleTrack) {
            scaleTrack.update(dt);
        }

        if (hasRotationTrack) {
            rotationTrack.update(dt);
        }

        if (hasColorTrack) {
            colorTrack.update(dt);
        }
    }
    
    /**
     * @brief Add position keyframe
     * @param time Time in seconds
     * @param position Position at keyframe
     * @param easing Easing function to next keyframe
     * @return True if keyframe was added
     */
    bool addPositionKeyframe(float time, Point position, EaseType easing = EaseType::LINEAR) {
        hasPositionTrack = true;
        return positionTrack.addKeyframe(PositionKeyframe(time, position, easing));
    }

    /**
     * @brief Add scale keyframe
     * @param time Time in seconds
     * @param scale Scale factor at keyframe
     * @param easing Easing function to next keyframe
     * @return True if keyframe was added
     */
    bool addScaleKeyframe(float time, float scale, EaseType easing = EaseType::LINEAR) {
        hasScaleTrack = true;
        return scaleTrack.addKeyframe(FloatKeyframe(time, scale, easing));
    }

    /**
     * @brief Add rotation keyframe
     * @param time Time in seconds
     * @param rotation Rotation in radians at keyframe
     * @param easing Easing function to next keyframe
     * @return True if keyframe was added
     */
    bool addRotationKeyframe(float time, float rotation, EaseType easing = EaseType::LINEAR) {
        hasRotationTrack = true;
        return rotationTrack.addKeyframe(FloatKeyframe(time, rotation, easing));
    }

    /**
     * @brief Add color keyframe
     * @param time Time in seconds
     * @param color Color at keyframe
     * @param easing Easing function to next keyframe
     * @return True if keyframe was added
     */
    bool addColorKeyframe(float time, Pixel4 color, EaseType easing = EaseType::LINEAR) {
        hasColorTrack = true;
        return colorTrack.addKeyframe(ColorKeyframe(time, color, easing));
    }
    
    /**
     * @brief Create orbital animation around a center point
     * @param center Center of orbit
     * @param radius Orbit radius
     * @param duration Animation duration in seconds
     * @param startAngle Starting angle in radians
     * @param clockwise True for clockwise rotation
     * @return True if animation was created
     */
    bool createOrbitAnimation(Point center, float radius, float duration,
                              float startAngle = 0.0f, bool clockwise = true) {
        hasPositionTrack = true;
        positionTrack.clearKeyframes();

        // Create circular motion with multiple keyframes for smooth orbit
        constexpr int steps = 8;
        for (int i = 0; i <= steps; ++i) {
            float progress = static_cast<float>(i) / static_cast<float>(steps);
            float time = progress * duration;

            float angle = startAngle + (clockwise ? 1.0f : -1.0f) * progress * 2.0f * 3.14159f;
            Point position(
                static_cast<int16_t>(center.x + radius * std::cos(angle)),
                static_cast<int16_t>(center.y + radius * std::sin(angle))
            );

            positionTrack.addKeyframe(PositionKeyframe(time, position, EaseType::LINEAR));
        }

        positionTrack.setLoopMode(LoopMode::LOOP);
        return true;
    }
    
    /**
     * @brief Create pulsing scale animation
     * @param minScale Minimum scale factor
     * @param maxScale Maximum scale factor
     * @param duration Pulse duration in seconds
     * @return True if animation was created
     */
    bool createPulseAnimation(float minScale, float maxScale, float duration) {
        hasScaleTrack = true;
        scaleTrack.clearKeyframes();

        scaleTrack.addKeyframe(FloatKeyframe(0.0f, minScale, EaseType::EASE_IN_OUT));
        scaleTrack.addKeyframe(FloatKeyframe(duration / 2.0f, maxScale, EaseType::EASE_IN_OUT));
        scaleTrack.addKeyframe(FloatKeyframe(duration, minScale, EaseType::EASE_IN_OUT));

        scaleTrack.setLoopMode(LoopMode::LOOP);
        return true;
    }
    
    /**
     * @brief Create color fade animation
     * @param fromColor Starting color
     * @param toColor Ending color
     * @param duration Animation duration in seconds
     * @param pingPong True to fade back and forth
     * @return True if animation was created
     */
    bool createFadeAnimation(Pixel4 fromColor, Pixel4 toColor, float duration, bool pingPong = false) {
        hasColorTrack = true;
        colorTrack.clearKeyframes();

        colorTrack.addKeyframe(ColorKeyframe(0.0f, fromColor, EaseType::EASE_IN_OUT));
        colorTrack.addKeyframe(ColorKeyframe(duration, toColor, EaseType::EASE_IN_OUT));

        colorTrack.setLoopMode(pingPong ? LoopMode::PING_PONG : LoopMode::NONE);
        return true;
    }
    
    /**
     * @brief Play all animations
     */
    void playAll() {
        if (hasPositionTrack) positionTrack.play();
        if (hasScaleTrack) scaleTrack.play();
        if (hasRotationTrack) rotationTrack.play();
        if (hasColorTrack) colorTrack.play();
    }
    
    /**
     * @brief Pause all animations
     */
    void pauseAll() {
        if (hasPositionTrack) positionTrack.pause();
        if (hasScaleTrack) scaleTrack.pause();
        if (hasRotationTrack) rotationTrack.pause();
        if (hasColorTrack) colorTrack.pause();
    }
    
    /**
     * @brief Stop all animations
     */
    void stopAll() {
        if (hasPositionTrack) positionTrack.stop();
        if (hasScaleTrack) scaleTrack.stop();
        if (hasRotationTrack) rotationTrack.stop();
        if (hasColorTrack) colorTrack.stop();
    }
    
    /**
     * @brief Get position track
     * @return Reference to position animation track
     */
    PositionTrack& getPositionTrack() { return positionTrack; }

    /**
     * @brief Get scale track
     * @return Reference to scale animation track
     */
    FloatTrack& getScaleTrack() { return scaleTrack; }

    /**
     * @brief Get rotation track
     * @return Reference to rotation animation track
     */
    FloatTrack& getRotationTrack() { return rotationTrack; }

    /**
     * @brief Get color track
     * @return Reference to color animation track
     */
    ColorTrack& getColorTrack() { return colorTrack; }

    /**
     * @brief Set auto-start behavior
     * @param autoStart True to auto-start animations on component start
     */
    void setAutoStart(bool autoStart) {
        this->autoStart = autoStart;
    }
    
    /**
     * @brief Enable/disable position updates
     * @param update True to update position component from animation
     */
    void setUpdatePosition(bool update) {
        updatePosition = update;
    }
    
    /**
     * @brief Enable/disable color updates
     * @param update True to update drawable color from animation
     */
    void setUpdateColor(bool update) {
        updateColor = update;
    }
};

} // namespace enjin2