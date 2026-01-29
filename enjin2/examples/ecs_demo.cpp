/**
 * @file ecs_demo.cpp
 * @brief Demonstration of the Enjin2 ECS system
 * 
 * Shows how to create objects, components, scenes, and manage
 * the complete lifecycle of an Enjin2 application.
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include "../include/enjin2/core/scene.hpp"
#include "../include/enjin2/core/scene_state_machine.hpp"
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/position.hpp"
#include "../include/enjin2/components/drawable.hpp"
#include "../include/enjin2/graphics/canvas.hpp"

using namespace enjin2;

/**
 * @brief Simple rectangle drawable component for testing
 */
class RectangleDrawable : public C_Drawable {
private:
    Pixel4 color;
    
public:
    /**
     * @brief Constructor
     */
    RectangleDrawable(Object* owner, uint16_t width, uint16_t height, Pixel4 rectColor)
        : C_Drawable(owner, width, height), color(rectColor) {}
    
    /**
     * @brief Draw to 4-bit canvas
     */
    void draw(ICanvas<Pixel4>& canvas) override {
        if (!isVisible()) return;
        
        Point pos = getRenderPosition();
        Size sz = getSize();
        
        
        // Draw filled rectangle
        for (uint16_t y = 0; y < sz.height; ++y) {
            for (uint16_t x = 0; x < sz.width; ++x) {
                canvas.setPixel(pos.x + x, pos.y + y, color);
            }
        }
    }
    
    /**
     * @brief Draw to 8-bit canvas
     */
    void draw(ICanvas<uint8_t>& canvas) override {
        if (!isVisible()) return;
        
        Point pos = getRenderPosition();
        Size sz = getSize();
        
        // Convert 4-bit color to 8-bit
        uint8_t color8 = color.value * 17; // Scale 0-15 to 0-255
        
        // Draw filled rectangle
        for (uint16_t y = 0; y < sz.height; ++y) {
            for (uint16_t x = 0; x < sz.width; ++x) {
                canvas.setPixel(pos.x + x, pos.y + y, color8);
            }
        }
    }
    
    /**
     * @brief Set rectangle color
     */
    void setColor(Pixel4 newColor) {
        color = newColor;
    }
};

/**
 * @brief Moving object that bounces around the screen
 */
class BouncingObject : public Object {
private:
    Point velocity;
    Rect bounds;
    
public:
    /**
     * @brief Constructor
     */
    BouncingObject(int16_t x, int16_t y, Pixel4 color, const Rect& screenBounds)
        : velocity(2, 1), bounds(screenBounds) {
        
        // Set initial position
        auto pos = getComponent<C_Position>();
        if (pos) {
            pos->setPosition(x, y);
        }
        
        // Add rectangle drawable
        addComponent<RectangleDrawable>(20, 15, color);
    }
    
    /**
     * @brief Update position and handle bouncing
     */
    void update(uint16_t deltaTime) override {
        Object::update(deltaTime);
        
        auto pos = getComponent<C_Position>();
        auto drawable = getComponent<RectangleDrawable>();
        if (!pos || !drawable) return;
        
        Point currentPos = pos->getPosition();
        Size size = drawable->getSize();
        
        // Update position
        currentPos.x += velocity.x;
        currentPos.y += velocity.y;
        
        // Bounce off walls
        if (currentPos.x <= bounds.x || currentPos.x + static_cast<int16_t>(size.width) >= bounds.x + static_cast<int16_t>(bounds.width)) {
            velocity.x = -velocity.x;
            currentPos.x = std::max(bounds.x, std::min(currentPos.x, static_cast<int16_t>(bounds.x + bounds.width - size.width)));
        }
        
        if (currentPos.y <= bounds.y || currentPos.y + static_cast<int16_t>(size.height) >= bounds.y + static_cast<int16_t>(bounds.height)) {
            velocity.y = -velocity.y;
            currentPos.y = std::max(bounds.y, std::min(currentPos.y, static_cast<int16_t>(bounds.y + bounds.height - size.height)));
        }
        
        pos->setPosition(currentPos.x, currentPos.y);
    }
};

/**
 * @brief Demo scene with bouncing objects
 */
class DemoScene : public Scene {
private:
    Rect screenBounds;
    uint32_t frameCount;
    
public:
    /**
     * @brief Constructor
     */
    DemoScene(uint32_t id, const Rect& bounds) 
        : Scene(id), screenBounds(bounds), frameCount(0) {}
    
protected:
    /**
     * @brief Create initial objects
     */
    void onCreate() override {
        std::cout << "DemoScene: Creating scene objects...\n";
        
        // Create several bouncing objects with different colors
        addObject<BouncingObject>(10, 10, Pixel4(15), screenBounds);   // White
        addObject<BouncingObject>(50, 30, Pixel4(10), screenBounds);   // Light gray
        addObject<BouncingObject>(80, 20, Pixel4(5), screenBounds);    // Dark gray
        addObject<BouncingObject>(30, 50, Pixel4(12), screenBounds);   // Another shade
        
        std::cout << "DemoScene: Created " << getObjects().size() << " objects\n";
    }
    
    /**
     * @brief Scene activated
     */
    void onActivate() override {
        std::cout << "DemoScene: Scene activated\n";
    }
    
