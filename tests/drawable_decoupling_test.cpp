/**
 * @file drawable_decoupling_test.cpp
 * @brief Regression test for getComponents<C_Drawable>() API
 *
 * Verifies that Object::getComponents<T>() correctly scans components
 * via dynamic_cast and returns matching instances. This is the replacement
 * API after the C_Drawable cache was removed from Object in Phase 36-01.
 *
 * Tests cover:
 *   1. Object with no drawable components returns 0
 *   2. Object with one drawable returns 1 with correct pointer
 *   3. Object with two drawables returns 2 in insertion order
 *   4. maxOut cap limits results correctly
 */
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/drawable.hpp"
#include <cassert>
#include <cstdio>

// Minimal concrete drawable for testing multiple-drawable case.
// C_Drawable constructor requires (Object*, uint8_t width, uint8_t height).
struct TestDrawable : public enjin2::C_Drawable {
    TestDrawable(enjin2::Object* o) : enjin2::C_Drawable(o, 1, 1) {}
    void draw(enjin2::ICanvas<enjin2::Pixel4>&) override {}
};

int main() {
    // Test 1: Object with no extra drawables (Object auto-adds C_Position only)
    // C_Position is NOT a C_Drawable, so result should be 0.
    {
        enjin2::Object obj;
        enjin2::C_Drawable* out[16];
        size_t n = obj.getComponents<enjin2::C_Drawable>(out, 16);
        assert(n == 0 && "Object with only C_Position should have 0 drawables");
        printf("PASS: no drawables on fresh Object\n");
    }

    // Test 2: Object with one TestDrawable
    {
        enjin2::Object obj;
        auto* td = obj.addComponent<TestDrawable>();
        assert(td != nullptr);
        enjin2::C_Drawable* out[16];
        size_t n = obj.getComponents<enjin2::C_Drawable>(out, 16);
        assert(n == 1 && "Object with one TestDrawable should return 1");
        assert(out[0] == td && "Returned pointer must match the added component");
        printf("PASS: one drawable returned correctly\n");
    }

    // Test 3: Object with two TestDrawables — returned in insertion order
    {
        enjin2::Object obj;
        auto* td1 = obj.addComponent<TestDrawable>();
        auto* td2 = obj.addComponent<TestDrawable>();
        assert(td1 != nullptr && td2 != nullptr);
        enjin2::C_Drawable* out[16];
        size_t n = obj.getComponents<enjin2::C_Drawable>(out, 16);
        assert(n == 2 && "Object with two TestDrawables should return 2");
        assert(out[0] == td1);
        assert(out[1] == td2);
        printf("PASS: two drawables returned in insertion order\n");
    }

    // Test 4: maxOut limits results
    {
        enjin2::Object obj;
        obj.addComponent<TestDrawable>();
        obj.addComponent<TestDrawable>();
        enjin2::C_Drawable* out[1];
        size_t n = obj.getComponents<enjin2::C_Drawable>(out, 1);
        assert(n == 1 && "maxOut=1 must cap results at 1");
        printf("PASS: maxOut cap works correctly\n");
    }

    printf("ALL drawable_decoupling_test PASSED\n");
    return 0;
}
