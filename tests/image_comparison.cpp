#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <cstdio>
#include <cstdlib>
#include <string>

/**
 * Compare two BMP images and return the pixel difference percentage.
 *
 * @param file1 Path to first BMP file
 * @param file2 Path to second BMP file
 * @return Pixel difference percentage (0.0 to 100.0)
 */
float compareBMP(const char* file1, const char* file2) {
    // Load both images
    int width1, height1, channels1;
    unsigned char* data1 = stbi_load(file1, &width1, &height1, &channels1, 3); // Force 3 channels (RGB)

    int width2, height2, channels2;
    unsigned char* data2 = stbi_load(file2, &width2, &height2, &channels2, 3); // Force 3 channels (RGB)

    // Check if images loaded successfully
    if (!data1 || !data2) {
        fprintf(stderr, "Error: Failed to load images\n");
        if (!data1) fprintf(stderr, "  Failed to load: %s\n", file1);
        if (!data2) fprintf(stderr, "  Failed to load: %s\n", file2);
        return -1.0f;
    }

    // Verify dimensions match
    if (width1 != width2 || height1 != height2) {
        fprintf(stderr, "Error: Image dimensions don't match\n");
        fprintf(stderr,  "  %s: %dx%d\n", file1, width1, height1);
        fprintf(stderr,  "  %s: %dx%d\n", file2, width2, height2);
        stbi_image_free(data1);
        stbi_image_free(data2);
        return -1.0f;
    }

    // Compare pixels
    uint32_t totalPixels = width1 * height1;
    uint32_t diffCount = 0;

    for (int i = 0; i < totalPixels * 3; i += 3) {
        // Compare all three channels (R, G, B)
        if (data1[i] != data2[i] ||     // R
            data1[i+1] != data2[i+1] || // G
            data1[i+2] != data2[i+2]) {  // B
            diffCount++;
        }
    }

    // Free image data
    stbi_image_free(data1);
    stbi_image_free(data2);

    // Calculate percentage
    return (static_cast<float>(diffCount) / static_cast<float>(totalPixels)) * 100.0f;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file1.bmp> <file2.bmp>\n", argv[0]);
        fprintf(stderr, "Returns 0 if pixel difference <= 3%%, 1 otherwise\n");
        return 1;
    }

    const char* file1 = argv[1];
    const char* file2 = argv[2];

    printf("Comparing images:\n");
    printf("  File 1: %s\n", file1);
    printf("  File 2: %s\n", file2);
    printf("\n");

    float diffPercent = compareBMP(file1, file2);

    if (diffPercent < 0.0f) {
        // Error occurred
        return 1;
    }

    printf("Pixel difference: %.2f%%\n", diffPercent);

    const float TOLERANCE = 3.0f;
    if (diffPercent <= TOLERANCE) {
        printf("Result: PASS (within %.1f%% tolerance)\n", TOLERANCE);
        return 0;
    } else {
        printf("Result: FAIL (exceeds %.1f%% tolerance)\n", TOLERANCE);
        return 1;
    }
}
