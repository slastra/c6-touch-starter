#include "power_manager.h"
#include "../app_config.h"
#include "display_power.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

static const char *TAG = "power_manager";

// Maximum number of state change callbacks
#define MAX_CALLBACKS 5

// Power management context
typedef struct {
    power_state_t current_state;
    power_state_t previous_state;
    int64_t last_activity_us;
    power_config_t config;
    
    // Thread safety
    SemaphoreHandle_t state_mutex;
    
    // FreeRTOS timer for state management
    TimerHandle_t state_timer;
    
    // State change callbacks
    power_state_cb_t callbacks[MAX_CALLBACKS];
    uint8_t callback_count;
    
    // Initialization flag
    bool initialized;
} power_context_t;

static power_context_t ctx = {0};

// Forward declarations
static void power_timer_callback(TimerHandle_t timer);
static esp_err_t transition_to_state(power_state_t new_state);
static const char* state_to_string(power_state_t state);

const char* wake_source_to_string(wake_source_t source) {
    switch (source) {
        case WAKE_SOURCE_TOUCH: return "Touch";
        case WAKE_SOURCE_BUTTON: return "Button";
        case WAKE_SOURCE_MOTION: return "Motion";
        case WAKE_SOURCE_WOM: return "WoM";
        case WAKE_SOURCE_TIMER: return "Timer";
        case WAKE_SOURCE_API: return "API";
        default: return "Unknown";
    }
}

