/**
 * @file space_ui_demo.cpp
 * @brief Comprehensive demo of Enjin2's space-themed UI components
 * 
 * Demonstrates the orbital visualization system with planets, satellites,
 * probes, and animations that make Enjin2 unique for audio applications.
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include <enjin2/core/scene.hpp>
#include <enjin2/core/scene_state_machine.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/components/planet.hpp>
#include <enjin2/components/satellite.hpp>
#include <enjin2/components/probe.hpp>
#include <enjin2/components/animation.hpp>
#include <enjin2/graphics/canvas.hpp>

using namespace enjin2;

/**
 * @brief Audio parameter control object with planet and satellites
 */
class AudioControlCenter : public Object {
private:
    C_Planet* planet;
    C_Satellite* satellites[4];
    
public:
    /**
     * @brief Constructor
     */
    AudioControlCenter(Point center) {
        // Set position
        auto pos = getComponent<C_Position>();
        if (pos) {
            pos->setPosition(center.x - 25, center.y - 25);  // Center the 50x50 planet
        }
        
        // Add main planet (represents the audio module)
        planet = addComponent<C_Planet>(25.0f, Pixel4(12));  // Large gray planet
        planet->setAtmosphere(true, 1.8f, Pixel4(6));       // Atmospheric glow
        planet->setPulsing(true, 1.5f, 0.15f);              // Gentle pulsing
        planet->setRotation(true, 0.3f);                    // Slow rotation
        
        // Initialize satellites array
        for (int i = 0; i < 4; ++i) {
            satellites[i] = nullptr;
        }
    }
    
    /**
     * @brief Add satellite for parameter control
     */
    void addSatellite(int index, float orbitRadius, float startAngle, Pixel4 color) {
        if (index >= 0 && index < 4) {
            satellites[index] = addComponent<C_Satellite>(planet, orbitRadius, startAngle);
            satellites[index]->setAppearance(3.0f, color, true);
            satellites[index]->setOrbit(orbitRadius, 0.8f + index * 0.2f);  // Different speeds
            satellites[index]->setParameterEffects(true, false, true);      // Radius and color from param
            satellites[index]->setTrail(true, Pixel4(color.value / 2));
        }
    }
    
    /**
     * @brief Update parameter values (simulates audio parameter changes)
     */
    void updateParameters(uint32_t time) {
        for (int i = 0; i < 4; ++i) {
            if (satellites[i]) {
                // Simulate parameter changes with different frequencies
                float t = (time / 1000.0f) + i * 1.5f;
                float param = 0.5f + 0.4f * std::sin(t * (0.5f + i * 0.3f));
                satellites[i]->setParameterValue(param);
            }
        }
    }
};

/**
 * @brief Scanner probe object
 */
class ScannerProbe : public Object {
public:
    /**
     * @brief Constructor
     */
    ScannerProbe(Point startPos) {
        auto pos = getComponent<C_Position>();
        if (pos) {
            pos->setPosition(startPos.x, startPos.y);
        }
        
        auto probe = addComponent<C_Probe>(C_Probe::ProbeType::SCANNER, 4.0f);
        probe->setAppearance(C_Probe::ProbeType::SCANNER, 4.0f, Pixel4(15), Pixel4(10));
        probe->setMovement(C_Probe::MovementPattern::SINE_WAVE, 0.8f);
        probe->setPulsing(true, 3.0f);
        probe->setScanner(20.0f, 2.0f);
        probe->setTrail(false);  // Scanner doesn't need trail
    }
};

/**
 * @brief Particle field for ambient effects
 */
