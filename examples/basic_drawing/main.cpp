#include <enjin2/core/types.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <iostream>

using namespace enjin2;

// Simple ASCII renderer for demonstration
void printCanvas(const Canvas4<32, 16>& canvas) {
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 32; ++x) {
            Pixel4 pixel = canvas.getPixel(x, y);
            char c;
            if (pixel.value == 0) c = ' ';
            else if (pixel.value < 4) c = '.';
            else if (pixel.value < 8) c = '+';
            else if (pixel.value < 12) c = '#';
            else c = '@';
            std::cout << c;
        }
        std::cout << '\n';
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "Enjin 2.0 Graphics Library Demo\n";
    std::cout << "================================\n\n";
    
    // Create a small 4-bit canvas
    Canvas4<32, 16> canvas;
    
    // Test 1: Basic shapes
    std::cout << "Test 1: Basic shapes\n";
    canvas.clear(Colors::BLACK);
    
    // Draw some primitives
    Primitives4::drawRect(canvas, Rect(2, 2, 8, 6), Colors::WHITE);
    Primitives4::fillRect(canvas, Rect(12, 2, 6, 4), Colors::GRAY);
    Primitives4::drawCircle(canvas, 25, 8, 4, Colors::LIGHT_GRAY);
    Primitives4::drawLine(canvas, 0, 0, 31, 15, Colors::DARK_GRAY);
    
    printCanvas(canvas);
    
    // Test 2: Filled shapes
    std::cout << "Test 2: Filled shapes\n";
    canvas.clear(Colors::BLACK);
    
    Primitives4::fillCircle(canvas, 8, 8, 6, Colors::WHITE);
    Primitives4::fillTriangle(canvas, 16, 2, 20, 14, 24, 2, Colors::GRAY);
    
    printCanvas(canvas);
    
    // Test 3: 4-bit packed storage efficiency
    std::cout << "Test 3: Memory efficiency\n";
    std::cout << "Canvas size: 32x16 = 512 pixels\n";
    std::cout << "4-bit storage: " << canvas.getBufferSize() << " bytes (2 pixels/byte)\n";
    std::cout << "8-bit equivalent: " << (32 * 16) << " bytes\n";
    std::cout << "Space savings: " << (100 - (canvas.getBufferSize() * 100) / (32 * 16)) << "%\n\n";
    
    // Test 4: Pixel format conversion
    std::cout << "Test 4: Pixel format conversion\n";
    Canvas8<32, 16> canvas8;
    canvas8.clear(128); // Mid-gray in 8-bit
    Primitives8::fillCircle(canvas8, 16, 8, 5, 255);
    
    Canvas4<32, 16> converted;
    canvas8.convertTo4bit(converted);
    
    std::cout << "Converted from 8-bit to 4-bit:\n";
    printCanvas(converted);
    
    return 0;
}