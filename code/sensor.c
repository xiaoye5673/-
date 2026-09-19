#include "sensor.h"
#include "global.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>

/* 老师ADC驱动的ioctl命令码：切换通道 */
#define SET_CHANNEL _IO('A', 0)

/* ================================================================
 * ADC通道映射（通过adc_test实测确认）
 *   开发板Arduino扩展板拨码：AD1=酒精, AD2=光敏, AD3=火焰, AD4=气体
 * ================================================================ */
#define CH_ALCOHOL  7      /* 酒精传感器 → ADC通道7 */
#define CH_LIGHT    6      /* 光敏传感器 → ADC通道6 */
#define CH_FLAME    3      /* 火焰传感器 → ADC通道3 */
#define CH_GAS      5      /* 气体传感器 → ADC通道5 */

static int adc_fd = -1;   /* ADC设备文件描述符 */

/**
 * @brief  初始化传感器（打开/dev/adc）
 * @return 0成功，-1失败
 */
int sensor_init(void)
{
    adc_fd = open("/dev/adc", O_RDWR);
    if (adc_fd < 0) {
        perror("[sensor] open /dev/adc failed");
        return -1;
    }
    printf("[sensor] init done\n");
    return 0;
}

/**
 * @brief  读取指定ADC通道的12位AD值
 * @param  channel 通道号0~7
 * @return 12位AD值（0~4095），失败返回-1
 */
int sensor_read(int channel)
{
    int data = 0;
    if (adc_fd < 0) return -1;
    if (channel < 0 || channel > 7) return -1;

    /* 第一步：ioctl切换到指定通道 */
    if (ioctl(adc_fd, SET_CHANNEL, channel) < 0) {
        perror("[sensor] ioctl SET_CHANNEL failed");
        return -1;
    }
    /* 第二步：读取4字节int（老师驱动要求count必须等于sizeof(int)）*/
    if (read(adc_fd, &data, sizeof(int)) != sizeof(int)) {
        perror("[sensor] read failed");
        return -1;
    }
    return data & 0xFFF;  /* 取低12位 */
}

/**
 * @brief  关闭传感器设备
 */
void sensor_close(void)
{
    if (adc_fd >= 0) close(adc_fd);
}

/**
 * @brief  传感器采集线程（子线程，每秒采样一次并判断报警）
 *
 * 采集4路传感器 → 更新全局状态 → 判断是否超阈值 → 设置alarm标志
 * alarm标志被alarm_thread读取，自动联动蜂鸣器和风扇
 */
void *sensor_thread(void *arg)
{
    while (1) {
        int a, l, f, g;

        /* 依次读取4路传感器 */
        a = sensor_read(CH_ALCOHOL);  /* 酒精 */
        l = sensor_read(CH_LIGHT);    /* 光敏 */
        f = sensor_read(CH_FLAME);    /* 火焰 */
        g = sensor_read(CH_GAS);      /* 气体 */

        STATE_LOCK();

        /* 更新传感器值（读取失败则保留上一次的值）*/
        if (a >= 0) g_state.alcohol = a;
        if (l >= 0) g_state.light   = l;
        if (f >= 0) g_state.flame   = f;
        if (g >= 0) g_state.gas     = g;

        /* ================================================================
         * 报警判断：4个传感器任一超阈值即触发报警
         *   酒精 > 1200  → 酒精浓度超标
         *   气体 > 400   → 气体浓度超标
         *   火焰 < 2000  → 检测到火焰（值越小火焰越强）
         *   光敏 < 1000  → 检测到强光（火灾产生强光，值越小光照越强）
         * ================================================================ */
        int trigger = 0;
        if (g_state.alcohol > ALCOHOL_THRESHOLD) trigger = 1;
        if (g_state.gas     > GAS_THRESHOLD)     trigger = 1;
        if (g_state.flame   < FLAME_THRESHOLD)   trigger = 1;
        if (g_state.light   < LIGHT_THRESHOLD)   trigger = 1;

        /* 状态变化时打印提示（只在跳变时打印，避免刷屏）*/
        if (trigger && !g_state.alarm)
            printf("\n[报警] 警报触发！蜂鸣器+风扇已启动\n");
        if (!trigger && g_state.alarm)
            printf("\n[报警] 警报解除\n");

        /* 更新报警标志和系统状态 */
        g_state.alarm  = trigger;
        g_state.status = trigger ?
                         (g_state.net_ok ? STATE_ALARM : STATE_ALARM_NONET) :
                         STATE_NORMAL;

        STATE_UNLOCK();

        sleep(1);  /* 每秒采样一次 */
    }
    return NULL;
}
