#ifndef S_DRAWABLE_HPP
#define S_DRAWABLE_HPP

#include <map>
#include <vector>

#include "C_Drawable.hpp"
#include "../Object.hpp"
#include "../enjin2_compat.hpp"

namespace enjin
{
    class S_Drawable
    {
    public:
        void Add(std::vector<std::shared_ptr<Object>> &object);
        void Add(std::shared_ptr<Object> object);

        void ProcessRemovals();

        void Draw(EiseiCanvas &canvas);
        void Sort();

    private:
        std::map<DrawLayer, std::vector<std::shared_ptr<C_Drawable>>> drawables;
    };
}
#endif // S_DRAWABLE_HPP
