#include "SceneStateMachine.hpp"

namespace enjin
{
    void SceneStateMachine::ProcessInput()
    {

        if (curScene)
        {
            curScene->ProcessInput();
        }
    }

    void SceneStateMachine::Update(uint16_t delta_time)
    {
        if (curScene)
        {
            curScene->Update(delta_time);
        }
    }

    void SceneStateMachine::LateUpdate(uint16_t delta_time)
    {
        if (curScene)
        {
            curScene->LateUpdate(delta_time);
        }
    }

    void SceneStateMachine::Draw(Display &display)
    {
        if (curScene)
        {
            curScene->Draw(display);
        }
    }

    uint8_t SceneStateMachine::Add(std::shared_ptr<Scene> scene)
    {
        auto inserted = scenes.insert(std::make_pair(insertedSceneID, scene));

        insertedSceneID++;

        inserted.first->second->OnCreate();

        return insertedSceneID - 1;
    }

    void SceneStateMachine::Remove(uint8_t id)
    {
        auto it = scenes.find(id);
        if (it != scenes.end())
        {
            if (curScene == it->second)
            {
                // If the scene we are removing is the current scene,
                // we also want to set that to a null pointer so the scene
                // is no longer updated.
                curScene = nullptr;
            }

            // We make sure to call the OnDestroy method
            // of the scene we are removing.
            it->second->OnDestroy();

            scenes.erase(it);
        }
    }

    void SceneStateMachine::SwitchTo(uint8_t id)
    {
        auto it = scenes.find(id);
        if (it != scenes.end())
        {
            if (curScene)
            {
                // If we have a current scene, we call its OnDeactivate method.
                curScene->OnDeactivate();
            }

            // Setting the current scene ensures that it is updated and drawn.
            curScene = it->second;
            curSceneID = id;
            curScene->OnActivate();
        }
    }
}