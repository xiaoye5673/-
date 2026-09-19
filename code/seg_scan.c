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
    int bit;
    fd = open("/dev/i2c-2", O_RDWR);
    ioctl(fd, I2C_SLAVE, 0x30);

    /* 清屏 */
    for (bit = 0; bit < 8; bit++) wr(0x10 + bit, 0x00);

    printf("\n=== 段码位扫描（只看第1位数码管）===\n");
    printf("数码管段位置参考:\n");
    printf("    ___a___\n");
    printf("   |       |\n");
    printf("  f|       |b\n");
    printf("   |___g___|\n");
    printf("   |       |\n");
    printf("  e|       |c\n");
    printf("   |___d___|  dp(小数点)\n\n");

    for (bit = 0; bit < 8; bit++) {
        wr(0x10, 1 << bit);
        printf("写 0x%02x (只有bit%d=1)，第1位哪个段亮了？输入 a/b/c/d/e/f/g/dp/无 然后回车: ", 1 << bit, bit);
        getchar();
    }

    wr(0x10, 0x00);  /* 清屏 */
    close(fd);
    return 0;
}
