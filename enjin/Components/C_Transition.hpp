#ifndef C_TRANSITION_HPP
#define C_TRANSITION_HPP

#include <memory>
#include <map>
#include <vector>
#include <iostream>
#include <functional>
#include "Component.hpp"
#include "../utils/Easing.hpp"

namespace enjin
{
    template <typename T>
    class C_Transition : public Component
    {
    public:
        C_Transition(Object *owner) : Component(owner), isActive(false), elapsedTime(0), parameterSetter(nullptr) {}

        void StartTransition(T startValue, T endValue, unsigned long time, std::function<float(float)> easingFunction)
        {
            this->startValue = startValue;
            this->endValue = endValue;
            this->totalTime = time;
            this->easingFunction = easingFunction;

            elapsedTime = 0;
            isActive = true;
        }

        void SetParameterSetter(std::function<void(T)> setter)
        {
            parameterSetter = setter;
        }

        void Update(uint16_t deltaTime) override
        {
            if (!isActive)
            {
                return;
            }

            elapsedTime += deltaTime;

            if (elapsedTime >= totalTime)
            {
                elapsedTime = totalTime;
                isActive = false;
            }

            float t = (float)elapsedTime / (float)totalTime;
            float easing = easingFunction(t);

            T interpolatedValue = Interpolate(startValue, endValue, easing);

            if (parameterSetter)
            {
                parameterSetter(interpolatedValue);
            }
        }

        bool IsActive() const
        {
            return isActive;
        }

    private:
        T startValue;
        T endValue;
        unsigned long totalTime;
        unsigned long elapsedTime;
        std::function<float(float)> easingFunction;
        std::function<void(T)> parameterSetter;
        bool isActive;

        T Interpolate(T start, T end, float t) const
        {
            return static_cast<T>(start + (end - start) * t);
        }
    };
}
#endif // C_TRANSITION_HPP
