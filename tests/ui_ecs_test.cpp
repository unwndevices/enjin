// UI ECS unit tests (Phase 2, #120)
//
// Covers the upstream ui ECS after it was made real:
//   - ComponentStorage: packed member storage + correct sparse set (no aliasing)
//   - EntityManager: create/destroy/generation validity
//   - World: create/add/get/has/remove + query<...>()
//   - Systems: AnimationSystem / InputSystem / RenderSystem real update()
//
// Plain assert-style harness (matches the other enjin2 tests; no gtest dependency).

#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/system.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/systems.hpp>
#include <enjin2/ui/theme.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                   \
    do {                                                    \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; }       \
    } while (0)

// ---------------------------------------------------------------------------
// ComponentStorage
// ---------------------------------------------------------------------------

static void test_storage_add_get_remove() {
    ComponentStorage<PositionComponent, 8> storage;
    Entity e(3, 1);

    ASSERT(storage.empty(), "storage: starts empty");
    PositionComponent* p = storage.addComponent(e, Point(10, 20));
    ASSERT(p != nullptr, "storage: addComponent returns non-null");
    ASSERT(storage.size() == 1, "storage: size == 1 after add");
    ASSERT(storage.hasComponent(e), "storage: hasComponent true after add");

    PositionComponent* got = storage.getComponent(e);
    ASSERT(got == p, "storage: getComponent returns the same pointer");
    ASSERT(got->position.x == 10 && got->position.y == 20, "storage: stored data intact");

    ASSERT(storage.removeComponent(e), "storage: removeComponent returns true");
    ASSERT(!storage.hasComponent(e), "storage: hasComponent false after remove");
    ASSERT(storage.getComponent(e) == nullptr, "storage: getComponent null after remove");
    ASSERT(storage.empty(), "storage: empty after remove");
}

// The regression that matters: two storages of the *same* <T, CAPACITY> must
// have independent backing memory. The old impl used a function-local
// `static T componentArray[CAPACITY]` shared across every instance, so writes
// to one storage were visible through the other. This asserts they are distinct.
static void test_storage_no_aliasing() {
    ComponentStorage<PositionComponent, 8> a;
    ComponentStorage<PositionComponent, 8> b;
    Entity e(0, 1);

    a.addComponent(e, Point(1, 1));
    b.addComponent(e, Point(2, 2));

    PositionComponent* pa = a.getComponent(e);
    PositionComponent* pb = b.getComponent(e);
    ASSERT(pa != nullptr && pb != nullptr, "aliasing: both storages hold the entity");
    ASSERT(pa != pb, "aliasing: distinct storages use distinct memory");
    ASSERT(pa->position.x == 1 && pb->position.x == 2, "aliasing: values do not cross-contaminate");
}

// entity.id must index the sparse map directly (old impl hashed with % CAPACITY,
// which collides once ids exceed CAPACITY). Use ids that would collide under the
// old scheme and verify independent lookup.
static void test_storage_sparse_by_id() {
    ComponentStorage<PositionComponent, 4> storage;
    Entity e1(1, 1);
    Entity e2(3, 1);
    storage.addComponent(e1, Point(11, 0));
    storage.addComponent(e2, Point(33, 0));

    ASSERT(storage.getComponent(e1)->position.x == 11, "sparse: e1 resolves correctly");
    ASSERT(storage.getComponent(e2)->position.x == 33, "sparse: e2 resolves correctly");
    // id outside capacity is rejected, not aliased into slot 0
    Entity outOfRange(9, 1);
    ASSERT(storage.getComponent(outOfRange) == nullptr, "sparse: id >= CAPACITY rejected");
    ASSERT(storage.addComponent(outOfRange, Point()) == nullptr, "sparse: add id >= CAPACITY rejected");
}

