/**
 * @file graphics_output_demo.cpp
 * @brief Graphics output demo for Enjin2 space UI with PGM export
 * 
 * Creates a simplified space UI demo that exports frames as PGM images
 * so we can properly visualize the orbital animation system.
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

#include <enjin2/core/scene.hpp>
#include <enjin2/core/scene_state_machine.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/components/planet.hpp>
#include <enjin2/components/satellite.hpp>
#include <enjin2/components/probe.hpp>
#include <enjin2/components/animation.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/image_export.hpp>

using namespace enjin2;

/**
 * @brief Simplified audio control center for graphics demo
 */
class SimpleControlCenter : public Object {
private:
    C_Planet* planet;
    C_Satellite* satellites[2];  // Just 2 satellites for clarity
    
public:
    /**
     * @brief Constructor
     */
    SimpleControlCenter(Point center) {
        // Set position
        auto pos = getComponent<C_Position>();
        if (pos) {
            pos->setPosition(center.x, center.y);
        }
        
        // Add central planet
        planet = addComponent<C_Planet>(15.0f, Pixel4(12));  // Medium gray planet
        planet->setAtmosphere(true, 1.5f, Pixel4(8));       // Atmospheric glow
        planet->setPulsing(true, 2.0f, 0.2f);               // Gentle pulsing
        planet->setRotation(true, 0.5f);                    // Rotation
        
        // Initialize satellites
        for (int i = 0; i < 2; ++i) {
            satellites[i] = nullptr;
        }
    }
    
    /**
     * @brief Add satellite for parameter control
     */
    void addSatellite(int index, float orbitRadius, float startAngle, Pixel4 color) {
        if (index >= 0 && index < 2) {
            satellites[index] = addComponent<C_Satellite>(planet, orbitRadius, startAngle);
            satellites[index]->setAppearance(2.5f, color, true);
            satellites[index]->setOrbit(orbitRadius, 1.0f + index * 0.5f);  // Different speeds
            satellites[index]->setParameterEffects(true, false, true);      // Radius and color from param
            satellites[index]->setTrail(true, Pixel4(color.value / 3));
        }
    }
    
    /**
     * @brief Update parameters (simulated audio changes)
     */
    void updateParameters(float time) {
        for (int i = 0; i < 2; ++i) {
            if (satellites[i]) {
                // Simple sine wave parameter changes
                float t = time + i * 2.0f;
                float param = 0.5f + 0.3f * std::sin(t * (1.0f + i * 0.8f));
                satellites[i]->setParameterValue(param);
            }
        }
    }
};

/**
 * @brief Simple scanner probe for the demo
 */
class SimpleScanner : public Object {
public:
    /**
     * @brief Constructor
     */
    SimpleScanner(Point startPos) {
        auto pos = getComponent<C_Position>();
        if (pos) {
            pos->setPosition(startPos.x, startPos.y);
        }
        
        auto probe = addComponent<C_Probe>(C_Probe::ProbeType::SCANNER, 3.0f);
        probe->setAppearance(C_Probe::ProbeType::SCANNER, 3.0f, Pixel4(15), Pixel4(10));
        probe->setMovement(C_Probe::MovementPattern::SINE_WAVE, 0.6f);
        probe->setPulsing(true, 2.5f);
        probe->setScanner(15.0f, 1.5f);
    }
};

/**
 * @brief Graphics demo scene
 */
class GraphicsDemoScene : public Scene {
private:
    SimpleControlCenter* controlCenter;
    SimpleScanner* scanner;
    float sceneTime;
    
public:
    /**
     * @brief Constructor
     */
    GraphicsDemoScene(uint32_t id) : Scene(id), sceneTime(0.0f) {}
    
protected:
    /**
     * @brief Create demo elements
     */
    void onCreate() override {
        std::cout << "GraphicsDemoScene: Creating simplified orbital interface...\n";
        
        // Main control center at canvas center
        controlCenter = addObject<SimpleControlCenter>(Point(64, 32));
        
        // Add two satellites for clear visualization
        controlCenter->addSatellite(0, 25.0f, 0.0f, Pixel4(15));      // Bright satellite
        controlCenter->addSatellite(1, 35.0f, 3.14f, Pixel4(10));     // Dimmer satellite
        
        // Scanner probe
        scanner = addObject<SimpleScanner>(Point(20, 50));
        
        std::cout << "GraphicsDemoScene: Created " << getObjects().size() << " objects\n";
    }
    
