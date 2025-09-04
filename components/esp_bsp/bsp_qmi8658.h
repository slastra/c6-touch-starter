#pragma once
#include <stdio.h>
#include "driver/i2c_master.h"

#define QMI8658_SENSOR_ADDR 0x6B

// Wake on Motion (WoM) specific register defines
#define QMI8658_CAL1_L      0x0B  // WoM threshold register low byte
#define QMI8658_CAL1_H      0x0C  // WoM interrupt config register
#define QMI8658_CAL2_L      0x0D  // WoM threshold register low byte
#define QMI8658_CAL2_H      0x0E  // WoM threshold register high byte
#define QMI8658_CAL3_L      0x0F  // WoM blanking samples register
#define CTRL_CMD_WRITE_WOM_SETTING  0x08  // WoM configuration command
#define CTRL_CMD_ACK        0x00  // Command acknowledge

typedef enum
{
    QMI8658_WHO_AM_I,
    QMI8658_REVISION_ID,
    QMI8658_CTRL1,
    QMI8658_CTRL2,
    QMI8658_CTRL3,
    QMI8658_CTRL4,
    QMI8658_CTRL5,
    QMI8658_CTRL6,
    QMI8658_CTRL7,
    QMI8658_CTRL8,
    QMI8658_CTRL9,
    QMI8658_CATL1_L,
    QMI8658_CATL1_H,
    QMI8658_CATL2_L,
    QMI8658_CATL2_H,
    QMI8658_CATL3_L,
    QMI8658_CATL3_H,
    QMI8658_CATL4_L,
    QMI8658_CATL4_H,
    QMI8658_FIFO_WTM_TH,
    QMI8658_FIFO_CTRL,
    QMI8658_FIFO_SMPL_CNT,
    QMI8658_FIFO_STATUS,
    QMI8658_FIFO_DATA,
    QMI8658_STATUSINT = 45,
    QMI8658_STATUS0,
    QMI8658_STATUS1,
    QMI8658_TIMESTAMP_LOW,
    QMI8658_TIMESTAMP_MID,
    QMI8658_TIMESTAMP_HIGH,
    QMI8658_TEMP_L,
    QMI8658_TEMP_H,
    QMI8658_AX_L,
    QMI8658_AX_H,
    QMI8658_AY_L,
    QMI8658_AY_H,
    QMI8658_AZ_L,
    QMI8658_AZ_H,
    QMI8658_GX_L,
    QMI8658_GX_H,
    QMI8658_GY_L,
    QMI8658_GY_H,
    QMI8658_GZ_L,
    QMI8658_GZ_H,
    QMI8658_COD_STATUS = 70,
    QMI8658_dQW_L = 73,
    QMI8658_dQW_H,
    QMI8658_dQX_L,
    QMI8658_dQX_H,
    QMI8658_dQY_L,
    QMI8658_dQY_H,
    QMI8658_dQZ_L,
    QMI8658_dQZ_H,
    QMI8658_dVX_L,
    QMI8658_dVX_H,
    QMI8658_dVY_L,
    QMI8658_dVY_H,
    QMI8658_dVZ_L,
    QMI8658_dVZ_H,
    QMI8658_TAP_STATUS = 89,
    QMI8658_STEP_CNT_LOW,
    QMI8658_STEP_CNT_MIDL,
    QMI8658_STEP_CNT_HIGH,
    QMI8658_RESET = 96
} qmi8658_reg_t;

// 倾角结构体
typedef struct{
    int16_t acc_x;
	int16_t acc_y;
	int16_t acc_z;
	int16_t gyr_x;
	int16_t gyr_y;
	int16_t gyr_z;
	float AngleX;
	float AngleY;
	float AngleZ;
    float temp;
}qmi8658_data_t;

// Wake on Motion (WoM) configuration structure
typedef struct {
    uint16_t threshold_mg;      // Motion threshold in milligrams (50-500mg)
    uint8_t blanking_samples;   // Number of samples to ignore after trigger (1-255)
    uint8_t odr_mode;           // Output data rate mode (0xC0=128Hz, 0xD0=21Hz, 0xE0=11Hz, 0xF0=3Hz)
    bool enable_interrupt;      // Enable WoM interrupt output
} qmi8658_wom_config_t;

#ifdef __cplusplus
extern "C"
{
#endif

bool bsp_qmi8658_init(i2c_master_bus_handle_t bus_handle);
bool bsp_qmi8658_init_wom_only(i2c_master_bus_handle_t bus_handle);
void bsp_qmi8658_test(void);
bool bsp_qmi8658_read_data(qmi8658_data_t *data);

// Wake on Motion (WoM) functions
esp_err_t bsp_qmi8658_configure_wom(const qmi8658_wom_config_t *config);
esp_err_t bsp_qmi8658_enable_wom_interrupt(void);
esp_err_t bsp_qmi8658_disable_wom_interrupt(void);
bool bsp_qmi8658_check_wom_event(void);
esp_err_t bsp_qmi8658_exit_wom_mode(void);
esp_err_t bsp_qmi8658_soft_reset(void);
esp_err_t bsp_qmi8658_debug_registers(void);

#ifdef __cplusplus
}
#endif