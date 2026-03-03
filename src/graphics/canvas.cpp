#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../vendor/stb_image_write.h"

#include <vector>

#include "../../include/enjin2/graphics/canvas.hpp"

namespace enjin2 {

// Template explicit instantiations for BMP export
template <uint16_t WIDTH, uint16_t HEIGHT>
void Canvas8<WIDTH, HEIGHT>::exportToBMP(const char *filename) const
{
    // Convert 8-bit grayscale to 24-bit RGB (BMP limitation)
    std::vector<uint8_t> rgbBuffer(WIDTH * HEIGHT * 3);

    for (size_t i = 0; i < WIDTH * HEIGHT; ++i)
    {
        uint8_t gray = buffer[i];
        size_t rgbIndex = i * 3;
        rgbBuffer[rgbIndex + 0] = gray; // R
        rgbBuffer[rgbIndex + 1] = gray; // G
        rgbBuffer[rgbIndex + 2] = gray; // B
    }

    // Write BMP using stb_image_write
    stbi_write_bmp(filename, WIDTH, HEIGHT, 3, rgbBuffer.data());
}

// Explicit template instantiations for common canvas sizes
template void Canvas8<128, 64>::exportToBMP(const char *filename) const;
template void Canvas8<128, 128>::exportToBMP(const char *filename) const;
template void Canvas8<320, 240>::exportToBMP(const char *filename) const;

} // namespace enjin2