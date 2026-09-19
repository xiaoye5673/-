#ifndef GLOBAL_H
#define GLOBAL_H
#include <pthread.h>

/* ================================================================
 * 报警阈值（根据实际测试值设定，方便触发测试）
 * 酒精/气体：值越高越危险 → 大于阈值报警
 * 火焰/光敏：值越低越危险 → 小于阈值报警
 *   火焰：有火焰时传感器输出降低
 *   光敏：光照越强（火灾强光）AD值越小（正常~1520，打光~633）
 * ================================================================ */
#define ALCOHOL_THRESHOLD  1800   /* 酒精：大于1800报警（正常约1000）*/
#define GAS_THRESHOLD       500   /* 气体：大于500报警（正常约300）*/
#define FLAME_THRESHOLD    1000   /* 火焰：小于1000报警（正常约3400，值越小火焰越强）*/
#define LIGHT_THRESHOLD    1000   /* 光敏：小于1000报警（正常约1520，打光约633，值越小光照越强）*/

/* 系统运行状态 */
typedef enum {
    STATE_STARTING = 0,    /* 启动中 */
    STATE_NORMAL,          /* 正常运行 */
    STATE_ALARM,           /* 报警中（有网络）*/
    STATE_ALARM_NONET      /* 报警但无网络/GPS */
} SystemStatus;

/* 全局状态结构体：所有线程共享，用互斥锁保护 */
typedef struct {
    /* ---- 传感器AD值（12位，0~4095）---- */
    int alcohol;           /* 酒精传感器AD值 */
    int gas;               /* 气体传感器AD值 */
    int flame;             /* 火焰传感器AD值 */
    int light;             /* 光敏传感器AD值 */

    /* ---- 执行器状态 ---- */
    int fan_on;            /* 风扇/继电器：1=开，0=关 */
    int buzzer_on;         /* 蜂鸣器：1=响，0=停 */

    /* ---- 系统状态 ---- */
    SystemStatus status;   /* 当前运行状态 */
    int alarm;             /* 报警标志：1=报警，0=正常 */
    int net_ok;            /* 4G网络状态：1=正常，0=异常 */
    int gps_ok;            /* GPS状态：1=正常，0=异常 */

    /* ---- GPS数据 ---- */
    double latitude;       /* 纬度 */
    double longitude;      /* 经度 */

    /* ---- 报警电话（可由平台下发修改）---- */
    char alarm_phone[20];

    /* ---- 互斥锁：保护多线程对g_state的并发访问 ---- */
    pthread_mutex_t lock;
} SystemState;

/* 全局状态实例（在main.c中定义，其他文件通过extern引用）*/
extern SystemState g_state;

/* 线程安全的状态读写宏 */
#define STATE_LOCK()   pthread_mutex_lock(&g_state.lock)
#define STATE_UNLOCK() pthread_mutex_unlock(&g_state.lock)

#endif /* GLOBAL_H */
