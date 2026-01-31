cmake_minimum_required(VERSION 3.16)

if(ENJIN2_BUILD_LUA)
    add_executable(lua_demo main.cpp)
    target_link_libraries(lua_demo PRIVATE enjin2)
    target_include_directories(lua_demo PRIVATE ${CMAKE_SOURCE_DIR}/include)
    
    # Copy Lua scripts to build directory
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/demo.lua 
                   ${CMAKE_CURRENT_BINARY_DIR}/demo.lua COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/animation.lua 
                   ${CMAKE_CURRENT_BINARY_DIR}/animation.lua COPYONLY)
endif()
