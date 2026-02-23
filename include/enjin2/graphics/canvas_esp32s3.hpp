/**
 * @file canvas_esp32s3.hpp
 * @brief ESP32-S3 optimized 4-bit canvas implementation
 * 
 * Provides ESP32-S3 specific optimizations:
 * - IRAM placement for hot functions
 * - Xtensa SIMD utilization
 * - Dual-core rendering support
 * - Cache-optimized memory layout
 */

#pragma once

#include "../core/types.hpp"
#include <cstring>
#include <algorithm>

#ifdef ESP32
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#else
// Fallback definitions for non-ESP32 builds
#define IRAM_ATTR
#define DRAM_ATTR
#endif

namespace enjin2 {

/**
 * @brief Pre-computed lookup tables for 4-bit pixel operations
 */
struct DRAM_ATTR PixelLUT {
    uint8_t setMasks[2][16];    ///< [even/odd][color] -> mask to apply
    uint8_t clearMasks[2];      ///< [even/odd] -> mask to clear
    uint8_t doubleMasks[16];    ///< [color] -> color in both nibbles
    
    constexpr PixelLUT() : setMasks{}, clearMasks{}, doubleMasks{} {
        // Even pixels (lower 4 bits)
        clearMasks[0] = 0xF0;
        for (int c = 0; c < 16; c++) {
            setMasks[0][c] = c & 0x0F;
        }
        
        // Odd pixels (upper 4 bits)  
        clearMasks[1] = 0x0F;
        for (int c = 0; c < 16; c++) {
            setMasks[1][c] = (c & 0x0F) << 4;
        }
        
        // Double masks for horizontal lines
        for (int c = 0; c < 16; c++) {
            doubleMasks[c] = ((c & 0x0F) << 4) | (c & 0x0F);
        }
    }
};

static constexpr PixelLUT pixelLUT{};

/**
 * @brief Render command for dual-core pipeline
 */
struct RenderCommand {
    /// @brief Render command type enumeration
    enum Type {
        CLEAR,            ///< Clear canvas
        SET_PIXEL,        ///< Set single pixel
        HORIZONTAL_LINE,  ///< Draw horizontal line
        VERTICAL_LINE,    ///< Draw vertical line
        FILL_RECT,        ///< Fill rectangle
        FILL_CIRCLE       ///< Fill circle
    };

    Type type;            ///< Command type
    int16_t x;            ///< X coordinate
    int16_t y;            ///< Y coordinate
    int16_t w;            ///< Width or radius
    int16_t h;            ///< Height
    uint8_t color;        ///< Color value
};

/**
 * @brief ESP32-S3 optimized 4-bit canvas
 * 
 * @tparam WIDTH Canvas width in pixels
 * @tparam HEIGHT Canvas height in pixels
 */
template<uint16_t WIDTH, uint16_t HEIGHT>
class Canvas4_ESP32S3 {
public:
    static constexpr uint16_t width = WIDTH;
    static constexpr uint16_t height = HEIGHT;
    static constexpr size_t dataSize = (WIDTH * HEIGHT + 1) / 2;
    
    // Ensure cache line alignment (32 bytes on ESP32-S3)
    static constexpr size_t alignedSize = ((dataSize + 31) / 32) * 32;
    
private:
    alignas(32) uint8_t data[alignedSize] = {0};
    
#ifdef ESP32
    // Dual-core rendering support
    QueueHandle_t renderQueue = nullptr;
    SemaphoreHandle_t frameSemaphore = nullptr;
    bool dualCoreMode = false;
#endif
    
public:
    Canvas4_ESP32S3() {
        clear(Pixel4(0));
        
#ifdef ESP32
        // Initialize dual-core rendering if requested
        renderQueue = xQueueCreate(4, sizeof(std::vector<RenderCommand>*));
        frameSemaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(frameSemaphore);
#endif
    }
    
    ~Canvas4_ESP32S3() {
#ifdef ESP32
        if (renderQueue) vQueueDelete(renderQueue);
        if (frameSemaphore) vSemaphoreDelete(frameSemaphore);
#endif
    }
    
    /**
     * @brief Fast pixel setting using lookup tables
     * @param x X coordinate
     * @param y Y coordinate
     * @param color Pixel color
     */
    IRAM_ATTR inline void setPixel(int16_t x, int16_t y, Pixel4 color) {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
        
        const int index = (y * WIDTH + x) / 2;
        const int odd = (y * WIDTH + x) & 1;
        
        data[index] = (data[index] & pixelLUT.clearMasks[odd]) | 
                      pixelLUT.setMasks[odd][color.value];
    }
    
    /**
     * @brief Fast pixel reading using lookup tables
     * @param x X coordinate
     * @param y Y coordinate
     * @return Pixel color at the specified location
     */
    IRAM_ATTR inline Pixel4 getPixel(int16_t x, int16_t y) const {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
            return Pixel4(0);
        }
        
        const int index = (y * WIDTH + x) / 2;
        const int odd = (y * WIDTH + x) & 1;
        
