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
#include <initializer_list>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); failures++; } \
        else { passes++; } \
    } while (0)

// Tones are shade-chain steps from fill (never stored).
static void test_derived_tones() {
    printf("--- derived tones from fill ---\n");
    Style s{};
    s.fill = Pixel4(ROLE_MUTED);
    ASSERT(s.towardText() == ROLE_TEXT, "towardText() of muted is text");
    ASSERT(s.towardBg() == ROLE_SURFACE, "towardBg() of muted is surface");
    ASSERT(s.shadow() == ROLE_SURFACE, "shadow() of muted is surface");
    ASSERT(s.shadow() == s.towardBg(), "shadow tone equals the toward-bg tone (#19)");

    // The ends of the chain hold.
    Style end{};
    end.fill = Pixel4(ROLE_TEXT);
    ASSERT(end.towardText() == ROLE_TEXT, "towardText() holds at text");
    end.fill = Pixel4(ROLE_BG);
    ASSERT(end.towardBg() == ROLE_BG, "towardBg() holds at bg");
    // An accent fill steps into muted toward bg.
    end.fill = Pixel4(ROLE_ACCENT);
    ASSERT(end.shadow() == ROLE_MUTED, "shadow() of accent is muted");
}

// Slot count + default table are consistent and picked by job (ADR-0019).
static void test_default_table() {
    printf("--- ROM default table ---\n");
    ASSERT(kStyleSlotCount == 6, "six slots in v1");
    const Style& panel = kDefaultStyles[(int)StyleSlot::Panel];
    ASSERT(panel.fill.value == ROLE_SURFACE, "Panel fill is surface");
    ASSERT(panel.text.value == ROLE_TEXT, "Panel text is text");
    ASSERT(panel.border.value == ROLE_MUTED, "Panel border is muted");
    // Selected variants take the accent fill, and content on accent is bg.
    for (StyleSlot sel : {StyleSlot::PanelSelected, StyleSlot::SceneObjectSelected}) {
        ASSERT(kDefaultStyles[(int)sel].fill.value == ROLE_ACCENT, "selected fill is accent");
        ASSERT(kDefaultStyles[(int)sel].text.value == ROLE_BG, "text on accent is bg");
    }
    ASSERT(kDefaultStyles[(int)StyleSlot::Popup].fill.value == ROLE_SURFACE, "Popup fill is surface");
    ASSERT(kDefaultStyles[(int)StyleSlot::Banner].fill.value == ROLE_SURFACE, "Banner fill is surface");
    ASSERT(kDefaultStyles[(int)StyleSlot::SceneObject].fill.value == ROLE_MUTED, "SceneObject fill is muted");
    // Metrics are sane placeholders.
    ASSERT(panel.borderWidth == 1, "Panel border width 1");
    // Type-system legibility outline (#40): on for the banner, off for panel text.
    ASSERT(kDefaultStyles[(int)StyleSlot::Banner].textOutline == 1, "Banner outline on by default");
    ASSERT(panel.textOutline == 0, "Panel text outline off");
    ASSERT(kDefaultStyles[(int)StyleSlot::Popup].textOutline == 0, "Popup text outline off");
}

// The role contract: text never shares a role with its fill, every colour is
// a Palette role (0-7), and content never sits on accent2/accent3.
static void test_slots_follow_role_contract() {
    printf("--- default slots follow the role contract ---\n");
    for (int i = 0; i < kStyleSlotCount; ++i) {
        const Style& st = kDefaultStyles[i];
        ASSERT(st.fill.value < ROLE_COUNT, "fill is a Palette role");
        ASSERT(st.text.value < ROLE_COUNT, "text is a Palette role");
        ASSERT(st.border.value < ROLE_COUNT, "border is a Palette role");
        ASSERT(st.text.value != st.fill.value, "text never shares a role with its fill");
        ASSERT(st.border.value != st.fill.value, "border differs from its fill");
        ASSERT(st.fill.value != ROLE_ACCENT2 && st.fill.value != ROLE_ACCENT3,
               "no content on accent2/accent3");
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
    test_slots_follow_role_contract();
    test_slot_name_cascade();
    printf("\nstyle_slot_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
