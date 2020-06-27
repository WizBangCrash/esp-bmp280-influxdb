PROGRAM=bmp280-influxdb
EXTRA_COMPONENTS = extras/i2c extras/bmp280 extras/sntp


PROJGITSHORTREV=\"$(shell git rev-parse --short -q HEAD 2> /dev/null)\"
ifeq ($(PROJGITSHORTREV),\"\")
  PROJGITSHORTREV="\"(nogit)\"" # (same length as a short git hash)
endif
CPPFLAGS += -DPROJGITSHORTREV=$(PROJGITSHORTREV)

# Debug Flags
# CPPFLAGS += -DBMP280_INFLUX_DEBUG
CPPFLAGS += -DconfigUSE_TRACE_FACILITY=1


# Settings for the ESP Dev board
FLASH_SIZE = 32
FLASH_MODE = dio
FLASH_SPEED = 80
ESPPORT = "/dev/tty.SLAB_USBtoUART"

include ../esp-open-rtos/common.mk


monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)

dave:
	echo $(ROOT)$(PROGRAM)
	cd $(ROOT)$(PROGRAM); git rev-parse --short -q HEAD

fred:
	echo $(PROJGITSHORTREV)
