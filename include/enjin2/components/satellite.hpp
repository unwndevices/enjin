/**
 * @file satellite.hpp
 * @brief Satellite component for orbiting parameter controls
 *
 * Represents a small satellite that orbits around a planet.
 * Used for audio parameter visualization and control in space-themed UI.
 * Can represent things like filter frequency, resonance, gain, etc.
 */
#pragma once

#include "../components/drawable.hpp"
#include "../components/planet.hpp"
#include "../core/types.hpp"
#include <cmath>

namespace enjin2 {

/**
 * @brief Satellite component for orbiting parameter controls
 * 
 * Represents a small satellite that orbits around a planet.
 * Used for audio parameter visualization and control in the
 * space-themed UI. Can represent things like filter frequency,
 * resonance, gain, etc.
 */
class C_Satellite : public C_Drawable {
private:
    C_Planet* orbitPlanet;      ///< Planet to orbit around (can be nullptr for manual positioning)
    float orbitRadius;          ///< Distance from planet center
    float orbitSpeed;           ///< Orbital speed in radians per second
    float currentAngle;         ///< Current orbital angle
    bool clockwise;             ///< Orbit direction
    
    // Satellite appearance
    float satelliteRadius;      ///< Satellite size
    Pixel4 satelliteColor;      ///< Satellite color
    Pixel4 trailColor;          ///< Orbital trail color
    bool showTrail;             ///< Whether to show orbital trail
    bool showConnection;        ///< Show line to planet
    
    // Parameter binding
    float parameterValue;       ///< Current parameter value (0.0 to 1.0)
    float minValue;             ///< Minimum parameter value
    float maxValue;             ///< Maximum parameter value
    bool radiusFromParameter;   ///< Use parameter to control orbit radius
    bool speedFromParameter;    ///< Use parameter to control orbit speed
    bool colorFromParameter;    ///< Use parameter to control color
    
    // Visual effects
    bool pulsing;               ///< Enable pulsing based on parameter
    uint32_t animationTime;     ///< Animation timer
    
    // Trail rendering (simple circular buffer)
    static constexpr size_t TRAIL_LENGTH = 16;
    Point trailPoints[TRAIL_LENGTH];
    size_t trailIndex;
    bool trailInitialized;
    
public:
    /**
     * @brief Constructor
     * @param owner Owner object
     * @param planet Planet to orbit (can be nullptr)
     * @param radius Orbit radius
     * @param startAngle Initial orbital angle
     */
    C_Satellite(Object* owner, C_Planet* planet = nullptr, float radius = 30.0f, float startAngle = 0.0f)
        : C_Drawable(owner, static_cast<uint16_t>(radius * 2.5f), static_cast<uint16_t>(radius * 2.5f)),
          orbitPlanet(planet), orbitRadius(radius), orbitSpeed(1.0f), currentAngle(startAngle),
          clockwise(true), satelliteRadius(3.0f), satelliteColor(15), trailColor(8),
          showTrail(true), showConnection(false), parameterValue(0.5f),
          minValue(0.0f), maxValue(1.0f), radiusFromParameter(false),
          speedFromParameter(false), colorFromParameter(false), pulsing(true),
          animationTime(0), trailIndex(0), trailInitialized(false) {
        
        setDrawLayer(DrawLayer::ENTITIES);
        
        // Initialize trail
        for (size_t i = 0; i < TRAIL_LENGTH; ++i) {
            trailPoints[i] = Point(0, 0);
        }
    }
    
    /**
     * @brief Update satellite position and animation
     */
    void update(uint16_t deltaTime) override {
        Component::update(deltaTime);
        
        animationTime += deltaTime;
        
        // Update orbital position
        if (orbitPlanet) {
            float currentRadius = orbitRadius;
            float currentSpeed = orbitSpeed;
            
            // Modify orbit based on parameter
            if (radiusFromParameter) {
                float radiusScale = 0.5f + parameterValue;  // 0.5x to 1.5x
                currentRadius *= radiusScale;
            }
            
            if (speedFromParameter) {
                currentSpeed *= (0.2f + parameterValue * 1.8f);  // 0.2x to 2.0x speed
            }
            
            // Update angle
            float deltaAngle = currentSpeed * (deltaTime / 1000.0f);
            if (!clockwise) deltaAngle = -deltaAngle;
            currentAngle += deltaAngle;
            
            // Keep angle in range
            if (currentAngle >= 2.0f * 3.14159f) {
                currentAngle -= 2.0f * 3.14159f;
            } else if (currentAngle < 0.0f) {
                currentAngle += 2.0f * 3.14159f;
            }
            
            // Calculate new position relative to planet
            Point planetCenter = orbitPlanet->getCenterPosition();
            Point newPos(
                static_cast<int16_t>(planetCenter.x + currentRadius * std::cos(currentAngle) - getWidth() / 2),
                static_cast<int16_t>(planetCenter.y + currentRadius * std::sin(currentAngle) - getHeight() / 2)
            );
            
            // Update position component
            auto position = owner->getPosition();
            if (position) {
                position->setPosition(newPos.x, newPos.y);
            }
            
            // Update trail
            updateTrail(Point(
                static_cast<int16_t>(planetCenter.x + currentRadius * std::cos(currentAngle)),
                static_cast<int16_t>(planetCenter.y + currentRadius * std::sin(currentAngle))
            ));
        }
    }
    
