#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include "i2c/i2c.h"
#include "bmp280/bmp280.h"

const uint8_t i2c_bus = 0;
const uint8_t scl_pin = 0;
const uint8_t sda_pin = 2;

const int led_pin = 13;


static void bmp280_task_normal(void *pvParameters)
{
    bmp280_params_t  params;
    float pressure, temperature, humidity;

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

        bool bme280p = bmp280_dev.id == BME280_CHIP_ID;
        printf("BMP280: found %s\n", bme280p ? "BME280" : "BMP280");

        while(1) {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            if (!bmp280_read_float(&bmp280_dev, &temperature, &pressure, &humidity)) {
                printf("Temperature/pressure reading failed\n");
                break;
            }
            printf("Pressure: %.2f Pa, Temperature: %.2f C\n", pressure, temperature);
            // Wake up the LED blink task
            vTaskResume( pvParameters );
        }
    }
}

//
// Perform a brief flash of the LED when woken up
// and then go back to sleep
//
void led_blink_task(void *pvParameters)
{
    gpio_enable(led_pin, GPIO_OUTPUT);

    while (1) {
        vTaskSuspend(NULL);
        gpio_write(led_pin, 1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        gpio_write(led_pin, 0);
    }
}


void user_init(void)
{
    TaskHandle_t led_blink_task_handle;
    uart_set_baud(0, 115200);

    // Just some information
    printf("\n");
    printf("SDK version : %s\n", sdk_system_get_sdk_version());
    printf("GIT version : %s\n", GITSHORTREV);

    i2c_init(i2c_bus, scl_pin, sda_pin, I2C_FREQ_400K);

    xTaskCreate(led_blink_task, "led_blink_task", 256, NULL, 2, &led_blink_task_handle);
    xTaskCreate(bmp280_task_normal, "bmp280_task", 256, led_blink_task_handle, 2, NULL);
}
