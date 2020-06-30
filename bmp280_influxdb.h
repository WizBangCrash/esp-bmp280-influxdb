
#include <stdio.h>

// #define BMP280_INFLUX_DEBUG

#ifdef BMP280_INFLUX_DEBUG
#define DEBUG(fmt, ...)         printf("%s: " message "\n", __func__, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif

#define INFO(message, ...)      printf(message "\n", ##__VA_ARGS__)
#define ERROR(message, ...)     printf("! " message "\n", ##__VA_ARGS__)
#define FREEHEAP()              printf("Free Heap: %d\n", xPortGetFreeHeapSize())


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