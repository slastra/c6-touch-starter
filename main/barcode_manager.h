#pragma once

#include "driver/uart.h"
#include "esp_err.h"

#define BARCODE_UART_NUM         UART_NUM_1
#define BARCODE_TXD_PIN          6
#define BARCODE_RXD_PIN          5
#define BARCODE_UART_BUFFER_SIZE 1024
#define BARCODE_MAX_LENGTH       256

typedef struct {
    char data[BARCODE_MAX_LENGTH];
    size_t length;
    bool valid;
} barcode_data_t;

typedef void (*barcode_callback_t)(const barcode_data_t* barcode);

esp_err_t barcode_manager_init(barcode_callback_t callback);
esp_err_t barcode_manager_deinit(void);