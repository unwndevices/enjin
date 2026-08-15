// Shape widget unit test (unwn #206): the engine-internal shape primitives
// promoted to one reflected ShapeComponent + ShapeSystem, placed by the shared
// Position+Size box.
//
// The geometry is the seam: a rect fills / frames the box, a circle is
// inscribed in it, a line runs the box corner to corner. A render pass draws
// onto a Canvas4 and asserts each primitive inks where it should and leaves the
// rest black; a zero-size box is a no-op.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/widgets/shape.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

using ShapeWorld = World<8, ShapeComponent, PositionComponent, SizeComponent>;

template<int W, int H>
static Entity placeShape(ShapeWorld& world, const ShapeComponent& shape,
                         Point pos, Size box) {
    Entity e = world.create();
    world.add<ShapeComponent>(e, shape.type, shape.filled, shape.thickness, shape.color, shape.radius);
    world.add<PositionComponent>(e, pos);
    world.add<SizeComponent>(e, box);
    return e;
}

// A rect frame (filled:false) strokes the box border but leaves the interior;
// a filled rect inks the interior too.
static void test_rect_frame_vs_filled() {
    {
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Rect, false, 1, Pixel4(15)),
                           Point(0, 0), Size(10, 10));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(0, 0) == 15, "shape: frame inks the top-left corner");
        ASSERT((uint8_t)canvas.getPixel(9, 0) == 15, "shape: frame inks the top-right corner");
        ASSERT((uint8_t)canvas.getPixel(5, 5) == 0, "shape: frame leaves the interior black");
    }
    {
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Rect, true, 1, Pixel4(15)),
                           Point(0, 0), Size(10, 10));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(5, 5) == 15, "shape: filled rect inks the interior");
    }
}

// A thick outline strokes concentric rings: thickness 2 inks both the border
// and the ring one pixel in.
static void test_rect_thickness_rings() {
    ShapeWorld world;
    Canvas4<16, 16> canvas;
    ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
    placeShape<16, 16>(world, ShapeComponent(ShapeType::Rect, false, 2, Pixel4(15)),
                       Point(0, 0), Size(10, 10));
    canvas.clear(Colors::BLACK);
    system.update(0.0f);
    ASSERT((uint8_t)canvas.getPixel(0, 0) == 15, "shape: thick frame inks the outer ring");
    ASSERT((uint8_t)canvas.getPixel(1, 1) == 15, "shape: thick frame inks the inner ring");
    ASSERT((uint8_t)canvas.getPixel(5, 5) == 0, "shape: thick frame still leaves the core black");
}

// A circle is inscribed in the box: the rim reaches the box edge at the
// mid-height, and an unfilled circle leaves its center black.
static void test_circle_inscribed() {
    {
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        // 11x11 box: r=5, center (5,5); the rim's leftmost point is (0,5).
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Circle, false, 1, Pixel4(15)),
                           Point(0, 0), Size(11, 11));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(0, 5) == 15, "shape: circle rim reaches the box edge");
        ASSERT((uint8_t)canvas.getPixel(5, 5) == 0, "shape: outline circle leaves its center black");
    }
    {
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Circle, true, 1, Pixel4(15)),
                           Point(0, 0), Size(11, 11));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(5, 5) == 15, "shape: filled circle inks its center");
    }
}

// A line runs the box's top-left to bottom-right corner along the diagonal.
static void test_line_corner_to_corner() {
    ShapeWorld world;
    Canvas4<16, 16> canvas;
    ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
    placeShape<16, 16>(world, ShapeComponent(ShapeType::Line, false, 1, Pixel4(15)),
                       Point(0, 0), Size(8, 8));
    canvas.clear(Colors::BLACK);
    system.update(0.0f);
    ASSERT((uint8_t)canvas.getPixel(0, 0) == 15, "shape: line inks the top-left corner");
    ASSERT((uint8_t)canvas.getPixel(7, 7) == 15, "shape: line inks the bottom-right corner");
    ASSERT((uint8_t)canvas.getPixel(3, 3) == 15, "shape: line inks the diagonal midpoint");
    ASSERT((uint8_t)canvas.getPixel(7, 0) == 0, "shape: line leaves the off-diagonal corner black");
}

// A rect with radius>0 rounds its corners (unwn #245): the round-rect path
// cuts the corner pixel a sharp rect would ink, while the body still fills.
// radius==0 keeps the corner (byte-identical to a pre-#245 sharp rect).
static void test_rect_rounded_corners() {
    {
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Rect, true, 1, Pixel4(15), 3),
                           Point(0, 0), Size(10, 10));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(0, 0) == 0, "shape: rounded rect clears the corner pixel");
        ASSERT((uint8_t)canvas.getPixel(5, 5) == 15, "shape: rounded rect still fills its body");
    }
    {
        // radius==0 is the sharp path: the corner is inked, unchanged from #206.
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Rect, true, 1, Pixel4(15), 0),
                           Point(0, 0), Size(10, 10));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(0, 0) == 15, "shape: radius 0 keeps the sharp corner");
    }
    {
        // An over-large radius is clamped to min(w,h)/2 (unwn #168 has no
        // internal clamp): the shape still draws a valid inked body, no overrun.
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Rect, true, 1, Pixel4(15), 200),
                           Point(0, 0), Size(10, 10));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(5, 5) == 15, "shape: over-large radius clamps and still fills");
    }
}

// Circle and Line ignore radius entirely (unwn #245): a radius set on either
// draws exactly as it would with radius 0.
static void test_circle_line_ignore_radius() {
    {
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Circle, true, 1, Pixel4(15), 4),
                           Point(0, 0), Size(11, 11));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(5, 5) == 15, "shape: circle ignores radius (fills center)");
    }
    {
        ShapeWorld world;
        Canvas4<16, 16> canvas;
        ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
        placeShape<16, 16>(world, ShapeComponent(ShapeType::Line, false, 1, Pixel4(15), 4),
                           Point(0, 0), Size(8, 8));
        canvas.clear(Colors::BLACK);
        system.update(0.0f);
        ASSERT((uint8_t)canvas.getPixel(0, 0) == 15, "shape: line ignores radius (inks corner)");
        ASSERT((uint8_t)canvas.getPixel(3, 3) == 15, "shape: line ignores radius (inks diagonal)");
    }
}

// A zero-size box has no geometry and draws nothing.
static void test_empty_box_is_safe() {
    ShapeWorld world;
    Canvas4<16, 16> canvas;
    ShapeSystem<ShapeWorld, Canvas4<16, 16>> system(&world, &canvas);
    placeShape<16, 16>(world, ShapeComponent(ShapeType::Rect, true, 1, Pixel4(15)),
                       Point(0, 0), Size(0, 0));
    canvas.clear(Colors::BLACK);
    system.update(0.0f);
    bool anyInk = false;
    for (int16_t y = 0; y < 16; ++y)
        for (int16_t x = 0; x < 16; ++x)
            if ((uint8_t)canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "shape: a zero-size box draws nothing");
}

int main() {
    test_rect_frame_vs_filled();
    test_rect_thickness_rings();
    test_rect_rounded_corners();
    test_circle_line_ignore_radius();
    test_circle_inscribed();
    test_line_corner_to_corner();
    test_empty_box_is_safe();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
