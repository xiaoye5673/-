#include "buzzer.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* ================================================================
 * fs6818_pwm 硬件PWM驱动（/dev/pwm，Timer2输出，GPIOC14引脚）
 * 无源蜂鸣器需要2-4kHz方波，硬件PWM能产生，GPIO模拟方式不行
 * ioctl命令：
 *   PWM_ON  = _IO('K',0)  启动PWM输出
 *   PWM_OFF = _IO('K',1)  停止PWM输出
 *   SET_CNT = _IOW('K',3,int)  设置频率参数（data越大频率越低）
 * ================================================================ */
#define PWM_ON   _IO('K', 0)
#define PWM_OFF  _IO('K', 1)
#define SET_CNT  _IOW('K', 3, int)

#define PWM_DEV "/dev/pwm"

static int pwm_fd = -1;
static int pwm_data = 150;  /* 频率参数：data越大频率越低，150约对应2-3kHz（蜂鸣器谐振范围，声音最大）*/

/**
 * @brief  初始化蜂鸣器（打开/dev/pwm，设置PWM频率）
 * @return 0成功，-1失败
 */
int buzzer_init(void)
{
    pwm_fd = open(PWM_DEV, O_RDWR);
    if (pwm_fd < 0) {
        perror("[buzzer] open /dev/pwm failed");
        return -1;
    }
    /* 设置PWM频率参数（必须传int指针，驱动用copy_from_user读取）*/
    int data = pwm_data;
    ioctl(pwm_fd, SET_CNT, &data);
    printf("[buzzer] init done (/dev/pwm, data=%d)\n", pwm_data);
    return 0;
}

/**
 * @brief  蜂鸣器开始响（启动PWM输出）
 */
void buzzer_on(void)
{
    if (pwm_fd < 0) return;
    ioctl(pwm_fd, PWM_ON, 0);
}

/**
 * @brief  蜂鸣器停止响（停止PWM输出）
 */
void buzzer_off(void)
{
    if (pwm_fd < 0) return;
    ioctl(pwm_fd, PWM_OFF, 0);
}

/**
 * @brief  关闭蜂鸣器设备
 */
void buzzer_close(void)
{
    if (pwm_fd >= 0) {
        ioctl(pwm_fd, PWM_OFF, 0);
        close(pwm_fd);
        pwm_fd = -1;
    }
}
