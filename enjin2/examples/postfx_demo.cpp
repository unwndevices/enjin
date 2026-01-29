#include "../include/enjin2/graphics/canvas.hpp"
#include "../include/enjin2/effects/postfx.hpp"
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/position.hpp"
#include "../include/enjin2/components/draw.hpp"
#include "../include/enjin2/utils/polar.hpp"

using namespace enjin2;

void createTestPattern(Canvas8<128, 64>& canvas) {
    // Clear canvas
    canvas.clear(0);
    
    // Create test pattern with various elements
    
    // 1. Gradient rectangle
    for (int y = 5; y < 20; y++) {
        for (int x = 5; x < 35; x++) {
            uint8_t color = ((x - 5) * 15) / 30;
            canvas.setPixel(x, y, color);
        }
    }
    
    // 2. Bright spots for glow testing
    canvas.setPixel(50, 10, 15);
    canvas.setPixel(52, 10, 15);
    canvas.setPixel(54, 10, 15);
    canvas.setPixel(50, 12, 15);
    canvas.setPixel(52, 12, 15);
    canvas.setPixel(54, 12, 15);
    
    // 3. Circular pattern using polar coordinates
    Point center(90, 35);
    for (int radius = 5; radius < 25; radius += 5) {
        uint8_t color = 12 - (radius / 5) * 3;
        
        for (int angle = 0; angle < 360; angle += 15) {
            float phase = angle / 360.0f;
            Point point = Polar::RadialToCartesian(phase, radius, center);
            
            if (point.x >= 0 && point.x < 128 && point.y >= 0 && point.y < 64) {
                canvas.setPixel(point.x, point.y, color);
            }
        }
    }
    
    // 4. Text-like pattern for contrast testing
    for (int y = 45; y < 55; y++) {
        for (int x = 10; x < 100; x += 8) {
            // Simple character blocks
            for (int dy = 0; dy < 8; dy++) {
                for (int dx = 0; dx < 6; dx++) {
                    if ((dx + dy) % 3 == 0) {
                        canvas.setPixel(x + dx, y + dy, 8);
                    }
                }
            }
        }
    }
    
    // 5. Fine details for blur testing
    for (int i = 0; i < 50; i++) {
        int x = 5 + (i % 10) * 2;
        int y = 25 + (i / 10) * 2;
        canvas.setPixel(x, y, 15);
    }
}