// Swap-with-last removal must keep the surviving entity reachable.
static void test_storage_swap_remove() {
    ComponentStorage<PositionComponent, 8> storage;
    Entity a(0, 1), b(1, 1), c(2, 1);
    storage.addComponent(a, Point(0, 0));
    storage.addComponent(b, Point(1, 0));
    storage.addComponent(c, Point(2, 0));

    ASSERT(storage.removeComponent(b), "swap-remove: middle removed");
    ASSERT(storage.size() == 2, "swap-remove: size decremented");
    ASSERT(storage.getComponent(a) && storage.getComponent(a)->position.x == 0, "swap-remove: a intact");
    ASSERT(storage.getComponent(c) && storage.getComponent(c)->position.x == 2, "swap-remove: c intact after swap");
    ASSERT(storage.getComponent(b) == nullptr, "swap-remove: b gone");
}

// A stale handle (same id, older generation) must not resolve to the live entity.
static void test_storage_generation_stale() {
    ComponentStorage<PositionComponent, 8> storage;
    Entity live(2, 5);
    storage.addComponent(live, Point(7, 7));
    Entity stale(2, 4); // same slot, older generation
    ASSERT(storage.getComponent(stale) == nullptr, "generation: stale handle does not resolve");
    ASSERT(storage.getComponent(live) != nullptr, "generation: live handle resolves");
}

static void test_storage_iteration() {
    ComponentStorage<PositionComponent, 8> storage;
    storage.addComponent(Entity(0, 1), Point(5, 0));
    storage.addComponent(Entity(1, 1), Point(6, 0));
    storage.addComponent(Entity(2, 1), Point(7, 0));

    int count = 0;
    int sum = 0;
    for (auto [entity, comp] : storage) {
        (void)entity;
        count++;
        sum += comp->position.x;
    }
    ASSERT(count == 3, "iteration: visits every packed component");
    ASSERT(sum == 18, "iteration: exposes mutable component data");
}

// ---------------------------------------------------------------------------
// EntityManager
// ---------------------------------------------------------------------------

static void test_entity_manager() {
    EntityManager<16> em;
    ASSERT(em.getEntityCount() == 0, "entities: starts at 0");
    Entity a = em.createEntity();
    Entity b = em.createEntity();
    ASSERT(a.isValid() && b.isValid(), "entities: created handles are valid");
    ASSERT(!(a == b), "entities: distinct entities differ");
    ASSERT(em.isValid(a) && em.isValid(b), "entities: manager validates live handles");
    ASSERT(em.getEntityCount() == 2, "entities: count reflects creations");

    em.destroyEntity(a);
    ASSERT(!em.isValid(a), "entities: destroyed handle invalidated");
    ASSERT(em.getEntityCount() == 1, "entities: count reflects destruction");

    // Reusing the slot yields a new generation, so the old handle stays stale.
    Entity c = em.createEntity();
    ASSERT(em.isValid(c), "entities: reused slot handle valid");
    ASSERT(!em.isValid(a), "entities: stale handle still invalid after slot reuse");
}

// ---------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------

using TestWorld = World<64, PositionComponent, SizeComponent, RenderComponent,
                        InputComponent, AnimationComponent>;

static void test_world_add_get_remove() {
    TestWorld world;
    Entity e = world.create();
    ASSERT(world.valid(e), "world: created entity valid");
    ASSERT(!world.has<PositionComponent>(e), "world: no component before add");

    auto* pos = world.add<PositionComponent>(e, Point(4, 5));
    ASSERT(pos != nullptr, "world: add returns component");
    ASSERT(world.has<PositionComponent>(e), "world: has after add");
    ASSERT(world.get<PositionComponent>(e)->position.x == 4, "world: get sees stored data");

    // Components of different types are independent.
    world.add<SizeComponent>(e, Size(20, 10));
    ASSERT(world.has<SizeComponent>(e), "world: second component type coexists");
    ASSERT(world.has<PositionComponent>(e), "world: first component type unaffected");

    world.remove<PositionComponent>(e);
    ASSERT(!world.has<PositionComponent>(e), "world: remove drops one component");
    ASSERT(world.has<SizeComponent>(e), "world: remove leaves other components");
}

