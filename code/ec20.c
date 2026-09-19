#include "ec20.h"
#include "global.h"
#include "relay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>

/* ================================================================
 * 平台服务器配置
 * BIND_TO_4G = 1 : 强制上报走4G网卡usb0（公网/域名服务器用这个）
 * BIND_TO_4G = 0 : 走eth0有线（局域网服务器192.168.x.x测试时用这个）
 * ================================================================ */
#define BIND_TO_4G       1
#define SERVER_IP        "43.173.97.251"   /* 平台TCP服务器域名 */
#define SERVER_PORT      5003                  /* 平台TCP端口（分布式同学给的） */
#define ENABLE_AUTO_CALL 1                     /* 1=真实拨打，0=只模拟不真拨 */

static int sock_fd = -1;   /* 平台TCP连接套接字 */
static int at_fd   = -1;   /* AT指令串口（打电话用，/dev/ttyUSB2） */
static int net_ok  = 0;    /* 4G是否联网成功标志 */

/* ----------------------------------------------------------------
 * 等待4G拨号获取usb0 IP，最多等20秒
 * 用 ip 命令检测usb0是否有inet地址，比ifconfig更可靠
 * ---------------------------------------------------------------- */
static int wait_for_network(void)
{
    int i;
    for (i = 0; i < 40; i++) {
        FILE *fp = popen("ip -4 addr show usb0 2>/dev/null | grep 'inet '", "r");
        if (fp) {
            char buf[256] = {0};
            if (fgets(buf, sizeof(buf), fp)) {
                pclose(fp);
                printf("[ec20] 4G联网成功（usb0已获取IP）\n");
                return 0;
            }
            pclose(fp);
        }
        usleep(500000);  /* 每500ms检测一次，40次共20秒 */
    }
    printf("[ec20] 4G联网超时（20秒内未获取到IP）\n");
    return -1;
}

/* ----------------------------------------------------------------
 * 连接平台服务器
 * BIND_TO_4G=1时，socket绑定usb0网卡，强制上报流量走4G
 * ---------------------------------------------------------------- */
static int connect_server(void)
{
    /* 先关闭旧连接 */
    if (sock_fd >= 0) { close(sock_fd); sock_fd = -1; }

    /* 解析域名/IP（支持域名和点分十进制IP） */
    struct hostent *he = gethostbyname(SERVER_IP);
    if (!he) {
        printf("[ec20] 服务器地址解析失败: %s\n", SERVER_IP);
        return -1;
    }

    /* 创建TCP套接字 */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) return -1;

