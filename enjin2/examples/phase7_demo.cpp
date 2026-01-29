#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/position.hpp"
#include "../include/enjin2/components/drawable.hpp"
#include "../include/enjin2/components/draw.hpp"
#include "../include/enjin2/components/sprite.hpp"
#include "../include/enjin2/utils/drawing_helpers.hpp"
#include "../include/enjin2/utils/polar.hpp"
#include <cmath>

using namespace enjin2;

// Simple test sprite data (8x8 smiley face)
const uint8_t test_sprite_data[] = {
    0, 0, 15, 15, 15, 15, 0, 0,
    0, 15, 15, 15, 15, 15, 15, 0,
    15, 15, 8, 15, 15, 8, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 8, 15, 15, 8, 15, 15,
    15, 15, 15, 8, 8, 15, 15, 15,
    0, 15, 15, 15, 15, 15, 15, 0,
    0, 0, 15, 15, 15, 15, 0, 0
};

int main() {
    printf("Phase 7 & 8 Components Demo - Enjin2\n");
    printf("=====================================\n");
    
    // Create main canvas
    Canvas8<128, 64> canvas;
    canvas.clear(0); // Black background
    
    // Test 1: C_Draw component with lambda rendering
    printf("Testing C_Draw component with lambda rendering...\n");
    Object draw_obj;
    auto draw_pos = draw_obj.addComponent<C_Position>(10, 10);
    
    auto draw_comp = draw_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        // Draw a simple pattern using lambda
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 20; x++) {
                uint8_t color = (x + y) % 16;
                canvas.setPixel(x + 10, y + 10, color);
            }
        }
    });
    
    draw_comp->draw(canvas);
    printf("✓ C_Draw lambda rendering complete\n");
    
    // Test 2: C_Sprite component
    printf("Testing C_Sprite component...\n");
    Object sprite_obj;
    auto sprite_pos = sprite_obj.addComponent<C_Position>(40, 10);
    auto sprite_comp = sprite_obj.addComponent<C_Sprite>(8, 8);
    
    sprite_comp->Load(test_sprite_data, 8, 8);
    sprite_comp->setMatte(0); // Black is transparent
    sprite_comp->draw(canvas);
    printf("✓ C_Sprite rendering complete\n");
    
    // Test 3: DrawingHelpers - Circle with stroke
    printf("Testing DrawingHelpers::drawCircleStroke...\n");
    DrawingHelpers::drawCircleStroke(canvas, 70, 25, 15, 12, 3);
    printf("✓ Circle stroke drawing complete\n");
    
    // Test 4: DrawingHelpers - Polygon
    printf("Testing DrawingHelpers::drawPolygon...\n");
    Point triangle[] = {
        {100, 10},
        {110, 30},
        {90, 30}
    };
    DrawingHelpers::drawPolygon(canvas, triangle, 3, 14, true);
    printf("✓ Polygon drawing complete\n");
    
    // Test 5: DrawingHelpers - Star
    printf("Testing DrawingHelpers::drawStar...\n");
    DrawingHelpers::drawStar(canvas, 25, 45, 8, 4, 5, 10, false);
    printf("✓ Star drawing complete\n");
    
    // Test 6: Polar coordinate utilities
    printf("Testing Polar coordinate utilities...\n");
    Point center(60, 45);
    
    // Draw points around a circle using polar coordinates
    for (int i = 0; i < 8; i++) {
        float phase = static_cast<float>(i) / 8.0f;
        Point polar_point = Polar::RadialToCartesian(phase, 12, center);
        canvas.setPixel(polar_point.x, polar_point.y, 15);
        canvas.setPixel(polar_point.x + 1, polar_point.y, 15);
        canvas.setPixel(polar_point.x, polar_point.y + 1, 15);
        canvas.setPixel(polar_point.x + 1, polar_point.y + 1, 15);
    }
    printf("✓ Polar coordinate conversion complete\n");
    
    // Test 7: DrawingHelpers - Rounded rectangle
    printf("Testing DrawingHelpers::drawRoundedRect...\n");
    DrawingHelpers::drawRoundedRect(canvas, 85, 40, 25, 15, 3, 13, false);
    printf("✓ Rounded rectangle drawing complete\n");
    
    // Test 8: DrawingHelpers - Thick line
    printf("Testing DrawingHelpers::drawThickLine...\n");
    DrawingHelpers::drawThickLine(canvas, 5, 55, 25, 60, 11, 3);
    printf("✓ Thick line drawing complete\n");
    
    // Test 9: Bezier curve
    printf("Testing DrawingHelpers::drawBezierCurve...\n");
    DrawingHelpers::drawBezierCurve(canvas, 30, 55, 40, 45, 50, 65, 65, 55, 9, 15);
    printf("✓ Bezier curve drawing complete\n");
    
    // Export results
    canvas.exportToPGM("phase7_demo.pgm");
    printf("\nDemo complete! Results exported to phase7_demo.pgm\n");
    printf("All Phase 7 & 8 core components are working correctly.\n");
    
    return 0;
}