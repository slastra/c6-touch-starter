#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Power management states
 */
typedef enum {
    POWER_STATE_ACTIVE = 0,    /**< Normal operation, full brightness */
    POWER_STATE_DIM,           /**< Dimmed display to save power */
    POWER_STATE_OFF,           /**< Display off, minimal power */
    POWER_STATE_SLEEP          /**< System in light sleep (future use) */
} power_state_t;

/**
 * @brief Wake sources for activity tracking
 */
typedef enum {
    WAKE_SOURCE_TOUCH = 0,     /**< Touch screen interaction */
    WAKE_SOURCE_BUTTON,        /**< Physical button press */
    WAKE_SOURCE_MOTION,        /**< Motion sensor detection */
    WAKE_SOURCE_WOM,           /**< Wake on Motion interrupt */
    WAKE_SOURCE_TIMER,         /**< Periodic timer wake */
    WAKE_SOURCE_API            /**< Software API call */
} wake_source_t;

/**
 * @brief Power management configuration
 */
typedef struct {
    uint32_t dim_timeout_ms;        /**< Time before dimming (ms) */
    uint32_t off_timeout_ms;        /**< Time before turning off display (ms) */
    uint32_t sleep_timeout_ms;      /**< Time before sleep mode (ms) */
    uint8_t dim_brightness;         /**< Brightness level when dimmed (0-100%) */
    uint8_t user_brightness;        /**< Default user brightness (0-100%) */
    bool enable_fade;               /**< Enable smooth brightness transitions */
    uint16_t fade_duration_ms;      /**< Fade animation duration (ms) */
} power_config_t;

/**
 * @brief Callback function for power state changes
 * @param old_state Previous power state
 * @param new_state New power state
 */
typedef void (*power_state_cb_t)(power_state_t old_state, power_state_t new_state);

/**
 * @brief Initialize power management system
 * @param config Power management configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t power_manager_init(const power_config_t *config);

/**
 * @brief Register callback for power state changes
 * @param callback Callback function to register
 * @return ESP_OK on success, error code on failure
 */
esp_err_t power_manager_register_state_callback(power_state_cb_t callback);

/**
 * @brief Reset activity timer (wake up system)
 * @param source Wake source that triggered activity
 */
void power_manager_reset_activity(wake_source_t source);

/**
 * @brief Get current power state
 * @return Current power state
 */
power_state_t power_manager_get_state(void);

/**
 * @brief Set user brightness level
 * @param brightness Brightness level (0-100%)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t power_manager_set_user_brightness(uint8_t brightness);

/**
 * @brief Get current user brightness level
 * @return Current user brightness (0-100%)
 */
uint8_t power_manager_get_user_brightness(void);

/**
 * @brief Get time since last activity in milliseconds
 * @return Idle time in milliseconds
 */
uint32_t power_manager_get_idle_time_ms(void);

/**
 * @brief Force power state transition (for testing/debugging)
 * @param state Target power state
 * @return ESP_OK on success, error code on failure
 */
esp_err_t power_manager_force_state(power_state_t state);

/**
 * @brief Configure light sleep wake sources
 * @return ESP_OK on success, error code on failure
 */
esp_err_t power_manager_configure_light_sleep(void);

/**
 * @brief Enter manual light sleep for specified duration
 * @param duration_ms Sleep duration in milliseconds
 * @return ESP_OK on success, error code on failure
 */
esp_err_t power_manager_enter_manual_sleep(uint32_t duration_ms);

/**
 * @brief Get wake cause as human-readable string
 * @return String describing the wake cause
 */
const char* power_manager_get_wake_cause_string(void);

/**
 * @brief Cleanup power management resources
 */
void power_manager_deinit(void);

#ifdef __cplusplus
}
#endif