esp_err_t power_manager_init(const power_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ctx.initialized) {
        ESP_LOGW(TAG, "Power manager already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing power management system");
    
    // Copy configuration
    ctx.config = *config;
    ctx.current_state = POWER_STATE_ACTIVE;
    ctx.previous_state = POWER_STATE_ACTIVE;
    ctx.last_activity_us = esp_timer_get_time();
    ctx.callback_count = 0;
    
    // Create mutex for thread safety
    ctx.state_mutex = xSemaphoreCreateMutex();
    if (ctx.state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Create FreeRTOS timer for power state management
    ctx.state_timer = xTimerCreate(
        "power_timer",
        pdMS_TO_TICKS(1000),  // 1 second period
        pdTRUE,               // Auto-reload
        NULL,                 // Timer ID (unused)
        power_timer_callback  // Callback function
    );
    
    if (ctx.state_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create power timer");
        vSemaphoreDelete(ctx.state_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize display power subsystem
    esp_err_t ret = display_power_init(config->user_brightness, config->enable_fade);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display power: %s", esp_err_to_name(ret));
        vSemaphoreDelete(ctx.state_mutex);
        xTimerDelete(ctx.state_timer, 0);
        return ret;
    }
    
    // Start the power timer
    if (xTimerStart(ctx.state_timer, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start power timer");
        display_power_deinit();
        vSemaphoreDelete(ctx.state_mutex);
        xTimerDelete(ctx.state_timer, 0);
        return ESP_FAIL;
    }
    
    ctx.initialized = true;
    
    ESP_LOGI(TAG, "Power manager initialized: dim=%ums, off=%ums, brightness=%u%%",
             (unsigned)config->dim_timeout_ms,
             (unsigned)config->off_timeout_ms,
             config->user_brightness);
    
    return ESP_OK;
}

esp_err_t power_manager_register_state_callback(power_state_cb_t callback)
{
    if (!ctx.initialized) {
        ESP_LOGE(TAG, "Power manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (callback == NULL) {
        ESP_LOGE(TAG, "Callback cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(ctx.state_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for callback registration");
        return ESP_ERR_TIMEOUT;
    }
    
    if (ctx.callback_count >= MAX_CALLBACKS) {
        xSemaphoreGive(ctx.state_mutex);
        ESP_LOGE(TAG, "Maximum number of callbacks reached");
        return ESP_ERR_NO_MEM;
    }
    
    ctx.callbacks[ctx.callback_count++] = callback;
    xSemaphoreGive(ctx.state_mutex);
    
    ESP_LOGI(TAG, "Registered state callback #%d", ctx.callback_count);
    return ESP_OK;
}

void power_manager_reset_activity(wake_source_t source)
{
    if (!ctx.initialized) {
        return;
    }
    
    int64_t now = esp_timer_get_time();
    int64_t old_time = ctx.last_activity_us;
    ctx.last_activity_us = now;
    
    ESP_LOGI(TAG, "Activity reset by %s (idle was %.1fs)",
             wake_source_to_string(source),
             (now - old_time) / 1000000.0);
    
    // If we were dimmed or off, transition back to active
    if (ctx.current_state != POWER_STATE_ACTIVE) {
        transition_to_state(POWER_STATE_ACTIVE);
    }
}

power_state_t power_manager_get_state(void)
{
    return ctx.current_state;
}

esp_err_t power_manager_set_user_brightness(uint8_t brightness)
{
    if (!ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (brightness > 100) {
        ESP_LOGW(TAG, "Brightness clamped to 100%% (was %u%%)", brightness);
        brightness = 100;
    }
    
    ctx.config.user_brightness = brightness;
    
    // Update display brightness if currently active
    if (ctx.current_state == POWER_STATE_ACTIVE) {
        esp_err_t ret = display_power_set_brightness(brightness, ctx.config.enable_fade);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update brightness: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "User brightness set to %u%%", brightness);
    return ESP_OK;
}

uint8_t power_manager_get_user_brightness(void)
{
    return ctx.config.user_brightness;
}

uint32_t power_manager_get_idle_time_ms(void)
{
    if (!ctx.initialized) {
        return 0;
    }
    
    int64_t now = esp_timer_get_time();
    return (uint32_t)((now - ctx.last_activity_us) / 1000);
}

esp_err_t power_manager_force_state(power_state_t state)
{
    if (!ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGW(TAG, "Forcing state transition to %s", state_to_string(state));
    return transition_to_state(state);
}

void power_manager_deinit(void)
{
    if (!ctx.initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing power manager");
    
    // Stop and delete timer
    if (ctx.state_timer) {
        xTimerStop(ctx.state_timer, pdMS_TO_TICKS(100));
        xTimerDelete(ctx.state_timer, pdMS_TO_TICKS(100));
        ctx.state_timer = NULL;
    }
    
    // Cleanup display power
    display_power_deinit();
    
    // Delete mutex
    if (ctx.state_mutex) {
        vSemaphoreDelete(ctx.state_mutex);
        ctx.state_mutex = NULL;
    }
    
    // Reset context
    memset(&ctx, 0, sizeof(ctx));
}

// Private functions

static esp_err_t transition_to_state(power_state_t new_state)
{
    if (xSemaphoreTake(ctx.state_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for state transition");
        return ESP_ERR_TIMEOUT;
    }
    
    power_state_t old_state = ctx.current_state;
    
    if (old_state == new_state) {
        xSemaphoreGive(ctx.state_mutex);
        return ESP_OK; // No change needed
    }
    
    ESP_LOGI(TAG, "State transition: %s -> %s", 
             state_to_string(old_state), state_to_string(new_state));
    
    // Update state
    ctx.previous_state = old_state;
    ctx.current_state = new_state;
    
    // Handle display changes based on new state
    esp_err_t ret = ESP_OK;
    switch (new_state) {
        case POWER_STATE_ACTIVE:
            ret = display_power_set_brightness(ctx.config.user_brightness, ctx.config.enable_fade);
            break;
        case POWER_STATE_DIM:
            ret = display_power_set_brightness(ctx.config.dim_brightness, ctx.config.enable_fade);
            break;
        case POWER_STATE_OFF:
            ret = display_power_set_brightness(0, ctx.config.enable_fade);
            break;
        case POWER_STATE_SLEEP:
            ret = display_power_set_brightness(0, false); // Immediate off before sleep
            break;
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update display for state %s: %s", 
                 state_to_string(new_state), esp_err_to_name(ret));
    }
    
    // Notify all registered callbacks
    for (uint8_t i = 0; i < ctx.callback_count; i++) {
        if (ctx.callbacks[i]) {
            ctx.callbacks[i](old_state, new_state);
        }
    }
    
    xSemaphoreGive(ctx.state_mutex);
    return ret;
}

static void power_timer_callback(TimerHandle_t timer)
{
    if (!ctx.initialized) {
        return;
    }
    
    uint32_t idle_time_ms = power_manager_get_idle_time_ms();
    
    // State machine logic
    power_state_t target_state = ctx.current_state;
    
    if (idle_time_ms >= ctx.config.off_timeout_ms) {
        target_state = POWER_STATE_OFF;
    } else if (idle_time_ms >= ctx.config.dim_timeout_ms) {
        target_state = POWER_STATE_DIM;
    } else {
        target_state = POWER_STATE_ACTIVE;
    }
    
    // Transition if state should change
    if (target_state != ctx.current_state) {
        transition_to_state(target_state);
    }
    
    // Periodic logging (every 10 seconds)
    static uint8_t log_counter = 0;
    if (++log_counter >= 10) {
        ESP_LOGI(TAG, "State: %s, Idle: %.1fs", 
                 state_to_string(ctx.current_state), idle_time_ms / 1000.0);
        log_counter = 0;
    }
}

static const char* state_to_string(power_state_t state)
{
    switch (state) {
        case POWER_STATE_ACTIVE: return "Active";
        case POWER_STATE_DIM: return "Dim";
        case POWER_STATE_OFF: return "Off";
        case POWER_STATE_SLEEP: return "Sleep";
        default: return "Unknown";
    }
}

// Sleep configuration functions

static esp_err_t configure_sleep_wake_sources(void)
{
    ESP_LOGI(TAG, "Configuring sleep wake sources");
    
    // Configure timer wake source (wake every 100ms for system checks)
    esp_sleep_enable_timer_wakeup(100000);  // 100ms in microseconds
    ESP_LOGI(TAG, "Timer wake source configured: 100ms");
    
    // Configure GPIO wake source for WoM interrupt (GPIO9)
    // GPIO9 is connected to QMI8658 INT2 pin for Wake on Motion
    gpio_wakeup_enable(GPIO_NUM_9, GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    ESP_LOGI(TAG, "GPIO9 wake source configured for WoM interrupt");
    
    // Configure UART wake source for barcode scanner
    // UART0 is used for console, UART1 for barcode scanner
    esp_sleep_enable_uart_wakeup(1);  // UART1 for barcode scanner
    ESP_LOGI(TAG, "UART1 wake source configured for barcode scanner");
    
    return ESP_OK;
}

esp_err_t power_manager_configure_light_sleep(void)
{
    if (!ctx.initialized) {
        ESP_LOGE(TAG, "Power manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Configure wake sources for light sleep
    esp_err_t ret = configure_sleep_wake_sources();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure sleep wake sources: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Light sleep configuration complete");
    return ESP_OK;
}

esp_err_t power_manager_enter_manual_sleep(uint32_t duration_ms)
{
    if (!ctx.initialized) {
        ESP_LOGE(TAG, "Power manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Entering manual light sleep for %ums", (unsigned)duration_ms);
    
    // Configure wake sources
    esp_sleep_enable_timer_wakeup(duration_ms * 1000);  // Convert ms to microseconds
    esp_sleep_enable_uart_wakeup(1);  // Barcode scanner
    
    // Enter light sleep
    esp_err_t ret = esp_light_sleep_start();
    
    // Check wake cause after wake-up
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    const char* wake_cause_str = "Unknown";
    
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            wake_cause_str = "Timer";
            break;
        case ESP_SLEEP_WAKEUP_UART:
            wake_cause_str = "UART";
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            wake_cause_str = "GPIO";
            break;
        default:
            wake_cause_str = "Other/Unknown";
            break;
    }
    
    ESP_LOGI(TAG, "Woke from light sleep, cause: %s, result: %s", 
             wake_cause_str, esp_err_to_name(ret));
    
    // Reset activity after manual sleep
    power_manager_reset_activity(WAKE_SOURCE_TIMER);
    
    return ret;
}

const char* power_manager_get_wake_cause_string(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    switch (cause) {
        case ESP_SLEEP_WAKEUP_UNDEFINED: return "Undefined (normal boot)";
        case ESP_SLEEP_WAKEUP_ALL: return "All wake sources";
        case ESP_SLEEP_WAKEUP_EXT0: return "External (RTC_IO)";
        case ESP_SLEEP_WAKEUP_EXT1: return "External (RTC_CNTL)";
        case ESP_SLEEP_WAKEUP_TIMER: return "Timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "Touchpad";
        case ESP_SLEEP_WAKEUP_ULP: return "ULP program";
        case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
        case ESP_SLEEP_WAKEUP_UART: return "UART";
        case ESP_SLEEP_WAKEUP_WIFI: return "WiFi";
        case ESP_SLEEP_WAKEUP_COCPU: return "COCPU";
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG: return "COCPU trap trigger";
        case ESP_SLEEP_WAKEUP_BT: return "Bluetooth";
        default: return "Unknown";
    }
}