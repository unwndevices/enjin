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
        spiffs
        esp_timer
        esp_lcd
        driver
)

# Embed the spiffs_data/ directory into the "storage" SPIFFS partition.
# The image is auto-flashed with `idf.py flash`.
spiffs_create_partition_image(storage ../spiffs_data FLASH_IN_PROJECT)

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
