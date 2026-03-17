#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "driver/uart.h"

/* WIFI */
#define WIFI_SSID "VJU Office"
#define WIFI_PASS "VJuOffice@2023"

/* UART */
#define UART_PORT UART_NUM_2
#define TXD_PIN 12
#define RXD_PIN 13

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "PZEM_ESP32";

static EventGroupHandle_t wifi_event_group;

/* Google Script URL */
const char *google_url =
"https://script.google.com/macros/s/AKfycbwkmgvFBo__LTcMGpYcvYlYQdKMeFotokXGD9-RnOkYZPWrsGtZo6lMu_vowQrOLZRR/exec";

/* URL buffer (GLOBAL -> tránh stack overflow) */
static char url[512];

/* PZEM data */
float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;
float frequency = 0;
float pf = 0;


/* WIFI EVENT */

void wifi_event_handler(void* arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data)
{

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG,"WiFi reconnecting...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        ESP_LOGI(TAG,"Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }

}


/* WIFI INIT */

void wifi_init_sta()
{

    wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);

    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        }
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG,"WiFi started");
}


/* UART INIT */

void pzem_uart_init()
{

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(UART_PORT, 256, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);

    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

}


/* READ PZEM (Fake test) */

void read_pzem()
{

    voltage = 220 + (rand()%10);
    current = 1 + (rand()%100)/100.0;

    power = voltage * current;

    energy += 0.01;

    frequency = 50;
    pf = 0.95;

    ESP_LOGI(TAG,"Voltage: %.2f V",voltage);
    ESP_LOGI(TAG,"Current: %.2f A",current);
    ESP_LOGI(TAG,"Power: %.2f W",power);

}


/* SEND DATA */

void send_to_google()
{

    snprintf(url,
             sizeof(url),
             "%s?voltage=%.2f&current=%.2f&power=%.2f&energy=%.3f&frequency=%.1f&pf=%.2f",
             google_url,
             voltage,
             current,
             power,
             energy,
             frequency,
             pf);

    ESP_LOGI(TAG,"Sending: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {

        int status = esp_http_client_get_status_code(client);

        ESP_LOGI(TAG,"HTTP Status = %d", status);

    }
    else
    {
        ESP_LOGW(TAG,"HTTP warning: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

}


/* MAIN */

void app_main(void)
{

    nvs_flash_init();

    wifi_init_sta();

    pzem_uart_init();

    /* Wait WiFi */

    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        false,
                        true,
                        portMAX_DELAY);


    while(1)
    {

        read_pzem();

        send_to_google();

        vTaskDelay(10000 / portTICK_PERIOD_MS);

    }

}