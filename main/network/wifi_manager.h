#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include <stdbool.h>

// WiFi Manager initialization and control
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start(void);
void wifi_manager_stop(void);

// Connection status
bool wifi_manager_is_connected(void);
bool wifi_manager_is_connecting(void);

// Event handler
void wifi_manager_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// Status update callback type
typedef void (*wifi_status_callback_t)(const char* status, uint32_t color);
void wifi_manager_set_status_callback(wifi_status_callback_t callback);