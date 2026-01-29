#include "PostFx.hpp"

namespace enjin
{

    PostFx::PostFx() : Object()
    {
        crtSimComponent = AddComponent<C_CrtSim>();
        crtSimComponent->SetDrawLayer(DrawLayer::UI);

        crtSimComponent->SetVisibility(true);
    }
}
