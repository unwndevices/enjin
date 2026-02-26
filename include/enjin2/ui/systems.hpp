#pragma once

#include "system.hpp"
#include "components.hpp"
#include "../graphics/canvas.hpp"
#include "../graphics/primitives.hpp"
#include <algorithm>

namespace enjin2 {

/**
 * @brief Animation system for updating time-based animations
 * 
 * Updates all entities with AnimationComponent, handling timing,
 * looping, and ping-pong behavior.
 */
class AnimationSystem : public System<AnimationSystem> {
public:
    /**
     * @brief Update all animations
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        // In a real implementation, this would iterate over all entities
        // with AnimationComponent using the component storage system
        
        // Pseudo-code for the iteration:
        // for (auto [entity, animation] : animationStorage) {
        //     if (animation->playing) {
        //         updateAnimation(*animation, deltaTime);
        //     }
        // }
    }
    
    /**
     * @brief Get system priority (animations should run early)
     * @return Priority value
     */
    int getPriority() const override { return 10; }

private:
    /**
     * @brief Update individual animation component
     * @param animation Animation to update
     * @param dt Delta time in seconds
     */
    void updateAnimation(AnimationComponent& animation, float dt) {
        if (!animation.playing) return;

        animation.currentTime += dt * animation.speed;
        
        if (animation.currentTime >= animation.duration) {
            if (animation.looping) {
                if (animation.pingPong) {
                    // Reverse direction for ping-pong
                    animation.speed = -animation.speed;
                    animation.currentTime = animation.duration;
                } else {
                    // Reset for normal loop
                    animation.currentTime = 0.0f;
                }
            } else {
                // Clamp to end for one-shot animation
                animation.currentTime = animation.duration;
                animation.playing = false;
            }
        } else if (animation.currentTime < 0.0f && animation.pingPong) {
            // Handle ping-pong reverse
            animation.speed = -animation.speed;
            animation.currentTime = 0.0f;
        }
    }
};

/**
 * @brief Input system for handling user interaction
 * 
 * Processes input events and updates InputComponent states.
 * Handles hit testing against entity bounds.
 */
class InputSystem : public System<InputSystem> {
private:
    Point mousePos;      ///< Current mouse position
    bool mousePressed;   ///< Mouse button state
    bool mouseClicked;   ///< Mouse clicked this frame
    
public:
    /**
     * @brief Constructor initializes input state
     */
    InputSystem() : mousePos(), mousePressed(false), mouseClicked(false) {}
    
    /**
     * @brief Update input processing
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        // Reset transient states
        mouseClicked = false;
        
        // In a real implementation, this would:
        // 1. Iterate over all entities with InputComponent + PositionComponent + SizeComponent
        // 2. Perform hit testing against entity bounds
        // 3. Update hover/focus states
        // 4. Generate input events
        
        // Pseudo-code:
        // for (auto [entity, input, pos, size] : query<InputComponent, PositionComponent, SizeComponent>()) {
        //     Rect bounds(pos->position.x, pos->position.y, size->size.width, size->size.height);
        //     bool wasHovered = input->hovered;
        //     
        //     input->hovered = bounds.contains(mousePos.x, mousePos.y);
        //     
        //     if (input->hovered && !wasHovered) {
        //         input->onHoverEnter();
        //     } else if (!input->hovered && wasHovered) {
        //         input->onHoverExit();
        //     }
        //     
        //     if (input->hovered && mouseClicked) {
        //         input->onPress(mousePos);
        //     }
        //     
        //     input->resetTransientState();
        // }
    }
    
    /**
     * @brief Handle mouse move event
     * @param pos New mouse position
     */
    void onMouseMove(Point pos) {
        mousePos = pos;
    }
    
    /**
     * @brief Handle mouse button press
     * @param pos Mouse position
     */
    void onMousePress(Point pos) {
        mousePos = pos;
        mousePressed = true;
        mouseClicked = true;
    }
    
    /**
     * @brief Handle mouse button release
     * @param pos Mouse position
     */
    void onMouseRelease(Point pos) {
        mousePos = pos;
        mousePressed = false;
    }
    
    /**
     * @brief Get system priority (input should run first)
     * @return Priority value
     */
    int getPriority() const override { return 0; }
};

/**
 * @brief Rendering system for drawing entities to canvas
 * @tparam TCanvas Canvas type for rendering
 * 
 * Renders all visible entities with RenderComponent to the target canvas.
 * Handles z-ordering and shape rendering.
 */
template<typename TCanvas>
class RenderSystem : public System<RenderSystem<TCanvas>> {
private:
    TCanvas* canvas;                     ///< Target canvas for rendering
    std::vector<Entity> sortedEntities; ///< Entities sorted by z-order
    
public:
    /**
     * @brief Constructor with target canvas
     * @param targetCanvas Canvas to render to
     */
    RenderSystem(TCanvas* targetCanvas) : canvas(targetCanvas) {}
    