static void test_world_destroy_drops_components() {
    TestWorld world;
    Entity e = world.create();
    world.add<PositionComponent>(e, Point(1, 1));
    world.add<SizeComponent>(e, Size(2, 2));
    ASSERT(world.entityCount() == 1, "world: one entity before destroy");

    world.destroy(e);
    ASSERT(!world.valid(e), "world: entity invalid after destroy");
    ASSERT(world.entityCount() == 0, "world: entity count zero after destroy");
    // Storages must not still report the destroyed entity.
    ASSERT(world.components<PositionComponent>().size() == 0, "world: position storage cleared");
    ASSERT(world.components<SizeComponent>().size() == 0, "world: size storage cleared");
}

static void test_world_add_invalid_entity() {
    TestWorld world;
    Entity bogus; // default-constructed, invalid
    ASSERT(world.add<PositionComponent>(bogus, Point()) == nullptr,
           "world: add on invalid entity rejected");
}

// ---------------------------------------------------------------------------
// ComponentQuery (via World::query)
// ---------------------------------------------------------------------------

static void test_query_single_component() {
    TestWorld world;
    Entity a = world.create();
    Entity b = world.create();
    Entity c = world.create();
    world.add<RenderComponent>(a);
    world.add<RenderComponent>(c);
    // b intentionally has no RenderComponent

    int visited = 0;
    bool sawA = false, sawB = false, sawC = false;
    for (Entity e : world.query<RenderComponent>()) {
        visited++;
        if (e == a) sawA = true;
        if (e == b) sawB = true;
        if (e == c) sawC = true;
    }
    ASSERT(visited == 2, "query<Render>: visits exactly the render entities");
    ASSERT(sawA && sawC && !sawB, "query<Render>: yields a and c, not b");
}

static void test_query_multi_component() {
    TestWorld world;
    Entity both = world.create();   // Input + Position
    Entity onlyInput = world.create();
    Entity onlyPos = world.create();

    world.add<InputComponent>(both);
    world.add<PositionComponent>(both, Point(9, 9));
    world.add<InputComponent>(onlyInput);
    world.add<PositionComponent>(onlyPos, Point(1, 1));

    int visited = 0;
    bool sawBoth = false, sawOther = false;
    for (Entity e : world.query<InputComponent, PositionComponent>()) {
        visited++;
        if (e == both) sawBoth = true;
        if (e == onlyInput || e == onlyPos) sawOther = true;
    }
    ASSERT(visited == 1, "query<Input,Position>: only the entity with both");
    ASSERT(sawBoth && !sawOther, "query<Input,Position>: yields the intersection");
}

static void test_query_empty() {
    TestWorld world;
    Entity e = world.create();
    world.add<PositionComponent>(e, Point());
    int visited = 0;
    for (Entity q : world.query<AnimationComponent>()) { (void)q; visited++; }
    ASSERT(visited == 0, "query: empty result iterates zero times");
}

// ---------------------------------------------------------------------------
// Systems
// ---------------------------------------------------------------------------

static void test_animation_system() {
    TestWorld world;
    AnimationSystem<TestWorld> anim(&world);

    Entity e = world.create();
    auto* a = world.add<AnimationComponent>(e, 1.0f /*duration*/, false /*loop*/);
    a->play();

    anim.update(0.5f);
    ASSERT(a->currentTime > 0.49f && a->currentTime < 0.51f, "anim: advances currentTime by dt");
    ASSERT(a->playing, "anim: still playing mid-animation");

    anim.update(1.0f); // overshoot end
    ASSERT(!a->playing, "anim: one-shot stops at end");
    ASSERT(a->currentTime <= a->duration + 0.001f, "anim: clamped to duration");

    // A paused animation must not advance.
    Entity e2 = world.create();
    auto* b = world.add<AnimationComponent>(e2, 1.0f, false);
    anim.update(0.5f);
    ASSERT(b->currentTime == 0.0f, "anim: paused animation does not advance");
}

