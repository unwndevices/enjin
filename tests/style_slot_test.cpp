/**
 * @file style_slot_test.cpp
 * @brief Tests for the style-slot layer (#19 / build #37).
 *
 * Pure-logic half: derived tones from `fill`, the ROM default table, and the
 * slot-name strcmp cascade. The Lua-surface half (setStyle mask behaviour,
 * setTheme clear, panel slot-vs-legacy dispatch) lives in style_slot_lua_test.
 */
#include <enjin2/ui/style.hpp>
#include <enjin2/graphics/palette.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); failures++; } \
        else { passes++; } \
    } while (0)

// Tones are shade-stepped from fill within its ramp (never stored).
static void test_derived_tones() {
    printf("--- derived tones from fill ---\n");
    // fill = base shade of ramp 3 (scenery): index ramp(3,BASE).
    Style s{};
    s.fill = Pixel4(Palette::ramp(3, SHADE_BASE));
    ASSERT(s.light() == Palette::ramp(3, SHADE_LIGHT), "light() steps to ramp light shade");
    ASSERT(s.dark()  == Palette::ramp(3, SHADE_DARK),  "dark() steps to ramp dark shade");
    ASSERT(s.shadow() == Palette::darken(s.fill.value), "shadow() == darken(fill)");
    ASSERT(s.shadow() == s.dark(), "shadow tone equals dark tone (#19)");

    // A light fill has no lighten headroom → light() clamps to itself.
    Style top{};
    top.fill = Pixel4(Palette::ramp(1, SHADE_LIGHT));
    ASSERT(top.light() == top.fill.value, "light() clamps at the ramp light end");
}

// Slot count + default table are consistent and role-based.
static void test_default_table() {
    printf("--- ROM default table ---\n");
    ASSERT(kStyleSlotCount == 6, "six slots in v1");
    // Selected variants take the selection-accent ramp (role 4).
    ASSERT(kDefaultStyles[(int)StyleSlot::PanelSelected].fill.value == Palette::ramp(4, SHADE_BASE),
           "PanelSelected fill is the selection-accent base");
    ASSERT(kDefaultStyles[(int)StyleSlot::SceneObject].fill.value == Palette::ramp(3, SHADE_BASE),
           "SceneObject fill is the scenery base");
    // Metrics are sane placeholders.
    ASSERT(kDefaultStyles[(int)StyleSlot::Panel].borderWidth == 1, "Panel border width 1");
}

// Every default fill sits at a ramp *base* shade — i.e. it is a structural role
// index, not a hand-picked hex. That is what lets a palette re-author re-skin
// the indices while the roles hold (#12/#19): the index is chosen by role, and
// the palette only changes the RGB behind it.
static void test_fills_are_role_indices() {
    printf("--- default fills are role ramp bases, not hexes ---\n");
    for (int i = 0; i < kStyleSlotCount; ++i) {
        const uint8_t f = kDefaultStyles[i].fill.value;
        ASSERT(f % RAMP_SHADES == SHADE_BASE, "fill is a ramp base shade (role index)");
    }
}

// Slot-name cascade matches the six names and rejects the unknown.
static void test_slot_name_cascade() {
    printf("--- slot-name cascade ---\n");
    StyleSlot s;
    ASSERT(styleSlotFromName("panel", s) && s == StyleSlot::Panel, "panel");
    ASSERT(styleSlotFromName("panelSelected", s) && s == StyleSlot::PanelSelected, "panelSelected");
    ASSERT(styleSlotFromName("popup", s) && s == StyleSlot::Popup, "popup");
    ASSERT(styleSlotFromName("banner", s) && s == StyleSlot::Banner, "banner");
    ASSERT(styleSlotFromName("sceneObject", s) && s == StyleSlot::SceneObject, "sceneObject");
    ASSERT(styleSlotFromName("sceneObjectSelected", s) && s == StyleSlot::SceneObjectSelected, "sceneObjectSelected");
    ASSERT(!styleSlotFromName("nope", s), "unknown name rejected");
    ASSERT(!styleSlotFromName(nullptr, s), "null name rejected");
}

int main() {
    test_derived_tones();
    test_default_table();
    test_fills_are_role_indices();
    test_slot_name_cascade();
    printf("\nstyle_slot_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
