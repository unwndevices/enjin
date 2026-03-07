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
