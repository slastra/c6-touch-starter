#include "tile_gyroscope.h"
#include "../ui_theme.h"
#include "../../app_config.h"
#include "esp_log.h"
#include "bsp_qmi8658.h"

static const char *TAG = "tile_gyroscope";

// Gyroscope chart widgets
static lv_obj_t *gyro_chart = NULL;
static lv_chart_series_t *gyro_series_x = NULL;
static lv_chart_series_t *gyro_series_y = NULL;
static lv_chart_series_t *gyro_series_z = NULL;

lv_obj_t* tile_gyroscope_create(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating gyroscope tile");
    
    // Gyroscope tile content
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Gyroscope");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y_OFFSET);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, ui_theme_get_title_text_color(), 0);
    
    // Create gyroscope chart (most of the screen)
    gyro_chart = lv_chart_create(parent);
    lv_obj_set_size(gyro_chart, EXAMPLE_LCD_H_RES - 20, 200); // Larger chart
    lv_obj_align(gyro_chart, LV_ALIGN_TOP_MID, 0, 55);
    lv_chart_set_type(gyro_chart, LV_CHART_TYPE_LINE);
    
    // Configure chart properties for gyroscope (±500°/s range)
    lv_chart_set_point_count(gyro_chart, 50);  // 50 points = 10 seconds at 5Hz
    lv_chart_set_range(gyro_chart, LV_CHART_AXIS_PRIMARY_Y, -500, 500);  // ±500°/s range
    lv_chart_set_div_line_count(gyro_chart, 10, 8);  // Grid lines for rotation tracking
    
    // Create data series for X, Y, Z axes with different colors
    gyro_series_x = lv_chart_add_series(gyro_chart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y); // Red for X
    gyro_series_y = lv_chart_add_series(gyro_chart, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y); // Green for Y  
    gyro_series_z = lv_chart_add_series(gyro_chart, lv_color_hex(0x0080FF), LV_CHART_AXIS_PRIMARY_Y); // Blue for Z
    
    // Hide chart markers/dots to show continuous lines only
    lv_obj_set_style_size(gyro_chart, 0, LV_PART_INDICATOR);
    
    // Chart styling - white background with gray grid
    lv_obj_set_style_bg_color(gyro_chart, ui_theme_get_tile_bg_color(), 0);
    lv_obj_set_style_border_color(gyro_chart, ui_theme_get_muted_text_color(), 0);
    lv_obj_set_style_border_width(gyro_chart, 1, 0);
    lv_obj_set_style_line_color(gyro_chart, ui_theme_get_muted_text_color(), LV_PART_ITEMS);
    
    return parent;
}

esp_err_t tile_gyroscope_init(lv_obj_t *tile)
{
    ESP_LOGI(TAG, "Initializing gyroscope tile resources");
    // Sensor initialization is done at the ui_manager level
    // This tile just provides the chart interface
    return ESP_OK;
}

lv_obj_t* tile_gyroscope_get_chart(void)
{
    return gyro_chart;
}

void tile_gyroscope_get_series(lv_chart_series_t **series_x, 
                               lv_chart_series_t **series_y,
                               lv_chart_series_t **series_z)
{
    if (series_x) *series_x = gyro_series_x;
    if (series_y) *series_y = gyro_series_y;
    if (series_z) *series_z = gyro_series_z;
}