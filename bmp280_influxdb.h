

// #define BMP280_INFLUX_DEBUG

#ifdef BMP280_INFLUX_DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s" fmt "\n", "bmpinflux: ", ## __VA_ARGS__);
#else
#define debug(fmt, ...)
#endif

