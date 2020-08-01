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

#define APP_DEBUG true
#include "bmp280_influxdb.h"

extern const app_config_t app_config;


#define INFLUXDB_DATA_LEN 201
#define HTTP_POST_REQ_LEN 500
#if( INCLUDE_vTaskSuspend != 1 )
    #error INCLUDE_vTaskSuspend must be set to 1 if configUSE_TICKLESS_IDLE is not set to 0
#endif /* INCLUDE_vTaskSuspend */

int open_socket_on_influxdb(char *server_name, char *server_port)
{
    struct addrinfo *res = NULL;
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    // Resolve the name of the server
    int ret_code = getaddrinfo(server_name, server_port, &hints, &res);
    if (ret_code != 0) {
        debug("Unable to resolve hostname. Error=%d", ret_code);
        return -1;
    }

    // Open a socket on the influxdb server
    int s = socket(res->ai_family, res->ai_socktype, 0);
    if(s < 0) {
        ERROR("Failed to allocate socket.");
        freeaddrinfo(res);
        return -1;
    }

    // Connect to the influxdb server socket
    if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
        close(s);
        freeaddrinfo(res);
        ERROR("socket connect failed.");
        return -1;
    }

    // Free up the resolved name info as we don't need it anymore
    freeaddrinfo(res);

    // Return a connected socket
    return s;
}

void write_influxdb_task(void *pvParameters)
{
    debug("Starting InfluxDB write task...");

    while(1) {
        QueueHandle_t sensor_queue = ((Resources_t *)pvParameters)->sensorQueue;
        SensorReading_t sensor_reading;

        // Wait for a sensor reading or time out after 60 seconds
        debug("Wait for next sensor reading or timeout...");
        if (xQueueReceive(sensor_queue, &sensor_reading, 60000/portTICK_PERIOD_MS) != pdPASS) {
            INFO("Timed out waiting for a sensor reading");
            continue;
        }

        // We have a reading to send to influxdb, so let's connect to it
        // and write the values into the database
        int sock_influx = open_socket_on_influxdb(app_config.influxdb_conf.server_name, app_config.influxdb_conf.server_port);
        char *value_buffer = malloc(INFLUXDB_DATA_LEN);
        char *post_request = malloc(HTTP_POST_REQ_LEN);
        bool reading_sent = true;  // Assume success
        debug("sock_influx: %d, vb: 0x%p, pr: 0x%p", sock_influx, value_buffer, post_request);
        if (sock_influx >= 0 && value_buffer != NULL && post_request != NULL) {
            // We have a connection to the server, so lets send the reading
            // Build the influxdb data string
            // influxdb expects Unix epoch time in nanoseconds so add 9 zeros
            int buffer_size = snprintf(value_buffer, INFLUXDB_DATA_LEN,
                "sensor,location=office,device=nodeMCU,type=bme280 pressure=%.2f,temperature=%.2f,humidity=%.2f %ld000000000",
                sensor_reading.pressure, sensor_reading.temperature, sensor_reading.humidity, (long)sensor_reading.readingTime);

            // Create the HTTP POST request
            snprintf(post_request, HTTP_POST_REQ_LEN, "POST /write?db=%s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: bmp280_influxdb/0.1 esp8266\r\n"
                "Content-type: text/html\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s"
                "\r\n",
                app_config.influxdb_conf.dbname,
                app_config.influxdb_conf.server_name,
                buffer_size, value_buffer);

            // Send the request
            debug("Request:\n%s", post_request);
            if (write(sock_influx, post_request, strlen(post_request)) >= 0) {
                // Check for a response
                char *post_response = calloc(512, sizeof(char));
                if (post_response != NULL) {
                    read(sock_influx, post_response, 511);
                    if (strstr(post_response, "204 No Content") == NULL || errno != 0) {
                        ERROR("%s\nerrno(%d)", post_response, errno);
                        if (errno != 0) errno = 0;
                    } else {
                        debug("Response\n%s", post_response);
                    }
                    free(post_response);
                }
            } else {
                ERROR("socket send failed");
                reading_sent = false;
            }
        } else {
            reading_sent = false;
        }

        // Put the message back on the queue if we failed to send it
        if (reading_sent == false) {
            // If we fail to put it on the queue it will be because it is full
            xQueueSendToFront(sensor_queue, &sensor_reading, 0);
            taskYIELD();
        }
        // Clean up resources we have used
        if (post_request != NULL) {
            free(post_request);
        }
        if (value_buffer != NULL) {
            free(value_buffer);
        }
        if (sock_influx >= 0) {
            close(sock_influx);
        }
    }
}
