#ifndef C_KEYFRAMEANIMATOR_HPP
#define C_KEYFRAMEANIMATOR_HPP

#include <memory>
#include <vector>
#include <algorithm>

#include "Component.hpp"
#include "C_Animation.hpp"
#include "C_Position.hpp"
#include "../utils/Easing.hpp"

namespace enjin
{
    struct Keyframe
    {
        uint32_t time;
        Vector2 position;
        AnimationState state;
        EasingFunction easingFunction;
    };

    class C_KeyframeAnimator : public Component
    {
    public:
        C_KeyframeAnimator(Object *owner);

        void Update(uint16_t deltaTime) override;

        void AddKeyframe(Keyframe keyframe);
        void ClearKeyframes();

        // Sets the number of times the animation should loop (-1 disables looping)
        void SetLoops(int loopCount)
        {
            if (loopCount < 0)
            {
                this->looping = false;
                loopCount = 1;
            }
            else
            {
                this->looping = true;
                this->loopCount = loopCount;
            }
        };

    private:
        std::shared_ptr<C_Position> position;
        std::shared_ptr<C_Animation> animation;

        bool looping;
        int loopCount;

        std::vector<Keyframe> keyframes;
        uint16_t currentKeyframeIndex;
        unsigned long elapsedTime;

        Vector2 InterpolatePosition(Keyframe kf1, Keyframe kf2, float t);
    };
}
#endif // C_KEYFRAMEANIMATOR_HPP
