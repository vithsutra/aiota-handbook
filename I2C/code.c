#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_log.h"

/* ---------------- CONFIG ---------------- */
#define TAG "I2C_LOOPBACK"

#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_SLAVE_NUM      I2C_NUM_1

#define I2C_MASTER_SDA     16
#define I2C_MASTER_SCL     35

#define I2C_SLAVE_SDA      36
#define I2C_SLAVE_SCL      37

#define I2C_SLAVE_ADDR     0x28
#define I2C_FREQ_HZ        10000

#define MSG_LEN            5   // "HELLO"

/* ---------------- SLAVE TASK ---------------- */
static void i2c_slave_task(void *arg)
{
    uint8_t rx_buf[MSG_LEN + 1];

    while (1) {
        /* Flush RX FIFO to avoid residue */
        i2c_reset_rx_fifo(I2C_SLAVE_NUM);

        int len = i2c_slave_read_buffer(
            I2C_SLAVE_NUM,
            rx_buf,
            MSG_LEN,
            pdMS_TO_TICKS(2000)
        );

        if (len == MSG_LEN) {
            rx_buf[MSG_LEN] = '\0';
            ESP_LOGI(TAG, "Slave received: %s", rx_buf);
        } else if (len > 0) {
            ESP_LOGW(TAG, "Partial read: %d bytes", len);
        }
    }
}

/* ---------------- MASTER INIT ---------------- */
static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA,
        .scl_io_num = I2C_MASTER_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

/* ---------------- SLAVE INIT ---------------- */
static void i2c_slave_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = I2C_SLAVE_SDA,
        .scl_io_num = I2C_SLAVE_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave.addr_10bit_en = 0,
        .slave.slave_addr = I2C_SLAVE_ADDR
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_SLAVE_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_SLAVE_NUM, conf.mode, 128, 128, 0));
}

/* ---------------- MASTER SEND ---------------- */
static void i2c_master_send(const char *msg)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(
        cmd,
        (I2C_SLAVE_ADDR << 1) | I2C_MASTER_WRITE,
        true
    );
    i2c_master_write(cmd, (uint8_t *)msg, MSG_LEN, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(
        I2C_MASTER_NUM,
        cmd,
        pdMS_TO_TICKS(1000)
    );

    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Master sent: %s", msg);
    } else {
        ESP_LOGE(TAG, "Master send failed: %s", esp_err_to_name(ret));
    }
}

/* ---------------- APP MAIN ---------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting I2C loopback test");

    i2c_slave_init();
    i2c_master_init();

    xTaskCreate(
        i2c_slave_task,
        "i2c_slave_task",
        4096,
        NULL,
        5,
        NULL
    );

    /* Let slave stabilize */
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        i2c_master_send("HELLO");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

