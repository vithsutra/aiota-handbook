#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "onewire_bus.h"
#include "ds18b20.h"

static const char *TAG = "DS18B20";

#define EXAMPLE_ONEWIRE_BUS_GPIO    X //define pin available in GPIO on Panel
#define EXAMPLE_ONEWIRE_MAX_DS18B20 4

void app_main(void)
{
    float temperature;

    /* ---------------- 1-Wire Bus Init ---------------- */
    onewire_bus_handle_t bus = NULL;

    onewire_bus_config_t bus_config = {
        .bus_gpio_num = EXAMPLE_ONEWIRE_BUS_GPIO,
        .flags = {
            .en_pull_up = true,   // internal pull-up (external 4.7k still recommended)
        }
    };

    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10,
    };

    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

    /* ---------------- Device Search ---------------- */
    ds18b20_device_handle_t ds18b20s[EXAMPLE_ONEWIRE_MAX_DS18B20];
    int ds18b20_device_num = 0;

    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next_device;
    esp_err_t search_result;

    ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
    ESP_LOGI(TAG, "Searching for 1-Wire devices...");

    do {
        search_result = onewire_device_iter_get_next(iter, &next_device);
        if (search_result == ESP_OK) {

            ds18b20_config_t ds_cfg = {};
            onewire_device_address_t address;

            if (ds18b20_new_device_from_enumeration(
                    &next_device,
                    &ds_cfg,
                    &ds18b20s[ds18b20_device_num]) == ESP_OK) {

                ds18b20_get_device_address(ds18b20s[ds18b20_device_num], &address);
                ESP_LOGI(TAG,
                         "Found DS18B20[%d], ROM: %016llX",
                         ds18b20_device_num,
                         address);

                ds18b20_device_num++;
                if (ds18b20_device_num >= EXAMPLE_ONEWIRE_MAX_DS18B20) {
                    break;
                }
            } else {
                ESP_LOGI(TAG,
                         "Unknown 1-Wire device, ROM: %016llX",
                         next_device.address);
            }
        }
    } while (search_result != ESP_ERR_NOT_FOUND);

    ESP_ERROR_CHECK(onewire_del_device_iter(iter));

    ESP_LOGI(TAG, "Search done, %d DS18B20 device(s) found",
             ds18b20_device_num);

    if (ds18b20_device_num == 0) {
        ESP_LOGE(TAG, "No DS18B20 sensors found");
        return;
    }

    /* ---------------- Read Loop ---------------- */
    while (1) {
        ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion_for_all(bus));
        vTaskDelay(pdMS_TO_TICKS(750)); // 12-bit conversion time

        for (int i = 0; i < ds18b20_device_num; i++) {
            ESP_ERROR_CHECK(ds18b20_get_temperature(ds18b20s[i], &temperature));
            ESP_LOGI(TAG, "DS18B20[%d] Temperature: %.2f °C", i, temperature);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

