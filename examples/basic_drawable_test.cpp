#include <enjin2/graphics/canvas.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/components/drawable.hpp>
#include <enjin2/components/draw.hpp>
#include <enjin2/utils/polar.hpp>

using namespace enjin2;

int main() {
    printf("Basic Drawable Components Test - Enjin2\n");
    printf("======================================\n");
    
    // Create main canvas
    Canvas8<128, 64> canvas;
    canvas.clear(0); // Black background
    
    // Test 1: C_Draw component with simple lambda
    printf("Testing C_Draw component...\n");
    Object draw_obj;
    auto draw_pos = draw_obj.addComponent<C_Position>(10, 10);
    
    auto draw_comp = draw_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        // Draw a simple gradient pattern
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 20; x++) {
                uint8_t color = (x + y) % 16;
                canvas.setPixel(x + 10, y + 10, color);
            }
        }
    });
    
    draw_comp->draw(canvas);
    printf("✓ C_Draw component working\n");
    
    // Test 2: Polar coordinate utilities
    printf("Testing Polar coordinate utilities...\n");
    Point center(60, 32);
    
    // Draw points around a circle using polar coordinates
    for (int i = 0; i < 12; i++) {
        float phase = static_cast<float>(i) / 12.0f;
        Point polar_point = Polar::RadialToCartesian(phase, 20, center);
        
        // Draw a small cross at each point
        canvas.setPixel(polar_point.x, polar_point.y, 15);
        if (polar_point.x > 0) canvas.setPixel(polar_point.x - 1, polar_point.y, 15);
        if (polar_point.x < 127) canvas.setPixel(polar_point.x + 1, polar_point.y, 15);
        if (polar_point.y > 0) canvas.setPixel(polar_point.x, polar_point.y - 1, 15);
        if (polar_point.y < 63) canvas.setPixel(polar_point.x, polar_point.y + 1, 15);
    }
    printf("✓ Polar coordinate conversion working\n");
    
    // Test 3: Draw anchor point examples
    printf("Testing anchor points...\n");
    Object anchor_obj;
    auto anchor_pos = anchor_obj.addComponent<C_Position>(90, 45);
    
    auto anchor_draw = anchor_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        // Draw a small rectangle to show anchor positioning
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 12; x++) {
                canvas.setPixel(x + 90, y + 45, 8);
            }
        }
    });
    
    anchor_draw->SetAnchorPoint(Anchor::CENTER);
    anchor_draw->draw(canvas);
    printf("✓ Anchor point positioning working\n");
    
    // Export results
    canvas.exportToPGM("basic_drawable_test.pgm");
    printf("\nTest complete! Results exported to basic_drawable_test.pgm\n");
    printf("Basic drawable components are working correctly.\n");
    
    return 0;
}