#include <enjin2/input/input_state.hpp>
#include <cstdio>
using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

static void test_zero_state() {
    InputState s{};
    ASSERT(!s.justPressed(0),   "zero state: justPressed(0) == false");
    ASSERT(!s.held(0),          "zero state: held(0) == false");
    ASSERT(!s.justReleased(0),  "zero state: justReleased(0) == false");
}

static void test_just_pressed() {
    InputState s{};
    s.buttons = static_cast<uint16_t>(1u << 0);
    s.prev_buttons = 0;
    ASSERT( s.justPressed(0),  "just pressed: justPressed(0) == true");
    ASSERT( s.held(0),         "just pressed: held(0) == true");
    ASSERT(!s.justReleased(0), "just pressed: justReleased(0) == false");
}

static void test_held() {
    InputState s{};
    s.buttons      = static_cast<uint16_t>(1u << 0);
    s.prev_buttons = static_cast<uint16_t>(1u << 0);
    ASSERT(!s.justPressed(0),  "held: justPressed(0) == false");
    ASSERT( s.held(0),         "held: held(0) == true");
    ASSERT(!s.justReleased(0), "held: justReleased(0) == false");
}

static void test_just_released() {
    InputState s{};
    s.buttons      = 0;
    s.prev_buttons = static_cast<uint16_t>(1u << 0);
    ASSERT(!s.justPressed(0),  "just released: justPressed(0) == false");
    ASSERT(!s.held(0),         "just released: held(0) == false");
    ASSERT( s.justReleased(0), "just released: justReleased(0) == true");
}

static void test_advance_frame() {
    InputState s{};
    s.buttons   = static_cast<uint16_t>(1u << 3);
    s.axes[2]   = 0.5f;
    input_advance_frame(&s);
    ASSERT(s.prev_buttons == static_cast<uint16_t>(1u << 3), "advance: prev_buttons == (1<<3)");
    ASSERT(s.buttons      == 0,                              "advance: buttons == 0 after advance");
    ASSERT(s.prev_axes[2] == 0.5f,                           "advance: prev_axes[2] == 0.5f");
    ASSERT(s.axes[2]      == 0.0f,                           "advance: axes[2] == 0.0f after advance");
}

static void test_no_cross_contamination() {
    InputState s{};
    s.buttons = static_cast<uint16_t>(1u << 1);
    ASSERT(!s.held(0), "no cross-contamination: held(0) == false when only btn 1 set");
    ASSERT( s.held(1), "no cross-contamination: held(1) == true");
}

int main() {
    printf("=== input_test ===\n");
    test_zero_state();
    test_just_pressed();
    test_held();
    test_just_released();
    test_advance_frame();
    test_no_cross_contamination();
    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
