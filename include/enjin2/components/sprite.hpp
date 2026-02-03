/**
 * @file sprite.hpp
 * @brief Sprite component for bitmap rendering
 *
 * Component wrapper around Sprite class, providing ECS integration
 * for bitmap image rendering with frame animation support.
 * Based on original Enjin C_Sprite.
 */
#ifndef ENJIN2_COMPONENTS_SPRITE_HPP
#define ENJIN2_COMPONENTS_SPRITE_HPP

#include "drawable.hpp"
#include "../graphics/sprite.hpp"
#include "../core/object.hpp"

namespace enjin2 {

/**
 * @brief Sprite component for bitmap rendering (matches original Enjin C_Sprite)
 * 
 * Component wrapper around the Sprite class, providing ECS integration
 * for bitmap image rendering with frame animation support.
 */
class C_Sprite : public C_Drawable {
public:
    /**
     * @brief Construct a new Sprite component
     * @param owner The object that owns this component
     * @param width Width of the sprite in pixels
     * @param height Height of the sprite in pixels
     */
    C_Sprite(Object* owner, uint8_t width, uint8_t height)
        : C_Drawable(owner, width, height)
        , sprite()
    {}

    /**
     * @brief Load texture data into the sprite
     * @param texture Pointer to texture bitmap data
     * @param width Width in pixels
     * @param height Height in pixels
     */
    void Load(const uint8_t* texture, uint8_t width, uint8_t height) {
        sprite.setTexture(texture, width, height);
    }

    /**
     * @brief Load a specific frame from texture data
     * @param texture Pointer to texture bitmap data
     * @param frameId Frame index to load
     */
    void LoadFrame(const uint8_t* texture, uint8_t frameId) {
        sprite.setTexture(texture, frameId);
    }

    /**
     * @brief Load a specific frame (texture already set)
     * @param frameId Frame index to load
     */
    void LoadFrame(uint8_t frameId) {
        sprite.setTexture(frameId);
    }

    /**
     * @brief Draw the sprite to canvas (overrides C_Drawable)
     * @param canvas The canvas to draw to
     */
    void draw(ICanvas<uint8_t>& canvas) override {
        if (!is_visible) return;
        
        // Update sprite position from component position
        if (position) {
            Point render_pos = GetOffsetPosition();
            sprite.setPosition(render_pos);
        }
        
        // Draw using the sprite's draw method
        sprite.draw(canvas);
    }

    /**
     * @brief Check if should continue drawing (matches original Enjin)
     * @return True if object is not queued for removal
     */
    bool continueToDraw() const override {
        return !owner->isQueuedForRemoval();
    }

    /**
     * @brief Late update method for animation (matches original Enjin)
     * @param deltaTime Time delta in milliseconds
     */
    void lateUpdate(uint16_t deltaTime) override {
        // Override in derived classes for animation logic
        Component::lateUpdate(deltaTime);
    }

    /**
     * @brief Set the matte (transparent) color
     * @param matte Matte color value
     */
    void setMatte(uint8_t matte) {
        sprite.setMatte(matte);
    }

    /**
     * @brief Get the underlying sprite object
     * @return Reference to the sprite
     */
    Sprite& getSprite() {
        return sprite;
    }

    /**
     * @brief Get the underlying sprite object (const)
     * @return Const reference to the sprite
     */
    const Sprite& getSprite() const {
        return sprite;
    }

private:
    Sprite sprite;  ///< The underlying sprite object
};

} // namespace enjin2

#endif // ENJIN2_COMPONENTS_SPRITE_HPP