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
	/* SNTP will request an update each 5 minutes */
	sntp_set_update_delay(5*60000);
	/* Set GMT+0 zone, daylight savings on */
	const struct timezone tz = {0, 0};
	/* SNTP initialization */
	sntp_initialize(&tz);
	/* Servers must be configured right after initialization */
	sntp_set_servers(servers, sizeof(servers) / sizeof(char*));
	printf("DONE!\n");

	/* Print date and time each 30 seconds */
	while(1) {
		vTaskDelayMs(30000);
		time_t ts = time(NULL);
		printf("EPOCH: %ld, TIME: %s", (long)time(NULL), ctime(&ts));
	}
}
