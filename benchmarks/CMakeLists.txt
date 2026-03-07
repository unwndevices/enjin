cmake_minimum_required(VERSION 3.16)

# INTERFACE library exposing the vendored nanobench single header
add_library(nanobench_vendor INTERFACE)
target_include_directories(nanobench_vendor INTERFACE
    ${CMAKE_SOURCE_DIR}/vendor
)

# Smoke benchmark — verifies the nanobench header compiles and links against engine libs
add_executable(bench_smoke
    bench_smoke.cpp
)
target_include_directories(bench_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(bench_smoke PRIVATE
    nanobench_vendor
    enjin2_core
    enjin2_graphics
)

# bench_canvas — Canvas4/Canvas8/LayerCompositor pixel op benchmarks (BENCH-02)
add_executable(bench_canvas bench_canvas.cpp)
target_include_directories(bench_canvas PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bench_canvas PRIVATE
    nanobench_vendor
    enjin2_core
    enjin2_graphics
)

# bench_ecs — Object/Scene/Component ECS benchmarks (BENCH-03)
add_executable(bench_ecs bench_ecs.cpp)
target_include_directories(bench_ecs PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bench_ecs PRIVATE
    nanobench_vendor
    enjin2_core
    enjin2_graphics
    enjin2_ui
)

# bench_lua — LuaEngine headless benchmarks (BENCH-04)
# Only available when Lua is enabled (enjin2_lua target must exist)
# enjin2_lua transitively carries LUA_LIBRARIES — no explicit Lua link needed
if(TARGET enjin2_lua)
    add_executable(bench_lua bench_lua.cpp)
    target_include_directories(bench_lua PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${LUA_INCLUDE_DIRS}
    )
    target_link_libraries(bench_lua PRIVATE
        nanobench_vendor
        enjin2_lua
        enjin2_core
        enjin2_graphics
        enjin2_ui
    )
endif()
