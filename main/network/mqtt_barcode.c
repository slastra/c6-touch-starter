#include "mqtt_barcode.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>

static const char *TAG = "mqtt_barcode";

// Event bits for connection status
#define MQTT_CONNECTED_BIT    BIT0

// Request tracking structure
typedef struct {
    uint32_t request_id;
    char barcode[32];
    mqtt_barcode_callback_t callback;
    TimerHandle_t timeout_timer;
    bool active;
} barcode_request_t;

// Global state structure
static struct {
    esp_mqtt_client_handle_t client;
    EventGroupHandle_t event_group;
    char client_id[32];
    char response_topic[64];
    barcode_request_t pending_request;
    bool initialized;
    const char* status_message;
} mqtt_state = {0};

// Forward declarations
static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data);
static void handle_barcode_response(const char* data, int len);
static void request_timeout_callback(TimerHandle_t timer);
static uint32_t generate_request_id(void);
static void clear_pending_request(void);

/**
 * Generate unique client ID based on MAC address
 */
static void generate_client_id(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mqtt_state.client_id, sizeof(mqtt_state.client_id), 
             "%s_%02x%02x%02x", MQTT_CLIENT_ID_PREFIX, mac[3], mac[4], mac[5]);
    
    // Create response topic
    snprintf(mqtt_state.response_topic, sizeof(mqtt_state.response_topic),
             "%s/%s", MQTT_BARCODE_RESPONSE_TOPIC, mqtt_state.client_id);
    
    ESP_LOGI(TAG, "Generated client ID: %s", mqtt_state.client_id);
    ESP_LOGI(TAG, "Response topic: %s", mqtt_state.response_topic);
}

/**
 * Generate unique request ID
 */
static uint32_t generate_request_id(void) {
    return esp_random();
}

/**
 * Clear pending request and cleanup
 */
static void clear_pending_request(void) {
    if (mqtt_state.pending_request.timeout_timer != NULL) {
        xTimerStop(mqtt_state.pending_request.timeout_timer, 0);
        xTimerDelete(mqtt_state.pending_request.timeout_timer, 0);
        mqtt_state.pending_request.timeout_timer = NULL;
    }
    
    memset(&mqtt_state.pending_request, 0, sizeof(barcode_request_t));
    mqtt_state.pending_request.active = false;
}

/**
 * Handle request timeout
 */
static void request_timeout_callback(TimerHandle_t timer) {
    ESP_LOGW(TAG, "Barcode lookup timeout for request ID %u", 
             mqtt_state.pending_request.request_id);
    
    if (mqtt_state.pending_request.active && mqtt_state.pending_request.callback) {
        mqtt_barcode_result_t result = {0};
        strncpy(result.barcode, mqtt_state.pending_request.barcode, sizeof(result.barcode) - 1);
        result.success = false;
        result.request_id = mqtt_state.pending_request.request_id;
        
        // Call callback with timeout result
        mqtt_state.pending_request.callback(&result);
    }
    
    clear_pending_request();
}

/**
 * Handle barcode lookup response from MQTT
 */
