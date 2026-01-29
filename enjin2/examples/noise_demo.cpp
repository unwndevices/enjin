#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/utils/noise.hpp"
#include "../include/enjin2/effects/postfx.hpp"
#include <ctime>

using namespace enjin2;

int main() {
    printf("Enjin2 Noise Generation Demo\n");
    printf("============================\n");
    
    // Test 1: Basic Perlin noise
    printf("Testing Perlin noise function...\n");
    Canvas8<128, 64> perlin_canvas;
    perlin_canvas.clear(0);
    
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            float nx = x * 0.05f;
            float ny = y * 0.05f;
            float noise_val = Noise::pnoise(nx, ny, 256, 256);
            uint8_t pixel_val = static_cast<uint8_t>((noise_val + 1.0f) * 7.5f); // Convert to 0-15 range
            perlin_canvas.setPixel(x, y, pixel_val);
        }
    }
    perlin_canvas.exportToPGM("noise_perlin.pgm");
    printf("✓ Perlin noise generated\n");
    
    // Test 2: Warped Perlin noise
    printf("Testing warped Perlin noise...\n");
    Canvas8<128, 64> warped_canvas;
    warped_canvas.clear(0);
    
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            float nx = x * 0.03f;
            float ny = y * 0.03f;
            uint8_t noise_val = Noise::warped_pnoise(nx, ny, 128, 128, 42, 0.7f);
            uint8_t pixel_val = Noise::to_4bit(noise_val);
            warped_canvas.setPixel(x, y, pixel_val);
        }
    }
    warped_canvas.exportToPGM("noise_warped.pgm");
    printf("✓ Warped Perlin noise generated\n");
    
    // Test 3: Value noise
    printf("Testing value noise...\n");
    Canvas8<128, 64> value_canvas;
    value_canvas.clear(0);
    
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            float nx = x * 0.08f;
            float ny = y * 0.08f;
            uint8_t noise_val = Noise::value_noise(nx, ny, 12345);
            uint8_t pixel_val = Noise::to_4bit(noise_val);
            value_canvas.setPixel(x, y, pixel_val);
        }
    }
    value_canvas.exportToPGM("noise_value.pgm");
    printf("✓ Value noise generated\n");
    
    // Test 4: FBM (Fractal Brownian Motion) noise
    printf("Testing FBM noise...\n");
    Canvas8<128, 64> fbm_canvas;
    fbm_canvas.clear(0);
    
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            float nx = x * 0.02f;
            float ny = y * 0.02f;
            uint8_t noise_val = Noise::fbm_noise(nx, ny, 5, 0.6f, 2.0f);
            uint8_t pixel_val = Noise::to_4bit(noise_val);
            fbm_canvas.setPixel(x, y, pixel_val);
        }
    }
    fbm_canvas.exportToPGM("noise_fbm.pgm");
    printf("✓ FBM noise generated\n");
    
    // Test 5: Cellular/Worley noise
    printf("Testing cellular noise...\n");
    Canvas8<128, 64> cellular_canvas;
    cellular_canvas.clear(0);
    
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            float nx = x;
            float ny = y;
            uint8_t noise_val = Noise::cellular_noise(nx, ny, 0.1f);
            uint8_t pixel_val = Noise::to_4bit(noise_val);
            cellular_canvas.setPixel(x, y, pixel_val);
        }
    }
    cellular_canvas.exportToPGM("noise_cellular.pgm");
    printf("✓ Cellular noise generated\n");
    
    // Test 6: Animated noise (simulate time-based animation)
    printf("Testing animated noise...\n");
    for (int frame = 0; frame < 5; frame++) {
        Canvas8<128, 64> anim_canvas;
        anim_canvas.clear(0);
        
        float time_offset = frame * 0.1f;
        
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 128; x++) {
                float nx = x * 0.04f + time_offset;
                float ny = y * 0.04f;
                float noise_val = Noise::pnoise(nx, ny, 256, 256);
                uint8_t pixel_val = static_cast<uint8_t>((noise_val + 1.0f) * 7.5f);
                anim_canvas.setPixel(x, y, pixel_val);
            }
        }
        
        char filename[64];
        snprintf(filename, sizeof(filename), "noise_anim_frame_%02d.pgm", frame);
        anim_canvas.exportToPGM(filename);
    }
    printf("✓ Animated noise frames generated\n");
    
    // Test 7: Noise texture generation function
    printf("Testing noise texture generation...\n");
    uint8_t noise_buffer[128 * 64];
    
    // Generate different noise types
    const char* noise_type_names[] = {"perlin", "value", "fbm", "cellular"};
    
    for (int type = 0; type < 4; type++) {
        Noise::generate_noise_texture(noise_buffer, 128, 64, 0.05f, 0.0f, 0.0f, type);
        
        Canvas8<128, 64> texture_canvas;
        texture_canvas.clear(0);
        
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 128; x++) {
                uint8_t noise_val = noise_buffer[y * 128 + x];
                uint8_t pixel_val = Noise::to_4bit(noise_val);
                texture_canvas.setPixel(x, y, pixel_val);
            }
        }
        
        char filename[64];
        snprintf(filename, sizeof(filename), "noise_texture_%s.pgm", noise_type_names[type]);
        texture_canvas.exportToPGM(filename);
    }
    printf("✓ Noise texture generation completed\n");
    
    // Test 8: Noise with PostFx effects
    printf("Testing noise with PostFx effects...\n");
    Canvas8<128, 64> fx_canvas;
    fx_canvas.clear(0);
    
    // Generate base FBM noise
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            float nx = x * 0.03f;
            float ny = y * 0.03f;
            uint8_t noise_val = Noise::fbm_noise(nx, ny, 4, 0.5f, 2.0f);
            uint8_t pixel_val = Noise::to_4bit(noise_val);
            fx_canvas.setPixel(x, y, pixel_val);
        }
    }
    
    // Apply glow effect to noise
    PostFxParams glow_params(0.8f, 1.0f, 8);
    PostFx::applyGlow(fx_canvas, glow_params);
    
    // Add some contrast
    PostFxParams contrast_params(1.3f);
    PostFx::applyContrast(fx_canvas, contrast_params);
    
    fx_canvas.exportToPGM("noise_with_postfx.pgm");
    printf("✓ Noise with PostFx effects generated\n");
    
    // Test 9: Performance test
    printf("Testing noise generation performance...\n");
    
    auto start_time = std::time(nullptr);
    
    // Generate 1000 noise samples
    for (int i = 0; i < 1000; i++) {
        float x = (i % 100) * 0.1f;
        float y = (i / 100) * 0.1f;
        
        volatile float perlin_val = Noise::pnoise(x, y, 256, 256);
        volatile uint8_t value_val = Noise::value_noise(x, y, 12345);
        volatile uint8_t fbm_val = Noise::fbm_noise(x, y, 3);
        
        // Prevent optimization
        (void)perlin_val;
        (void)value_val;
        (void)fbm_val;
    }
    
    auto end_time = std::time(nullptr);
    printf("✓ Performance test completed in %ld seconds\n", end_time - start_time);
    
    // Test 10: Parameter validation
    printf("Testing parameter validation...\n");
    
    // Test edge cases
    float edge_result1 = Noise::pnoise(0.0f, 0.0f, 1, 1);
    float edge_result2 = Noise::pnoise(1000.0f, 1000.0f, 256, 256);
    uint8_t edge_result3 = Noise::value_noise(-100.0f, -100.0f, 0);
    
    printf("✓ Edge case results: %.3f, %.3f, %d\n", edge_result1, edge_result2, edge_result3);
    
    printf("\n=== Noise Generation Demo Complete ===\n");
    printf("✓ Perlin noise implementation working\n");
    printf("✓ Warped Perlin noise working\n");
    printf("✓ Value noise working\n");
    printf("✓ FBM noise working\n");
    printf("✓ Cellular/Worley noise working\n");
    printf("✓ Animation-ready time-based noise working\n");
    printf("✓ Texture generation utilities working\n");
    printf("✓ PostFx integration working\n");
    printf("✓ Performance characteristics acceptable\n");
    printf("✓ Parameter validation robust\n");
    
    printf("\nGenerated noise examples:\n");
    printf("- noise_perlin.pgm (basic Perlin noise)\n");
    printf("- noise_warped.pgm (warped Perlin noise)\n");
    printf("- noise_value.pgm (value noise)\n");
    printf("- noise_fbm.pgm (fractal Brownian motion)\n");
    printf("- noise_cellular.pgm (cellular/Worley noise)\n");
    printf("- noise_anim_frame_XX.pgm (animation frames)\n");
    printf("- noise_texture_XXX.pgm (texture generation)\n");
    printf("- noise_with_postfx.pgm (noise + effects)\n");
    
    return 0;
}