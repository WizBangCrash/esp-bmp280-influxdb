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

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "ssid_config.h"

#include "bmp280_influxdb.h"

#define WEB_SERVER "dixnas1.lan"
#define WEB_PORT "8086"
#define WEB_PATH "/write?db=testdb"

extern float pressure, temperature;

// sdk_wifi_station_get_connect_status

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
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
    }
    struct sockaddr *sa = res->ai_addr;
    if (sa->sa_family == AF_INET) {
        debug("DNS lookup succeeded. IP=%s\n", inet_ntoa(((struct sockaddr_in *)sa)->sin_addr));
    }

    return res;
}

void http_post_task(void *pvParameters)
{
    struct addrinfo *host_addrinfo = NULL;

    char value_buffer[50];
    printf("HTTP get task starting...\r\n");

    while(1) {
        // Wait for a sensor reading to complete
        vTaskSuspend(NULL);

        // Wait for Wifi Station Connection
        while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
            printf("Waiting for station connect...\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        // Get IP address of hostname
        host_addrinfo = resolve_hostname(WEB_SERVER, WEB_PORT);

        int s = socket(host_addrinfo->ai_family, host_addrinfo->ai_socktype, 0);
        if(s < 0) {
            printf("... Failed to allocate socket.\r\n");
            freeaddrinfo(host_addrinfo);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if(connect(s, host_addrinfo->ai_addr, host_addrinfo->ai_addrlen) != 0) {
            close(s);
            freeaddrinfo(host_addrinfo);
            printf("... socket connect failed.\r\n");
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        freeaddrinfo(host_addrinfo);

        // Build the influxdb data string
        int buffer_size = sprintf(value_buffer, "sensor,device=nodemcu pressure=%.2f,temperature=%.2f", pressure, temperature);
        // Create the HTTP POST request
        char *post_request = malloc(500);
        sprintf(post_request, "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: esp-open-rtos/0.1 esp8266\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s"
            "\r\n", WEB_PATH, WEB_SERVER, buffer_size, value_buffer);

        if (write(s, post_request, strlen(post_request)) < 0) {
            printf("... socket send failed\r\n");
            free(post_request);
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        // Check for a response
        char *post_response = calloc(512, sizeof(char));
        read(s, post_response, 511);
        if (strstr(post_response, "204 No Content") == NULL || errno != 0) {
            printf("%s\nerrno(%d)\n", post_response, errno);
        }
        free(post_response);
        free(post_request);

        close(s);
    }
}

#ifdef NOTDEFINED
void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(&http_post_task, "post_task", 384, NULL, 2, NULL);
}
#endif
