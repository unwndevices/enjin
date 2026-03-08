cmake_minimum_required(VERSION 3.16)

# Input abstraction unit test
add_executable(input_test
    input_test.cpp
)
target_include_directories(input_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(input_test PRIVATE
    enjin2
)
add_test(NAME input_test COMMAND input_test)

# Palette unit test
add_executable(palette_test
    palette_test.cpp
)
target_include_directories(palette_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(palette_test PRIVATE
    enjin2
)
add_test(NAME palette_test COMMAND palette_test)

# Image comparison test for shadow mode verification
add_executable(image_comparison_test
    image_comparison.cpp
)
target_include_directories(image_comparison_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor
)
target_link_libraries(image_comparison_test PRIVATE
    enjin2
)

# Shadow mode test for parallel execution of both backends
add_executable(shadow_mode_test
    shadow_mode_test.cpp
)
target_include_directories(shadow_mode_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(shadow_mode_test PRIVATE
    enjin2
)

# Sprite system unit test (SpriteSheet + C_Sprite animation)
add_executable(sprite_test
    sprite_test.cpp
)
target_include_directories(sprite_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(sprite_test PRIVATE
    enjin2
)
add_test(NAME sprite_test COMMAND sprite_test)

# Compositor unit test (LAYER-01, LAYER-02, LAYER-03, LAYER-04)
add_executable(compositor_test
    compositor_test.cpp
)
target_include_directories(compositor_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(compositor_test PRIVATE
    enjin2
)
add_test(NAME compositor_test COMMAND compositor_test)

# Named objects unit test (OBJ-01 through OBJ-04: name/tag identity on Object)
add_executable(named_objects_test named_objects_test.cpp)
target_include_directories(named_objects_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(named_objects_test PRIVATE
    enjin2_core enjin2_graphics enjin2_ui enjin2_input)
add_test(NAME named_objects_test COMMAND named_objects_test)

# Drawable decoupling test (Phase 36: getComponents<C_Drawable>() regression)
add_executable(drawable_decoupling_test drawable_decoupling_test.cpp)
target_include_directories(drawable_decoupling_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(drawable_decoupling_test PRIVATE
    -Wl,--start-group enjin2_core enjin2_graphics enjin2_ui enjin2_input -Wl,--end-group)
add_test(NAME drawable_decoupling_test COMMAND drawable_decoupling_test)

# Scene onRender Pixel4 dispatch test (RENDER-01)
add_executable(scene_render_test
    scene_render_test.cpp
)
target_include_directories(scene_render_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(scene_render_test PRIVATE
    enjin2
)
add_test(NAME scene_render_test COMMAND scene_render_test)

# Scene transition unit test (SCENE-01, SCENE-02, SCENE-03: SSM back-pointer + deferred self-transition)
add_executable(scene_transition_test scene_transition_test.cpp)
target_include_directories(scene_transition_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(scene_transition_test PRIVATE enjin2)
add_test(NAME scene_transition_test COMMAND scene_transition_test)

# Persistent object unit test (PERSIST-01..PERSIST-10: registry, extraction, injection, transition lifecycle)
add_executable(persistent_object_test persistent_object_test.cpp)
target_include_directories(persistent_object_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(persistent_object_test PRIVATE enjin2)
add_test(NAME persistent_object_test COMMAND persistent_object_test)

# Frame timing instrumentation unit test (FRAME-01, FRAME-02, FRAME-03)
add_executable(frame_timing_test frame_timing_test.cpp)
target_include_directories(frame_timing_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(frame_timing_test PRIVATE enjin2_core)
target_compile_definitions(frame_timing_test PRIVATE ENJIN2_FRAME_TIMING=1)
add_test(NAME frame_timing_test COMMAND frame_timing_test)

# Layer binding unit test (Lua layer API: setLayer, getLayer, clearLayer, etc.)
if(ENJIN2_BUILD_LUA)
    add_executable(layer_binding_test
        layer_binding_test.cpp
    )
    target_include_directories(layer_binding_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(layer_binding_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME layer_binding_test COMMAND layer_binding_test)

    # Hot-reload state management test (resetSpritePool, registerAll reset, reload cycle)
    add_executable(hot_reload_test
        hot_reload_test.cpp
    )
    target_include_directories(hot_reload_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(hot_reload_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME hot_reload_test COMMAND hot_reload_test)

    # engine.* global table test (Phase 31: ENG-01..ENG-06)
    add_executable(engine_table_test
        engine_table_test.cpp
    )
    target_include_directories(engine_table_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(engine_table_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME engine_table_test COMMAND engine_table_test)

    # Text binding test (text(), textWrapped(), measurement, font switching)
    add_executable(text_binding_test
        text_binding_test.cpp
    )
    target_include_directories(text_binding_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(text_binding_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME text_binding_test COMMAND text_binding_test)

    # Math binding test (Vec2, Point, Rect userdata + clamp, lerp, remap, sign, smoothstep, distance)
    add_executable(math_binding_test
        math_binding_test.cpp
    )
    target_include_directories(math_binding_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(math_binding_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME math_binding_test COMMAND math_binding_test)

    # Collision binding test (engine.collision.* C++ + Lua)
    add_executable(collision_test
        collision_test.cpp
    )
    target_include_directories(collision_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(collision_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME collision_test COMMAND collision_test)

    # Error policy test (ERR-01 through ERR-05: ScriptErrorPolicy on C_LuaScript)
    add_executable(error_policy_test
        error_policy_test.cpp
    )
    target_include_directories(error_policy_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(error_policy_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME error_policy_test COMMAND error_policy_test)

    # Input event callback test (INPUT-01, INPUT-02, INPUT-03: on_button_pressed/released edge callbacks)
    add_executable(input_event_callback_test
        input_event_callback_test.cpp
    )
    target_include_directories(input_event_callback_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(input_event_callback_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME input_event_callback_test COMMAND input_event_callback_test)

    # GC control + component assertion test (GC-01, GC-02, DEP-01, DEP-02, DEP-03)
    add_executable(gc_assert_test
        gc_assert_test.cpp
    )
    target_include_directories(gc_assert_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(gc_assert_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME gc_assert_test COMMAND gc_assert_test)

    # Script proxy lifetime test (PROXY-STALE: stale proxy error; TAG-01..TAG-03: tag bindings)
    add_executable(script_proxy_lifetime_test
        script_proxy_lifetime_test.cpp
    )
    target_include_directories(script_proxy_lifetime_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(script_proxy_lifetime_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME script_proxy_lifetime_test COMMAND script_proxy_lifetime_test)

    # Object proxy test (Phase 37: ObjectProxy safety + stale invalidation + enable/disable)
    add_executable(object_proxy_test
        object_proxy_test.cpp
    )
    target_include_directories(object_proxy_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(object_proxy_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME object_proxy_test COMMAND object_proxy_test)

    # Component proxy test (Phase 39: PROXY-01..PROXY-04)
    add_executable(component_proxy_test
        component_proxy_test.cpp
    )
    target_include_directories(component_proxy_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(component_proxy_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME component_proxy_test COMMAND component_proxy_test)

    # Sprite flipping test (drawSprite flipH/flipV/rotate90)
    add_executable(sprite_flip_test
        sprite_flip_test.cpp
    )
    target_include_directories(sprite_flip_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(sprite_flip_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME sprite_flip_test COMMAND sprite_flip_test)

    # Collision response test (aabbOverlap, circleResponse, reflect — C++ and Lua)
    add_executable(collision_response_test
        collision_response_test.cpp
    )
    target_include_directories(collision_response_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(collision_response_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME collision_response_test COMMAND collision_response_test)

    # Seeded RNG test (engine.random.seed / integer / float determinism)
    add_executable(rng_test
        rng_test.cpp
    )
    target_include_directories(rng_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(rng_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME rng_test COMMAND rng_test)

    # Persistent store test (engine.store.save / load / exists / delete / clear)
    add_executable(store_test
        store_test.cpp
    )
    target_include_directories(store_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(store_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME store_test COMMAND store_test)

    # Timer component test (Phase 40: TIMER-01..TIMER-05)
    add_executable(timer_test
        timer_test.cpp
    )
    target_include_directories(timer_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(timer_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME timer_test COMMAND timer_test)

    # State machine component test (Phase 41: FSM-01..FSM-05)
    add_executable(state_machine_test
        state_machine_test.cpp
    )
    target_include_directories(state_machine_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(state_machine_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME state_machine_test COMMAND state_machine_test)

    # Overflow boundary test (Phase 46: TEST-02)
    add_executable(overflow_test
        overflow_test.cpp
    )
    target_include_directories(overflow_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(overflow_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME overflow_test COMMAND overflow_test)

    # Sprite asset loading / .njn Binary Test
    # Only build sprite_load_test when GTest is available (guards against missing GTest on CI)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_executable(sprite_load_test
            sprite_load_test.cpp
        )
        target_include_directories(sprite_load_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
        )
        target_link_libraries(sprite_load_test PRIVATE
            enjin2_lua
            enjin2_core
            enjin2_graphics
            GTest::gtest_main
        )
        add_test(NAME sprite_load_test COMMAND sprite_load_test)
    endif()

    # Copy test .njn file to build directory for sprite_load_test to find
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/test_pikachu.njn" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests")

    # EventBus test (Phase 42: EVENT-01..EVENT-05)
    add_executable(eventbus_test
        eventbus_test.cpp
    )
    target_include_directories(eventbus_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(eventbus_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME eventbus_test COMMAND eventbus_test)

    # Tilemap component test (Phase 43: TMAP-01..TMAP-04)
    add_executable(tilemap_test
        tilemap_test.cpp
    )
    target_include_directories(tilemap_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(tilemap_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME tilemap_test COMMAND tilemap_test)

    # Tilemap Lua binding test (Phase 43: TMAP-05..TMAP-08)
    add_executable(tilemap_lua_test
        tilemap_lua_test.cpp
    )
    target_include_directories(tilemap_lua_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(tilemap_lua_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME tilemap_lua_test COMMAND tilemap_lua_test)

    # Camera component test (Phase 44: CAM-01..CAM-06)
    add_executable(camera_test
        camera_test.cpp
    )
    target_include_directories(camera_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(camera_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME camera_test COMMAND camera_test)

    # Camera Lua binding test (Phase 44: CAM-07, CAM-08, CAM-09)
    add_executable(camera_lua_test
        camera_lua_test.cpp
    )
    target_include_directories(camera_lua_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(camera_lua_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME camera_lua_test COMMAND camera_lua_test)

    # Scripting API improvements test (quick-007: API-01..API-07)
    add_executable(scripting_api_test
        scripting_api_test.cpp
    )
    target_include_directories(scripting_api_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(scripting_api_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME scripting_api_test COMMAND scripting_api_test)

    # Physics helper test (Phase 45: PHYS-01..PHYS-08)
    add_executable(physics_test
        physics_test.cpp
    )
    target_include_directories(physics_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(physics_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME physics_test COMMAND physics_test)

    # Physics Lua binding test (Phase 45: PHYS-09..PHYS-13)
    add_executable(physics_lua_test
        physics_lua_test.cpp
    )
    target_include_directories(physics_lua_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(physics_lua_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME physics_lua_test COMMAND physics_lua_test)

    # Debug draw binding test (Phase 47: DEBUG-01..DEBUG-03)
    add_executable(debug_draw_test
        debug_draw_test.cpp
    )
    target_include_directories(debug_draw_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(debug_draw_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME debug_draw_test COMMAND debug_draw_test)

    # Camera follow binding test (Phase 48: CAM-01, CAM-02)
    add_executable(camera_follow_test
        camera_follow_test.cpp
    )
    target_include_directories(camera_follow_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(camera_follow_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME camera_follow_test COMMAND camera_follow_test)

    # Coroutine async scheduler test (Phase 49: ASYNC-01..ASYNC-03)
    add_executable(coroutine_async_test
        coroutine_async_test.cpp
    )
    target_include_directories(coroutine_async_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(coroutine_async_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME coroutine_async_test COMMAND coroutine_async_test)

    # Tween helpers test (Phase 50: TWEEN-01..TWEEN-03)
    add_executable(tween_test
        tween_test.cpp
    )
    target_include_directories(tween_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(tween_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME tween_test COMMAND tween_test)

    # QoL features test (Phase 57: QOL-01, QOL-02, QOL-03)
    add_executable(qol_test
        qol_test.cpp
    )
    target_include_directories(qol_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(qol_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME qol_test COMMAND qol_test)

    # Persistent Lua binding test (Phase 51: PERSIST-01..PERSIST-03)
    add_executable(persistent_lua_test
        persistent_lua_test.cpp
    )
    target_include_directories(persistent_lua_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(persistent_lua_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME persistent_lua_test COMMAND persistent_lua_test)

    # UI component binding test (Phase 52: UI-01..UI-04)
    add_executable(ui_binding_test
        ui_binding_test.cpp
    )
    target_include_directories(ui_binding_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(ui_binding_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME ui_binding_test COMMAND ui_binding_test)

    # Lua profiler test (Phase 63: PROF-01, PROF-02, PROF-03, PROF-06)
    add_executable(lua_profiler_test
        lua_profiler_test.cpp
    )
    target_include_directories(lua_profiler_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(lua_profiler_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME lua_profiler_test COMMAND lua_profiler_test)
endif()

# SDL3 sprite visual test (pikachu with palette colors)
if(ENJIN2_BUILD_SDL)
    add_executable(sprite_sdl_test
        sprite_sdl_test.cpp
    )
    target_include_directories(sprite_sdl_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(sprite_sdl_test PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_input
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
        SDL3::SDL3
    )
    target_compile_definitions(sprite_sdl_test PRIVATE
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA>
    )

    # Copy Lua demo script to build directory
    if(ENJIN2_BUILD_LUA)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/scripts)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/pikachu_demo.lua
            ${CMAKE_CURRENT_BINARY_DIR}/scripts/pikachu_demo.lua
            COPYONLY
        )
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/features_demo.lua
            ${CMAKE_CURRENT_BINARY_DIR}/scripts/features_demo.lua
            COPYONLY
        )
    endif()
endif()
