#ifndef SENSOR_H
#define SENSOR_H

#define SENSOR_ALCOHOL  1   /* AD1 */
#define SENSOR_LIGHT    2   /* AD2 */
#define SENSOR_FLAME    3   /* AD3 */
#define SENSOR_GAS      4   /* AD4 */

int  sensor_init(void);
int  sensor_read(int channel);   /* 读取指定通道AD值，返回-1失败 */
void sensor_close(void);
void *sensor_thread(void *arg);  /* 传感器采集线程 */

#endif
