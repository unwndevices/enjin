#include <iostream>

#include "C_Sprite.hpp"
#include "../Object.hpp"

namespace enjin
{
    C_Sprite::C_Sprite(Object *owner, uint8_t width, uint8_t height) : C_Drawable(width, height), Component(owner)
    {
        position = owner->GetComponent<C_Position>();
        if (!position)
        {
            std::cerr << "C_Satellite requires C_Position component.\n";
        }
        sprite.setTexture(nullptr, width, height);
    }

    void C_Sprite::Load(const uint8_t texture[], uint8_t width, uint8_t height)
    {
        sprite.setPosition(position->GetPosition());
        sprite.setTexture(texture, width, height);
    }

    void C_Sprite::Load(const uint8_t texture[], uint8_t width, uint8_t height, SpriteFormat format)
    {
        sprite.setPosition(position->GetPosition());
        sprite.setTexture(texture, width, height, format);
    }

    void C_Sprite::LoadFrame(const uint8_t texture[], uint8_t frameId)
    {
        sprite.setTexture(texture, frameId);
    }

    void C_Sprite::LoadFrame(uint8_t frameId)
    {
        sprite.setTexture(frameId);
    }

    void C_Sprite::Draw(EiseiCanvas &canvas)
    {
        const Vector2 offset = GetOffsetPosition();
        const SpriteFormat format = sprite.GetFormat();
        const uint8_t matte = sprite.GetMatte();
        const uint8_t spriteWidth = sprite.GetWidth();
        const uint8_t spriteHeight = sprite.GetHeight();

        switch (GetBlendMode())
        {
        case BlendMode::Normal:
            sprite.draw(canvas, offset.x, offset.y);
            break;
        case BlendMode::Add:
            if (format == SpriteFormat::Grayscale8)
            {
                canvas.add(sprite.GetTexture());
            }
            else
            {
                sprite.draw(canvas, offset.x, offset.y);
            }
            break;
        case BlendMode::Sub:
            if (format == SpriteFormat::Grayscale8)
            {
                canvas.subtract(sprite.GetTexture());
            }
            else
            {
                sprite.draw(canvas, offset.x, offset.y);
            }
            break;
        case BlendMode::Difference:
            if (format == SpriteFormat::Grayscale8)
            {
                canvas.difference(offset.x, offset.y, sprite.GetTexture(), spriteWidth, spriteHeight);
            }
            else
            {
                sprite.draw(canvas, offset.x, offset.y);
            }
            break;
        case BlendMode::Opacity50:
            if (format == SpriteFormat::Grayscale8)
            {
                canvas.drawGrayscaleBitmap(offset.x, offset.y, sprite.GetTexture(), matte, spriteWidth, spriteHeight, 2U);
            }
            else
            {
                sprite.draw(canvas, offset.x, offset.y);
            }
            break;
        case BlendMode::Opacity25:
            if (format == SpriteFormat::Grayscale8)
            {
                canvas.drawGrayscaleBitmap(offset.x, offset.y, sprite.GetTexture(), matte, spriteWidth, spriteHeight, 4U);
            }
            else
            {
                sprite.draw(canvas, offset.x, offset.y);
            }
            break;

        default:
            sprite.draw(canvas, offset.x, offset.y);
            break;
        }
    }

    bool C_Sprite::ContinueToDraw() const
    {
        return !owner->IsQueuedForRemoval();
    }

    void C_Sprite::LateUpdate(uint16_t deltaTime)
    {
    }
}
