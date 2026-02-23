/**
 * @file image_cache.hpp
 * @brief Image cache component for sprite management
 *
 * Provides static memory allocation for 4-bit image data with
 * efficient caching and frame-based animation support.
 * Based on original Enjin C_ImageCache design.
 */
#pragma once

#include "../core/component.hpp"
#include "../core/types.hpp"
#include <cstdint>
#include <stdexcept>

namespace enjin2 {

// Forward declarations
class Object;

/**
 * @brief Abstract file interface for platform independence
 * 
 * Allows ImageCache to work with different file systems
 * (embedded flash, SD card, etc.)
 */
class FileInterface {
public:
    virtual ~FileInterface() = default;
    /// @brief Open the file for reading
    /// @return True if file opened successfully
    virtual bool open() = 0;
    /// @brief Close the file
    virtual void close() = 0;
    /**
     * @brief Read data from the file
     * @param buffer Output buffer for read data
     * @param length Number of bytes to read
     * @return Number of bytes actually read
     */
    virtual size_t read(uint8_t* buffer, size_t length) = 0;
    /**
     * @brief Seek to position in the file
     * @param position Byte offset from beginning of file
     * @return True if seek successful
     */
    virtual bool seek(size_t position) = 0;
    /// @brief Get file size
    /// @return File size in bytes
    virtual size_t size() const = 0;
};

/**
 * @brief Image entry descriptor for cached images
 */
struct ImageEntry {
    size_t offset;      ///< Position in cache buffer
    size_t size;        ///< Total size in bytes (width * height * frames)
    uint16_t width;     ///< Image width
    uint16_t height;    ///< Image height  
    uint16_t frames;    ///< Number of animation frames
    bool active;        ///< Allocation status
    
    ImageEntry() : offset(0), size(0), width(0), height(0), frames(0), active(false) {}
};

/**
 * @brief Exception class for ImageCache errors
 */
class ImageCacheException : public std::runtime_error {
public:
    /// @brief Construct with error message
    /// @param message Error description
    explicit ImageCacheException(const char* message) : std::runtime_error(message) {}
};

/**
 * @brief Image cache component for sprite management
 * 
 * Provides static memory allocation for 4-bit image data with
 * efficient caching and frame-based animation support.
 * Based on original Enjin C_ImageCache design.
 */
class C_ImageCache : public Component {
public:
    static constexpr size_t CACHE_SIZE = 32768;  ///< 32KB cache size for embedded use
    
    /**
     * @brief Constructor
     * @param owner Parent object
     */
    explicit C_ImageCache(Object* owner);
    
    /**
     * @brief Destructor
     */
    ~C_ImageCache() = default;
    
    // Component interface
    void awake() override;
    void start() override {}
    void update(uint16_t deltaTime) override;
    
    // Static cache operations (matches original Enjin exactly)
    
    /**
     * @brief Add image to cache from file
     * @param file File interface to read from
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param frameCount Number of animation frames
     * @return ImageEntry descriptor for cached image
     * @throws ImageCacheException on allocation or I/O errors
     */
    static ImageEntry AddImage(FileInterface& file, uint16_t width, uint16_t height, uint16_t frameCount);
    
    /**
     * @brief Release cached image entry
     * @param entry Image entry to release
     * @throws ImageCacheException if entry is inactive
     */
    static void ReleaseEntry(ImageEntry& entry);
    
    /**
     * @brief Get pointer to image data in cache
     * @param entry Image entry descriptor
     * @param frameOffset Frame number (0-based)
     * @return Pointer to 4-bit image data
     * @throws ImageCacheException on invalid access
     */
    static const uint8_t* GetImageData(const ImageEntry& entry, size_t frameOffset = 0);
    
    /**
     * @brief Get cache statistics
     * @return Pair of (used_bytes, total_bytes)
     */
    static std::pair<size_t, size_t> GetCacheStats();
    
    /**
     * @brief Clear entire cache
     */
    static void ClearCache();

private:
    static uint8_t textureCache[CACHE_SIZE];  ///< Static cache buffer
    static bool initialized;                  ///< Initialization flag
    
    // Memory management
    static size_t FindFreeSpace(size_t requiredSize);
    static void ValidateImageParameters(uint16_t width, uint16_t height, uint16_t frameCount, size_t fileSize);
    
    // Utility functions
    static void SerialLog(const char* message);
    static void InitializeCache();
};

} // namespace enjin2