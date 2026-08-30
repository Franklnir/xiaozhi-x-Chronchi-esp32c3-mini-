idf_component_register(
    SRCS "recovery_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES app_update bt esp_app_format esp_driver_gpio mbedtls nvs_flash
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    RECOVERY_BOARD_NAME="esp32c3-inmp441-production"
)
