#ifndef ZLG72128_H
#define ZLG72128_H

int  zlg72128_init(void);
int  zlg72128_display_string(const char *str);
int  zlg72128_clear(void);
int  zlg72128_read_key(void);
void zlg72128_close(void);

/* 按键值：0=无按键，1-64=有按键（老师驱动返回原始键值） */
#define KEY_NONE   0

#endif
