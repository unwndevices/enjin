/**
 * @file tickmarks.hpp
 * @brief Tickmarks component for drawing measurement scales
 *
 * A component that draws tickmarks around a circular arc, useful for creating
 * dial scales, meters, and other measurement indicators.
 */
#ifndef ENJIN2_COMPONENTS_TICKMARKS_HPP
#define ENJIN2_COMPONENTS_TICKMARKS_HPP

#include "../core/component.hpp"
#include "drawable.hpp"
#include "position.hpp"
#include "../graphics/canvas.hpp"
#include "../core/math.hpp"

namespace enjin2 {

/**
 * @brief Tickmarks component for drawing measurement scales
 * 
 * A component that draws tickmarks around a circular arc, useful for creating
 * dial scales, meters, and other measurement indicators.
 */
class Tickmarks : public Component {
private:
    Vector2 center;
    int16_t start_angle;
    int16_t stop_angle;
    uint8_t spacing;
    uint8_t length;
    uint8_t radius;
    float current_value;
    Position* position;
    mutable Canvas<uint8_t, 128, 128> internal_canvas; // Large canvas for full scale

public:
    /**
     * @brief Construct a new Tickmarks component
     * @param owner The object that owns this component
     * @param centerPoint Center point for the tickmarks arc
     * @param startAngle Starting angle in degrees
     * @param stopAngle Ending angle in degrees
     * @param tickSpacing Spacing between tickmarks in degrees
     * @param tickLength Length of the tickmarks in pixels
     * @param arcRadius Radius of the arc on which tickmarks are drawn
     */
    Tickmarks(Object* owner, Vector2 centerPoint, int16_t startAngle, int16_t stopAngle, 
             uint8_t tickSpacing, uint8_t tickLength, uint8_t arcRadius)
        : Component(owner)
        , center(centerPoint)
        , start_angle(startAngle)
        , stop_angle(stopAngle)
        , spacing(tickSpacing)
        , length(tickLength)
        , radius(arcRadius)
        , current_value(0.0f)
        , position(nullptr)
    {
        position = owner->getComponent<Position>();
    }

    /// @brief Initialize the tickmarks
    void onCreate() override {}

    /// @brief Update the tickmarks state
    /// @param deltaTime Time elapsed since last update in seconds
    void onUpdate(float deltaTime) override {}

    /**
     * @brief Draw the tickmarks to the canvas
     * @param canvas The canvas to draw to
     */
    void draw(ICanvas<uint8_t>& canvas) {
        if (!position) return;

        // Clear internal canvas
        internal_canvas.clear(16);

        // Draw tickmarks
        for (int16_t i = start_angle; i <= stop_angle; i += spacing) {
            // Calculate angle adjusted for current value
            int angle = i - ((int)(current_value * 100.0f) * spacing / 10) % spacing - 90;
            
            // Calculate tick value for determining tick length
            float tick_value = floorf((current_value * 10.0f) + (angle + 90.0f) / spacing);
            
            // Major ticks (every 10th) are longer
            uint8_t tick_length = ((int)tick_value % 10 == 0) ? length : length / 2;
            
            // Convert angle to radians
            float angle_rad = angle * PI / 180.0f;
            
            // Calculate start and end points of the tickmark
            int start_x = center.x + (radius - tick_length) * sin(angle_rad);
            int start_y = center.y + (radius - tick_length) * cos(angle_rad);
            int end_x = start_x + tick_length * sin(angle_rad);
            int end_y = start_y + tick_length * cos(angle_rad);
            
            // Ensure coordinates are within bounds
            if (start_x >= 0 && start_x < 128 && start_y >= 0 && start_y < 128 &&
                end_x >= 0 && end_x < 128 && end_y >= 0 && end_y < 128) {
                internal_canvas.drawLine(start_x, start_y, end_x, end_y, 15);
            }
        }

        // Copy to main canvas
        Vector2 pos = position->getGlobalPosition();
        canvas.blit(internal_canvas, pos.x, pos.y, 128, 128);
    }

    /**
     * @brief Set the current value for tickmark positioning
     * @param value Value that affects tickmark positioning (typically 0.0-1.0)
     */
    void setValue(float value) {
        current_value = value;
    }

    /**
     * @brief Get the current value
     * @return Current value
     */
    float getValue() const {
        return current_value;
    }

    /**
     * @brief Set the center point of the tickmarks
     * @param newCenter New center point
     */
    void setCenter(Vector2 newCenter) {
        center = newCenter;
    }

    /**
     * @brief Set the angle range for the tickmarks
     * @param startAngle Starting angle in degrees
     * @param stopAngle Ending angle in degrees
     */
    void setAngleRange(int16_t startAngle, int16_t stopAngle) {
        start_angle = startAngle;
        stop_angle = stopAngle;
    }

    /**
     * @brief Set the tickmark spacing
     * @param newSpacing Spacing between tickmarks in degrees
     */
    void setSpacing(uint8_t newSpacing) {
        spacing = newSpacing;
    }

    /**
     * @brief Set the tickmark length
     * @param newLength Length of the tickmarks in pixels
     */
    void setLength(uint8_t newLength) {
        length = newLength;
    }

    /**
     * @brief Set the arc radius
     * @param newRadius Radius of the arc on which tickmarks are drawn
     */
    void setRadius(uint8_t newRadius) {
        radius = newRadius;
    }
};

} // namespace enjin2

#endif // ENJIN2_COMPONENTS_TICKMARKS_HPP