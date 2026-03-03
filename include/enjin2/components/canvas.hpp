/**
 * @file canvas.hpp
 * @brief Canvas component for embedding canvases as components
 *
 * Provides a component wrapper for Canvas objects, allowing
 * canvases to be managed as components within the object system.
 */
#pragma once

#include "../core/types.hpp"
#include "../graphics/canvas.hpp"
#include "drawable.hpp"
#include "position.hpp"

namespace enjin2
{

    // Forward declarations
    class Object;

    /**
     * @brief Canvas component for custom drawing operations
     *
     * A drawable component that wraps an internal canvas for
     * custom graphics operations. Supports multiple blend modes
     * for composition. Based on original Enjin C_Canvas.
     */
    class C_Canvas : public C_Drawable
    {
    public:
        /**
         * @brief Constructor
         * @param owner Parent object
         * @param width Canvas width in pixels
         * @param height Canvas height in pixels
         */
        C_Canvas(Object *owner, uint16_t width, uint16_t height);

        /**
         * @brief Destructor
         */
        ~C_Canvas() = default;

        // Component interface
        void awake() override;
        void start() override {}
        void update(float dt) override {}
        void lateUpdate(float dt) override;

        // Drawable interface
        void draw(ICanvas<Pixel4> &canvas) override;
        bool continueToDraw() const override;

        /**
         * @brief Get access to internal canvas for drawing
         * @return Reference to internal canvas
         */
        template <size_t W, size_t H>
        Canvas8<W, H> &getCanvas()
        {
            return *static_cast<Canvas8<W, H> *>(internal_canvas.get());
        }

        /**
         * @brief Get const access to internal canvas
         * @return Const reference to internal canvas
         */
        template <size_t W, size_t H>
        const Canvas8<W, H> &getCanvas() const
        {
            return *static_cast<const Canvas8<W, H> *>(internal_canvas.get());
        }

        /**
         * @brief Clear the canvas
         * @param color Fill color (0-15)
         */
        void clear(uint8_t color = 0);

        /**
         * @brief Get canvas width
         * @return Width in pixels
         */
        uint16_t getWidth() const { return canvas_width; }

        /**
         * @brief Get canvas height
         * @return Height in pixels
         */
        uint16_t getHeight() const { return canvas_height; }

        /**
         * @brief Set matte color (transparent color that won't be drawn)
         * @param matte Matte color value (default: 16 for compatibility)
         */
        void setMatteColor(uint8_t matte) { matte_color = matte; }

        /**
         * @brief Get current matte color
         * @return Current matte color
         */
        uint8_t getMatteColor() const { return matte_color; }

    private:
        std::unique_ptr<ICanvas<uint8_t>> internal_canvas; ///< Internal canvas for drawing
        uint16_t canvas_width;                             ///< Canvas width
        uint16_t canvas_height;                            ///< Canvas height
        uint8_t matte_color = 16;                          ///< Matte color (transparent, default 16)
        C_Position *position;                              ///< Cached position component

        /**
         * @brief Create internal canvas of specified size
         * @param width Canvas width
         * @param height Canvas height
         */
        void createCanvas(uint16_t width, uint16_t height);

        /**
         * @brief Apply blend mode when drawing to target canvas (deferred — ENG-01)
         * @param target_canvas Target canvas to draw to
         */
        void applyBlendMode(ICanvas<Pixel4> &target_canvas);
    };

} // namespace enjin2