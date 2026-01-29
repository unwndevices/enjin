cmake_minimum_required(VERSION 3.16)

add_executable(ecs_demo main.cpp)
target_link_libraries(ecs_demo PRIVATE enjin2)

add_executable(simple_demo simple_demo.cpp)
target_link_libraries(simple_demo PRIVATE enjin2)

add_executable(ecs_phase4_demo ../ecs_demo.cpp)
target_link_libraries(ecs_phase4_demo PRIVATE enjin2)

add_executable(space_ui_demo ../space_ui_demo.cpp)
target_link_libraries(space_ui_demo PRIVATE enjin2)