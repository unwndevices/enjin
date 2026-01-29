#ifndef TYPES_HPP
#define TYPES_HPP
#include <stdint.h>
#include <math.h>

namespace enjin
{
    typedef struct Vector2
    {
        int16_t x;
        int16_t y;

        Vector2(int16_t x = 0, int16_t y = 0) : x(x), y(y) {}

        static inline Vector2 Lerp(const Vector2 &v1, const Vector2 &v2, float t)
        {
            return Vector2((int16_t)(v1.x + (int16_t)(ceil((v2.x - v1.x) * t))), (int16_t)(v1.y + (int16_t)(ceil((v2.y - v1.y) * t))));
        }

        static inline Vector2 Lerp(const Vector2 &v1, const Vector2 &v2, uint16_t t)
        {
            // Linear interpolation in integer space.
            return Vector2((v1.x * (254 - t) + v2.x * t) / 254,
                           (v1.y * (254 - t) + v2.y * t) / 254);
        }
        Vector2 operator+(const Vector2 &other) const
        {
            return Vector2(x + other.x, y + other.y);
        }
        Vector2 &operator+=(const Vector2 &other)
        {
            x += other.x;
            y += other.y;
            return *this;
        }
        Vector2 &operator-=(const Vector2 &other)
        {
            x -= other.x;
            y -= other.y;
            return *this;
        }

        Vector2 operator-(const Vector2 &other) const
        {
            return Vector2(x - other.x, y - other.y);
        }

        Vector2 operator*(const int16_t &scalar) const
        {
            return Vector2(x * scalar, y * scalar);
        }

        Vector2 operator/(const int16_t &scalar) const
        {
            return Vector2(x / scalar, y / scalar);
        }
    } Vector2;

    typedef struct Vector3
    {
        float x = 0, y = 0, z = 0;

        void normalize()
        {
            float len = sqrt(x * x + y * y + z * z);
            if (len > 1e-6)
            { // Avoid division by zero
                x /= len;
                y /= len;
                z /= len;
            }
        }
    } Vector3;

}
#endif // !TYPES_HPP
