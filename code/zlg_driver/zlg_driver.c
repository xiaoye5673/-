#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h> 
#include <linux/fs.h> /*file_operations*/
#include <linux/slab.h> /* kzalloc */
#include <asm-generic/uaccess.h> /* copy_from_user() */
#include <linux/delay.h> /* sleep()*/

#include <linux/i2c.h>


/*
 *Description of This Driver
 * */
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Driver for 6818M4's zlg7290");
MODULE_AUTHOR("Farsight yanfa <support@farsight.com.cn>");
MODULE_VERSION("V1.0");

//关系到设备的名字 /dev/DEVICE_NAME
#define DEVICE_NAME "zlg7290"

/*********************调试使用*************************************/
//#define DEBUG
//调试打印信息
#ifdef DEBUG
#define LOGD(fmt, args...) \
	{printk(KERN_INFO "<<-Device:%s->> line %d at %s ", DEVICE_NAME, __LINE__, __func__); \
		printk(KERN_INFO fmt, ##args);}
#else
#define LOGD(fmt, args...) (void)(0);
#endif
//正常提示信息
#define LOGI(fmt, args...) {printk(KERN_INFO fmt, ##args);}
//错误提示信息
#define LOGE(fmt, args...) {printk(KERN_ERR fmt, ##args);}
/***************************************************************/

/************************IOCTL CMD******************************/
/*
 * 	IOCTL CMD
 * 	_IOWR(type, nr, size)
 * */
#define IOCTL_MAGIC 	'Z'
#define SET_VAL _IO(IOCTL_MAGIC, 0)
#define GET_KEY	_IO(IOCTL_MAGIC, 1)
/**************************************************************/

struct zlg7290 {
	struct i2c_client *client;
	struct miscdevice misc;
	wait_queue_head_t readq;
	struct delayed_work work;
	int current_key;
	
};
//struct i2c_client *client;
static int zlg7290_hw_write(struct zlg7290 *dev, int len,
		size_t *retlen, char *buf)
{
	struct i2c_client *client = dev->client;
	int ret;

	struct i2c_msg msg[] = {
		/*the buf contains register address*/
		{ client->addr, 0, len, buf},   
	};

	ret =i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		//LOGE("ret=%d, addr=%x\n", ret, client->addr);
		dev_err(&client->dev, "i2c write error");
		return -EIO;
	}

	*retlen = len;


	return 0;
}

static int zlg7290_hw_read(struct zlg7290 *dev , int len, 
					size_t *retlen, char *buf)
{
	struct i2c_client *client = dev->client;
	int ret;

	struct i2c_msg msg[] = { 
		/*the buf contains register address*/
		{ client->addr, 0, len, buf}, 
		/*the buf contains register value*/
		{ client->addr, I2C_M_RD, len, buf },
	};

	ret =i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		//LOGE("ret=%d, addr=%x\n", ret, client->addr);
		dev_err(&client->dev, "i2c read error");
		return -EIO;
	}

	*retlen = len;
	return 0;
}

static long dev_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	//获取设备结构体
	struct zlg7290 *dev = container_of(filp->private_data, 
							struct zlg7290, misc);
	unsigned char buf[8] = {0};
	ssize_t len = 0;
	unsigned char val[2] = {0};
	unsigned char reg[8] = 
			{0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
	int i = 0;

	//判断魔数是否一致
	if (_IOC_TYPE(cmd) != IOCTL_MAGIC)
		return -ENOTTY;
	//判断基数是否超出范围
	if (_IOC_NR(cmd) > 2)
		return -ENOTTY;

	switch (cmd) {
	case SET_VAL:
		if (copy_from_user(buf, (void *)arg, sizeof(buf)))
			return -EFAULT;
		for(i = 0; i < 8; i++) {
			val[0] = reg[i];
			switch(buf[i]) {
			case '0': val[1] = 0xfc; break;
			case '1': val[1] = 0x60; break;
			case '2': val[1] = 0xda; break;
			case '3': val[1] = 0xf2; break;
			case '4': val[1] = 0x66; break;
			case '5': val[1] = 0xb6; break;
			case '6': val[1] = 0xbe; break;
			case '7': val[1] = 0xe0; break;
			case '8': val[1] = 0xfe; break;
			case '9': val[1] = 0xf6; break;
			case 'a':
			case 'A': val[1] = 0xee; break;
			case 'b':
			case 'B': val[1] = 0x3e; break;
			case 'c':
			case 'C': val[1] = 0x9c; break;
			case 'd':
			case 'D': val[1] = 0x7a; break;
			case 'e':
			case 'E': val[1] = 0x9e; break;
			case 'f':
			case 'F': val[1] = 0x8e; break;
			case ' ': val[1] = 0x00; break;
			case '-': val[1] = 0x02; break;
			default:
			 val[1] = 0x00; break;
			}
			msleep(10);
			zlg7290_hw_write(dev, 2, &len, val);
		}
			break;
	case GET_KEY:
		LOGD("GET_KEY\n");
		wait_event_interruptible(dev->readq,
							dev->current_key != 0);
		if (copy_to_user((void *)arg, &dev->current_key, 4))
			return -EFAULT;
		dev->current_key = 0;
		break;

		default:
		LOGE("<%s> Please check your command\n", DEVICE_NAME);
		break;
	}

	return 0;
}

