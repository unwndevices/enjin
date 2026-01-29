#include "Animation.hpp"
#include <stdint.h>

namespace enjin
{
    Animation::Animation(FacingDirection direction) : frames(0), currentFrameIndex(0), currentFrameTime(0), direction(direction), releaseFirstFrame(true), isLooped(true) {}

    void Animation::AddFrame(const uint8_t *texture, int textureID, int width, int height, uint16_t frameTime, bool looped, SpriteFormat format)
    {
        FrameData data;
        data.texture = texture;
        data.id = textureID;
        data.width = width;
        data.height = height;
        data.displayTimeMs = frameTime;
        data.format = format;
        frames.push_back(data);
        isLooped = looped;
    }

    const FrameData *Animation::GetCurrentFrame() const
    {
        if (frames.size() > 0)
        {
            return &frames[currentFrameIndex];
        }

        return nullptr;
    }

    bool Animation::UpdateFrame(uint16_t deltaTime)
    {
        // TODO: A bit cumbersome. Is there another way to do this?
        if (releaseFirstFrame)
        {
            RunActionForCurrentFrame();
            releaseFirstFrame = false;
            return true;
        }

        if (frames.size() > 1 && (isLooped || currentFrameIndex < frames.size() - 1))
        {
            currentFrameTime += deltaTime;

            if (currentFrameTime >= frames[currentFrameIndex].displayTimeMs)
            {
                currentFrameTime = 0.f;
                IncrementFrame();
                RunActionForCurrentFrame();
                return true;
            }
        }

        return false;
    }

    void Animation::IncrementFrame()
    {
        // check if we reached the last frame
        if (currentFrameIndex == (frames.size() - 1))
        {
            currentFrameIndex = 0;
        }
        else
        {
            currentFrameIndex++;
        }
    }

    void Animation::Reset()
    {
        currentFrameIndex = 0;
        currentFrameTime = 0;
        releaseFirstFrame = true;
    }

    void Animation::SetDirection(FacingDirection dir)
    {
        if (direction != dir)
        {
            direction = dir;
            for (auto &f : frames)
            {
                f.width *= -1;
            }
        }
    }

    FacingDirection Animation::GetDirection() const
    {
        return direction;
    }

    void Animation::AddFrameAction(unsigned int frame, AnimationAction action)
    {
        if (frame < frames.size())
        {

            auto actionKey = actions.find(frame);

            if (actionKey == actions.end())
            {
                framesWithActions.SetBit(frame);
                actions.insert(std::make_pair(frame, std::vector<AnimationAction>{action}));
            }
            else
            {
                actionKey->second.emplace_back(action);
            }
        }
        else
        {
            // log_d("Frame index %u is out of range. Frames size: %zu", frame, frames.size());
        }
    }

    void Animation::RunActionForCurrentFrame()
    {
        if (actions.size() > 0)
        {

            if (framesWithActions.GetBit(currentFrameIndex))
            {
                auto actionsToRun = actions.at(currentFrameIndex);

                for (auto f : actionsToRun)
                {
                    f();
                }
            }
        }
    }

    void Animation::SetLooped(bool looped)
    {
        isLooped = looped;
    }

    bool Animation::IsLooped()
    {
        return isLooped;
    }
}