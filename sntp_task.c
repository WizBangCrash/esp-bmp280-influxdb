//
// ntp_task
//
// Periodically sends a UDP NTP request and then sets the internal time
//
#include <espressif/esp_common.h>
#include <esp/uart.h>

#include <string.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#include <ssid_config.h>

/* Add extras/sntp component to makefile for this include to work */
#include <sntp.h>
#include <time.h>

#include "bmp280_influxdb.h"

#define SNTP_SERVERS 	"0.pool.ntp.org", "1.pool.ntp.org", \
						"2.pool.ntp.org", "3.pool.ntp.org"

#define UNUSED_ARG(x)	(void)x

#ifdef INCLUDE_TIME
void sntp_task(void *pvParameters)
{
	const char *servers[] = {SNTP_SERVERS};
	UNUSED_ARG(pvParameters);

	/* Wait until we have joined AP and are assigned an IP */
	while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
		vTaskDelayMs(1000);
	}

	/* Start SNTP */
	printf("Starting SNTP... ");
	// SNTP will request an update each 10 minutes
	// My calculations how the RTC drifts about 600ms every 10mins
	sntp_set_update_delay(10*60000);
	// SNTP initialization
	// Don't use timezone as this has not been implemented correctly in esp_open-rtos
	sntp_initialize(NULL);
	/* Servers must be configured right after initialization */
	sntp_set_servers(servers, sizeof(servers) / sizeof(char*));
	printf("DONE!\n");

	/* Print date and time each 60 seconds */
	while(1) {
		vTaskDelayMs(60000);
		time_t ts = time(NULL);
		printf("EPOCH: %ld, GMT: %s", (long)time(NULL), ctime(&ts));
	}
}
#endif