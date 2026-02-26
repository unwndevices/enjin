/**
 * @file planet.hpp
 * @brief Planet component for orbital visualization
 *
 * Represents a central planet or control hub in space-themed UI.
 * Can have satellites orbiting around it and supports various visual
 * effects like pulsing, rotation, and atmospheric effects.
 */
#pragma once

#include "../components/drawable.hpp"
#include "../components/animation.hpp"
#include "../core/types.hpp"
#include <cmath>

namespace enjin2 {

/**
 * @brief Planet component for orbital visualization
 * 
 * Represents a central planet or control hub in the space-themed UI.
 * Can have satellites orbiting around it and supports various visual
 * effects like pulsing, rotation, and atmospheric effects.
 */
class C_Planet : public C_Drawable {
private:
    float radius;               ///< Planet radius
    Pixel4 coreColor;          ///< Core planet color
    Pixel4 atmosphereColor;    ///< Atmosphere/glow color
    float atmosphereRadius;    ///< Atmosphere radius (multiple of core radius)
    bool hasAtmosphere;        ///< Whether to draw atmosphere
    
    // Visual effects
    bool pulsing;              ///< Enable pulsing effect
    float pulseSpeed;          ///< Pulse animation speed
    float pulseAmount;         ///< Pulse intensity (0.0 to 1.0)
    
    bool rotating;             ///< Enable rotation effect
    float rotationSpeed;       ///< Rotation speed in radians per second
    float currentRotation;     ///< Current rotation angle
    
    // Surface details
    bool hasRings;             ///< Draw rings around planet
    float ringInnerRadius;     ///< Inner radius of rings
    float ringOuterRadius;     ///< Outer radius of rings
    Pixel4 ringColor;          ///< Ring color
    
    // Animation state
    uint32_t animationTime;    ///< Internal animation timer
    
public:
    /**
     * @brief Constructor
     * @param owner Owner object
     * @param planetRadius Planet radius in pixels
     * @param color Planet core color
     */
    C_Planet(Object* owner, float planetRadius, Pixel4 color = Pixel4(12))
        : C_Drawable(owner, static_cast<uint16_t>(planetRadius * 2.5f), static_cast<uint16_t>(planetRadius * 2.5f)),
          radius(planetRadius), coreColor(color), atmosphereColor(color.value / 2),
          atmosphereRadius(1.5f), hasAtmosphere(true), pulsing(false),
          pulseSpeed(2.0f), pulseAmount(0.2f), rotating(false),
          rotationSpeed(0.5f), currentRotation(0.0f), hasRings(false),
          ringInnerRadius(0.0f), ringOuterRadius(0.0f), ringColor(8),
          animationTime(0) {
        
        SetBufferIndex(0);
    }
    
    /**
     * @brief Update planet animations
     * @param deltaTime Time elapsed since last update in milliseconds
     */
    void update(uint16_t deltaTime) override {
        Component::update(deltaTime);
        
        animationTime += deltaTime;
        
        // Update rotation
        if (rotating) {
            currentRotation += rotationSpeed * (deltaTime / 1000.0f);
            if (currentRotation >= 2.0f * 3.14159f) {
                currentRotation -= 2.0f * 3.14159f;
            }
        }
    }
    
    /**
     * @brief Draw planet to 4-bit canvas
     * @param canvas Target 4-bit canvas
     */
    void draw(ICanvas<Pixel4>& canvas) override {
        if (!isVisible()) return;
        
        Point center = getRenderPosition();
        center.x += getWidth() / 2;   // Center in drawable area
        center.y += getHeight() / 2;
        
        float currentRadius = radius;
        
        // Apply pulsing effect
        if (pulsing) {
            float pulse = std::sin((animationTime / 1000.0f) * pulseSpeed * 2.0f * 3.14159f);
            currentRadius *= (1.0f + pulse * pulseAmount);
        }
        
        // Draw atmosphere (outer glow)
        if (hasAtmosphere) {
            drawCircle(canvas, center, currentRadius * atmosphereRadius, atmosphereColor, true);
        }
        
        // Draw rings (behind planet)
        if (hasRings) {
            drawRing(canvas, center, ringInnerRadius * currentRadius, ringOuterRadius * currentRadius, ringColor);
        }
        
        // Draw planet core
        drawCircle(canvas, center, currentRadius, coreColor, true);
        
        // Draw surface details based on rotation
        if (rotating) {
            drawSurfaceDetails(canvas, center, currentRadius);
        }
    }
    
    /**
     * @brief Set planet core color
     * @param color New core color
     */
    void setCoreColor(Pixel4 color) {
        coreColor = color;
        atmosphereColor = Pixel4(color.value / 2);  // Dimmer atmosphere
    }
    
