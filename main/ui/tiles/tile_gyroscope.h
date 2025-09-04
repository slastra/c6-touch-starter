#pragma once

#include "tile_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the gyroscope tile with live chart
 * @param parent The parent tileview object
 * @return Pointer to the created tile object
 */
lv_obj_t* tile_gyroscope_create(lv_obj_t *parent);

/**
 * @brief Initialize gyroscope tile resources
 * @param tile The tile object
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t tile_gyroscope_init(lv_obj_t *tile);

/**
 * @brief Get the gyroscope chart widget
 * @return Pointer to the chart widget
 */
lv_obj_t* tile_gyroscope_get_chart(void);

/**
 * @brief Get the gyroscope chart series for external updates
 * @param series_x Pointer to store X-axis series
 * @param series_y Pointer to store Y-axis series  
 * @param series_z Pointer to store Z-axis series
 */
void tile_gyroscope_get_series(lv_chart_series_t **series_x, 
                               lv_chart_series_t **series_y,
                               lv_chart_series_t **series_z);

#ifdef __cplusplus
}
#endif