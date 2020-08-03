#include <stdio.h>
#include <string.h>
#include <time.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
/* Add extras/sntp component to makefile for this include to work */
#include "sntp.h"
#include "ssid_config.h"

#define APP_DEBUG true
#include "bmp280_influxdb.h"


extern void write_influxdb_task();
extern void bmp280_task_normal();

//
// Configuration data
//
#define APP_VERSION "0.0.9"


app_config_t app_config = {
    .wifi_ssid = WIFI_SSID,
    .wifi_password = WIFI_PASS,
    .led_gpio = 13,
    .sntp_servers = {"dixnas1.lan", "0.uk.pool.ntp.org", "1.uk.pool.ntp.org", "pool.ntp.org"},
    .queue_depth = MAX_QUEUE_DEPTH,
    .device = "nodeMCU",
    .location = "office",
    .influxdb_conf = {
        .server_name = "dixnas1.lan",
        .server_port = "8086",
        .dbname = "testdb"
    },
    .sensor_conf = {
        .i2c_bus = 0,
        .i2c_addr = 1,
        .scl_gpio = 0,
        .sda_gpio = 2,
        .poll_period = SENSOR_READ_RATE
    }
};
static Resources_t app_resources;


//
// Perform a brief flash of the LED when woken up
// and then go back to sleep
//
// TODO: Use xTaskNotify to pass option for blink rate etc.
void led_blink_task(void *pvParameters)
{
    debug("Initialising LED blink task");
    gpio_enable(app_config.led_gpio, GPIO_OUTPUT);

    while (1) {
        // Wait until I'm asked to flash the LED
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        debug("Time to blink..");
        gpio_write(app_config.led_gpio, 1);
        vTaskDelayMs(200);
        gpio_write(app_config.led_gpio, 0);
    }
}


#ifdef APP_DEBUG
// Periodically print out the stack space of each task
void
stats_task(void *pvParameters)
{
    TaskStatus_t taskStatus;

    debug("Initialising stats task");
    while (1) {
        FREEHEAP();
        vTaskGetInfo(app_resources.taskLedBlink, &taskStatus, pdTRUE, eInvalid);
        printf("LED: %d, ", taskStatus.usStackHighWaterMark);
        vTaskGetInfo(app_resources.taskWriteInfluxdb, &taskStatus, pdTRUE, eInvalid);
        printf("Influx: %d, ", taskStatus.usStackHighWaterMark);
        vTaskGetInfo(app_resources.taskBMP280, &taskStatus, pdTRUE, eInvalid);
        printf("BMP: %d, ", taskStatus.usStackHighWaterMark);

        // Show how much room is available in the queue
        printf("Queue has %d messages\n", (int)uxQueueMessagesWaiting(app_resources.sensorQueue));
        vTaskDelayMs(60000);
    }
}
#endif

//
// Wait until we are part of a WiFi network and then start main program tasks
//
void my_init_task(void *pvParameters)
{
	UNUSED_ARG(pvParameters);
    QueueHandle_t newQueue;

    /* Wait until we have joined AP and are assigned an IP */
	while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
		vTaskDelayMs(1000);
	}
;
	/* Start SNTP */
	INFO("Starting SNTP... ");
	// SNTP will request an update each 10 minutes
	// My calculations how the RTC drifts about 600ms every 10mins
    // Looking at sntp.c any value above 15000 is set to 15000
	sntp_set_update_delay(10*60000);
	// SNTP initialization
	// Don't use timezone as this has not been implemented correctly in esp_open-rtos
	sntp_initialize(NULL);
	/* Servers must be configured right after initialization */
    debug("1: %s, 2: %s, 3: %s, 4: %s",
        app_config.sntp_servers[0], app_config.sntp_servers[1], app_config.sntp_servers[2], app_config.sntp_servers[3] );
	sntp_set_servers(app_config.sntp_servers, MAX_SNTP_SERVERS);

    // Wait for time to be set
    while (time(NULL) < 1593383378) {
        debug("Waiting for time to be retrieved via NTP");
        vTaskDelayMs(1000);
    }

    // Create a queue for passing messages between sensor task and influx task
    newQueue = xQueueCreate(app_config.queue_depth, sizeof(sensor_reading_t));
    if (newQueue == NULL) {
        debug("Could not allocate sensorQueue");
        vTaskDelete(NULL);
    }
    app_resources.sensorQueue = newQueue;

    xTaskCreate(led_blink_task, "led_blink", 256, NULL, 2, &app_resources.taskLedBlink);
    xTaskCreate(write_influxdb_task, "write_influxdb", 384, (void *)&app_resources, 2, &app_resources.taskWriteInfluxdb);
    xTaskCreate(bmp280_task_normal, "bmp280", 384, (void *)&app_resources, 2, &app_resources.taskBMP280);
#ifdef APP_DEBUG
    xTaskCreate(stats_task, "Stats", 1024, NULL, 1, NULL);
#endif
	/* Print date and time each 60 seconds */
	while(1) {
		vTaskDelayMs(60000);
		time_t ts = time(NULL);
		printf("EPOCH: %ld, GMT: %s", (long)time(NULL), ctime(&ts));
	}

    vQueueDelete(newQueue);
    vTaskDelete(NULL);
}

void user_init(void)
{
    uart_set_baud(0, 115200);

    // Just some information
    printf("\n");
    printf("SDK version : %s\n", sdk_system_get_sdk_version());
    printf("APP version : %s\n", APP_VERSION);

    struct sdk_station_config sta_config;
    memset(&sta_config, 0, sizeof(sta_config));
    memcpy(sta_config.ssid, app_config.wifi_ssid, 32);
    memcpy(sta_config.password, app_config.wifi_password, 64);

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&sta_config);

    xTaskCreate(my_init_task, "Init Task", 1024, NULL, 1, NULL);
}
