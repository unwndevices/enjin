cmake_minimum_required(VERSION 3.16)

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
