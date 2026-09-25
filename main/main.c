#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dashboard.h"
#include "display.h"
#include "vems.h"

#define UART_RX_BUFFER_SIZE 1024
#define UI_REFRESH_MS       33
#define DEMO_TASK_PERIOD_MS 50

static const char *TAG = "camarodisplay";
static portMUX_TYPE s_data_lock = portMUX_INITIALIZER_UNLOCKED;
static vems_data_t s_vems_data;

#if !CONFIG_CAMARO_DEMO_MODE
static void vems_uart_task(void *argument)
{
    (void)argument;
    vems_parser_t parser;
    vems_parser_init(&parser);
    uint8_t bytes[64];

    while (true) {
        const int length = uart_read_bytes(CONFIG_CAMARO_VEMS_UART_PORT, bytes,
                                           sizeof(bytes), pdMS_TO_TICKS(20));
        for (int i = 0; i < length; ++i) {
            portENTER_CRITICAL(&s_data_lock);
            vems_parser_feed(&parser, bytes[i], &s_vems_data);
            portEXIT_CRITICAL(&s_data_lock);
        }
    }
}
#endif

#if CONFIG_CAMARO_DEMO_MODE
static void demo_data_task(void *argument)
{
    (void)argument;
    float t = 0.0f;

    while (true) {
        const vems_data_t demo = {
            .rpm = (uint16_t)(900 + (sinf(t * 0.3f) + 1.0f) * 0.5f * 5600),
            .map_kpa = 35.0f + (sinf(t * 0.6f) + 1.0f) * 45.0f,
            .coolant_c = (int16_t)(75 + (int16_t)(sinf(t * 0.05f) * 15)),
            .lambda = 0.85f + (sinf(t * 0.4f) + 1.0f) * 0.15f,
            .valid_mask = VEMS_VALID_RPM | VEMS_VALID_MAP | VEMS_VALID_CLT |
                          VEMS_VALID_LAMBDA,
        };
        portENTER_CRITICAL(&s_data_lock);
        s_vems_data = demo;
        portEXIT_CRITICAL(&s_data_lock);

        t += DEMO_TASK_PERIOD_MS / 1000.0f;
        vTaskDelay(pdMS_TO_TICKS(DEMO_TASK_PERIOD_MS));
    }
}
#endif

static void dashboard_timer(lv_timer_t *timer)
{
    (void)timer;
    vems_data_t snapshot;
    portENTER_CRITICAL(&s_data_lock);
    snapshot = s_vems_data;
    portEXIT_CRITICAL(&s_data_lock);
    dashboard_manager_update(&snapshot);
}

#if !CONFIG_CAMARO_DEMO_MODE
static esp_err_t vems_uart_init(void)
{
    const uart_config_t config = {
        .baud_rate = CONFIG_CAMARO_VEMS_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(CONFIG_CAMARO_VEMS_UART_PORT,
                                             UART_RX_BUFFER_SIZE, 0, 0, NULL, 0),
                        TAG, "UART driver install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(CONFIG_CAMARO_VEMS_UART_PORT, &config),
                        TAG, "UART config failed");
    return uart_set_pin(CONFIG_CAMARO_VEMS_UART_PORT,
                        CONFIG_CAMARO_VEMS_UART_TX_GPIO,
                        CONFIG_CAMARO_VEMS_UART_RX_GPIO,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}
#endif

void app_main(void)
{
#if !CONFIG_CAMARO_DEMO_MODE
    ESP_ERROR_CHECK(vems_uart_init());
#endif
    ESP_ERROR_CHECK(display_init());

    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    ESP_ERROR_CHECK(dashboard_manager_init());
    lv_timer_create(dashboard_timer, UI_REFRESH_MS, NULL);
    esp_lv_adapter_unlock();

#if CONFIG_CAMARO_DEMO_MODE
    BaseType_t created = xTaskCreate(demo_data_task, "demo_data", 4096, NULL, 5, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Could not create demo data task");
        abort();
    }
    ESP_LOGI(TAG, "Ready: demo mode, generating synthetic VEMS data");
#else
    BaseType_t created = xTaskCreate(vems_uart_task, "vems_uart", 4096, NULL, 5, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Could not create VEMS UART task");
        abort();
    }

    ESP_LOGI(TAG, "Ready: VEMS AIM on UART%d, RX GPIO%d, TX GPIO%d at %d baud",
             CONFIG_CAMARO_VEMS_UART_PORT, CONFIG_CAMARO_VEMS_UART_RX_GPIO,
             CONFIG_CAMARO_VEMS_UART_TX_GPIO, CONFIG_CAMARO_VEMS_UART_BAUD);
#endif
}
