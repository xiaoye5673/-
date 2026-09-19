#ifndef __EC20_H__
#define __EC20_H__

/* 4G模块初始化：拨号联网 + 打开AT串口 + 连接平台TCP */
int ec20_init(void);

/* 通过TCP向平台发送数据，返回实际发送字节数，失败返回-1 */
int ec20_send_data(char *data, int len);

/* 拨打报警电话（AT指令 ATD号码;），成功返回0 */
int ec20_call_phone(char *phone);

/* 挂断电话（ATH指令），成功返回0 */
int ec20_hangup(void);

/* 上传历史路径文件（uppath命令调用）
 * filename为NULL或空时使用默认"history_path.txt"
 * 返回成功上传的坐标点数，失败返回-1 */
int ec20_upload_path(const char *filename);

/* 通信线程：每10秒定时上报 + 报警自动拨号 + 监听平台下发指令 */
void *comm_thread(void *arg);

/* 关闭资源：关闭socket、AT串口、杀掉quectel-CM进程 */
void ec20_close(void);

#endif
