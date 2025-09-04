#include "image_downloader.h"
#include "wifi_manager.h"
#include "../app_config.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "image_downloader";

// Configuration
#define MAX_IMAGE_SIZE          (50 * 1024)    // 50KB max
#define DOWNLOAD_TIMEOUT_MS     10000           // 10 second timeout
#define HTTP_BUFFER_SIZE        4096            // 4KB chunks

// Download state
typedef struct {
    uint8_t *buffer;
    size_t buffer_size;
    size_t data_size;
    image_download_callback_t callback;
    void *user_data;
    bool busy;
    char url[256];
} download_state_t;

static download_state_t download_state = {0};

/**
 * HTTP event handler for image download
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (download_state.data_size + evt->data_len > MAX_IMAGE_SIZE) {
                ESP_LOGW(TAG, "Image too large, truncating at %d bytes", MAX_IMAGE_SIZE);
                return ESP_FAIL;
            }
            
            if (download_state.data_size + evt->data_len > download_state.buffer_size) {
                ESP_LOGE(TAG, "Buffer overflow prevented");
                return ESP_FAIL;
            }
            
            memcpy(download_state.buffer + download_state.data_size, evt->data, evt->data_len);
            download_state.data_size += evt->data_len;
            break;
            
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "Image download completed: %d bytes", download_state.data_size);
            break;
            
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP connection disconnected");
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

/**
 * Download task function
 */
static void download_task(void *param)
{
    image_download_result_t result = {0};
    
    ESP_LOGI(TAG, "Starting image download from: %s", download_state.url);
    
    // Configure HTTP client for insecure HTTPS (no certificate validation)
    esp_http_client_config_t config = {
        .url = download_state.url,
        .event_handler = http_event_handler,
        .timeout_ms = DOWNLOAD_TIMEOUT_MS,
        .buffer_size = HTTP_BUFFER_SIZE,
        .max_redirection_count = 3,               // Follow redirects
        .skip_cert_common_name_check = true,      // Skip CN validation
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        strncpy(result.error_msg, "HTTP client init failed", sizeof(result.error_msg) - 1);
        goto cleanup;
    }
    
    // Perform HTTP GET request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        snprintf(result.error_msg, sizeof(result.error_msg), "Download failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    
    // Check HTTP status
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGW(TAG, "HTTP request returned status %d", status_code);
        snprintf(result.error_msg, sizeof(result.error_msg), "HTTP %d", status_code);
        goto cleanup;
    }
    
    // Success - prepare result
    if (download_state.data_size > 0) {
        result.data = download_state.buffer;
        result.size = download_state.data_size;
        result.success = true;
        ESP_LOGI(TAG, "Image downloaded successfully: %d bytes", download_state.data_size);
    } else {
        strncpy(result.error_msg, "No data received", sizeof(result.error_msg) - 1);
    }
    
cleanup:
    if (client) {
        esp_http_client_cleanup(client);
    }
    
    // Call callback with result
    if (download_state.callback) {
        download_state.callback(&result, download_state.user_data);
    }
    
    // Cleanup state
    download_state.busy = false;
    
    // Task will be deleted automatically when function returns
    vTaskDelete(NULL);
}

esp_err_t image_downloader_init(void)
{
    ESP_LOGI(TAG, "Initializing image downloader");
    
    // Allocate buffer for downloads
    if (download_state.buffer == NULL) {
        download_state.buffer = malloc(MAX_IMAGE_SIZE);
        if (download_state.buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate image buffer");
            return ESP_ERR_NO_MEM;
        }
        download_state.buffer_size = MAX_IMAGE_SIZE;
    }
    
    download_state.busy = false;
    
    ESP_LOGI(TAG, "Image downloader initialized (max size: %d KB)", MAX_IMAGE_SIZE / 1024);
    return ESP_OK;
}

esp_err_t image_downloader_download_async(const char *url, 
                                          image_download_callback_t callback,
                                          void *user_data)
{
    if (!url || !callback) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (download_state.busy) {
        ESP_LOGW(TAG, "Download already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (strlen(url) >= sizeof(download_state.url)) {
        ESP_LOGE(TAG, "URL too long");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Reset download state
    download_state.data_size = 0;
    download_state.callback = callback;
    download_state.user_data = user_data;
    download_state.busy = true;
    strncpy(download_state.url, url, sizeof(download_state.url) - 1);
    
    // Create download task
    BaseType_t result = xTaskCreate(
        download_task,
        "img_download",
        8192,  // 8KB stack
        NULL,
        5,     // Priority
        NULL
    );
    
    if (result != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create download task");
        download_state.busy = false;
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

lv_img_dsc_t* image_downloader_create_lvgl_img(const uint8_t *data, 
                                               size_t size,
                                               uint16_t width,
                                               uint16_t height)
{
    if (!data || size == 0) {
        return NULL;
    }
    
    ESP_LOGI(TAG, "Creating LVGL image from %d bytes of RGB565 data (%dx%d)", size, width, height);
    
    // Validate RGB565 data size
    size_t expected_size = width * height * 2;  // 2 bytes per pixel for RGB565
    if (size != expected_size) {
        ESP_LOGW(TAG, "Size mismatch for RGB565: expected %d bytes for %dx%d, got %d bytes", 
                 expected_size, width, height, size);
        // Still try to process, but log the issue
    }
    
    lv_img_dsc_t *img_dsc = malloc(sizeof(lv_img_dsc_t));
    if (!img_dsc) {
        ESP_LOGE(TAG, "Failed to allocate image descriptor");
        return NULL;
    }
    
    // Copy the RGB565 pixel data to a permanent buffer
    uint8_t *img_data = malloc(size);
    if (!img_data) {
        ESP_LOGE(TAG, "Failed to allocate image data buffer");
        free(img_dsc);
        return NULL;
    }
    
    memcpy(img_data, data, size);
    
    // Set up image descriptor for raw RGB565 data
    img_dsc->data = img_data;
    img_dsc->data_size = size;
    
    // Configure for RGB565 format (LVGL native 16-bit color)
    img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565 format
    img_dsc->header.w = width;
    img_dsc->header.h = height;
    img_dsc->header.always_zero = 0;            // Must be 0
    img_dsc->header.reserved = 0;               // Must be 0
    
    ESP_LOGI(TAG, "LVGL RGB565 image descriptor created: %dx%d pixels", width, height);
    return img_dsc;
}

void image_downloader_free_lvgl_img(lv_img_dsc_t *img_dsc)
{
    if (img_dsc) {
        if (img_dsc->data) {
            free((void*)img_dsc->data);
        }
        free(img_dsc);
    }
}

bool image_downloader_is_busy(void)
{
    return download_state.busy;
}