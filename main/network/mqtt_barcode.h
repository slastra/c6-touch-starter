#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Barcode lookup result structure
 */
typedef struct {
    char barcode[32];           // Original barcode scanned
    char name[128];             // Product name
    char brand[64];             // Brand name
    char model[64];             // Model/MPN (Manufacturer Part Number)
    char category[64];          // Product category
    char price[32];             // Price information
    char description[256];      // Product description (optional)
    char image_url[256];        // Product image URL (optional)
    bool success;               // Whether lookup was successful
    uint32_t request_id;        // Request correlation ID
    uint32_t lookup_time_ms;    // Time taken for lookup
} mqtt_barcode_result_t;

/**
 * @brief Callback function for barcode lookup results
 * @param result Pointer to barcode lookup result
 */
typedef void (*mqtt_barcode_callback_t)(const mqtt_barcode_result_t *result);

/**
 * @brief Initialize MQTT barcode lookup system
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_barcode_init(void);

/**
 * @brief Deinitialize MQTT barcode lookup system
 */
void mqtt_barcode_deinit(void);

/**
 * @brief Lookup barcode information via MQTT
 * @param barcode Barcode string to lookup
 * @param callback Callback function for result
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_barcode_lookup(const char *barcode, mqtt_barcode_callback_t callback);

/**
 * @brief Check if MQTT client is connected
 * @return true if connected, false otherwise
 */
bool mqtt_barcode_is_connected(void);

/**
 * @brief Get connection status string
 * @return Status string (for UI display)
 */
const char* mqtt_barcode_get_status(void);

#ifdef __cplusplus
}
#endif