    /**
     * @brief Scene activated
     */
    void onActivate() override {
        std::cout << "GraphicsDemoScene: Graphics demo activated\n";
    }
    
    /**
     * @brief Update scene
     */
    void onUpdate(float dt) override {
        sceneTime += dt;
        
        // Update control center parameters
        if (controlCenter) {
            controlCenter->updateParameters(sceneTime);
        }
    }
    
    /**
     * @brief Render space background (4-bit)
     */
    void onRender(ICanvas<Pixel4>& canvas) override {
        // Dark space background
        canvas.clear(Pixel4(2));
        
        // Add some stars for reference
        static bool starsDrawn = false;
        if (!starsDrawn) {
            for (int i = 0; i < 12; ++i) {
                int x = (i * 23 + 17) % 128;
                int y = (i * 37 + 29) % 64;
                canvas.setPixel(x, y, Pixel4(4 + (i % 2)));
            }
            starsDrawn = true;
        }
    }
    
    /**
     * @brief Render space background (8-bit)
     */
    void onRender(ICanvas<uint8_t>& canvas) override {
        canvas.clear(32);  // Dark space
        
        static bool starsDrawn = false;
        if (!starsDrawn) {
            for (int i = 0; i < 12; ++i) {
                int x = (i * 23 + 17) % 128;
                int y = (i * 37 + 29) % 64;
                canvas.setPixel(x, y, static_cast<uint8_t>(64 + (i % 2) * 32));
            }
            starsDrawn = true;
        }
    }
};

/**
 * @brief Main graphics demo application
 */
int main() {
    std::cout << "=== Enjin2 Graphics Output Demo ===\n";
    std::cout << "Orbital Animation with PGM Export\n\n";
    
    // Create canvas
    constexpr uint16_t CANVAS_WIDTH = 128;
    constexpr uint16_t CANVAS_HEIGHT = 64;
    Canvas4<CANVAS_WIDTH, CANVAS_HEIGHT> canvas;
    
    // Create scene manager
    SceneStateMachine sceneManager;
    
    // Add graphics demo scene
    auto* demoScene = sceneManager.addScene<GraphicsDemoScene>(1);
    if (!demoScene) {
        std::cerr << "Failed to create graphics demo scene!\n";
        return 1;
    }
    
    // Start the scene
    if (!sceneManager.changeScene(1)) {
        std::cerr << "Failed to start graphics demo scene!\n";
        return 1;
    }
    
    std::cout << "Starting graphics demo...\n";
    std::cout << "Exporting 20 frames as PGM images and showing improved ASCII\n\n";
    
    // Main loop - export frames for visualization
    auto lastTime = std::chrono::steady_clock::now();
    constexpr int TOTAL_FRAMES = 20;
    int frameNum = 0;
    
    while (frameNum < TOTAL_FRAMES) {
        auto currentTime = std::chrono::steady_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastTime).count();
        lastTime = currentTime;
        
        float dt = static_cast<float>(std::min(static_cast<long long>(deltaTime), 50LL)) / 1000.0f;

        // Update scene
        sceneManager.update(dt);
        
        // Render to canvas
        sceneManager.render(canvas);
        
        // Export frame as PGM every 2 frames
        if (frameNum % 2 == 0) {
            std::stringstream filename;
            filename << "frame_" << std::setfill('0') << std::setw(3) << frameNum << ".pgm";
            
            if (ImageExporter::exportToPGM(canvas, filename.str(), 6)) {
                std::cout << "Exported " << filename.str() << "\n";
            } else {
                std::cout << "Failed to export " << filename.str() << "\n";
            }
        }
        
        // Show improved ASCII visualization every 5 frames
        if (frameNum % 5 == 0) {
            std::cout << "\n=== Frame " << frameNum << " - Visual Output ===\n";
            ImageExporter::printColorVisual(canvas, "Orbital Animation");
        }
        
        frameNum++;
        
        // Faster frame rate for animation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n=== Graphics Demo Complete ===\n";
    std::cout << "Exported " << (TOTAL_FRAMES / 2) << " PGM image files\n";
    std::cout << "You can view the .pgm files with image viewers that support the format\n";
    std::cout << "- Linux: display, eog, gimp\n";
    std::cout << "- macOS: Preview (after converting), Photoshop\n";
    std::cout << "- Windows: IrfanView, GIMP\n";
    std::cout << "\nOr convert to PNG: convert frame_000.pgm frame_000.png\n";
    
    return 0;
}