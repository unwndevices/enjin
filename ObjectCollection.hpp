#ifndef OBJECTCOLLECTION_HPP
#define OBJECTCOLLECTION_HPP

#include <memory>
#include <vector>

#include "Object.hpp"
#include "Components/S_Drawable.hpp"
#include "enjin2_compat.hpp"

namespace enjin
{
    class ObjectCollection
    {
    public:
        void Add(std::shared_ptr<Object> object);

        void Update(uint16_t deltaTime);
        void LateUpdate(uint16_t deltaTime);
        void Draw(EiseiCanvas &canvas);

        void ProcessNewObjects();
        void ProcessRemovals();

    private:
        std::vector<std::shared_ptr<Object>> objects;
        std::vector<std::shared_ptr<Object>> newObjects;
        S_Drawable drawables;
    };
}
#endif // OBJECTCOLLECTION_HPP
