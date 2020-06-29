/* http_get - Retrieves a web page over HTTP GET.
 *
 * See http_get_ssl for a TLS-enabled version.
 *
 * This sample code is in the public domain.,
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"

#include <unistd.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "bmp280_influxdb.h"

#define WEB_SERVER "dixnas1.lan"
#define WEB_PORT "8086"
#define WEB_PATH "/write?db=testdb"

struct addrinfo *resolve_hostname(char *hostname, char *port)
{
    struct addrinfo *res = NULL;
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    while(res == NULL) {
        debug("Running DNS lookup for %s...", hostname);
        int err = getaddrinfo(hostname, port, &hints, &res);

        if (err != 0 || res == NULL) {
            printf("DNS lookup failed err=%d res=%p\r\n", err, res);
            if(res)
                freeaddrinfo(res);
            vTaskDelayMs(1000);
            continue;
        }
    }
    struct sockaddr *sa = res->ai_addr;
    if (sa->sa_family == AF_INET) {
        debug("DNS lookup succeeded. IP=%s\n", inet_ntoa(((struct sockaddr_in *)sa)->sin_addr));
    }

    return res;
}

#define INFLUXDB_DATA_LEN 100
#define HTTP_POST_REQ_LEN 500

void write_influxdb_task(void *pvParameters)
{
    Environment_t *environment = &((Resources_t *)pvParameters)->environment;
    struct addrinfo *host_addrinfo = NULL;

    char *value_buffer = malloc(INFLUXDB_DATA_LEN);
    debug("Initialising HTTP POST task...\n");

    // Wait for time to be set
    while (time(NULL) < 1593383378) {
        debug("Waiting to time to be retrieved\n");
        vTaskDelayMs(1000);
    }

    while(1) {
        // Wait for a sensor reading to complete
        debug("Wait for next sensor reading...\n")
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Wait for Wifi Station Connection
        while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
            printf("Waiting for station connect...\n");
            vTaskDelayMs(1000);
        }

        // Get IP address of hostname
        host_addrinfo = resolve_hostname(WEB_SERVER, WEB_PORT);

        int s = socket(host_addrinfo->ai_family, host_addrinfo->ai_socktype, 0);
        if(s < 0) {
            printf("... Failed to allocate socket.\r\n");
            freeaddrinfo(host_addrinfo);
            vTaskDelayMs(1000);
            continue;
        }

        if(connect(s, host_addrinfo->ai_addr, host_addrinfo->ai_addrlen) != 0) {
            close(s);
            freeaddrinfo(host_addrinfo);
            printf("... socket connect failed.\r\n");
            vTaskDelayMs(4000);
            continue;
        }

        freeaddrinfo(host_addrinfo);

        // Build the influxdb data string
#ifdef INCLUDE_TIME
        // influxdb expects Unix epoch time in nanoseconds so add 9 zeros
        int buffer_size = snprintf(value_buffer, INFLUXDB_DATA_LEN,\
            "sensor,location=office,device=bmp280 pressure=%.2f,temperature=%.2f %ld000000000", \
            environment->pressure, environment->temperature, (long)time(NULL));
#else
        int buffer_size = snprintf(value_buffer, INFLUXDB_DATA_LEN,\
            "sensor,location=office,device=bmp280 pressure=%.2f,temperature=%.2f", \
            environment->pressure, environment->temperature);
#endif
        // Create the HTTP POST request
        char *post_request = malloc(HTTP_POST_REQ_LEN);
        snprintf(post_request, HTTP_POST_REQ_LEN, "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: esp-open-rtos/0.1 esp8266\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s"
            "\r\n", WEB_PATH, WEB_SERVER, buffer_size, value_buffer);

        debug("Request:\n%s", post_request);
        if (write(s, post_request, strlen(post_request)) < 0) {
            printf("... socket send failed\r\n");
            free(post_request);
            close(s);
            vTaskDelayMs(4000);
            continue;
        }

        // Check for a response
        char *post_response = calloc(512, sizeof(char));
        read(s, post_response, 511);
        if (strstr(post_response, "204 No Content") == NULL || errno != 0) {
            printf("%s\nerrno(%d)\n", post_response, errno);
            if (errno != 0) errno = 0;
        }
        free(post_response);
        free(post_request);

        close(s);
    }
}
