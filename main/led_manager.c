#include "led_manager.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "led_manager";

static led_strip_handle_t led_strip = NULL;

// Store current RGB values
static uint8_t current_red = 0;
static uint8_t current_green = 0;
static uint8_t current_blue = 0;

// Store original RGB values before scaling
static uint8_t original_red = 0;
static uint8_t original_green = 0;
static uint8_t original_blue = 0;

esp_err_t led_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WS2812B LED on GPIO %d", LED_GPIO_NUM);
    
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO_NUM,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz
        .flags.with_dma = false,
    };
    
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    led_manager_clear();
    
    ESP_LOGI(TAG, "LED manager initialized successfully");
    return ESP_OK;
}

esp_err_t led_manager_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    if (led_strip == NULL) {
        ESP_LOGW(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Update both current and original values (user's selected values)
    current_red = red;
    current_green = green;
    current_blue = blue;
    
    original_red = red;
    original_green = green;
    original_blue = blue;
    
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, red, green, blue));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    
    ESP_LOGD(TAG, "LED set to RGB(%d, %d, %d)", red, green, blue);
    return ESP_OK;
}

esp_err_t led_manager_get_rgb(uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (red != NULL) *red = original_red;
    if (green != NULL) *green = original_green;
    if (blue != NULL) *blue = original_blue;
    
    return ESP_OK;
}

esp_err_t led_manager_set_brightness_scale(uint8_t percentage)
{
    if (led_strip == NULL) {
        ESP_LOGW(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clamp percentage to 0-100
    if (percentage > 100) percentage = 100;
    
    // Scale the original RGB values by the percentage
    // original_* values remain unchanged (user's selected values)
    // current_* values reflect what's actually displayed
    uint8_t scaled_red = (original_red * percentage) / 100;
    uint8_t scaled_green = (original_green * percentage) / 100;
    uint8_t scaled_blue = (original_blue * percentage) / 100;
    
    // Update only current values (displayed values)
    current_red = scaled_red;
    current_green = scaled_green;
    current_blue = scaled_blue;
    
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, scaled_red, scaled_green, scaled_blue));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    
    ESP_LOGD(TAG, "LED brightness scaled to %d%% - RGB(%d,%d,%d) from original RGB(%d,%d,%d)", 
             percentage, scaled_red, scaled_green, scaled_blue, 
             original_red, original_green, original_blue);
    
    return ESP_OK;
}

esp_err_t led_manager_clear(void)
{
    if (led_strip == NULL) {
        ESP_LOGW(TAG, "LED strip not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    current_red = 0;
    current_green = 0;
    current_blue = 0;
    
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    
    ESP_LOGD(TAG, "LED cleared");
    return ESP_OK;
}

void led_manager_deinit(void)
{
    if (led_strip != NULL) {
        led_strip_clear(led_strip);
        led_strip_del(led_strip);
        led_strip = NULL;
        ESP_LOGI(TAG, "LED manager deinitialized");
    }
}