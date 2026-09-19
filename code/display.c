#include "display.h"
#include "global.h"
#include "zlg72128.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ================================================================
 * 显示项定义（共7项：4个传感器 + 3个动作器件状态）
 * 数码管用数字编号（ZLG72128B只支持0-9/A-F/空格/-）
 * 控制台输出中文名称
 * ================================================================ */
static const char *item_disp[]  = {"1", "2", "3", "4", "5", "6", "7"};
static const char *item_names[] = {"酒精", "光敏", "火焰", "气体", "风扇", "蜂鸣器", "报警"};
#define ITEM_COUNT 7

/* 上翻键：左列 4(20) 7(12) *(4) */
static int is_key_up(int key)
{
    return (key == 20 || key == 12 || key == 4);
}

/* 下翻键：右列 6(18) 9(10) #(2) */
static int is_key_down(int key)
{
    return (key == 18 || key == 10 || key == 2);
}

/**
 * @brief  数码管显示+按键线程（通用功能1）
 *
 * 功能：
 *   - 按键上翻/下翻切换显示项，数码管显示编号+数值，控制台输出类型+数值
 *   - 10秒无按键自动进入缺省状态，循环显示每项3秒，不输出控制台
 */
void *display_thread(void *arg)
{
    int current_item = 0;
    int last_key = 0;
    time_t last_key_time = time(NULL);
    time_t auto_timer = 0;
    int auto_item = 0;
    char disp_buf[16];

    while (1) {
        int key = zlg72128_read_key();

        if (key != 0 && key != last_key) {
            last_key = key;
            last_key_time = time(NULL);

            /* 解析按键：上翻/下翻 */
            if (is_key_up(key)) {
                current_item = (current_item + ITEM_COUNT - 1) % ITEM_COUNT;
            } else if (is_key_down(key)) {
                current_item = (current_item + 1) % ITEM_COUNT;
            } else {
                usleep(100000);
                continue;  /* 其他键忽略 */
            }

            /* 读取当前显示项的值 */
            STATE_LOCK();
            int val = 0;
            switch (current_item) {
            case 0: val = g_state.alcohol;   break;  /* 酒精AD值 */
            case 1: val = g_state.light;     break;  /* 光敏AD值 */
            case 2: val = g_state.flame;     break;  /* 火焰AD值 */
            case 3: val = g_state.gas;       break;  /* 气体AD值 */
            case 4: val = g_state.fan_on;    break;  /* 风扇：1开0关 */
            case 5: val = g_state.buzzer_on; break;  /* 蜂鸣器：1响0停 */
            case 6: val = g_state.alarm;     break;  /* 报警：1是0否 */
            }
            STATE_UNLOCK();

            /* 数码管显示：编号 + 空格 + 4位数值（如 "1 1004"）*/
            sprintf(disp_buf, "%s %4d", item_disp[current_item], val);
            zlg72128_display_string(disp_buf);

            /* 控制台同时输出数据类型和数值（功能1要求：数码管不支持汉字，控制台补充）*/
            printf("[数码管] %s: %d\n", item_names[current_item], val);
        }
        else if (key == 0) {
            last_key = 0;

            /* 10秒无按键 → 自动循环显示，每项3秒，不输出控制台 */
            if (time(NULL) - last_key_time > 10) {
                if (time(NULL) - auto_timer >= 3) {
                    auto_timer = time(NULL);
                    auto_item = (auto_item + 1) % ITEM_COUNT;

                    STATE_LOCK();
                    int val = 0;
                    switch (auto_item) {
                    case 0: val = g_state.alcohol;   break;
                    case 1: val = g_state.light;     break;
                    case 2: val = g_state.flame;     break;
                    case 3: val = g_state.gas;       break;
                    case 4: val = g_state.fan_on;    break;
                    case 5: val = g_state.buzzer_on; break;
                    case 6: val = g_state.alarm;     break;
                    }
                    STATE_UNLOCK();

                    sprintf(disp_buf, "%s %4d", item_disp[auto_item], val);
                    zlg72128_display_string(disp_buf);
                    /* 自动循环不输出控制台，防止与其他操作冲突 */
                }
            }
        }

        usleep(100000);  /* 100ms轮询一次按键 */
    }
    return NULL;
}
