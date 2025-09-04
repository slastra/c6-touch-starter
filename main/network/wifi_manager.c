#include "wifi_manager.h"
#include "../app_config.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "../ui/ui_components.h"

static const char *TAG = "wifi_manager";

// WiFi event group and status
static EventGroupHandle_t wifi_event_group;
static bool wifi_connected = false;
static bool wifi_connecting = false;
static wifi_status_callback_t status_callback = NULL;

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi Manager");
    
    wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    ESP_LOGI(TAG, "Starting WiFi connection");
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler, NULL, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Enable WiFi power save mode for light sleep compatibility
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_LOGI(TAG, "WiFi power save mode enabled (MIN_MODEM) for light sleep");
    
    ESP_LOGI(TAG, "WiFi initialization complete. SSID: %s", CONFIG_WIFI_SSID);
    return ESP_OK;
}

void wifi_manager_stop(void)
{
    ESP_LOGI(TAG, "Stopping WiFi");
    esp_wifi_stop();
    wifi_connected = false;
    wifi_connecting = false;
}

bool wifi_manager_is_connected(void)
{
    return wifi_connected;
}

bool wifi_manager_is_connecting(void)
{
    return wifi_connecting;
}

void wifi_manager_set_status_callback(wifi_status_callback_t callback)
{
    status_callback = callback;
}

void wifi_manager_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        wifi_connecting = true;
        wifi_connected = false;
        if (status_callback) {
            status_callback("WiFi: Connecting...", BLACK);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        wifi_connecting = true;
        wifi_connected = false;
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        if (status_callback) {
            status_callback("WiFi: Disconnected", BLACK);
            // Clear IP address from system info tile when disconnected
            ui_components_update_ip_status("Not Connected", BLACK);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_connected = true;
        wifi_connecting = false;
        
        if (status_callback) {
            // Send clean WiFi status to updates tile (without IP address)
            status_callback("Connected", BLACK);
            
            // Send IP address separately to system info tile
            char ip_text[32];
            snprintf(ip_text, sizeof(ip_text), IPSTR, IP2STR(&event->ip_info.ip));
            ui_components_update_ip_status(ip_text, BLACK);
        }
    }
}