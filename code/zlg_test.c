#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int fd;

void wr(unsigned char reg, unsigned char val)
{
    unsigned char buf[2] = {reg, val};
    write(fd, buf, 2);
    usleep(2000);
}

int main()
{
    int i, reg;
    unsigned char segs[10] = {0xfc,0x60,0xda,0xf2,0x66,0xb6,0xbe,0xe0,0xfe,0xf6};
    unsigned char w, r;

    fd = open("/dev/i2c-2", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    ioctl(fd, I2C_SLAVE, 0x30);

    /* ===== 测试1: 0x10-0x17 全亮 ===== */
    printf("\n=== 测试1: 向 0x10~0x17 写 0xFF（应该8位全亮）===\n");
    for (i = 0; i < 8; i++) wr(0x10 + i, 0xFF);
    printf("  观察数码管，是不是8位都亮了？按回车继续...\n");
    getchar();

    /* ===== 测试2: 全灭 ===== */
    printf("\n=== 测试2: 向 0x10~0x17 写 0x00（全灭）===\n");
    for (i = 0; i < 8; i++) wr(0x10 + i, 0x00);
    sleep(1);

    /* ===== 测试3: 第1位逐个显示0-9，确认段码表 ===== */
    printf("\n=== 测试3: 第1位(0x10)逐个显示0-9 ===\n");
    for (i = 0; i < 10; i++) {
        wr(0x10, segs[i]);
        printf("  写段码 0x%02x，数码管第1位显示的是 %d 吗？(y/n) ", segs[i], i);
        getchar();
    }

    /* ===== 测试4: 扫描按键寄存器 ===== */
    printf("\n=== 测试4: 扫描按键寄存器 ===\n");
    printf("  现在用手指按住4x4键盘上任意一个键不放，然后按回车...\n");
    getchar();
    printf("  按住了吗？开始扫描 0x00~0x30...\n");
    for (reg = 0x00; reg <= 0x30; reg++) {
        w = reg;
        write(fd, &w, 1);
        if (read(fd, &r, 1) == 1 && r != 0 && r != 0xFF)
            printf("  [有值] reg 0x%02x = 0x%02x (%d)\n", reg, r, r);
    }
    printf("  扫描完成，上面有值的寄存器就是按键寄存器\n");

    /* 清屏 */
    for (i = 0; i < 8; i++) wr(0x10 + i, 0x00);
    close(fd);
    return 0;
}
