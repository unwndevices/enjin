#pragma once

#include "system.hpp"
#include "components.hpp"
#include "world.hpp"
#include "../graphics/canvas.hpp"
#include "../graphics/primitives.hpp"
#include <algorithm>
#include <vector>

namespace enjin2 {

/**
 * @brief Animation system for updating time-based animations
 * @tparam TWorld World type providing the AnimationComponent storage
 *
 * Advances every playing AnimationComponent in the world, handling looping and
 * ping-pong behaviour.
 */
template<typename TWorld>
class AnimationSystem : public System<AnimationSystem<TWorld>> {
public:
    /**
     * @brief Construct against the world whose animations it drives
     * @param world World to update (borrowed, not owned)
     */
    explicit AnimationSystem(TWorld* world) : world_(world) {}

    /**
     * @brief Update all animations
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        if (!world_) return;
        for (auto [entity, animation] : world_->template components<AnimationComponent>()) {
            (void)entity;
            if (animation->playing) {
                updateAnimation(*animation, dt);
            }
        }
    }

    /**
     * @brief Get system priority (animations should run early)
     * @return Priority value
     */
    int getPriority() const override { return 10; }

private:
    TWorld* world_; ///< World whose animations are updated

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
 * @tparam TWorld World type providing Input/Position/Size storages
 *
 * Hit-tests the current pointer against every entity that has an InputComponent,
 * PositionComponent and SizeComponent, driving hover/press state through the
 * component's own event hooks.
 */
template<typename TWorld>
class InputSystem : public System<InputSystem<TWorld>> {
private:
    TWorld* world_;      ///< World whose input state is updated
    Point mousePos;      ///< Current mouse position
    bool mousePressed;   ///< Mouse button state
    bool mouseClicked;   ///< Mouse clicked this frame

public:
    /**
     * @brief Construct against the world it processes input for
     * @param world World to update (borrowed, not owned)
     */
    explicit InputSystem(TWorld* world)
        : world_(world), mousePos(), mousePressed(false), mouseClicked(false) {}

    /**
     * @brief Update input processing
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        (void)dt;
        if (!world_) return;

        for (Entity e : world_->template query<InputComponent, PositionComponent, SizeComponent>()) {
            auto* input = world_->template get<InputComponent>(e);
            auto* pos = world_->template get<PositionComponent>(e);
            auto* size = world_->template get<SizeComponent>(e);
            if (!input || !pos || !size) continue;

            Rect bounds(pos->position.x, pos->position.y, size->size.width, size->size.height);
            bool inside = bounds.contains(mousePos.x, mousePos.y);

            // Drive hover transitions through the component's hooks (which respect
            // the enabled flag), rather than poking `hovered` directly.
            if (inside && !input->hovered) {
                input->onHoverEnter();
            } else if (!inside && input->hovered) {
                input->onHoverExit();
            }

            if (input->hovered && mouseClicked) {
                input->onPress(mousePos);
            }

            input->resetTransientState();
        }

        // The click is consumed once it has been dispatched to hovered entities.
        mouseClicked = false;
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
 * @tparam TWorld World type providing the render/shape/position/size storages
 * @tparam TCanvas Canvas type for rendering
 *
 * Renders all visible entities that have a RenderComponent to the target canvas,
 * in ascending z-order.
 *
 * @note TWorld must compose PositionComponent, SizeComponent, RenderComponent and
 *       ShapeComponent — the renderer looks all four up by type. TCanvas must be a
 *       Pixel4 canvas, since RenderComponent stores a Pixel4 color.
 */
template<typename TWorld, typename TCanvas>
class RenderSystem : public System<RenderSystem<TWorld, TCanvas>> {
private:
    TWorld* world_;                     ///< World whose entities are rendered
    TCanvas* canvas;                    ///< Target canvas for rendering
    std::vector<Entity> sortedEntities; ///< Entities sorted by z-order

public:
    /**
     * @brief Construct with the world to render and the target canvas
     * @param world World to render (borrowed, not owned)
     * @param targetCanvas Canvas to render to (borrowed, not owned)
     */
    RenderSystem(TWorld* world, TCanvas* targetCanvas)
        : world_(world), canvas(targetCanvas) {}

    /**
     * @brief Update rendering
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        (void)dt;
        if (!canvas || !world_) return;

        // Clear canvas
        canvas->clear(Colors::BLACK);

        // Collect renderable entities.
        sortedEntities.clear();
        for (Entity e : world_->template query<RenderComponent>()) {
            auto* render = world_->template get<RenderComponent>(e);
            if (render && render->shouldRender()) {
                sortedEntities.push_back(e);
            }
        }

        // Sort back-to-front by z-order.
        std::sort(sortedEntities.begin(), sortedEntities.end(),
                  [this](Entity a, Entity b) {
                      auto* renderA = world_->template get<RenderComponent>(a);
                      auto* renderB = world_->template get<RenderComponent>(b);
                      return renderA->zOrder < renderB->zOrder;
                  });

        for (Entity entity : sortedEntities) {
            renderEntity(entity);
        }
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
        auto* pos = world_->template get<PositionComponent>(entity);
        auto* render = world_->template get<RenderComponent>(entity);
        if (!pos || !render) return;

        auto* size = world_->template get<SizeComponent>(entity);
        auto* shape = world_->template get<ShapeComponent>(entity);

        if (shape) {
            renderShape(*pos, size, *render, *shape);
        } else if (size) {
            renderRectangle(*pos, *size, *render);
        }
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
        using Prims = Primitives<typename TCanvas::PixelType>;

        switch (shape.type) {
            case ShapeComponent::RECTANGLE:
                if (size) {
                    Rect rect(pos.position.x, pos.position.y, size->size.width, size->size.height);
                    if (shape.filled) {
                        Prims::fillRect(*canvas, rect, render.color);
                    } else {
                        Prims::drawRect(*canvas, rect, render.color);
                    }
                }
                break;

            case ShapeComponent::CIRCLE:
                if (shape.filled) {
                    Prims::fillCircle(*canvas, pos.position.x, pos.position.y,
                                      shape.radius, render.color);
                } else {
                    Prims::drawCircle(*canvas, pos.position.x, pos.position.y,
                                      shape.radius, render.color);
                }
                break;

            case ShapeComponent::TRIANGLE:
                if (shape.filled) {
                    Prims::fillTriangle(*canvas,
                        pos.position.x + shape.p1.x,
                        pos.position.y + shape.p1.y,
                        pos.position.x + shape.p2.x,
                        pos.position.y + shape.p2.y,
                        pos.position.x + shape.p3.x,
                        pos.position.y + shape.p3.y,
                        render.color);
                } else {
                    Prims::drawTriangle(*canvas,
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
                Prims::drawLine(*canvas,
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
        using Prims = Primitives<typename TCanvas::PixelType>;

        Rect rect(pos.position.x, pos.position.y, size.size.width, size.size.height);
        Prims::fillRect(*canvas, rect, render.color);
    }
};

} // namespace enjin2
