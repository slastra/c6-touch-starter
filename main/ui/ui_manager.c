// Main headers
#include "ui_manager.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "../app_config.h"
#include "../led_manager.h"
#include "../power/power_manager.h"

// Tile modules
#include "tiles/tile_interface.h"
#include "tiles/tile_main.h"
#include "tiles/tile_settings.h"
#include "tiles/tile_updates.h"
#include "tiles/tile_info.h"
#include "tiles/tile_accelerometer.h"
#include "tiles/tile_gyroscope.h"
#include "tiles/tile_barcode.h"
#include "tiles/tile_led.h"

// ESP-IDF components
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include <math.h>
#include "driver/gpio.h"
#include "esp_sleep.h"

// BSP (Board Support Package) components
#include "bsp_display.h"
#include "bsp_touch.h"
#include "bsp_spi.h"
#include "bsp_i2c.h"
#include "bsp_qmi8658.h"
#include "bsp_battery.h"

// LVGL
#include "lvgl.h"

static const char *TAG = "ui_manager";

// GPIO pin definitions - reserved for future use

// LVGL display and touch handles
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

// Main screen reference
static lv_obj_t *main_screen = NULL;

// Tileview and tiles
static lv_obj_t *tileview = NULL;
static lv_obj_t *tile_settings = NULL;
static lv_obj_t *tile_main = NULL;
static lv_obj_t *tile_updates = NULL;
static lv_obj_t *tile_info = NULL;
static lv_obj_t *tile_accel = NULL;
static lv_obj_t *tile_gyro = NULL;
static lv_obj_t *tile_barcode = NULL;
static lv_obj_t *tile_led = NULL;

// LED power management state
static uint8_t led_user_r = 0, led_user_g = 0, led_user_b = 0;  // User's LED RGB values
static bool led_was_on = false;  // Track if LED was on before dimming/off

// Active tile tracking for sensor optimization
static lv_obj_t *current_active_tile = NULL;

// Sensor management
static lv_timer_t *sensor_timer = NULL;

// Universal Touch Detection System
static TaskHandle_t touch_task_handle = NULL;
static esp_lcd_touch_handle_t global_touch_handle = NULL;
static bool touch_interrupt_triggered = false;

// System handles

// Hardware handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static i2c_master_bus_handle_t i2c_bus_handle = NULL;

// Forward declarations
static void tileview_scroll_event_cb(lv_event_t *e);
static void ui_power_state_callback(power_state_t old_state, power_state_t new_state);

