#ifndef C_SPRITE_HPP
#define C_SPRITE_HPP
#include <memory>
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "../Sprite.hpp"
#include "C_Position.hpp"

namespace enjin
{
    class C_Sprite : public C_Drawable
    {
    public:
        C_Sprite(Object *owner, uint8_t width, uint8_t height);
        void Load(const uint8_t texture[], uint8_t width, uint8_t height);
        void Load(const uint8_t texture[], uint8_t width, uint8_t height, SpriteFormat format);
        void LoadFrame(const uint8_t texture[], uint8_t frameId);
        void LoadFrame(uint8_t frameId);
        // We override the draw method so we can draw our sprite.
        void Draw(EiseiCanvas &canvas) override;
        bool ContinueToDraw() const override;
        void LateUpdate(uint16_t deltaTime) override;

        void setMatte(uint8_t matte)
        {
            sprite.setMatte(matte);
        };

        // Convenience methods for monochrome sprite setup
        void setMonochromePalette(uint8_t foreground, uint8_t background)
        {
            sprite.setMonochromePalette(foreground, background);
        }

        void loadMonochrome(const uint8_t texture[], uint8_t width, uint8_t height)
        {
            sprite.setTexture(texture, width, height, SpriteFormat::Monochrome1);
        }

        void loadMonochrome(const uint8_t texture[], uint8_t width, uint8_t height,
                           uint8_t foreground, uint8_t background)
        {
            sprite.setTexture(texture, width, height, SpriteFormat::Monochrome1);
            sprite.setMonochromePalette(foreground, background);
        }

        Sprite &GetSprite()
        {
            return sprite;
        }

        const Sprite &GetSprite() const
        {
            return sprite;
        }

    private:
        Sprite sprite;
    };
}
#endif // C_SPRITE_HPP