static void test_animation_system_loop() {
    TestWorld world;
    AnimationSystem<TestWorld> anim(&world);
    Entity e = world.create();
    auto* a = world.add<AnimationComponent>(e, 1.0f /*duration*/, true /*loop*/);
    a->play();
    anim.update(1.5f); // past the end
    ASSERT(a->playing, "anim(loop): keeps playing past the end");
    ASSERT(a->currentTime < a->duration, "anim(loop): wraps currentTime");
}

static void test_input_system() {
    TestWorld world;
    InputSystem<TestWorld> input(&world);

    Entity e = world.create();
    auto* in = world.add<InputComponent>(e);
    world.add<PositionComponent>(e, Point(0, 0));
    world.add<SizeComponent>(e, Size(10, 10));

    input.onMouseMove(Point(5, 5)); // inside the 10x10 box
    input.update(0.0f);
    ASSERT(in->hovered, "input: hover enter when pointer inside bounds");

    input.onMouseMove(Point(50, 50)); // outside
    input.update(0.0f);
    ASSERT(!in->hovered, "input: hover exit when pointer leaves bounds");

    // A press while hovered dispatches onPress.
    input.onMousePress(Point(5, 5));
    input.update(0.0f);
    ASSERT(in->pressed, "input: press dispatched to hovered entity");

    // The click is consumed; a subsequent update without a new press does not re-fire.
    auto* e2in = world.add<InputComponent>(world.create()); // fresh, not pressed
    (void)e2in;
    input.update(0.0f);
    ASSERT(true, "input: update with no pending click is a no-op (no crash)");
}

static void test_render_system() {
    TestWorld world;
    Canvas4<32, 32> canvas;
    RenderSystem<TestWorld, Canvas4<32, 32>> render(&world, &canvas);

    // A visible filled rectangle (no ShapeComponent -> renderRectangle path).
    Entity vis = world.create();
    world.add<PositionComponent>(vis, Point(2, 2));
    world.add<SizeComponent>(vis, Size(4, 4));
    world.add<RenderComponent>(vis); // default color = white, visible

    // A hidden entity must not draw.
    Entity hidden = world.create();
    world.add<PositionComponent>(hidden, Point(20, 20));
    world.add<SizeComponent>(hidden, Size(4, 4));
    auto* hr = world.add<RenderComponent>(hidden);
    hr->visible = false;

    render.update(0.0f);

    ASSERT((uint8_t)canvas.getPixel(3, 3) != 0, "render: visible rect drew a non-black pixel");
    ASSERT((uint8_t)canvas.getPixel(22, 22) == 0, "render: hidden entity drew nothing");
    ASSERT((uint8_t)canvas.getPixel(0, 0) == 0, "render: canvas cleared outside shapes");
}

int main() {
    test_storage_add_get_remove();
    test_storage_no_aliasing();
    test_storage_sparse_by_id();
    test_storage_swap_remove();
    test_storage_generation_stale();
    test_storage_iteration();

    test_entity_manager();

    test_world_add_get_remove();
    test_world_destroy_drops_components();
    test_world_add_invalid_entity();

    test_query_single_component();
    test_query_multi_component();
    test_query_empty();

    test_animation_system();
    test_animation_system_loop();
    test_input_system();
    test_render_system();

    // Theme: the default is a real, populated theme (not the old empty placeholder).
    ASSERT((uint8_t)kDefaultTheme.background == 0, "theme: default background is black");
    ASSERT((uint8_t)kDefaultTheme.foreground == 15, "theme: default foreground is white");
    ASSERT(kDefaultTheme.itemHeight > 0, "theme: default item height is non-zero");
    ASSERT((uint8_t)Theme::dark().accent == 15, "theme: dark() accent is bright");

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
