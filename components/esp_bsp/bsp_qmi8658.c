#include "bsp_qmi8658.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#include "esp_log.h"

#include "bsp_i2c.h"

static const char *TAG = "bsp_qmi8658";
#define ACC_SENSITIVITY (4 / 32768.0f) // 每LSB对应的加速度（单位为g）

static i2c_master_dev_handle_t dev_handle;


// 读取QMI8658寄存器的值
static esp_err_t bsp_qmi8658_reg_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    //return bsp_i2c_reg8_read(QMI8658_SENSOR_ADDR, reg_addr, data, len);
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, pdMS_TO_TICKS(1000));
}

// 给QMI8658的寄存器写值
static esp_err_t bsp_qmi8658_reg_write_byte(uint8_t reg_addr, uint8_t *data, size_t len)
{
    uint8_t buf[len + 1];
    buf[0] = reg_addr;
    memcpy(buf + 1, data, len);
    //return bsp_i2c_reg8_write(QMI8658_SENSOR_ADDR, reg_addr, data, len);
    return i2c_master_transmit(dev_handle, buf, len + 1, pdMS_TO_TICKS(1000));
}

bool bsp_qmi8658_read_data(qmi8658_data_t *data)
{
    uint8_t status;
    float mask;
    int16_t buf[6];  // Changed to signed int16_t for proper handling of negative values
    uint8_t temp_data[2];
    
    esp_err_t ret = bsp_qmi8658_reg_read(QMI8658_STATUS0, &status, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read status register: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Status logging removed to prevent performance issues
    
    if (status & 0x03)
    {
        ret = bsp_qmi8658_reg_read(QMI8658_AX_L, (uint8_t *)buf, 12);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read sensor data: %s", esp_err_to_name(ret));
            return false;
        }
        
        data->acc_x = buf[0];
        data->acc_y = buf[1];
        data->acc_z = buf[2];
        data->gyr_x = buf[3];
        data->gyr_y = buf[4];
        data->gyr_z = buf[5];
        
        // Read temperature data
        ret = bsp_qmi8658_reg_read(QMI8658_TEMP_L, temp_data, 2);
        if (ret == ESP_OK) {
            int16_t temp_raw = (temp_data[1] << 8) | temp_data[0];
            data->temp = temp_raw / 256.0f;  // Convert to Celsius (typical QMI8658 scaling)
        } else {
            data->temp = 0.0f;  // Default if temperature read fails
        }
        
        // Removed frequent logging to prevent watchdog timeout

        mask = (float)data->acc_x / sqrt(((float)data->acc_y * (float)data->acc_y + (float)data->acc_z * (float)data->acc_z));
        data->AngleX = atan(mask) * 57.29578f; // 180/π=57.29578
        mask = (float)data->acc_y / sqrt(((float)data->acc_x * (float)data->acc_x + (float)data->acc_z * (float)data->acc_z));
        data->AngleY = atan(mask) * 57.29578f; // 180/π=57.29578
        mask = sqrt(((float)data->acc_x * (float)data->acc_x + (float)data->acc_y * (float)data->acc_y)) / (float)data->acc_z;
        data->AngleZ = atan(mask) * 57.29578f; // 180/π=57.29578
        return true;
    } else {
        ESP_LOGW(TAG, "QMI8658 data not ready, status: 0x%02X", status);
    }
    return false;
}

