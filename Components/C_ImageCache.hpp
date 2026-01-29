#ifndef C_IMAGE_CACHE_HPP
#define C_IMAGE_CACHE_HPP

#include <cstdint>
#include <memory>
#include <stdexcept>
#include "Component.hpp"

namespace enjin
{

    // Forward declarations
    class Object;

    // Abstract file interface for platform independence
    class FileInterface
    {
    public:
        virtual ~FileInterface() = default;
        virtual bool open() = 0;
        virtual void close() = 0;
        virtual size_t read(uint8_t *buffer, size_t length) = 0;
        virtual bool seek(size_t position) = 0;
        virtual size_t size() const = 0;
    };

    struct ImageEntry
    {
        size_t offset;   // Position in cache
        size_t size;     // Total size in bytes (width * height * frames / 2)
        uint16_t width;  // Image width
        uint16_t height; // Image height
        uint16_t frames; // Number of frames
        bool active;     // Allocation status

        ImageEntry() : offset(0), size(0), width(0), height(0), frames(0), active(false) {}
    };

    class ImageCacheException : public std::runtime_error
    {
    public:
        explicit ImageCacheException(const char *message) : std::runtime_error(message) {}
    };

    class C_ImageCache : public Component
    {
    public:
        static constexpr size_t CACHE_SIZE = 1000; // 32KB cache size

        explicit C_ImageCache(Object *owner);
        ~C_ImageCache() = default;

        // Core cache operations
        static ImageEntry AddImage(FileInterface &file, uint16_t width, uint16_t height, uint16_t frameCount);
        static void ReleaseEntry(ImageEntry &entry);

        // Cache access
        static const uint8_t *GetImageData(const ImageEntry &entry, size_t frameOffset = 0);

        // Component interface
        void Awake() override;
        void Update(uint16_t deltaTime) override;

    private:
        static uint8_t textureCache[CACHE_SIZE];
        static bool initialized;

        // Memory management
        static size_t FindFreeSpace(size_t requiredSize);
        static void ValidateImageParameters(uint16_t width, uint16_t height, uint16_t frameCount, size_t fileSize);

        // Utility functions
        static void SerialLog(const char *message);
        static void InitializeCache();
    };

} // namespace enjin

#endif // C_IMAGE_CACHE_HPP