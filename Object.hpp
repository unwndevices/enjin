#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <stdint.h>
#include <Adafruit_GFX.h>

#include <memory>
#include <vector>

#include "Components/Component.hpp"
#include "Components/C_Drawable.hpp"
#include "Components/C_Position.hpp"

namespace enjin
{
    class Object
    {
    public:
        Object();
        // Awake is called when object created. Use to ensure
        // required components are present.
        void Awake();
        void Start();

        virtual void Update(uint16_t deltaTime)
        {
            for (const auto &component : components)
            {
                component->Update(deltaTime);
            }
        };

        void LateUpdate(uint16_t deltaTime);

        template <typename T>
        std::shared_ptr<T> AddComponent()
        {
            static_assert(std::is_base_of<Component, T>::value,
                          "T must derive from Component");

            // The object does not have this component so we create it and
            // add it to our list.
            std::shared_ptr<T> newComponent = std::make_shared<T>(this);
            components.push_back(newComponent);

            if (std::dynamic_pointer_cast<C_Drawable>(newComponent))
            {
                drawables.push_back(std::dynamic_pointer_cast<C_Drawable>(newComponent));
            }

            return newComponent;
        };

        template <typename T, typename... Args>
        std::shared_ptr<T> AddComponent(Args &&...args)
        {
            static_assert(std::is_base_of<Component, T>::value,
                          "T must derive from Component");

            // The object does not have this component so we create it and
            // add it to our list.
            std::shared_ptr<T> newComponent = std::make_shared<T>(this, std::forward<Args>(args)...);
            components.push_back(newComponent);

            if (std::dynamic_pointer_cast<C_Drawable>(newComponent))
            {
                drawables.push_back(std::dynamic_pointer_cast<C_Drawable>(newComponent));
            }

            return newComponent;
        };

        template <typename T>
        std::shared_ptr<T> GetComponent()
        {
            static_assert(std::is_base_of<Component, T>::value,
                          "T must derive from Component");

            // Check that we don't already have a component of this type.
            for (auto &exisitingComponent : components)
            {
                if (std::dynamic_pointer_cast<T>(exisitingComponent))
                {
                    return std::dynamic_pointer_cast<T>(exisitingComponent);
                }
            }

            return nullptr;
        };
        std::vector<std::shared_ptr<C_Drawable>> GetDrawables();

        std::shared_ptr<C_Position> position;

        bool IsQueuedForRemoval();
        void QueueForRemoval();

    protected:
        std::vector<std::shared_ptr<Component>> components;
        std::vector<std::shared_ptr<C_Drawable>> drawables; // Multiple drawables
        bool queuedForRemoval;
    };
}
#endif // OBJECT_HPP