static void dev_work(struct work_struct *work)
{
	//获取设备结构体
	struct zlg7290 *dev = container_of(work, struct zlg7290,
							work.work);
	unsigned char val = 0, status = 0;
	size_t len;

	zlg7290_hw_read(dev, 1, &len, &status);
	LOGD("zlg7290_hw_read status=%d\n", status);
	if(status & 0x1) {
		val = 1;
		LOGD("read key");
		zlg7290_hw_read(dev, 1, &len, &val);

		if (val == 0) {
			val = 3;
			zlg7290_hw_read(dev, 1, &len, &val);
			if (val == 0 || val == 0xFF)
				goto out;
		}

		if (val > 56) {
			switch (val) {
				case 0xFE: val = 57; break;
				case 0xFD: val = 58; break;
				case 0xFB: val = 59; break;
				case 0xF7: val = 60; break;
				case 0xEF: val = 61; break;
				case 0xDF: val = 62; break;
				case 0xBF: val = 63; break;
				case 0x7F: val = 64; break;
			}
		}

		LOGD("key_val = %02x\n", val);

		dev->current_key  = val;
		LOGD("dev->current_key = %02x\n", val);
		wake_up_interruptible(&dev->readq);
	} 

out:
	LOGD("jiffies = %ld\n", jiffies);
	schedule_delayed_work(&dev->work, HZ / 5);
}

static int dev_open(struct inode *inode, struct file *filp)
{
	//获取设备结构体
	struct zlg7290 *dev = container_of(filp->private_data, 
							struct zlg7290, misc);
	//每200ms轮询一次
	schedule_delayed_work(&dev->work, HZ / 5);	
	return 0;
}

static int dev_release(struct inode *inode, struct file *filp)
{
	//获取设备结构体
	struct zlg7290 *dev = container_of(filp->private_data, 
							struct zlg7290, misc);

	return 0;
}

/*
 *	设备操作集，根据需求填充
 * */
static struct file_operations dev_fops = {
	.owner			= THIS_MODULE,
	.open			= dev_open,
	.release		= dev_release,
	.unlocked_ioctl	= dev_ioctl,
};

static int dev_probe(struct i2c_client *client, 
		const struct i2c_device_id *id)
{
	int err = -1;
	struct zlg7290 *dev;
	LOGI("<%s> probe\n", DEVICE_NAME);
	//检查IIC设备的功能
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	dev = kzalloc(sizeof(struct zlg7290), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	/*
	 *	miscdevice init
	 * */
	dev->misc.minor = MISC_DYNAMIC_MINOR;
	dev->misc.name = DEVICE_NAME;
	dev->misc.fops = &dev_fops;

	dev->client = client;
	i2c_set_clientdata(client, dev);

	//初始化等待队列
	init_waitqueue_head(&dev->readq);
	//初始化工作队列
	INIT_DELAYED_WORK(&dev->work, dev_work);

	err = misc_register(&dev->misc);
	if (err)
		goto err;

	LOGI("<%s-minor(%d)> miscdevice register successfully\n",
			DEVICE_NAME, dev->misc.minor);
	return 0;	

err:
	kfree(dev);
	return err;
}

static int dev_remove(struct i2c_client *client)
{
	struct zlg7290 *dev = i2c_get_clientdata(client);
	
	//取消工作队列
	cancel_delayed_work(&dev->work);
	misc_deregister(&dev->misc);
	i2c_set_clientdata(client, NULL);
	kfree(dev);

	return 0;
}
#if 0
/*
 *	i2c device info
 * */
#define ZLG7290_I2C_BUS		(2)
static struct i2c_board_info zlg7290_i2c_bdi = {
	I2C_BOARD_INFO(DEVICE_NAME, 0x70>>1),
};
#endif
static const struct i2c_device_id dev_id[] = {
	{DEVICE_NAME, 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, dev_id);
/*
 *	i2c driver info	
 * */
static struct i2c_driver zlg7290_driver = {
	.driver		= {
		.name	= DEVICE_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= dev_probe,
	.remove		= dev_remove,
	.id_table	= dev_id,
};


/*
 * 	Driver Init
 * */
static int __init i2c_module_init(void)
{
	//自动注册i2c设备结构体
#if 0
	struct i2c_adapter *adap;

	//获取一个适配器
	adap = i2c_get_adapter(ZLG7290_I2C_BUS);	
	if (!adap)
		return -ENODEV;
	/*
	 * 注册i2c device：zlg7290
	 * */
	client = i2c_new_device(adap, &zlg7290_i2c_bdi);
	LOGI("<%s> register successfully\n", DEVICE_NAME);
#endif
	return i2c_add_driver(&zlg7290_driver);

}

module_init(i2c_module_init);

/*
 *Driver Exit
 * */
static void __exit i2c_module_exit(void)
{
#if 0
	/*
	 * 注销i2c device：zlg7290
	 * */
	i2c_unregister_device(client);
#endif
	i2c_del_driver(&zlg7290_driver);
	LOGI("<%s> exit successfully\n", DEVICE_NAME);
}

module_exit(i2c_module_exit);

