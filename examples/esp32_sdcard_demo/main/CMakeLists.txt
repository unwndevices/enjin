idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
        "../../../include"
    REQUIRES
        esp_system
        freertos
        nvs_flash
        fatfs
        sdmmc
        driver
        esp_timer
)

target_link_libraries(${COMPONENT_LIB} PRIVATE
    enjin2_core
    enjin2_graphics
    enjin2_ui
    enjin2_lua
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ESP32
    ENJIN2_BUILD_LUA=1
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
)
