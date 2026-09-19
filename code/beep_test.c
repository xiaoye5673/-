#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define BEEP_ON   _IO('B', 0)
#define BEEP_OFF  _IO('B', 1)
#define SET_FREQ  _IOW('B', 2, int)
int main() {
    int fd = open("/dev/beep", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    int f = 1;
    ioctl(fd, SET_FREQ, &f);
    ioctl(fd, BEEP_ON, 0);
    printf("beep on 5 seconds...\n");
    sleep(5);
    ioctl(fd, BEEP_OFF, 0);
    close(fd);
    printf("beep off\n");
    return 0;
}
