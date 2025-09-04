#include "tile_settings.h"
#include "../ui_manager.h"  // For power management functions
#include "../ui_theme.h"
#include "../../app_config.h"
#include "../../power/power_manager.h"  // For new power management API
#include "esp_log.h"
#include "bsp_battery.h"
#include "bsp_qmi8658.h"

static const char *TAG = "tile_settings";

// Settings tile widgets
static lv_obj_t *status_table = NULL;
static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_label = NULL;
static lv_timer_t *battery_timer = NULL;

static void brightness_slider_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        int32_t brightness_value = lv_slider_get_value(slider);
        
        // Update user brightness preference via power manager
        power_manager_set_user_brightness((uint8_t)brightness_value);
        
        // Update the label to show current percentage
        if (brightness_label != NULL) {
            lv_label_set_text_fmt(brightness_label, "Brightness: %d%%", (int)brightness_value);
        }
        
        // Reset activity to keep screen active during brightness adjustment
        power_manager_reset_activity(WAKE_SOURCE_API);
        
        ESP_LOGI(TAG, "Brightness set to %d%% via power manager", (int)brightness_value);
    }
}

static void battery_update_timer_callback(lv_timer_t *timer)
{
    // Get current active tile using external tileview reference
    // This will need to be refactored to access the active tile properly
    
    float voltage;
    uint16_t adc_raw;
    
    // Read battery voltage
    bsp_battery_get_voltage(&voltage, &adc_raw);
    
    // Calculate battery percentage (3.0V = 0%, 4.2V = 100%)
    float percentage = ((voltage - 3.0f) / (4.2f - 3.0f)) * 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 100.0f) percentage = 100.0f;
    
    // Update battery status in status table
    if (status_table != NULL) {
        lv_table_set_cell_value_fmt(status_table, 1, 1, "%.2fV", voltage);
    }
}

lv_obj_t* tile_settings_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating settings tile");
    
    // Status tile content
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Status");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, ui_theme_get_title_text_color(), 0);
    
    // Create status table
    status_table = lv_table_create(parent);
    lv_obj_set_width(status_table, EXAMPLE_LCD_H_RES - 20);
    lv_obj_set_height(status_table, LV_SIZE_CONTENT);  // Auto-size to content
    lv_obj_align(status_table, LV_ALIGN_TOP_MID, 0, 55);
    
    // Configure table: 2 columns, 4 rows
    lv_table_set_col_cnt(status_table, 2);
    lv_table_set_row_cnt(status_table, 4);
    
    // Set column widths
    lv_table_set_col_width(status_table, 0, 70);  // Labels column
    lv_table_set_col_width(status_table, 1, 80);  // Values column
    
    // Apply table styling to match theme
    lv_obj_set_style_bg_color(status_table, ui_theme_get_tile_bg_color(), 0);
    lv_obj_set_style_border_color(status_table, ui_theme_get_muted_text_color(), 0);
    lv_obj_set_style_border_width(status_table, 1, 0);
    lv_obj_set_style_text_font(status_table, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_table, ui_theme_get_default_text_color(), 0);
    
    // Set table cell values - Labels in column 0
    lv_table_set_cell_value(status_table, 0, 0, "WiFi");
    lv_table_set_cell_value(status_table, 1, 0, "Batt.");
    lv_table_set_cell_value(status_table, 2, 0, "Temp.");
    lv_table_set_cell_value(status_table, 3, 0, "Ver.");
    
    // Set initial values - Values in column 1
    lv_table_set_cell_value(status_table, 0, 1, CONFIG_WIFI_SSID);
    lv_table_set_cell_value(status_table, 1, 1, "--V");
    lv_table_set_cell_value(status_table, 2, 1, "--°C");
    lv_table_set_cell_value(status_table, 3, 1, FIRMWARE_VERSION);
    
    // Add brightness control below table
    brightness_label = lv_label_create(parent);
    lv_label_set_text(brightness_label, "Brightness: 80%");
    lv_obj_align_to(brightness_label, status_table, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(brightness_label, ui_theme_get_default_text_color(), 0);
    
    brightness_slider = lv_slider_create(parent);
    lv_obj_set_width(brightness_slider, 120);
    lv_obj_align_to(brightness_slider, brightness_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_slider_set_range(brightness_slider, 10, 100);  // Min 10% to prevent completely dark screen
    lv_slider_set_value(brightness_slider, 80, LV_ANIM_OFF);  // Default to current brightness
    
    // Apply slider styling to match theme
    lv_obj_set_style_bg_color(brightness_slider, ui_theme_get_muted_text_color(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, ui_theme_get_primary_color(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, ui_theme_get_tile_bg_color(), LV_PART_KNOB);
    
    // Add event handler
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    return parent;
}

esp_err_t tile_settings_init(lv_obj_t *tile)
{
    ESP_LOGI(TAG, "Initializing settings tile resources");
    
    // Initialize battery monitoring
    ESP_LOGI(TAG, "Initializing battery monitoring");
    bsp_battery_init();
    
    // Start battery update timer (update every 1000ms)
    battery_timer = lv_timer_create(battery_update_timer_callback, 1000, NULL);
    
    // Add touch handlers to brightness slider for power management
    if (brightness_slider != NULL) {
        lv_obj_add_event_cb(brightness_slider, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(brightness_slider, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
    }
    
    return ESP_OK;
}

lv_obj_t* tile_settings_get_status_table(void)
{
    return status_table;
}

lv_obj_t* tile_settings_get_brightness_slider(void)
{
    return brightness_slider;
}

lv_obj_t* tile_settings_get_brightness_label(void)
{
    return brightness_label;
}

lv_timer_t* tile_settings_get_battery_timer(void)
{
    return battery_timer;
}