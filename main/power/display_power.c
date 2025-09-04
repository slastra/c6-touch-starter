#include "display_power.h"
#include "../app_config.h"
#include "bsp_display.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "display_power";

// Display power context
typedef struct {
    uint8_t current_brightness;
    uint8_t target_brightness;
    bool fade_enabled;
    bool fade_active;
    TimerHandle_t fade_timer;
    bool initialized;
} display_context_t;

static display_context_t ctx = {0};

// Forward declarations
static void fade_timer_callback(TimerHandle_t timer);

esp_err_t display_power_init(uint8_t initial_brightness, bool enable_fade)
{
    if (ctx.initialized) {
        ESP_LOGW(TAG, "Display power already initialized");
        return ESP_OK;
    }
    
    if (initial_brightness > 100) {
        ESP_LOGW(TAG, "Initial brightness clamped to 100%% (was %u%%)", initial_brightness);
        initial_brightness = 100;
    }
    
    ESP_LOGI(TAG, "Initializing display power management");
    
    // Initialize BSP display brightness
    bsp_display_brightness_init();
    
    // Set initial state
    ctx.current_brightness = initial_brightness;
    ctx.target_brightness = initial_brightness;
    ctx.fade_enabled = enable_fade;
    ctx.fade_active = false;
    
    // Create fade timer if fade is enabled
    if (enable_fade) {
        ctx.fade_timer = xTimerCreate(
            "fade_timer",
            pdMS_TO_TICKS(FADE_STEP_MS),  // Period from app_config.h
            pdTRUE,                       // Auto-reload
            NULL,                         // Timer ID (unused)
            fade_timer_callback           // Callback function
        );
        
        if (ctx.fade_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create fade timer");
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Set initial brightness
    bsp_display_set_brightness(initial_brightness);
    
    ctx.initialized = true;
    
    ESP_LOGI(TAG, "Display power initialized: brightness=%u%%, fade=%s",
             initial_brightness, enable_fade ? "enabled" : "disabled");
    
    return ESP_OK;
}

esp_err_t display_power_set_brightness(uint8_t brightness, bool animate)
{
    if (!ctx.initialized) {
        ESP_LOGE(TAG, "Display power not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (brightness > 100) {
        ESP_LOGW(TAG, "Brightness clamped to 100%% (was %u%%)", brightness);
        brightness = 100;
    }
    
    ctx.target_brightness = brightness;
    
    // If animation is disabled or fade is not supported, set immediately
    if (!animate || !ctx.fade_enabled || !ctx.fade_timer) {
        ctx.current_brightness = brightness;
        ctx.fade_active = false;
        
        bsp_display_set_brightness(brightness);
        
        ESP_LOGD(TAG, "Brightness set to %u%% (immediate)", brightness);
        return ESP_OK;
    }
    
    // Start fade animation
    if (ctx.current_brightness != ctx.target_brightness) {
        ctx.fade_active = true;
        
        if (xTimerStart(ctx.fade_timer, pdMS_TO_TICKS(10)) != pdPASS) {
            ESP_LOGE(TAG, "Failed to start fade timer");
            // Fall back to immediate change
            ctx.current_brightness = brightness;
            ctx.fade_active = false;
            bsp_display_set_brightness(brightness);
            return ESP_FAIL;
        }
        
        ESP_LOGD(TAG, "Starting fade: %u%% -> %u%%", ctx.current_brightness, brightness);
    }
    
    return ESP_OK;
}

uint8_t display_power_get_brightness(void)
{
    return ctx.current_brightness;
}

bool display_power_is_fading(void)
{
    return ctx.fade_active;
}

void display_power_stop_fade(void)
{
    if (ctx.fade_timer && ctx.fade_active) {
        xTimerStop(ctx.fade_timer, 0);
        ctx.fade_active = false;
        ESP_LOGD(TAG, "Fade animation stopped");
    }
}

void display_power_deinit(void)
{
    if (!ctx.initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing display power");
    
    // Stop fade timer
    if (ctx.fade_timer) {
        xTimerStop(ctx.fade_timer, pdMS_TO_TICKS(100));
        xTimerDelete(ctx.fade_timer, pdMS_TO_TICKS(100));
        ctx.fade_timer = NULL;
    }
    
    // Reset context
    memset(&ctx, 0, sizeof(ctx));
}

// Private functions

static void fade_timer_callback(TimerHandle_t timer)
{
    if (!ctx.fade_active || ctx.current_brightness == ctx.target_brightness) {
        ctx.fade_active = false;
        xTimerStop(timer, 0);
        ESP_LOGD(TAG, "Fade complete: %u%%", ctx.current_brightness);
        return;
    }
    
    // Calculate next brightness step
    if (ctx.current_brightness < ctx.target_brightness) {
        uint8_t step = FADE_STEP_SIZE;
        if (ctx.current_brightness + step > ctx.target_brightness) {
            step = ctx.target_brightness - ctx.current_brightness;
        }
        ctx.current_brightness += step;
    } else {
        uint8_t step = FADE_STEP_SIZE;
        if (ctx.current_brightness < step || ctx.current_brightness - step < ctx.target_brightness) {
            ctx.current_brightness = ctx.target_brightness;
        } else {
            ctx.current_brightness -= step;
        }
    }
    
    // Apply brightness
    bsp_display_set_brightness(ctx.current_brightness);
}