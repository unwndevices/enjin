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
target_link_libraries(named_objects_test PRIVATE enjin2)
add_test(NAME named_objects_test COMMAND named_objects_test)

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
