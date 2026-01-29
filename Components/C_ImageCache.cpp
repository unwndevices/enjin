#include "C_ImageCache.hpp"
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <iostream>
#endif

namespace enjin
{

    // Static member initialization
    uint8_t C_ImageCache::textureCache[CACHE_SIZE] = {0};
    bool C_ImageCache::initialized = false;

    C_ImageCache::C_ImageCache(Object *owner) : Component(owner)
    {
        if (!initialized)
        {
            InitializeCache();
        }
    }

    void C_ImageCache::InitializeCache()
    {
        std::memset(textureCache, 0, CACHE_SIZE);
        initialized = true;
    }

    void C_ImageCache::Awake()
    {
        // No initialization needed in Awake
    }

    void C_ImageCache::Update(uint16_t deltaTime)
    {
        // No per-frame updates needed
    }

    ImageEntry C_ImageCache::AddImage(FileInterface &file, uint16_t width, uint16_t height, uint16_t frameCount)
    {
        if (!initialized)
        {
            InitializeCache();
        }

        // Calculate required size for packed data (2 pixels per byte)
        size_t packedSize = (static_cast<size_t>(width) * height * frameCount + 1) / 2;

        // Validate parameters
        ValidateImageParameters(width, height, frameCount, file.size());

        // Find space in cache
        size_t offset = FindFreeSpace(packedSize);
        if (offset == CACHE_SIZE)
        {
            throw ImageCacheException("Cache full - no space available");
        }

        // Open and read file
        if (!file.open())
        {
            throw ImageCacheException("Failed to open image file");
        }

        // Create a temporary buffer for packed data
        uint8_t *packedData = new uint8_t[packedSize];
        size_t bytesRead = file.read(packedData, packedSize);
        file.close();

        if (bytesRead != packedSize)
        {
            delete[] packedData;
            throw ImageCacheException("File read size mismatch");
        }

        // Unpack the pixels into the cache
        for (size_t i = 0; i < packedSize; i++)
        {
            uint8_t byte = packedData[i];
            textureCache[offset + (i * 2)] = byte >> 4;       // High 4 bits
            textureCache[offset + (i * 2) + 1] = byte & 0x0F; // Low 4 bits
        }

        delete[] packedData;

        // Create and return entry
        ImageEntry entry;
        entry.offset = offset;
        entry.size = packedSize * 2; // Size is now doubled because we unpacked the pixels
        entry.width = width;
        entry.height = height;
        entry.frames = frameCount;
        entry.active = true;

        return entry;
    }

    void C_ImageCache::ReleaseEntry(ImageEntry &entry)
    {
        if (!entry.active)
        {
            throw ImageCacheException("Attempting to release inactive entry");
        }

        // Mark as inactive but don't clear memory
        entry.active = false;
    }

    const uint8_t *C_ImageCache::GetImageData(const ImageEntry &entry, size_t frameOffset)
    {
        if (!entry.active)
        {
            throw ImageCacheException("Attempting to access inactive entry");
        }

        if (frameOffset >= entry.frames)
        {
            throw ImageCacheException("Frame offset out of bounds");
        }

        size_t frameSize = (entry.width * entry.height + 1) / 2;
        size_t offset = entry.offset + (frameOffset * frameSize);

        if (offset + frameSize > CACHE_SIZE)
        {
            throw ImageCacheException("Cache access out of bounds");
        }

        return &textureCache[offset];
    }

    size_t C_ImageCache::FindFreeSpace(size_t requiredSize)
    {
        if (requiredSize > CACHE_SIZE)
        {
            throw ImageCacheException("Required size exceeds cache capacity");
        }

        // Simple first-fit allocation
        size_t currentOffset = 0;
        size_t bestFitOffset = CACHE_SIZE;
        size_t bestFitSize = CACHE_SIZE;

        while (currentOffset < CACHE_SIZE)
        {
            // Check if we have enough contiguous free space
            size_t freeSpace = 0;
            while ((currentOffset + freeSpace < CACHE_SIZE) &&
                   (textureCache[currentOffset + freeSpace] == 0))
            {
                freeSpace++;
                if (freeSpace >= requiredSize)
                {
                    // Found a suitable space - use first fit
                    return currentOffset;
                }
            }

            // Skip past the used block
            currentOffset += freeSpace + 1;
        }

        return CACHE_SIZE; // No suitable space found
    }

    void C_ImageCache::ValidateImageParameters(uint16_t width, uint16_t height, uint16_t frameCount, size_t fileSize)
    {
        if (width == 0 || height == 0 || frameCount == 0)
        {
            throw ImageCacheException("Invalid image dimensions");
        }

        size_t expectedSize = (static_cast<size_t>(width) * height * frameCount + 1) / 2;
        if (fileSize != expectedSize)
        {
            throw ImageCacheException("File size does not match expected dimensions");
        }
    }

    void C_ImageCache::SerialLog(const char *message)
    {
#ifdef ARDUINO
        Serial.println(message);
#else
        std::cerr << "[C_ImageCache] " << message << std::endl;
#endif
    }

} // namespace enjin