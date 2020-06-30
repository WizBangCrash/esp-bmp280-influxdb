#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include "ssid_config.h"

#include "bmp280_influxdb.h"

extern void write_influxdb_task();
extern void bmp280_task_normal();
extern void sntp_task();

const int led_pin = 13;


//
// Perform a brief flash of the LED when woken up
// and then go back to sleep
//
// TODO: Use xTaskNotify to pass option for blink rate etc.
void led_blink_task(void *pvParameters)
{
    DEBUG("Initialising LED blink task");
    gpio_enable(led_pin, GPIO_OUTPUT);

    while (1) {
        // Wait until I'm asked to flash the LED
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        DEBUG("Time to blink..");
        gpio_write(led_pin, 1);
        vTaskDelayMs(200);
        gpio_write(led_pin, 0);
    }
}

static Resources_t app_resources;

#ifdef BMP280_INFLUX_DEBUG
void stats_task(void *pvParameters)
{
// Periodically print out the stack space of each task
    TaskStatus_t taskStatus;

    while (1) {
        FREEHEAP();
        vTaskGetInfo(app_resources.taskSNTP, &taskStatus, pdTRUE, eInvalid);
        printf("SNTP: %d, ", taskStatus.usStackHighWaterMark);
        vTaskGetInfo(app_resources.taskLedBlink, &taskStatus, pdTRUE, eInvalid);
        printf("LED: %d, ", taskStatus.usStackHighWaterMark);
        vTaskGetInfo(app_resources.taskWriteInfluxdb, &taskStatus, pdTRUE, eInvalid);
        printf("Influx: %d, ", taskStatus.usStackHighWaterMark);
        vTaskGetInfo(app_resources.taskBMP280, &taskStatus, pdTRUE, eInvalid);
        printf("BMP: %d\n", taskStatus.usStackHighWaterMark);
        vTaskDelayMs(60000);
    }
}
#endif

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

    xTaskCreate(sntp_task, "SNTP", 512, NULL, 1, &app_resources.taskSNTP);
    xTaskCreate(led_blink_task, "led_blink", 128, NULL, 2, &app_resources.taskLedBlink);
    xTaskCreate(write_influxdb_task, "write_influxdb", 384, (void *)&app_resources, 2, &app_resources.taskWriteInfluxdb);
    xTaskCreate(bmp280_task_normal, "bmp280", 256, (void *)&app_resources, 2, &app_resources.taskBMP280);
#ifdef BMP280_INFLUX_DEBUG
    xTaskCreate(stats_task, "Stats", 1024, NULL, 1, NULL);
#endif
}
