# ESP-IDF CMakeLists.txt for Enjin2 with Lua support
# This shows how to integrate Enjin2 Lua scripting in an ESP-IDF project

cmake_minimum_required(VERSION 3.16)

# Set project name
set(PROJECT_NAME "enjin2_esp32_lua")

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# ESP32 platform definition
set(ESP32 ON)

# Lua configuration for ESP32
set(ENJIN2_BUILD_LUA ON)

# Lua library paths (adjust based on your Lua component)
# Option 1: ESP-IDF Lua component
set(LUA_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components/lua/include")
set(LUA_LIBRARIES "lua")

# Option 2: NodeMCU Lua (alternative)
# set(LUA_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components/nodemcu-lua/include")
# set(LUA_LIBRARIES "nodemcu_lua")

# Add Enjin2 as subdirectory
add_subdirectory(../../ enjin2)

# Define the project
project(${PROJECT_NAME})

# Main component
idf_component_register(
    SRCS 
        "main/main.cpp"
        "../esp32_lua_integration.cpp"
    INCLUDE_DIRS 
        "main"
        "../../include"
    REQUIRES 
        spiffs
        esp_system
        freertos
        lua  # Your Lua component name
)

# Link Enjin2 libraries
target_link_libraries(${COMPONENT_LIB} PRIVATE
    enjin2_core
    enjin2_graphics
    enjin2_ui
    enjin2_lua
)

# ESP32-specific compile definitions
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ESP32
    CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ=240
    ENJIN2_BUILD_LUA=1
)

# Optimization flags for embedded
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os                    # Size optimization
    -ffunction-sections    # Function sections for linker optimization
    -fdata-sections       # Data sections for linker optimization
)