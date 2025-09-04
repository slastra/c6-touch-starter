#include "tile_interface.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "barcode_manager.h"
#include "network/mqtt_barcode.h"
#include "network/image_downloader.h"
#include "app_config.h"
#include "ui_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "tile_barcode";

// UI elements
static lv_obj_t *status_label = NULL;
static lv_obj_t *barcode_label = NULL;
static lv_obj_t *product_image = NULL;
static lv_obj_t *image_spinner = NULL;
static lv_obj_t *product_brand_label = NULL;
static lv_obj_t *product_model_label = NULL;
static lv_obj_t *product_price_label = NULL;
static lv_obj_t *mqtt_status_label = NULL;

// Image state
static lv_img_dsc_t *current_img_dsc = NULL;

// Current barcode being processed
static char current_barcode[32] = {0};

// Function to update MQTT status dynamically
static void update_mqtt_status_label(void) {
    if (mqtt_status_label) {
        lv_label_set_text_fmt(mqtt_status_label, "MQTT: %s", mqtt_barcode_get_status());
    }
}

// Image download callback
static void image_download_callback(const image_download_result_t *result, void *user_data) {
    if (!result) {
        return;
    }
    
    ESP_LOGI(TAG, "Image download result: success=%d", result->success);
    
    // Hide spinner now that download is complete (success or failure)
    if (image_spinner) {
        lv_obj_add_flag(image_spinner, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (result->success && result->data && result->size > 0) {
        // Create LVGL image descriptor for RGB565 data (80x80 pixels)
        lv_img_dsc_t *img_dsc = image_downloader_create_lvgl_img(result->data, result->size, 80, 80);
        if (img_dsc) {
            // Free previous image if any
            if (current_img_dsc) {
                image_downloader_free_lvgl_img(current_img_dsc);
            }
            current_img_dsc = img_dsc;
            
            // Show and update image widget
            if (product_image) {
                lv_obj_clear_flag(product_image, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(product_image, img_dsc);
                ESP_LOGI(TAG, "RGB565 product image displayed");
            }
        } else {
            ESP_LOGW(TAG, "Failed to create LVGL image descriptor");
            // Hide image widget if descriptor creation fails
            if (product_image) {
                lv_obj_add_flag(product_image, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        ESP_LOGW(TAG, "Image download failed: %s", result->error_msg);
        // Hide image widget on failure
        if (product_image) {
            lv_obj_add_flag(product_image, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// MQTT lookup result callback
static void mqtt_lookup_result_callback(const mqtt_barcode_result_t *result) {
    if (!result) {
        return;
    }
    
    ESP_LOGI(TAG, "MQTT lookup result: success=%d, barcode=%s", result->success, result->barcode);
    
    // Update status
    if (status_label) {
        if (result->success) {
            lv_label_set_text(status_label, "Product Found");
            lv_obj_set_style_text_color(status_label, ui_theme_get_success_text_color(), 0);
        } else {
            lv_label_set_text(status_label, "Product Not Found");
            lv_obj_set_style_text_color(status_label, ui_theme_get_error_text_color(), 0);
        }
    }
    
    // Update product information
    if (result->success) {
        // Show brand prominently
        if (product_brand_label && strlen(result->brand) > 0) {
            lv_label_set_text(product_brand_label, result->brand);
        }
        
        // Show model/MPN if available
        if (product_model_label && strlen(result->model) > 0) {
            lv_label_set_text(product_model_label, result->model);
        } else if (product_model_label) {
            // Show truncated product name if no model available
            if (strlen(result->name) > 0) {
                // Truncate long product names to fit display
                char truncated_name[32];
                if (strlen(result->name) > 30) {
                    strncpy(truncated_name, result->name, 27);
                    truncated_name[27] = '.';
                    truncated_name[28] = '.';
                    truncated_name[29] = '.';
                    truncated_name[30] = '\0';
                } else {
                    strncpy(truncated_name, result->name, sizeof(truncated_name) - 1);
                    truncated_name[sizeof(truncated_name) - 1] = '\0';
                }
                lv_label_set_text(product_model_label, truncated_name);
            }
        }
        
        if (product_price_label && strlen(result->price) > 0) {
            lv_label_set_text(product_price_label, result->price);
        }
        
        // Download product image if available
        if (strlen(result->image_url) > 0) {
            ESP_LOGI(TAG, "Downloading product image: %s", result->image_url);
            esp_err_t err = image_downloader_download_async(result->image_url, image_download_callback, NULL);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to start image download: %s", esp_err_to_name(err));
                // Hide spinner and image widget if download fails
                if (image_spinner) {
                    lv_obj_add_flag(image_spinner, LV_OBJ_FLAG_HIDDEN);
                }
                if (product_image) {
                    lv_obj_add_flag(product_image, LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                // Keep spinner visible during download (don't show image yet)
                ESP_LOGI(TAG, "Image download started, spinner continues");
            }
        } else {
            ESP_LOGI(TAG, "No image URL available for this product");
            // Hide spinner and image widget when no image available
            if (image_spinner) {
                lv_obj_add_flag(image_spinner, LV_OBJ_FLAG_HIDDEN);
            }
            if (product_image) {
                lv_obj_add_flag(product_image, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        // Clear product information for not found
        if (product_brand_label) {
            lv_label_set_text(product_brand_label, "");
        }
        if (product_model_label) {
            lv_label_set_text(product_model_label, "");
        }
        if (product_price_label) {
            lv_label_set_text(product_price_label, "");
        }
        // Hide spinner and image for not found products
        if (image_spinner) {
            lv_obj_add_flag(image_spinner, LV_OBJ_FLAG_HIDDEN);
        }
        if (product_image) {
            lv_obj_add_flag(product_image, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    ESP_LOGI(TAG, "UI updated with lookup result");
}

static void barcode_received_callback(const barcode_data_t* barcode)
{
    if (barcode && barcode->valid) {
        ESP_LOGI(TAG, "Barcode scanned: %s", barcode->data);
        
        // Store current barcode
        strncpy(current_barcode, barcode->data, sizeof(current_barcode) - 1);
        
        // Update barcode display
        if (barcode_label) {
            lv_label_set_text(barcode_label, barcode->data);
        }
        
        // Reset power management activity (wake up device if sleeping/dimmed)
        ui_manager_reset_activity();
        
        // Auto-switch to barcode tile to show the result
        ui_manager_switch_to_barcode_tile();
        
        // Clear previous product information
        if (product_brand_label) lv_label_set_text(product_brand_label, "");
        if (product_model_label) lv_label_set_text(product_model_label, "");
        if (product_price_label) lv_label_set_text(product_price_label, "");
        
        // Clear and hide previous product image
        if (product_image) {
            lv_obj_add_flag(product_image, LV_OBJ_FLAG_HIDDEN);
            lv_img_set_src(product_image, NULL);  // Clear image source
        }
        
        // Free previous image descriptor if exists
        if (current_img_dsc) {
            image_downloader_free_lvgl_img(current_img_dsc);
            current_img_dsc = NULL;
        }
        
        // Show spinner while waiting for product data
        if (image_spinner) {
            lv_obj_clear_flag(image_spinner, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Check MQTT connection status
        if (!mqtt_barcode_is_connected()) {
            if (status_label) {
                lv_label_set_text(status_label, "MQTT Disconnected");
                lv_obj_set_style_text_color(status_label, ui_theme_get_error_text_color(), 0);
            }
            ESP_LOGW(TAG, "MQTT not connected, cannot lookup barcode");
            return;
        }
        
        // Show looking up status
        if (status_label) {
            lv_label_set_text(status_label, "Looking up product...");
            lv_obj_set_style_text_color(status_label, ui_theme_get_muted_text_color(), 0);
        }
        
        // Update MQTT status
        update_mqtt_status_label();
        
        // Start MQTT lookup
        esp_err_t err = mqtt_barcode_lookup(barcode->data, mqtt_lookup_result_callback);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start MQTT lookup: %s", esp_err_to_name(err));
            if (status_label) {
                lv_label_set_text(status_label, "Lookup Failed");
                lv_obj_set_style_text_color(status_label, ui_theme_get_error_text_color(), 0);
            }
        }
        
        ESP_LOGI(TAG, "Barcode scan triggered device wake and lookup");
    }
}

TILE_CREATE_FUNCTION(barcode)
{
    ESP_LOGI(TAG, "Creating enhanced barcode tile with product lookup");
    
    // Create title label
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Barcode Scanner");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, ui_theme_get_title_text_color(), 0);
    
    // Create status label (for lookup status)
    status_label = lv_label_create(parent);
    lv_label_set_text(status_label, "Ready to scan");
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET + 25);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_label, ui_theme_get_status_text_color(), 0);
    
    // Create barcode display label
    barcode_label = lv_label_create(parent);
    lv_label_set_text(barcode_label, "No barcode yet");
    lv_obj_align(barcode_label, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET + 50);
    lv_obj_set_style_text_font(barcode_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(barcode_label, ui_theme_get_default_text_color(), 0);
    lv_label_set_long_mode(barcode_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(barcode_label, LV_HOR_RES - 40);
    lv_obj_set_style_text_align(barcode_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Create product image widget
    product_image = lv_img_create(parent);
    lv_obj_set_size(product_image, 80, 80);
    lv_obj_align(product_image, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET + 75);
    lv_obj_set_style_border_color(product_image, ui_theme_get_muted_text_color(), 0);
    lv_obj_set_style_border_width(product_image, 1, 0);
    lv_obj_add_flag(product_image, LV_OBJ_FLAG_HIDDEN); // Hidden by default
    
    // Create loading spinner (same position as image, but smaller and centered)
    image_spinner = lv_spinner_create(parent, 1000, 60);  // 1 second animation, 60° arc
    lv_obj_set_size(image_spinner, 40, 40);  // Smaller than image
    lv_obj_align(image_spinner, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET + 95);  // Centered in image area
    lv_obj_set_style_arc_color(image_spinner, ui_theme_get_background_color(), LV_PART_MAIN);  // Background color
    lv_obj_set_style_arc_color(image_spinner, ui_theme_get_muted_text_color(), LV_PART_INDICATOR);  // Gray indicator
    lv_obj_add_flag(image_spinner, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    // Create product information labels (adjusted Y offset to make room for image)
    int y_offset = UI_TITLE_Y_OFFSET + 165;
    
    // Product brand (larger, prominent)
    product_brand_label = lv_label_create(parent);
    lv_label_set_text(product_brand_label, "");
    lv_obj_align(product_brand_label, LV_ALIGN_TOP_MID, 0, y_offset);
    lv_obj_set_style_text_font(product_brand_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(product_brand_label, ui_theme_get_default_text_color(), 0);
    lv_label_set_long_mode(product_brand_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(product_brand_label, LV_HOR_RES - 30);
    lv_obj_set_style_text_align(product_brand_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Product model/MPN
    product_model_label = lv_label_create(parent);
    lv_label_set_text(product_model_label, "");
    lv_obj_align(product_model_label, LV_ALIGN_TOP_MID, 0, y_offset + 25);
    lv_obj_set_style_text_font(product_model_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(product_model_label, ui_theme_get_default_text_color(), 0);
    lv_label_set_long_mode(product_model_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(product_model_label, LV_HOR_RES - 30);
    lv_obj_set_style_text_align(product_model_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Product price (larger font)
    product_price_label = lv_label_create(parent);
    lv_label_set_text(product_price_label, "");
    lv_obj_align(product_price_label, LV_ALIGN_TOP_MID, 0, y_offset + 55);
    lv_obj_set_style_text_font(product_price_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(product_price_label, ui_theme_get_default_text_color(), 0);
    lv_label_set_long_mode(product_price_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(product_price_label, LV_HOR_RES - 30);
    lv_obj_set_style_text_align(product_price_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // MQTT connection status (bottom)
    mqtt_status_label = lv_label_create(parent);
    lv_label_set_text(mqtt_status_label, "MQTT: Initializing");
    lv_obj_align(mqtt_status_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(mqtt_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mqtt_status_label, ui_theme_get_muted_text_color(), 0);
    
    // Update MQTT status after initial creation
    update_mqtt_status_label();
    
    ESP_LOGI(TAG, "Enhanced barcode tile created with product display");
    return parent;
}

TILE_INIT_FUNCTION(barcode)
{
    esp_err_t ret = barcode_manager_init(barcode_received_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize barcode manager: %s", esp_err_to_name(ret));
        if (status_label) {
            lv_label_set_text(status_label, "Scanner init failed");
        }
        return ret;
    }
    
    // Initialize image downloader
    ret = image_downloader_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize image downloader: %s", esp_err_to_name(ret));
        // Continue without image support
    } else {
        ESP_LOGI(TAG, "Image downloader initialized");
    }
    
    ESP_LOGI(TAG, "Barcode tile initialized successfully");
    return ESP_OK;
}