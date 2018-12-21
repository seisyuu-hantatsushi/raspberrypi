TARGET = gpio_ctrl

DEBUG_TARGET = $(TARGET)_debug
RELEASE_TARGET = $(TARGET)

C_OBJS = main.o motor_driver.o
CXX_OBJS =

INCDIRS = -I../../../../../raspberrypi_sysroot/usr/include

LDFLAGS = -L../../../../../raspberrypi_sysroot/usr/lib -lpigpiod_if2 -lm

include  ../../tools/Makefile.common
