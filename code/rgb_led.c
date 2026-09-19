#include "rgb_led.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* ================================================================
 * fs6818_led驱动（/dev/newled，主设备号500）
 * ioctl: LED_ON=_IOW('L',0,int), LED_OFF=_IOW('L',1,int)
 * 参数: RED_LED='r', GREEN_LED='g', BLUE_LED='b'
 * 注意：驱动里绿=GPB12, 蓝=GPE13，和硬件标注可能相反，测试后确认
 * ================================================================ */
#define LED_DEV "/dev/newled"
#define LED_ON   _IOW('L', 0, int)
#define LED_OFF  _IOW('L', 1, int)
#define RED_LED    'r'
#define GREEN_LED  'g'
#define BLUE_LED   'b'

static int led_fd = -1;

int rgb_led_init(void)
{
    led_fd = open(LED_DEV, O_RDWR);
    if (led_fd < 0) {
        perror("[rgb] open /dev/newled failed");
        return -1;
    }
    /* 初始全灭 */
    int nr;
    nr = RED_LED;   ioctl(led_fd, LED_OFF, &nr);
    nr = GREEN_LED; ioctl(led_fd, LED_OFF, &nr);
    nr = BLUE_LED;  ioctl(led_fd, LED_OFF, &nr);
    printf("[rgb] init done (/dev/newled)\n");
    return 0;
}

void rgb_set(int r, int g, int b)
{
    int nr;
    /* 红灯 = GPA28，正常 */
    nr = RED_LED;
    if (r) ioctl(led_fd, LED_ON, &nr);
    else   ioctl(led_fd, LED_OFF, &nr);

    /* 驱动里绿=GPB12(实际蓝灯), 蓝=GPE13(实际绿灯)，所以g和b互换 */
    nr = BLUE_LED;   /* 实际是绿灯 */
    if (g) ioctl(led_fd, LED_ON, &nr);
    else   ioctl(led_fd, LED_OFF, &nr);

    nr = GREEN_LED;  /* 实际是蓝灯 */
    if (b) ioctl(led_fd, LED_ON, &nr);
    else   ioctl(led_fd, LED_OFF, &nr);
}


void rgb_led_close(void)
{
    if (led_fd >= 0) {
        int nr;
        nr = RED_LED;   ioctl(led_fd, LED_OFF, &nr);
        nr = GREEN_LED; ioctl(led_fd, LED_OFF, &nr);
        nr = BLUE_LED;  ioctl(led_fd, LED_OFF, &nr);
        close(led_fd);
        led_fd = -1;
    }
}
