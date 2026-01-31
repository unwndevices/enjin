cmake_minimum_required(VERSION 3.16)

add_executable(basic_drawing_demo main.cpp)
target_link_libraries(basic_drawing_demo PRIVATE enjin2)
target_include_directories(basic_drawing_demo PRIVATE ${CMAKE_SOURCE_DIR}/include)