    /**
     * @brief Scene deactivated
     */
    void onDeactivate() override {
        std::cout << "DemoScene: Scene deactivated\n";
    }
    
    /**
     * @brief Frame update
     */
    void onUpdate(uint16_t deltaTime) override {
        frameCount++;
        
        // Print status every 60 frames (roughly 1 second at 60fps)
        if (frameCount % 60 == 0) {
            std::cout << "DemoScene: Frame " << frameCount 
                      << ", Active objects: " << getObjects().size()
                      << ", Delta: " << deltaTime << "ms\n";
        }
    }
    
    /**
     * @brief Render 4-bit background
     */
    void onRender(ICanvas<Pixel4>& canvas) override {
        canvas.clear(Pixel4(1)); // Dark background
    }
    
    /**
     * @brief Render 8-bit background
     */
    void onRender(ICanvas<uint8_t>& canvas) override {
        canvas.clear(static_cast<uint8_t>(17)); // Dark 8-bit background
    }
};

/**
 * @brief Main demo application
 */
int main() {
    std::cout << "=== Enjin2 ECS System Demo ===\n\n";
    
    // Create canvas for rendering
    constexpr uint16_t CANVAS_WIDTH = 128;
    constexpr uint16_t CANVAS_HEIGHT = 64;
    Canvas4<CANVAS_WIDTH, CANVAS_HEIGHT> canvas;
    
    // Create scene state machine
    SceneStateMachine sceneManager;
    
    // Add demo scene
    Rect screenBounds(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
    auto* demoScene = sceneManager.addScene<DemoScene>(1, screenBounds);
    if (!demoScene) {
        std::cerr << "Failed to create demo scene!\n";
        return 1;
    }
    
    // Set up scene transition callbacks
    auto onSceneChange = sceneManager.connectOnSceneChangeComplete(
        [](Scene* from, Scene* to) {
            std::cout << "Scene transition complete: " 
                      << (from ? std::to_string(from->getId()) : "null")
                      << " -> " 
                      << (to ? std::to_string(to->getId()) : "null") << "\n";
        });
    
    // Start the demo scene
    if (!sceneManager.changeScene(1)) {
        std::cerr << "Failed to start demo scene!\n";
        return 1;
    }
    
    std::cout << "Starting main loop...\n";
    std::cout << "Demo will run for 300 frames (about 5 seconds)\n\n";
    
    // Main loop
    auto lastTime = std::chrono::steady_clock::now();
    constexpr int MAX_FRAMES = 300;
    int frameNum = 0;
    
    while (frameNum < MAX_FRAMES) {
        auto currentTime = std::chrono::steady_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Cap delta time to prevent large jumps
        uint16_t deltaMs = static_cast<uint16_t>(std::min(static_cast<long long>(deltaTime), 33LL)); // Max 33ms (30fps)
        
        // Update scene manager
        sceneManager.update(deltaMs);
        
        // Render
        sceneManager.render(canvas);
        
        // Every 50 frames, show animated canvas view
        if (frameNum % 50 == 0) {
            auto* scene = sceneManager.getCurrentScene();
            if (scene && scene->getObjects().size() > 0) {
                auto* firstObj = scene->getObjects().getObject(0);
                if (firstObj && firstObj->getPosition()) {
                    auto pos = firstObj->getPosition()->getPosition();
                    int sampleX = std::max(0, std::min(pos.x - 5, canvas.getWidth() - 20));
                    int sampleY = std::max(0, std::min(pos.y - 5, canvas.getHeight() - 12));
                    
                    std::cout << "\n=== Frame " << frameNum << " - Bouncing Objects Demo ===\n";
                    std::cout << "Canvas view around (" << sampleX << ", " << sampleY << "):\n";
                    
                    for (int y = 0; y < 12; ++y) {
                        for (int x = 0; x < 20; ++x) {
                            Pixel4 pixel = canvas.getPixel(sampleX + x, sampleY + y);
                            if (pixel.value == 1) {
                                std::cout << ". ";  // Background
                            } else if (pixel.value >= 10) {
                                std::cout << "█ ";  // Object (bright)
                            } else {
                                std::cout << "▓ ";  // Object (dim)
                            }
                        }
                        std::cout << "\n";
                    }
                    
                    // Show object positions
                    std::cout << "Object positions: ";
                    scene->getObjects().forEach([&](Object* obj) {
                        if (obj && obj->getPosition()) {
                            auto objPos = obj->getPosition()->getPosition();
                            std::cout << "(" << objPos.x << "," << objPos.y << ") ";
                        }
                    });
                    std::cout << "\n";
                }
            }
        }
        
        frameNum++;
        
        // Sleep to maintain roughly 60fps
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    std::cout << "\nDemo completed!\n";
    std::cout << "Final stats:\n";
    std::cout << "- Frames rendered: " << frameNum << "\n";
    std::cout << "- Current scene: " << (sceneManager.getCurrentScene() ? 
                                         std::to_string(sceneManager.getCurrentScene()->getId()) : "none") << "\n";
    
    if (auto* scene = sceneManager.getCurrentScene()) {
        std::cout << "- Objects in scene: " << scene->getObjects().size() << "\n";
    }
    
    return 0;
}