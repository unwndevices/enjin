#include <iostream>
#include <enjin2/core/scene.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/components/label.hpp>
#include <enjin2/graphics/image_export.hpp>

// Define required macros and types for font
#ifndef PROGMEM
#define PROGMEM
#endif

// Forward declare GFX types (defined in text_renderer.hpp)
using GFXglyph = enjin2::GFXglyph;
using GFXfont = enjin2::GFXfont;

// Include the awkward font
extern "C" {
#include "Fonts/awkward.h"
}

using namespace enjin2;

class TextTestScene : public Scene {
private:
    Canvas8<128, 128> canvas;
    Object* label_obj;

public:
    TextTestScene() : Scene(1) {} // ID = 1
    
    void onCreate() override {
        // Create label object
        label_obj = addObject<Object>();
        
        // Add position component
        auto position = label_obj->addComponent<C_Position>();
        position->setPosition(64, 32);
        position->setAnchor(Anchor::CENTER);
        
        // Add label component with simple bitmap font
        auto label = label_obj->addComponent<Label>(
            120, 60,                    // width, height
            nullptr,                   // font (use simple bitmap font)
            1,                         // font size
            15,                        // text color (white)
            1,                         // background color (very dark gray)
            8                          // pointer height
        );
        
        label->setText("Hello World!\nThis is a test of the awkward font with word wrapping.");
        label->setAlignment(LabelAlign::Center);
        label->setMargins(4, 4);
    }

    void onUpdate(float deltaTime) {
        Scene::update(static_cast<uint16_t>(deltaTime));
        
        // Clear canvas
        canvas.clear(0);
        
        // Draw all objects manually (simplified for demo)
        if (label_obj) {
            // Get label component and draw it
            auto label = label_obj->getComponent<Label>();
            if (label) {
                label->draw(canvas);
            }
        }
    }

    void exportFrame(int frame) {
        std::string filename = "text_demo_frame_" + std::string(2 - std::to_string(frame).length(), '0') + std::to_string(frame) + ".pgm";
        ImageExporter::exportToPGM(canvas, filename);
        std::cout << "Exported: " << filename << std::endl;
    }

    const Canvas8<128, 128>& getCanvas() const { return canvas; }
};

int main() {
    std::cout << "=== Enjin2 Text Rendering Demo ===" << std::endl;
    std::cout << "Testing Label component with Awkward font" << std::endl;

    try {
        TextTestScene scene;
        scene.onCreate();

        // Update a few times and export frames
        for (int frame = 0; frame < 3; frame++) {
            scene.onUpdate(16.0f); // 16ms = ~60 FPS
            scene.exportFrame(frame);
        }

        std::cout << "Text demo completed successfully!" << std::endl;
        std::cout << "Check generated PGM files to see text rendering results." << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}