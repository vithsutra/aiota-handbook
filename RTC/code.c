#include <stdio.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

/* ================= CONFIG ================= */
#define TAG "DS3231_RTC"

#define I2C_PORT    I2C_NUM_0
#define SDA_PIN     16
#define SCL_PIN     35
#define I2C_FREQ    100000

#define DS3231_ADDR 0x68

/* ================= BCD UTILS ================= */
static uint8_t dec_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

static uint8_t bcd_to_dec(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

/* ================= I2C INIT ================= */
static esp_err_t rtc_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    return i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

/* ================= SET RTC TIME ================= */
static esp_err_t rtc_set_time(struct tm *time)
{
    uint8_t data[8];

    data[0] = 0x00; // start register
    data[1] = dec_to_bcd(time->tm_sec);
    data[2] = dec_to_bcd(time->tm_min);
    data[3] = dec_to_bcd(time->tm_hour);
    data[4] = dec_to_bcd(time->tm_wday + 1);
    data[5] = dec_to_bcd(time->tm_mday);
    data[6] = dec_to_bcd(time->tm_mon + 1);
    data[7] = dec_to_bcd(time->tm_year - 100);

    return i2c_master_write_to_device(
        I2C_PORT, DS3231_ADDR, data, sizeof(data), pdMS_TO_TICKS(100)
    );
}

/* ================= READ RTC TIME ================= */
static esp_err_t rtc_get_time(struct tm *time)
{
    uint8_t reg = 0x00;
    uint8_t data[7];

    ESP_ERROR_CHECK(i2c_master_write_to_device(
        I2C_PORT, DS3231_ADDR, &reg, 1, pdMS_TO_TICKS(100)));

    ESP_ERROR_CHECK(i2c_master_read_from_device(
        I2C_PORT, DS3231_ADDR, data, 7, pdMS_TO_TICKS(100)));

    time->tm_sec  = bcd_to_dec(data[0] & 0x7F);
    time->tm_min  = bcd_to_dec(data[1]);
    time->tm_hour = bcd_to_dec(data[2] & 0x3F);
    time->tm_wday = bcd_to_dec(data[3]) - 1;
    time->tm_mday = bcd_to_dec(data[4]);
    time->tm_mon  = bcd_to_dec(data[5]) - 1;
    time->tm_year = bcd_to_dec(data[6]) + 100;

    return ESP_OK;
}

/* ================= MAIN ================= */
void app_main(void)
{
    ESP_ERROR_CHECK(rtc_i2c_init());

    /* ---- SET TIME ONCE ---- */
    struct tm set_time = {
        .tm_year = 2026 - 1900,
        .tm_mon  = 0,     // January
        .tm_mday = 30,
        .tm_hour = 17,
        .tm_min  = 15,
        .tm_sec  = 0,
        .tm_wday = 5
    };

    ESP_LOGI(TAG, "Setting RTC time...");
    rtc_set_time(&set_time);

    /* ---- READ LOOP ---- */
    while (1) {
        struct tm now;
        rtc_get_time(&now);

        ESP_LOGI(TAG,
            "RTC: %04d-%02d-%02d %02d:%02d:%02d",
            now.tm_year + 1900,
            now.tm_mon + 1,
            now.tm_mday,
            now.tm_hour,
            now.tm_min,
            now.tm_sec
        );

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

