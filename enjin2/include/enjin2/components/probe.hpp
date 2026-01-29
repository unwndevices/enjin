#pragma once

#include "../components/drawable.hpp"
#include "../core/types.hpp"
#include <cmath>

namespace enjin2 {

/**
 * @brief Probe component for dynamic indicators and effects
 * 
 * Represents small probes, particles, or indicators that can
 * move freely in space, show status information, or create
 * visual effects like particle trails, scanners, or data flows.
 */
class C_Probe : public C_Drawable {
public:
    /**
     * @brief Probe types for different visual styles
     */
    enum class ProbeType {
        DOT,            ///< Simple dot
        DIAMOND,        ///< Diamond shape
        CROSS,          ///< Cross/plus shape
        TRIANGLE,       ///< Triangle
        SCANNER,        ///< Scanning beam
        PARTICLE        ///< Particle with tail
    };
    
    /**
     * @brief Movement patterns for autonomous probes
     */
    enum class MovementPattern {
        STATIC,         ///< No movement
        LINEAR,         ///< Linear movement with velocity
        ORBITAL,        ///< Circular orbital motion
        SPIRAL,         ///< Spiral motion
        RANDOM_WALK,    ///< Random walk movement
        SINE_WAVE,      ///< Sine wave pattern
        TRIANGLE_WAVE   ///< Triangle wave pattern
    };

private:
    ProbeType probeType;        ///< Visual style of probe
    MovementPattern movement;   ///< Movement pattern
    
    // Appearance
    float probeSize;            ///< Size of probe
    Pixel4 probeColor;          ///< Primary color
    Pixel4 accentColor;         ///< Secondary/accent color
    bool pulsing;               ///< Enable pulsing effect
    float pulseSpeed;           ///< Pulse frequency
    
    // Movement state
    Point velocity;             ///< Current velocity (for linear movement)
    Point homePosition;         ///< Base position for patterns
    float movementSpeed;        ///< Movement speed multiplier
    float phase;                ///< Phase for wave patterns
    uint32_t animationTime;     ///< Animation timer
    
    // Scanner-specific
    float scanAngle;            ///< Current scan angle (for scanner type)
    float scanRadius;           ///< Scan radius
    float scanSpeed;            ///< Scan rotation speed
    
    // Particle trail
    static constexpr size_t TRAIL_LENGTH = 8;
    Point trailPoints[TRAIL_LENGTH];
    size_t trailIndex;
    bool hasTrail;
    
    // Bounds for movement
    Rect movementBounds;
    bool constrainToBounds;
    
public:
    /**
     * @brief Constructor
     * @param owner Owner object
     * @param type Probe visual type
     * @param size Probe size
     */
    C_Probe(Object* owner, ProbeType type = ProbeType::DOT, float size = 2.0f)
        : C_Drawable(owner, static_cast<uint16_t>(size * 4), static_cast<uint16_t>(size * 4)),
          probeType(type), movement(MovementPattern::STATIC), probeSize(size),
          probeColor(15), accentColor(10), pulsing(false), pulseSpeed(2.0f),
          velocity(0, 0), homePosition(0, 0), movementSpeed(1.0f), phase(0.0f),
          animationTime(0), scanAngle(0.0f), scanRadius(10.0f), scanSpeed(1.0f),
          trailIndex(0), hasTrail(false), movementBounds(0, 0, 0, 0),
          constrainToBounds(false) {
        
        setDrawLayer(DrawLayer::FOREGROUND);
        
        // Initialize trail
        for (size_t i = 0; i < TRAIL_LENGTH; ++i) {
            trailPoints[i] = Point(0, 0);
        }
    }
    
    /**
     * @brief Update probe movement and animation
     */
    void update(uint16_t deltaTime) override {
        Component::update(deltaTime);
        
        animationTime += deltaTime;
        float deltaSeconds = deltaTime / 1000.0f;
        
        // Update position based on movement pattern
        updateMovement(deltaSeconds);
        
        // Update scanner angle
        if (probeType == ProbeType::SCANNER) {
            scanAngle += scanSpeed * deltaSeconds * 2.0f * 3.14159f;
            if (scanAngle >= 2.0f * 3.14159f) {
                scanAngle -= 2.0f * 3.14159f;
            }
        }
        
        // Update trail
        if (hasTrail) {
            Point center = getRenderPosition();
            center.x += getWidth() / 2;
            center.y += getHeight() / 2;
            updateTrail(center);
        }
    }
    
