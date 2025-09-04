#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LED_GPIO_NUM 4
#define LED_STRIP_LED_COUNT 1

esp_err_t led_manager_init(void);

esp_err_t led_manager_set_rgb(uint8_t red, uint8_t green, uint8_t blue);

esp_err_t led_manager_get_rgb(uint8_t *red, uint8_t *green, uint8_t *blue);

esp_err_t led_manager_set_brightness_scale(uint8_t percentage);

esp_err_t led_manager_clear(void);

void led_manager_deinit(void);

#ifdef __cplusplus
}
#endif