static void handle_barcode_response(const char* data, int len) {
    ESP_LOGI(TAG, "Parsing JSON response: %d bytes", len);
    
    // Limit JSON size to prevent stack overflow
    if (len > 2048) {
        ESP_LOGW(TAG, "JSON response too large (%d bytes), truncating to 2048 bytes", len);
        len = 2048;
    }
    
    cJSON *json = cJSON_ParseWithLength(data, len);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response (len=%d)", len);
        ESP_LOGE(TAG, "JSON preview: %.100s...", data);
        return;
    }
    
    cJSON *request_id_item = cJSON_GetObjectItem(json, "request_id");
    cJSON *success_item = cJSON_GetObjectItem(json, "success");
    cJSON *barcode_item = cJSON_GetObjectItem(json, "barcode");
    cJSON *product_item = cJSON_GetObjectItem(json, "product");
    cJSON *lookup_time_item = cJSON_GetObjectItem(json, "lookup_time_ms");
    
    if (!request_id_item || !success_item || !barcode_item) {
        ESP_LOGE(TAG, "Invalid JSON response format");
        cJSON_Delete(json);
        return;
    }
    
    uint32_t response_request_id = (uint32_t)cJSON_GetNumberValue(request_id_item);
    
    // Check if this matches our pending request
    if (!mqtt_state.pending_request.active || 
        mqtt_state.pending_request.request_id != response_request_id) {
        ESP_LOGW(TAG, "Received response for unknown/expired request ID %u", response_request_id);
        cJSON_Delete(json);
        return;
    }
    
    ESP_LOGI(TAG, "Received barcode response for request %u", response_request_id);
    
    mqtt_barcode_result_t result = {0};
    
    // Basic fields
    strncpy(result.barcode, cJSON_GetStringValue(barcode_item), sizeof(result.barcode) - 1);
    result.success = cJSON_IsTrue(success_item);
    result.request_id = response_request_id;
    result.lookup_time_ms = lookup_time_item ? (uint32_t)cJSON_GetNumberValue(lookup_time_item) : 0;
    
    // Product information (if available)
    if (result.success && cJSON_IsObject(product_item)) {
        cJSON *name_item = cJSON_GetObjectItem(product_item, "name");
        cJSON *brand_item = cJSON_GetObjectItem(product_item, "brand");
        cJSON *model_item = cJSON_GetObjectItem(product_item, "model");
        cJSON *category_item = cJSON_GetObjectItem(product_item, "category");
        cJSON *price_item = cJSON_GetObjectItem(product_item, "price");
        cJSON *description_item = cJSON_GetObjectItem(product_item, "description");
        cJSON *image_url_item = cJSON_GetObjectItem(product_item, "image_url");
        
        if (name_item && cJSON_IsString(name_item)) {
            strncpy(result.name, cJSON_GetStringValue(name_item), sizeof(result.name) - 1);
        }
        if (brand_item && cJSON_IsString(brand_item)) {
            strncpy(result.brand, cJSON_GetStringValue(brand_item), sizeof(result.brand) - 1);
        }
        if (model_item && cJSON_IsString(model_item)) {
            strncpy(result.model, cJSON_GetStringValue(model_item), sizeof(result.model) - 1);
        }
        if (category_item && cJSON_IsString(category_item)) {
            strncpy(result.category, cJSON_GetStringValue(category_item), sizeof(result.category) - 1);
        }
        if (price_item && cJSON_IsString(price_item)) {
            strncpy(result.price, cJSON_GetStringValue(price_item), sizeof(result.price) - 1);
        }
        if (description_item && cJSON_IsString(description_item)) {
            strncpy(result.description, cJSON_GetStringValue(description_item), sizeof(result.description) - 1);
        }
        if (image_url_item && cJSON_IsString(image_url_item)) {
            strncpy(result.image_url, cJSON_GetStringValue(image_url_item), sizeof(result.image_url) - 1);
        }
        
        ESP_LOGI(TAG, "Product found: %s by %s (%s)", result.name, result.brand, result.price);
    } else {
        ESP_LOGI(TAG, "Product not found for barcode: %s", result.barcode);
    }
    
    // Call callback with result
    if (mqtt_state.pending_request.callback) {
        mqtt_state.pending_request.callback(&result);
    }
    
    // Cleanup
    clear_pending_request();
    cJSON_Delete(json);
}

/**
 * MQTT event handler (based on QuietSmart pattern)
 */
static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected to %s", MQTT_BROKER_URI);
            mqtt_state.status_message = "Connected";
            
            // Subscribe to response topic
            int msg_id = esp_mqtt_client_subscribe(mqtt_state.client, mqtt_state.response_topic, 1);
            ESP_LOGI(TAG, "Subscribed to %s, msg_id=%d", mqtt_state.response_topic, msg_id);
            
            xEventGroupSetBits(mqtt_state.event_group, MQTT_CONNECTED_BIT);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            mqtt_state.status_message = "Disconnected";
            xEventGroupClearBits(mqtt_state.event_group, MQTT_CONNECTED_BIT);
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Subscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Unsubscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT Published, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT Data received on topic: %.*s", event->topic_len, event->topic);
            if (strncmp(event->topic, mqtt_state.response_topic, event->topic_len) == 0) {
                handle_barcode_response(event->data, event->data_len);
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error occurred");
            mqtt_state.status_message = "Error";
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGE(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                        strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
            
        default:
            ESP_LOGI(TAG, "MQTT Event: %d", event_id);
            break;
    }
}

/**
 * Initialize MQTT barcode lookup system
 */
esp_err_t mqtt_barcode_init(void) {
    if (mqtt_state.initialized) {
        ESP_LOGW(TAG, "MQTT barcode system already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing MQTT barcode lookup system");
    mqtt_state.status_message = "Initializing";
    
    // Generate client ID and topic names
    generate_client_id();
    
    // Create event group for connection status
    mqtt_state.event_group = xEventGroupCreate();
    if (mqtt_state.event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Configure MQTT client with larger stack for JSON parsing
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.keepalive = MQTT_KEEPALIVE_SEC,
        .network.reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT_MS,
        .session.last_will.topic = NULL,  // No last will for now
        .credentials.client_id = mqtt_state.client_id,
        .task.stack_size = MQTT_TASK_STACK_SIZE,  // 8KB stack for JSON parsing
        .task.priority = MQTT_TASK_PRIORITY,
    };
    
    mqtt_state.client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_state.client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        vEventGroupDelete(mqtt_state.event_group);
        return ESP_FAIL;
    }
    
    // Register event handler
    esp_err_t err = esp_mqtt_client_register_event(mqtt_state.client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(mqtt_state.client);
        vEventGroupDelete(mqtt_state.event_group);
        return err;
    }
    
    // Start MQTT client
    err = esp_mqtt_client_start(mqtt_state.client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(mqtt_state.client);
        vEventGroupDelete(mqtt_state.event_group);
        return err;
    }
    
    mqtt_state.initialized = true;
    ESP_LOGI(TAG, "MQTT barcode system initialized successfully");
    
    return ESP_OK;
}