bool bsp_qmi8658_init(i2c_master_bus_handle_t bus_handle)
{
    uint8_t id = 0;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "QMI8658 Initialize");
    
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_SENSOR_ADDR,
        .scl_speed_hz = 400000,
    };
    
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add QMI8658 I2C device: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = bsp_qmi8658_reg_read(QMI8658_WHO_AM_I, &id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I register: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "QMI8658 WHO_AM_I: 0x%02X (expected: 0x05)", id);
    
    if (0x05 != id) {
        ESP_LOGE(TAG, "QMI8658 not found or wrong device ID");
        return false;
    }
    
    ESP_LOGI(TAG, "QMI8658 detected successfully");
    
    // Reset the device
    ret = bsp_qmi8658_reg_write_byte(QMI8658_RESET, (uint8_t[]){0xb0}, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset QMI8658: %s", esp_err_to_name(ret));
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Increased delay for proper reset
    
    // Configure the sensor
    ret = bsp_qmi8658_reg_write_byte(QMI8658_CTRL1, (uint8_t[]){0x40}, 1); // Address auto-increment
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CTRL1: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = bsp_qmi8658_reg_write_byte(QMI8658_CTRL7, (uint8_t[]){0x03}, 1); // Enable accelerometer and gyroscope
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CTRL7: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = bsp_qmi8658_reg_write_byte(QMI8658_CTRL2, (uint8_t[]){0x95}, 1); // Acc: 4g, 250Hz
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CTRL2: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = bsp_qmi8658_reg_write_byte(QMI8658_CTRL3, (uint8_t[]){0xd5}, 1); // Gyr: 512dps, 250Hz
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CTRL3: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "QMI8658 initialization completed successfully");
    return true;
}

// Initialize QMI8658 for WoM only (no normal sensor operation)
bool bsp_qmi8658_init_wom_only(i2c_master_bus_handle_t bus_handle)
{
    uint8_t id = 0;
    esp_err_t ret;
    
    ESP_LOGI(TAG, "QMI8658 WoM-only Initialize");
    
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_SENSOR_ADDR,
        .scl_speed_hz = 400000,
    };
    
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add QMI8658 I2C device: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = bsp_qmi8658_reg_read(0x00, &id, 1);  // WHO_AM_I
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I register: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "QMI8658 WHO_AM_I: 0x%02X (expected: 0x05)", id);
    
    if (0x05 != id) {
        ESP_LOGE(TAG, "QMI8658 not found or wrong device ID");
        return false;
    }
    
    ESP_LOGI(TAG, "QMI8658 detected successfully - WoM mode only");
    
    // Reset the device
    ret = bsp_qmi8658_soft_reset();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset QMI8658: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Minimal configuration for WoM only - don't configure for normal operation
    ret = bsp_qmi8658_reg_write_byte(0x02, (uint8_t[]){0x40}, 1); // CTRL1: Address auto-increment
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CTRL1: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "QMI8658 WoM-only initialization completed successfully");
    return true;
}

