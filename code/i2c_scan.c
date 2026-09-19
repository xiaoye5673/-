#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int main()
{
    int fd, addr, found = 0;
    unsigned char reg;

    fd = open("/dev/i2c-2", O_RDWR);
    if (fd < 0) { perror("open /dev/i2c-2"); return 1; }

    printf("扫描 I2C 总线 2 (0x03~0x77)...\n");
    for (addr = 0x03; addr <= 0x77; addr++) {
        ioctl(fd, I2C_SLAVE, addr);
        reg = 0x10;
        if (write(fd, &reg, 1) == 1) {
            printf("  [√] 发现设备: 7位地址=0x%02x  写地址=0x%02x\n", addr, addr << 1);
            found++;
        }
    }
    if (!found) printf("  未发现任何I2C设备！检查SW8拨码或接线\n");
    close(fd);
    return 0;
}
