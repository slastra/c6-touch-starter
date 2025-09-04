#include "tile_accelerometer.h"
#include "../ui_theme.h"
#include "../../app_config.h"
#include "esp_log.h"
#include "bsp_qmi8658.h"

static const char *TAG = "tile_accelerometer";

// Accelerometer chart widgets
static lv_obj_t *accel_chart = NULL;
static lv_chart_series_t *accel_series_x = NULL;
static lv_chart_series_t *accel_series_y = NULL;
static lv_chart_series_t *accel_series_z = NULL;

lv_obj_t* tile_accelerometer_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating accelerometer tile");
    
    // Accelerometer tile content
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Accelerometer");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, ui_theme_get_title_text_color(), 0);
    
    // Create accelerometer chart (most of the screen)
    accel_chart = lv_chart_create(parent);
    lv_obj_set_size(accel_chart, EXAMPLE_LCD_H_RES - 20, 200); // Larger chart
    lv_obj_align(accel_chart, LV_ALIGN_TOP_MID, 0, 55);
    lv_chart_set_type(accel_chart, LV_CHART_TYPE_LINE);
    
    // Configure chart properties for accelerometer (±4g range)
    lv_chart_set_point_count(accel_chart, 50);  // 50 points = 10 seconds at 5Hz
    lv_chart_set_range(accel_chart, LV_CHART_AXIS_PRIMARY_Y, -4000, 4000);  // ±4g in millig
    lv_chart_set_div_line_count(accel_chart, 8, 8);  // More grid lines for precision
    
    // Create data series for X, Y, Z axes with different colors
    accel_series_x = lv_chart_add_series(accel_chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y); // Red for X
    accel_series_y = lv_chart_add_series(accel_chart, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y); // Green for Y  
    accel_series_z = lv_chart_add_series(accel_chart, lv_color_hex(0x0080FF), LV_CHART_AXIS_PRIMARY_Y); // Blue for Z
    
    // Hide chart markers/dots to show continuous lines only
    lv_obj_set_style_size(accel_chart, 0, LV_PART_INDICATOR);
    
    // Chart styling - white background with gray grid
    lv_obj_set_style_bg_color(accel_chart, ui_theme_get_tile_bg_color(), 0);
    lv_obj_set_style_border_color(accel_chart, ui_theme_get_muted_text_color(), 0);
    lv_obj_set_style_border_width(accel_chart, 1, 0);
    lv_obj_set_style_line_color(accel_chart, ui_theme_get_muted_text_color(), LV_PART_ITEMS);
    
    return parent;
}

esp_err_t tile_accelerometer_init(lv_obj_t *tile)
{
    ESP_LOGI(TAG, "Initializing accelerometer tile resources");
    // Sensor initialization is done at the ui_manager level
    // This tile just provides the chart interface
    return ESP_OK;
}

lv_obj_t* tile_accelerometer_get_chart(void)
{
    return accel_chart;
}

void tile_accelerometer_get_series(lv_chart_series_t **series_x, 
                                   lv_chart_series_t **series_y,
                                   lv_chart_series_t **series_z)
{
    if (series_x) *series_x = accel_series_x;
    if (series_y) *series_y = accel_series_y;
    if (series_z) *series_z = accel_series_z;
}