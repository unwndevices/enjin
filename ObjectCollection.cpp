#include "ObjectCollection.hpp"

namespace enjin
{
    void ObjectCollection::Add(std::shared_ptr<Object> object)
    {
        newObjects.push_back(object);
    }

    void ObjectCollection::ProcessNewObjects()
    {
        if (newObjects.size() > 0)
        {
            for (const auto &o : newObjects)
            {
                o->Awake();
            }

            for (const auto &o : newObjects)
            {
                o->Start();
            }

            objects.assign(newObjects.begin(), newObjects.end());
            drawables.Add(newObjects);
            newObjects.clear();
        }
    }

    void ObjectCollection::ProcessRemovals()
    {
        auto objIterator = objects.begin();
        while (objIterator != objects.end())
        {
            auto obj = **objIterator;

            if (obj.IsQueuedForRemoval())
            {
                objIterator = objects.erase(objIterator);
            }
            else
            {
                ++objIterator;
            }
        }
        drawables.ProcessRemovals();
    }

    void ObjectCollection::Update(uint16_t deltaTime)
    {
        for (auto &o : objects)
        {
            o->Update(deltaTime);
        }
    }

    void ObjectCollection::LateUpdate(uint16_t deltaTime)
    {
        for (auto &o : objects)
        {
            o->LateUpdate(deltaTime);
        }
    }

    void ObjectCollection::Draw(EiseiCanvas &canvas)
    {
        drawables.Draw(canvas);
    }
}