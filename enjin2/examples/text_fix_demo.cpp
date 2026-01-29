#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/position.hpp"
#include "../include/enjin2/components/label.hpp"
#include "../include/enjin2/components/draw.hpp"

using namespace enjin2;

int main() {
    printf("Text Rendering Fix Demo\n");
    printf("======================\n");
    
    Canvas8<128, 64> canvas;
    canvas.clear(0);
    
    // Demo 1: Current broken Label behavior
    printf("Demo 1: Current Label (overlapping text)\n");
    Object broken_obj;
    auto broken_pos = broken_obj.addComponent<C_Position>(5, 5);
    auto broken_label = broken_obj.addComponent<Label>(118, 40);
    broken_label->setText("This is a long text that should wrap to multiple lines properly");
    broken_label->setAlignment(LabelAlign::Left);
    broken_label->draw(canvas);
    
    // Export the broken version
    canvas.exportToPGM("text_fix_broken.pgm");
    
    // Clear canvas for next demo
    canvas.clear(0);
    
    // Demo 2: Fixed version using C_Draw with proper TextRenderer
    printf("Demo 2: Fixed version with proper spacing\n");
    Object fixed_obj;
    auto fixed_pos = fixed_obj.addComponent<C_Position>(5, 5);
    auto fixed_draw = fixed_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        TextRenderer<uint8_t> renderer;
        renderer.setFont(nullptr); // Use built-in font
        renderer.setTextSize(1);
        renderer.setTextColor(15);
        
        // Draw text with proper wrapping
        const char* text = "This is a long text that should wrap to multiple lines properly";
        renderer.drawStringWrapped(canvas, 5, 5, 118, text);
    });
    fixed_draw->draw(canvas);
    
    // Export the fixed version
    canvas.exportToPGM("text_fix_working.pgm");
    
    // Clear canvas for next demo
    canvas.clear(0);
    
    // Demo 3: Multiple separate lines with proper spacing
    printf("Demo 3: Multiple separate text lines\n");
    Object multi_obj;
    auto multi_draw = multi_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        TextRenderer<uint8_t> renderer;
        renderer.setFont(nullptr);
        renderer.setTextSize(1);
        renderer.setTextColor(15);
        
        // Draw multiple lines with proper spacing
        const char* lines[] = {
            "Line 1: First line of text",
            "Line 2: Second line of text", 
            "Line 3: Third line of text",
            "Line 4: Fourth line of text"
        };
        
        int16_t y = 5;
        int16_t line_height = renderer.getCharHeight(); // Should be 8 for built-in font
        
        for (int i = 0; i < 4; i++) {
            renderer.setCursor(5, y);
            renderer.drawString(canvas, lines[i]);
            y += line_height + 2; // Add 2 pixels spacing between lines
        }
    });
    multi_draw->draw(canvas);
    
    // Export the multi-line version
    canvas.exportToPGM("text_fix_multiline.pgm");
    
    printf("\n✓ Generated test images:\n");
    printf("  - text_fix_broken.pgm (shows overlapping issue)\n");
    printf("  - text_fix_working.pgm (shows proper wrapping)\n");
    printf("  - text_fix_multiline.pgm (shows proper line spacing)\n");
    
    printf("\nThe issue is in the Label component's renderText() method.\n");
    printf("It's fighting with TextRenderer's automatic cursor management.\n");
    
    return 0;
}