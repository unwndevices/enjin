#pragma once

#include "canvas.hpp"
#include "../core/types.hpp"
#include <string>
#include <fstream>
#include <iostream>

namespace enjin2 {

/**
 * @brief Simple image export utilities for visualizing canvas output
 * 
 * Provides basic image export functionality to visualize the canvas
 * content as actual images rather than ASCII output.
 */
class ImageExporter {
public:
    /**
     * @brief Export 4-bit canvas to PGM (Portable Graymap) format
     * @param canvas Canvas to export
     * @param filename Output filename
     * @param scale Scale factor for output image (1 = 1:1, 2 = 2x size, etc.)
     * @return True if export succeeded
     */
    template<uint16_t WIDTH, uint16_t HEIGHT>
    static bool exportToPGM(const Canvas4<WIDTH, HEIGHT>& canvas, const std::string& filename, int scale = 4) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        int outputWidth = WIDTH * scale;
        int outputHeight = HEIGHT * scale;
        
        // PGM header
        file << "P2\n";
        file << outputWidth << " " << outputHeight << "\n";
        file << "15\n";  // Max value (4-bit = 0-15)
        
        // Write pixel data
        for (int y = 0; y < outputHeight; ++y) {
            for (int x = 0; x < outputWidth; ++x) {
                // Scale down to original canvas coordinates
                int canvasX = x / scale;
                int canvasY = y / scale;
                
                Pixel4 pixel = canvas.getPixel(canvasX, canvasY);
                file << static_cast<int>(pixel.value);
                
                if (x < outputWidth - 1) file << " ";
            }
            file << "\n";
        }
        
        file.close();
        return true;
    }
    
    /**
     * @brief Export 8-bit canvas to PGM format
     * @param canvas Canvas to export
     * @param filename Output filename
     * @param scale Scale factor for output image
     * @return True if export succeeded
     */
    template<uint16_t WIDTH, uint16_t HEIGHT>
    static bool exportToPGM(const Canvas8<WIDTH, HEIGHT>& canvas, const std::string& filename, int scale = 4) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        int outputWidth = WIDTH * scale;
        int outputHeight = HEIGHT * scale;
        
        // PGM header
        file << "P2\n";
        file << outputWidth << " " << outputHeight << "\n";
        file << "255\n";  // Max value (8-bit = 0-255)
        
        // Write pixel data
        for (int y = 0; y < outputHeight; ++y) {
            for (int x = 0; x < outputWidth; ++x) {
                int canvasX = x / scale;
                int canvasY = y / scale;
                
                uint8_t pixel = canvas.getPixel(canvasX, canvasY);
                file << static_cast<int>(pixel);
                
                if (x < outputWidth - 1) file << " ";
            }
            file << "\n";
        }
        
        file.close();
        return true;
    }
    
    /**
     * @brief Export 4-bit canvas to simple visual ASCII format (better than before)
     * @param canvas Canvas to export
     * @param title Title to display above the visualization
     */
    template<uint16_t WIDTH, uint16_t HEIGHT>
    static void printVisual(const Canvas4<WIDTH, HEIGHT>& canvas, const std::string& title = "") {
        if (!title.empty()) {
            std::cout << title << "\n";
        }
        
        // Print border
        std::cout << "+";
        for (int x = 0; x < WIDTH; ++x) std::cout << "-";
        std::cout << "+\n";
        
        // Print canvas content
        for (int y = 0; y < HEIGHT; ++y) {
            std::cout << "|";
            for (int x = 0; x < WIDTH; ++x) {
                Pixel4 pixel = canvas.getPixel(x, y);
                
                // Use different characters for different intensity levels
                if (pixel.value == 0) {
                    std::cout << " ";          // Black/empty
                } else if (pixel.value <= 2) {
                    std::cout << "·";          // Very dim
                } else if (pixel.value <= 5) {
                    std::cout << "░";          // Dim
                } else if (pixel.value <= 8) {
                    std::cout << "▒";          // Medium
                } else if (pixel.value <= 12) {
                    std::cout << "▓";          // Bright
                } else {
                    std::cout << "█";          // Very bright
                }
            }
            std::cout << "|\n";
        }
        
        // Print border
        std::cout << "+";
        for (int x = 0; x < WIDTH; ++x) std::cout << "-";
        std::cout << "+\n";
    }
    
    /**
     * @brief Create a color-coded terminal visualization using ANSI colors
     * @param canvas Canvas to display
     * @param title Title to display
     */
    template<uint16_t WIDTH, uint16_t HEIGHT>
    static void printColorVisual(const Canvas4<WIDTH, HEIGHT>& canvas, const std::string& title = "") {
        if (!title.empty()) {
            std::cout << "\033[1;37m" << title << "\033[0m\n";  // Bold white title
        }
        
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                Pixel4 pixel = canvas.getPixel(x, y);
                
                // Use ANSI color codes based on pixel intensity
                if (pixel.value == 0) {
                    std::cout << " ";                                    // Black
                } else if (pixel.value <= 3) {
                    std::cout << "\033[38;5;232m▓\033[0m";              // Very dark gray
                } else if (pixel.value <= 6) {
                    std::cout << "\033[38;5;240m▓\033[0m";              // Dark gray
                } else if (pixel.value <= 9) {
                    std::cout << "\033[38;5;248m▓\033[0m";              // Medium gray
                } else if (pixel.value <= 12) {
                    std::cout << "\033[38;5;255m▓\033[0m";              // Light gray
                } else {
                    std::cout << "\033[38;5;15m█\033[0m";               // White
                }
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
};

} // namespace enjin2