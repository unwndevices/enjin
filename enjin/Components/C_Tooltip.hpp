#ifndef C_TOOLTIP_HPP
#define C_TOOLTIP_HPP
#include <memory>
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"

namespace enjin
{
    class C_Tooltip : public C_Drawable
    {
    public:
        C_Tooltip(Object *owner, int8_t precision, uint8_t width, uint8_t height);
        void Awake() override;
        void Update(uint16_t deltaTime) override;
        void Draw(EiseiCanvas &canvas) override;
        bool ContinueToDraw() const override;
        void SetPosition(Vector2 pos) { position->SetPosition(pos); };
        void SetOrigin(Vector2 pos) { origin = pos; };
        void SetValue(float val);

    private:
        float value;
        int8_t precision;
        uint8_t width;

        Vector2 origin; // position of the tooltip target

        EiseiCanvas canvas;
    };
}
#endif // C_TOOLTIP_HPP
