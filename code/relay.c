#include "relay.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define GPIO_RELAY  94

static int gpio_export(int gpio)
{
    int fd;
    char buf[64];
    sprintf(buf, "/sys/class/gpio/gpio%d", gpio);
    if (access(buf, F_OK) == 0)
        return 0;
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0)
        return -1;
    sprintf(buf, "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
}

static int gpio_set_dir(int gpio, int out)
{
    int fd;
    char buf[64];
    sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
    fd = open(buf, O_WRONLY);
    if (fd < 0)
        return -1;
    write(fd, out ? "out" : "in", out ? 3 : 2);
    close(fd);
    return 0;
}

static int gpio_set_value(int gpio, int val)
{
    int fd;
    char buf[64];
    sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
    fd = open(buf, O_WRONLY);
    if (fd < 0)
        return -1;
    write(fd, val ? "1" : "0", 1);
    close(fd);
    return 0;
}

int relay_init(void)
{
    gpio_export(GPIO_RELAY);
    gpio_set_dir(GPIO_RELAY, 1);
    relay_off();
    printf("[relay] init done (GPIO=%d)\n", GPIO_RELAY);
    return 0;
}

void relay_on(void)
{
    gpio_set_value(GPIO_RELAY, 0);
}

void relay_off(void)
{
    gpio_set_value(GPIO_RELAY, 1);
}

void relay_close(void)
{
    relay_off();
}
