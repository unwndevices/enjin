#ifndef FRAMECONTROLLEDSPRITE_HPP
#define FRAMECONTROLLEDSPRITE_HPP

#include <cstddef>

#include "../Object.hpp"
#include "../Components/C_Sprite.hpp"

namespace enjin
{
    // UI object that exposes a sprite animation controlled by a normalized parameter.
    class FrameControlledSprite : public Object
    {
    public:
        FrameControlledSprite(uint8_t width, uint8_t height, DrawLayer layer = DrawLayer::UI)
            : frames(nullptr), frameCount(0), controlValue(0.0f), currentFrame(0)
        {
            position->SetPosition(63, 63);
            sprite = AddComponent<C_Sprite>(width, height);
            sprite->SetDrawLayer(layer);
            sprite->SetBlendMode(BlendMode::Normal);
            sprite->SetAnchorPoint(Anchor::CENTER_);
        }

        void LoadAnimation(const uint8_t *frameData, size_t count, uint8_t width, uint8_t height, SpriteFormat inputFormat = SpriteFormat::Grayscale8,
                           uint8_t matte = 16U, uint8_t foreground = 0xFF, uint8_t background = 0x00)
        {
            frames = frameData;
            if (count == 0)
            {
                frameCount = 0;
                return;
            }

            frameCount = (count > 255) ? 255 : count;
            sprite->Load(frameData, width, height, inputFormat);
            sprite->setMatte(matte);
            if (inputFormat == SpriteFormat::Monochrome1)
            {
                sprite->GetSprite().setMonochromePalette(foreground, background);
            }
            UpdateFrameFromControl(true);
        }

        void SetControlValue(float value)
        {
            controlValue = value;
            UpdateFrameFromControl();
        }

        float GetControlValue() const
        {
            return controlValue;
        }

        void SetVisibility(bool visibility)
        {
            sprite->SetVisibility(visibility);
        }

        void SetAnchorPoint(Anchor anchor)
        {
            sprite->SetAnchorPoint(anchor);
        }

        void AddOffset(Vector2 offset)
        {
            sprite->AddOffset(offset);
        }

        void SetOffset(Vector2 offset)
        {
            sprite->SetOffset(offset);
        }

        void SetMonochromePalette(uint8_t foreground, uint8_t background)
        {
            sprite->GetSprite().setMonochromePalette(foreground, background);
        }

        std::shared_ptr<C_Sprite> GetSpriteComponent()
        {
            return sprite;
        }

    private:
        void UpdateFrameFromControl(bool force = false)
        {
            if (!frames || frameCount == 0)
            {
                return;
            }

            float clamped = controlValue;
            if (clamped < 0.0f)
            {
                clamped = 0.0f;
            }
            else if (clamped > 1.0f)
            {
                clamped = 1.0f;
            }

            size_t targetIndex = 0;
            if (frameCount > 1)
            {
                size_t scaled = static_cast<size_t>(clamped * static_cast<float>(frameCount));
                if (scaled >= frameCount)
                {
                    scaled = frameCount - 1;
                }
                targetIndex = scaled;
            }

            if (!force && targetIndex == currentFrame)
            {
                return;
            }

            currentFrame = static_cast<uint8_t>(targetIndex);
            sprite->LoadFrame(currentFrame);
        }

        std::shared_ptr<C_Sprite> sprite;
        const uint8_t *frames;
        size_t frameCount;
        float controlValue;
        uint8_t currentFrame;
    };
}

#endif // FRAMECONTROLLEDSPRITE_HPP
