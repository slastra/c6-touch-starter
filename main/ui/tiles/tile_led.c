#include "tile_led.h"
#include "../ui_theme.h"
#include "../ui_manager.h"
#include "../../app_config.h"
#include "../../led_manager.h"
#include "esp_log.h"

static const char *TAG = "tile_led";

static lv_obj_t *red_slider = NULL;
static lv_obj_t *green_slider = NULL;
static lv_obj_t *blue_slider = NULL;
static lv_obj_t *color_preview = NULL;
static lv_obj_t *toggle_btn = NULL;
static lv_obj_t *toggle_label = NULL;

static uint8_t current_red = 0;
static uint8_t current_green = 0;
static uint8_t current_blue = 0;
static bool led_enabled = false;

static void update_led_color(void)
{
    // Only update physical LED if enabled
    if (led_enabled) {
        led_manager_set_rgb(current_red, current_green, current_blue);
    }
    
    // Always update color preview
    if (color_preview != NULL) {
        lv_color_t color = lv_color_make(current_red, current_green, current_blue);
        lv_obj_set_style_bg_color(color_preview, color, 0);
    }
}

static void toggle_button_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        led_enabled = !led_enabled;
        
        if (led_enabled) {
            // Turn LED on with current RGB values
            led_manager_set_rgb(current_red, current_green, current_blue);
            lv_label_set_text(toggle_label, "LED: ON");
            lv_obj_set_style_bg_color(toggle_btn, ui_theme_get_success_text_color(), 0);
            ESP_LOGI(TAG, "LED turned ON");
        } else {
            // Turn LED off
            led_manager_clear();
            lv_label_set_text(toggle_label, "LED: OFF");
            lv_obj_set_style_bg_color(toggle_btn, ui_theme_get_error_text_color(), 0);
            ESP_LOGI(TAG, "LED turned OFF");
        }
    }
}

static void red_slider_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        current_red = (uint8_t)lv_slider_get_value(slider);
        update_led_color();
        
        ESP_LOGD(TAG, "Red value changed to %d", current_red);
    }
}

static void green_slider_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        current_green = (uint8_t)lv_slider_get_value(slider);
        update_led_color();
        
        ESP_LOGD(TAG, "Green value changed to %d", current_green);
    }
}

static void blue_slider_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        current_blue = (uint8_t)lv_slider_get_value(slider);
        update_led_color();
        
        ESP_LOGD(TAG, "Blue value changed to %d", current_blue);
    }
}

lv_obj_t* tile_led_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating LED control tile");
    
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "LED Control");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, ui_theme_get_title_text_color(), 0);
    
    color_preview = lv_obj_create(parent);
    lv_obj_set_size(color_preview, 80, 40);
    lv_obj_align(color_preview, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(color_preview, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_border_color(color_preview, ui_theme_get_muted_text_color(), 0);
    lv_obj_set_style_border_width(color_preview, 2, 0);
    
    // Red slider (no label, moved up)
    red_slider = lv_slider_create(parent);
    lv_obj_set_width(red_slider, EXAMPLE_LCD_H_RES - 40);
    lv_obj_align(red_slider, LV_ALIGN_TOP_MID, 0, 110);
    lv_slider_set_range(red_slider, 0, 255);
    lv_slider_set_value(red_slider, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(red_slider, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(red_slider, lv_color_hex(0xFF0000), LV_PART_KNOB);
    lv_obj_add_event_cb(red_slider, red_slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Green slider (no label, moved up)
    green_slider = lv_slider_create(parent);
    lv_obj_set_width(green_slider, EXAMPLE_LCD_H_RES - 40);
    lv_obj_align(green_slider, LV_ALIGN_TOP_MID, 0, 155);
    lv_slider_set_range(green_slider, 0, 255);
    lv_slider_set_value(green_slider, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(green_slider, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(green_slider, lv_color_hex(0x00FF00), LV_PART_KNOB);
    lv_obj_add_event_cb(green_slider, green_slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Blue slider (no label, moved up)
    blue_slider = lv_slider_create(parent);
    lv_obj_set_width(blue_slider, EXAMPLE_LCD_H_RES - 40);
    lv_obj_align(blue_slider, LV_ALIGN_TOP_MID, 0, 200);
    lv_slider_set_range(blue_slider, 0, 255);
    lv_slider_set_value(blue_slider, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(blue_slider, lv_color_hex(0x0000FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(blue_slider, lv_color_hex(0x0000FF), LV_PART_KNOB);
    lv_obj_add_event_cb(blue_slider, blue_slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // LED On/Off toggle button
    toggle_btn = lv_btn_create(parent);
    lv_obj_set_size(toggle_btn, UI_BUTTON_WIDTH, UI_BUTTON_HEIGHT);
    lv_obj_align(toggle_btn, LV_ALIGN_TOP_MID, 0, 250);
    lv_obj_set_style_bg_color(toggle_btn, ui_theme_get_error_text_color(), 0);  // Dark red when off
    lv_obj_add_event_cb(toggle_btn, toggle_button_event_handler, LV_EVENT_CLICKED, NULL);
    
    toggle_label = lv_label_create(toggle_btn);
    lv_label_set_text(toggle_label, "LED: OFF");
    lv_obj_center(toggle_label);
    lv_obj_set_style_text_color(toggle_label, ui_theme_get_tile_bg_color(), 0);  // White text on colored button
    
    ESP_LOGI(TAG, "LED control tile created");
    
    return parent;
}

esp_err_t tile_led_init(lv_obj_t *tile)
{
    ESP_LOGI(TAG, "Initializing LED tile");
    
    led_manager_clear();
    
    return ESP_OK;
}

bool tile_led_is_enabled(void)
{
    return led_enabled;
}