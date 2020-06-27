#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include "i2c/i2c.h"
#include "bmp280/bmp280.h"

#include "ssid_config.h"

#include "bmp280_influxdb.h"

extern void http_post_task();

const uint8_t i2c_bus = 0;
const uint8_t scl_pin = 0;
const uint8_t sda_pin = 2;

const int led_pin = 13;


// TODO: Verify we are not loosing memory in this task
void bmp280_task_normal(void *pvParameters)
{
    Resources_t *task_list = (Resources_t *)pvParameters;
    Environment_t *environment = &task_list->environment;
    bmp280_params_t  params;

    debug("Initialising BMP280 task...\n");
    bmp280_init_default_params(&params);
    // Change standby time to 1 second
    params.standby = BMP280_STANDBY_1000;

    bmp280_t bmp280_dev;
    bmp280_dev.i2c_dev.bus = i2c_bus;
    bmp280_dev.i2c_dev.addr = BMP280_I2C_ADDRESS_0;

    while (1) {
        while (!bmp280_init(&bmp280_dev, &params)) {
            printf("BMP280 initialization failed\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

#ifdef BMP280_INFLUX_DEBUG
        bool bme280p = bmp280_dev.id == BME280_CHIP_ID;
        printf("BMP280: found %s\n", bme280p ? "BME280" : "BMP280");
#endif

        while(1) {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            xTaskNotifyGive( task_list->taskLedBlink );
            if (!bmp280_read_float(&bmp280_dev, &environment->temperature, &environment->pressure, &environment->humidity)) {
                printf("Temperature/pressure reading failed\n");
                break;
            }
            printf("Pressure: %.2f Pa, Temperature: %.2f C\n", environment->pressure, environment->temperature);
            // Pass control to the http_post_task
            xTaskNotifyGive( task_list->taskHttpPost );
        }
    }
}

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
        vTaskDelay(200 / portTICK_PERIOD_MS);
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
    printf("GIT version : %s\n", GITSHORTREV);

    i2c_init(i2c_bus, scl_pin, sda_pin, I2C_FREQ_400K);

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(led_blink_task, "led_blink_task", 256, NULL, 2, &app_resources.taskLedBlink);
    xTaskCreate(http_post_task, "http_post_task", 384, (void *)&app_resources, 2, &app_resources.taskHttpPost);
    xTaskCreate(bmp280_task_normal, "bmp280_task", 256, (void *)&app_resources, 2, NULL);
}
