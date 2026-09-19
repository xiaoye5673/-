#include "zlg72128.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <string.h>

#define I2C_BUS    "/dev/i2c-2"
#define ZLG_ADDR   0x30
#define DISP_BUF   0x10

static int i2c_fd = -1;
static int current_key = 0;

/* ZLG72128B 共阴段码表（位序：bit0=a bit1=b bit2=c bit3=d bit4=e bit5=f bit6=g bit7=dp）*/
static const unsigned char seg_code[128] = {
    ['0']=0x3F, ['1']=0x06, ['2']=0x5B, ['3']=0x4F,
    ['4']=0x66, ['5']=0x6D, ['6']=0x7D, ['7']=0x07,
    ['8']=0x7F, ['9']=0x6F, ['A']=0x77, ['B']=0x7C,
    ['C']=0x39, ['D']=0x5E, ['E']=0x79, ['F']=0x71,
    [' ']=0x00, ['-']=0x40,
};

static int i2c_write_reg(unsigned char reg, unsigned char val)
{
    unsigned char buf[2] = {reg, val};
    if (i2c_fd < 0) return -1;
    return (write(i2c_fd, buf, 2) == 2) ? 0 : -1;
}

static int i2c_read_reg(unsigned char reg, unsigned char *val)
{
    unsigned char wbuf[1] = {reg};
    if (i2c_fd < 0) return -1;
    if (write(i2c_fd, wbuf, 1) != 1) return -1;
    if (read(i2c_fd, val, 1) != 1) return -1;
    return 0;
}

/* 按键线程：读0x00状态寄存器判断有无按键，有则读0x01取键值 */
static void *key_thread(void *arg)
{
    unsigned char status, key;
    while (i2c_fd >= 0) {
        if (i2c_read_reg(0x00, &status) == 0 && (status & 0x01)) {
            if (i2c_read_reg(0x01, &key) == 0 && key != 0 && key != 0xFF) {
                current_key = key;
                usleep(200000);  /* 消抖 */
            }
        }
        usleep(50000);  /* 20次/秒轮询 */
    }
    return NULL;
}

int zlg72128_init(void)
{
    pthread_t tid;
    i2c_fd = open(I2C_BUS, O_RDWR);
    if (i2c_fd < 0) {
        perror("[zlg] open /dev/i2c-2 failed");
        return -1;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, ZLG_ADDR) < 0) {
        perror("[zlg] set slave addr failed");
        close(i2c_fd);
        i2c_fd = -1;
        return -1;
    }
    zlg72128_clear();
    pthread_create(&tid, NULL, key_thread, NULL);
    printf("[zlg] init done (i2c-2, addr=0x30)\n");
    return 0;
}

int zlg72128_display_string(const char *str)
{
    int i, len;
    unsigned char seg;
    if (i2c_fd < 0) return -1;
    len = strlen(str);
    if (len > 8) len = 8;
    for (i = 0; i < 8; i++) {
        if (i < len && (unsigned char)str[i] < 128)
            seg = seg_code[(unsigned char)str[i]];
        else
            seg = 0x00;
        i2c_write_reg(DISP_BUF + i, seg);
        usleep(1000);
    }
    return 0;
}

int zlg72128_clear(void)
{
    int i;
    if (i2c_fd < 0) return -1;
    for (i = 0; i < 8; i++)
        i2c_write_reg(DISP_BUF + i, 0x00);
    return 0;
}

int zlg72128_read_key(void)
{
    int key = current_key;
    current_key = 0;
    return key;
}

void zlg72128_close(void)
{
    if (i2c_fd >= 0) {
        zlg72128_clear();
        close(i2c_fd);
        i2c_fd = -1;
    }
}
