#include "S_Drawable.hpp"
#include <algorithm>

namespace enjin
{
    void S_Drawable::Add(std::vector<std::shared_ptr<Object>> &objects)
    {
        for (auto o : objects)
        {
            Add(o);
        }

        Sort();
    }
    void S_Drawable::Add(std::shared_ptr<Object> object)
    {
        std::vector<std::shared_ptr<C_Drawable>> objectDrawables = object->GetDrawables();

        for (auto &drawable : objectDrawables)
        {
            DrawLayer layer = drawable->GetDrawLayer();

            auto itr = this->drawables.find(layer);

            if (itr != this->drawables.end())
            {
                this->drawables[layer].push_back(drawable);
            }
            else
            {
                std::vector<std::shared_ptr<C_Drawable>> newDrawableList;
                newDrawableList.push_back(drawable);

                this->drawables.insert(std::make_pair(layer, newDrawableList));
            }
        }
    }

    void S_Drawable::Draw(EiseiCanvas &canvas)
    {
        for (auto &layer : drawables)
        {
            for (auto &drawable : layer.second)
            {
                if (drawable->GetVisibility())
                {
                    drawable->Draw(canvas);
                }
            }
        }
    }

    void S_Drawable::Sort()
    {
        for (auto &layer : drawables)
        {
            std::stable_sort(layer.second.begin(), layer.second.end(),
                      [](std::shared_ptr<C_Drawable> a,
                         std::shared_ptr<C_Drawable> b) -> bool
                      {
                          return a->GetSortOrder() < b->GetSortOrder();
                      });
        }
    }

    void S_Drawable::ProcessRemovals()
    {
        for (auto &layer : drawables)
        {
            auto drawIterator = layer.second.begin();
            while (drawIterator != layer.second.end())
            {
                auto drawable = *drawIterator;

                if (drawable->GetOwner()->IsQueuedForRemoval())
                {
                    drawIterator = layer.second.erase(drawIterator);
                }
                else
                {
                    ++drawIterator;
                }
            }
        }
    }
}