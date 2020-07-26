#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include "i2c/i2c.h"
#include "bmp280/bmp280.h"

// #define APP_DEBUG true
#include "bmp280_influxdb.h"


// TODO: Make these configurable outside this source file
static const uint8_t i2c_bus = 0;
static const uint8_t scl_pin = 0;
static const uint8_t sda_pin = 2;

// TODO: Use forced mode and only take a reading every 60 seconds
void bmp280_task_normal(void *pvParameters)
{
    Resources_t *task_list = (Resources_t *)pvParameters;
    Environment_t *environment = &task_list->environment;
    bmp280_params_t  params;

    debug("Initialising BMP280 task...");

    i2c_init(i2c_bus, scl_pin, sda_pin, I2C_FREQ_400K);

    bmp280_init_default_params(&params);
    // Change standby time to 1 second
    params.standby = BMP280_STANDBY_1000;

    bmp280_t bmp280_dev;
    bmp280_dev.i2c_dev.bus = i2c_bus;
    bmp280_dev.i2c_dev.addr = BMP280_I2C_ADDRESS_1;

    while (1) {
        while (!bmp280_init(&bmp280_dev, &params)) {
            ERROR("BMP280 initialization failed");
            vTaskDelayMs(1000);
        }

#ifdef BMP280_DEBUG
        bool bme280p = bmp280_dev.id == BME280_CHIP_ID;
        INFO("BMP280: found %s", bme280p ? "BME280" : "BMP280");
#endif

        while(1) {
            vTaskDelayMs(10000);
            xTaskNotifyGive( task_list->taskLedBlink );
            if (!bmp280_read_float(&bmp280_dev, &environment->temperature, &environment->pressure, &environment->humidity)) {
                ERROR("Temperature/pressure reading failed");
                break;
            }
            INFO("Pressure: %.2f Pa, Temperature: %.2f C, Humidity: %.2f %%RH", environment->pressure, environment->temperature, environment->humidity);
            // Pass control to the write_influxdb_task
            xTaskNotifyGive( task_list->taskWriteInfluxdb );
        }
    }
}
