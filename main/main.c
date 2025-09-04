#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_pm.h"
#include "mdns.h"

// Application modules
#include "app_config.h"
#include "ui/ui_manager.h"
#include "ui/ui_components.h"
#include "network/wifi_manager.h"
#include "network/ota_manager.h"
#include "network/mqtt_barcode.h"
#include "led_manager.h"

static const char *TAG = "c6_touch_starter";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-C6 Touch Starter - Firmware %s", FIRMWARE_VERSION);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize LED Manager
    ESP_LOGI(TAG, "Initializing LED manager...");
    ESP_ERROR_CHECK(led_manager_init());
    
    // Initialize UI Manager
    ESP_LOGI(TAG, "Initializing UI...");
    ESP_ERROR_CHECK(ui_manager_init());
    ui_manager_create_main_screen();

    // Initialize Network Managers
    ESP_LOGI(TAG, "Initializing WiFi...");
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(ota_manager_init());

    // Set up callback connections between modules
    wifi_manager_set_status_callback(ui_components_update_wifi_status);
    
    ota_manager_set_status_callback(ui_components_update_ota_status);
    ota_manager_set_progress_callback(ui_components_update_ota_progress);
    ota_manager_set_progress_bar_show_callback(ui_components_show_progress_bar);
    ota_manager_set_progress_bar_hide_callback(ui_components_hide_progress_bar);
    ota_manager_set_progress_bar_set_callback(ui_components_set_progress_value);

    // Start WiFi connection
    ESP_ERROR_CHECK(wifi_manager_start());

    // Initialize mDNS for hostname resolution (following QuietSmart pattern)
    ESP_LOGI(TAG, "Initializing mDNS...");
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("esp32-c6-touch"));
    ESP_ERROR_CHECK(mdns_service_add("ESP32-C6-Touch", "_esp32", "_tcp", 80, NULL, 0));
    ESP_LOGI(TAG, "mDNS initialized - hostname: esp32-c6-touch.local");

    // Initialize MQTT barcode lookup system
    ESP_LOGI(TAG, "Initializing MQTT barcode system...");
    esp_err_t mqtt_err = mqtt_barcode_init();
    if (mqtt_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize MQTT barcode system: %s", esp_err_to_name(mqtt_err));
        ESP_LOGW(TAG, "Barcode resolution will not be available");
    } else {
        ESP_LOGI(TAG, "MQTT barcode system initialized successfully");
    }

    // Enable power management with automatic light sleep
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,        // Maximum CPU frequency for performance
        .min_freq_mhz = 40,         // Lower minimum frequency for power savings
        .light_sleep_enable = true  // Enable automatic light sleep when idle
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    ESP_LOGI(TAG, "Power management configured - automatic light sleep enabled (40-160MHz)");

    ESP_LOGI(TAG, "Application started successfully");
    
    // Subscribe main task to watchdog
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    
    // Main loop - all functionality is now handled by the modules
    while (1) {
        // Feed the watchdog to prevent timeout
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}