    /**
     * @brief Update rendering
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        if (!canvas) return;
        
        // Clear canvas
        canvas->clear(Colors::BLACK);
        
        // In a real implementation:
        // 1. Collect all renderable entities
        // 2. Sort by z-order
        // 3. Render each entity based on its components
        
        // Pseudo-code:
        // sortedEntities.clear();
        // for (auto [entity, render] : query<RenderComponent>()) {
        //     if (render->shouldRender()) {
        //         sortedEntities.push_back(entity);
        //     }
        // }
        // 
        // std::sort(sortedEntities.begin(), sortedEntities.end(),
        //           [this](Entity a, Entity b) {
        //               auto renderA = getComponent<RenderComponent>(a);
        //               auto renderB = getComponent<RenderComponent>(b);
        //               return renderA->zOrder < renderB->zOrder;
        //           });
        // 
        // for (Entity entity : sortedEntities) {
        //     renderEntity(entity);
        // }
    }
    
    /**
     * @brief Get system priority (rendering should run last)
     * @return Priority value
     */
    int getPriority() const override { return 1000; }

private:
    /**
     * @brief Render individual entity
     * @param entity Entity to render
     */
    void renderEntity(Entity entity) {
        // This would get components and render based on entity type
        // auto pos = getComponent<PositionComponent>(entity);
        // auto size = getComponent<SizeComponent>(entity);
        // auto render = getComponent<RenderComponent>(entity);
        // auto shape = getComponent<ShapeComponent>(entity);
        // 
        // if (!pos || !render) return;
        // 
        // if (shape) {
        //     renderShape(*pos, size, *render, *shape);
        // } else if (size) {
        //     renderRectangle(*pos, *size, *render);
        // }
    }
    
    /**
     * @brief Render shape component
     * @param pos Position component
     * @param size Size component (may be null)
     * @param render Render component
     * @param shape Shape component
     */
    void renderShape(const PositionComponent& pos, const SizeComponent* size,
                    const RenderComponent& render, const ShapeComponent& shape) {
        using Primitives = Primitives<typename TCanvas::PixelType>;
        
        switch (shape.type) {
            case ShapeComponent::RECTANGLE:
                if (size) {
                    Rect rect(pos.position.x, pos.position.y, size->size.width, size->size.height);
                    if (shape.filled) {
                        Primitives::fillRect(*canvas, rect, render.color);
                    } else {
                        Primitives::drawRect(*canvas, rect, render.color);
                    }
                }
                break;
                
            case ShapeComponent::CIRCLE:
                if (shape.filled) {
                    Primitives::fillCircle(*canvas, pos.position.x, pos.position.y,
                                         shape.radius, render.color);
                } else {
                    Primitives::drawCircle(*canvas, pos.position.x, pos.position.y,
                                         shape.radius, render.color);
                }
                break;
                
            case ShapeComponent::TRIANGLE:
                if (shape.filled) {
                    Primitives::fillTriangle(*canvas,
                        pos.position.x + shape.p1.x,
                        pos.position.y + shape.p1.y,
                        pos.position.x + shape.p2.x,
                        pos.position.y + shape.p2.y,
                        pos.position.x + shape.p3.x,
                        pos.position.y + shape.p3.y,
                        render.color);
                } else {
                    Primitives::drawTriangle(*canvas,
                        pos.position.x + shape.p1.x,
                        pos.position.y + shape.p1.y,
                        pos.position.x + shape.p2.x,
                        pos.position.y + shape.p2.y,
                        pos.position.x + shape.p3.x,
                        pos.position.y + shape.p3.y,
                        render.color);
                }
                break;
                
            case ShapeComponent::LINE:
                Primitives::drawLine(*canvas,
                    pos.position.x + shape.start.x,
                    pos.position.y + shape.start.y,
                    pos.position.x + shape.end.x,
                    pos.position.y + shape.end.y,
                    render.color);
                break;
        }
    }
    
    /**
     * @brief Render simple rectangle
     * @param pos Position component
     * @param size Size component
     * @param render Render component
     */
    void renderRectangle(const PositionComponent& pos, const SizeComponent& size,
                        const RenderComponent& render) {
        using Primitives = Primitives<typename TCanvas::PixelType>;
        
        Rect rect(pos.position.x, pos.position.y, size.size.width, size.size.height);
        Primitives::fillRect(*canvas, rect, render.color);
    }
};

/**
 * @brief Type alias for 4-bit render system
 */
using RenderSystem4 = RenderSystem<Canvas4<128, 64>>;

/**
 * @brief Type alias for 8-bit render system
 */
using RenderSystem8 = RenderSystem<Canvas8<128, 64>>;

} // namespace enjin2