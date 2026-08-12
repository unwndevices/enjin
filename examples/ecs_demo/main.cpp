#include "../enjin2/ui/component.hpp"
#include "../enjin2/ui/system.hpp"
#include "../enjin2/ui/components.hpp"
#include "../enjin2/ui/systems.hpp"
#include "../enjin2/graphics/canvas.hpp"
#include <iostream>
#include <memory>
#include <cmath>
#include <vector>

using namespace enjin2;

/**
 * @brief World class managing ECS components and systems
 * 
 * Demonstrates Phase 2 memory management improvements:
 * - Static object pools for components
 * - Handle-based entity system
 * - Zero heap allocation after initialization
 */
class World {
private:
    // Entity management
    EntityManager entityManager;
    
    // Component storage (static pools)
    ComponentStorage<PositionComponent, 256> positions;
    ComponentStorage<SizeComponent, 256> sizes;
    ComponentStorage<RenderComponent, 256> renderables;
    ComponentStorage<AnimationComponent, 128> animations;
    ComponentStorage<InputComponent, 64> inputs;
    
    // Systems
    SystemManager<8> systemManager;
    
    // Canvas for rendering
    Canvas4<128, 64> canvas;
    
    // System instances (allocated once)
    std::unique_ptr<AnimationSystem> animationSystem;
    std::unique_ptr<InputSystem> inputSystem;
    std::unique_ptr<RenderSystem4> renderSystem;
    
public:
    /**
     * @brief Initialize world with systems
     */
    World() {
        // Create systems
        animationSystem = std::make_unique<AnimationSystem>();
        inputSystem = std::make_unique<InputSystem>();
        renderSystem = std::make_unique<RenderSystem4>(&canvas);
        
        // Add systems to manager (automatically sorted by priority)
        systemManager.addSystem(inputSystem.get());      // Priority 0
        systemManager.addSystem(animationSystem.get());  // Priority 10  
        systemManager.addSystem(renderSystem.get());     // Priority 1000
        
        std::cout << "ECS World initialized with " << systemManager.getSystemCount() 
                  << " systems\n";
    }
    
    /**
     * @brief Create entity with position and size
     * @param pos Initial position
     * @param size Initial size
     * @return Entity handle
     */
    Entity createRectangle(Point pos, Size size) {
        Entity entity = entityManager.createEntity();
        if (!entity.isValid()) return entity;
        
        // Add components using static pools
        positions.addComponent(entity, pos);
        sizes.addComponent(entity, size);
        renderables.addComponent(entity, Colors::WHITE, Colors::BLACK, 255, true, 0);

        return entity;
    }
    
    /**
     * @brief Create animated circle entity
     * @param pos Initial position
     * @param radius Circle radius
     * @return Entity handle
     */
    Entity createAnimatedCircle(Point pos, uint16_t radius) {
        Entity entity = entityManager.createEntity();
        if (!entity.isValid()) return entity;
        
        positions.addComponent(entity, pos);
        // Retired shape primitives (unwn #206): the demo renders a filled box
        // over the entity's Size, so give the "circle" a square Size box.
        sizes.addComponent(entity, Size(static_cast<uint16_t>(radius * 2),
                                        static_cast<uint16_t>(radius * 2)));
        renderables.addComponent(entity, Colors::LIGHT_GRAY, Colors::BLACK, 255, true, 1);

        // Add animation that loops every 2 seconds
        auto* anim = animations.addComponent(entity, 2.0f, true, false, 1.0f);
        if (anim) {
            anim->play();
        }
        
        return entity;
    }
    
    /**
     * @brief Create interactive button entity
     * @param pos Position
     * @param size Size
     * @return Entity handle
     */
    Entity createButton(Point pos, Size size) {
        Entity entity = entityManager.createEntity();
        if (!entity.isValid()) return entity;
        
        positions.addComponent(entity, pos);
        sizes.addComponent(entity, size);
        renderables.addComponent(entity, Colors::GRAY, Colors::DARK_GRAY, 255, true, 2);
        inputs.addComponent(entity, true);
        
        return entity;
    }
    
    /**
     * @brief Update world simulation
     * @param deltaTime Time since last update
     */
    void update(float deltaTime) {
        // Update entity logic based on components
        updateAnimatedEntities(deltaTime);
        updateInteractiveEntities();
        
        // Manual rendering since the systems are just examples
        renderEntities();
        
        // Update all systems in priority order
        systemManager.update(deltaTime);
    }
    
    /**
     * @brief Get canvas for display
     * @return Reference to render canvas
     */
    const Canvas4<128, 64>& getCanvas() const {
        return canvas;
    }
    
