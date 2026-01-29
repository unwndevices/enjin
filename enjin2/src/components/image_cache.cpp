#include "../../include/enjin2/components/image_cache.hpp"
#include "../../include/enjin2/core/object.hpp"
#include <cstring>
#include <iostream>

namespace enjin2 {

// Static member initialization
uint8_t C_ImageCache::textureCache[CACHE_SIZE] = {0};
bool C_ImageCache::initialized = false;

C_ImageCache::C_ImageCache(Object* owner) : Component(owner) {
    if (!initialized) {
        InitializeCache();
    }
}

void C_ImageCache::InitializeCache() {
    std::memset(textureCache, 0, CACHE_SIZE);
    initialized = true;
    SerialLog("ImageCache initialized with 32KB capacity");
}

void C_ImageCache::awake() {
    // No initialization needed in awake
}

void C_ImageCache::update(uint16_t deltaTime) {
    // No per-frame updates needed for cache
}

ImageEntry C_ImageCache::AddImage(FileInterface& file, uint16_t width, uint16_t height, uint16_t frameCount) {
    if (!initialized) {
        InitializeCache();
    }
    
    // Calculate required size for 4-bit packed data (2 pixels per byte)
    size_t packedSize = (static_cast<size_t>(width) * height * frameCount + 1) / 2;
    
    // Validate parameters
    ValidateImageParameters(width, height, frameCount, file.size());
    
    // Find space in cache
    size_t offset = FindFreeSpace(packedSize * 2); // Need unpacked size
    if (offset == CACHE_SIZE) {
        throw ImageCacheException("Cache full - no space available");
    }
    
    // Open and read file
    if (!file.open()) {
        throw ImageCacheException("Failed to open image file");
    }
    
    // Create temporary buffer for packed data
    uint8_t* packedData = new uint8_t[packedSize];
    size_t bytesRead = file.read(packedData, packedSize);
    file.close();
    
    if (bytesRead != packedSize) {
        delete[] packedData;
        throw ImageCacheException("File read size mismatch");
    }
    
    // Unpack pixels into cache (matches original Enjin exactly)
    for (size_t i = 0; i < packedSize; i++) {
        uint8_t byte = packedData[i];
        
        // Store pixels as unpacked 4-bit values
        if (offset + (i * 2) < CACHE_SIZE) {
            textureCache[offset + (i * 2)] = byte >> 4;        // High 4 bits
        }
        if (offset + (i * 2) + 1 < CACHE_SIZE) {
            textureCache[offset + (i * 2) + 1] = byte & 0x0F;  // Low 4 bits
        }
    }
    
    delete[] packedData;
    
    // Create and return entry
    ImageEntry entry;
    entry.offset = offset;
    entry.size = packedSize * 2;  // Size is doubled because we unpacked
    entry.width = width;
    entry.height = height;
    entry.frames = frameCount;
    entry.active = true;
    
    SerialLog("Image added to cache successfully");
    return entry;
}

void C_ImageCache::ReleaseEntry(ImageEntry& entry) {
    if (!entry.active) {
        throw ImageCacheException("Attempting to release inactive entry");
    }
    
    // Mark as inactive (matches original Enjin - no memory clearing)
    entry.active = false;
    SerialLog("Image entry released from cache");
}

const uint8_t* C_ImageCache::GetImageData(const ImageEntry& entry, size_t frameOffset) {
    if (!entry.active) {
        throw ImageCacheException("Attempting to access inactive entry");
    }
    
    if (frameOffset >= entry.frames) {
        throw ImageCacheException("Frame offset out of bounds");
    }
    
    // Calculate frame size and offset (matches original Enjin)
    size_t frameSize = entry.width * entry.height;
    size_t offset = entry.offset + (frameOffset * frameSize);
    
    if (offset + frameSize > CACHE_SIZE) {
        throw ImageCacheException("Cache access out of bounds");
    }
    
    return &textureCache[offset];
}

std::pair<size_t, size_t> C_ImageCache::GetCacheStats() {
    size_t usedBytes = 0;
    
    // Count non-zero bytes as used (simple heuristic)
    for (size_t i = 0; i < CACHE_SIZE; i++) {
        if (textureCache[i] != 0) {
            usedBytes++;
        }
    }
    
    return std::make_pair(usedBytes, CACHE_SIZE);
}

void C_ImageCache::ClearCache() {
    std::memset(textureCache, 0, CACHE_SIZE);
    SerialLog("Cache cleared");
}

size_t C_ImageCache::FindFreeSpace(size_t requiredSize) {
    if (requiredSize > CACHE_SIZE) {
        throw ImageCacheException("Required size exceeds cache capacity");
    }
    
    // Simple first-fit allocation (matches original Enjin exactly)
    size_t currentOffset = 0;
    
    while (currentOffset < CACHE_SIZE) {
        // Check if we have enough contiguous free space
        size_t freeSpace = 0;
        while ((currentOffset + freeSpace < CACHE_SIZE) &&
               (textureCache[currentOffset + freeSpace] == 0)) {
            freeSpace++;
            if (freeSpace >= requiredSize) {
                // Found suitable space - use first fit
                return currentOffset;
            }
        }
        
        // Skip past the used block
        currentOffset += freeSpace + 1;
    }
    
    return CACHE_SIZE; // No suitable space found
}

void C_ImageCache::ValidateImageParameters(uint16_t width, uint16_t height, uint16_t frameCount, size_t fileSize) {
    if (width == 0 || height == 0 || frameCount == 0) {
        throw ImageCacheException("Invalid image dimensions");
    }
    
    // Calculate expected packed file size (2 pixels per byte)
    size_t expectedSize = (static_cast<size_t>(width) * height * frameCount + 1) / 2;
    if (fileSize != expectedSize) {
        throw ImageCacheException("File size does not match expected dimensions");
    }
}

void C_ImageCache::SerialLog(const char* message) {
    std::cout << "[C_ImageCache] " << message << std::endl;
}

} // namespace enjin2