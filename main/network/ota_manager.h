#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

// OTA Manager initialization and control
esp_err_t ota_manager_init(void);
void ota_manager_start_update(void);
void ota_manager_abort_update(void);

// Status queries
bool ota_manager_is_in_progress(void);

// Callback types for status updates
typedef void (*ota_status_callback_t)(const char* status, uint32_t color);
typedef void (*ota_progress_callback_t)(int percentage, size_t downloaded_kb, size_t total_kb);
typedef void (*ota_progress_bar_callback_t)(void);

// Callback registration
void ota_manager_set_status_callback(ota_status_callback_t callback);
void ota_manager_set_progress_callback(ota_progress_callback_t callback);
void ota_manager_set_progress_bar_show_callback(ota_progress_bar_callback_t callback);
void ota_manager_set_progress_bar_hide_callback(ota_progress_bar_callback_t callback);
void ota_manager_set_progress_bar_set_callback(void (*callback)(int percentage));