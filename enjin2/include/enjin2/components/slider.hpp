#ifndef ENJIN2_COMPONENTS_SLIDER_HPP
#define ENJIN2_COMPONENTS_SLIDER_HPP

#include "../core/component.hpp"
#include "drawable.hpp"
#include "position.hpp"
#include "../graphics/canvas.hpp"

namespace enjin2 {

/**
 * @brief Linear slider component for parameter control
 * 
 * A drawable slider component that displays a linear slider with a filled portion
 * indicating the current value. Can be used for controlling continuous parameters.
 */
class Slider : public Component {
private:
    uint8_t slider_width;
    uint8_t slider_height;
    uint8_t color;
    float value;
    Position* position;
    mutable Canvas<uint8_t, 64, 32> internal_canvas; // Max size for sliders

public:
    /**
     * @brief Construct a new Slider component
     * @param owner The object that owns this component
     * @param width Width of the slider in pixels
     * @param height Height of the slider in pixels
     */
    Slider(Object* owner, uint8_t width, uint8_t height)
        : Component(owner)
        , slider_width(width)
        , slider_height(height)
        , color(12)
        , value(0.0f)
        , position(nullptr)
    {
        position = owner->getComponent<Position>();
    }

    void onCreate() override {}

    void onUpdate(float deltaTime) override {}

    /**
     * @brief Draw the slider to the canvas
     * @param canvas The canvas to draw to
     */
    void draw(ICanvas<uint8_t>& canvas) {
        if (!position) return;

        // Clear internal canvas
        internal_canvas.clear(16);

        int midX = slider_width / 2;
        int midY = slider_height / 2;

        // Calculate slider fill length
        int sliderLength = std::max((int)(slider_width * value), 1);
        int leftX = midX - sliderLength / 2;

        // Draw slider track
        internal_canvas.drawVLine(midX - slider_width / 2, 0, slider_height, color);
        internal_canvas.drawVLine(midX + slider_width / 2, 0, slider_height, color);
        
        // Draw center line (dashed effect by drawing every other pixel)
        for (int x = midX - slider_width / 2; x <= midX + slider_width / 2; x += 2) {
            internal_canvas.setPixel(x, midY, color / 2);
        }

        // Draw filled portion
        internal_canvas.fillRect(leftX, midY - slider_height / 4, sliderLength, slider_height / 2, 16);
        internal_canvas.drawRect(leftX, midY - slider_height / 4, sliderLength, slider_height / 2, color);

        // Copy to main canvas
        Vector2 pos = position->getGlobalPosition();
        canvas.blit(internal_canvas, pos.x, pos.y, slider_width, slider_height);
    }

    /**
     * @brief Set the slider value
     * @param newValue Value between 0.0 and 1.0
     */
    void setValue(float newValue) {
        value = std::clamp(newValue, 0.0f, 1.0f);
    }

    /**
     * @brief Get the current slider value
     * @return Value between 0.0 and 1.0
     */
    float getValue() const {
        return value;
    }

    /**
     * @brief Set the slider color
     * @param newColor Color value (0-15 for 4-bit grayscale)
     */
    void setColor(uint8_t newColor) {
        color = newColor;
    }
};

} // namespace enjin2

#endif // ENJIN2_COMPONENTS_SLIDER_HPP