// Wait for CTRL9 command completion
static esp_err_t wait_ctrl9_completion(void) {
    uint8_t status;
    int timeout = 100;  // 100ms timeout
    
    while (timeout-- > 0) {
        esp_err_t err = bsp_qmi8658_reg_read(0x2D, &status, 1);  // STATUSINT
        if (err != ESP_OK) return err;
        
        if (status & 0x80) {  // bit 7 = CmdDone
            // Acknowledge completion
            uint8_t ack_cmd = CTRL_CMD_ACK;
            return bsp_qmi8658_reg_write_byte(0x0A, &ack_cmd, 1);  // CTRL9
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    ESP_LOGE(TAG, "CTRL9 command timeout");
    return ESP_ERR_TIMEOUT;
}

// Software reset function
esp_err_t bsp_qmi8658_soft_reset(void) {
    uint8_t reset_val = 0xB0;
    esp_err_t err = bsp_qmi8658_reg_write_byte(0x60, &reset_val, 1);  // RESET
    if (err != ESP_OK) return err;
    
    // Wait for reset to complete (15ms max according to datasheet)
    vTaskDelay(pdMS_TO_TICKS(20));
    
    ESP_LOGI(TAG, "QMI8658 software reset completed");
    return ESP_OK;
}

// Configure Wake on Motion with full parameter control
esp_err_t bsp_qmi8658_configure_wom(const qmi8658_wom_config_t *config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "WoM config cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err;
    
    ESP_LOGI(TAG, "Configuring WoM: threshold=%dmg, blanking=%d samples, ODR=0x%02X", 
             config->threshold_mg, config->blanking_samples, config->odr_mode);
    
    // Step 1: Disable all sensors before WoM configuration (required by datasheet)
    uint8_t disable_sensors = 0x00;
    err = bsp_qmi8658_reg_write_byte(0x08, &disable_sensors, 1);  // CTRL7
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable sensors: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // Small delay after disabling sensors
    
    // Step 2: Configure accelerometer ODR and full-scale range (±4g)
    uint8_t odr_val = config->odr_mode | 0x05;  // ODR + ±4g range
    err = bsp_qmi8658_reg_write_byte(0x03, &odr_val, 1);  // CTRL2
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure accelerometer ODR: %s", esp_err_to_name(err));
        return err;
    }
    
    // Step 3: Write WoM threshold to CAL1_L register (datasheet section 12.2)
    uint8_t wom_threshold = config->threshold_mg;
    err = bsp_qmi8658_reg_write_byte(0x0B, &wom_threshold, 1);  // CAL1_L
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WoM threshold: %s", esp_err_to_name(err));
        return err;
    }
    
    // Step 4: Configure interrupt settings in CAL1_H register
    // According to datasheet Table 39: 
    // Bits [7:6] = blanking time (2 bits)
    // Bits [5:4] = WoM Interrupt Initial Value: 01=INT2(val=0), 11=INT2(val=1), 00=INT1(val=0), 10=INT1(val=1)
    // Bits [3:0] = reserved
    uint8_t int_config = ((config->blanking_samples & 0x03) << 6) |  // Blanking time (2 bits max)
                        (0x01 << 4) |                                // INT2 with initial value 0 (bits 5:4 = 01)
                        0x00;                                        // Reserved bits [3:0] = 0000
    err = bsp_qmi8658_reg_write_byte(0x0C, &int_config, 1);  // CAL1_H
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure WoM interrupt: %s", esp_err_to_name(err));
        return err;
    }
    
    // Step 5: Execute CTRL9 command to apply WoM settings
    uint8_t wom_cmd = CTRL_CMD_WRITE_WOM_SETTING;
    err = bsp_qmi8658_reg_write_byte(0x0A, &wom_cmd, 1);  // CTRL9
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute WoM command: %s", esp_err_to_name(err));
        return err;
    }
    
    // Step 6: Wait for command completion
    err = wait_ctrl9_completion();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WoM command completion failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Step 7: Enable accelerometer to start WoM mode
    uint8_t enable_accel = 0x01;  // aEN=1, gEN=0 (accelerometer only)
    err = bsp_qmi8658_reg_write_byte(0x08, &enable_accel, 1);  // CTRL7
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable accelerometer for WoM: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "WoM configured successfully following datasheet sequence");
    return ESP_OK;
}

// Enable WoM interrupt output
esp_err_t bsp_qmi8658_enable_wom_interrupt(void) {
    uint8_t ctrl1;
    esp_err_t err = bsp_qmi8658_reg_read(0x02, &ctrl1, 1);  // CTRL1
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CTRL1: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "Current CTRL1 value: 0x%02X", ctrl1);
    
    // Enable INT2 output in push-pull mode (bit 4) and ensure address auto-increment (bit 6)
    // CTRL1: bit 6=addr_auto_incr, bit 4=INT2_EN, bit 3=INT1_EN
    ctrl1 |= 0x50;  // Enable both address auto-increment (0x40) and INT2 (0x10)
    
    err = bsp_qmi8658_reg_write_byte(0x02, &ctrl1, 1);  // CTRL1
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable WoM interrupt: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "WoM interrupt enabled on INT2 (GPIO9), CTRL1=0x%02X", ctrl1);
    return ESP_OK;
}

// Disable WoM interrupt output
esp_err_t bsp_qmi8658_disable_wom_interrupt(void) {
    uint8_t ctrl1;
    esp_err_t err = bsp_qmi8658_reg_read(0x02, &ctrl1, 1);  // CTRL1
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CTRL1: %s", esp_err_to_name(err));
        return err;
    }
    
    // Disable INT2 output (clear bit 4)
    ctrl1 &= ~0x10;
    
    err = bsp_qmi8658_reg_write_byte(0x02, &ctrl1, 1);  // CTRL1
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable WoM interrupt: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "WoM interrupt disabled");
    return ESP_OK;
}

// Check if WoM event occurred
bool bsp_qmi8658_check_wom_event(void) {
    uint8_t status1;
    
    if (bsp_qmi8658_reg_read(0x2F, &status1, 1) == ESP_OK) {  // STATUS1
        return (status1 & 0x04) != 0;  // Bit 2 = WoM flag
    }
    return false;
}

