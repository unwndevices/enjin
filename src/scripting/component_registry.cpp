#include "../../include/enjin2/scripting/component_registry.hpp"
#include "../../include/enjin2/core/object.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/state_machine.hpp"
#include "../../include/enjin2/components/tilemap.hpp"
#include "../../include/enjin2/components/camera.hpp"
#include "../../include/enjin2/components/sprite.hpp"
#include <cstring>

namespace enjin2 {

namespace {

// Generated thunks: one instantiation of getComponent<T>/addComponent<T> per
// registered type, so the whole component vocabulary reduces to plain function
// pointers the Lua dispatch can iterate.
template <typename T>
Component* regGet(Object* o) { return o ? o->getComponent<T>() : nullptr; }

template <typename T>
Component* regAdd(Object* o) { return o ? o->addComponent<T>() : nullptr; }

const ComponentRegistryEntry kEntries[] = {
#define X(Type, Meta) {#Type, Meta, &regGet<Type>, &regAdd<Type>},
    ENJIN2_COMPONENT_LIST(X)
#undef X
};

constexpr std::size_t kCount = sizeof(kEntries) / sizeof(kEntries[0]);

} // namespace

const ComponentRegistryEntry* componentRegistry(std::size_t& countOut) {
    countOut = kCount;
    return kEntries;
}

const ComponentRegistryEntry* findComponentEntry(const char* luaName) {
    if (!luaName) return nullptr;
    for (std::size_t i = 0; i < kCount; ++i) {
        if (std::strcmp(kEntries[i].luaName, luaName) == 0) return &kEntries[i];
    }
    return nullptr;
}

} // namespace enjin2