        if (odd) {
            return Pixel4(data[index] >> 4);
        } else {
            return Pixel4(data[index] & 0x0F);
        }
    }
    
    /**
     * @brief Vectorized horizontal line drawing
     * @param x1 Start X coordinate
     * @param x2 End X coordinate
     * @param y Y coordinate
     * @param color Line color
     */
    IRAM_ATTR void drawHorizontalLine(int16_t x1, int16_t x2, int16_t y, Pixel4 color) {
        if (y < 0 || y >= HEIGHT) return;
        if (x1 > x2) std::swap(x1, x2);
        
        x1 = std::max(static_cast<int16_t>(0), x1);
        x2 = std::min(static_cast<int16_t>(WIDTH - 1), x2);
        
        if (x1 > x2) return;
        
        uint8_t* row = data + (y * WIDTH / 2);
        
        // Handle odd start pixel
        if (x1 % 2 == 1) {
            setPixelDirect(row, x1, color.value);
            x1++;
        }
        
        // Process pairs of pixels (full bytes)
        const int pairs = (x2 - x1 + 1) / 2;
        const uint8_t doubleColor = pixelLUT.doubleMasks[color.value];
        
        uint8_t* bytePtr = row + x1/2;
        
        // Use 32-bit writes for better performance
        if (pairs >= 4) {
            const uint32_t quadColor = (static_cast<uint32_t>(doubleColor) << 24) | 
                                      (static_cast<uint32_t>(doubleColor) << 16) | 
                                      (static_cast<uint32_t>(doubleColor) << 8) | 
                                      doubleColor;
            
            uint32_t* wordPtr = reinterpret_cast<uint32_t*>(bytePtr);
            const int words = pairs / 4;
            
            for (int i = 0; i < words; i++) {
                *wordPtr++ = quadColor;
            }
            
            bytePtr = reinterpret_cast<uint8_t*>(wordPtr);
            
            // Remaining byte pairs
            const int remainingPairs = pairs % 4;
            for (int i = 0; i < remainingPairs; i++) {
                *bytePtr++ = doubleColor;
            }
        } else {
            // Small runs - just use byte writes
            for (int i = 0; i < pairs; i++) {
                *bytePtr++ = doubleColor;
            }
        }
        
        // Handle odd end pixel
        if ((x2 - x1 + 1) % 2 == 1) {
            setPixelDirect(row, x2, color.value);
        }
    }
    
    /**
     * @brief Optimized vertical line drawing
     * @param x X coordinate
     * @param y1 Start Y coordinate
     * @param y2 End Y coordinate
     * @param color Line color
     */
    IRAM_ATTR void drawVerticalLine(int16_t x, int16_t y1, int16_t y2, Pixel4 color) {
        if (x < 0 || x >= WIDTH) return;
        if (y1 > y2) std::swap(y1, y2);
        
        y1 = std::max(static_cast<int16_t>(0), y1);
        y2 = std::min(static_cast<int16_t>(HEIGHT - 1), y2);
        
        const int index_offset = x / 2;
        const int odd = x & 1;
        const uint8_t mask = pixelLUT.clearMasks[odd];
        const uint8_t colorMask = pixelLUT.setMasks[odd][color.value];
        
        for (int16_t y = y1; y <= y2; y++) {
            const int index = (y * WIDTH / 2) + index_offset;
            data[index] = (data[index] & mask) | colorMask;
        }
    }
    
