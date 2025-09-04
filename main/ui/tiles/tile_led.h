#pragma once

#include "tile_interface.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t* tile_led_create(lv_obj_t *parent);

esp_err_t tile_led_init(lv_obj_t *tile);

bool tile_led_is_enabled(void);

#ifdef __cplusplus
}
#endif