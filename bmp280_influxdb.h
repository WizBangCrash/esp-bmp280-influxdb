
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
#ifdef APP_DEBUG
#   define SENSOR_READ_RATE        10000
#else
#   define SENSOR_READ_RATE        30000
#endif
#define MAX_QUEUE_DEPTH 120
#define MAX_SNTP_SERVERS 4

typedef enum _sensor_measurements {
    SENSOR_TEMPERATURE  = 0x01,
    SENSOR_HUMIDITY     = 0x02,
    SENSOR_PRESSURE     = 0x04
} sensor_measurements_t;

//
// Structure defining the sensor reading message passed to influxdb task
//
typedef struct _sensor_reading
{
    const char *type;        // Chip name.
    uint8_t measurements;    // Measurements available
    float pressure;
    float temperature;
    float humidity;
    time_t time;             // GMT time reading was taken
} sensor_reading_t;

//
// Influxdb server config
//
typedef struct _influxdb_config
{
    char *server_name;
    char *server_port;
    char *dbname;
} influxdb_config_t;

//
// BMP280 Sensor config
//
typedef struct _bmp280_config
{
    uint8_t i2c_bus;
    uint8_t i2c_addr;
    uint8_t scl_gpio;
    uint8_t sda_gpio;
    uint16_t poll_period;
} bmp280_config_t;

//
// Application configuration structure
//
typedef struct _app_config {
    uint8_t wifi_ssid[32];
    uint8_t wifi_password[64];
    uint8_t led_gpio;
    const char *sntp_servers[MAX_SNTP_SERVERS];
    uint8_t queue_depth;
    const char *device;
    const char *location;
    influxdb_config_t influxdb_conf;
    bmp280_config_t sensor_conf;
} app_config_t;



typedef struct biRESOURCES
{
    QueueHandle_t sensorQueue;
	TaskHandle_t taskWriteInfluxdb;
	TaskHandle_t taskLedBlink;
	TaskHandle_t taskBMP280;
} Resources_t;
