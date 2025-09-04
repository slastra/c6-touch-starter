#pragma once

#include "lvgl.h"
#include "../app_config.h"

// UI component references (these will be set by ui_manager)
extern lv_obj_t *g_wifi_status_label;
extern lv_obj_t *g_ota_status_label;
extern lv_obj_t *g_ota_progress_bar;
extern lv_obj_t *g_ip_status_label;

// Component update functions
void ui_components_update_wifi_status(const char* status, uint32_t color);
void ui_components_update_ip_status(const char* ip_address, uint32_t color);
void ui_components_update_ota_status(const char* status, uint32_t color);
void ui_components_update_ota_progress(int percentage, size_t downloaded_kb, size_t total_kb);

// Progress bar control
void ui_components_show_progress_bar(void);
void ui_components_hide_progress_bar(void);
void ui_components_set_progress_value(int percentage);

// Button event handler
void ui_components_button_event_handler(lv_event_t * e);

// Component creation helpers
lv_obj_t* ui_components_create_button(lv_obj_t *parent);
lv_obj_t* ui_components_create_status_label(lv_obj_t *parent, const char *text, int y_offset, uint32_t color);
lv_obj_t* ui_components_create_progress_bar(lv_obj_t *parent);

// Navigation list helpers
lv_obj_t* ui_components_create_nav_list(lv_obj_t *parent);
lv_obj_t* ui_components_add_nav_item(lv_obj_t *list, const char *text);