    /**
     * @brief Draw satellite to 4-bit canvas
     */
    void draw(ICanvas<Pixel4>& canvas) override {
        if (!isVisible()) return;
        
        Point center = getRenderPosition();
        center.x += getWidth() / 2;
        center.y += getHeight() / 2;
        
        // Draw connection line to planet
        if (showConnection && orbitPlanet) {
            Point planetCenter = orbitPlanet->getCenterPosition();
            drawLine(canvas, planetCenter, center, Pixel4(4));
        }
        
        // Draw orbital trail
        if (showTrail && trailInitialized) {
            drawTrail(canvas);
        }
        
        // Calculate current satellite properties
        float currentRadius = satelliteRadius;
        Pixel4 currentColor = satelliteColor;
        
        // Apply parameter-based effects
        if (pulsing) {
            float pulse = std::sin((animationTime / 1000.0f) * 4.0f * 2.0f * 3.14159f);
            currentRadius *= (1.0f + pulse * 0.3f * parameterValue);
        }
        
        if (colorFromParameter) {
            // Interpolate color based on parameter
            uint8_t intensity = static_cast<uint8_t>(4 + parameterValue * 11);  // 4 to 15
            currentColor = Pixel4(intensity);
        }
        
        // Draw satellite body
        drawCircle(canvas, center, currentRadius, currentColor, true);
        
        // Draw satellite highlight
        Point highlight(center.x - 1, center.y - 1);
        drawCircle(canvas, highlight, currentRadius * 0.4f, Pixel4(15), true);
    }
    
    /**
     * @brief Draw satellite to 8-bit canvas
     */
    void draw(ICanvas<uint8_t>& canvas) override {
        if (!isVisible()) return;
        
        Point center = getRenderPosition();
        center.x += getWidth() / 2;
        center.y += getHeight() / 2;
        
        if (showConnection && orbitPlanet) {
            Point planetCenter = orbitPlanet->getCenterPosition();
            drawLine(canvas, planetCenter, center, static_cast<uint8_t>(64));
        }
        
        if (showTrail && trailInitialized) {
            drawTrail(canvas);
        }
        
        float currentRadius = satelliteRadius;
        uint8_t currentColor = satelliteColor.to8bit();
        
        if (pulsing) {
            float pulse = std::sin((animationTime / 1000.0f) * 4.0f * 2.0f * 3.14159f);
            currentRadius *= (1.0f + pulse * 0.3f * parameterValue);
        }
        
        if (colorFromParameter) {
            currentColor = static_cast<uint8_t>(64 + parameterValue * 191);  // 64 to 255
        }
        
        drawCircle(canvas, center, currentRadius, currentColor, true);
        
        Point highlight(center.x - 1, center.y - 1);
        drawCircle(canvas, highlight, currentRadius * 0.4f, static_cast<uint8_t>(255), true);
    }
    
    /**
     * @brief Set parameter value (0.0 to 1.0)
     */
    void setParameterValue(float value) {
        parameterValue = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    }
    
    /**
     * @brief Get parameter value
     */
    float getParameterValue() const {
        return parameterValue;
    }
    
    /**
     * @brief Set parameter range
     */
    void setParameterRange(float min, float max) {
        minValue = min;
        maxValue = max;
    }
    
    /**
     * @brief Get scaled parameter value in range
     */
    float getScaledValue() const {
        return minValue + parameterValue * (maxValue - minValue);
    }
    
    /**
     * @brief Set from scaled value
     */
    void setScaledValue(float scaledValue) {
        if (maxValue != minValue) {
            setParameterValue((scaledValue - minValue) / (maxValue - minValue));
        }
    }
    