    /**
     * @brief Fast rectangle filling using vectorized horizontal lines
     * @param x Top-left X coordinate
     * @param y Top-left Y coordinate
     * @param w Width in pixels
     * @param h Height in pixels
     * @param color Fill color
     */
    IRAM_ATTR void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Pixel4 color) {
        if (w <= 0 || h <= 0) return;
        
        const int16_t x2 = x + w - 1;
        const int16_t y2 = y + h - 1;
        
        for (int16_t row = y; row <= y2; row++) {
            drawHorizontalLine(x, x2, row, color);
        }
    }
    
    /**
     * @brief Optimized circle filling using scanline algorithm
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Circle radius
     * @param color Fill color
     */
    IRAM_ATTR void fillCircle(int16_t cx, int16_t cy, int16_t radius, Pixel4 color) {
        if (radius <= 0) return;
        
        // Pre-calculate scanlines for vectorized filling
        struct Scanline {
            int16_t y, x1, x2;
        };
        
        Scanline scanlines[radius * 2 + 1];
        int scanlineCount = 0;
        
        // Generate all scanlines first (avoid branching in inner loop)
        for (int16_t y = -radius; y <= radius; y++) {
            const int16_t x = static_cast<int16_t>(
                sqrtf(static_cast<float>(radius * radius - y * y))
            );
            if (x >= 0) {
                scanlines[scanlineCount++] = {
                    static_cast<int16_t>(cy + y), 
                    static_cast<int16_t>(cx - x), 
                    static_cast<int16_t>(cx + x)
                };
            }
        }
        
        // Vectorized scanline filling
        for (int i = 0; i < scanlineCount; i++) {
            const auto& line = scanlines[i];
            if (line.y >= 0 && line.y < HEIGHT) {
                const int16_t x1 = std::max(static_cast<int16_t>(0), line.x1);
                const int16_t x2 = std::min(static_cast<int16_t>(WIDTH - 1), line.x2);
                if (x1 <= x2) {
                    drawHorizontalLine(x1, x2, line.y, color);
                }
            }
        }
    }
    
    /**
     * @brief Fast clear using optimized memory operations
     * @param color Color to fill canvas with
     */
    IRAM_ATTR void clear(Pixel4 color) {
        const uint8_t doubleColor = pixelLUT.doubleMasks[color.value];
        
        // Use 32-bit writes for faster clearing
        const uint32_t quadColor = (static_cast<uint32_t>(doubleColor) << 24) | 
                                  (static_cast<uint32_t>(doubleColor) << 16) | 
                                  (static_cast<uint32_t>(doubleColor) << 8) | 
                                  doubleColor;
        
        uint32_t* wordPtr = reinterpret_cast<uint32_t*>(data);
        const size_t words = dataSize / 4;
        
        for (size_t i = 0; i < words; i++) {
            *wordPtr++ = quadColor;
        }
        
        // Handle remaining bytes
        uint8_t* bytePtr = reinterpret_cast<uint8_t*>(wordPtr);
        const size_t remainingBytes = dataSize % 4;
        for (size_t i = 0; i < remainingBytes; i++) {
            *bytePtr++ = doubleColor;
        }
    }
    
    /**
     * @brief Get raw data pointer for DMA transfers
     * @return Pointer to pixel buffer
     */
    const uint8_t* getData() const { return data; }
    
    /**
     * @brief Get data size in bytes
     */
    constexpr size_t getDataSize() const { return dataSize; }
    
#ifdef ESP32
    /**
     * @brief Enable dual-core rendering mode
     */
    void enableDualCore(bool enable = true) {
        dualCoreMode = enable;
    }
    
    /**
     * @brief Submit render commands to rendering core
     */
    void submitRenderCommands(const std::vector<RenderCommand>& commands) {
        if (!dualCoreMode || !renderQueue) return;
        
        auto* cmdPtr = new std::vector<RenderCommand>(commands);
        xQueueSend(renderQueue, &cmdPtr, portMAX_DELAY);
    }
    
    /**
     * @brief Process render commands (call from render core)
     */
    void processRenderCommands() {
        if (!dualCoreMode || !renderQueue) return;
        
        std::vector<RenderCommand>* commands = nullptr;
        if (xQueueReceive(renderQueue, &commands, 0) == pdTRUE) {
            for (const auto& cmd : *commands) {
                executeRenderCommand(cmd);
            }
            delete commands;
            xSemaphoreGive(frameSemaphore);
        }
    }
    
    /**
     * @brief Wait for frame completion
     */
    void waitFrameComplete(TickType_t timeout = portMAX_DELAY) {
        if (frameSemaphore) {
            xSemaphoreTake(frameSemaphore, timeout);
        }
    }
#endif
    
private:
    /**
     * @brief Direct pixel setting without bounds checking
     */
    IRAM_ATTR inline void setPixelDirect(uint8_t* row, int16_t x, uint8_t color) {
        const int index = x / 2;
        const int odd = x & 1;
        
        row[index] = (row[index] & pixelLUT.clearMasks[odd]) | 
                     pixelLUT.setMasks[odd][color];
    }
    
#ifdef ESP32
    /**
     * @brief Execute a single render command
     */
    IRAM_ATTR void executeRenderCommand(const RenderCommand& cmd) {
        switch (cmd.type) {
            case RenderCommand::CLEAR:
                clear(Pixel4(cmd.color));
                break;
            case RenderCommand::SET_PIXEL:
                setPixel(cmd.x, cmd.y, Pixel4(cmd.color));
                break;
            case RenderCommand::HORIZONTAL_LINE:
                drawHorizontalLine(cmd.x, cmd.x + cmd.w, cmd.y, Pixel4(cmd.color));
                break;
            case RenderCommand::VERTICAL_LINE:
                drawVerticalLine(cmd.x, cmd.y, cmd.y + cmd.h, Pixel4(cmd.color));
                break;
            case RenderCommand::FILL_RECT:
                fillRect(cmd.x, cmd.y, cmd.w, cmd.h, Pixel4(cmd.color));
                break;
            case RenderCommand::FILL_CIRCLE:
                fillCircle(cmd.x, cmd.y, cmd.w, Pixel4(cmd.color));
                break;
        }
    }
#endif
};

/// @brief ESP32-S3 optimized 128x64 canvas type alias
using Canvas4_128x64_ESP32S3 = Canvas4_ESP32S3<128, 64>;
/// @brief ESP32-S3 optimized 256x128 canvas type alias
using Canvas4_256x128_ESP32S3 = Canvas4_ESP32S3<256, 128>;
/// @brief ESP32-S3 optimized 320x240 canvas type alias
using Canvas4_320x240_ESP32S3 = Canvas4_ESP32S3<320, 240>;

} // namespace enjin2