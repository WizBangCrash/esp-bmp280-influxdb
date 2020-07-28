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
#include "queue.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

// #define APP_DEBUG true
#include "bmp280_influxdb.h"


#define INFLUXDB_SERVER "dixnas1.lan"
#define INFLUXDB_PORT "8086"
#define INFLUXDB_PATH "/write?db=testdb"
#define INFLUXDB_DATA_LEN 201
#define HTTP_POST_REQ_LEN 500
#if( INCLUDE_vTaskSuspend != 1 )
    #error INCLUDE_vTaskSuspend must be set to 1 if configUSE_TICKLESS_IDLE is not set to 0
#endif /* INCLUDE_vTaskSuspend */


void write_influxdb_task(void *pvParameters)
{
    Resources_t *task_list = (Resources_t *)pvParameters;

    debug("Initialising HTTP POST task...");

    while(1) {
        SensorReading_t sensor_reading;
        struct addrinfo *res = NULL;
        const struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
        };

        // Wait for a sensor reading or time out after 60 seconds
        debug("Wait for next sensor reading or timeout...");
        if (xQueuePeek(task_list->sensorQueue, &sensor_reading, 60000/portTICK_PERIOD_MS) != pdPASS) {
            INFO("Timed out waiting for a sensor reading");
            continue;
        }

        // We have a reading to send to influxdb, so let's connect to it
        // and write the values into the database

        // Resolve the name of the server
        int ret_code = getaddrinfo(INFLUXDB_SERVER, INFLUXDB_PORT, &hints, &res);
        if (ret_code != 0) {
            debug("Unable to resolve hostname. Error=%d", ret_code);
            continue;
        }

        // Open a socket on the influxdb server
        int s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ERROR("Failed to allocate socket.");
            freeaddrinfo(res);
            continue;
        }

        // Connect to the influxdb server socket
        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            close(s);
            freeaddrinfo(res);
            ERROR("socket connect failed.");
            continue;
        }

        // Free up the resolved name info as we don't need it anymore
        freeaddrinfo(res);

        // Build the influxdb data string
        // influxdb expects Unix epoch time in nanoseconds so add 9 zeros
        char *value_buffer = malloc(INFLUXDB_DATA_LEN);
        int buffer_size = snprintf(value_buffer, INFLUXDB_DATA_LEN,
            "sensor,location=office,device=nodeMCU,type=bme280 pressure=%.2f,temperature=%.2f,humidity=%.2f %ld000000000",
            sensor_reading.pressure, sensor_reading.temperature, sensor_reading.humidity, (long)sensor_reading.readingTime);

        // Create the HTTP POST request
        char *post_request = malloc(HTTP_POST_REQ_LEN);
        snprintf(post_request, HTTP_POST_REQ_LEN, "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: bmp280_influxdb/0.1 esp8266\r\n"
            "Content-type: text/html\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s"
            "\r\n", INFLUXDB_PATH, INFLUXDB_SERVER, buffer_size, value_buffer);

        // Send the request
        debug("Request:\n%s", post_request);
        if (write(s, post_request, strlen(post_request)) < 0) {
            ERROR("socket send failed");
            free(post_request);
            free(value_buffer);
            close(s);
            continue;
        }

        // Check for a response
        char *post_response = calloc(512, sizeof(char));
        read(s, post_response, 511);
        if (strstr(post_response, "204 No Content") == NULL || errno != 0) {
            ERROR("%s\nerrno(%d)", post_response, errno);
            if (errno != 0) errno = 0;
        } else {
            debug("Response: %s", post_response);
        }

        free(post_response);
        free(post_request);
        free(value_buffer);
        close(s);
        // We successfully sent the reading to influxdb so remove it from the queue
        xQueueReceive(task_list->sensorQueue, &sensor_reading, 0);
    }
}
