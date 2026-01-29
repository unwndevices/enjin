#ifndef ENUMCLASSHASH_HPP
#define ENUMCLASSHASH_HPP
#include <stddef.h>

namespace enjin
{
    struct EnumClassHash
    {
        template <typename T>
        size_t operator()(T t) const
        {
            return static_cast<size_t>(t);
        }
    };
}
#endif // ENUMCLASSHASH_HPP
