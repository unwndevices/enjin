#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/text_renderer.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/components/label.hpp>

// Include PROGMEM definition before font
#ifndef PROGMEM
#define PROGMEM
#endif

// GFXglyph / GFXfont are global types (graphics/gfxfont.h, pulled in via
// text_renderer.hpp), which is exactly what the font header below expects.

// Include the font - it will use the global GFXfont/GFXglyph definitions
#include "../Libs/Adafruit-GFX-Library/Fonts/awkward.h"

using namespace enjin2;

int main() {
    // Create canvas
    Canvas8<128, 64> canvas;
    canvas.clear(0); // Black background
    
    // Create object and position
    Object label_obj;
    auto position = label_obj.addComponent<C_Position>(10, 10);
    
    // Test 1: Default built-in font
    auto label1 = label_obj.addComponent<Label>(
        100, 20,        // width, height
        nullptr,        // Use default built-in font
        1,              // font size
        15,             // text color (white)
        1,              // background color (dark gray)
        0               // no pointer
    );
    label1->setText("Built-in Font");
    label1->draw(canvas);
    
    // Test 2: GFX font (Awkward)
    position->setPosition({10, 35});
    auto label2 = label_obj.addComponent<Label>(
        100, 25,        // width, height
        &Awkward8pt7b,  // GFX font
        1,              // font size
        15,             // text color (white)
        2,              // background color (slightly lighter)
        8               // pointer height
    );
    label2->setText("Awkward GFX Font!");
    label2->draw(canvas);
    
    // Export with proper color scaling
    canvas.exportToPGM("text_demo_improved.pgm");
    
    printf("Text demo exported to text_demo_improved.pgm\n");
    printf("Built-in font and GFX font rendering test complete.\n");
    
    return 0;
}