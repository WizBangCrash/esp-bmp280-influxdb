
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
#define MAX_QUEUE_DEPTH 120
#define MAX_SNTP_SERVERS 4
#define SNTP_SERVERS 	"dixnas1.lan", "0.uk.pool.ntp.org", "1.uk.pool.ntp.org"

typedef enum _sensor_type {
    SENSOR_BMP280 = 0,
    SENSOR_BME280
} sensor_type_t;

//
// Structure defining the sensor reading message passed to influxdb task
//
typedef struct biSENSORREADING
{
    sensor_type_t type;
    float pressure;
    float temperature;
    float humidity;
    time_t readingTime;
} SensorReading_t;

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
