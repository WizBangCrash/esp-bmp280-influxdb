#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include "ssid_config.h"

#include "bmp280_influxdb.h"

//#define BMPINFLUX_DEBUG true

#ifdef BMPINFLUX_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s: " fmt "\n", __func__, ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif


extern void write_influxdb_task();
extern void bmp280_task_normal();
#ifdef INCLUDE_TIME
extern void sntp_task();
#endif

const int led_pin = 13;


//
// Perform a brief flash of the LED when woken up
// and then go back to sleep
//
// TODO: Use xTaskNotify to pass option for blink rate etc.
void led_blink_task(void *pvParameters)
{
    debug("Initialising LED blink task");
    gpio_enable(led_pin, GPIO_OUTPUT);

    while (1) {
        // Wait until I'm asked to flash the LED
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        debug("Time to blink..");
        gpio_write(led_pin, 1);
        vTaskDelayMs(200);
        gpio_write(led_pin, 0);
    }
}

static Resources_t app_resources;

#ifdef BMPINFLUX_DEBUG
// Periodically print out the stack space of each task
void
stats_task(void *pvParameters)
{
    TaskStatus_t taskStatus;

    debug("Initialising stats task");
    while (1) {
        FREEHEAP();
#ifdef INCLUDE_TIME
        vTaskGetInfo(app_resources.taskSNTP, &taskStatus, pdTRUE, eInvalid);
        printf("SNTP: %d, ", taskStatus.usStackHighWaterMark);
#endif
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

void
user_init(void)
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

#ifdef INCLUDE_TIME
    xTaskCreate(sntp_task, "SNTP", 512, NULL, 1, &app_resources.taskSNTP);
#endif
    xTaskCreate(led_blink_task, "led_blink", 256, NULL, 2, &app_resources.taskLedBlink);
    xTaskCreate(write_influxdb_task, "write_influxdb", 384, (void *)&app_resources, 2, &app_resources.taskWriteInfluxdb);
    xTaskCreate(bmp280_task_normal, "bmp280", 384, (void *)&app_resources, 2, &app_resources.taskBMP280);
#ifdef BMPINFLUX_DEBUG
    xTaskCreate(stats_task, "Stats", 1024, NULL, 1, NULL);
#endif
}