    /**
     * @brief Draw probe to 4-bit canvas
     */
    void draw(ICanvas<Pixel4>& canvas) override {
        if (!isVisible()) return;
        
        Point center = getRenderPosition();
        center.x += getWidth() / 2;
        center.y += getHeight() / 2;
        
        // Draw trail first (behind probe)
        if (hasTrail) {
            drawTrail(canvas);
        }
        
        // Calculate current size and color
        float currentSize = probeSize;
        Pixel4 currentColor = probeColor;
        
        if (pulsing) {
            float pulse = std::sin((animationTime / 1000.0f) * pulseSpeed * 2.0f * 3.14159f);
            currentSize *= (1.0f + pulse * 0.4f);
            if (pulse > 0) {
                currentColor = accentColor;
            }
        }
        
        // Draw based on probe type
        switch (probeType) {
            case ProbeType::DOT:
                drawDot(canvas, center, currentSize, currentColor);
                break;
                
            case ProbeType::DIAMOND:
                drawDiamond(canvas, center, currentSize, currentColor);
                break;
                
            case ProbeType::CROSS:
                drawCross(canvas, center, currentSize, currentColor);
                break;
                
            case ProbeType::TRIANGLE:
                drawTriangle(canvas, center, currentSize, currentColor);
                break;
                
            case ProbeType::SCANNER:
                drawScanner(canvas, center, currentSize, currentColor);
                break;
                
            case ProbeType::PARTICLE:
                drawParticle(canvas, center, currentSize, currentColor);
                break;
        }
    }
    
    /**
     * @brief Draw probe to 8-bit canvas
     */
    void draw(ICanvas<uint8_t>& canvas) override {
        if (!isVisible()) return;
        
        Point center = getRenderPosition();
        center.x += getWidth() / 2;
        center.y += getHeight() / 2;
        
        if (hasTrail) {
            drawTrail(canvas);
        }
        
        float currentSize = probeSize;
        uint8_t currentColor = probeColor.to8bit();
        
        if (pulsing) {
            float pulse = std::sin((animationTime / 1000.0f) * pulseSpeed * 2.0f * 3.14159f);
            currentSize *= (1.0f + pulse * 0.4f);
            if (pulse > 0) {
                currentColor = accentColor.to8bit();
            }
        }
        
        switch (probeType) {
            case ProbeType::DOT:
                drawDot(canvas, center, currentSize, currentColor);
                break;
            case ProbeType::DIAMOND:
                drawDiamond(canvas, center, currentSize, currentColor);
                break;
            case ProbeType::CROSS:
                drawCross(canvas, center, currentSize, currentColor);
                break;
            case ProbeType::TRIANGLE:
                drawTriangle(canvas, center, currentSize, currentColor);
                break;
            case ProbeType::SCANNER:
                drawScanner(canvas, center, currentSize, currentColor);
                break;
            case ProbeType::PARTICLE:
                drawParticle(canvas, center, currentSize, currentColor);
                break;
        }
    }
    
    /**
     * @brief Set probe appearance
     */
    void setAppearance(ProbeType type, float size, Pixel4 color, Pixel4 accent = Pixel4(10)) {
        probeType = type;
        probeSize = size;
        probeColor = color;
        accentColor = accent;
        
        // Update drawable size
        setSize(static_cast<uint16_t>(size * 4), static_cast<uint16_t>(size * 4));
    }
    
    /**
     * @brief Set movement pattern
     */
    void setMovement(MovementPattern pattern, float speed = 1.0f) {
        movement = pattern;
        movementSpeed = speed;
        
        // Set home position to current position
        auto pos = owner->getPosition();
        if (pos) {
            homePosition = pos->getPosition();
        }
    }
    
    /**
     * @brief Set linear velocity (for LINEAR movement)
     */
    void setVelocity(Point vel) {
        velocity = vel;
    }
    