// Universal Touch Interrupt Handler
static void IRAM_ATTR universal_touch_interrupt_handler(esp_lcd_touch_handle_t tp)
{
    // Set flag to indicate touch was detected
    touch_interrupt_triggered = true;
    
    // Notify the touch task if it exists
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (touch_task_handle != NULL) {
        xTaskNotifyFromISR(touch_task_handle, 1, eSetBits, &xHigherPriorityTaskWoken);
    }
    
    // Yield if a higher priority task was woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Universal Touch Detection Task - handles touch interrupt notifications
static void universal_touch_detect_task(void *arg)
{
    touch_task_handle = xTaskGetCurrentTaskHandle();
    uint32_t notification_value;
    
    ESP_LOGI(TAG, "Universal touch detection task started - waiting for touch interrupts");
    
    while (1) {
        // Wait for touch interrupt notification
        if (xTaskNotifyWait(0, ULONG_MAX, &notification_value, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Universal touch interrupt detected!");
            
            // Reset activity timer to wake up the display
            power_manager_reset_activity(WAKE_SOURCE_TOUCH);
            
            // Clear the interrupt flag
            touch_interrupt_triggered = false;
            
            // Optional: Small delay to debounce multiple rapid touches
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

// Initialize Universal Touch Detection System
static esp_err_t init_universal_touch_detection(esp_lcd_touch_handle_t touch_handle)
{
    if (touch_handle == NULL) {
        ESP_LOGE(TAG, "Touch handle is NULL - cannot initialize universal touch detection");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Store the global touch handle
    global_touch_handle = touch_handle;
    
    // Register the universal touch interrupt callback
    esp_err_t ret = esp_lcd_touch_register_interrupt_callback(touch_handle, universal_touch_interrupt_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register touch interrupt callback: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create universal touch detection task
    BaseType_t task_ret = xTaskCreate(
        universal_touch_detect_task,
        "universal_touch",
        4096,
        NULL,
        6,  // Higher priority than UI tasks
        &touch_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create universal touch detection task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Universal touch detection system initialized successfully");
    return ESP_OK;
}




// Power state callback to handle UI updates on power state changes
static void ui_power_state_callback(power_state_t old_state, power_state_t new_state)
{
    ESP_LOGI(TAG, "Power state changed: %s -> %s", 
             old_state == POWER_STATE_ACTIVE ? "Active" :
             old_state == POWER_STATE_DIM ? "Dim" :
             old_state == POWER_STATE_OFF ? "Off" : "Sleep",
             new_state == POWER_STATE_ACTIVE ? "Active" :
             new_state == POWER_STATE_DIM ? "Dim" :
             new_state == POWER_STATE_OFF ? "Off" : "Sleep");
    
    // Handle UI-specific actions based on power state
    switch (new_state) {
        case POWER_STATE_OFF:
            // Turn LED off only if it was enabled by user
            led_was_on = tile_led_is_enabled();
            if (led_was_on) {
                led_manager_get_rgb(&led_user_r, &led_user_g, &led_user_b);
                led_manager_set_brightness_scale(0);  // Turn LED off
                ESP_LOGI(TAG, "LED turned off with screen");
            }
            
            // Pause UI timers when screen turns off for power saving
            if (sensor_timer) {
                lv_timer_pause(sensor_timer);
                ESP_LOGI(TAG, "Sensor timer paused (200ms -> paused)");
            }
            
            // Slow down battery timer when screen is off
            lv_timer_t *battery_timer_off = tile_settings_get_battery_timer();
            if (battery_timer_off) {
                lv_timer_set_period(battery_timer_off, 5000);  // 1s -> 5s
                ESP_LOGI(TAG, "Battery timer slowed (1s -> 5s)");
            }
            
            ESP_LOGI(TAG, "System ready for automatic light sleep - touch/barcode/timer wake enabled");
            break;
            
        case POWER_STATE_DIM:
            // Dim LED to LED_DIM_PERCENTAGE brightness only if it was enabled by user
            led_was_on = tile_led_is_enabled();
            if (led_was_on) {
                led_manager_get_rgb(&led_user_r, &led_user_g, &led_user_b);
                led_manager_set_brightness_scale(LED_DIM_PERCENTAGE);  // Dim LED
                ESP_LOGI(TAG, "LED dimmed to %d%% with screen", LED_DIM_PERCENTAGE);
            }
            
            // Keep all timers running in dim state, but log the change
            ESP_LOGI(TAG, "Dim state - all timers remain active");
            break;
            
        case POWER_STATE_ACTIVE:
            // Resume UI timers when screen becomes active
            if (old_state == POWER_STATE_OFF) {
                if (sensor_timer) {
                    lv_timer_resume(sensor_timer);
                    ESP_LOGI(TAG, "Sensor timer resumed (paused -> 200ms)");
                }
                
                // Restore battery timer to normal speed
                lv_timer_t *battery_timer_active = tile_settings_get_battery_timer();
                if (battery_timer_active) {
                    lv_timer_set_period(battery_timer_active, 1000);  // 5s -> 1s
                    ESP_LOGI(TAG, "Battery timer restored (5s -> 1s)");
                }
            }
            
            // Restore LED to user's RGB values if it was on
            if (led_was_on) {
                led_manager_set_brightness_scale(100);  // Restore to full brightness
                led_manager_get_rgb(&led_user_r, &led_user_g, &led_user_b);
                ESP_LOGI(TAG, "LED restored to user RGB(%d,%d,%d)", led_user_r, led_user_g, led_user_b);
            }
            
            // Update brightness slider to reflect current setting
            lv_obj_t *slider = tile_settings_get_brightness_slider();
            lv_obj_t *label = tile_settings_get_brightness_label();
            if (slider != NULL) {
                uint8_t brightness = power_manager_get_user_brightness();
                lv_slider_set_value(slider, brightness, LV_ANIM_OFF);
            }
            if (label != NULL) {
                uint8_t brightness = power_manager_get_user_brightness();
                lv_label_set_text_fmt(label, "Brightness: %d%%", brightness);
            }
            
            ESP_LOGI(TAG, "Active state - all timers at normal speed");
            break;
            
        case POWER_STATE_SLEEP:
            // Pause all non-essential timers for deep power saving
            if (sensor_timer) {
                lv_timer_pause(sensor_timer);
                ESP_LOGI(TAG, "Sensor timer paused for sleep mode");
            }
            
            lv_timer_t *battery_timer_sleep = tile_settings_get_battery_timer();
            if (battery_timer_sleep) {
                lv_timer_pause(battery_timer_sleep);
                ESP_LOGI(TAG, "Battery timer paused for sleep mode");
            }
            
            ESP_LOGI(TAG, "Sleep state - entering light sleep mode");
            break;
    }
}

/**
 * @brief Initialize LVGL port with display and touch drivers
 * 
 * Sets up SPI, display panel, I2C, touch controller, and LVGL port.
 * Configures display with proper resolution and touch input.
 * 
 * @return ESP_OK on success, ESP_FAIL on initialization failure
 */
esp_err_t ui_manager_init_lvgl_port(void)
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    bsp_spi_init();
    
    ESP_LOGI(TAG, "Install panel IO");
    bsp_display_init(&io_handle, &panel_handle, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT);
    
    ESP_LOGI(TAG, "Initialize touch");
    i2c_bus_handle = bsp_i2c_init();
    bsp_touch_init(&touch_handle, i2c_bus_handle, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, 0);
    
    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,       /* LVGL task priority */
        .task_stack = 1024 * 10,  /* LVGL task stack size */
        .task_affinity = -1,      /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5      /* LVGL timer tick period in ms */
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));
    
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = true,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        }
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    
    // Initialize universal touch detection system with the touch handle
    if (init_universal_touch_detection(touch_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize universal touch detection system");
    }
    
    // Initialize display brightness (start at normal brightness)
    bsp_display_brightness_init();
    bsp_display_set_brightness(80);
    
    return ESP_OK;
}






void global_touch_event_cb(lv_event_t *e)
{
    // Touch events wake the device by resetting activity timer
    power_manager_reset_activity(WAKE_SOURCE_TOUCH);
    
    // Optional: Log touch events for debugging
    lv_event_code_t code = lv_event_get_code(e);
    ESP_LOGD(TAG, "Touch event: code=%d - activity reset", code);
}

// Tile change event callback for optimization logging
static void tileview_scroll_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_SCROLL_END) {
        lv_obj_t *active_tile = lv_tileview_get_tile_act(tileview);
        
        // Log tile changes for optimization tracking
        const char* tile_name = "Unknown";
        if (active_tile == tile_settings) tile_name = "Settings";
        else if (active_tile == tile_main) tile_name = "Main";
        else if (active_tile == tile_updates) tile_name = "Updates";
        else if (active_tile == tile_info) tile_name = "Info";
        else if (active_tile == tile_accel) tile_name = "Accelerometer";
        else if (active_tile == tile_gyro) tile_name = "Gyroscope";
        
        ESP_LOGI(TAG, "Switched to %s tile - sensor optimization active", tile_name);
        
        // Tile changes no longer reset activity - motion detection handles wake
    }
}

// Sensor update callback for chart data

static void sensor_update_timer_callback(lv_timer_t *timer)
{
    // Get current active tile to optimize sensor reading
    lv_obj_t *active_tile = lv_tileview_get_tile_act(tileview);
    current_active_tile = active_tile;
    
    // Only read sensor data when viewing sensor tiles or settings tile
    bool should_read_sensors = (active_tile == tile_accel || active_tile == tile_gyro || active_tile == tile_settings);
    
    if (!should_read_sensors) {
        // Skip sensor reading when not on sensor tiles
        static uint8_t skip_counter = 0;
        if (++skip_counter >= 25) {  // Log every 5 seconds (200ms * 25)
            ESP_LOGI(TAG, "Skipping sensor reads - not viewing sensor tiles (optimization active)");
            skip_counter = 0;
        }
        return;
    }
    
    qmi8658_data_t sensor_data;
    
    // Read sensor data
    if (bsp_qmi8658_read_data(&sensor_data)) {
        // Convert raw values to physical units
        float acc_x = sensor_data.acc_x / 16384.0f;
        float acc_y = sensor_data.acc_y / 16384.0f;
        float acc_z = sensor_data.acc_z / 16384.0f;
        float gyr_x = sensor_data.gyr_x / 16.4f;
        float gyr_y = sensor_data.gyr_y / 16.4f;
        float gyr_z = sensor_data.gyr_z / 16.4f;
        
        // Update accelerometer chart data only when viewing accelerometer tile
        if (active_tile == tile_accel) {
            lv_chart_series_t *series_x, *series_y, *series_z;
            tile_accelerometer_get_series(&series_x, &series_y, &series_z);
            lv_obj_t *chart = tile_accelerometer_get_chart();
            if (chart != NULL && series_x != NULL) {
                lv_chart_set_next_value(chart, series_x, (int32_t)(acc_x * 1000));
                lv_chart_set_next_value(chart, series_y, (int32_t)(acc_y * 1000));
                lv_chart_set_next_value(chart, series_z, (int32_t)(acc_z * 1000));
            }
        }
        
        // Update gyroscope chart data only when viewing gyroscope tile
        if (active_tile == tile_gyro) {
            lv_chart_series_t *series_x, *series_y, *series_z;
            tile_gyroscope_get_series(&series_x, &series_y, &series_z);
            lv_obj_t *chart = tile_gyroscope_get_chart();
            if (chart != NULL && series_x != NULL) {
                lv_chart_set_next_value(chart, series_x, (int32_t)gyr_x);
                lv_chart_set_next_value(chart, series_y, (int32_t)gyr_y);
                lv_chart_set_next_value(chart, series_z, (int32_t)gyr_z);
            }
        }
        
        // Update temperature in status table only when viewing settings tile
        if (active_tile == tile_settings) {
            lv_obj_t *table = tile_settings_get_status_table();
            if (table != NULL) {
                lv_table_set_cell_value_fmt(table, 2, 1, "%.1f°C", sensor_data.temp);
            }
        }
    }
}


esp_err_t ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI Manager");
    return ui_manager_init_lvgl_port();
}

/**
 * @brief Create the main tileview UI with all tiles and initialize resources
 * 
 * Creates the main screen container, tileview with 6 tiles, initializes
 * sensors, power management, and sets up touch event handlers. This is the
 * main UI creation function called after LVGL port initialization.
 */
void ui_manager_create_main_screen(void)
{
    ESP_LOGI(TAG, "Creating tileview UI with swipeable screens");
    
    // Lock LVGL for thread safety
    if (lvgl_port_lock(0)) {
        ESP_LOGI(TAG, "LVGL locked successfully, creating UI objects...");
        
        // Use the active screen directly instead of creating a new one
        main_screen = lv_scr_act();
        if (main_screen == NULL) {
            ESP_LOGE(TAG, "Failed to get active screen!");
            lvgl_port_unlock();
            return;
        }
        ESP_LOGI(TAG, "Using active screen for UI");
        
        // Create tileview on the active screen
        tileview = lv_tileview_create(main_screen);
        lv_obj_set_size(tileview, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
        lv_obj_set_pos(tileview, 0, 0);
        
        // Style the tileview indicator bar with primary red color
        lv_obj_set_style_bg_color(tileview, lv_color_hex(PRIMARY_RED), LV_PART_INDICATOR);
        
        // Add tiles with horizontal swipe navigation
        // Settings (0,0), Main (1,0), Updates (2,0), Info (3,0), Accelerometer (4,0), Gyroscope (5,0), Barcode (6,0), LED (7,0)
        tile_settings = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
        tile_main = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
        tile_updates = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
        tile_info = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
        tile_accel = lv_tileview_add_tile(tileview, 4, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
        tile_gyro = lv_tileview_add_tile(tileview, 5, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
        tile_barcode = lv_tileview_add_tile(tileview, 6, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
        tile_led = lv_tileview_add_tile(tileview, 7, 0, LV_DIR_LEFT);
        
        // Create content for each tile using new tile modules
        tile_settings_create(tile_settings);
        tile_main_create(tile_main);
        tile_updates_create(tile_updates);
        tile_info_create(tile_info);
        tile_accelerometer_create(tile_accel);
        tile_gyroscope_create(tile_gyro);
        tile_barcode_create(tile_barcode);
        tile_led_create(tile_led);
        
        // Initialize tile-specific resources
        tile_settings_init(tile_settings);
        tile_accelerometer_init(tile_accel);
        tile_gyroscope_init(tile_gyro);
        tile_barcode_init(tile_barcode);
        tile_led_init(tile_led);
        
        // Initialize QMI8658 sensor for normal operation (accelerometer and gyroscope)
        if (!bsp_qmi8658_init(i2c_bus_handle)) {
            ESP_LOGE(TAG, "Failed to initialize QMI8658 sensor");
            // Continue without sensor - UI will show default values
        } else {
            ESP_LOGI(TAG, "QMI8658 sensor initialized successfully");
        }
        
        // Create sensor timer to read data every 200ms for chart updates
        sensor_timer = lv_timer_create(sensor_update_timer_callback, 200, NULL);
        ESP_LOGI(TAG, "Sensor timer created - updating charts every 200ms");
        
        // Initialize new power management system
        power_config_t pm_config = {
            .dim_timeout_ms = SCREEN_DIM_START_MS,
            .off_timeout_ms = SCREEN_OFF_TIMEOUT_MS,
            .sleep_timeout_ms = LIGHT_SLEEP_TIMEOUT_MS,
            .dim_brightness = DIM_TARGET_BRIGHTNESS,
            .user_brightness = 80,
            .enable_fade = true,
            .fade_duration_ms = 1000
        };
        
        if (power_manager_init(&pm_config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize power management system");
        } else {
            // Register callback for power state changes
            if (power_manager_register_state_callback(ui_power_state_callback) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to register power state callback");
            }
            
            // Configure light sleep wake sources
            if (power_manager_configure_light_sleep() != ESP_OK) {
                ESP_LOGE(TAG, "Failed to configure light sleep wake sources");
            }
            
            ESP_LOGI(TAG, "Power management system initialized with light sleep configuration");
        }
        
        // Add comprehensive touch event handlers for wake detection
        // Multiple event types for reliable detection
        lv_obj_add_event_cb(tileview, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(tileview, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(tileview, global_touch_event_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(tileview, global_touch_event_cb, LV_EVENT_CLICKED, NULL);
        
        // Add tile change event handler for optimization tracking
        lv_obj_add_event_cb(tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_END, NULL);
        
        lv_obj_add_event_cb(main_screen, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(main_screen, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(main_screen, global_touch_event_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(main_screen, global_touch_event_cb, LV_EVENT_CLICKED, NULL);
        
        // Add touch handlers to all individual tiles for comprehensive coverage
        lv_obj_add_event_cb(tile_settings, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(tile_settings, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(tile_main, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(tile_main, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(tile_updates, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(tile_updates, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(tile_info, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(tile_info, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(tile_accel, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(tile_accel, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(tile_gyro, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(tile_gyro, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(tile_barcode, global_touch_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(tile_barcode, global_touch_event_cb, LV_EVENT_RELEASED, NULL);
        
        // Touch handlers for interactive elements are now handled by individual tiles
        
        ESP_LOGI(TAG, "Power management initialized via power_manager module");
        ESP_LOGI(TAG, "Touch event handlers registered on all tiles and interactive elements");
        ESP_LOGI(TAG, "Sensor optimization enabled - sensors only read when viewing relevant tiles");
        
        // Start on the main tile (center) by scrolling to x=172 (1 screen width)
        lv_obj_scroll_to_x(tileview, EXAMPLE_LCD_H_RES, LV_ANIM_OFF);
        
        ESP_LOGI(TAG, "Tileview UI created with 8 swipeable screens: %dx%d", EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
        
        // Unlock LVGL 
        lvgl_port_unlock();
        
        
        ESP_LOGI(TAG, "UI creation complete, display at 80%% brightness");
    }
}

lv_obj_t* ui_manager_get_main_screen(void)
{
    return main_screen;
}

void ui_manager_reset_activity(void)
{
    // Legacy function - power management now handles activity via power_manager
    power_manager_reset_activity(WAKE_SOURCE_API);
}

void ui_manager_switch_to_tile(int tile_index)
{
    if (tileview) {
        int tile_x = tile_index * EXAMPLE_LCD_H_RES;
        lv_obj_scroll_to_x(tileview, tile_x, LV_ANIM_ON);
        ESP_LOGI(TAG, "Switched to tile %d", tile_index);
        
        // Activity is now handled by motion detection only
    } else {
        ESP_LOGW(TAG, "Cannot switch to tile %d - tileview not initialized", tile_index);
    }
}

void ui_manager_switch_to_settings_tile(void)
{
    ui_manager_switch_to_tile(0);
}

void ui_manager_switch_to_main_tile(void)
{
    ui_manager_switch_to_tile(1);
}

void ui_manager_switch_to_updates_tile(void)
{
    ui_manager_switch_to_tile(2);
}

void ui_manager_switch_to_info_tile(void)
{
    ui_manager_switch_to_tile(3);
}

void ui_manager_switch_to_accelerometer_tile(void)
{
    ui_manager_switch_to_tile(4);
}

void ui_manager_switch_to_gyroscope_tile(void)
{
    ui_manager_switch_to_tile(5);
}

void ui_manager_switch_to_barcode_tile(void)
{
    ui_manager_switch_to_tile(6);
}

void ui_manager_switch_to_led_tile(void)
{
    ui_manager_switch_to_tile(7);
}

void ui_manager_cleanup(void)
{
    ESP_LOGI(TAG, "Cleaning up UI Manager");
    // Cleanup would go here if needed
}