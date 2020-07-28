
#include <stdio.h>
#include <time.h>


#define INFO(message, ...)      printf(message "\n", ##__VA_ARGS__)
#define ERROR(message, ...)     printf("! " message "\n", ##__VA_ARGS__)
#ifdef APP_DEBUG
#  define debug(fmt, ...)       printf("%s: " fmt "\n", __func__, ## __VA_ARGS__)
#else
#  define debug(fmt, ...)
#endif
#define FREEHEAP()              printf("Free Heap: %d\n", xPortGetFreeHeapSize())
#define UNUSED_ARG(x)	        (void)x


// Ask for delay in milliseconds
#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_PERIOD_MS)

// SENSOR_READ_RATE: Number of milliseconds between sensor readings
#ifdef APP_DEDUG
#   define SENSOR_READ_RATE        10000
#else
#   define SENSOR_READ_RATE        30000
#endif

//
// Structure defining the sensor reading message passed to influxdb task
//
typedef struct biSENSORREADING
{
    time_t readingTime;
    float pressure;
    float temperature;
    float humidity;
} SensorReading_t;

typedef struct biRESOURCES
{
    QueueHandle_t sensorQueue;
	TaskHandle_t taskWriteInfluxdb;
	TaskHandle_t taskLedBlink;
	TaskHandle_t taskBMP280;
} Resources_t;
