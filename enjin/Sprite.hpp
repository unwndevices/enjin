#ifndef SPRITE_HPP
#define SPRITE_HPP
#include <stdint.h>

#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "utils/Types.hpp"
#include "Components/C_Drawable.hpp"

namespace enjin
{
    enum class SpriteFormat
    {
        Grayscale8,
        Grayscale4,
        Monochrome1
    };

    class Sprite
    {
    public:
        Sprite()
        {
            InitDefaults();
        };

        Sprite(const uint8_t texture[], uint8_t width, uint8_t height, BlendMode mode = BlendMode::Normal)
        {
            InitDefaults();
            setTexture(texture, width, height, SpriteFormat::Grayscale8);
            _mode = mode;
        };

        void draw(EiseiCanvas &canvas)
        {
            draw(canvas, _position.x, _position.y);
        };

        void draw(EiseiCanvas &canvas, int16_t x, int16_t y)
        {
            if (!_texture)
            {
                return;
            }

            const uint8_t *framePointer = GetFramePointer();
            if (!framePointer)
            {
                return;
            }

            switch (_format)
            {
            case SpriteFormat::Grayscale8:
                canvas.drawGrayscaleBitmap(x, y, framePointer, _matte, _width, _height);
                break;
            case SpriteFormat::Grayscale4:
                DrawGrayscale4(canvas, x, y, framePointer);
                break;
            case SpriteFormat::Monochrome1:
                DrawMonochrome(canvas, x, y, framePointer);
                break;
            }
        };

        void Add(EiseiCanvas &canvas)
        {
            if (_format != SpriteFormat::Grayscale8)
            {
                draw(canvas, _position.x, _position.y);
                return;
            }

            const uint8_t *texture = GetFramePointer();
            if (texture)
            {
                canvas.add(texture);
            }
        }

        void Subtract(EiseiCanvas &canvas)
        {
            if (_format != SpriteFormat::Grayscale8)
            {
                draw(canvas, _position.x, _position.y);
                return;
            }

            const uint8_t *texture = GetFramePointer();
            if (texture)
            {
                canvas.subtract(texture);
            }
        }

        void setTexture(const uint8_t texture[], uint8_t width, uint8_t height)
        {
            setTexture(texture, width, height, _format); // Preserve existing format
        };

        void setTexture(const uint8_t texture[], uint8_t width, uint8_t height, SpriteFormat format)
        {
            _texture = texture;
            _width = width;
            _height = height;
            _format = format;
            switch (_format)
            {
            case SpriteFormat::Grayscale8:
                _frameStride = static_cast<uint16_t>(_width) * static_cast<uint16_t>(_height);
                break;
            case SpriteFormat::Grayscale4:
                _frameStride = static_cast<uint16_t>((static_cast<uint32_t>(_width) + 1U) / 2U) * static_cast<uint16_t>(_height);
                break;
            case SpriteFormat::Monochrome1:
                _frameStride = static_cast<uint16_t>((_width + 7U) / 8U) * static_cast<uint16_t>(_height);
                break;
            }
        };

        void setTexture(const uint8_t texture[], uint8_t frameId)
        {
            _texture = texture;
            _frame = frameId;
        };

        void setTexture(uint8_t frameId)
        {
            _frame = frameId;
        };

        void setPosition(int16_t x, int16_t y)
        {
            _position.x = x;
            _position.y = y;
        };
        void setPosition(Vector2 pos)
        {
            _position.x = pos.x;
            _position.y = pos.y;
        };
        void setMatte(uint8_t matte)
        {
            _matte = matte;
        };

        void setMonochromePalette(uint8_t foreground, uint8_t background)
        {
            _foreground = foreground;
            _background = background;
        };

        const uint8_t *GetTexture()
        {
            return GetFramePointer();
        };

        SpriteFormat GetFormat() const
        {
            return _format;
        }

        uint8_t GetWidth() const { return _width; };
        uint8_t GetHeight() const { return _height; };
        uint8_t GetMatte() const { return _matte; };

        uint8_t GetForeground() const { return _foreground; };
        uint8_t GetBackground() const { return _background; };

    protected:
        const uint8_t *GetFramePointer() const
        {
            if (!_texture)
            {
                return nullptr;
            }

            return _texture + (static_cast<uint32_t>(_frame) * _frameStride);
        }

        uint16_t GetRowStride() const
        {
            switch (_format)
            {
            case SpriteFormat::Grayscale8:
                return _width;
            case SpriteFormat::Grayscale4:
                return static_cast<uint16_t>((_width + 1U) / 2U);
            case SpriteFormat::Monochrome1:
                return static_cast<uint16_t>((_width + 7U) / 8U);
            }

            return 0;
        }

        void DrawGrayscale4(EiseiCanvas &canvas, int16_t x, int16_t y, const uint8_t *framePointer)
        {
            const uint8_t matteNibble = _matte & 0x0FU;
            const uint16_t rowStride = GetRowStride();
            for (uint8_t py = 0; py < _height; ++py)
            {
                const uint8_t *row = framePointer + (py * rowStride);
                for (uint8_t px = 0; px < _width; ++px)
                {
                    const uint8_t packed = row[px / 2U];
                    const uint8_t nibble = (px & 0x01U) ? (packed & 0x0FU) : (packed >> 4U);
                    if (nibble == matteNibble)
                    {
                        continue;
                    }

                    const uint8_t color = nibble;
                    canvas.setPixel(x + px, y + py, color);
                }
            }
        }

        void DrawMonochrome(EiseiCanvas &canvas, int16_t x, int16_t y, const uint8_t *framePointer)
        {
            const uint16_t rowStride = GetRowStride();
            for (uint8_t py = 0; py < _height; ++py)
            {
                const uint8_t *row = framePointer + (py * rowStride);
                for (uint8_t px = 0; px < _width; ++px)
                {
                    bool bitSet = row[px / 8U] & static_cast<uint8_t>(0x80U >> (px & 0x07U));
                    uint8_t color = bitSet ? _foreground : _background;
                    if (color != _matte)
                    {
                        canvas.setPixel(x + px, y + py, color);
                    }
                }
            }
        }

        void InitDefaults()
        {
            _texture = nullptr;
            _width = 0;
            _height = 0;
            _frameStride = 0;
            _matte = 16U;
            _foreground = 0xFFU;
            _background = 0x00U;
            _position.x = 0;
            _position.y = 0;
            _frame = 0;
            _mode = BlendMode::Normal;
            _format = SpriteFormat::Grayscale8;
        }

        const uint8_t *_texture;
        uint8_t _width;
        uint8_t _height;
        uint16_t _frameStride;
        Vector2 _position;
        uint8_t _matte;
        uint8_t _foreground;
        uint8_t _background;
        uint8_t _frame;
        BlendMode _mode;
        SpriteFormat _format;
    };
}

#endif // !SPRITE_H
