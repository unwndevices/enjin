#include "Bitmask.hpp"

namespace enjin
{
    Bitmask::Bitmask() : bits{0, 0, 0, 0} {}

    void Bitmask::SetMask(Bitmask &other)
    {
        for (int i = 0; i < 4; i++)
        {
            bits[i] = other.GetMask(i);
        }
    }

    uint32_t Bitmask::GetMask(int index) const
    {
        if (index >= 4)
        {
            return 0; // Or throw an error
        }
        return bits[index];
    }

    bool Bitmask::GetBit(int pos) const
    {
        int index = pos / 32;
        int bitPos = pos % 32;

        if (index >= 4)
        {
            return false; // Or throw an error
        }

        return (bits[index] & (1 << bitPos)) != 0;
    }

    void Bitmask::SetBit(int pos, bool on)
    {
        if (on)
        {
            SetBit(pos);
        }
        else
        {
            ClearBit(pos);
        }
    }

    void Bitmask::SetBit(int pos)
    {
        int index = pos / 32;
        int bitPos = pos % 32;

        if (index >= 4)
        {
            return; // Or throw an error
        }

        bits[index] = bits[index] | (1 << bitPos);
    }

    void Bitmask::ClearBit(int pos)
    {
        int index = pos / 32;
        int bitPos = pos % 32;

        if (index >= 4)
        {
            return; // Or throw an error
        }

        bits[index] = bits[index] & ~(1 << bitPos);
    }

    void Bitmask::Clear()
    {
        for (int i = 0; i < 4; i++)
        {
            bits[i] = 0;
        }
    }
}