#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize display power management
 * @param initial_brightness Initial brightness level (0-100%)
 * @param enable_fade Enable smooth brightness transitions
 * @return ESP_OK on success, error code on failure
 */
esp_err_t display_power_init(uint8_t initial_brightness, bool enable_fade);

/**
 * @brief Set display brightness
 * @param brightness Brightness level (0-100%)
 * @param animate Enable fade animation
 * @return ESP_OK on success, error code on failure
 */
esp_err_t display_power_set_brightness(uint8_t brightness, bool animate);

/**
 * @brief Get current display brightness
 * @return Current brightness level (0-100%)
 */
uint8_t display_power_get_brightness(void);

/**
 * @brief Check if fade animation is active
 * @return true if fade is in progress, false otherwise
 */
bool display_power_is_fading(void);

/**
 * @brief Stop any active fade animation
 */
void display_power_stop_fade(void);

/**
 * @brief Cleanup display power resources
 */
void display_power_deinit(void);

#ifdef __cplusplus
}
#endif