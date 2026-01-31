cmake_minimum_required(VERSION 3.16)

# DISABLED: Old ECS architecture demo (uses EntityManager which doesn't exist in current enjin2)
# The current architecture uses Object/ObjectCollection instead of entity-component-system
# add_executable(ecs_demo main.cpp)
# target_link_libraries(ecs_demo PRIVATE enjin2)

# DISABLED: Simple demo - needs updating for current API
# add_executable(simple_demo simple_demo.cpp)
# target_link_libraries(simple_demo PRIVATE enjin2)

# Updated ECS demo using current architecture (Object-based)
add_executable(ecs_phase4_demo ../ecs_demo.cpp)
target_link_libraries(ecs_phase4_demo PRIVATE enjin2)
target_include_directories(ecs_phase4_demo PRIVATE ${CMAKE_SOURCE_DIR}/include)

# DISABLED: Space UI demo - needs updating for current API
# add_executable(space_ui_demo ../space_ui_demo.cpp)
# target_link_libraries(space_ui_demo PRIVATE enjin2)