#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Image download result structure
 */
typedef struct {
    uint8_t *data;              // Raw image data (JPEG/PNG)
    size_t size;                // Size of image data in bytes
    bool success;               // Whether download succeeded
    char error_msg[64];         // Error message if failed
} image_download_result_t;

/**
 * @brief Image download callback function
 * @param result Pointer to download result
 * @param user_data User data passed to download function
 */
typedef void (*image_download_callback_t)(const image_download_result_t *result, void *user_data);

/**
 * @brief Initialize image downloader
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t image_downloader_init(void);

/**
 * @brief Download image from URL asynchronously
 * @param url Image URL to download
 * @param callback Callback function for result
 * @param user_data User data to pass to callback
 * @return ESP_OK if download started, error code otherwise
 */
esp_err_t image_downloader_download_async(const char *url, 
                                          image_download_callback_t callback,
                                          void *user_data);

/**
 * @brief Create LVGL image descriptor from downloaded data
 * @param data Raw image data (JPEG)
 * @param size Size of data in bytes
 * @param max_width Maximum width for decoded image
 * @param max_height Maximum height for decoded image
 * @return LVGL image descriptor or NULL if failed
 */
lv_img_dsc_t* image_downloader_create_lvgl_img(const uint8_t *data, 
                                               size_t size,
                                               uint16_t max_width,
                                               uint16_t max_height);

/**
 * @brief Free LVGL image descriptor created by image_downloader_create_lvgl_img
 * @param img_dsc Image descriptor to free
 */
void image_downloader_free_lvgl_img(lv_img_dsc_t *img_dsc);

/**
 * @brief Check if downloader is busy
 * @return true if download in progress, false otherwise
 */
bool image_downloader_is_busy(void);

#ifdef __cplusplus
}
#endif