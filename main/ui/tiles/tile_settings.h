#pragma once

#include "tile_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the settings tile with status table and brightness control
 * @param parent The parent tileview object
 * @return Pointer to the created tile object
 */
lv_obj_t* tile_settings_create(lv_obj_t *parent);

/**
 * @brief Initialize settings tile resources (battery and sensor timers)
 * @param tile The tile object
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t tile_settings_init(lv_obj_t *tile);

/**
 * @brief Get the status table widget for external updates
 * @return Pointer to the status table widget
 */
lv_obj_t* tile_settings_get_status_table(void);

/**
 * @brief Get the brightness slider widget for external updates
 * @return Pointer to the brightness slider widget
 */
lv_obj_t* tile_settings_get_brightness_slider(void);

/**
 * @brief Get the brightness label widget for external updates
 * @return Pointer to the brightness label widget
 */
lv_obj_t* tile_settings_get_brightness_label(void);

/**
 * @brief Get the battery timer for power management control
 * @return Pointer to the battery timer
 */
lv_timer_t* tile_settings_get_battery_timer(void);

#ifdef __cplusplus
}
#endif