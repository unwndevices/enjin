#include "../../include/enjin2/ui/style.hpp"
#include <cstring>

namespace enjin2 {

// Slot-name → enum cascade. No luaL_checkoption in the repo (see #19); this is
// the same {name → enum} strcmp cascade the font registry uses. Names are the
// lower-camel slot ids so Lua reads `setStyle("panelSelected", …)`.
bool styleSlotFromName(const char* name, StyleSlot& out) {
    if (!name) return false;
    if (strcmp(name, "panel") == 0)                { out = StyleSlot::Panel; return true; }
    if (strcmp(name, "panelSelected") == 0)        { out = StyleSlot::PanelSelected; return true; }
    if (strcmp(name, "popup") == 0)                { out = StyleSlot::Popup; return true; }
    if (strcmp(name, "banner") == 0)               { out = StyleSlot::Banner; return true; }
    if (strcmp(name, "sceneObject") == 0)          { out = StyleSlot::SceneObject; return true; }
    if (strcmp(name, "sceneObjectSelected") == 0)  { out = StyleSlot::SceneObjectSelected; return true; }
    return false;
}

}  // namespace enjin2