class ParticleField : public Object {
private:
    C_Probe* particles[8];
    
public:
    /**
     * @brief Constructor
     */
    ParticleField(Rect area) {
        auto pos = getComponent<C_Position>();
        if (pos) {
            pos->setPosition(area.x, area.y);
        }
        
        // Create multiple small particle probes
        for (int i = 0; i < 8; ++i) {
            particles[i] = addComponent<C_Probe>(C_Probe::ProbeType::PARTICLE, 1.5f);
            particles[i]->setAppearance(C_Probe::ProbeType::PARTICLE, 1.5f + i * 0.2f, 
                                       Pixel4(6 + i), Pixel4(3 + i/2));
            particles[i]->setMovement(C_Probe::MovementPattern::RANDOM_WALK, 0.3f + i * 0.1f);
            particles[i]->setMovementBounds(area, true);
            particles[i]->setPulsing(true, 1.0f + i * 0.5f);
            particles[i]->setTrail(i % 2 == 0);  // Half have trails
        }
    }
};

/**
 * @brief Main space UI demo scene
 */
class SpaceUIScene : public Scene {
private:
    AudioControlCenter* controlCenter;
    ScannerProbe* scanner;
    ParticleField* particles;
    uint32_t sceneTime;
    
public:
    /**
     * @brief Constructor
     */
    SpaceUIScene(uint32_t id) : Scene(id), sceneTime(0) {}
    
protected:
    /**
     * @brief Create space UI elements
     */
    void onCreate() override {
        std::cout << "SpaceUIScene: Creating orbital interface...\n";
        
        // Main audio control center (planet with satellites)
        controlCenter = addObject<AudioControlCenter>(Point(64, 32));
        
        // Add satellites for different audio parameters
        controlCenter->addSatellite(0, 35.0f, 0.0f, Pixel4(15));      // Filter frequency (white)
        controlCenter->addSatellite(1, 42.0f, 1.57f, Pixel4(12));     // Resonance (light gray)
        controlCenter->addSatellite(2, 50.0f, 3.14f, Pixel4(9));      // Gain (medium gray)
        controlCenter->addSatellite(3, 58.0f, 4.71f, Pixel4(6));      // Drive (dark gray)
        
        // Scanner probe for visualizing audio analysis
        scanner = addObject<ScannerProbe>(Point(10, 45));
        
        // Ambient particle field for atmosphere
        particles = addObject<ParticleField>(Rect(0, 0, 128, 64));
        
        std::cout << "SpaceUIScene: Created orbital control system with "
                  << getObjects().size() << " objects\n";
    }
    
    /**
     * @brief Scene activated
     */
    void onActivate() override {
        std::cout << "SpaceUIScene: Orbital interface activated\n";
    }
    
    /**
     * @brief Update scene
     */
    void onUpdate(uint16_t deltaTime) override {
        sceneTime += deltaTime;
        
        // Update audio parameters (simulated)
        if (controlCenter) {
            controlCenter->updateParameters(sceneTime);
        }
        
        // Print status every 2 seconds
        if (sceneTime % 2000 < deltaTime) {
            std::cout << "SpaceUIScene: T+" << (sceneTime/1000) << "s - "
                      << getObjects().size() << " objects active\n";
        }
    }
    
    /**
     * @brief Render space background
     */
    void onRender(ICanvas<Pixel4>& canvas) override {
        // Dark space background
        canvas.clear(Pixel4(1));
        
        // Add some stars (static dots)
        static bool starsDrawn = false;
        if (!starsDrawn) {
            for (int i = 0; i < 20; ++i) {
                // Simple pseudo-random star positions
                int x = (i * 17 + 23) % 128;
                int y = (i * 31 + 47) % 64;
                canvas.setPixel(x, y, Pixel4(3 + (i % 3)));
            }
            starsDrawn = true;
        }
    }
    
    /**
     * @brief Render space background (8-bit)
     */
    void onRender(ICanvas<uint8_t>& canvas) override {
        canvas.clear(17);  // Dark space
        
        static bool starsDrawn = false;
        if (!starsDrawn) {
            for (int i = 0; i < 20; ++i) {
                int x = (i * 17 + 23) % 128;
                int y = (i * 31 + 47) % 64;
                canvas.setPixel(x, y, static_cast<uint8_t>(51 + (i % 3) * 20));
            }
            starsDrawn = true;
        }
    }
};

