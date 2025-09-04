#include "tile_updates.h"
#include "../ui_theme.h"
#include "../ui_components.h"
#include "../../app_config.h"
#include "esp_log.h"

static const char *TAG = "tile_updates";

lv_obj_t* tile_updates_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating updates tile");
    
    // Updates tile content
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Updates");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, ui_theme_get_title_text_color(), 0);
    
    // Create center button for OTA updates
    ui_components_create_button(parent);
    
    // Create status labels (using global variables from ui_components)
    extern lv_obj_t *g_wifi_status_label;
    extern lv_obj_t *g_ota_status_label;
    extern lv_obj_t *g_ota_progress_bar;
    
    g_wifi_status_label = ui_components_create_status_label(parent, "WiFi: Connecting...", UI_WIFI_STATUS_Y_OFFSET, BLACK);
    g_ota_status_label = ui_components_create_status_label(parent, "OTA: Ready", UI_OTA_STATUS_Y_OFFSET, BLACK);
    
    // Create progress bar (initially hidden)
    g_ota_progress_bar = ui_components_create_progress_bar(parent);
    
    return parent;
}