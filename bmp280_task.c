#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "i2c/i2c.h"
#include "bmp280/bmp280.h"


#define APP_DEBUG true
#include "bmp280_influxdb.h"

extern app_config_t app_config;


// TODO: Use forced mode and only take a reading every 60 seconds
void bmp280_task_normal(void *pvParameters)
{
    Resources_t *task_list = (Resources_t *)pvParameters;
    bmp280_config_t *bmp_conf = &app_config.sensor_conf;
    bmp280_params_t  params;

    debug("Initialising BMP280 task...");

    i2c_init(bmp_conf->i2c_bus, bmp_conf->scl_gpio, bmp_conf->sda_gpio, I2C_FREQ_400K);

    bmp280_init_default_params(&params);
    // Change standby time to 1 second
    params.standby = BMP280_STANDBY_1000;

    bmp280_t bmp280_dev;
    bmp280_dev.i2c_dev.bus = bmp_conf->i2c_bus;
    bmp280_dev.i2c_dev.addr = bmp_conf->i2c_addr == 0 ? BMP280_I2C_ADDRESS_0 : BMP280_I2C_ADDRESS_1;

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
            QueueHandle_t xQueue = task_list->sensorQueue;
            SensorReading_t sensor_reading;

            vTaskDelayMs(bmp_conf->poll_period);
            xTaskNotifyGive( task_list->taskLedBlink );
            if (!bmp280_read_float(&bmp280_dev, &sensor_reading.temperature, &sensor_reading.pressure, &sensor_reading.humidity)) {
                ERROR("Temperature/pressure reading failed");
                continue;
            }
            sensor_reading.readingTime = time(NULL);
            INFO("Pressure: %.2f Pa, Temperature: %.2f C, Humidity: %.2f %%RH, EPOCH: %ld",
                sensor_reading.pressure, sensor_reading.temperature, sensor_reading.humidity, (long)sensor_reading.readingTime);

            // Add the reading to the queue
            if (xQueueSend(xQueue, (void*)&sensor_reading, 0) != pdPASS) {
                time_t ts = time(NULL);
                INFO("Sensor Queue full at %s GMT", ctime(&ts));
            }
        }
    }
}
