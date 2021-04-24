TARGET = gpio_ctrl

DEBUG_TARGET = $(TARGET)_debug
RELEASE_TARGET = $(TARGET)

RASPBERRYPI_ROOTFS = ../../../../raspberrypi/rootfs

C_OBJS = main.o motor_driver.o
CXX_OBJS =

INCDIRS = -I$(RASPBERRYPI_ROOTFS)/usr/include

LDFLAGS_APPEND = -L$(RASPBERRYPI_ROOTFS)/usr/lib -lpigpiod_if2 -lm

include  ../../tools/Makefile.common