/**
 * Deinitialize MQTT barcode lookup system
 */
void mqtt_barcode_deinit(void) {
    if (!mqtt_state.initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing MQTT barcode system");
    
    // Clear any pending request
    clear_pending_request();
    
    // Stop and destroy MQTT client
    if (mqtt_state.client) {
        esp_mqtt_client_stop(mqtt_state.client);
        esp_mqtt_client_destroy(mqtt_state.client);
        mqtt_state.client = NULL;
    }
    
    // Delete event group
    if (mqtt_state.event_group) {
        vEventGroupDelete(mqtt_state.event_group);
        mqtt_state.event_group = NULL;
    }
    
    mqtt_state.initialized = false;
    mqtt_state.status_message = "Deinitialized";
    
    ESP_LOGI(TAG, "MQTT barcode system deinitialized");
}

/**
 * Lookup barcode information via MQTT
 */
esp_err_t mqtt_barcode_lookup(const char *barcode, mqtt_barcode_callback_t callback) {
    if (!mqtt_state.initialized) {
        ESP_LOGE(TAG, "MQTT barcode system not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (barcode == NULL || callback == NULL) {
        ESP_LOGE(TAG, "Invalid parameters for barcode lookup");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if we have a pending request
    if (mqtt_state.pending_request.active) {
        ESP_LOGW(TAG, "Barcode lookup already in progress, cancelling previous request");
        clear_pending_request();
    }
    
    // Check MQTT connection
    if (!mqtt_barcode_is_connected()) {
        ESP_LOGW(TAG, "MQTT not connected, cannot perform lookup");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting barcode lookup for: %s", barcode);
    
    // Generate request ID and setup tracking
    uint32_t request_id = generate_request_id();
    mqtt_state.pending_request.request_id = request_id;
    mqtt_state.pending_request.callback = callback;
    mqtt_state.pending_request.active = true;
    strncpy(mqtt_state.pending_request.barcode, barcode, sizeof(mqtt_state.pending_request.barcode) - 1);
    
    // Create timeout timer
    mqtt_state.pending_request.timeout_timer = xTimerCreate(
        "barcode_timeout",
        pdMS_TO_TICKS(MQTT_REQUEST_TIMEOUT_MS),
        pdFALSE,  // One-shot timer
        NULL,
        request_timeout_callback
    );
    
    if (mqtt_state.pending_request.timeout_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timeout timer");
        clear_pending_request();
        return ESP_ERR_NO_MEM;
    }
    
    // Create JSON request
    cJSON *json = cJSON_CreateObject();
    cJSON *barcode_item = cJSON_CreateString(barcode);
    cJSON *request_id_item = cJSON_CreateNumber((double)request_id);
    cJSON *timestamp_item = cJSON_CreateNumber((double)(esp_timer_get_time() / 1000000));
    
    if (json == NULL || barcode_item == NULL || request_id_item == NULL || timestamp_item == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON request");
        cJSON_Delete(json);
        clear_pending_request();
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddItemToObject(json, "barcode", barcode_item);
    cJSON_AddItemToObject(json, "request_id", request_id_item);
    cJSON_AddItemToObject(json, "timestamp", timestamp_item);
    
    char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to serialize JSON request");
        cJSON_Delete(json);
        clear_pending_request();
        return ESP_ERR_NO_MEM;
    }
    
    // Build request topic
    char request_topic[64];
    snprintf(request_topic, sizeof(request_topic), "%s/%s", MQTT_BARCODE_REQUEST_TOPIC, mqtt_state.client_id);
    
    // Publish request
    int msg_id = esp_mqtt_client_publish(mqtt_state.client, request_topic, json_string, 0, 1, 0);
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to publish barcode request");
        free(json_string);
        cJSON_Delete(json);
        clear_pending_request();
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Published barcode request %u to %s", request_id, request_topic);
    
    // Start timeout timer
    xTimerStart(mqtt_state.pending_request.timeout_timer, 0);
    
    // Cleanup
    free(json_string);
    cJSON_Delete(json);
    
    return ESP_OK;
}

/**
 * Check if MQTT client is connected
 */
bool mqtt_barcode_is_connected(void) {
    if (!mqtt_state.initialized || mqtt_state.event_group == NULL) {
        return false;
    }
    
    EventBits_t bits = xEventGroupGetBits(mqtt_state.event_group);
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

/**
 * Get connection status string
 */
const char* mqtt_barcode_get_status(void) {
    if (!mqtt_state.initialized) {
        return "Not initialized";
    }
    
    if (mqtt_state.status_message) {
        return mqtt_state.status_message;
    }
    
    return mqtt_barcode_is_connected() ? "Connected" : "Connecting";
}