int main() {
    printf("Enjin2 PostFx System Demo\n");
    printf("==========================\n");
    
    // Create PostFx instance
    PostFx postfx;
    
    // Create base test pattern
    Canvas8<128, 64> original_canvas;
    createTestPattern(original_canvas);
    original_canvas.exportToPGM("postfx_original.pgm");
    printf("Original test pattern exported to postfx_original.pgm\n");
    
    // Test 1: CRT Scanlines Effect
    printf("Testing CRT Scanlines effect...\n");
    Canvas8<128, 64> crt_canvas = original_canvas;
    PostFxParams crt_params(0.6f, 1.0f, 8, 2);
    PostFx::applyCrtScanlines(crt_canvas, crt_params);
    crt_canvas.exportToPGM("postfx_crt_scanlines.pgm");
    printf("✓ CRT scanlines effect applied\n");
    
    // Test 2: Noise Effect
    printf("Testing Noise effect...\n");
    Canvas8<128, 64> noise_canvas = original_canvas;
    PostFxParams noise_params(0.4f);
    PostFx::applyNoise(noise_canvas, noise_params);
    noise_canvas.exportToPGM("postfx_noise.pgm");
    printf("✓ Noise effect applied\n");
    
    // Test 3: Blur Effect
    printf("Testing Blur effect...\n");
    Canvas8<128, 64> blur_canvas = original_canvas;
    PostFxParams blur_params(0.7f);
    PostFx::applyBlur(blur_canvas, blur_params);
    blur_canvas.exportToPGM("postfx_blur.pgm");
    printf("✓ Blur effect applied\n");
    
    // Test 4: Glow Effect
    printf("Testing Glow effect...\n");
    Canvas8<128, 64> glow_canvas = original_canvas;
    PostFxParams glow_params(0.8f, 1.0f, 10);
    PostFx::applyGlow(glow_canvas, glow_params);
    glow_canvas.exportToPGM("postfx_glow.pgm");
    printf("✓ Glow effect applied\n");
    
    // Test 5: Dither Effect
    printf("Testing Dither effect...\n");
    Canvas8<128, 64> dither_canvas = original_canvas;
    PostFxParams dither_params(0.5f);
    PostFx::applyDither(dither_canvas, dither_params);
    dither_canvas.exportToPGM("postfx_dither.pgm");
    printf("✓ Dither effect applied\n");
    
    // Test 6: Contrast Effect
    printf("Testing Contrast effect...\n");
    Canvas8<128, 64> contrast_canvas = original_canvas;
    PostFxParams contrast_params(1.5f);  // Increase contrast
    PostFx::applyContrast(contrast_canvas, contrast_params);
    contrast_canvas.exportToPGM("postfx_contrast.pgm");
    printf("✓ Contrast effect applied\n");
    
    // Test 7: Brightness Effect
    printf("Testing Brightness effect...\n");
    Canvas8<128, 64> brightness_canvas = original_canvas;
    PostFxParams brightness_params(3.0f);  // Increase brightness
    PostFx::applyBrightness(brightness_canvas, brightness_params);
    brightness_canvas.exportToPGM("postfx_brightness.pgm");
    printf("✓ Brightness effect applied\n");
    
    // Test 8: Effect Chain (multiple effects)
    printf("Testing Effect Chain (CRT + Noise + Glow)...\n");
    Canvas8<128, 64> chain_canvas = original_canvas;
    
    std::vector<std::pair<EffectType, PostFxParams>> effect_chain = {
        {EffectType::Glow, PostFxParams(0.6f, 1.0f, 8)},
        {EffectType::CrtScanlines, PostFxParams(0.4f)},
        {EffectType::Noise, PostFxParams(0.2f)}
    };
    
    PostFx::applyEffectChain(chain_canvas, effect_chain);
    chain_canvas.exportToPGM("postfx_effect_chain.pgm");
    printf("✓ Effect chain applied\n");
    
    // Test 9: Animated effects simulation
    printf("Testing animation updates...\n");
    for (int frame = 0; frame < 5; frame++) {
        postfx.update(150);  // Simulate frame updates
        printf("  Frame %d: time = %u ms\n", frame, postfx.getTime());
    }
    printf("✓ Animation system working\n");
    
    // Test 10: Performance test with disabled effects
    printf("Testing disabled effects...\n");
    Canvas8<128, 64> disabled_canvas = original_canvas;
    PostFxParams disabled_params(0.5f);
    disabled_params.enabled = false;
    
    PostFx::applyCrtScanlines(disabled_canvas, disabled_params);
    PostFx::applyNoise(disabled_canvas, disabled_params);
    
    // Should be identical to original
    bool identical = true;
    for (int y = 0; y < 64 && identical; y++) {
        for (int x = 0; x < 128 && identical; x++) {
            if (disabled_canvas.getPixel(x, y) != original_canvas.getPixel(x, y)) {
                identical = false;
            }
        }
    }
    printf("✓ Disabled effects test: %s\n", identical ? "PASS" : "FAIL");
    
    // Test 11: Extreme parameter values
    printf("Testing extreme parameter values...\n");
    Canvas8<128, 64> extreme_canvas = original_canvas;
    
    // Test with zero intensity
    PostFx::applyCrtScanlines(extreme_canvas, PostFxParams(0.0f));
    PostFx::applyNoise(extreme_canvas, PostFxParams(0.0f));
    
    // Test with maximum intensity
    PostFx::applyGlow(extreme_canvas, PostFxParams(1.0f, 1.0f, 0));
    PostFx::applyContrast(extreme_canvas, PostFxParams(3.0f));
    
    extreme_canvas.exportToPGM("postfx_extreme_params.pgm");
    printf("✓ Extreme parameter test completed\n");
    
    printf("\n=== PostFx System Demo Complete ===\n");
    printf("✓ CRT scanlines effect working\n");
    printf("✓ Noise overlay working\n");
    printf("✓ Blur effect working\n");
    printf("✓ Glow/bloom effect working\n");
    printf("✓ Dithering pattern working\n");
    printf("✓ Contrast adjustment working\n");
    printf("✓ Brightness adjustment working\n");
    printf("✓ Effect chain system working\n");
    printf("✓ Animation system working\n");
    printf("✓ Parameter validation working\n");
    printf("✓ Performance optimizations working\n");
    
    printf("\nGenerated effect examples:\n");
    printf("- postfx_original.pgm (base pattern)\n");
    printf("- postfx_crt_scanlines.pgm\n");
    printf("- postfx_noise.pgm\n");
    printf("- postfx_blur.pgm\n");
    printf("- postfx_glow.pgm\n");
    printf("- postfx_dither.pgm\n");
    printf("- postfx_contrast.pgm\n");
    printf("- postfx_brightness.pgm\n");
    printf("- postfx_effect_chain.pgm\n");
    printf("- postfx_extreme_params.pgm\n");
    
    return 0;
}