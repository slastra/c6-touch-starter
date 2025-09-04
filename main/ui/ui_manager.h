#pragma once

#include "esp_err.h"
#include "lvgl.h"

/**
 * @brief UI Manager - Main UI coordinator and power management
 * 
 * This module manages the overall UI structure, handles power management
 * including screen dimming and fade animations, and coordinates between
 * different UI tiles. It provides a clean interface between hardware
 * (display, touch, sensors) and the modular tile system.
 */

// UI Manager initialization and lifecycle
esp_err_t ui_manager_init(void);
void ui_manager_create_main_screen(void);

// Screen management  
lv_obj_t* ui_manager_get_main_screen(void);

// Display management
esp_err_t ui_manager_init_lvgl_port(void);
void ui_manager_reset_activity(void);
void ui_manager_cleanup(void);

// Tile navigation functions
void ui_manager_switch_to_tile(int tile_index);
void ui_manager_switch_to_settings_tile(void);
void ui_manager_switch_to_main_tile(void);
void ui_manager_switch_to_updates_tile(void);
void ui_manager_switch_to_info_tile(void);
void ui_manager_switch_to_accelerometer_tile(void);
void ui_manager_switch_to_gyroscope_tile(void);
void ui_manager_switch_to_barcode_tile(void);
void ui_manager_switch_to_led_tile(void);

// Power management functions for tiles
extern uint8_t user_brightness;        ///< Current user brightness preference (0-100%)
void power_mgmt_reset_activity(void);
void start_brightness_fade(uint8_t brightness);
void global_touch_event_cb(lv_event_t *e);