idf_component_register(
    SRCS
        "main.c"
        "display.c"
        "dashboard.c"
        "dashboard_classic.c"
        "dashboard_racepak.c"
        "vems.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_timer esp_driver_uart esp_driver_gpio esp_driver_i2c
)
