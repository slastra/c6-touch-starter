#include "tile_main.h"
#include "../ui_theme.h"
#include "../ui_components.h"
#include "../../app_config.h"
#include "esp_log.h"

static const char *TAG = "tile_main";

lv_obj_t* tile_main_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating main tile with navigation menu");
    
    // Create title label
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "C6 Touch Starter");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, ui_theme_get_title_text_color(), 0);
    
    // Create version label
    lv_obj_t *version = lv_label_create(parent);
    lv_label_set_text(version, FIRMWARE_VERSION);
    lv_obj_align(version, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET + 20);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(version, ui_theme_get_subtitle_text_color(), 0);
    
    // Create navigation list
    ui_components_create_nav_list(parent);
    
    ESP_LOGI(TAG, "Main tile created with navigation list");
    
    return parent;
}