    /**
     * @brief Enable pulsing effect
     */
    void setPulsing(bool enabled, float speed = 2.0f) {
        pulsing = enabled;
        pulseSpeed = speed;
    }
    
    /**
     * @brief Enable particle trail
     */
    void setTrail(bool enabled) {
        hasTrail = enabled;
    }
    
    /**
     * @brief Set movement bounds
     */
    void setMovementBounds(const Rect& bounds, bool constrain = true) {
        movementBounds = bounds;
        constrainToBounds = constrain;
    }
    
    /**
     * @brief Set scanner properties (for SCANNER type)
     */
    void setScanner(float radius, float speed = 1.0f) {
        scanRadius = radius;
        scanSpeed = speed;
    }

private:
    /**
     * @brief Update movement based on pattern
     */
    void updateMovement(float deltaTime) {
        auto position = owner->getPosition();
        if (!position) return;
        
        Point currentPos = position->getPosition();
        Point newPos = currentPos;
        
        switch (movement) {
            case MovementPattern::STATIC:
                // No movement
                break;
                
            case MovementPattern::LINEAR:
                newPos.x += static_cast<int16_t>(velocity.x * deltaTime * movementSpeed);
                newPos.y += static_cast<int16_t>(velocity.y * deltaTime * movementSpeed);
                break;
                
            case MovementPattern::ORBITAL: {
                phase += deltaTime * movementSpeed;
                float radius = 20.0f;
                newPos.x = static_cast<int16_t>(homePosition.x + radius * std::cos(phase));
                newPos.y = static_cast<int16_t>(homePosition.y + radius * std::sin(phase));
                break;
            }
            
            case MovementPattern::SPIRAL: {
                phase += deltaTime * movementSpeed;
                float radius = 5.0f + phase * 2.0f;
                newPos.x = static_cast<int16_t>(homePosition.x + radius * std::cos(phase));
                newPos.y = static_cast<int16_t>(homePosition.y + radius * std::sin(phase));
                break;
            }
            
            case MovementPattern::RANDOM_WALK: {
                // Simple random walk
                static int randomSeed = 12345;
                randomSeed = (randomSeed * 1103515245 + 12345) & 0x7fffffff;
                float randomX = (randomSeed % 1000 - 500) / 500.0f;
                randomSeed = (randomSeed * 1103515245 + 12345) & 0x7fffffff;
                float randomY = (randomSeed % 1000 - 500) / 500.0f;
                
                newPos.x += static_cast<int16_t>(randomX * deltaTime * movementSpeed * 10.0f);
                newPos.y += static_cast<int16_t>(randomY * deltaTime * movementSpeed * 10.0f);
                break;
            }
            
            case MovementPattern::SINE_WAVE: {
                phase += deltaTime * movementSpeed;
                newPos.x = static_cast<int16_t>(homePosition.x + phase * 20.0f);
                newPos.y = static_cast<int16_t>(homePosition.y + std::sin(phase) * 15.0f);
                break;
            }
            
            case MovementPattern::TRIANGLE_WAVE: {
                phase += deltaTime * movementSpeed;
                float triangleWave = 2.0f * std::abs(2.0f * (phase / (2.0f * 3.14159f) - std::floor(phase / (2.0f * 3.14159f) + 0.5f))) - 1.0f;
                newPos.x = static_cast<int16_t>(homePosition.x + phase * 20.0f);
                newPos.y = static_cast<int16_t>(homePosition.y + triangleWave * 15.0f);
                break;
            }
        }
        
        // Apply bounds constraint
        if (constrainToBounds) {
            int16_t maxX = movementBounds.x + static_cast<int16_t>(movementBounds.width);
            int16_t maxY = movementBounds.y + static_cast<int16_t>(movementBounds.height);
            newPos.x = std::max(movementBounds.x, std::min(newPos.x, maxX));
            newPos.y = std::max(movementBounds.y, std::min(newPos.y, maxY));
        }
        
        position->setPosition(newPos.x, newPos.y);
    }
    
    /**
     * @brief Update trail points
     */
    void updateTrail(Point newPoint) {
        trailPoints[trailIndex] = newPoint;
        trailIndex = (trailIndex + 1) % TRAIL_LENGTH;
    }
    
