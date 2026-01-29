#ifndef ENJIN2_COMPONENTS_DRAW_HPP
#define ENJIN2_COMPONENTS_DRAW_HPP

#include "drawable.hpp"
#include "../graphics/canvas.hpp"
#include "../core/object.hpp"
#include <functional>

namespace enjin2 {

/**
 * @brief Function type for custom drawing operations (matches original Enjin DrawFunction)
 */
using DrawFunction = std::function<void(ICanvas<uint8_t>& canvas)>;

/**
 * @brief Draw component for lambda-based custom rendering (matches original Enjin C_Draw)
 * 
 * Allows custom drawing operations to be performed via lambda functions,
 * providing flexibility for procedural graphics and custom visual effects.
 */
class C_Draw : public C_Drawable {
public:
    /**
     * @brief Construct a new Draw component
     * @param owner The object that owns this component
     * @param drawFunc Optional draw function to execute
     */
    C_Draw(Object* owner, DrawFunction drawFunc = nullptr)
        : C_Drawable(owner, 127, 127)  // Default size matches original Enjin
        , draw_function(drawFunc)
    {
        // Position is already set by C_Drawable constructor
    }

    /**
     * @brief Draw using the stored draw function
     * @param canvas The canvas to draw to
     */
    void draw(ICanvas<uint8_t>& canvas) override {
        if (draw_function && is_visible) {
            draw_function(canvas);
        }
    }

    /**
     * @brief Check if should continue drawing (matches original Enjin)
     * @return True if object is not queued for removal
     */
    bool continueToDraw() const override {
        return !owner->isQueuedForRemoval();
    }

    /**
     * @brief Set the draw function
     * @param drawFunc Function to execute when drawing
     */
    void SetDrawFunction(DrawFunction drawFunc) {
        draw_function = drawFunc;
    }

    /**
     * @brief Get the current draw function
     * @return Current draw function (may be nullptr)
     */
    const DrawFunction& GetDrawFunction() const {
        return draw_function;
    }

private:
    DrawFunction draw_function;  ///< Function to execute when drawing
};

} // namespace enjin2

#endif // ENJIN2_COMPONENTS_DRAW_HPP