# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(enjin2 VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler dependency tracking
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -MMD -MP")

# Documentation generation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_SOURCE_DIR}/docs/xml)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-api-docs.js
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen and Docusaurus"
        BYPRODUCTS ${DOXYGEN_OUT}/index.xml
    )

    message(STATUS "Doxygen found - documentation target available: cmake --build . --target docs")
else()
    message(WARNING "Doxygen not found - documentation generation unavailable")
endif()

# Options
option(ENJIN2_BUILD_TESTS "Build tests" ON)
option(ENJIN2_BUILD_EXAMPLES "Build examples" ON)
option(ENJIN2_BUILD_LUA "Build Lua bindings" ON)
option(ENJIN2_BUILD_WASM "Build for WebAssembly" OFF)
option(ENJIN2_USE_SIMD "Use SIMD optimizations" ON)

# Configurable canvas dimensions (must be even for 4-bit packing)
set(ENJIN2_CANVAS_WIDTH 128 CACHE STRING "Canvas width in pixels (must be even)")
set(ENJIN2_CANVAS_HEIGHT 128 CACHE STRING "Canvas height in pixels")

# -- Static analysis (clang-tidy) -----------------------------------------
# Usage: cmake -B build -DCLANG_TIDY=ON && cmake --build build --target lint
option(CLANG_TIDY "Enable clang-tidy lint target (runs clang-tidy on all src/*.cpp)" OFF)
if(CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        file(GLOB_RECURSE ENJIN2_ALL_SOURCES CONFIGURE_DEPENDS
            ${CMAKE_SOURCE_DIR}/src/*.cpp)
        add_custom_target(lint
            COMMAND ${CLANG_TIDY_EXE}
                -p ${CMAKE_BINARY_DIR}
                --warnings-as-errors=*
                ${ENJIN2_ALL_SOURCES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy on enjin2 sources (warnings-as-errors=* mode)..."
        )
        message(STATUS "clang-tidy lint target enabled: cmake --build build --target lint")
        message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "CLANG_TIDY=ON but clang-tidy executable not found in PATH")
    endif()
endif()
# -------------------------------------------------------------------------

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
if(NOT ESP32)
    target_compile_definitions(enjin2_core PUBLIC VCV_RACK)
endif()

# Graphics library
add_library(enjin2_graphics STATIC)
target_sources(enjin2_graphics PRIVATE
    src/graphics/canvas.cpp
    src/graphics/primitives.cpp
    src/graphics/palette.cpp
    src/graphics/effects.cpp
)
# Private include directories for strict isolation
target_include_directories(enjin2_graphics
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
if(NOT ESP32)
    target_compile_definitions(enjin2_graphics PUBLIC VCV_RACK)
endif()
target_link_libraries(enjin2_graphics PRIVATE enjin2_core)

# UI library
add_library(enjin2_ui STATIC)
target_sources(enjin2_ui PRIVATE
    src/components/drawable.cpp
    src/utils/polar.cpp
    # UI ECS translation units (Phase 2, #120): static ID counters + EntityManager.
    src/ui/component.cpp
    src/ui/system.cpp
    src/ui/theme.cpp
)
# Private include directories for strict isolation
target_include_directories(enjin2_ui
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${CMAKE_SOURCE_DIR}/../Libs/Adafruit-GFX-Library
)
if(NOT ESP32)
    target_compile_definitions(enjin2_ui PUBLIC VCV_RACK)
endif()
target_link_libraries(enjin2_ui PRIVATE enjin2_graphics)

# Lua bindings
if(ENJIN2_BUILD_LUA)
    if(EMSCRIPTEN)
        # LuaJIT does not support WebAssembly (no WASM architecture backend).
        # Building Lua 5.4.8 from source for WebAssembly (LuaJIT is x86/ARM only)
        message(STATUS "Building Lua 5.4.8 from source for WebAssembly (LuaJIT is x86/ARM only)")
        include(FetchContent)
        FetchContent_Declare(
            lua54
            URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
        )
        FetchContent_MakeAvailable(lua54)

        file(GLOB LUA54_SOURCES "${lua54_SOURCE_DIR}/src/l*.c")
        # Exclude standalone interpreter (lua.c) and compiler (luac.c)
        list(FILTER LUA54_SOURCES EXCLUDE REGEX ".*/(lua|luac)\\.c$")

        add_library(lua54_wasm STATIC ${LUA54_SOURCES})
        target_include_directories(lua54_wasm PUBLIC ${lua54_SOURCE_DIR}/src)

        set(LUA_LIBRARIES lua54_wasm)
        set(LUA_INCLUDE_DIRS ${lua54_SOURCE_DIR}/src)
    elseif(ESP32)
        # LuaJIT requires a native CPU architecture — use standard Lua 5.4.8 for ESP32 cross-compilation.
        # Building Lua 5.4.8 from source for ESP32
        message(STATUS "Building Lua 5.4.8 from source for ESP32")
        include(FetchContent)
        FetchContent_Declare(
            lua54
            URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
        )
        FetchContent_MakeAvailable(lua54)

        file(GLOB LUA54_SOURCES "${lua54_SOURCE_DIR}/src/l*.c")
        list(FILTER LUA54_SOURCES EXCLUDE REGEX ".*/(lua|luac)\\.c$")

        add_library(lua54_esp32 STATIC ${LUA54_SOURCES})
        target_include_directories(lua54_esp32 PUBLIC ${lua54_SOURCE_DIR}/src)

        set(LUA_LIBRARIES lua54_esp32)
        set(LUA_INCLUDE_DIRS ${lua54_SOURCE_DIR}/src)
    else()
        # Desktop platforms - use system Lua
        find_package(Lua QUIET)

        # Provide clear error if Lua was requested but not found
        # FindLua sets LUA_FOUND (all-caps); Lua_FOUND is the modern alias — check both
        if(NOT LUA_FOUND AND NOT Lua_FOUND AND ENJIN2_BUILD_LUA)
            message(FATAL_ERROR
                "Lua was requested (ENJIN2_BUILD_LUA=ON) but could not be found.\n"
                "Install Lua from https://lua.org/ or set ENJIN2_BUILD_LUA=OFF to build without Lua support.\n"
                "Package managers:\n"
                "  Debian/Ubuntu: apt-get install liblua5.4-dev\n"
                "  macOS: brew install lua\n"
            )
        endif()

        # FindLua sets LUA_INCLUDE_DIR (singular); normalise to LUA_INCLUDE_DIRS used below
        set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
    endif()
    
    add_library(enjin2_lua STATIC)
    target_sources(enjin2_lua PRIVATE
        src/scripting/lua_engine.cpp
        src/scripting/lua_platform.cpp
        src/scripting/bindings.cpp
        src/scripting/bindings_proxy.cpp
        src/scripting/bindings_draw.cpp
        src/scripting/bindings_input_sprites.cpp
        src/scripting/bindings_sprite_load.cpp
        src/scripting/bindings_layers_text.cpp
        src/scripting/bindings_engine.cpp
        src/scripting/bindings_physics.cpp
        src/scripting/bindings_debug.cpp
        src/scripting/bindings_math.cpp
        src/scripting/bindings_store.cpp
        src/scripting/wasm_storage.cpp
        src/scripting/esp32_storage.cpp
        src/scripting/bindings_async.cpp
        src/scripting/bindings_tween.cpp
        src/scripting/bindings_system.cpp
        src/scripting/bindings_ui.cpp
        src/components/lua_script.cpp
        src/components/timer.cpp
        src/components/state_machine.cpp
        src/components/tilemap.cpp
        src/components/camera.cpp
        src/scripting/lua_event_bus.cpp
    )
    target_include_directories(enjin2_lua
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            ${LUA_INCLUDE_DIRS}
        INTERFACE
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

    target_compile_options(enjin2_lua PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Woverride>)
endif()

# Input abstraction library
add_library(enjin2_input STATIC)
target_sources(enjin2_input PRIVATE
    src/input/input.cpp
)
target_include_directories(enjin2_input
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Enable -Woverride on all engine targets to catch detached virtual overrides (DT-03)
# -Woverride is Clang-specific; GCC enforces override keyword mismatches as hard errors natively
foreach(tgt enjin2_core enjin2_graphics enjin2_ui enjin2_input)
    target_compile_options(${tgt} PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Woverride>)
endforeach()

# Main library
add_library(enjin2 INTERFACE)

target_link_libraries(enjin2 INTERFACE
    enjin2_core
    enjin2_graphics
    enjin2_ui
    enjin2_input
    $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
)

# Tests
if(ENJIN2_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks (desktop only — nanobench requires POSIX/Linux perf headers)
option(ENJIN2_BUILD_BENCHMARKS "Build nanobench benchmark suite (desktop only)" OFF)

if(ENJIN2_BUILD_BENCHMARKS)
    if(EMSCRIPTEN OR ESP32)
        message(FATAL_ERROR "ENJIN2_BUILD_BENCHMARKS is not supported on WASM or ESP32 targets")
    endif()
    add_subdirectory(benchmarks)
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
        # EM_JS symbols in static libraries are dropped by wasm-ld (no other
        # symbol in the object file is referenced). Compile directly into the
        # executable so the linker sees the __em_js_* metadata.
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:src/scripting/wasm_storage.cpp>
    )
    # Lua include dirs are propagated via enjin2_lua -> LUA_INCLUDE_DIRS (PUBLIC on lua54_wasm or luajit_embedded)
    target_compile_definitions(enjin2_wasm PRIVATE
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA=1>
        ENJIN2_CANVAS_WIDTH=${ENJIN2_CANVAS_WIDTH}
        ENJIN2_CANVAS_HEIGHT=${ENJIN2_CANVAS_HEIGHT}
    )
    target_link_libraries(enjin2_wasm PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
        enjin2_input
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
    )
    
    # Emscripten-specific settings (Emscripten uses Clang, so -Woverride is valid here)
    target_compile_options(enjin2_wasm PRIVATE
        -Woverride
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

# SDL3 desktop runner
option(ENJIN2_BUILD_SDL "Build SDL3 desktop runner executable (requires internet on first configure)" OFF)

if(ENJIN2_BUILD_SDL)
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.2
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(SDL3)
    # FetchContent_MakeAvailable will fail CMake configure if download fails — no fallback needed

    add_executable(enjin2_sdl
        src/platform/sdl/sdl_main.cpp
    )
    target_include_directories(enjin2_sdl PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:${LUA_INCLUDE_DIRS}>
    )
    target_link_libraries(enjin2_sdl PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_input
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
        SDL3::SDL3
    )
    target_compile_definitions(enjin2_sdl PRIVATE
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA=1>
        ENJIN2_CANVAS_WIDTH=${ENJIN2_CANVAS_WIDTH}
        ENJIN2_CANVAS_HEIGHT=${ENJIN2_CANVAS_HEIGHT}
        ENJIN2_FRAME_TIMING=1
    )
    target_compile_options(enjin2_sdl PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Woverride>)
endif()

# Headless Lua runner for profiling (PROF-04..PROF-06)
option(ENJIN2_BUILD_HEADLESS "Build headless Lua profiling runner (enjin_run)" OFF)

if(ENJIN2_BUILD_HEADLESS)
    if(EMSCRIPTEN OR ESP32)
        message(FATAL_ERROR "ENJIN2_BUILD_HEADLESS is not supported on WASM or ESP32 targets")
    endif()
    if(NOT TARGET enjin2_lua)
        message(FATAL_ERROR "ENJIN2_BUILD_HEADLESS requires ENJIN2_BUILD_LUA=ON")
    endif()
    add_executable(enjin_run
        src/platform/headless/headless_main.cpp
    )
    target_include_directories(enjin_run PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        ${LUA_INCLUDE_DIRS}
    )
    target_link_libraries(enjin_run PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
        enjin2_input
        enjin2_lua
    )
    target_compile_definitions(enjin_run PRIVATE
        ENJIN2_BUILD_LUA=1
        ENJIN2_CANVAS_WIDTH=${ENJIN2_CANVAS_WIDTH}
        ENJIN2_CANVAS_HEIGHT=${ENJIN2_CANVAS_HEIGHT}
    )
endif()