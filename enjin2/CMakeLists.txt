# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(enjin2 VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler dependency tracking
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -MMD -MP")

# Options
option(ENJIN2_BUILD_TESTS "Build tests" ON)
option(ENJIN2_BUILD_EXAMPLES "Build examples" ON)
option(ENJIN2_BUILD_LUA "Build Lua bindings" ON)
option(ENJIN2_BUILD_WASM "Build for WebAssembly" OFF)
option(ENJIN2_USE_SIMD "Use SIMD optimizations" ON)

# Core library
add_library(enjin2_core STATIC)
target_sources(enjin2_core PRIVATE
    src/core/memory.cpp
    src/core/math.cpp
    src/core/types.cpp
    src/core/object.cpp
    src/core/scene.cpp
    src/animation/keyframe.cpp
)
# Private include directories for strict isolation
target_include_directories(enjin2_core
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_definitions(enjin2_core PUBLIC VCV_RACK)

# Graphics library
add_library(enjin2_graphics STATIC)
target_sources(enjin2_graphics PRIVATE
    src/graphics/canvas.cpp
    src/graphics/primitives.cpp
    src/graphics/effects.cpp
)
# Private include directories for strict isolation
target_include_directories(enjin2_graphics
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_definitions(enjin2_graphics PUBLIC VCV_RACK)
target_link_libraries(enjin2_graphics PRIVATE enjin2_core)

# UI library
add_library(enjin2_ui STATIC)
target_sources(enjin2_ui PRIVATE
    src/components/drawable.cpp
    src/utils/polar.cpp
)
# Private include directories for strict isolation
target_include_directories(enjin2_ui
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${CMAKE_SOURCE_DIR}/../Libs/Adafruit-GFX-Library
)
target_compile_definitions(enjin2_ui PUBLIC VCV_RACK)
target_link_libraries(enjin2_ui PRIVATE enjin2_graphics)

# Lua bindings
if(ENJIN2_BUILD_LUA)
    if(EMSCRIPTEN)
        # Build LuaJIT from source for WebAssembly
        message(STATUS "Building LuaJIT from source for WebAssembly")
        
        # Use LuaJIT amalgamated build for optimal performance
        set(LUAJIT_CORE_SOURCES
            luajit/src/ljamalg.c
        )
        
        # Create embedded LuaJIT library
        add_library(luajit_embedded STATIC ${LUAJIT_CORE_SOURCES})
        target_include_directories(luajit_embedded PUBLIC luajit/src)
        
        # LuaJIT-specific compile definitions for WebAssembly
        target_compile_definitions(luajit_embedded PRIVATE
            LUAJIT_DISABLE_FFI  # Disable FFI for simpler WebAssembly build
            LUAJIT_DISABLE_JIT  # Disable JIT compilation for WebAssembly
            LUA_USE_APICHECK    # Enable API checking for debugging
        )
        
        set(LUA_LIBRARIES luajit_embedded)
        set(LUA_INCLUDE_DIRS luajit/src)
    elseif(ESP32)
        # ESP32 platform - use NodeMCU Lua or ESP-IDF Lua component
        message(STATUS "Building with ESP32 Lua support")
        
        # ESP32 users should provide LUA_INCLUDE_DIRS and LUA_LIBRARIES
        # Example for NodeMCU lua:
        # set(LUA_INCLUDE_DIRS "${IDF_PATH}/components/lua/include")
        # set(LUA_LIBRARIES lua)
        
        if(NOT DEFINED LUA_INCLUDE_DIRS OR NOT DEFINED LUA_LIBRARIES)
            message(WARNING "ESP32 build requires LUA_INCLUDE_DIRS and LUA_LIBRARIES to be set")
            message(STATUS "Example: cmake -DLUA_INCLUDE_DIRS=/path/to/lua/include -DLUA_LIBRARIES=lua")
        endif()
    else()
        # Desktop platforms - use system Lua
        find_package(Lua REQUIRED)
    endif()
    
    add_library(enjin2_lua STATIC)
    target_sources(enjin2_lua PRIVATE
        src/scripting/lua_engine.cpp
        src/scripting/lua_platform.cpp
        src/scripting/bindings.cpp
    )
    # Private include directories for strict isolation
    target_include_directories(enjin2_lua
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            ${LUA_INCLUDE_DIRS}
    )
    target_link_libraries(enjin2_lua PRIVATE
        enjin2_graphics
        enjin2_ui
        ${LUA_LIBRARIES}
    )
    
    # Platform-specific definitions
    if(ESP32)
        target_compile_definitions(enjin2_lua PRIVATE ESP32)
    endif()
endif()

# Main library
add_library(enjin2 INTERFACE)

# Backend compile definitions
if(USE_ENJIN1)
    target_compile_definitions(enjin2 INTERFACE USE_ENJIN1_BACKEND=1)
else()
    target_compile_definitions(enjin2 INTERFACE USE_ENJIN1_BACKEND=0)
endif()

target_link_libraries(enjin2 INTERFACE
    enjin2_core
    enjin2_graphics
    enjin2_ui
    $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
)

# Tests
if(ENJIN2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# WebAssembly build
if(ENJIN2_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "WebAssembly build requires Emscripten toolchain")
    endif()
    
    # WebAssembly-specific library with Emscripten bindings
    add_executable(enjin2_wasm)
    target_sources(enjin2_wasm PRIVATE
        src/bindings/emscripten_bindings.cpp
    )
    target_include_directories(enjin2_wasm PRIVATE luajit/src)
    target_link_libraries(enjin2_wasm PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
        enjin2_lua
    )
    
    # Emscripten-specific settings
    target_compile_options(enjin2_wasm PRIVATE
        -sIMPORTED_MEMORY=1
        -sALLOW_MEMORY_GROWTH=1
        -sMAXIMUM_MEMORY=67108864  # 64MB
        -sSTACK_SIZE=1048576       # 1MB stack
        -sASSERTIONS=1
        -sEXPORT_ES6=1
        -sMODULARIZE=1
        -sEXPORT_NAME=Enjin2Module
    )
    
    target_link_options(enjin2_wasm PRIVATE
        --bind
        -sIMPORTED_MEMORY=1
        -sALLOW_MEMORY_GROWTH=1
        -sMAXIMUM_MEMORY=67108864
        -sSTACK_SIZE=1048576
        -sASSERTIONS=1
        -sEXPORT_ES6=1
        -sMODULARIZE=1
        -sEXPORT_NAME=Enjin2Module
        -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']
        --closure=0
        --pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/pre.js
    )
    
    # Output files
    set_target_properties(enjin2_wasm PROPERTIES
        OUTPUT_NAME "enjin2"
        SUFFIX ".js"
    )
endif()

# Examples
if(ENJIN2_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()