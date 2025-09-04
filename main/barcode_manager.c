#include "barcode_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "barcode_manager";

static QueueHandle_t uart_queue;
static TaskHandle_t barcode_task_handle = NULL;
static barcode_callback_t barcode_callback = NULL;
static bool is_initialized = false;

static void print_hex_data(const uint8_t* data, int len, const char* prefix) {
    char hex_str[256] = {0};
    for (int i = 0; i < len && i < 32; i++) {  // Limit to 32 bytes for display
        snprintf(hex_str + (i * 3), sizeof(hex_str) - (i * 3), "%02X ", data[i]);
    }
    ESP_LOGI(TAG, "%s: %s(%d bytes)", prefix, hex_str, len);
}

static void barcode_task(void *arg)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t*) malloc(BARCODE_UART_BUFFER_SIZE);
    
    while (true) {
        if (xQueueReceive(uart_queue, (void*)&event, portMAX_DELAY)) {
            bzero(dtmp, BARCODE_UART_BUFFER_SIZE);
            
            switch (event.type) {
                case UART_DATA:
                    uart_read_bytes(BARCODE_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    
                    // Log all raw UART data received
                    print_hex_data(dtmp, event.size, "📡 UART received");
                    
                    if (event.size > 0 && barcode_callback) {
                        barcode_data_t barcode;
                        memset(&barcode, 0, sizeof(barcode_data_t));
                        
                        size_t copy_size = (event.size < BARCODE_MAX_LENGTH - 1) ? 
                                         event.size : BARCODE_MAX_LENGTH - 1;
                        memcpy(barcode.data, dtmp, copy_size);
                        barcode.data[copy_size] = '\0';
                        barcode.length = copy_size;
                        barcode.valid = true;
                        
                        for (size_t i = 0; i < copy_size; i++) {
                            if (barcode.data[i] == '\r' || barcode.data[i] == '\n') {
                                barcode.data[i] = '\0';
                                barcode.length = i;
                                break;
                            }
                        }
                        
                        ESP_LOGI(TAG, "📱 Barcode data: '%s' (length: %d)", barcode.data, barcode.length);
                        barcode_callback(&barcode);
                    }
                    break;
                    
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART FIFO overflow");
                    uart_flush_input(BARCODE_UART_NUM);
                    xQueueReset(uart_queue);
                    break;
                    
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART buffer full");
                    uart_flush_input(BARCODE_UART_NUM);
                    xQueueReset(uart_queue);
                    break;
                    
                default:
                    break;
            }
        }
    }
    
    free(dtmp);
    vTaskDelete(NULL);
}

esp_err_t barcode_manager_init(barcode_callback_t callback)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "Barcode manager already initialized");
        return ESP_OK;
    }
    
    if (!callback) {
        ESP_LOGE(TAG, "Callback cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    barcode_callback = callback;
    
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    esp_err_t ret = uart_driver_install(BARCODE_UART_NUM, BARCODE_UART_BUFFER_SIZE * 2, 
                                        BARCODE_UART_BUFFER_SIZE * 2, 20, &uart_queue, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_param_config(BARCODE_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        uart_driver_delete(BARCODE_UART_NUM);
        return ret;
    }
    
    ret = uart_set_pin(BARCODE_UART_NUM, BARCODE_TXD_PIN, BARCODE_RXD_PIN, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        uart_driver_delete(BARCODE_UART_NUM);
        return ret;
    }
    
    BaseType_t task_ret = xTaskCreate(barcode_task, "barcode_task", 4096, NULL, 10, &barcode_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create barcode task");
        uart_driver_delete(BARCODE_UART_NUM);
        return ESP_FAIL;
    }
    
    is_initialized = true;
    ESP_LOGI(TAG, "Barcode manager initialized (UART%d: TX=%d, RX=%d, 9600 bps, auto-scan mode)", 
             BARCODE_UART_NUM, BARCODE_TXD_PIN, BARCODE_RXD_PIN);
    
    return ESP_OK;
}

esp_err_t barcode_manager_deinit(void)
{
    if (!is_initialized) {
        return ESP_OK;
    }
    
    if (barcode_task_handle) {
        vTaskDelete(barcode_task_handle);
        barcode_task_handle = NULL;
    }
    
    uart_driver_delete(BARCODE_UART_NUM);
    barcode_callback = NULL;
    is_initialized = false;
    
    ESP_LOGI(TAG, "Barcode manager deinitialized");
    return ESP_OK;
}