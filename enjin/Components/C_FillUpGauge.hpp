#ifndef C_FILLUPGAUGE_HPP
#define C_FILLUPGAUGE_HPP

#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include <memory>
#include <iostream>

namespace enjin
{

    enum class GaugeMode
    {
        Unidirectional,
        Bidirectional
    };

    class C_FillUpGauge : public C_Drawable
    {
    public:
        C_FillUpGauge(Object *owner, uint16_t width, uint16_t height, uint16_t color, GaugeMode mode);
        void Awake() override;
        void Draw(EiseiCanvas &canvas) override;
        bool ContinueToDraw() const override;
        void SetValue(float value);
        void SetMode(GaugeMode mode);
        float GetValue() const;
        void LateUpdate(uint16_t deltaTime) override;

    private:
        uint16_t width;
        uint16_t height;
        uint16_t color;
        float currentValue;
        EiseiCanvas internalCanvas;
        GFXcanvas1 mask;
        GaugeMode mode;
    };
}
#endif // C_FILLUPGAUGE_HPP