#if BIND_TO_4G
    /* 强制此socket走4G网卡usb0，不影响eth0的NFS等流量 */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "usb0", sizeof(ifr.ifr_name) - 1);
    if (setsockopt(sock_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        printf("[ec20] 绑定4G网卡失败: %s\n", strerror(errno));
    }
#endif

    /* 填充服务器地址结构 */
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(SERVER_PORT);
    memcpy(&serv.sin_addr, he->h_addr, he->h_length);

    /* 发起连接 */
    if (connect(sock_fd, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        printf("[ec20] 连接服务器失败: %s\n", strerror(errno));
        close(sock_fd);
        sock_fd = -1;
        return -1;
    }
    printf("[ec20] 已连接服务器 %s:%d\n", SERVER_IP, SERVER_PORT);
    return 0;
}

/* ----------------------------------------------------------------
 * 初始化4G模块：杀残留进程 → 启动拨号 → 等IP → 开AT口 → 连服务器
 * ---------------------------------------------------------------- */
int ec20_init(void)
{
    /* 1. 杀掉残留的quectel-CM，重新启动拨号 */
    system("killall quectel-CM 2>/dev/null");
    usleep(200000);
    system("/app/quectel-CM &");

    /* 2. 等待4G获取IP（最多20秒） */
    if (wait_for_network() == 0) {
        net_ok = 1;
    }

    /* 3. 打开AT串口（用于拨打报警电话） */
    /* 3. 打开AT串口（用于拨打报警电话） */
    at_fd = open("/dev/ttyUSB2", O_RDWR | O_NOCTTY);
    if (at_fd < 0) {
        printf("[ec20] open AT port(/dev/ttyUSB2) failed: %s\n", strerror(errno));
    } else {
        /* AT口初始化：关闭命令回显(ATE0)，开启错误报告(AT+CMEE=1) */
        write(at_fd, "ATE0\r", 5);
        usleep(300000);
        write(at_fd, "AT+CMEE=1\r", 10);
        usleep(300000);
        /* 清空输入缓冲区里的回显垃圾 */
        char dummy[64];
        int fl = fcntl(at_fd, F_GETFL, 0);
        fcntl(at_fd, F_SETFL, fl | O_NONBLOCK);
        while (read(at_fd, dummy, sizeof(dummy)) > 0);
        fcntl(at_fd, F_SETFL, fl);
    }


    /* 4. 4G联网成功才尝试连接平台 */
    if (net_ok) {
        connect_server();
    }

    /* 5. 同步状态到全局结构体 */
    STATE_LOCK();
    g_state.net_ok = (sock_fd >= 0) ? 1 : 0;
    g_state.gps_ok = 0;   /* 未接GPS天线，暂时为0 */
    STATE_UNLOCK();

    printf("[ec20] init done, 4G=%s, server=%s\n",
           net_ok ? "ok" : "fail",
           sock_fd >= 0 ? "connected" : "disconnected");
    return 0;
}

/* ----------------------------------------------------------------
 * 向平台发送数据，返回实际发送字节数，失败返回-1
 * ---------------------------------------------------------------- */
int ec20_send_data(char *data, int len)
{
    if (sock_fd < 0 || !data || len <= 0) return -1;
    return send(sock_fd, data, len, 0);
}

/* ----------------------------------------------------------------
 * 拨打报警电话（AT指令 ATD号码;）
 * ENABLE_AUTO_CALL=1时真实拨打，=0时只打印不真拨（测试安全）
 * ---------------------------------------------------------------- */
int ec20_call_phone(char *phone)
{
    char cmd[64];
    char resp[256];
    if (at_fd < 0 || !phone || phone[0] == 0) return -1;
#if ENABLE_AUTO_CALL
    /* 报警拨号优先：先断开4G数据连接，避免和语音冲突 */
    system("killall quectel-CM 2>/dev/null");
    sleep(2);   /* 等模块从QMI数据模式释放，稳定后再发AT指令 */

    /* 清空AT口输入缓冲区 */
    int flags = fcntl(at_fd, F_GETFL, 0);
    fcntl(at_fd, F_SETFL, flags | O_NONBLOCK);
    char dummy[64];
    while (read(at_fd, dummy, sizeof(dummy)) > 0);
    fcntl(at_fd, F_SETFL, flags);

    /* 发送拨号指令（只用\r，EC20标准AT指令结束符，不要\r\n） */
    snprintf(cmd, sizeof(cmd), "ATD%s;\r", phone);
    write(at_fd, cmd, strlen(cmd));
    printf("[ec20] 正在拨打报警电话: %s\n", phone);

    /* 循环读取响应，最多等8秒，直到读到OK/NO CARRIER/ERROR */
    resp[0] = '\0';
    int total = 0;
    fcntl(at_fd, F_SETFL, flags | O_NONBLOCK);
    int i;
    for (i = 0; i < 16; i++) {  /* 16 * 500ms = 8秒 */
        usleep(500000);
        int n = read(at_fd, resp + total, sizeof(resp) - total - 1);
        if (n > 0) {
            total += n;
            resp[total] = '\0';
            if (strstr(resp, "OK") || strstr(resp, "NO CARRIER") || strstr(resp, "ERROR")) {
                break;
            }
        }
    }
    fcntl(at_fd, F_SETFL, flags);
    printf("[ec20] 模块响应: %s\n", resp);

    /* 拨号结束后重新启动4G数据连接（恢复上报） */
    //system("/app/quectel-CM &");
#else
    printf("[ec20] [模拟拨号] 应拨打: %s\n", phone);
#endif
    return 0;
}



/* ----------------------------------------------------------------
 * 挂断电话（ATH指令）
 * ---------------------------------------------------------------- */
int ec20_hangup(void)
{
    if (at_fd < 0) return -1;
    write(at_fd, "ATH\r\n", 5);
    return 0;
}

/* ----------------------------------------------------------------
 * 通信线程：每10秒上报一次 + 报警打电话 + 监听平台下发
 * 所有业务逻辑在此线程，主线程只负责命令行交互
 * ---------------------------------------------------------------- */
void *comm_thread(void *arg)
{
    int counter = 0;
    static int called = 0;   /* 防止报警时重复拨打，报警解除后重置 */

    while (1) {
        /* ---- 每10秒上报一次传感器数据 ---- */
        if (++counter >= 3) {
            counter = 0;
            if (sock_fd >= 0) {
                char buf[256];
                STATE_LOCK();
                snprintf(buf, sizeof(buf),
                    "alcohol=%d,gas=%d,flame=%d,light=%d,alarm=%d,fan=%d,buzzer=%d\n",
                    g_state.alcohol, g_state.gas, g_state.flame,
                    g_state.light, g_state.alarm,
                    g_state.fan_on, g_state.buzzer_on);
                STATE_UNLOCK();
                if (ec20_send_data(buf, strlen(buf)) < 0) {
                    /* 上报失败说明连接断了，尝试重连 */
                    printf("[ec20] 上报失败，尝试重连服务器...\n");
                    close(sock_fd); sock_fd = -1;
                    connect_server();
                }
            }
        }

        /* ---- 报警时拨打报警电话（只打一次，报警解除后重置） ---- */
        STATE_LOCK();
        int alarm = g_state.alarm;
        char phone[20];
        strncpy(phone, g_state.alarm_phone, sizeof(phone) - 1);
        phone[sizeof(phone) - 1] = 0;
        STATE_UNLOCK();

        if (alarm && !called && phone[0]) {
            ec20_call_phone(phone);
            called = 1;
        }
        if (!alarm) {
            if (called) {
                /* 报警解除，挂断电话并恢复4G数据连接 */
                ec20_hangup();
                system("/app/quectel-CM &");
                printf("[ec20] 报警解除，已挂断电话并恢复4G\n");
            }
            called = 0;   /* 报警解除后重置，下次报警再打 */
        }

        /* ---- 监听平台下发指令（非阻塞recv，没数据立即返回） ---- */
        if (sock_fd >= 0) {
            char recv_buf[256];
            int n = recv(sock_fd, recv_buf, sizeof(recv_buf) - 1, MSG_DONTWAIT);
            if (n > 0) {
                recv_buf[n] = '\0';
                printf("[ec20] 收到平台指令: %s\n", recv_buf);

                /* 指令1: 设置报警电话  格式: phone:110 */
                if (strncmp(recv_buf, "phone:", 6) == 0) {
                    STATE_LOCK();
                    strncpy(g_state.alarm_phone, recv_buf + 6,
                            sizeof(g_state.alarm_phone) - 1);
                    STATE_UNLOCK();
                    printf("[ec20] 报警电话已更新为: %s\n", recv_buf + 6);
                }
                /* 指令2: 关闭风扇  格式: fan:off */
                else if (strcmp(recv_buf, "fan:off") == 0) {
                    relay_off();
                    STATE_LOCK(); g_state.fan_on = 0; STATE_UNLOCK();
                    printf("[ec20] 平台指令：关闭风扇\n");
                }
                /* 指令3: 打开风扇  格式: fan:on */
                else if (strcmp(recv_buf, "fan:on") == 0) {
                    relay_on();
                    STATE_LOCK(); g_state.fan_on = 1; STATE_UNLOCK();
                    printf("[ec20] 平台指令：打开风扇\n");
                }
            }
        }

        sleep(1);
    }
    return NULL;
}

/* ================================================================
 * 上传历史路径文件（uppath命令调用）
 * 文件格式：每行一个点 "纬度,经度"，#开头的注释行自动跳过
 * 上传协议：逐点发送 "path:<序号>,<纬度>,<经度>\n"
 *           最后发 "path:end,<总点数>\n" 表示结束
 * 返回成功上传的坐标点数，失败返回-1
 * ================================================================ */
int ec20_upload_path(const char *filename)
{
    /* 必须先连上平台才能上传 */
    if (sock_fd < 0) {
        printf("[ec20] 未连接服务器，无法上传路径（先确认4G已连平台）\n");
        return -1;
    }
    /* 没指定文件名就用默认的 */
    if (!filename || !filename[0]) {
        filename = "history_path.txt";
    }

    /* 打开路径文件 */
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[ec20] 打开路径文件失败: %s\n", filename);
        return -1;
    }

    char line[128];
    int count = 0;
    printf("[ec20] 开始上传历史路径文件: %s\n", filename);

    /* 逐行读取解析坐标 */
    while (fgets(line, sizeof(line), fp)) {
        double lat = 0, lng = 0;
        /* 解析每行：纬度,经度（#注释行解析失败自动跳过） */
        if (sscanf(line, "%lf,%lf", &lat, &lng) == 2) {
            char buf[128];
            snprintf(buf, sizeof(buf), "path:%d,%.6f,%.6f\n", count + 1, lat, lng);
            if (send(sock_fd, buf, strlen(buf), 0) < 0) {
                printf("[ec20] 路径上传中断（连接断开）\n");
                fclose(fp);
                return -1;
            }
            count++;
            usleep(50000);   /* 每点间隔50ms，避免发太快丢包 */
        }
    }
    fclose(fp);

    /* 发送结束标记，告诉服务器传完了 */
    char end[64];
    snprintf(end, sizeof(end), "path:end,%d\n", count);
    send(sock_fd, end, strlen(end), 0);
    printf("[ec20] 历史路径上传完成，共 %d 个坐标点\n", count);
    return count;
}

/* ----------------------------------------------------------------
 * 清理：关闭socket、AT口、杀掉quectel-CM
 * ---------------------------------------------------------------- */
void ec20_close(void)
{
    if (sock_fd >= 0) close(sock_fd);
    if (at_fd   >= 0) close(at_fd);
    system("killall quectel-CM 2>/dev/null");
}
