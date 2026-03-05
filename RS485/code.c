#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

/* ================= CONFIG ================= */
#define UART_NUM    UART_NUM_2
#define TXD_PIN     GPIO_NUM_35
#define RXD_PIN     GPIO_NUM_16
#define DE_PIN      GPIO_NUM_36
#define RE_PIN      GPIO_NUM_37

#define BAUD_RATE   9600
#define BUF_SIZE    256

static const char *TAG = "SOIL_RS485";

/* ================= MODBUS QUERIES ================= */
uint8_t npkQuery[]             = {0x01,0x03,0x00,0x1E,0x00,0x03,0x65,0xCD};
uint8_t phQuery[]              = {0x01,0x03,0x00,0x06,0x00,0x01,0x64,0x0B};
uint8_t soilMoistureQuery[]    = {0x01,0x03,0x00,0x12,0x00,0x01,0x24,0x0F};
uint8_t soilTemperatureQuery[] = {0x01,0x03,0x00,0x13,0x00,0x01,0x75,0xCF};
uint8_t conductivityQuery[]    = {0x01,0x03,0x00,0x15,0x00,0x01,0x95,0xCE};

/* ================= RS485 CONTROL ================= */
static inline void rs485_tx_mode(void)
{
    gpio_set_level(DE_PIN, 1);   // Enable driver
    gpio_set_level(RE_PIN, 1);   // Disable receiver
    esp_rom_delay_us(20);
}

static inline void rs485_rx_mode(void)
{
    gpio_set_level(DE_PIN, 0);   // Disable driver
    gpio_set_level(RE_PIN, 0);   // Enable receiver
    esp_rom_delay_us(20);
}

/* ================= GPIO INIT ================= */
void rs485_gpio_init(void)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << DE_PIN) | (1ULL << RE_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    rs485_rx_mode();
    ESP_LOGI(TAG, "RS485 GPIO initialized");
}

/* ================= UART INIT ================= */
void rs485_uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN,
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART initialized TX=%d RX=%d", TXD_PIN, RXD_PIN);
}

/* ================= SEND MODBUS QUERY ================= */
void modbus_send(uint8_t *data, uint8_t len)
{
    uart_flush(UART_NUM);
    rs485_tx_mode();
    uart_write_bytes(UART_NUM, (char *)data, len);
    uart_wait_tx_done(UART_NUM, pdMS_TO_TICKS(100));
    rs485_rx_mode();
}

/* ================= READ RESPONSE ================= */
int modbus_read(uint8_t *buf, int expected, int timeout_ms)
{
    int len = uart_read_bytes(UART_NUM, buf, expected,
                              pdMS_TO_TICKS(timeout_ms));
    return len;
}

/* ================= SENSOR TASK ================= */
void soil_task(void *arg)
{
    uint8_t rx[32];

    ESP_LOGI(TAG, "Waiting for sensor warm-up...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {

        /* -------- pH -------- */
        modbus_send(phQuery, sizeof(phQuery));
        int len = modbus_read(rx, 7, 1000);
        float pH = (len == 7) ? ((rx[3] << 8 | rx[4]) / 100.0f) : 0;

        /* -------- Moisture -------- */
        modbus_send(soilMoistureQuery, sizeof(soilMoistureQuery));
        len = modbus_read(rx, 7, 1000);
        float moisture = (len == 7) ? ((rx[3] << 8 | rx[4]) / 10.0f) : 0;

        /* -------- Temperature -------- */
        modbus_send(soilTemperatureQuery, sizeof(soilTemperatureQuery));
        len = modbus_read(rx, 7, 1000);
        float temp = (len == 7) ? ((rx[3] << 8 | rx[4]) / 10.0f) : 0;

        /* -------- EC -------- */
        modbus_send(conductivityQuery, sizeof(conductivityQuery));
        len = modbus_read(rx, 7, 1000);
        uint16_t ec = (len == 7) ? (rx[3] << 8 | rx[4]) : 0;

        /* -------- NPK -------- */
        modbus_send(npkQuery, sizeof(npkQuery));
        len = modbus_read(rx, 11, 1500);

        uint16_t n=0,p=0,k=0;
        if (len == 11) {
            n = rx[3]<<8 | rx[4];
            p = rx[5]<<8 | rx[6];
            k = rx[7]<<8 | rx[8];
        }

        printf("\n========= SOIL DATA =========\n");
        printf("pH          : %.2f\n", pH);
        printf("Moisture    : %.1f %%\n", moisture);
        printf("Temperature : %.1f C\n", temp);
        printf("EC          : %u uS/cm\n", ec);
        printf("Nitrogen    : %u mg/kg\n", n);
        printf("Phosphorus  : %u mg/kg\n", p);
        printf("Potassium   : %u mg/kg\n", k);
        printf("=============================\n");

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ================= MAIN ================= */
void app_main(void)
{
    printf("\nNPK 7-in-1 Soil Sensor (ESP32-S3 RS485)\n");

    rs485_gpio_init();
    rs485_uart_init();

    xTaskCreatePinnedToCore(
        soil_task,
        "soil_task",
        4096,
        NULL,
        5,
        NULL,
        0
    );
}

