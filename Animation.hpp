#ifndef ANIMATION_HPP
#define ANIMATION_HPP

#include <stdint.h>
#include <vector>
#include <functional>
#include <map>

#include "utils/Bitmask.hpp"
#include "Sprite.hpp"

namespace enjin
{
    enum class FacingDirection
    {
        None,
        Left,
        Right
    };

    struct FrameData
    {
        const uint8_t *texture;
        int id;
        int width;
        int height;
        uint16_t displayTimeMs;
        SpriteFormat format;
    };

    using AnimationAction = std::function<void(void)>;
    class Animation
    {
    public:
        Animation(FacingDirection direction);

        void AddFrame(const uint8_t *texture, int textureID, int width, int height, uint16_t frameTime, bool looped = false, SpriteFormat format = SpriteFormat::Grayscale8);
        void AddFrameAction(unsigned int frame, AnimationAction action);

        const FrameData *GetCurrentFrame() const;

        bool UpdateFrame(uint16_t deltaTime);

        void Reset();

        void SetDirection(FacingDirection dir);

        FacingDirection GetDirection() const;

        std::vector<FrameData> frames;

        void SetLooped(bool looped);
        void SetFrame(uint16_t frame)
        {
            if (currentFrameIndex != frame)
                currentFrameIndex = frame;
        }
        bool IsLooped();

    private:
        void IncrementFrame();
        void RunActionForCurrentFrame();

        uint32_t currentFrameIndex;
        uint16_t currentFrameTime;
        FacingDirection direction;

        bool releaseFirstFrame;
        Bitmask framesWithActions;
        std::map<int, std::vector<AnimationAction>> actions;

        bool isLooped;
    };
}
#endif// ANIMATION_HPP
