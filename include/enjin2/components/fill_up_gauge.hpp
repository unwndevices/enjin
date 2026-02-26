/**
 * @file fill_up_gauge.hpp
 * @brief Fill-up gauge component for level display
 *
 * A visual gauge that fills up to represent a value, similar to a VU meter.
 * Supports both unidirectional (0-1) and bidirectional (-1 to 1) modes.
 */
#ifndef ENJIN2_COMPONENTS_FILL_UP_GAUGE_HPP
#define ENJIN2_COMPONENTS_FILL_UP_GAUGE_HPP

#include "../core/component.hpp"
#include "drawable.hpp"
#include "position.hpp"
#include "../graphics/canvas.hpp"

namespace enjin2 {

/**
 * @brief Mode for fill-up gauge behavior
 */
enum class GaugeMode {
    Unidirectional, ///< Fill from bottom to top (0.0 to 1.0)
    Bidirectional   ///< Fill from center outward (-1.0 to 1.0)
};

/**
 * @brief Fill-up gauge component for level display
 * 
 * A visual gauge that fills up to represent a value, similar to a VU meter.
 * Supports both unidirectional (0-1) and bidirectional (-1 to 1) modes.
 */
class FillUpGauge : public Component {
private:
    uint16_t width;
    uint16_t height;
    uint16_t color;
    float current_value;
    GaugeMode mode;
    Position* position;
    mutable Canvas<uint8_t, 64, 64> internal_canvas; // Max size for gauges

    // Dither pattern for fill effect
    static constexpr uint8_t pattern[16] = {
        8, 0, 8, 0,  // row 1: alternating
        0, 0, 0, 8,
        8, 0, 8, 0,
        0, 8, 0, 0   // row 2: offset alternating
    };

public:
    /**
     * @brief Construct a new FillUpGauge component
     * @param owner The object that owns this component
     * @param w Width of the gauge in pixels
     * @param h Height of the gauge in pixels
     * @param gaugeColor Color for the gauge outline and indicator
     * @param gaugeMode Mode of operation (unidirectional or bidirectional)
     */
    FillUpGauge(Object* owner, uint16_t w, uint16_t h, uint16_t gaugeColor, GaugeMode gaugeMode)
        : Component(owner)
        , width(w)
        , height(h)
        , color(gaugeColor)
        , current_value(0.0f)
        , mode(gaugeMode)
        , position(nullptr)
    {
        position = owner->getComponent<Position>();
    }

    /// @brief Initialize the gauge
    void onCreate() override {}

    /// @brief Update the gauge state
    /// @param dt Time elapsed since last update in seconds
    void onUpdate(float dt) override {}

    /**
     * @brief Draw the gauge to the canvas
     * @param canvas The canvas to draw to
     */
    void draw(ICanvas<uint8_t>& canvas) {
        if (!position) return;

        // Clear internal canvas
        internal_canvas.clear(16);

        if (mode == GaugeMode::Unidirectional) {
            drawUnidirectional();
        } else {
            drawBidirectional();
        }

        // Draw circular mask/border
        internal_canvas.drawCircle(width / 2, height / 2, width / 2, color);

        // Copy to main canvas
        Vector2 pos = position->getGlobalPosition();
        canvas.blit(internal_canvas, pos.x, pos.y, width, height);
    }

    /**
     * @brief Set the gauge value
     * @param value Value to display (0.0-1.0 for unidirectional, -1.0-1.0 for bidirectional)
     */
    void setValue(float value) {
        if (mode == GaugeMode::Bidirectional) {
            current_value = std::clamp(value, -1.0f, 1.0f);
        } else {
            current_value = std::clamp(value, 0.0f, 1.0f);
        }
    }

    /**
     * @brief Get the current gauge value
     * @return Current value
     */
    float getValue() const {
        return current_value;
    }

    /**
     * @brief Set the gauge mode
     * @param newMode New mode (unidirectional or bidirectional)
     */
    void setMode(GaugeMode newMode) {
        mode = newMode;
    }

    /**
     * @brief Get the current gauge mode
     * @return Current mode
     */
    GaugeMode getMode() const {
        return mode;
    }

    /**
     * @brief Set the gauge color
     * @param newColor Color value (0-15 for 4-bit grayscale)
     */
    void setColor(uint16_t newColor) {
        color = newColor;
    }

private:
    /**
     * @brief Draw unidirectional gauge (fills from bottom)
     */
    void drawUnidirectional() {
        uint16_t filled_height = height * current_value;
        
        // Draw filled area with pattern
        drawPatternRect(0, height - filled_height, width, filled_height);
        
        // Draw line at fill level
        internal_canvas.drawHLine(0, height - filled_height, width, 8);
    }

    /**
     * @brief Draw bidirectional gauge (fills from center)
     */
    void drawBidirectional() {
        uint16_t filled_height = abs(height * current_value / 2);
        
        if (current_value >= 0) {
            // Fill upward from center
            drawPatternRect(0, height / 2 - filled_height, width, filled_height);
            internal_canvas.drawHLine(0, height / 2 - filled_height, width, 8);
        } else {
            // Fill downward from center
            drawPatternRect(0, height / 2, width, filled_height);
            internal_canvas.drawHLine(0, height / 2 + filled_height, width, 8);
        }
        
        // Draw center line
        internal_canvas.drawHLine(0, height / 2, width, color);
    }

    /**
     * @brief Draw a rectangle with dither pattern
     * @param x X coordinate
     * @param y Y coordinate
     * @param w Width
     * @param h Height
     */
    void drawPatternRect(int x, int y, int w, int h) {
        for (int py = y; py < y + h; py++) {
            for (int px = x; px < x + w; px++) {
                // Use pattern based on position
                int pattern_x = px % 4;
                int pattern_y = py % 4;
                uint8_t pattern_value = pattern[pattern_y * 4 + pattern_x];
                
                if (pattern_value > 0) {
                    internal_canvas.setPixel(px, py, pattern_value);
                }
            }
        }
    }
};

} // namespace enjin2

#endif // ENJIN2_COMPONENTS_FILL_UP_GAUGE_HPP