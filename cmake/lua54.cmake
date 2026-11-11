# cmake/lua54.cmake: the one Lua every host builds (Tomodachi #481).
#
# Lua 5.4.8 from source, as the static library target `lua54` (its include dir
# PUBLIC), for desktop, WebAssembly and the ESP32 alike: no build depends on
# the machine's Lua (Arch ships 5.5, which enjin does not compile against).
#
# Included by enjin's CMakeLists.txt when no `lua54` target exists yet. A parent
# project that needs Lua before (or without) enjin includes this same file
# first; enjin then reuses that target, so a build tree holds one Lua library.
#
# The FetchContent name is `lua54`: -DFETCHCONTENT_SOURCE_DIR_LUA54=<dir> hands
# CMake an unpacked lua-5.4.8 instead of downloading it
# (Tomodachi's scripts/worktree-setup.sh does).
include_guard(GLOBAL)

if(NOT TARGET lua54)
    include(FetchContent)
    FetchContent_Declare(lua54
        URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
        URL_HASH SHA256=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # The tarball has no CMakeLists.txt: MakeAvailable only unpacks it.
    FetchContent_MakeAvailable(lua54)

    file(GLOB ENJIN2_LUA54_SOURCES "${lua54_SOURCE_DIR}/src/*.c")
    # lua.c and luac.c are the standalone interpreter and compiler (each a main()).
    list(FILTER ENJIN2_LUA54_SOURCES EXCLUDE REGEX ".*/(lua|luac)\\.c$")
    add_library(lua54 STATIC ${ENJIN2_LUA54_SOURCES})
    target_include_directories(lua54 PUBLIC "${lua54_SOURCE_DIR}/src")
    message(STATUS "Lua 5.4.8 from source: ${lua54_SOURCE_DIR}")
endif()
