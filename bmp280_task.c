#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "i2c/i2c.h"
#include "bmp280/bmp280.h"


// #define APP_DEBUG true
#include "bmp280_influxdb.h"

extern const app_config_t app_config;

static const char *bmp280_s = "bmp280";
static const char *bme280_s = "bme280";


// TODO: Use forced mode and only take a reading every 60 seconds
void bmp280_task_normal(void *pvParameters)
{
    Resources_t *task_list = (Resources_t *)pvParameters;
    const bmp280_config_t *bmp_conf = &app_config.sensor_conf;
    bmp280_params_t  params;

    debug("Initialising task...");

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

        INFO("BMP280: found %s", bmp280_dev.id == BMP280_CHIP_ID ? bmp280_s : bme280_s);

        // Create a sensor reading buffer and
        // set the available sensors based on sensor type
        sensor_reading_t sensor_reading = {
            .type = bmp280_dev.id == BMP280_CHIP_ID ? bmp280_s : bme280_s,
            .measurements = bmp280_dev.id == BMP280_CHIP_ID ? (SENSOR_TEMPERATURE | SENSOR_PRESSURE) : (SENSOR_TEMPERATURE | SENSOR_PRESSURE | SENSOR_HUMIDITY)
        };

        while(1) {
            QueueHandle_t xQueue = task_list->sensorQueue;

            vTaskDelayMs(bmp_conf->poll_period);
            xTaskNotifyGive(task_list->taskLedBlink);
            if (!bmp280_read_float(&bmp280_dev, &sensor_reading.temperature, &sensor_reading.pressure, &sensor_reading.humidity)) {
                ERROR("Temperature/pressure reading failed");
                continue;
            }
            sensor_reading.time = time(NULL);
            INFO("Temperature: %.2f C, Pressure: %.2f Pa, Humidity: %.2f %%RH, EPOCH: %ld",
                sensor_reading.pressure, sensor_reading.temperature,
                sensor_reading.humidity, (long)sensor_reading.time);

            // Add the reading to the queue
            if (xQueueSend(xQueue, (void*)&sensor_reading, 0) != pdPASS) {
                time_t ts = time(NULL);
                INFO("Sensor Queue full at %s GMT", ctime(&ts));
            }
        }
    }
}
