cmake_minimum_required(VERSION 3.16)

add_subdirectory(basic_drawing)
add_subdirectory(ecs_demo)
add_subdirectory(lua_scripting)

# Text rendering demo
add_executable(text_demo text_demo.cpp)
target_link_libraries(text_demo PRIVATE enjin2)
target_include_directories(text_demo PRIVATE ${CMAKE_SOURCE_DIR}/../Libs/Adafruit-GFX-Library)

# Enjin vs Enjin2 comparison benchmark
add_executable(enjin_comparison_benchmark enjin_comparison_benchmark.cpp)
target_link_libraries(enjin_comparison_benchmark PRIVATE enjin2)
target_include_directories(enjin_comparison_benchmark PRIVATE 
    ${CMAKE_SOURCE_DIR}/../Libs/Adafruit-GFX-Library
    ${CMAKE_SOURCE_DIR}/../Libs/enjin)
target_compile_definitions(enjin_comparison_benchmark PRIVATE VCV_RACK)

# Eisei Game benchmark (Old Enjin vs Enjin2)
add_executable(eisei_game_benchmark eisei_game_benchmark.cpp)
target_link_libraries(eisei_game_benchmark PRIVATE enjin2)
target_include_directories(eisei_game_benchmark PRIVATE 
    ${CMAKE_SOURCE_DIR}/../Libs/Adafruit-GFX-Library
    ${CMAKE_SOURCE_DIR}/../Libs/enjin
    ${CMAKE_SOURCE_DIR}/../eisei
    ${CMAKE_SOURCE_DIR}/../Libs/unwnlib)
target_compile_definitions(eisei_game_benchmark PRIVATE VCV_RACK)

# Performance optimization analysis
add_executable(performance_optimization_analysis performance_optimization_analysis.cpp)
target_link_libraries(performance_optimization_analysis PRIVATE enjin2)
target_compile_definitions(performance_optimization_analysis PRIVATE VCV_RACK)
target_compile_options(performance_optimization_analysis PRIVATE -O3)

# Optimization test
add_executable(optimization_test optimization_test.cpp)
target_link_libraries(optimization_test PRIVATE enjin2)
target_compile_definitions(optimization_test PRIVATE VCV_RACK)
target_compile_options(optimization_test PRIVATE -O3)

# Packing overhead benchmark
add_executable(packing_overhead_benchmark packing_overhead_benchmark.cpp)
target_link_libraries(packing_overhead_benchmark PRIVATE enjin2)
target_compile_definitions(packing_overhead_benchmark PRIVATE VCV_RACK)
target_compile_options(packing_overhead_benchmark PRIVATE -O3)

# Real Adafruit vs Enjin2 benchmark
add_executable(real_adafruit_benchmark real_adafruit_benchmark.cpp)
target_link_libraries(real_adafruit_benchmark PRIVATE enjin2)
target_include_directories(real_adafruit_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/../Libs/Adafruit-GFX-Library)
target_compile_definitions(real_adafruit_benchmark PRIVATE VCV_RACK)
target_compile_options(real_adafruit_benchmark PRIVATE -O3)

# Updated comparison benchmark
add_executable(updated_comparison_benchmark updated_comparison_benchmark.cpp)
target_link_libraries(updated_comparison_benchmark PRIVATE enjin2)
target_compile_definitions(updated_comparison_benchmark PRIVATE VCV_RACK)
target_compile_options(updated_comparison_benchmark PRIVATE -O3)

# Standalone benchmark (rebuilt with optimizations)
add_executable(standalone_benchmark_optimized standalone_benchmark.cpp)
target_link_libraries(standalone_benchmark_optimized PRIVATE enjin2)
target_include_directories(standalone_benchmark_optimized PRIVATE ${CMAKE_SOURCE_DIR}/../Libs/Adafruit-GFX-Library)
target_compile_definitions(standalone_benchmark_optimized PRIVATE VCV_RACK)
target_compile_options(standalone_benchmark_optimized PRIVATE -O3)