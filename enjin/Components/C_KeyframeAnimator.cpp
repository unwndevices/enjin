#include <iostream>
#include "C_KeyframeAnimator.hpp"
#include "../Object.hpp"

namespace enjin
{
    C_KeyframeAnimator::C_KeyframeAnimator(Object *owner)
        : Component(owner), looping(false), loopCount(1), currentKeyframeIndex(0), elapsedTime(0)
    {
        position = owner->GetComponent<C_Position>();
        animation = owner->GetComponent<C_Animation>();

        if (!position)
        {
            std::cerr << "C_KeyframeAnimator requires C_Position component.\n";
        }
        if (!animation)
        {
            std::cerr << "C_KeyframeAnimator requires C_Animation component.\n";
        }
    }

    void C_KeyframeAnimator::AddKeyframe(Keyframe keyframe)
    {
        keyframes.push_back(keyframe);
        // Sort keyframes by time
        std::sort(keyframes.begin(), keyframes.end(), [](const Keyframe &a, const Keyframe &b)
                  { return a.time < b.time; });
    }

    void C_KeyframeAnimator::ClearKeyframes()
    {
        keyframes.clear();
        currentKeyframeIndex = 0;
        elapsedTime = 0;
    }

    void C_KeyframeAnimator::Update(uint16_t deltaTime)
    {
        if (keyframes.empty())
        {
            return;
        }

        Keyframe currentKeyframe = keyframes[currentKeyframeIndex];
        Keyframe nextKeyframe = currentKeyframe;

        elapsedTime += deltaTime;

        if (elapsedTime < currentKeyframe.time)
        {
            return;
        }

        if (currentKeyframeIndex + 1 < (uint16_t)keyframes.size())
        {
            nextKeyframe = keyframes[currentKeyframeIndex + 1];
        }

        float t = ((float)(elapsedTime - currentKeyframe.time) / (float)(nextKeyframe.time - currentKeyframe.time));
        if (t > 0.99f)
            t = 1.0f;
        else if (t < 0.01f)
            t = 0.0f;

        if (elapsedTime >= nextKeyframe.time)
        {
            t = 1.0f;
            if (currentKeyframeIndex >= keyframes.size() - 1)
            {
                if (looping && loopCount > 0)
                {
                    elapsedTime = 0;
                    currentKeyframeIndex = 0;
                    --loopCount;
                }
                else
                {
                    return;
                }
            }
            else
            {
                ++currentKeyframeIndex;
            }
        }

        Vector2 interpolatedPosition = InterpolatePosition(currentKeyframe, nextKeyframe, t);
        this->animation->SetAnimationState(currentKeyframe.state);
        this->position->SetPosition(interpolatedPosition);
    }

    ///////////////////
    ///// PRIVATE /////
    ///////////////////

    Vector2 C_KeyframeAnimator::InterpolatePosition(Keyframe kf1, Keyframe kf2, float t)
    {
        Vector2 p1 = kf1.position;
        Vector2 p2 = kf2.position;
        float easing = kf2.easingFunction(t);
        return Vector2::Lerp(p1, p2, easing);
    }
}