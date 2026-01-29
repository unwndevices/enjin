#ifndef COMPONENT_HPP
#define COMPONENT_HPP

#include <stdint.h>
#include <Adafruit_GFX.h>
#include "../utils/Types.hpp"

namespace enjin
{
    class Object;

    class Component
    {
    public:
        Component(Object *owner) : owner(owner) {}

        Object *GetOwner() const { return owner; }
        virtual void Awake() {};
        virtual void Start() {};

        virtual void Update(uint16_t deltaTime) {};
        virtual void LateUpdate(uint16_t deltaTime) {};

    protected:
        Object *owner;
    };
}
#endif// COMPONENT_HPP
