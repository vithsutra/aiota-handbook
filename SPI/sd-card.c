#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"

// --- Pin Definitions for ESP32-S3 ---
// Ensure these pins aren't used by your specific module's Octal SPI Flash
#define PIN_NUM_MISO 9
#define PIN_NUM_MOSI 8
#define PIN_NUM_CLK  10
#define PIN_NUM_CS   11
#define MOUNT_POINT "/sdcard"

// Function Prototypes
esp_err_t sd_write_file(const char *path, const char *data);
esp_err_t sd_read_file(const char *path);

void app_main(void) {
    esp_err_t ret;

    // 1. Mount Configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // 2. Host Configuration 
    // We modify the default to be slower (5MHz) to fix CRC/Timing errors
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 5000; 

    // 3. SPI Bus Configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // Initialize the SPI bus
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        printf("Failed to initialize SPI bus: %s\n", esp_err_to_name(ret));
        return;
    }

    // 4. Slot Configuration
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    // 5. Mounting
    sdmmc_card_t *card;
    printf("Mounting filesystem...\n");
    
    // Attempt to mount
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_CRC) {
            printf("Error: Checksum failure (0x109). Add 10k pull-up resistors to MISO/MOSI/CLK!\n");
        } else {
            printf("Mount failed: %s\n", esp_err_to_name(ret));
        }
        return;
    }
    
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    printf("Filesystem mounted successfully.\n");

    // 6. Test Operations
    sd_write_file(MOUNT_POINT "/hii.txt", "Hello vithsutra SD card FINE!\n");
    sd_read_file(MOUNT_POINT "/hii.txt");

    // Optional: Unmount when done (usually not needed in app_main if running forever)
    // esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
}

// --- HELPER FUNCTIONS ---

esp_err_t sd_write_file(const char *path, const char *data) {
    printf("Writing to %s... ", path);
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        printf("Failed to open file for writing!\n");
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    printf("Done.\n");
    return ESP_OK;
}

esp_err_t sd_read_file(const char *path) {
    printf("Reading %s:\n", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        printf("Failed to open file for reading.\n");
        return ESP_FAIL;
    }

    char line[64];
    while (fgets(line, sizeof(line), f) != NULL) {
        printf("  > %s", line);
    }
    fclose(f);
    return ESP_OK;
}
