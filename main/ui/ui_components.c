#include "ui_components.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "../network/wifi_manager.h"
#include "../network/ota_manager.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_components";

// Global UI component references
lv_obj_t *g_wifi_status_label = NULL;
lv_obj_t *g_ota_status_label = NULL;
lv_obj_t *g_ota_progress_bar = NULL;
lv_obj_t *g_ip_status_label = NULL;

void ui_components_update_wifi_status(const char* status, uint32_t color)
{
    if (g_wifi_status_label != NULL && lvgl_port_lock(0)) {
        lv_label_set_text(g_wifi_status_label, status);
        lv_obj_set_style_text_color(g_wifi_status_label, lv_color_hex(color), 0);
        lvgl_port_unlock();
    }
}

void ui_components_update_ip_status(const char* ip_address, uint32_t color)
{
    if (g_ip_status_label != NULL && lvgl_port_lock(0)) {
        char ip_text[50];
        snprintf(ip_text, sizeof(ip_text), "IP: %s", ip_address);
        lv_label_set_text(g_ip_status_label, ip_text);
        lv_obj_set_style_text_color(g_ip_status_label, lv_color_hex(color), 0);
        lvgl_port_unlock();
    }
}

void ui_components_update_ota_status(const char* status, uint32_t color)
{
    if (g_ota_status_label != NULL && lvgl_port_lock(0)) {
        lv_label_set_text(g_ota_status_label, status);
        lv_obj_set_style_text_color(g_ota_status_label, lv_color_hex(color), 0);
        lvgl_port_unlock();
    }
}

void ui_components_update_ota_progress(int percentage, size_t downloaded_kb, size_t total_kb)
{
    if (g_ota_progress_bar != NULL && lvgl_port_lock(0)) {
        // Update progress bar
        lv_bar_set_value(g_ota_progress_bar, percentage, LV_ANIM_OFF);
        
        // Update status text with progress
        char status_text[100];
        if (total_kb > 0) {
            snprintf(status_text, sizeof(status_text), "OTA: %zu/%zu KB", 
                     downloaded_kb, total_kb);
        } else {
            snprintf(status_text, sizeof(status_text), "OTA: %zu KB", 
                     downloaded_kb);
        }
        lv_label_set_text(g_ota_status_label, status_text);
        
        lvgl_port_unlock();
    }
}

void ui_components_show_progress_bar(void)
{
    if (g_ota_progress_bar != NULL && lvgl_port_lock(0)) {
        lv_obj_clear_flag(g_ota_progress_bar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(g_ota_progress_bar, 0, LV_ANIM_OFF);
        lvgl_port_unlock();
    }
}

void ui_components_hide_progress_bar(void)
{
    if (g_ota_progress_bar != NULL && lvgl_port_lock(0)) {
        lv_obj_add_flag(g_ota_progress_bar, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
    }
}

void ui_components_set_progress_value(int percentage)
{
    if (g_ota_progress_bar != NULL && lvgl_port_lock(0)) {
        lv_bar_set_value(g_ota_progress_bar, percentage, LV_ANIM_OFF);
        lvgl_port_unlock();
    }
}

void ui_components_button_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Button clicked! Starting OTA update...");
        
        // Reset power management activity timer
        ui_manager_reset_activity();
        
        // Change button color to show it was pressed
        lv_obj_t * btn = lv_event_get_target(e);
        lv_obj_t * btn_label = lv_obj_get_child(btn, 0);
        ui_theme_apply_button_pressed_style(btn, btn_label);
        
        // Start OTA update if WiFi is connected and not already in progress
        if (!ota_manager_is_in_progress() && wifi_manager_is_connected()) {
            ota_manager_start_update();
        } else if (!ota_manager_is_in_progress()) {
            ESP_LOGW(TAG, "WiFi not connected - cannot start OTA update");
            ui_components_update_ota_status("OTA: WiFi Required", DARK_RED);
        }
    }
}

lv_obj_t* ui_components_create_button(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, UI_BUTTON_WIDTH, UI_BUTTON_HEIGHT);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, UI_BUTTON_Y_OFFSET);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Update");
    lv_obj_center(btn_label);
    
    // Apply theme
    ui_theme_apply_button_style(btn, btn_label);
    
    // Add click event handler
    lv_obj_add_event_cb(btn, ui_components_button_event_handler, LV_EVENT_CLICKED, NULL);
    
    return btn;
}

lv_obj_t* ui_components_create_status_label(lv_obj_t *parent, const char *text, int y_offset, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, y_offset);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

lv_obj_t* ui_components_create_progress_bar(lv_obj_t *parent)
{
    lv_obj_t *progress_bar = lv_bar_create(parent);
    lv_obj_set_size(progress_bar, UI_PROGRESS_BAR_WIDTH, UI_PROGRESS_BAR_HEIGHT);
    lv_obj_align(progress_bar, LV_ALIGN_BOTTOM_MID, 0, UI_PROGRESS_BAR_Y_OFFSET);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    
    // Apply theme
    ui_theme_apply_progress_bar_style(progress_bar);
    
    // Initially hidden
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    
    return progress_bar;
}

// Navigation list event handler
static void nav_list_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED) {
        // Get the button text to determine which tile to navigate to
        const char *btn_text = lv_list_get_btn_text(lv_obj_get_parent(btn), btn);
        
        ESP_LOGI(TAG, "Navigation button clicked: %s", btn_text);
        
        // Reset power management activity
        ui_manager_reset_activity();
        
        // Navigate to the appropriate tile based on button text
        if (strcmp(btn_text, "Settings") == 0) {
            ui_manager_switch_to_settings_tile();
        } else if (strcmp(btn_text, "Updates") == 0) {
            ui_manager_switch_to_updates_tile();
        } else if (strcmp(btn_text, "System Info") == 0) {
            ui_manager_switch_to_info_tile();
        } else if (strcmp(btn_text, "Accelerometer") == 0) {
            ui_manager_switch_to_accelerometer_tile();
        } else if (strcmp(btn_text, "Gyroscope") == 0) {
            ui_manager_switch_to_gyroscope_tile();
        } else if (strcmp(btn_text, "Barcode Scanner") == 0) {
            ui_manager_switch_to_barcode_tile();
        } else if (strcmp(btn_text, "LED Control") == 0) {
            ui_manager_switch_to_led_tile();
        }
    }
}

lv_obj_t* ui_components_create_nav_list(lv_obj_t *parent)
{
    // Create the list widget
    lv_obj_t *list = lv_list_create(parent);
    lv_obj_set_size(list, LV_HOR_RES - 20, 240); // Leave 10px padding on each side, 240px height
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 20); // Center with slight downward offset
    
    // Add navigation items
    ui_components_add_nav_item(list, "Settings");
    ui_components_add_nav_item(list, "Updates");
    ui_components_add_nav_item(list, "System Info");
    ui_components_add_nav_item(list, "Accelerometer");
    ui_components_add_nav_item(list, "Gyroscope");
    ui_components_add_nav_item(list, "Barcode Scanner");
    ui_components_add_nav_item(list, "LED Control");
    
    return list;
}

lv_obj_t* ui_components_add_nav_item(lv_obj_t *list, const char *text)
{
    // Add button without icon (pass NULL for icon)
    lv_obj_t *btn = lv_list_add_btn(list, NULL, text);
    
    // Add click event handler
    lv_obj_add_event_cb(btn, nav_list_event_handler, LV_EVENT_CLICKED, NULL);
    
    return btn;
}