#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Common tile creation function signature
typedef lv_obj_t* (*tile_create_fn_t)(lv_obj_t *parent);

// Common tile interface functions that all tiles should implement
// Each tile module should provide these functions with tile_<name>_ prefix

/**
 * @brief Create a tile and all its UI elements
 * @param parent The parent tileview object
 * @return Pointer to the created tile object
 */
#define TILE_CREATE_FUNCTION(name) lv_obj_t* tile_##name##_create(lv_obj_t *parent)

/**
 * @brief Update tile with dynamic data (optional for tiles that have dynamic content)
 * @param tile The tile object to update
 */
#define TILE_UPDATE_FUNCTION(name) void tile_##name##_update(lv_obj_t *tile)

/**
 * @brief Initialize tile-specific resources (timers, etc.)
 * @param tile The tile object
 * @return ESP_OK on success, error code otherwise
 */
#define TILE_INIT_FUNCTION(name) esp_err_t tile_##name##_init(lv_obj_t *tile)

/**
 * @brief Cleanup tile-specific resources
 * @param tile The tile object
 */
#define TILE_CLEANUP_FUNCTION(name) void tile_##name##_cleanup(lv_obj_t *tile)

// Common tile utilities
/**
 * @brief Create a standard tile title label
 * @param parent Parent object for the title
 * @param text Title text
 * @return Pointer to the created label
 */
lv_obj_t* tile_create_title(lv_obj_t *parent, const char *text);

// Forward declarations for tile creation functions
lv_obj_t* tile_main_create(lv_obj_t *parent);
lv_obj_t* tile_settings_create(lv_obj_t *parent);
lv_obj_t* tile_updates_create(lv_obj_t *parent);
lv_obj_t* tile_info_create(lv_obj_t *parent);
lv_obj_t* tile_accelerometer_create(lv_obj_t *parent);
lv_obj_t* tile_gyroscope_create(lv_obj_t *parent);
lv_obj_t* tile_barcode_create(lv_obj_t *parent);
lv_obj_t* tile_led_create(lv_obj_t *parent);

// Tile initialization functions (for tiles that need timers/resources)
esp_err_t tile_settings_init(lv_obj_t *tile);
esp_err_t tile_accelerometer_init(lv_obj_t *tile);
esp_err_t tile_gyroscope_init(lv_obj_t *tile);
esp_err_t tile_barcode_init(lv_obj_t *tile);
esp_err_t tile_led_init(lv_obj_t *tile);

#ifdef __cplusplus
}
#endif