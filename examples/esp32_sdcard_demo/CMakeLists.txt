cmake_minimum_required(VERSION 3.16)

# Configure enjin2 build options for ESP32 before adding the subdirectory.
set(ESP32 ON)
set(ENJIN2_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(ENJIN2_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENJIN2_BUILD_WASM     OFF CACHE BOOL "" FORCE)
set(ENJIN2_BUILD_SDL      OFF CACHE BOOL "" FORCE)
set(ENJIN2_BUILD_LUA      ON)

# ESP-IDF project bootstrap (must precede project())
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(enjin2_esp32_sdcard)

# Add enjin2 as an external CMake subproject after project() so it is compiled
# with the active xtensa-esp32s3-elf-gcc cross-compiler.
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
    ${CMAKE_CURRENT_BINARY_DIR}/enjin2_lib
)

# Propagate ESP-IDF headers to enjin2_lua.
if(TARGET enjin2_lua)
    target_link_libraries(enjin2_lua PRIVATE
        idf::esp_system
        idf::heap
        idf::vfs
        idf::spiffs
        idf::newlib
        idf::nvs_flash
        idf::fatfs
        idf::sdmmc
    )
endif()

# lua54_esp32 uses longjmp which ESP-IDF wraps via FreeRTOS (__wrap_longjmp).
if(TARGET lua54_esp32)
    target_link_libraries(lua54_esp32 PRIVATE idf::freertos)
endif()
