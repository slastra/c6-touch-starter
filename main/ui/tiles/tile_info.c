#include "tile_info.h"
#include "../ui_theme.h"
#include "../ui_components.h"
#include "../../app_config.h"
#include "esp_log.h"

static const char *TAG = "tile_info";

lv_obj_t* tile_info_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating info tile");
    
    // System info tile content
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "System Info");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, ui_theme_get_title_text_color(), 0);
    
    // Hardware info
    lv_obj_t *hw_label = lv_label_create(parent);
    lv_label_set_text(hw_label, "ESP32-C6\nTouch LCD 1.47\"");
    lv_obj_align(hw_label, LV_ALIGN_CENTER, 0, -50);
    lv_obj_set_style_text_align(hw_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(hw_label, ui_theme_get_default_text_color(), 0);
    
    // Memory info placeholder
    lv_obj_t *mem_label = lv_label_create(parent);
    lv_label_set_text(mem_label, "Memory: OK\nFlash: 8MB");
    lv_obj_align(mem_label, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_align(mem_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(mem_label, ui_theme_get_default_text_color(), 0);
    
    // IP Address info (global variable for dynamic updates)
    extern lv_obj_t *g_ip_status_label;
    g_ip_status_label = lv_label_create(parent);
    lv_label_set_text(g_ip_status_label, "IP: Connecting...");
    lv_obj_align(g_ip_status_label, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_text_align(g_ip_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(g_ip_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_ip_status_label, ui_theme_get_default_text_color(), 0);
    
    return parent;
}