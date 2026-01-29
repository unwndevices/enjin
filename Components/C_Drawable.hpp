
#ifndef C_DRAWABLE_HPP
#define C_DRAWABLE_HPP

#include <memory>
#include <Adafruit_GFX.h>
#include "Component.hpp"
#include "C_Position.hpp"
#include "../enjin2_compat.hpp"

namespace enjin
{

    typedef enum
    {
        Default,
        Background,
        Entities,
        Foreground,
        Overlay,
        UI
    } DrawLayer;

    typedef enum
    {
        Normal,
        Add,
        Sub,
        Difference,
        Opacity50,
        Opacity25
    } BlendMode;

    typedef enum
    {
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT,
        CENTER_,
        CENTER_LEFT,
        CENTER_RIGHT,
        CENTER_TOP,
        CENTER_BOTTOM
    } Anchor;

    class C_Drawable : public virtual Component
    {
    public:
        C_Drawable(uint8_t width, uint8_t height);
        virtual ~C_Drawable();

        virtual void Draw(EiseiCanvas &canvas) = 0;
        virtual bool ContinueToDraw() const = 0;

        void SetSortOrder(int order);
        int GetSortOrder() const;
        void SetBlendMode(BlendMode mode) { blendMode = mode; }
        BlendMode GetBlendMode() const { return blendMode; }
        void SetDrawLayer(DrawLayer drawLayer);
        DrawLayer GetDrawLayer() const;
        void SetVisibility(bool visibility) { is_visible = visibility; }
        bool GetVisibility() { return is_visible; }
        void SetAnchorPoint(Anchor anchor);
        void AddOffset(Vector2 offset)
        {
            anchorOffset -= offset;
        }
        void SetOffset(Vector2 offset)
        {
            anchorOffset = offset;
        }
        Vector2 GetOffsetPosition()
        {
            Vector2 pos = position->GetPosition() - anchorOffset;
            return pos;
        }
        void SetXOffset(int16_t x) { anchorOffset.x = x; }
        void SetYOffset(int16_t y) { anchorOffset.y = y; }

        uint8_t GetWidth() { return width; };
        uint8_t GetHeight() { return height; };

    protected:
        std::shared_ptr<C_Position> position;
        Vector2 anchorOffset;
        static Vector2 abs_center;

        int sortOrder;
        DrawLayer layer;
        BlendMode blendMode;
        Anchor anchor;
        bool is_visible;
        uint8_t width, height;
    };
}
#endif //! C_DRAWABLE_HPP
