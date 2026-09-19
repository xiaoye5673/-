#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define PWM_OFF  _IO('K', 1)   /* 和fs6818_pwm驱动一致 */

int main(void)
{
    int fd = open("/dev/pwm", O_RDWR);
    if (fd < 0) {
        perror("open /dev/pwm");
        return 1;
    }
    ioctl(fd, PWM_OFF, 0);   /* 停止PWM，引脚切回高电平（关闭蜂鸣器） */
    close(fd);
    return 0;
}
