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
    -Wl,--start-group enjin2_core enjin2_graphics enjin2_ui enjin2_input -Wl,--end-group)
add_test(NAME named_objects_test COMMAND named_objects_test)

# Scene transition unit test (SCENE-01, SCENE-02, SCENE-03: SSM back-pointer + deferred self-transition)
add_executable(scene_transition_test scene_transition_test.cpp)
target_include_directories(scene_transition_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(scene_transition_test PRIVATE enjin2)
add_test(NAME scene_transition_test COMMAND scene_transition_test)

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
    endif()
endif()
