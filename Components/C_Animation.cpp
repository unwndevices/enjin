#include "C_Animation.hpp"
#include "../Object.hpp"

namespace enjin
{

    C_Animation::C_Animation(Object *owner) : Component(owner),
                                              currentAnimation(AnimationState::None, nullptr)
    {
    }

    void C_Animation::Awake()
    {
        sprite = owner->GetComponent<C_Sprite>();
    }

    void C_Animation::SetAnimationState(AnimationState state)
    {
        if (currentAnimation.first == state)
        {
            return;
        }

        auto animationList = animations.find(state);
        if (animationList != animations.end())
        {
            auto animation = animationList->second.find(FacingDirection::Right);

            if (animation != animationList->second.end())
            {
                currentAnimation.first = animationList->first;
                currentAnimation.second = animation->second;
                currentAnimation.second->Reset();
            }
        }
    }

    const AnimationState &C_Animation::GetAnimationState() const
    {
        return currentAnimation.first;
    }

    void C_Animation::Update(uint16_t deltaTime)
    {
        if (currentAnimation.first != AnimationState::None)
        {
            bool newFrame = currentAnimation.second->UpdateFrame(deltaTime);

            if (newFrame)
            {
                const FrameData *data = currentAnimation.second->GetCurrentFrame();
                sprite->Load(data->texture, data->width, data->height, data->format);
                sprite->LoadFrame(static_cast<uint8_t>(data->id));
            }
        }
    }

    void C_Animation::AddAnimation(AnimationState state, AnimationList &animationList)
    {
        animations.insert(std::make_pair(state, animationList));

        if (currentAnimation.first == AnimationState::None)
        {
            SetAnimationState(state);
        }
    }

    void C_Animation::AddAnimationAction(AnimationState state, FacingDirection dir, int frame, AnimationAction action)
    {
        auto animationList = animations.find(state);

        if (animationList != animations.end())
        {
            auto animation = animationList->second.find(dir);

            if (animation != animationList->second.end())
            {
                animation->second->AddFrameAction(frame, action);
            }
        }
    }

    void C_Animation::ResetAnimation()
    {
        if (currentAnimation.first != AnimationState::None && currentAnimation.second)
        {
            currentAnimation.second->Reset();
        }
    }
}
