#ifndef ENJIN2_COMPONENTS_LABEL_HPP
#define ENJIN2_COMPONENTS_LABEL_HPP

#include "../core/component.hpp"
#include "drawable.hpp"
#include "position.hpp"
#include "../graphics/canvas.hpp"
#include "../graphics/canvas_extended.hpp"
#include "../graphics/text_renderer.hpp"
#include "../core/types.hpp"
#include <string>
#include <vector>
#include <sstream>

namespace enjin2 {

/**
 * @brief Text alignment options for labels
 */
enum class LabelAlign {
    Left,
    Center,
    Right
};

/**
 * @brief Label component for text display with word wrapping and styling
 * 
 * A versatile text display component that supports:
 * - Custom fonts (GFX-style fonts)
 * - Text wrapping and alignment
 * - Background colors and borders
 * - Tooltip-style pointers
 * - Opacity/blend modes
 */
class Label : public Component {
private:
    uint16_t width;
    uint16_t height;
    uint8_t label_color;
    uint8_t bg_color;
    uint8_t pointer_height;
    bool transparent_bg;
    std::string text;
    const GFXfont* font;
    uint8_t font_size;
    LabelAlign alignment;
    bool word_wrap;
    int16_t left_margin;
    int16_t right_margin;
    C_Position* position;
    TextRenderer<uint8_t> text_renderer;
    mutable Canvas8<128, 64> internal_canvas; // Max size for labels

