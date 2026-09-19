#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "global.h"
#include "sensor.h"
#include "relay.h"
#include "buzzer.h"
#include "zlg72128.h"
#include "ec20.h"
#include "command.h"
#include "display.h"
#include "rgb_led.h"

/* 全局状态定义（其他文件通过extern引用）*/
SystemState g_state;

/* ================================================================
 * 报警执行线程：检测alarm标志变化，自动联动硬件
 *   报警触发 → 蜂鸣器响 + 风扇打开
 *   报警解除 → 蜂鸣器停 + 风扇关闭
 * 只在状态跳变时操作硬件，避免每秒重复写入
 * ================================================================ */
void *alarm_thread(void *arg)
{
    static int last_alarm = -1;  /* 记录上一次状态，-1确保首次必处理 */
    while (1) {
        STATE_LOCK();
        int alarm = g_state.alarm;
        /* 在读锁里把传感器值拷出来，避免打印时被其他线程修改 */
        int a = g_state.alcohol;
        int g = g_state.gas;
        int f = g_state.flame;
        int l = g_state.light;
        STATE_UNLOCK();

        if (alarm != last_alarm) {
            last_alarm = alarm;
            if (alarm) {
                /* 报警触发：蜂鸣器响 + 风扇打开 */
                buzzer_on();
                relay_on();
                STATE_LOCK();
                g_state.buzzer_on = 1;
                g_state.fan_on = 1;
                STATE_UNLOCK();

                /* 打印具体是哪个传感器触发的，以及当前值和阈值 */
                printf("\n[报警] 警报触发！触发源: ");
                if (a > ALCOHOL_THRESHOLD) printf("酒精(%d>%d) ", a, ALCOHOL_THRESHOLD);
                if (g > GAS_THRESHOLD)     printf("气体(%d>%d) ", g, GAS_THRESHOLD);
                if (f < FLAME_THRESHOLD)   printf("火焰(%d<%d) ", f, FLAME_THRESHOLD);
                if (l < LIGHT_THRESHOLD)   printf("光敏(%d<%d) ", l, LIGHT_THRESHOLD);
                printf("→ 蜂鸣器+风扇已启动\n");
            } else {
                /* 报警解除：蜂鸣器停 + 风扇关 */
                buzzer_off();
                relay_off();
                STATE_LOCK();
                g_state.buzzer_on = 0;
                g_state.fan_on = 0;
                STATE_UNLOCK();
                printf("\n[报警] 警报解除（所有传感器恢复正常）\n");
            }
            printf("> ");
            fflush(stdout);  /* 重新显示命令提示符 */
        }
        sleep(1);
    }
    return NULL;
}


/* ================================================================
 * RGB LED状态指示灯线程（通用功能3）
 *   (1) 起动状态：前3秒，绿200ms亮/300ms灭
 *   (2) 正常运行：绿200ms亮/1秒灭
 *   (3) 报警有网：红-红-绿-绿-蓝-蓝 快速闪烁
 *   (4) 报警无网/GPS：红-绿-蓝 轮流慢闪
 * ================================================================ */
/* RGB LED状态指示灯线程（通用功能3）*/
void *rgb_led_thread(void *arg)
{
    time_t start_time = time(NULL);
    while (1) {
        STATE_LOCK();
        int alarm  = g_state.alarm;
        int net_ok = g_state.net_ok;
        int gps_ok = g_state.gps_ok;
        STATE_UNLOCK();

        if (time(NULL) - start_time < 3) {
            /* (1) 起动状态：绿200ms亮，灭300ms */
            rgb_set(0, 1, 0); usleep(200000);
            rgb_set(0, 0, 0); usleep(300000);
        }
        else if (!alarm) {
            /* (2) 正常运行：绿200ms亮，灭1秒 */
            rgb_set(0, 1, 0); usleep(200000);
            rgb_set(0, 0, 0); usleep(1000000);
        }
        else if (net_ok && gps_ok) {
            /* (3) 报警状态：红100灭50 红100灭50 绿100灭50 绿100灭50 蓝100灭50 蓝100灭350 */
            rgb_set(1,0,0); usleep(100000); rgb_set(0,0,0); usleep(50000);
            rgb_set(1,0,0); usleep(100000); rgb_set(0,0,0); usleep(50000);
            rgb_set(0,1,0); usleep(100000); rgb_set(0,0,0); usleep(50000);
            rgb_set(0,1,0); usleep(100000); rgb_set(0,0,0); usleep(50000);
            rgb_set(0,0,1); usleep(100000); rgb_set(0,0,0); usleep(50000);
            rgb_set(0,0,1); usleep(100000); rgb_set(0,0,0); usleep(350000);
        }
        else {
            /* (4) 报警无网络/GPS：红300灭200 绿300灭200 蓝300灭200 */
            rgb_set(1,0,0); usleep(300000); rgb_set(0,0,0); usleep(200000);
            rgb_set(0,1,0); usleep(300000); rgb_set(0,0,0); usleep(200000);
            rgb_set(0,0,1); usleep(300000); rgb_set(0,0,0); usleep(200000);
        }
    }
    return NULL;
}


/* ================================================================
 * 主线程：初始化所有模块 → 创建子线程 → 进入命令行循环
 * 所有业务逻辑都在子线程中执行，主线程只负责命令行交互（通用功能2）
 * ================================================================ */
int main(int argc, char *argv[])
{
    pthread_t tid_sensor, tid_display, tid_comm, tid_alarm, tid_rgb;

    printf("========================================\n");
    printf("  危化品车辆监控终端 v1.0\n");
    printf("========================================\n\n");

    /* ---- 初始化全局状态 ---- */
    memset(&g_state, 0, sizeof(g_state));
    g_state.status = STATE_NORMAL;
    pthread_mutex_init(&g_state.lock, NULL);
    strcpy(g_state.alarm_phone, "18803315426");  /* 默认报警电话 */

    /* ---- 初始化各硬件模块（失败不退出，标记不可用后继续运行）---- */
    printf("[1/6] 初始化传感器...\n");
    sensor_init();

    printf("[2/6] 初始化继电器/风扇...\n");
    relay_init();

    printf("[3/6] 初始化蜂鸣器...\n");
    buzzer_init();

    printf("[4/6] 初始化数码管/按键...\n");
    zlg72128_init();

    printf("[5/6] 初始化4G模块...\n");
    ec20_init();

    printf("[6/6] 初始化RGB状态灯...\n");
    rgb_led_init();

    /* ---- 创建所有子线程 ---- */
    printf("\n创建工作线程...\n");
    pthread_create(&tid_sensor,  NULL, sensor_thread,  NULL);  /* 传感器采集+报警判断 */
    pthread_create(&tid_display, NULL, display_thread, NULL);  /* 数码管显示+按键 */
    pthread_create(&tid_comm,    NULL, comm_thread,    NULL);  /* 4G通信+上报 */
    pthread_create(&tid_alarm,   NULL, alarm_thread,   NULL);  /* 报警硬件联动 */
    pthread_create(&tid_rgb,     NULL, rgb_led_thread, NULL);  /* RGB状态灯 */

    printf("系统启动完成！输入 ? 查看命令列表\n\n");

    /* 主线程进入命令行循环（通用功能2：所有代码在子线程，主线程等命令）*/
    command_loop();

    /* ---- 清理资源 ---- */
    pthread_mutex_destroy(&g_state.lock);
    sensor_close();
    relay_close();
    buzzer_close();
    zlg72128_close();
    ec20_close();
    rgb_led_close();

    return 0;
}
