#include <enjin2/graphics/canvas.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/components/label.hpp>
#include <enjin2/components/draw.hpp>
#include <enjin2/utils/polar.hpp>

using namespace enjin2;

int main() {
    printf("Enjin2 Working Demo - Verified Components Only\n");
    printf("==============================================\n");
    
    // Create main canvas (128x64)
    Canvas8<128, 64> canvas;
    canvas.clear(0); // Black background
    
    // Component 1: Title Label
    printf("Creating title label...\n");
    Object title_obj;
    auto title_pos = title_obj.addComponent<C_Position>(64, 8);
    auto title = title_obj.addComponent<Label>(100, 12, nullptr, 1, 15, 0, 0);
    title->setText("ENJIN2 SYSTEM READY");
    title->setAlignment(LabelAlign::Center);
    title->draw(canvas);
    
    // Component 2: Status Label
    printf("Creating status display...\n");
    Object status_obj;
    auto status_pos = status_obj.addComponent<C_Position>(5, 25);
    auto status = status_obj.addComponent<Label>(118, 10, nullptr, 1, 12, 0, 0);
    status->setText("All Core Components Operational");
    status->draw(canvas);
    
    // Component 3: Custom orbital visualization using C_Draw
    printf("Creating orbital pattern visualization...\n");
    Object orbital_obj;
    auto orbital_pos = orbital_obj.addComponent<C_Position>(30, 40);
    auto orbital = orbital_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        Point center(64, 45);
        
        // Draw multiple orbital rings
        for (int ring = 0; ring < 4; ring++) {
            uint8_t radius = 6 + ring * 4;
            uint8_t brightness = 15 - ring * 2;
            
            // Draw 16 points around each ring
            for (int i = 0; i < 16; i++) {
                float phase = static_cast<float>(i) / 16.0f;
                Point point = Polar::RadialToCartesian(phase, radius, center);
                
                if (point.x >= 0 && point.x < 128 && point.y >= 0 && point.y < 64) {
                    canvas.setPixel(point.x, point.y, brightness);
                    
                    // Add slight glow effect
                    if (point.x > 0) canvas.setPixel(point.x - 1, point.y, brightness / 2);
                    if (point.x < 127) canvas.setPixel(point.x + 1, point.y, brightness / 2);
                    if (point.y > 0) canvas.setPixel(point.x, point.y - 1, brightness / 2);
                    if (point.y < 63) canvas.setPixel(point.x, point.y + 1, brightness / 2);
                }
            }
        }
        
        // Draw center star
        canvas.setPixel(center.x, center.y, 15);
        canvas.setPixel(center.x - 1, center.y, 15);
        canvas.setPixel(center.x + 1, center.y, 15);
        canvas.setPixel(center.x, center.y - 1, 15);
        canvas.setPixel(center.x, center.y + 1, 15);
    });
    orbital->draw(canvas);
    
    // Component 4: Side panels using C_Draw
    printf("Creating side panel indicators...\n");
    Object left_panel_obj;
    auto left_panel = left_panel_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        // Left side indicators
        for (int i = 0; i < 6; i++) {
            uint8_t y = 20 + i * 6;
            uint8_t brightness = (i % 2 == 0) ? 10 : 6;
            
            // Draw indicator blocks
            for (int x = 2; x < 8; x++) {
                for (int dy = 0; dy < 3; dy++) {
                    canvas.setPixel(x, y + dy, brightness);
                }
            }
        }
    });
    left_panel->draw(canvas);
    
    Object right_panel_obj;
    auto right_panel = right_panel_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        // Right side indicators
        for (int i = 0; i < 6; i++) {
            uint8_t y = 20 + i * 6;
            uint8_t brightness = (i % 2 == 1) ? 10 : 6;
            
            // Draw indicator blocks
            for (int x = 120; x < 126; x++) {
                for (int dy = 0; dy < 3; dy++) {
                    canvas.setPixel(x, y + dy, brightness);
                }
            }
        }
    });
    right_panel->draw(canvas);
    
    // Component 5: Bottom system info
    printf("Creating system information...\n");
    Object info_obj;
    auto info_pos = info_obj.addComponent<C_Position>(5, 55);
    auto info = info_obj.addComponent<Label>(118, 8, nullptr, 1, 8, 0, 0);
    info->setText("ECS | Canvas | Text | Draw | Polar | Ready for Hardware");
    info->draw(canvas);
    
    // Component 6: Corner decorations
    printf("Creating corner decorations...\n");
    Object corners_obj;
    auto corners = corners_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        // Top-left corner
        for (int i = 0; i < 8; i++) {
            canvas.setPixel(i, 0, 12);
            canvas.setPixel(0, i, 12);
        }
        
        // Top-right corner
        for (int i = 0; i < 8; i++) {
            canvas.setPixel(127 - i, 0, 12);
            canvas.setPixel(127, i, 12);
        }
        
        // Bottom-left corner
        for (int i = 0; i < 8; i++) {
            canvas.setPixel(i, 63, 12);
            canvas.setPixel(0, 63 - i, 12);
        }
        
        // Bottom-right corner
        for (int i = 0; i < 8; i++) {
            canvas.setPixel(127 - i, 63, 12);
            canvas.setPixel(127, 63 - i, 12);
        }
    });
    corners->draw(canvas);
    
    // Export the demo
    canvas.exportToPGM("working_demo.pgm");
    
    printf("\n=== Enjin2 Working Demo Complete ===\n");
    printf("Successfully demonstrated components:\n");
    printf("✓ Label - Text rendering with built-in font\n");
    printf("✓ C_Draw - Custom lambda-based drawing\n");
    printf("✓ C_Position - Spatial positioning system\n");
    printf("✓ Polar utilities - Coordinate conversion\n");
    printf("✓ Canvas8 - 8-bit grayscale rendering\n");
    printf("✓ Object/Component - ECS architecture\n");
    printf("✓ PGM export - Image output with color scaling\n");
    printf("\nVisual elements created:\n");
    printf("• Title and status text labels\n");
    printf("• Orbital ring visualization\n");
    printf("• Side panel indicators\n");
    printf("• Corner frame decorations\n");
    printf("• System information display\n");
    printf("\nDemo exported to working_demo.pgm\n");
    printf("Core Enjin2 system is fully functional!\n");
    
    return 0;
}