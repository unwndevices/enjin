idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
        "../../../include"
    REQUIRES
        esp_system
        freertos
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
    CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ=240
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
)
