#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define SET_CHANNEL _IO('A', 0)

int main(int argc, const char *argv[])
{
    int fd, data, ch, i;
    char input[16];

    printf("===== ADC通道测试程序 =====\n");
    printf("输入通道号 0-7 直接测试\n");
    printf("或按快捷键: A酒精 L光敏 F火焰 S气体 P电位器\n");
    printf("输入 q 退出\n\n");

    fd = open("/dev/adc", O_RDWR);
    if (fd < 0) { perror("open /dev/adc failed"); exit(1); }
    printf("/dev/adc 打开成功\n\n");

    while (1) {
        printf("> ");
        fflush(stdout);
        if (scanf("%15s", input) != 1) break;

        if (input[0] == 'q' || input[0] == 'Q') break;

        switch (input[0]) {
        case 'A': case 'a': ch = 7; printf("[酒精] 通道7\n"); break;
        case 'L': case 'l': ch = 6; printf("[光敏] 通道6\n"); break;
        case 'F': case 'f': ch = 3; printf("[火焰] 通道3\n"); break;
        case 'S': case 's': ch = 5; printf("[气体] 通道5\n"); break;
        case 'P': case 'p': ch = 0; printf("[电位器] 通道0\n"); break;
        default:            ch = atoi(input); printf("[通道%d]\n", ch); break;
        }

        if (ch < 0 || ch > 7) { printf("通道号必须 0-7\n"); continue; }

        if (ioctl(fd, SET_CHANNEL, ch) < 0) {
            perror("ioctl SET_CHANNEL failed");
            continue;
        }

        printf("连续读取5次:\n");
        for (i = 0; i < 5; i++) {
            if (read(fd, &data, sizeof(int)) == sizeof(int))
                printf("  raw=%-4d  voltage=%.2fV\n", data, 1.8 * data / 4096);
            else
                printf("  read failed\n");
            usleep(300000);
        }
        printf("\n");
    }

    close(fd);
    printf("退出\n");
    return 0;
}
