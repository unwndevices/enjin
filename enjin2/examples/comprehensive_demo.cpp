#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/position.hpp"
#include "../include/enjin2/components/slider.hpp"
#include "../include/enjin2/components/button_dial.hpp"
#include "../include/enjin2/components/fill_up_gauge.hpp"
#include "../include/enjin2/components/tickmarks.hpp"
#include "../include/enjin2/components/label.hpp"
#include "../include/enjin2/components/draw.hpp"
#include "../include/enjin2/utils/polar.hpp"

using namespace enjin2;

int main() {
    printf("Enjin2 Comprehensive Demo - All Components\n");
    printf("==========================================\n");
    
    // Create main canvas (128x64 to fit all components)
    Canvas8<128, 64> canvas;
    canvas.clear(0); // Black background
    
    // Component 1: Slider (top-left)
    printf("Creating Slider component...\n");
    Object slider_obj;
    auto slider_pos = slider_obj.addComponent<C_Position>(5, 5);
    auto slider = slider_obj.addComponent<Slider>(30, 8, 0.7f);
    slider->draw(canvas);
    
    // Component 2: ButtonDial (top-center)
    printf("Creating ButtonDial component...\n");
    Object dial_obj;
    auto dial_pos = dial_obj.addComponent<C_Position>(50, 10);
    auto dial = dial_obj.addComponent<ButtonDial>(12, 6, 0.3f);
    dial->draw(canvas);
    
    // Component 3: FillUpGauge (top-right)  
    printf("Creating FillUpGauge component...\n");
    Object gauge_obj;
    auto gauge_pos = gauge_obj.addComponent<C_Position>(90, 5);
    auto gauge = gauge_obj.addComponent<FillUpGauge>(25, 12, 0.6f, GaugeMode::Unidirectional);
    gauge->draw(canvas);
    
    // Component 4: Tickmarks (left side)
    printf("Creating Tickmarks component...\n");
    Object ticks_obj;
    auto ticks_pos = ticks_obj.addComponent<C_Position>(8, 25);
    auto ticks = ticks_obj.addComponent<Tickmarks>(15, 8, 0.0f, 1.0f, 0.1f);
    ticks->draw(canvas);
    
    // Component 5: Label with text (center)
    printf("Creating Label component...\n");
    Object label_obj;
    auto label_pos = label_obj.addComponent<C_Position>(35, 25);
    auto label = label_obj.addComponent<Label>(50, 12, nullptr, 1, 15, 2, 0);
    label->setText("Enjin2 Demo");
    label->draw(canvas);
    
    // Component 6: Custom drawing with C_Draw (orbital pattern)
    printf("Creating custom orbital pattern...\n");
    Object orbital_obj;
    auto orbital_pos = orbital_obj.addComponent<C_Position>(95, 35);
    auto orbital = orbital_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        Point center(95, 35);
        
        // Draw orbital rings using polar coordinates
        for (int ring = 0; ring < 3; ring++) {
            uint8_t radius = 8 + ring * 6;
            uint8_t color = 10 + ring * 2;
            
            for (int i = 0; i < 12; i++) {
                float phase = static_cast<float>(i) / 12.0f;
                Point point = Polar::RadialToCartesian(phase, radius, center);
                
                if (point.x >= 0 && point.x < 128 && point.y >= 0 && point.y < 64) {
                    canvas.setPixel(point.x, point.y, color);
                }
            }
        }
        
        // Draw center point
        canvas.setPixel(center.x, center.y, 15);
    });
    orbital->draw(canvas);
    
    // Component 7: Bottom status display
    printf("Creating status display...\n");
    Object status_obj;
    auto status_pos = status_obj.addComponent<C_Position>(5, 50);
    auto status_label = status_obj.addComponent<Label>(118, 10, nullptr, 1, 12, 0, 0);
    status_label->setText("All Components Working | Enjin2 v2.0 | Hardware Ready");
    status_label->draw(canvas);
    
    // Component 8: Animated indicator using Draw
    printf("Creating animated indicator...\n");
    Object indicator_obj;
    auto indicator_pos = indicator_obj.addComponent<C_Position>(10, 40);
    auto indicator = indicator_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        // Create a pulsing dot pattern
        Point center(15, 42);
        
        for (int i = 0; i < 4; i++) {
            uint8_t brightness = 15 - i * 3;
            canvas.setPixel(center.x + i, center.y, brightness);
            canvas.setPixel(center.x - i, center.y, brightness);
            canvas.setPixel(center.x, center.y + i, brightness);
            canvas.setPixel(center.x, center.y - i, brightness);
        }
    });
    indicator->draw(canvas);
    
    // Export the comprehensive demo
    canvas.exportToPGM("comprehensive_demo.pgm");
    
    printf("\n=== Enjin2 Comprehensive Demo Complete ===\n");
    printf("Components successfully demonstrated:\n");
    printf("✓ Slider - Parameter control interface\n");
    printf("✓ ButtonDial - Discrete value selection\n");
    printf("✓ FillUpGauge - Level/progress indication\n");
    printf("✓ Tickmarks - Measurement scales\n");
    printf("✓ Label - Text rendering with built-in font\n");
    printf("✓ C_Draw - Custom lambda-based drawing\n");
    printf("✓ Polar utilities - Coordinate conversion\n");
    printf("✓ Canvas system - 8-bit grayscale rendering\n");
    printf("✓ Component system - ECS architecture\n");
    printf("✓ Position system - Spatial management\n");
    printf("\nDemo exported to comprehensive_demo.pgm\n");
    printf("Ready for hardware integration!\n");
    
    return 0;
}