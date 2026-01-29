#ifndef C_ANIMATION_HPP
#define C_ANIMATION_HPP
#include <unordered_map>

#include <memory>
#include <map>
#include "Component.hpp"
#include "C_Sprite.hpp"
#include "../Animation.hpp"
#include "../EnumClassHash.hpp"

namespace enjin
{
    enum class AnimationState
    {
        None,
        Idle,
        Walk,
        SittingTransition,
        Sitting
    };

    using AnimationList = std::unordered_map<FacingDirection, std::shared_ptr<Animation>, EnumClassHash>;

    class C_Animation : public Component
    {
    public:
        C_Animation(Object *owner);

        void Awake() override;

        void Update(uint16_t deltaTime) override;

        // Add animation to object. We need its state as well
        // so that we can switch to it.
        void AddAnimation(AnimationState state, AnimationList &animationList);

        void SetAnimationDirection(FacingDirection dir);
        // Set current animation states.
        void SetAnimationState(AnimationState state);
        void AddAnimationAction(AnimationState state, FacingDirection dir, int frame, AnimationAction action);

        // Returns current animation state.
        const AnimationState &GetAnimationState() const;

        // Reset current animation to first frame
        void ResetAnimation();

    private:
        std::shared_ptr<C_Sprite> sprite;
        std::unordered_map<AnimationState, AnimationList, EnumClassHash> animations;

        std::pair<AnimationState, std::shared_ptr<Animation>> currentAnimation;
    };
}

#endif// C_ANIMATION_HPP
