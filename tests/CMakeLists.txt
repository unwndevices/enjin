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