    /**
     * @brief Get memory usage statistics
     */
    void printMemoryStats() const {
        std::cout << "\nMemory Usage Statistics:\n";
        std::cout << "Entities: " << entityManager.getEntityCount() 
                  << "/" << entityManager.getMaxEntities() << "\n";
        std::cout << "Positions: " << positions.size() << "/256\n";
        std::cout << "Sizes: " << sizes.size() << "/256\n";
        std::cout << "Renderables: " << renderables.size() << "/256\n";
        std::cout << "Animations: " << animations.size() << "/128\n";
        std::cout << "Inputs: " << inputs.size() << "/64\n";

        // Calculate total memory usage
        size_t totalMemory = sizeof(positions) + sizeof(sizes) + sizeof(renderables) +
                           sizeof(animations) + sizeof(inputs) +
                           sizeof(entityManager) + sizeof(systemManager) + sizeof(canvas);
        
        std::cout << "Total static memory: " << totalMemory << " bytes\n";
        std::cout << "Canvas memory: " << canvas.getBufferSize() << " bytes (4-bit packed)\n";
    }

private:
    /**
     * @brief Update entities with animations
     * @param deltaTime Delta time
     */
    void updateAnimatedEntities(float deltaTime) {
        // Iterate through animated entities
        for (auto [entity, anim] : animations) {
            if (!anim->playing) continue;
            
            auto* pos = positions.getComponent(entity);
            if (!pos) continue;
            
            // Example: circular motion animation
            float progress = anim->getProgress();
            float angle = progress * 2.0f * 3.14159f; // Full circle
            
            Point center(64, 32); // Canvas center
            int16_t radius = 20;
            
            Point newPos;
            newPos.x = center.x + static_cast<int16_t>(radius * cos(angle));
            newPos.y = center.y + static_cast<int16_t>(radius * sin(angle));
            
            pos->moveTo(newPos);
        }
    }
    
    /**
     * @brief Update interactive entities
     */
    void updateInteractiveEntities() {
        // Example: Change button color when hovered
        for (auto [entity, input] : inputs) {
            auto* render = renderables.getComponent(entity);
            if (!render) continue;
            
            if (input->hovered) {
                render->setColor(Colors::WHITE);
            } else if (input->pressed) {
                render->setColor(Colors::DARK_GRAY);
            } else {
                render->setColor(Colors::GRAY);
            }
        }
    }
    
    /**
     * @brief Render all entities to canvas
     */
    void renderEntities() {
        // Clear canvas
        canvas.clear(Colors::BLACK);
        
        // Debug: print what we're about to render
        std::cout << "Rendering " << renderables.size() << " renderable entities\n";
        
        // Render all entities with position and render components
        for (auto [entity, render] : renderables) {
            if (!render->shouldRender()) continue;
            
            auto* pos = positions.getComponent(entity);
            if (!pos) continue;
            
            auto* size = sizes.getComponent(entity);
            if (size) {
                // Render simple rectangle
                Rect rect(pos->position.x, pos->position.y, size->size.width, size->size.height);
                Primitives4::fillRect(canvas, rect, render->color);
            } else {
                // Render single pixel
                canvas.setPixel(pos->position.x, pos->position.y, render->color);
            }
        }
    }
};

/**
 * @brief Print canvas to console as ASCII art
 * @param canvas Canvas to print
 */
void printCanvas(const Canvas4<128, 64>& canvas) {
    // Print every 4th row and every 2nd column for console display
    for (int y = 0; y < 64; y += 4) {
        for (int x = 0; x < 128; x += 2) {
            Pixel4 pixel = canvas.getPixel(x, y);
            char c;
            if (pixel.value == 0) c = ' ';
            else if (pixel.value < 4) c = '.';
            else if (pixel.value < 8) c = '+';
            else if (pixel.value < 12) c = '#';
            else c = '@';
            std::cout << c;
        }
        std::cout << '\n';
    }
}

int main() {
    std::cout << "Enjin 2.0 ECS Demo - Phase 2 Memory Management\n";
    std::cout << "===============================================\n\n";
    
    // Create world with static memory allocation
    World world;
    
    // Create some entities to demonstrate the system
    Entity rect1 = world.createRectangle(Point(10, 10), Size(20, 15));
    Entity rect2 = world.createRectangle(Point(50, 20), Size(15, 10));
    Entity circle = world.createAnimatedCircle(Point(64, 32), 5);
    Entity button = world.createButton(Point(90, 45), Size(25, 12));
    
    std::cout << "Created entities:\n";
    std::cout << "- Rectangle 1: ID=" << rect1.id << ", Gen=" << (int)rect1.generation << "\n";
    std::cout << "- Rectangle 2: ID=" << rect2.id << ", Gen=" << (int)rect2.generation << "\n";
    std::cout << "- Animated Circle: ID=" << circle.id << ", Gen=" << (int)circle.generation << "\n";
    std::cout << "- Button: ID=" << button.id << ", Gen=" << (int)button.generation << "\n\n";
    
    // Simulate several frames
    float time = 0.0f;
    const float deltaTime = 0.1f;
    
    for (int frame = 0; frame < 5; ++frame) {
        std::cout << "Frame " << frame << " (t=" << time << "s):\n";
        
        world.update(deltaTime);
        printCanvas(world.getCanvas());
        
        std::cout << "\n";
        time += deltaTime;
    }
    
    // Show memory statistics
    world.printMemoryStats();
    
    std::cout << "\nECS Demo completed successfully!\n";
    std::cout << "Key improvements in Phase 2:\n";
    std::cout << "- Zero heap allocations during runtime\n";
    std::cout << "- Handle-based entity system (no raw pointers)\n";
    std::cout << "- Static component pools for cache efficiency\n";
    std::cout << "- Component storage with O(1) add/remove\n";
    std::cout << "- Memory-efficient packed entity arrays\n";
    
    return 0;
}