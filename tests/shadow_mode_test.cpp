#include <enjin2/core/scene.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/components/drawable.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <cmath>

namespace enjin2 {

/**
 * @brief Simple test drawable that renders a filled rectangle
 */
class C_Rectangle : public C_Drawable {
private:
    Pixel4 color;

public:
    C_Rectangle(Object* owner, uint8_t width, uint8_t height, uint8_t rect_color)
        : C_Drawable(owner, width, height), color(rect_color) {}

    void draw(ICanvas<Pixel4>& canvas) override {
        if (!is_visible) return;

        if (position) {
            Point render_pos = GetOffsetPosition();
            Rect rect(render_pos.x, render_pos.y, width, height);
            canvas.fill(rect, color);
        }
    }

    bool continueToDraw() const override {
        return !owner->isQueuedForRemoval();
    }
};

} // namespace enjin2

using namespace enjin2;
using namespace std::chrono;

int main(int argc, char* argv[]) {
    // Check for backend type argument (for compatibility)
    const char* backend = (argc > 1) ? argv[1] : "default";

    std::cout << "Shadow Mode Test - " << backend << " backend" << std::endl;
    std::cout << "=========================================" << std::endl;

    // Create canvas for rendering
    Canvas8_128x128 canvas;

    // Create test scene
    Scene testScene(1);

    // Initialize and activate scene
    testScene.initialize();
    testScene.activate();

    // Create 3 objects with position and rectangle components
    std::cout << "Creating test objects..." << std::endl;

    // Object 1: Top-left rectangle (dark gray)
    Object* obj1 = testScene.addObject<Object>();
    C_Position* pos1 = obj1->addComponent<C_Position>(10, 10);
    C_Rectangle* rect1 = obj1->addComponent<C_Rectangle>(30, 30, 80);
    rect1->SetDrawLayer(DrawLayer::Background);

    // Object 2: Center rectangle (medium gray)
    Object* obj2 = testScene.addObject<Object>();
    C_Position* pos2 = obj2->addComponent<C_Position>(49, 49);
    C_Rectangle* rect2 = obj2->addComponent<C_Rectangle>(30, 30, 128);
    rect2->SetDrawLayer(DrawLayer::Entities);

    // Object 3: Bottom-right rectangle (light gray)
    Object* obj3 = testScene.addObject<Object>();
    C_Position* pos3 = obj3->addComponent<C_Position>(88, 88);
    C_Rectangle* rect3 = obj3->addComponent<C_Rectangle>(30, 30, 180);
    rect3->SetDrawLayer(DrawLayer::Foreground);

    std::cout << "  - Object 1: Position (10, 10), Color 80 (dark gray)" << std::endl;
    std::cout << "  - Object 2: Position (49, 49), Color 128 (medium gray)" << std::endl;
    std::cout << "  - Object 3: Position (88, 88), Color 180 (light gray)" << std::endl;
    std::cout << std::endl;

    // Run scene for 60 frames
    const uint16_t FRAME_COUNT = 60;
    const uint16_t DELTA_TIME = 16; // ~60 FPS (16ms per frame)

    std::cout << "Running simulation for " << FRAME_COUNT << " frames..." << std::endl;

    // Start timing
    auto start_time = high_resolution_clock::now();

    for (uint16_t frame = 0; frame < FRAME_COUNT; ++frame) {
        testScene.update(DELTA_TIME);
        testScene.render(canvas);
    }

    // End timing
    auto end_time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end_time - start_time);

    std::cout << "Simulation complete." << std::endl;
    std::cout << std::endl;

    // Determine output filename based on backend
    const char* output_file;
    if (strcmp(backend, "enjin1") == 0) {
        output_file = "output-enjin1.bmp";
    } else if (strcmp(backend, "enjin2") == 0) {
        output_file = "output-enjin2.bmp";
    } else {
        // Default: use enjin2 output
        output_file = "output-enjin2.bmp";
    }

    // Export canvas to BMP
    std::cout << "Exporting output to: " << output_file << std::endl;
    canvas.exportToBMP(output_file);

    // Report timing
    std::cout << std::endl;
    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;

    return 0;
}
