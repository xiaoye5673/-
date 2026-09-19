# ========== 配置区 ==========
CROSS_COMPILE ?= arm-none-linux-gnueabi-
KERNELDIR ?= /home/linux/Desktop/linux-3.4.39
INSTALL_DIR ?= /source/rootfs/app

# ========== 以下不用改 ==========
CC := $(CROSS_COMPILE)gcc
APP_CFLAGS := -Wall -O2 -pthread
LDFLAGS := -lpthread

TARGET = monitor
OBJS = main.o sensor.o relay.o buzzer.o zlg72128.o ec20.o command.o display.o rgb_led.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(APP_CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(APP_CFLAGS) -c $< -o $@

install: all
	mkdir -p $(INSTALL_DIR)
	cp -f $(TARGET) $(INSTALL_DIR)/
	@echo "已安装到 $(INSTALL_DIR)/"

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all install clean