/**
 * @brief Main demo application
 */
int main() {
    std::cout << "=== Enjin2 Space UI Demo ===\n";
    std::cout << "Orbital Audio Interface Visualization\n\n";
    
    // Create canvas for rendering
    constexpr uint16_t CANVAS_WIDTH = 128;
    constexpr uint16_t CANVAS_HEIGHT = 64;
    Canvas4<CANVAS_WIDTH, CANVAS_HEIGHT> canvas;
    
    // Create scene state machine
    SceneStateMachine sceneManager;
    
    // Add space UI scene
    auto* spaceScene = sceneManager.addScene<SpaceUIScene>(1);
    if (!spaceScene) {
        std::cerr << "Failed to create space UI scene!\n";
        return 1;
    }
    
    // Set up scene callbacks
    auto onSceneActive = sceneManager.connectOnSceneChangeComplete(
        [](Scene* from, Scene* to) {
            std::cout << "Scene transition: -> " 
                      << (to ? std::to_string(to->getId()) : "null") << "\n";
        });
    
    // Start the space UI scene
    if (!sceneManager.changeScene(1)) {
        std::cerr << "Failed to start space UI scene!\n";
        return 1;
    }
    
    std::cout << "Starting orbital interface...\n";
    std::cout << "Demo will run for 500 frames (about 8 seconds)\n\n";
    
    // Main loop
    auto lastTime = std::chrono::steady_clock::now();
    constexpr int MAX_FRAMES = 500;
    int frameNum = 0;
    
    while (frameNum < MAX_FRAMES) {
        auto currentTime = std::chrono::steady_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastTime).count();
        lastTime = currentTime;
        
        uint16_t deltaMs = static_cast<uint16_t>(std::min(static_cast<long long>(deltaTime), 33LL));
        
        // Update scene manager
        sceneManager.update(deltaMs);
        
        // Render
        sceneManager.render(canvas);
        
        // Show animated view every 60 frames
        if (frameNum % 60 == 0) {
            std::cout << "\n=== Frame " << frameNum << " - Orbital Interface View ===\n";
            
            // Sample around the center where the planet should be (64, 32)
            for (int section = 0; section < 2; ++section) {
                int startX = 48 + section * 32;  // Center around (64, 32)
                int startY = 16;
                
                std::cout << "Center Sector " << (section + 1) << " (" << startX << "-" << (startX + 32) << "):\n";
                
                for (int y = 0; y < 16; ++y) {
                    for (int x = 0; x < 32; ++x) {
                        Pixel4 pixel = canvas.getPixel(startX + x, startY + y);
                        if (pixel.value <= 1) {
                            std::cout << "  ";  // Space
                        } else if (pixel.value <= 3) {
                            std::cout << "· ";  // Stars
                        } else if (pixel.value <= 6) {
                            std::cout << "▫ ";  // Dim objects
                        } else if (pixel.value <= 10) {
                            std::cout << "▪ ";  // Medium objects
                        } else {
                            std::cout << "⬛";   // Bright objects
                        }
                    }
                    std::cout << "\n";
                }
                std::cout << "\n";
            }
        }
        
        frameNum++;
        
        // Sleep to maintain roughly 60fps
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    std::cout << "\n=== Space UI Demo Complete ===\n";
    std::cout << "Final stats:\n";
    std::cout << "- Frames rendered: " << frameNum << "\n";
    std::cout << "- Scene: Orbital Audio Interface\n";
    
    if (auto* scene = sceneManager.getCurrentScene()) {
        std::cout << "- Active objects: " << scene->getObjects().size() << "\n";
        std::cout << "- Control planets: 1\n";
        std::cout << "- Parameter satellites: 4\n";
        std::cout << "- Scanner probes: 1\n";
        std::cout << "- Ambient particles: 8\n";
    }
    
    std::cout << "\nEnjin2 space-themed UI system ready for audio applications!\n";
    
    return 0;
}