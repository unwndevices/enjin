#include "Object.hpp"

namespace enjin
{
    Object::Object() : queuedForRemoval(false)
    {
        position = AddComponent<C_Position>();
    }

    void Object::Awake()
    {
        for (const auto &component : components)
        {
            component->Awake();
        }
    }

    void Object::Start()
    {
        for (const auto &component : components)
        {
            component->Start();
        }
    }

    void Object::LateUpdate(uint16_t deltaTime)
    {
        for (const auto &component : components)
        {
            component->LateUpdate(deltaTime);
        }
    }

    // void Object::Draw(EiseiCanvas &canvas)
    // {
    //     for (auto &drawable : drawables)
    //     {
    //         drawable->Draw(canvas);
    //     }
    // }

    void Object::QueueForRemoval()
    {
        queuedForRemoval = true;
    }

    bool Object::IsQueuedForRemoval()
    {
        return queuedForRemoval;
    }

    std::vector<std::shared_ptr<C_Drawable>> Object::GetDrawables()
    {
        return drawables;
    }
}