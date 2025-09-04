#pragma once

// Firmware Version
#define FIRMWARE_VERSION            "v3.0.12"

// Hardware Configuration
#define EXAMPLE_LCD_H_RES           (172)
#define EXAMPLE_LCD_V_RES           (320)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (100)

// WiFi Configuration - Include credentials file (not tracked in git)
#include "credentials.h"
#define WIFI_CONNECTED_BIT          BIT0

// OTA Configuration (GitHub latest release URL)
#define OTA_UPDATE_URL              "https://github.com/slastra/esp32-ota-firmware/releases/latest/download/firmware.bin"
#define OTA_RECV_TIMEOUT            5000

// MQTT Configuration (Barcode Resolution)
#define MQTT_BROKER_URI             "mqtt://desk.local:1883"
#define MQTT_BARCODE_REQUEST_TOPIC  "barcode/lookup/request"
#define MQTT_BARCODE_RESPONSE_TOPIC "barcode/lookup/response" 
#define MQTT_CLIENT_ID_PREFIX       "esp32c6"
#define MQTT_KEEPALIVE_SEC          30
#define MQTT_RECONNECT_TIMEOUT_MS   5000
#define MQTT_TASK_STACK_SIZE        8192    // Increased for large JSON parsing
#define MQTT_TASK_PRIORITY          5
#define MQTT_REQUEST_TIMEOUT_MS     10000

// Color Theme - Indigo & Black
#define PRIMARY_RED                 0x4B0082  // Indigo
#define DARK_RED                    0x2E0051  // Dark Indigo
#define LIGHT_RED                   0x6A4C93  // Light Indigo
#define BLACK                       0x000000
#define WHITE                       0xFFFFFF

// UI Layout Constants
#define UI_TITLE_Y_OFFSET           20
#define UI_BUTTON_Y_OFFSET          -20
#define UI_BUTTON_WIDTH             100
#define UI_BUTTON_HEIGHT            40
#define UI_WIFI_STATUS_Y_OFFSET     -80
#define UI_OTA_STATUS_Y_OFFSET      -60
#define UI_PROGRESS_BAR_Y_OFFSET    -45
#define UI_PROGRESS_BAR_WIDTH       140
#define UI_PROGRESS_BAR_HEIGHT      8
#define UI_RESOLUTION_Y_OFFSET      -20

// Corner marker settings
#define UI_CORNER_SIZE              10
#define UI_CORNER_OFFSET            2

// Progress update settings
#define OTA_PROGRESS_UPDATE_DELAY_MS 100

// Power Management Configuration
#define SCREEN_DIM_START_MS         (10 * 1000)    // Start dimming at 10s
#define SCREEN_OFF_TIMEOUT_MS       (20 * 1000)    // Turn off at 20s
#define LIGHT_SLEEP_TIMEOUT_MS      (30 * 1000)    // Reserved for future use
#define DIM_UPDATE_INTERVAL_MS      (1000)         // Update brightness every 1s
#define DIM_TARGET_BRIGHTNESS       2              // Fixed dimmed brightness level
#define LED_DIM_PERCENTAGE          20             // LED dimming percentage when screen dims
#define FADE_STEP_MS                50             // Fade animation step interval
#define FADE_STEP_SIZE              4              // Brightness change per fade step

// Motion Detection Configuration
#define MOTION_DETECT_INTERVAL_MS   100            // Check every 100ms for responsiveness
#define MOTION_WAKE_THRESHOLD_G     0.12f          // 0.12g movement to wake from sleep
#define MOTION_ACTIVITY_THRESHOLD_G 0.08f          // 0.08g to reset timer when awake
#define MOTION_DEBOUNCE_MS          300            // Prevent rapid triggers (ms)