    /**
     * @brief Set atmosphere properties
     * @param enabled True to enable atmosphere rendering
     * @param radiusMultiplier Atmosphere radius as multiple of core radius
     * @param color Atmosphere color (0 = derive from core color)
     */
    void setAtmosphere(bool enabled, float radiusMultiplier = 1.5f, Pixel4 color = Pixel4(0)) {
        hasAtmosphere = enabled;
        atmosphereRadius = radiusMultiplier;
        if (color.value > 0) {
            atmosphereColor = color;
        }
    }
    
    /**
     * @brief Enable pulsing animation
     * @param enabled True to enable pulsing
     * @param speed Pulse frequency multiplier
     * @param amount Pulse intensity (0.0 to 1.0)
     */
    void setPulsing(bool enabled, float speed = 2.0f, float amount = 0.2f) {
        pulsing = enabled;
        pulseSpeed = speed;
        pulseAmount = amount;
    }
    
    /**
     * @brief Enable rotation animation
     * @param enabled True to enable rotation
     * @param speed Rotation speed in radians per second
     */
    void setRotation(bool enabled, float speed = 0.5f) {
        rotating = enabled;
        rotationSpeed = speed;
    }
    
    /**
     * @brief Add rings around planet
     * @param enabled True to enable rings
     * @param innerRadius Inner ring radius as multiple of planet radius
     * @param outerRadius Outer ring radius as multiple of planet radius
     * @param color Ring color
     */
    void setRings(bool enabled, float innerRadius = 1.3f, float outerRadius = 1.7f, Pixel4 color = Pixel4(8)) {
        hasRings = enabled;
        ringInnerRadius = innerRadius;
        ringOuterRadius = outerRadius;
        ringColor = color;
    }
    
    /**
     * @brief Get planet radius
     * @return Planet radius in pixels
     */
    float getRadius() const {
        return radius;
    }
    
    /**
     * @brief Get planet center position in world coordinates
     * @return Center point of the planet
     */
    Point getCenterPosition() const {
        Point center = getRenderPosition();
        center.x += getWidth() / 2;
        center.y += getHeight() / 2;
        return center;
    }

private:
    /**
     * @brief Draw filled circle
     */
    template<typename PixelType>
    void drawCircle(ICanvas<PixelType>& canvas, Point center, float r, PixelType color, bool filled) {
        int radius = static_cast<int>(r);
        
        if (filled) {
            // Draw filled circle using scanlines
            for (int y = -radius; y <= radius; ++y) {
                int x = static_cast<int>(std::sqrt(radius * radius - y * y));
                for (int dx = -x; dx <= x; ++dx) {
                    canvas.setPixel(center.x + dx, center.y + y, color);
                }
            }
        } else {
            // Draw circle outline using Bresenham-like algorithm
            int x = 0;
            int y = radius;
            int d = 3 - 2 * radius;
            
            while (x <= y) {
                // Draw 8 symmetric points
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
    
    /**
     * @brief Draw ring (hollow circle)
     */
    template<typename PixelType>
    void drawRing(ICanvas<PixelType>& canvas, Point center, float innerR, float outerR, PixelType color) {
        int outerRadius = static_cast<int>(outerR);
        int innerRadius = static_cast<int>(innerR);
        
        for (int y = -outerRadius; y <= outerRadius; ++y) {
            for (int x = -outerRadius; x <= outerRadius; ++x) {
                float dist = std::sqrt(x * x + y * y);
                if (dist >= innerRadius && dist <= outerRadius) {
                    canvas.setPixel(center.x + x, center.y + y, color);
                }
            }
        }
    }
    
    /**
     * @brief Draw surface details that rotate with the planet
     */
    template<typename PixelType>
    void drawSurfaceDetails(ICanvas<PixelType>& canvas, Point center, float r) {
        // Draw some simple surface features that rotate
        PixelType detailColor;
        if constexpr (std::is_same_v<PixelType, Pixel4>) {
            detailColor = Pixel4(coreColor.value > 2 ? coreColor.value - 2 : 0);
        } else {
            detailColor = static_cast<PixelType>(coreColor.to8bit() * 0.7f);
        }
        
        // Draw a few rotating spots
        for (int i = 0; i < 3; ++i) {
            float angle = currentRotation + (i * 2.0f * 3.14159f / 3.0f);
            float spotRadius = r * 0.7f;
            int x = static_cast<int>(center.x + spotRadius * std::cos(angle));
            int y = static_cast<int>(center.y + spotRadius * std::sin(angle));
            
            // Draw small detail spot
            drawCircle(canvas, Point(x, y), r * 0.1f, detailColor, true);
        }
    }
};

} // namespace enjin2