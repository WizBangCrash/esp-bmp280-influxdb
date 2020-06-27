

// #define BMP280_INFLUX_DEBUG

#ifdef BMP280_INFLUX_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s" fmt "\n", "bmpinflux: ", ## __VA_ARGS__);
#else
#define debug(fmt, ...)
#endif

// Ask for delay in milliseconds
#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_PERIOD_MS)



typedef struct biENVIRONMENT
{
    float pressure;
    float temperature;
    float humidity;
} Environment_t;
typedef struct biRESOURCES
{
	TaskHandle_t taskWriteInfluxdb;
	TaskHandle_t taskLedBlink;
	TaskHandle_t taskBMP280;
	TaskHandle_t taskSNTP;
    Environment_t environment;
} Resources_t;