#ifndef POSTFX_HPP
#define POSTFX_HPP

#include "../Object.hpp"
#include "../Components/C_CrtSim.hpp"
#include <Adafruit_GFX.h>
#include <memory>

namespace enjin
{
    class PostFx : public Object
    {
    public:
        PostFx();

        std::shared_ptr<C_CrtSim> crtSimComponent;
    };
}

#endif // POSTFX_HPP
