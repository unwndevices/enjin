#include <enjin2/core/scene.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

// Test 1: Verify onRender(ICanvas<Pixel4>&) is called during Scene::render<Pixel4>()
static void test_onRender_pixel4_called() {
    struct TestScene4 : public Scene {
        TestScene4() : Scene(1) {}
        bool onRenderCalled = false;
        void onRender(ICanvas<Pixel4>& /*canvas*/) override {
            onRenderCalled = true;
        }
    };

    Canvas4<16, 16> canvas;
    TestScene4 scene;
    scene.initialize();
    scene.activate();
    scene.render(canvas);

    ASSERT(scene.onRenderCalled, "onRender(ICanvas<Pixel4>&) called during render()");
}

// Test 2: Pixels written in onRender appear in the output canvas
static void test_onRender_pixel4_pixels_appear() {
    struct TestScene4Pixel : public Scene {
        TestScene4Pixel() : Scene(2) {}
        void onRender(ICanvas<Pixel4>& canvas) override {
            canvas.setPixel(0, 0, Pixel4(7));
        }
    };

    Canvas4<16, 16> canvas;
    canvas.clear(Pixel4(0));

    TestScene4Pixel scene;
    scene.initialize();
    scene.activate();
    scene.render(canvas);

    ASSERT(canvas.getPixel(0, 0).value == 7, "pixel written in onRender appears in canvas");
}

// Test 3: Regression guard — uint8_t onRender path still works after fix
static void test_onRender_uint8_still_works() {
    struct TestScene8 : public Scene {
        TestScene8() : Scene(3) {}
        bool onRenderCalled = false;
        void onRender(ICanvas<uint8_t>& /*canvas*/) override {
            onRenderCalled = true;
        }
    };

    Canvas8<16, 16> canvas;
    TestScene8 scene;
    scene.initialize();
    scene.activate();
    scene.render(canvas);

    ASSERT(scene.onRenderCalled, "onRender(ICanvas<uint8_t>&) still called (regression guard)");
}

int main() {
    test_onRender_pixel4_called();
    test_onRender_pixel4_pixels_appear();
    test_onRender_uint8_still_works();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
