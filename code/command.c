#include "command.h"
#include "global.h"
#include "relay.h"
#include "buzzer.h"
#include <stdio.h>
#include <string.h>
#include "ec20.h"

static void show_help(void)
{
    printf("\n========== 命令列表 ==========\n");
    printf("  ? 或 help       显示帮助\n");
    printf("  info           查看当前传感器值和状态\n");
    printf("  fan on/off     手动开关风扇\n");
    printf("  buzzer on/off  手动开关蜂鸣器\n");
    printf("  alarm off      手动取消报警（处理误报）\n");
    printf("  phone <号码>   设置报警电话\n");
    printf("  uppath <文件>  上传历史路径（需4G）\n");
    printf("  quit           退出程序\n");
    printf("==============================\n\n");
}

static void show_info(void)
{
    STATE_LOCK();
    printf("\n========== 当前状态 ==========\n");
    printf("  酒精: %d\n", g_state.alcohol);
    printf("  光敏: %d\n", g_state.light);
    printf("  火焰: %d\n", g_state.flame);
    printf("  气体: %d\n", g_state.gas);
    printf("  风扇: %s\n", g_state.fan_on ? "开" : "关");
    printf("  蜂鸣: %s\n", g_state.buzzer_on ? "响" : "停");
    printf("  报警: %s\n", g_state.alarm ? "是" : "否");
    printf("  网络: %s\n", g_state.net_ok ? "正常" : "异常");
    printf("  GPS : %s\n", g_state.gps_ok ? "正常" : "异常");
    printf("  电话: %s\n", g_state.alarm_phone);
    printf("==============================\n\n");
    STATE_UNLOCK();
}

void command_loop(void)
{
    char cmd[256];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(cmd, sizeof(cmd), stdin) == NULL)
            continue;
        cmd[strcspn(cmd, "\n")] = 0;

        if (strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0) {
            show_help();
        } else if (strcmp(cmd, "info") == 0) {
            show_info();
        } else if (strncmp(cmd, "fan", 3) == 0) {
            if (strstr(cmd, "on")) {
                relay_on();
                STATE_LOCK(); g_state.fan_on = 1; STATE_UNLOCK();
                printf("风扇已打开\n");
            } else if (strstr(cmd, "off")) {
                relay_off();
                STATE_LOCK(); g_state.fan_on = 0; STATE_UNLOCK();
                printf("风扇已关闭\n");
            }
        } else if (strncmp(cmd, "buzzer", 6) == 0) {
            if (strstr(cmd, "on")) {
                buzzer_on();
                STATE_LOCK(); g_state.buzzer_on = 1; STATE_UNLOCK();
                printf("蜂鸣器已开启\n");
            } else if (strstr(cmd, "off")) {
                buzzer_off();
                STATE_LOCK(); g_state.buzzer_on = 0; STATE_UNLOCK();
                printf("蜂鸣器已关闭\n");
            }
        } else if (strncmp(cmd, "alarm", 5) == 0) {
            if (strstr(cmd, "off") || strstr(cmd, "reset") || strstr(cmd, "clear")) {
                /* 手动取消报警：清报警标志 + 关蜂鸣器 + 关风扇 */
                STATE_LOCK();
                g_state.alarm = 0;
                STATE_UNLOCK();
                buzzer_off();
                relay_off();
                STATE_LOCK();
                g_state.buzzer_on = 0;
                g_state.fan_on = 0;
                STATE_UNLOCK();
                printf("报警已手动取消（如传感器仍超阈值，1秒后将自动重新报警）\n");
            } else {
                printf("用法: alarm off  （手动取消报警，处理误报）\n");
            }
        } else if (strncmp(cmd, "phone", 5) == 0) {
            char *p = cmd + 6;
            while (*p == ' ') p++;
            if (*p) {
                STATE_LOCK();
                strncpy(g_state.alarm_phone, p, sizeof(g_state.alarm_phone)-1);
                STATE_UNLOCK();
                printf("报警电话已设为: %s\n", p);
            } else {
                printf("用法: phone <电话号码>\n");
            }
        
        } else if (strncmp(cmd, "uppath", 6) == 0) {
            char *f = cmd + 6;
            while (*f == ' ') f++;
            if (*f) {
                ec20_upload_path(f);          /* 指定文件名 */
            } else {
                ec20_upload_path("history_path.txt");  /* 默认文件 */
            }
        
        } else if (strcmp(cmd, "quit") == 0) {
            printf("退出程序\n");
            break;
        } else if (strlen(cmd) > 0) {
            printf("未知命令: %s，输入 ? 查看帮助\n", cmd);
        }
    }
}
