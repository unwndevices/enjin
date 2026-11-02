#include <enjin2/graphics/remap.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

static void test_default_is_identity()
{
    printf("--- default is identity ---\n");
    Remap r;
    ASSERT(r.isIdentity(), "default-constructed Remap should be identity");
    for (uint8_t i = 0; i < 16; ++i) {
        ASSERT(r.apply(i) == i, "identity: apply(i) == i");
    }
}

static void test_identity_factory()
{
    printf("--- identity() factory ---\n");
    Remap r = Remap::identity();
    ASSERT(r.isIdentity(), "identity() should be identity");
    ASSERT(r.apply(7) == 7, "identity() apply(7) == 7");
}

static void test_solid()
{
    printf("--- solid() ---\n");
    Remap r = Remap::solid(3);
    ASSERT(!r.isIdentity(), "solid(3) is not identity");
    for (uint8_t i = 0; i < 15; ++i) {
        ASSERT(r.apply(i) == 3, "solid(3): 0-14 map to 3");
    }
    ASSERT(r.apply(15) == 15, "solid(3): entry 15 stays transparent (15)");
}

static void test_apply_masks_low_nibble()
{
    printf("--- apply masks to low nibble ---\n");
    Remap r = Remap::identity();
    // 0x13 -> low nibble 3 -> lut[3] == 3
    ASSERT(r.apply(0x13) == 3, "apply masks high bits: apply(0x13)==3");
}

static void test_constexpr_usable()
{
    printf("--- constexpr usability ---\n");
    constexpr Remap r = Remap::solid(5);
    static_assert(r.lut[0] == 5, "solid(5) constexpr lut[0]==5");
    static_assert(r.lut[15] == 15, "solid(5) constexpr lut[15]==15");
    static_assert(Remap::identity().isIdentity(), "identity() constexpr isIdentity");
    ASSERT(true, "constexpr usage compiled");
}

static void test_toward_bg()
{
    printf("--- towardBg() ---\n");
    Remap r = Remap::towardBg();
    // bg <- surface <- muted <- text; bg holds; accents step into muted.
    ASSERT(r.apply(ROLE_BG) == ROLE_BG, "towardBg: bg holds");
    ASSERT(r.apply(ROLE_SURFACE) == ROLE_BG, "towardBg: surface -> bg");
    ASSERT(r.apply(ROLE_MUTED) == ROLE_SURFACE, "towardBg: muted -> surface");
    ASSERT(r.apply(ROLE_TEXT) == ROLE_MUTED, "towardBg: text -> muted");
    ASSERT(r.apply(ROLE_ACCENT) == ROLE_MUTED, "towardBg: accent -> muted");
    ASSERT(r.apply(ROLE_ACCENT4) == ROLE_MUTED, "towardBg: accent4 -> muted");
    ASSERT(r.apply(15) == 15, "towardBg: transparency passes through");
}

static void test_toward_text()
{
    printf("--- towardText() ---\n");
    Remap r = Remap::towardText();
    ASSERT(r.apply(ROLE_BG) == ROLE_SURFACE, "towardText: bg -> surface");
    ASSERT(r.apply(ROLE_SURFACE) == ROLE_MUTED, "towardText: surface -> muted");
    ASSERT(r.apply(ROLE_MUTED) == ROLE_TEXT, "towardText: muted -> text");
    ASSERT(r.apply(ROLE_TEXT) == ROLE_TEXT, "towardText: text holds");
    ASSERT(r.apply(ROLE_ACCENT2) == ROLE_TEXT, "towardText: accent2 -> text");
    ASSERT(r.apply(15) == 15, "towardText: transparency passes through");
}

static void test_recolor()
{
    printf("--- recolor() ---\n");
    Remap r = Remap::recolor(4, 9);
    ASSERT(!r.isIdentity(), "recolor(4,9) is not identity");
    ASSERT(r.apply(4) == 9, "recolor(4,9): 4 -> 9");
    ASSERT(r.apply(3) == 3, "recolor(4,9): other indices identity");
    ASSERT(r.apply(15) == 15, "recolor(4,9): transparency untouched");
    // A shader may repaint the transparent slot.
    Remap paint = Remap::recolor(15, 2);
    ASSERT(paint.apply(15) == 2, "recolor(15,2): shader paints the hole");
}

static void test_compose()
{
    printf("--- compose() ---\n");
    // Two steps toward bg: text -> muted -> surface.
    Remap twice = Remap::compose(Remap::towardBg(), Remap::towardBg());
    ASSERT(twice.apply(ROLE_TEXT) == ROLE_SURFACE, "compose(towardBg,towardBg): text -> surface");
    // towardBg then towardText: an accent lands on text (accent -> muted -> text).
    Remap r = Remap::compose(Remap::towardBg(), Remap::towardText());
    ASSERT(r.apply(ROLE_ACCENT) == ROLE_TEXT, "compose(towardBg,towardText): accent -> muted -> text");
    // Order matters: solid(3) then recolor(3->7) yields solid(7) over 0-14.
    Remap ordered = Remap::compose(Remap::solid(3), Remap::recolor(3, 7));
    ASSERT(ordered.apply(0) == 7, "compose(solid(3),recolor(3,7)): 0 -> 3 -> 7");
    ASSERT(ordered.apply(10) == 7, "compose: every source funnels 3 -> 7");
    // Composing with identity is a no-op either side.
    Remap ti = Remap::compose(Remap::towardText(), Remap::identity());
    ASSERT(ti.apply(ROLE_BG) == ROLE_SURFACE, "compose(towardText,identity) == towardText");
    Remap it = Remap::compose(Remap::identity(), Remap::towardText());
    ASSERT(it.apply(ROLE_BG) == ROLE_SURFACE, "compose(identity,towardText) == towardText");
}

static void test_chain_ctors_constexpr()
{
    printf("--- chain ctors constexpr ---\n");
    static_assert(Remap::towardBg().lut[ROLE_TEXT] == ROLE_MUTED, "towardBg() constexpr");
    static_assert(Remap::towardText().lut[ROLE_BG] == ROLE_SURFACE, "towardText() constexpr");
    static_assert(Remap::recolor(4, 9).lut[4] == 9, "recolor() constexpr");
    static_assert(Remap::compose(Remap::solid(3), Remap::recolor(3, 7)).lut[0] == 7, "compose() constexpr");
    ASSERT(true, "chain ctors usable at compile time");
}

int main()
{
    printf("remap_test\n");
    printf("==========\n");

    test_default_is_identity();
    test_identity_factory();
    test_solid();
    test_apply_masks_low_nibble();
    test_constexpr_usable();
    test_toward_bg();
    test_toward_text();
    test_recolor();
    test_compose();
    test_chain_ctors_constexpr();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
