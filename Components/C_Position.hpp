#ifndef C_POSITION_HPP
#define C_POSITION_HPP

#include "Component.hpp"

namespace enjin
{
    class C_Position : public Component
    {
    public:
        C_Position(Object *owner, int16_t x = 0, int16_t y = 0);

        void SetPosition(int16_t x, int16_t y);
        void SetPosition(Vector2 pos);

        void AddPosition(int16_t x, int16_t y);
        void AddPosition(Vector2 pos);

        // Allows us to set x/y values separately.
        void SetX(int16_t x);
        void SetY(int16_t y);
        void AddX(int16_t x);
        void AddY(int16_t y);

        const Vector2 &GetPosition() const;

        int16_t &x;
        int16_t &y;

    private:
        Vector2 position;
    };
}
#endif // C_POSITION_HPP
