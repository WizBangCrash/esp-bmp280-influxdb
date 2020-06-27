#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include "ssid_config.h"

#include "bmp280_influxdb.h"

extern void write_influxdb_task();
extern void bmp280_task_normal();

const int led_pin = 13;


//
// Perform a brief flash of the LED when woken up
// and then go back to sleep
//
// TODO: Use xTaskNotify to pass option for blink rate etc.
void led_blink_task(void *pvParameters)
{
    debug("Initialising LED blink task\n");
    gpio_enable(led_pin, GPIO_OUTPUT);

    while (1) {
        // Wait until I'm asked to flash the LED
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        debug("Time to blink..\n");
        gpio_write(led_pin, 1);
        vTaskDelayMs(200);
        gpio_write(led_pin, 0);
    }
}

static Resources_t app_resources;
//TODO: Add task to monitor memory usage
void user_init(void)
{
    uart_set_baud(0, 115200);

    // Just some information
    printf("\n");
    printf("SDK version : %s\n", sdk_system_get_sdk_version());
    printf("GIT version : %s\n", PROJGITSHORTREV);

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(led_blink_task, "led_blink_task", 256, NULL, 2, &app_resources.taskLedBlink);
    xTaskCreate(write_influxdb_task, "write_influxdb_task", 384, (void *)&app_resources, 2, &app_resources.taskWriteInfluxdb);
    xTaskCreate(bmp280_task_normal, "bmp280_task", 256, (void *)&app_resources, 2, NULL);
}
