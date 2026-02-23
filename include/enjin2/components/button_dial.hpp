/**
 * @file button_dial.hpp
 * @brief Circular button dial component for discrete parameter selection
 *
 * A drawable dial component with multiple buttons arranged in a circle.
 * Used for selecting discrete values or modes.
 */
#ifndef ENJIN2_COMPONENTS_BUTTON_DIAL_HPP
#define ENJIN2_COMPONENTS_BUTTON_DIAL_HPP

#include "../core/component.hpp"
#include "drawable.hpp"
#include "position.hpp"
#include "../graphics/canvas.hpp"
#include "../core/math.hpp"

namespace enjin2 {

/**
 * @brief Circular button dial component for discrete parameter selection
 * 
 * A drawable dial component with multiple buttons arranged in a circle.
 * Used for selecting discrete values or modes.
 */
class ButtonDial : public Component {
private:
    uint8_t outer_radius;
    uint8_t inner_radius;
    uint8_t button_count;
    uint8_t color;
    int selected_id;
    Position* position;
    mutable Canvas<uint8_t, 64, 64> internal_canvas; // Max size for dials

public:
    /**
     * @brief Construct a new ButtonDial component
     * @param owner The object that owns this component
     * @param outerRadius Outer radius of the dial
     * @param innerRadius Inner radius of the dial
     * @param buttonCount Number of buttons around the circumference
     */
    ButtonDial(Object* owner, uint8_t outerRadius, uint8_t innerRadius, uint8_t buttonCount)
        : Component(owner)
        , outer_radius(outerRadius)
        , inner_radius(innerRadius)
        , button_count(buttonCount)
        , color(12)
        , selected_id(-1)
        , position(nullptr)
    {
        position = owner->getComponent<Position>();
    }

    /// @brief Initialize the button dial
    void onCreate() override {}

    /// @brief Update the button dial state
    /// @param deltaTime Time elapsed since last update in seconds
    void onUpdate(float deltaTime) override {}

    /**
     * @brief Draw the button dial to the canvas
     * @param canvas The canvas to draw to
     */
    void draw(ICanvas<uint8_t>& canvas) {
        if (!position) return;

        // Clear internal canvas
        int size = outer_radius * 2 + 1;
        internal_canvas.clear(16);

        Vector2 center = {outer_radius, outer_radius};

        // Draw outer circle
        internal_canvas.drawCircle(center.x, center.y, outer_radius, color);

        // Draw buttons and dividing lines
        for (int i = 0; i < button_count; i++) {
            float phase = (float)i / button_count;
            float angle = phase * 2.0f * PI;
            
            Vector2 point = {
                (int16_t)(center.x + cos(angle) * outer_radius),
                (int16_t)(center.y + sin(angle) * outer_radius)
            };
            
            internal_canvas.setPixel(point.x, point.y, color);

            // Draw dividing lines (dashed effect)
            Vector2 direction = {
                (int16_t)(cos(angle) * (outer_radius - inner_radius)),
                (int16_t)(sin(angle) * (outer_radius - inner_radius))
            };
            
            for (int r = inner_radius; r < outer_radius; r += 2) {
                Vector2 line_point = {
                    (int16_t)(center.x + cos(angle) * r),
                    (int16_t)(center.y + sin(angle) * r)
                };
                internal_canvas.setPixel(line_point.x, line_point.y, color);
            }
        }

        // Draw inner circle
        internal_canvas.fillCircle(center.x, center.y, inner_radius, 16);
        internal_canvas.drawCircle(center.x, center.y, inner_radius, color);

        // Highlight active button
        if (selected_id >= 0 && selected_id < button_count) {
            float activePhase = (float)(selected_id + 0.5f) / button_count;
            float angle = activePhase * 2.0f * PI;
            int activeRadius = (outer_radius + inner_radius) / 2 - 1;
            
            Vector2 activePoint = {
                (int16_t)(center.x + cos(angle) * activeRadius),
                (int16_t)(center.y + sin(angle) * activeRadius)
            };
            
            internal_canvas.fillCircle(activePoint.x, activePoint.y, 3, color);
        }

        // Copy to main canvas
        Vector2 pos = position->getGlobalPosition();
        canvas.blit(internal_canvas, pos.x, pos.y, size, size);
    }

    /**
     * @brief Set the selected button
     * @param id Button ID (0 to buttonCount-1, -1 for no selection)
     */
    void setSelectedButton(int id) {
        if (id >= -1 && id < button_count) {
            selected_id = id;
        }
    }

    /**
     * @brief Get the currently selected button
     * @return Button ID (-1 if no selection)
     */
    int getSelectedButton() const {
        return selected_id;
    }

    /**
     * @brief Set the dial color
     * @param newColor Color value (0-15 for 4-bit grayscale)
     */
    void setColor(uint8_t newColor) {
        color = newColor;
    }

    /**
     * @brief Get the number of buttons
     * @return Number of buttons around the circumference
     */
    uint8_t getButtonCount() const {
        return button_count;
    }
};

} // namespace enjin2

#endif // ENJIN2_COMPONENTS_BUTTON_DIAL_HPP