    /**
     * @brief Set orbital properties
     */
    void setOrbit(float radius, float speed, bool clockwiseDirection = true) {
        orbitRadius = radius;
        orbitSpeed = speed;
        clockwise = clockwiseDirection;
    }
    
    /**
     * @brief Set visual properties
     */
    void setAppearance(float radius, Pixel4 color, bool showOrbitalTrail = true) {
        satelliteRadius = radius;
        satelliteColor = color;
        showTrail = showOrbitalTrail;
    }
    
    /**
     * @brief Enable parameter-based effects
     */
    void setParameterEffects(bool radiusEffect, bool speedEffect, bool colorEffect) {
        radiusFromParameter = radiusEffect;
        speedFromParameter = speedEffect;
        colorFromParameter = colorEffect;
    }
    
    /**
     * @brief Set trail appearance
     */
    void setTrail(bool enabled, Pixel4 color = Pixel4(8)) {
        showTrail = enabled;
        trailColor = color;
    }
    
    /**
     * @brief Set connection line to planet
     */
    void setConnection(bool enabled) {
        showConnection = enabled;
    }
    
    /**
     * @brief Get current orbital angle
     */
    float getAngle() const {
        return currentAngle;
    }
    
    /**
     * @brief Set orbital angle
     */
    void setAngle(float angle) {
        currentAngle = angle;
    }

private:
    /**
     * @brief Update trail points
     */
    void updateTrail(Point newPoint) {
        trailPoints[trailIndex] = newPoint;
        trailIndex = (trailIndex + 1) % TRAIL_LENGTH;
        if (!trailInitialized && trailIndex == 0) {
            trailInitialized = true;
        }
    }
    
    /**
     * @brief Draw orbital trail
     */
    template<typename PixelType>
    void drawTrail(ICanvas<PixelType>& canvas) {
        if (!trailInitialized) return;
        
        PixelType color;
        if constexpr (std::is_same_v<PixelType, Pixel4>) {
            color = trailColor;
        } else {
            color = static_cast<PixelType>(trailColor.to8bit());
        }
        
        // Draw trail points with fading
        for (size_t i = 0; i < TRAIL_LENGTH; ++i) {
            size_t index = (trailIndex + i) % TRAIL_LENGTH;
            float alpha = static_cast<float>(i) / static_cast<float>(TRAIL_LENGTH);
            
            if constexpr (std::is_same_v<PixelType, Pixel4>) {
                Pixel4 fadedColor(static_cast<uint8_t>(color.value * alpha));
                canvas.setPixel(trailPoints[index].x, trailPoints[index].y, fadedColor);
            } else {
                uint8_t fadedColor = static_cast<uint8_t>(color * alpha);
                canvas.setPixel(trailPoints[index].x, trailPoints[index].y, fadedColor);
            }
        }
    }
    
    /**
     * @brief Draw line between two points
     */
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
            if (e2 > -dy) {
                err -= dy;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                y += sy;
            }
        }
    }
    
    /**
     * @brief Draw filled circle
     */
    template<typename PixelType>
    void drawCircle(ICanvas<PixelType>& canvas, Point center, float r, PixelType color, bool filled) {
        int radius = static_cast<int>(r);
        
        if (filled) {
            for (int y = -radius; y <= radius; ++y) {
                int x = static_cast<int>(std::sqrt(radius * radius - y * y));
                for (int dx = -x; dx <= x; ++dx) {
                    canvas.setPixel(center.x + dx, center.y + y, color);
                }
            }
        } else {
            // Circle outline using Bresenham
            int x = 0;
            int y = radius;
            int d = 3 - 2 * radius;
            
            while (x <= y) {
                canvas.setPixel(center.x + x, center.y + y, color);
                canvas.setPixel(center.x - x, center.y + y, color);
                canvas.setPixel(center.x + x, center.y - y, color);
                canvas.setPixel(center.x - x, center.y - y, color);
                canvas.setPixel(center.x + y, center.y + x, color);
                canvas.setPixel(center.x - y, center.y + x, color);
                canvas.setPixel(center.x + y, center.y - x, color);
                canvas.setPixel(center.x - y, center.y - x, color);
                
                if (d < 0) {
                    d = d + 4 * x + 6;
                } else {
                    d = d + 4 * (x - y) + 10;
                    y--;
                }
                x++;
            }
        }
    }
};

} // namespace enjin2