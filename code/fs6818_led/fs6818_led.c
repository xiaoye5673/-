#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "fs6818_led.h"

MODULE_LICENSE("Dual BSD/GPL");

#define LED_MA 500
#define LED_MI 0
#define LED_NUM 1


//red
//#define GPA28OUT	 0xC001A004   //0=Low Level 1=High Level
//#define GPA28OUTENB  0xC001A000   //0=Input Mode 1=Output Mode
#define GPA28OUT	 0xC001A000   //0=Low Level 1=High Level
#define GPA28OUTENB  0xC001A004   //0=Input Mode 1=Output Mode
#define GPA28ALTFN   0xC001A020   //set two bits to 2'b00 

//blue
#define GPE13OUT    0xC001e000
#define GPE13OUTENB	0xC001e004
#define GPE13ALTFN	0xC001e020

//green
#define GPB12OUT    0xC001b000
#define GPB12OUTENB	0xC001b004
#define GPB12ALTFN	0xC001b020




static unsigned int *gpa28outenb;
static unsigned int *gpa28out;
static unsigned int *gpa28altfn;

static unsigned int *gpe13outenb;
static unsigned int *gpe13out;
static unsigned int *gpe13altfn;

static unsigned int *gpb12outenb;
static unsigned int *gpb12out;
static unsigned int *gpb12altfn;


struct cdev cdev;

void led_on(int nr)
{
	switch(nr) {
	case RED_LED: 
		writel(readl(gpa28out) | (0x1 << 28), gpa28out);
		break;
	case GREEN_LED: 
		writel(readl(gpb12out) | (0x1 << 12), gpb12out);
		break;
	case BLUE_LED: 
		writel(readl(gpe13out) | (0x1 << 13), gpe13out);
		break;
	}
}

void led_off(int nr)
{
	switch(nr) {
	case RED_LED: 
		writel(readl(gpa28out) & ~(1 << 28), gpa28out);
		break;
	case GREEN_LED: 
		writel(readl(gpb12out) & ~(1 << 12), gpb12out);
		break;
	case BLUE_LED: 
		writel(readl(gpe13out) & ~(1 << 13), gpe13out);
		break;
	}
}



static int led_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int led_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long led_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int nr;

	if(copy_from_user((void *)&nr, (void *)arg, sizeof(nr)))
		return -EFAULT;

//	if (nr < 1 || nr > 4)
//		return -EINVAL;

	switch (cmd) {
	case LED_ON:
		led_on(nr);
		//printk(KERN_INFO "LED ON\n");
		break;
	case LED_OFF:
		led_off(nr);
		//printk(KERN_INFO "LED OFF\n");
		break;
	default:
		printk(KERN_INFO "Invalid argument");
		return -EINVAL;
	}

	return 0;
}

int led_ioremap(void)
{
	int ret;

	gpa28outenb = ioremap(GPA28OUTENB, 4);
	if (gpa28outenb == NULL) {
		printk(KERN_INFO "ioremap gpa28outenb\n");
		ret = -ENOMEM;
		return ret;
	}

	gpa28out = ioremap(GPA28OUT, 4);
	if (gpa28out == NULL) {
		printk(KERN_INFO "ioremap gpa28out\n");
		ret = -ENOMEM;
		return ret;
	}

	gpa28altfn = ioremap(GPA28ALTFN, 4);
	if (gpa28altfn == NULL) {
		printk(KERN_INFO "ioremap gpa28altfn\n");
		ret = -ENOMEM;
		return ret;
	}

	gpe13outenb = ioremap(GPE13OUTENB, 4);
	if (gpe13outenb == NULL) {
		printk(KERN_INFO "ioremap gpe13outenb\n");
		ret = -ENOMEM;
		return ret;
	}

	gpe13out = ioremap(GPE13OUT, 4);
	if (gpe13out == NULL) {
		printk(KERN_INFO "ioremap gpe13out\n");
		ret = -ENOMEM;
		return ret;
	}

	gpe13altfn = ioremap(GPE13ALTFN, 4);
	if (gpe13altfn == NULL) {
		printk(KERN_INFO "ioremap gpe13altfn\n");
		ret = -ENOMEM;
		return ret;
	}

	gpb12outenb = ioremap(GPB12OUTENB, 4);
	if (gpb12outenb == NULL) {
		printk(KERN_INFO "ioremap gpb12outenb\n");
		ret = -ENOMEM;
		return ret;
	}

	gpb12out = ioremap(GPB12OUT, 4);
	if (gpb12out == NULL) {
		printk(KERN_INFO "ioremap gpb12out\n");
		ret = -ENOMEM;
		return ret;
	}

	gpb12altfn = ioremap(GPB12ALTFN, 4);
	if (gpb12altfn == NULL) {
		printk(KERN_INFO "ioremap gpb12altfn\n");
		ret = -ENOMEM;
		return ret;
	}
	return 0;
}

void led_iounmap(void)
{
	iounmap(gpa28outenb);
	iounmap(gpa28out);
	iounmap(gpa28altfn);
	iounmap(gpe13outenb);
	iounmap(gpe13out);
	iounmap(gpe13altfn);
	iounmap(gpb12outenb);
	iounmap(gpb12out);
	iounmap(gpb12altfn);
}

void led_io_init(void)
{
	//ALT function 0
	writel(readl(gpa28altfn) & ~(0x1 << 25) & ~(0x1 << 24), gpa28altfn);
	//led enable output
	writel(readl(gpa28outenb) | (0x1 << 28), gpa28outenb);
	//turn off led
	writel(readl(gpa28out) & ~(1 << 28), gpa28out);

	//ALT function 0
	writel(readl(gpe13altfn) & ~(0x1 << 27) & ~(0x1 << 26), gpe13altfn);
	// led enable output
	writel(readl(gpe13outenb) | (0x1 << 13), gpe13outenb);
	//turn off led
	writel(readl(gpe13out) & ~(1 << 13), gpe13out);

	//ALT function 2
	writel((readl(gpb12altfn) | (0x1 << 25)) & ~(0x1 << 24), gpb12altfn);
	// led enable output
	writel(readl(gpb12outenb) | (0x1 << 12), gpb12outenb);
	//turn off led
	writel(readl(gpb12out) & ~(1 << 12), gpb12out);
}

struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.release = led_release,
	.unlocked_ioctl = led_unlocked_ioctl,
};

static int led_init(void)
{
	dev_t devno = MKDEV(LED_MA, LED_MI); 
	int ret;

	ret = register_chrdev_region(devno, LED_NUM, "newled");
	if (ret < 0) {
		printk(KERN_INFO "register_chrdev_region\n");
		return ret;
	}

	cdev_init(&cdev, &led_fops);
	cdev.owner = THIS_MODULE;
	ret = cdev_add(&cdev, devno, LED_NUM);
	if (ret < 0) {
		printk(KERN_INFO "cdev_add\n");
		goto err1;
	}

	ret = led_ioremap();
	if (ret < 0)
		goto err2;


	led_io_init();

	printk(KERN_INFO "Led init succ\n");

	return 0;
err2:
	cdev_del(&cdev);
err1:
	unregister_chrdev_region(devno, LED_NUM);
	return ret;
}

static void led_exit(void)
{
	dev_t devno = MKDEV(LED_MA, LED_MI);

	led_iounmap();
	cdev_del(&cdev);
	unregister_chrdev_region(devno, LED_NUM);
	printk(KERN_INFO "Led exit\n");
}

module_init(led_init);
module_exit(led_exit);
