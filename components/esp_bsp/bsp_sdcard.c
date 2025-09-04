
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "nvs_flash.h"

#include "esp_check.h"
#include "esp_log.h"

#include "bsp_sdcard.h"
#include "bsp_spi.h"

sdmmc_card_t *card = NULL;

static const char *TAG = "bsp_sdcard";

uint64_t bsp_sdcard_get_size(void)
{
    uint64_t sdcard_size = 0;
    if (card != NULL)
    {
        sdcard_size = ((uint64_t)card->csd.capacity) * card->csd.sector_size;
    }
    return sdcard_size;
}


void bsp_sdcard_init(void)
{
    esp_err_t ret = ESP_OK;
    const char mount_point[] = "/sdcard";

    ESP_LOGI(TAG, "Initialize SPI SD Card");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    // sdmmc_card_t *card;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = EXAMPLE_SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = EXAMPLE_PIN_SD_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));

        }
    }
    else
    {
        ESP_LOGI(TAG, "Filesystem mounted");
        // Card has been initialized, print its properties
        sdmmc_card_print_info(stdout, card);
    }
}

// void bsp_sdcard_init(void)
// {
    // esp_err_t ret;

    // // Options for mounting the filesystem.
    // // If format_if_mount_failed is set to true, SD card will be partitioned and
    // // formatted in case when mounting fails.
    // esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    //     .format_if_mount_failed = false,
    //     .max_files = 5,
    //     .allocation_unit_size = 16 * 1024};
    // const char mount_point[] = "/sdcard";
    // ESP_LOGI(TAG, "Initializing SD card");

    // // Use settings defined above to initialize SD card and mount FAT filesystem.
    // // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // // Please check its source code and implement error recovery when developing
    // // production applications.

    // ESP_LOGI(TAG, "Using SDMMC peripheral");

    // // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    // sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // sdmmc_slot_config_t slot_config = {};

    // // On chips where the GPIOs used for SD card can be configured, set them in
    // // the slot_config structure:
    // slot_config.width = 4;
    // slot_config.cd = SDMMC_SLOT_NO_CD;
    // slot_config.wp = SDMMC_SLOT_NO_WP;
    // slot_config.width = SDMMC_SLOT_WIDTH_DEFAULT;
    // slot_config.flags = 0;
    // slot_config.clk = EXAMPLE_PIN_SD_CLK;
    // slot_config.cmd = EXAMPLE_PIN_SD_CMD;
    // slot_config.d0 = EXAMPLE_PIN_SD_D0;
    // slot_config.d1 = EXAMPLE_PIN_SD_D1;
    // slot_config.d2 = EXAMPLE_PIN_SD_D2;
    // slot_config.d3 = EXAMPLE_PIN_SD_D3;

    // // Enable internal pullups on enabled pins. The internal pullups
    // // are insufficient however, please make sure 10k external pullups are
    // // connected on the bus. This is for debug / example purpose only.
    // slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // ESP_LOGI(TAG, "Mounting filesystem");
    // ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    // if (ret != ESP_OK)
    // {
    //     if (ret == ESP_FAIL)
    //     {
    //         ESP_LOGE(TAG, "Failed to mount filesystem. "
    //                       "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
    //     }
    //     else
    //     {
    //         ESP_LOGE(TAG, "Failed to initialize the card (%s). "
    //                       "Make sure SD card lines have pull-up resistors in place.",
    //                  esp_err_to_name(ret));
    //     }
    //     return;
    // }
    // ESP_LOGI(TAG, "Filesystem mounted");

    // // Card has been initialized, print its properties
    // sdmmc_card_print_info(stdout, card);
// }