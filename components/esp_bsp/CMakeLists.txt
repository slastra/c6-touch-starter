# Only include BSP files we actually use for display and touch
set(SRC_FILES 
    "bsp_display.c"
    "bsp_touch.c" 
    "bsp_i2c.c"
    "bsp_spi.c"
    "bsp_qmi8658.c"
    "bsp_battery.c"
)

idf_component_register(SRCS ${SRC_FILES}
                    INCLUDE_DIRS "."
                    REQUIRES "esp_lcd" "driver" "esp_lcd_jd9853" "esp_lcd_touch_axs5106" "nvs_flash" "esp_adc")
