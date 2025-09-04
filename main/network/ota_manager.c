#include "ota_manager.h"
#include "wifi_manager.h"
#include "../app_config.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ota_manager";

// OTA state
static bool ota_in_progress = false;

// Callbacks
static ota_status_callback_t status_callback = NULL;
static ota_progress_callback_t progress_callback = NULL;
static ota_progress_bar_callback_t progress_bar_show_callback = NULL;
static ota_progress_bar_callback_t progress_bar_hide_callback = NULL;
static void (*progress_bar_set_callback)(int percentage) = NULL;

// Forward declarations
static void ota_task(void *param);

esp_err_t ota_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing OTA Manager");
    ota_in_progress = false;
    return ESP_OK;
}

void ota_manager_start_update(void)
{
    if (ota_in_progress) {
        ESP_LOGW(TAG, "OTA update already in progress");
        return;
    }
    
    ESP_LOGI(TAG, "Creating OTA update task...");
    xTaskCreate(ota_task, "ota_task", 16384, NULL, 5, NULL);
}

void ota_manager_abort_update(void)
{
    ota_in_progress = false;
}

bool ota_manager_is_in_progress(void)
{
    return ota_in_progress;
}

void ota_manager_set_status_callback(ota_status_callback_t callback)
{
    status_callback = callback;
}

void ota_manager_set_progress_callback(ota_progress_callback_t callback)
{
    progress_callback = callback;
}

void ota_manager_set_progress_bar_show_callback(ota_progress_bar_callback_t callback)
{
    progress_bar_show_callback = callback;
}

void ota_manager_set_progress_bar_hide_callback(ota_progress_bar_callback_t callback)
{
    progress_bar_hide_callback = callback;
}

void ota_manager_set_progress_bar_set_callback(void (*callback)(int percentage))
{
    progress_bar_set_callback = callback;
}

static void ota_task(void *param)
{
    ESP_LOGI(TAG, "Starting OTA update task...");
    ota_in_progress = true;
    
    // Show connecting status
    if (status_callback) {
        status_callback("OTA: Connecting...", LIGHT_RED);
    }
    
    esp_http_client_config_t config = {
        .url = OTA_UPDATE_URL,
        .timeout_ms = OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .max_redirection_count = 3,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    
    // Use advanced OTA API for progress tracking
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_config, &https_ota_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed: %s", esp_err_to_name(ret));
        if (status_callback) {
            status_callback("OTA: Connection failed", DARK_RED);
        }
        ota_in_progress = false;
        vTaskDelete(NULL);
        return;
    }
    
    // Get total image size
    int image_size = esp_https_ota_get_image_size(https_ota_handle);
    ESP_LOGI(TAG, "Image size: %d bytes", image_size);
    
    // Show initial progress
    if (status_callback) {
        status_callback("OTA: Please wait...", PRIMARY_RED);
    }
    if (progress_bar_show_callback) {
        progress_bar_show_callback();
    }
    
    // Download loop with progress updates
    while (1) {
        ret = esp_https_ota_perform(https_ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Calculate and update progress
        int downloaded = esp_https_ota_get_image_len_read(https_ota_handle);
        int percentage = (image_size > 0) ? (downloaded * 100) / image_size : 0;
        
        if (progress_callback) {
            progress_callback(percentage, downloaded / 1024, image_size / 1024);
        }
        
        // Update every 100ms
        vTaskDelay(pdMS_TO_TICKS(OTA_PROGRESS_UPDATE_DELAY_MS));
    }
    
    if (ret == ESP_OK) {
        // Show flash writing status
        if (status_callback) {
            status_callback("OTA: Writing to flash...", LIGHT_RED);
        }
        if (progress_bar_set_callback) {
            progress_bar_set_callback(100);
        }
        
        ret = esp_https_ota_finish(https_ota_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA update successful! Restarting...");
            if (status_callback) {
                status_callback("OTA: Success!", PRIMARY_RED);
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        } else {
            ESP_LOGE(TAG, "ESP HTTPS OTA finish failed: %s", esp_err_to_name(ret));
            if (status_callback) {
                status_callback("OTA: Flash write failed", DARK_RED);
            }
        }
    } else {
        ESP_LOGE(TAG, "ESP HTTPS OTA perform failed: %s", esp_err_to_name(ret));
        if (status_callback) {
            status_callback("OTA: Download failed", DARK_RED);
        }
        esp_https_ota_abort(https_ota_handle);
    }
    
    // Hide progress bar on completion/failure
    if (progress_bar_hide_callback) {
        progress_bar_hide_callback();
    }
    
    ota_in_progress = false;
    vTaskDelete(NULL);
}