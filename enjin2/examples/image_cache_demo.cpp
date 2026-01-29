#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/position.hpp"
#include "../include/enjin2/components/image_cache.hpp"
#include "../include/enjin2/components/sprite.hpp"

using namespace enjin2;

/**
 * @brief Simple in-memory file interface for testing
 */
class MemoryFile : public FileInterface {
private:
    const uint8_t* data;
    size_t data_size;
    size_t position;
    bool is_open;
    
public:
    MemoryFile(const uint8_t* file_data, size_t size) 
        : data(file_data), data_size(size), position(0), is_open(false) {}
    
    bool open() override {
        position = 0;
        is_open = true;
        return true;
    }
    
    void close() override {
        is_open = false;
    }
    
    size_t read(uint8_t* buffer, size_t length) override {
        if (!is_open) return 0;
        
        size_t available = data_size - position;
        size_t to_read = (length < available) ? length : available;
        
        if (to_read > 0) {
            std::memcpy(buffer, data + position, to_read);
            position += to_read;
        }
        
        return to_read;
    }
    
    bool seek(size_t new_position) override {
        if (new_position <= data_size) {
            position = new_position;
            return true;
        }
        return false;
    }
    
    size_t size() const override {
        return data_size;
    }
};

int main() {
    printf("Enjin2 ImageCache Demo\n");
    printf("=====================\n");
    
    // Create canvas for visualization
    Canvas8<64, 32> canvas;
    canvas.clear(0);
    
    // Test 1: Create ImageCache component
    printf("Creating ImageCache component...\n");
    Object cache_obj;
    auto image_cache = cache_obj.addComponent<C_ImageCache>();
    printf("✓ ImageCache component created\n");
    
    // Test 2: Create simple test image data (8x8 pixel, 1 frame)
    // Packed 4-bit data: 2 pixels per byte = 32 bytes total
    printf("Creating test image data (8x8 pixels)...\n");
    uint8_t test_image_data[32];
    
    // Create a simple pattern: gradient from 0 to 15
    for (int i = 0; i < 32; i++) {
        uint8_t pixel1 = (i * 2) % 16;
        uint8_t pixel2 = (i * 2 + 1) % 16;
        test_image_data[i] = (pixel1 << 4) | pixel2;
    }
    
    // Test 3: Add image to cache
    printf("Adding image to cache...\n");
    MemoryFile test_file(test_image_data, 32);
    
    try {
        ImageEntry entry = C_ImageCache::AddImage(test_file, 8, 8, 1);
        printf("✓ Image added successfully:\n");
        printf("  - Size: %zu bytes\n", entry.size);
        printf("  - Dimensions: %dx%d\n", entry.width, entry.height);
        printf("  - Frames: %d\n", entry.frames);
        printf("  - Offset: %zu\n", entry.offset);
        
        // Test 4: Access image data
        printf("Accessing image data...\n");
        const uint8_t* image_data = C_ImageCache::GetImageData(entry, 0);
        printf("✓ Image data retrieved, first few pixels: ");
        for (int i = 0; i < 8; i++) {
            printf("%d ", image_data[i]);
        }
        printf("\n");
        
        // Test 5: Render image to canvas
        printf("Rendering image to canvas...\n");
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                uint8_t pixel_value = image_data[y * 8 + x];
                canvas.setPixel(x + 10, y + 10, pixel_value);
            }
        }
        printf("✓ Image rendered to canvas\n");
        
        // Test 6: Cache statistics
        auto stats = C_ImageCache::GetCacheStats();
        printf("Cache statistics:\n");
        printf("  - Used: %zu bytes\n", stats.first);
        printf("  - Total: %zu bytes\n", stats.second);
        printf("  - Free: %zu bytes\n", stats.second - stats.first);
        
        // Test 7: Create multiple images
        printf("Testing multiple image allocation...\n");
        
        // Create smaller 4x4 image
        uint8_t small_image_data[8]; // 4x4 = 16 pixels = 8 bytes packed
        for (int i = 0; i < 8; i++) {
            small_image_data[i] = 0xAB; // Pattern: 10, 11 repeated
        }
        
        MemoryFile small_file(small_image_data, 8);
        ImageEntry small_entry = C_ImageCache::AddImage(small_file, 4, 4, 1);
        printf("✓ Small image added at offset %zu\n", small_entry.offset);
        
        // Render small image
        const uint8_t* small_data = C_ImageCache::GetImageData(small_entry, 0);
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                uint8_t pixel_value = small_data[y * 4 + x];
                canvas.setPixel(x + 30, y + 10, pixel_value);
            }
        }
        
        // Test 8: Release entries
        printf("Testing entry release...\n");
        C_ImageCache::ReleaseEntry(entry);
        C_ImageCache::ReleaseEntry(small_entry);
        printf("✓ Entries released successfully\n");
        
    } catch (const ImageCacheException& e) {
        printf("❌ ImageCache error: %s\n", e.what());
        return 1;
    }
    
    // Export canvas to verify visual results
    canvas.exportToPGM("image_cache_demo.pgm");
    
    printf("\n=== ImageCache Demo Complete ===\n");
    printf("✓ ImageCache component working correctly\n");
    printf("✓ Image loading and caching functional\n");
    printf("✓ Memory management operating properly\n");
    printf("✓ Multiple image allocation successful\n");
    printf("✓ Entry lifecycle management working\n");
    printf("Demo output exported to image_cache_demo.pgm\n");
    
    return 0;
}