    /**
     * @brief Draw trail
     */
    template<typename PixelType>
    void drawTrail(ICanvas<PixelType>& canvas) {
        for (size_t i = 0; i < TRAIL_LENGTH; ++i) {
            size_t index = (trailIndex + i) % TRAIL_LENGTH;
            float alpha = static_cast<float>(i) / static_cast<float>(TRAIL_LENGTH);
            
            if constexpr (std::is_same_v<PixelType, Pixel4>) {
                Pixel4 color(static_cast<uint8_t>(accentColor.value * alpha));
                canvas.setPixel(trailPoints[index].x, trailPoints[index].y, color);
            } else {
                uint8_t color = static_cast<uint8_t>(accentColor.to8bit() * alpha);
                canvas.setPixel(trailPoints[index].x, trailPoints[index].y, color);
            }
        }
    }
    
    /**
     * @brief Draw different probe shapes
     */
    template<typename PixelType>
    void drawDot(ICanvas<PixelType>& canvas, Point center, float size, PixelType color) {
        int radius = static_cast<int>(size);
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x * x + y * y <= radius * radius) {
                    canvas.setPixel(center.x + x, center.y + y, color);
                }
            }
        }
    }
    
    template<typename PixelType>
    void drawDiamond(ICanvas<PixelType>& canvas, Point center, float size, PixelType color) {
        int s = static_cast<int>(size);
        for (int y = -s; y <= s; ++y) {
            for (int x = -s; x <= s; ++x) {
                if (abs(x) + abs(y) <= s) {
                    canvas.setPixel(center.x + x, center.y + y, color);
                }
            }
        }
    }
    
    template<typename PixelType>
    void drawCross(ICanvas<PixelType>& canvas, Point center, float size, PixelType color) {
        int s = static_cast<int>(size);
        // Horizontal line
        for (int x = -s; x <= s; ++x) {
            canvas.setPixel(center.x + x, center.y, color);
        }
        // Vertical line
        for (int y = -s; y <= s; ++y) {
            canvas.setPixel(center.x, center.y + y, color);
        }
    }
    
    template<typename PixelType>
    void drawTriangle(ICanvas<PixelType>& canvas, Point center, float size, PixelType color) {
        int s = static_cast<int>(size);
        for (int y = 0; y <= s; ++y) {
            int width = s - y;
            for (int x = -width; x <= width; ++x) {
                canvas.setPixel(center.x + x, center.y - s + y, color);
            }
        }
    }
    
    template<typename PixelType>
    void drawScanner(ICanvas<PixelType>& canvas, Point center, float size, PixelType color) {
        // Draw scanning beam
        int beamLength = static_cast<int>(scanRadius);
        int endX = static_cast<int>(center.x + beamLength * std::cos(scanAngle));
        int endY = static_cast<int>(center.y + beamLength * std::sin(scanAngle));
        
        drawLine(canvas, center, Point(endX, endY), color);
        
        // Draw center dot
        drawDot(canvas, center, size * 0.5f, color);
    }
    
    template<typename PixelType>
    void drawParticle(ICanvas<PixelType>& canvas, Point center, float size, PixelType color) {
        // Bright center with dimmer halo
        drawDot(canvas, center, size * 0.6f, color);
        
        PixelType dimColor;
        if constexpr (std::is_same_v<PixelType, Pixel4>) {
            dimColor = Pixel4(color.value / 2);
        } else {
            dimColor = static_cast<PixelType>(color / 2);
        }
        drawDot(canvas, center, size, dimColor);
    }
    
    template<typename PixelType>
    void drawLine(ICanvas<PixelType>& canvas, Point from, Point to, PixelType color) {
        int dx = abs(to.x - from.x);
        int dy = abs(to.y - from.y);
        int sx = from.x < to.x ? 1 : -1;
        int sy = from.y < to.y ? 1 : -1;
        int err = dx - dy;
        
        int x = from.x;
        int y = from.y;
        
        while (true) {
            canvas.setPixel(x, y, color);
            if (x == to.x && y == to.y) break;
            
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    }
};

} // namespace enjin2