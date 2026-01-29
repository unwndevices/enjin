#ifndef C_PARAMETERANIMATOR_HPP
#define C_PARAMETERANIMATOR_HPP

#include <memory>
#include <map>
#include <vector>
#include <iostream>
#include <functional>
#include <algorithm>
#include "Component.hpp"
#include "../utils/Easing.hpp"

namespace enjin
{
    template <typename T>
    struct ParameterKeyframe
    {
        uint16_t time;
        T value;
        EasingFunction easingFunction;
    };

    template <typename T>
    class ParameterAnimation
    {
    public:
        void AddKeyframe(ParameterKeyframe<T> keyframe)
        {
            keyframes.push_back(keyframe);
            std::sort(keyframes.begin(), keyframes.end(), [](const ParameterKeyframe<T> &a, const ParameterKeyframe<T> &b)
                      { return a.time < b.time; });
        }

        const std::vector<ParameterKeyframe<T>> &GetKeyframes() const
        {
            return keyframes;
        }

        void SetEndCallback(std::function<void()> callback)
        {
            endCallback = callback;
        }

        std::function<void()> GetEndCallback() const
        {
            return endCallback;
        }

        void UpdateFirstKeyframeValue(T value)
        {
            if (!keyframes.empty())
            {
                keyframes[0].value = value;
            }
        }

    private:
        std::vector<ParameterKeyframe<T>> keyframes;
        std::function<void()> endCallback = nullptr;
    };

    template <typename T>
    class C_ParameterAnimator : public Component
    {
    public:
        C_ParameterAnimator(Object *owner);

        void Update(uint16_t deltaTime) override;

        void StartAnimation(bool reset = false);

        void SetParameterGetter(std::function<T()> getter);
        void SetParameterSetter(std::function<void(T)> setter);

        void SetAnimation(ParameterAnimation<T> &animation);

        bool hasFinished() { return !isActive; }

        T GetCurrentValue()
        {
            if (parameterGetter)
            {
                return parameterGetter();
            }
            else
            {
                return T();
            }
        }

    private:
        ParameterAnimation<T> *currentAnimation = nullptr;
        uint16_t currentKeyframeIndex;
        unsigned long elapsedTime;
        bool isActive;

        std::function<T()> parameterGetter;
        std::function<void(T)> parameterSetter;
        std::function<void()> endCallback;

        T InterpolateParameter(ParameterKeyframe<T> kf1, ParameterKeyframe<T> kf2, float t);
    };

    template <typename T>
    C_ParameterAnimator<T>::C_ParameterAnimator(Object *owner) : Component(owner), currentKeyframeIndex(0), elapsedTime(0), isActive(false)
    {
    }

    template <typename T>
    void C_ParameterAnimator<T>::Update(uint16_t deltaTime)
    {
        if (!currentAnimation || currentAnimation->GetKeyframes().size() == 0)
        {
            return;
        }

        if (!isActive)
        {
            return;
        }

        elapsedTime += deltaTime;

        if (currentKeyframeIndex >= currentAnimation->GetKeyframes().size() - 1)
        {
            isActive = false;

            if (auto callback = currentAnimation->GetEndCallback())
            {
                callback();
            }

            return;
        }

        auto currentKeyframe = currentAnimation->GetKeyframes()[currentKeyframeIndex];
        auto nextKeyframe = currentAnimation->GetKeyframes()[currentKeyframeIndex + 1];

        float t = static_cast<float>(elapsedTime - currentKeyframe.time) / static_cast<float>(nextKeyframe.time - currentKeyframe.time);

        if (t > 1.0f)
        {
            t = 1.0f;
            currentKeyframeIndex++;
            elapsedTime = currentKeyframe.time;
        }

        T value = InterpolateParameter(currentKeyframe, nextKeyframe, t);

        if (parameterSetter)
        {
            parameterSetter(value);
        }
    }

    template <typename T>
    void C_ParameterAnimator<T>::SetAnimation(ParameterAnimation<T> &animation)
    {
        currentAnimation = &animation;
        currentKeyframeIndex = 0; // Reset keyframe index
        elapsedTime = 0;          // Reset elapsed time
    }

    template <typename T>
    void C_ParameterAnimator<T>::StartAnimation(bool reset)
    {
        if (currentAnimation && parameterGetter && !reset)
        {
            T currentValue = parameterGetter();
            currentAnimation->UpdateFirstKeyframeValue(currentValue);
        }

        currentKeyframeIndex = 0;
        elapsedTime = 0;
        isActive = true;
    }

    template <typename T>
    void C_ParameterAnimator<T>::SetParameterGetter(std::function<T()> getter)
    {
        parameterGetter = getter;
    }

    template <typename T>
    void C_ParameterAnimator<T>::SetParameterSetter(std::function<void(T)> setter)
    {
        parameterSetter = setter;
    }

    template <typename T>
    T C_ParameterAnimator<T>::InterpolateParameter(ParameterKeyframe<T> kf1, ParameterKeyframe<T> kf2, float t)
    {
        float p1 = (float)kf1.value;
        float p2 = (float)kf2.value;
        float easing = kf2.easingFunction(t);
        float result = p1 + easing * (p2 - p1);
        return static_cast<T>(result);
    }
}
#endif // C_PARAMETERANIMATOR_HPP