    // Helper function to split text by spaces
    std::vector<std::string> split(const std::string& s, char delimiter) const {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

public:
    /**
     * @brief Construct a new Label component
     * @param owner The object that owns this component
     * @param w Width of the label in pixels
     * @param h Height of the label in pixels
     * @param textFont Font to use (nullptr for default)
     * @param fontSize Font size multiplier
     * @param textColor Text color (0-15 for 4-bit grayscale)
     * @param backgroundColor Background color (0 for transparent)
     * @param pointerHeight Height of tooltip pointer (0 for no pointer)
     */
    Label(Object* owner, uint16_t w, uint16_t h, const GFXfont* textFont = nullptr, 
          uint8_t fontSize = 1, uint8_t textColor = 14, uint8_t backgroundColor = 0, 
          uint8_t pointerHeight = 0)
        : Component(owner)
        , width(w)
        , height(h)
        , label_color(textColor)
        , bg_color(backgroundColor)
        , pointer_height(pointerHeight)
        , transparent_bg(backgroundColor == 0)
        , text("Label")
        , font(textFont)
        , font_size(fontSize)
        , alignment(LabelAlign::Center)
        , word_wrap(true)
        , left_margin(2)
        , right_margin(2)
        , position(nullptr)
    {
        position = owner->getComponent<C_Position>();
        
        // Configure text renderer
        text_renderer.setFont(font);
        text_renderer.setTextSize(font_size);
        if (transparent_bg) {
            text_renderer.setTextColor(label_color);
        } else {
            text_renderer.setTextColor(label_color, bg_color);
        }
    }


    /**
     * @brief Draw the label to the canvas
     * @param canvas The canvas to draw to
     */
    void draw(ICanvas<uint8_t>& canvas) {
        if (!position || text.empty()) return;

        // Clear internal canvas
        internal_canvas.clear(bg_color == 0 ? 16 : bg_color); // Use transparent or bg color

        // Calculate text layout
        layoutText();

        // Copy to main canvas manually
        Point pos = position->getPosition();
        for (int16_t y = 0; y < height + pointer_height; y++) {
            for (int16_t x = 0; x < width; x++) {
                uint8_t pixel = internal_canvas.getPixel(x, y);
                if (pixel != 16) { // Skip transparent pixels
                    canvas.setPixel(pos.x + x, pos.y + y, pixel);
                }
            }
        }
    }

    /**
     * @brief Set the label text
     * @param newText Text to display
     */
    void setText(const std::string& newText) {
        text = newText;
    }

    /**
     * @brief Get the current text
     * @return Current text string
     */
    const std::string& getText() const {
        return text;
    }

    /**
     * @brief Set text color
     * @param color New text color (0-15)
     */
    void setTextColor(uint8_t color) {
        label_color = color;
        if (transparent_bg) {
            text_renderer.setTextColor(label_color);
        } else {
            text_renderer.setTextColor(label_color, bg_color);
        }
    }

    /**
     * @brief Set background color
     * @param color Background color (0 for transparent)
     */
    void setBackgroundColor(uint8_t color) {
        bg_color = color;
        transparent_bg = (color == 0);
        if (transparent_bg) {
            text_renderer.setTextColor(label_color);
        } else {
            text_renderer.setTextColor(label_color, bg_color);
        }
    }

    /**
     * @brief Set text alignment
     * @param align Text alignment mode
     */
    void setAlignment(LabelAlign align) {
        alignment = align;
    }

    /**
     * @brief Set margins
     * @param left Left margin in pixels
     * @param right Right margin in pixels (defaults to left margin)
     */
    void setMargins(int16_t left, int16_t right = -1) {
        left_margin = left;
        right_margin = (right >= 0) ? right : left;
    }

    /**
     * @brief Enable or disable word wrapping
     * @param wrap True to enable word wrapping
     */
    void setWordWrap(bool wrap) {
        word_wrap = wrap;
    }

    /**
     * @brief Set font size
     * @param size Font size multiplier (1 = normal, 2 = double, etc.)
     */
    void setFontSize(uint8_t size) {
        font_size = size;
        text_renderer.setTextSize(font_size);
    }

    /**
     * @brief Set pointer height for tooltip-style labels
     * @param height Height of pointer in pixels (0 for no pointer)
     */
    void setPointerHeight(uint8_t height) {
        pointer_height = height;
    }

private:
    /**
     * @brief Layout and render text to internal canvas
     */
    void layoutText() {
        if (text.empty()) return;

        const int16_t padding = 2;
        const int16_t line_spacing = 2;
        int16_t box_height = height - pointer_height;

        // Clear background
        if (!transparent_bg) {
            // Draw background as simple rectangle for now
            for (int16_t y = 0; y < box_height; y++) {
                for (int16_t x = 0; x < width; x++) {
                    internal_canvas.setPixel(x, y, bg_color);
                }
            }
            
            // Draw border
            for (int16_t x = 0; x < width; x++) {
                internal_canvas.setPixel(x, 0, label_color);
                // Draw bottom border but leave opening for pointer
                if (pointer_height == 0 || x < width/2 - 3 || x > width/2 + 3) {
                    internal_canvas.setPixel(x, box_height - 1, label_color);
                }
            }
            for (int16_t y = 0; y < box_height; y++) {
                internal_canvas.setPixel(0, y, label_color);
                internal_canvas.setPixel(width - 1, y, label_color);
            }
        }

        // Draw pointer if needed
        if (pointer_height > 0 && !transparent_bg) {
            int16_t pointer_base_y = box_height - 1;
            int16_t pointer_tip_y = box_height + pointer_height - 1;
            int16_t pointer_center_x = width / 2;
            
            // Draw simple triangle pointer manually
            for (int16_t y = 0; y < pointer_height; y++) {
                int16_t left_x = pointer_center_x - (3 * (pointer_height - y)) / pointer_height;
                int16_t right_x = pointer_center_x + (3 * (pointer_height - y)) / pointer_height;
                for (int16_t x = left_x; x <= right_x; x++) {
                    internal_canvas.setPixel(x, box_height + y, bg_color);
                }
            }
            
            // Draw triangle outline
            for (int16_t y = 0; y < pointer_height; y++) {
                int16_t left_x = pointer_center_x - (3 * (pointer_height - y)) / pointer_height;
                int16_t right_x = pointer_center_x + (3 * (pointer_height - y)) / pointer_height;
                internal_canvas.setPixel(left_x, box_height + y, label_color);
                internal_canvas.setPixel(right_x, box_height + y, label_color);
            }
        }

        // Layout text
        renderText(padding, line_spacing, box_height);
    }

    /**
     * @brief Render text with word wrapping and alignment
     */
    void renderText(int16_t padding, int16_t line_spacing, int16_t box_height) {
        std::vector<std::string> lines;
        
        if (word_wrap) {
            // Word wrapping
            auto words = split(text, ' ');
            std::string current_line;
            
            for (const auto& word : words) {
                if (word.empty()) continue;
                
                std::string test_line = current_line;
                if (!test_line.empty()) {
                    test_line += " ";
                }
                test_line += word;
                
                uint16_t line_width = text_renderer.getTextWidth(test_line.c_str());
                
                if (line_width > width - 2 * padding - left_margin - right_margin) {
                    if (!current_line.empty()) {
                        lines.push_back(current_line);
                    }
                    current_line = word;
                } else {
                    current_line = test_line;
                }
            }
            
            if (!current_line.empty()) {
                lines.push_back(current_line);
            }
        } else {
            // No wrapping - just use the text as is
            lines.push_back(text);
        }

        // Get proper character height from text renderer
        uint16_t char_height = text_renderer.getCharHeight();
        uint16_t total_text_height = lines.size() * char_height * font_size + 
                                   (lines.size() - 1) * line_spacing;

        // Calculate starting Y position for vertical centering
        int16_t start_y = (box_height - total_text_height) / 2;
        if (start_y < padding) start_y = padding;

        // Render each line using text renderer properly
        int16_t current_y = start_y;
        for (const auto& line : lines) {
            int16_t line_x = calculateLineX(line, padding);
            
            // Use drawString instead of writeChar to avoid automatic cursor management conflicts
            text_renderer.drawString(internal_canvas, line_x, current_y, line.c_str());
            
            current_y += char_height * font_size + line_spacing;
        }
    }

    /**
     * @brief Calculate X position for a line based on alignment
     */
    int16_t calculateLineX(const std::string& line, int16_t padding) {
        uint16_t line_width = line.length() * 6 * font_size; // 6 pixels per char (5+1 spacing)
        
        switch (alignment) {
            case LabelAlign::Left:
                return padding + left_margin;
                
            case LabelAlign::Right:
                return width - padding - right_margin - line_width;
                
            case LabelAlign::Center:
            default:
                return (width - line_width) / 2;
        }
    }

};

} // namespace enjin2

#endif // ENJIN2_COMPONENTS_LABEL_HPP