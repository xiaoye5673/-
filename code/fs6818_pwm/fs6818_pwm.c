#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "fs6818_pwm.h"

MODULE_LICENSE("GPL");

/* PWM Base Address */
#define TIMER_BASE 		0xC0018000
/* PWM OFFSET */
#define TCFG0	0x00
#define TCFG1	0x04
#define TCON	0x08
#define TCNTB2	0x24
#define TCMPB2	0x28

static struct class *pwm_class;
static struct device *pwm_device;
static int pwm_major = 501;
static int pwm_minor = 0;
static int number_of_device = 1;

struct fs6818_pwm
{
	void __iomem *timer_base;
	struct cdev cdev;
};

static struct fs6818_pwm *pwm;
	
static int fs6818_pwm_open(struct inode *inode, struct file *file)
{
	/* set TCFG0 prescaler1 [15:8] */
	writel((readl(pwm->timer_base + TCFG0) & ~(0xff << 8)) | (1<<8), pwm->timer_base + TCFG0);
	/* set TCFG1 DIVIDER MUX2 select mux 1/4 [11:8] */
	writel((readl(pwm->timer_base + TCFG1) & ~(0x7 << 8)) | (0x1 << 8), pwm->timer_base + TCFG1);
	writel(300, pwm->timer_base + TCNTB2);
	writel(150, pwm->timer_base + TCMPB2);
	/* set TCON update TCNTB2,TCMPB2 [15:12]*/
	writel((readl(pwm->timer_base + TCON) & ~(0xf << 12)) | (0xA << 12), pwm->timer_base + TCON);
    nxp_soc_gpio_set_io_func(PAD_GPIO_C + 14, NX_GPIO_PADFUNC_2);
	return 0;
}

static int fs6818_pwm_rlease(struct inode *inode, struct file *file)
{
	/*stop timer2 */
	writel(readl(pwm->timer_base + TCON) & ~(0xf << 12), pwm->timer_base + TCON);

    nxp_soc_gpio_set_io_func(PAD_GPIO_C, NX_GPIO_PADFUNC_1);
	return 0;
}

static long fs6818_pwm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int data;

	if (_IOC_TYPE(cmd) != 'K')
		return -ENOTTY;

	if (_IOC_NR(cmd) > 3)
		return -ENOTTY;

	if (_IOC_DIR(cmd) == _IOC_WRITE)
		if (copy_from_user(&data, (void *)arg, sizeof(data)))
			return -EFAULT;

	switch(cmd)
	{
	case PWM_ON:
		/* 切回PWM功能（ALT2）*/
		nxp_soc_gpio_set_io_func(PAD_GPIO_C + 14, NX_GPIO_PADFUNC_2);
		/* 启动Timer2 & 自动重载 */
		writel((readl(pwm->timer_base + TCON) & ~(0xf << 12)) | (0x9 << 12), pwm->timer_base + TCON);
		break;
	case PWM_OFF:
		/* 停止Timer2 */
		writel(readl(pwm->timer_base + TCON) & ~(0xf << 12), pwm->timer_base + TCON);
		/* 切回GPIO功能，输出高电平（有源低电平蜂鸣器→高电平停止）*/
		nxp_soc_gpio_set_io_func(PAD_GPIO_C + 14, NX_GPIO_PADFUNC_0);
		nxp_soc_gpio_set_io_dir(PAD_GPIO_C + 14, PAD_MODE_OUT);
		nxp_soc_gpio_set_out_value(PAD_GPIO_C + 14, PAD_LEVEL_HIGH);
		break;

	case SET_PRE:
        /* NULL operation */
		break;
	case SET_CNT:
		/* set Count buffer Register TCNTB2 = 2 * TCMPB2 */
		writel(data << 5, pwm->timer_base + TCNTB2);
		writel(data << 3, pwm->timer_base + TCMPB2);
		break;
	}

	return 0;
}
	
static struct file_operations fs6818_pwm_fops = {
	.owner = THIS_MODULE,
	.open = fs6818_pwm_open,
	.release = fs6818_pwm_rlease,
	.unlocked_ioctl = fs6818_pwm_ioctl,
};

static int __init fs6818_pwm_init(void)
{
	int ret;
	dev_t devno = MKDEV(pwm_major, pwm_minor);
	if (pwm_major)
		ret = register_chrdev_region(devno, number_of_device, "pwm");
	else 
		ret = alloc_chrdev_region(&devno, 0, number_of_device, "pwm");

	if (ret < 0) {
		printk(KERN_INFO "faipwm : register_chrdev_region\n");
		return ret;
	}
	printk(KERN_INFO "pwm driver %d, %d\n", MAJOR(devno), MINOR(devno));
	pwm = kmalloc(sizeof(*pwm), GFP_KERNEL);
	if (pwm == NULL) {
		ret = -ENOMEM;
		printk(KERN_INFO "faipwm: kmalloc\n");
		goto err1;
	}
	memset(pwm, 0, sizeof(*pwm));

	cdev_init(&pwm->cdev, &fs6818_pwm_fops);
	pwm->cdev.owner = THIS_MODULE;
	ret = cdev_add(&pwm->cdev, devno, number_of_device);
	if (ret < 0) {
		printk(KERN_INFO "%s, %s,  cdev_add\n", __FILE__, __func__);
		goto err2;
	}
	/*  create sys/class/pwm */	
	pwm_class = class_create(THIS_MODULE, "pwm");
	if (!pwm_class) {
		printk(KERN_INFO "%s, %s,class_create failed\n", __FILE__, __func__);
		goto err3;
	}

	/* create device file /dev/pwm */
	pwm_device  = device_create(pwm_class, NULL, devno, NULL, "pwm");
	if (!pwm_device) {
		printk(KERN_INFO "%s, %s, device_create failed\n", __FILE__, __func__);
		goto err4;
	}
	
	pwm->timer_base = ioremap(TIMER_BASE, 0x32);
	if (pwm->timer_base == NULL) {
		ret = -ENOMEM;
		printk(KERN_INFO "failed: ioremap timer_base\n");
		goto err5;
	}


	return 0;
err5:
	device_destroy(pwm_class, devno);
err4:
	class_destroy(pwm_class);
err3:
	cdev_del(&pwm->cdev);
err2:
	kfree(pwm);
err1:
	unregister_chrdev_region(devno, number_of_device);
	return ret;
}

static void __exit fs6818_pwm_exit(void)
{
	dev_t devno = MKDEV(pwm_major, pwm_minor);
	iounmap(pwm->timer_base);
	cdev_del(&pwm->cdev);
	device_destroy(pwm_class, devno);
	class_destroy(pwm_class);
	kfree(pwm);
	unregister_chrdev_region(devno, number_of_device);
}

module_init(fs6818_pwm_init);
module_exit(fs6818_pwm_exit);
