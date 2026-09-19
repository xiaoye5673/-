/* relay.h */
#ifndef RELAY_H
#define RELAY_H

int relay_init(void);
void relay_on(void);    // 开风扇（应用层1=开）
void relay_off(void);   // 关风扇
void relay_close(void);

#endif