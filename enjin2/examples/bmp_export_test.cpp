/**
 * @file bmp_export_test.cpp
 * @brief Simple test for Canvas8::exportToBMP() functionality
 *
 * Tests that BMP export works correctly by creating a simple image
 * and exporting it to BMP format.
 */

#include <iostream>
#include "../include/enjin2/graphics/canvas.hpp"

using namespace enjin2;

int main() {
    std::cout << "=== BMP Export Test ===" << std::endl;

    // Create a 128x64 Canvas8
    Canvas8<128, 64> canvas;

    // Draw a simple test pattern
    // Background: dark gray
    canvas.clear(30);

    // Draw a white rectangle in the center
    for (int y = 20; y < 44; ++y) {
        for (int x = 40; x < 88; ++x) {
            canvas.setPixel(x, y, 200);
        }
    }

    // Draw a circle
    int cx = 64, cy = 32, r = 10;
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            if (x*x + y*y <= r*r) {
                canvas.setPixel(cx + x, cy + y, 255);
            }
        }
    }

    // Export to BMP
    const char* bmpFile = "test_output.bmp";
    std::cout << "Exporting canvas to BMP: " << bmpFile << std::endl;
    canvas.exportToBMP(bmpFile);

    // Export to PGM for comparison
    const char* pgmFile = "test_output.pgm";
    std::cout << "Exporting canvas to PGM: " << pgmFile << std::endl;
    canvas.exportToPGM(pgmFile);

    std::cout << "\nExport complete!" << std::endl;
    std::cout << "Generated files:" << std::endl;
    std::cout << "  - " << bmpFile << " (24-bit RGB BMP)" << std::endl;
    std::cout << "  - " << pgmFile << " (8-bit grayscale PGM)" << std::endl;
    std::cout << "\nYou can open these files with an image viewer." << std::endl;

    return 0;
}