// Exit WoM mode and return to normal operation
esp_err_t bsp_qmi8658_exit_wom_mode(void) {
    esp_err_t err;
    
    // Disable accelerometer to exit WoM mode
    uint8_t ctrl7_val = 0x00;  // aEN=0, gEN=0
    err = bsp_qmi8658_reg_write_byte(0x08, &ctrl7_val, 1);  // CTRL7
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable accelerometer: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "Exited WoM mode");
    return ESP_OK;
}

// Debug function to read and display key registers
esp_err_t bsp_qmi8658_debug_registers(void) {
    uint8_t reg_val;
    esp_err_t err;
    
    ESP_LOGI(TAG, "=== QMI8658 Register Debug ===");
    
    // Read WHO_AM_I
    err = bsp_qmi8658_reg_read(0x00, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WHO_AM_I (0x00): 0x%02X", reg_val);
    }
    
    // Read CTRL1
    err = bsp_qmi8658_reg_read(0x02, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CTRL1 (0x02): 0x%02X [INT2_EN=%d]", reg_val, (reg_val >> 4) & 1);
    }
    
    // Read CTRL2 (Accelerometer config)
    err = bsp_qmi8658_reg_read(0x03, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CTRL2 (0x03): 0x%02X [ACC_ODR]", reg_val);
    }
    
    // Read CTRL5 (Interrupt routing)
    err = bsp_qmi8658_reg_read(0x06, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CTRL5 (0x06): 0x%02X [INT routing]", reg_val);
    }
    
    // Read CTRL7 (Enable bits)
    err = bsp_qmi8658_reg_read(0x08, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CTRL7 (0x08): 0x%02X [ACC_EN=%d, GYR_EN=%d]", reg_val, reg_val & 1, (reg_val >> 1) & 1);
    }
    
    // Read CAL registers (WoM settings)
    err = bsp_qmi8658_reg_read(0x0B, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CAL1_L (0x0B): 0x%02X [WoM Enable]", reg_val);
    }
    
    err = bsp_qmi8658_reg_read(0x0C, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CAL1_H (0x0C): 0x%02X", reg_val);
    }
    
    err = bsp_qmi8658_reg_read(0x0D, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CAL2_L (0x0D): 0x%02X [Threshold Low]", reg_val);
    }
    
    err = bsp_qmi8658_reg_read(0x0E, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CAL2_H (0x0E): 0x%02X [Threshold High]", reg_val);
    }
    
    err = bsp_qmi8658_reg_read(0x0F, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "CAL3_L (0x0F): 0x%02X [Blanking Samples]", reg_val);
    }
    
    // Read status registers
    err = bsp_qmi8658_reg_read(0x2D, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "STATUSINT (0x2D): 0x%02X [CmdDone=%d]", reg_val, (reg_val >> 7) & 1);
    }
    
    err = bsp_qmi8658_reg_read(0x2E, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "STATUS0 (0x2E): 0x%02X", reg_val);
    }
    
    err = bsp_qmi8658_reg_read(0x2F, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "STATUS1 (0x2F): 0x%02X [WoM_Flag=%d]", reg_val, (reg_val >> 2) & 1);
    }
    
    ESP_LOGI(TAG, "=== End Register Debug ===");
    return ESP_OK;
}

static void qmi8658_test_task(void *arg)
{
    qmi8658_data_t data;
    while (1)
    {
        bsp_qmi8658_read_data(&data);
        // printf("Acc: %.2f %.2f %.2f-----Gyr: %04d %04d %04d\n", data.acc_x * ACC_SENSITIVITY * 9.8f, data.acc_y * ACC_SENSITIVITY * 9.8f, data.acc_z * ACC_SENSITIVITY * 9.8f, data.gyr_x, data.gyr_y, data.gyr_z);
        // printf("-------------------------------------------------------------------------\n");
        // printf("Angle: %.2f %.2f %.2f\n", data.AngleX, data.AngleY, data.AngleZ);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void bsp_qmi8658_test(void)
{
    xTaskCreate(qmi8658_test_task, "qmi8658_test", 4096, NULL, 0, NULL);
}

