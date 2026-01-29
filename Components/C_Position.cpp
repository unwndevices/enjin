#include "C_Position.hpp"

namespace enjin
{
    C_Position::C_Position(Object *owner, int16_t x, int16_t y)
        : Component(owner), position(x, y), x(position.x), y(position.y) {}

    void C_Position::SetPosition(int16_t x, int16_t y)
    {
        position.x = x;
        position.y = y;
    }

    void C_Position::SetPosition(Vector2 pos)
    {
        position = pos;
    }

    void C_Position::AddPosition(int16_t x, int16_t y)
    {
        position.x += x;
        position.y += y;
    }

    void C_Position::AddPosition(Vector2 pos)
    {
        position += pos;
    }

    void C_Position::SetX(int16_t x)
    {
        position.x = x;
    }

    void C_Position::SetY(int16_t y)
    {
        position.y = y;
    }

    void C_Position::AddX(int16_t x)
    {
        position.x += x;
    }

    void C_Position::AddY(int16_t y)
    {
        position.y += y;
    }

    const Vector2 &C_Position::GetPosition() const
    {
        return position;
    }
}