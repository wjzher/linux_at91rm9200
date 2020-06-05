/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_usri2c.c
 * 摘    要：对I2C接口编程, 目前只支持铁电和时钟芯片DS1337
 *			 对铁电的操作用read和write实现, 对DS1337的操作用ioctl实现
 *			 1.1 使用自旋锁保护临界区
 *			 1.2 在使用信号量保护临界区时用down_interruptible替代down
 *			 2.0 采用中断方式读写I2C总线, 目前为兼容以前接口,
 * 				 所有的数据结构保持不变, 以后有待改进
 * 
 * 当前版本：2.0
 * 作    者：wjzhe
 * 完成日期：2007年8月6日
 *
 * 取代版本：1.2 
 * 原作者  ：wjzhe
 * 完成日期：2007年4月4日
 */
#include <linux/unistd.h>   
#include <linux/ioport.h>  
#include <asm/io.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm-arm/arch-at91rm9200/AT91RM9200_SYS.h>
#include <asm/arch/pio.h>
#include <asm/arch/AT91RM9200_TWI.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/devfs_fs_kernel.h>
#include "usri2c.h"

#undef I2CLOCK
#define I2CSEM
#define PRIORITY 0
//#define DEBUG_I2C
#define I2CTBUF 5/* fram time before new transmit */

static int current_dev = -1;
static int i2c_enabled;
static AT91PS_TWI twi = (AT91PS_TWI) AT91C_VA_BASE_TWI;
static struct i2c_local i2c_dev[NR_I2C_DEVICES];
static enum devtype {ds1337dev, framdev} i2cdev;
#ifdef I2CSEM
static struct semaphore usri2c_lock;			/* protect access to SPI bus */
#endif
/* 定义struct completion, 用于wakeup */
//struct completion i2c_complete;
DECLARE_COMPLETION(i2c_complete);

#ifdef I2CLOCK
static DEFINE_SPINLOCK(usri2c_lock);
#endif

#if 0
static short poll(unsigned long bit) {
	int loop_cntr = 10000;
	do {
		udelay(10);
	} while (!(twi->TWI_SR & bit) && (--loop_cntr > 0));
	return (loop_cntr > 0);
}
#endif

static void reset_twi(void)
{
	twi->TWI_CR = AT91C_TWI_SWRST;		// software reset
	twi->TWI_IDR = 0x3ff;				// disable all interrupt
	twi->TWI_CR = AT91C_TWI_MSEN | AT91C_TWI_SVDIS;		// Master Transfer Enabled
	twi->TWI_CWGR = AT91C_TWI_CKDIV1 | AT91C_TWI_CLDIV3 | (AT91C_TWI_CLDIV3 << 8);
}

static irqreturn_t i2c_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
 	unsigned long status;
	struct i2c_local *device = &i2c_dev[current_dev];
#ifdef DEBUG_I2C
	printk("I2C interrupt %i\n", current_dev);
#endif
	status = twi->TWI_SR & twi->TWI_IMR;	/* read status */
	if (status != device->status) {
		printk("I2C status is %08x, but %08x\n", status, device->status);
		reset_twi();
		device->err = status;
		complete(&i2c_complete);
	}
	if (status == AT91C_TWI_TXRDY) {
		twi->TWI_THR = *(volatile char *)device->data;
		device->data++;
	} else if (status == AT91C_TWI_RXRDY) {
		*(volatile char *)device->data = twi->TWI_RHR;
		device->data++;
	} else if (status == AT91C_TWI_TXCOMP) {
		twi->TWI_IDR = 0x3FF;// close interrupt
		if (twi->TWI_SR & AT91C_TWI_RXRDY) {
			char tmp;
			tmp = twi->TWI_RHR;
			//printk("clear rxrdy %02x\n", tmp);
		}
		complete(&i2c_complete);
		goto out;
	} else {
		printk("I2C status error %08x\n", status);
		reset_twi();
		device->err = status;
		complete(&i2c_complete);
	}
	device->cnt--;
cnt0:
	if (device->cnt == 0) {
		//twi->TWI_IDR = device->status;//AT91C_TWI_RXRDY | AT91C_TWI_TXRDY;// close interrupt
		twi->TWI_CR = AT91C_TWI_STOP;
		// 到头了
#if 0
		if (status == AT91C_TWI_TXRDY) {
			twi->TWI_THR = *(volatile char *)device->data;
		} else if (status == AT91C_TWI_RXRDY) {
			*(volatile char *)device->data = twi->TWI_RHR;
		} else {
			printk("I2C status error %08x\n", status);
			reset_twi();
			device->err = status;
			complete(&i2c_complete);
		}
#endif
		twi->TWI_IDR = device->status;
		device->status = AT91C_TWI_TXCOMP;
		twi->TWI_IER = device->status;
		//twi->TWI_IER = device->status;
	}
out:	
	return IRQ_HANDLED;
}

static void access_i2c(short device)
{
	if ((device < 0) || (device >= NR_I2C_DEVICES))
		panic("at91_spi: spi_access_bus called with invalid device");

	if (i2c_enabled == 0) {
		AT91_SYS->PMC_PCER = 1 << AT91C_ID_TWI;	/* Enable Peripheral clock */
#ifdef DEBUG_I2C
		printk("I2C on\n");
#endif
	}
	i2c_enabled++;
	down(&usri2c_lock);
	current_dev = device;

	/* Enable PIO */
	if (!i2c_dev[device].pio_enabled) {
		AT91_CfgPIO_TWI();
		i2c_dev[device].pio_enabled = 1;
#ifdef DEBUG_I2C
		printk("I2C %i enabled\n", device);
#endif
	}
}

static void release_i2c(short device)
{
	if (device != current_dev)
		panic("at91_i2c: i2c_release called with invalid device");

	/* Release the SPI bus */
	current_dev = -1;

	i2c_enabled--;
	if (i2c_enabled == 0) {
		AT91_SYS->PMC_PCER = 1 << AT91C_ID_TWI;	/* Disable Peripheral clock */
#ifdef DEBUG_I2C
		printk("I2C off\n");
#endif
	}
	udelay(I2CTBUF);// 每次传输完停5 us
	up(&usri2c_lock);
}

static int msg_xfer(int flag, unsigned int addr, char *buf, unsigned int count, short device)
{
	//int ret, i;
	struct i2c_local *dev = &i2c_dev[device];
	unsigned int blockaddr;
	if (buf == NULL) {
		printk("xfer buff is NULL\n");
		return -EINVAL;
	}
	access_i2c(device);
	if (i2cdev == ds1337dev)
		twi->TWI_MMR = (DS_ADDR & 0x7F) << 16 | (flag & 0x1 ? AT91C_TWI_MREAD : 0) | AT91C_TWI_IADRSZ_1_BYTE;
	else {
		if (addr == 0xFFFFFFFF) {
			printk("addr ERROR!\n");
			return -1;
		}
		blockaddr = (addr >> 8) & 0x7;
		blockaddr |= FRAM_ADDR << 3;
		twi->TWI_MMR = (blockaddr & 0x7F) << 16 | (flag & 0x1 ? AT91C_TWI_MREAD : 0) | AT91C_TWI_IADRSZ_1_BYTE;
	}
	twi->TWI_IADR = addr & 0xFF;
	dev->cnt = count;
	dev->data = buf;
	// start twi
	//twi->TWI_CR = AT91C_TWI_START;
	if (flag & 0x1) {		// read
		dev->status = AT91C_TWI_RXRDY;
		twi->TWI_CR = AT91C_TWI_START;
		twi->TWI_IER = AT91C_TWI_RXRDY;	/* enable receive interrupt */
		wait_for_completion(&i2c_complete);
		if (dev->err) {
			dev->err = 0;
			printk("I2C interrupt AT91C_TWI_RXRDY error status\n");
			return -1;
		}
	} else {		// write
		twi->TWI_CR = AT91C_TWI_START;
		dev->status = AT91C_TWI_TXRDY;
		twi->TWI_IER = AT91C_TWI_TXRDY;	/* enable transmit interrupt */
#if 0
		twi->TWI_THR = *(volatile char *)dev->data;
		dev->data++;
		dev->cnt--;
		dev->status = AT91C_TWI_TXRDY;
		twi->TWI_IER = AT91C_TWI_TXRDY;	/* enable transmit interrupt */
		if (!dev->cnt) {
			twi->TWI_CR = AT91C_TWI_STOP;
		}
#endif
#if 0
		if (dev->cnt) {
		} else {
			dev->status = AT91C_TWI_TXCOMP;
			twi->TWI_CR = AT91C_TWI_STOP;
			twi->TWI_IER = AT91C_TWI_TXCOMP;
		}
#endif
		wait_for_completion(&i2c_complete);
		if (dev->err) {
			printk("I2C interrupt AT91C_TWI_TXRDY error status\n");
			dev->err = 0;
			return -1;
		}
	}
	dev->cnt = 0;
	dev->data = NULL;
	release_i2c(device);
	return 0;
}

static int tmbcd2bin(struct dstime *tm)
{
	int i;
	unsigned char *ptr = (unsigned char *)tm;
	unsigned char tmp;
	if (tm == NULL)
		return -EINVAL;
	for (i = 0; i < sizeof(struct dstime); i++) {
		tmp = *ptr;
		*ptr = BCD2BIN(tmp);
		ptr++;
	}
	return 0;
}

static int tmbin2bcd(struct dstime *tm)
{
	int i;
	unsigned char *ptr = (unsigned char *)tm;
	unsigned char tmp;
	if (tm == NULL)
		return -EINVAL;
	for (i = 0; i < sizeof(struct dstime); i++) {
		tmp = *ptr;
		*ptr = BIN2BCD(tmp);
		ptr++;
	}
	return 0;
}

static int usri2c_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	AT91_CfgPIO_TWI();
	twi->TWI_IDR = 0x3ff;				// disable all interrupt
	twi->TWI_CR = AT91C_TWI_SWRST;		// software reset
	twi->TWI_CR = AT91C_TWI_MSEN | AT91C_TWI_SVDIS;		// Master Transfer Enabled
	twi->TWI_CWGR = AT91C_TWI_CKDIV1 | AT91C_TWI_CLDIV3 | (AT91C_TWI_CLDIV3 << 8);
	AT91_SYS->PMC_PCER = 1 << AT91C_ID_TWI;		// enable TWI CLK
	//AT91_SYS->PMC_PCDR &= (!0x00001000);
	//ret = msg_xfer(1, DSSTAREG, &old_status, 1);
	/*
	 * 'private_data' is actually a pointer, but we overload it with the
	 * value we want to store.
	 */
	filp->private_data = (void *)MINOR(inode->i_rdev);
	return ret;
}

static int usri2c_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int device = (unsigned int)file->private_data;
	int i, ret;
	enum devtype tmpi2c;
	unsigned char dssta = 0;//, *pdstm;
	struct dstime dstm;
	unsigned int twi_clk, twi_sclock, twi_cldiv2, twi_cldiv3;//TWI_SCLOCK, TWI_CLDIV2
	memset(&dstm, 0, sizeof(dstm));
	switch (cmd) {
	case SETDSTIME:		//设置时钟时间
		if ((ret = copy_from_user(&dstm, (char *)arg, sizeof(dstm))) < 0) {
			printk("error copy from user!\n");
			return ret;
		}
		tmbin2bcd(&dstm);
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(0, DSSECREG, (char *)&dstm, sizeof(dstm), device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		break;
	case GETDSTIME:			//读取时钟时间
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSSECREG, (char *)&dstm, sizeof(dstm), device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		if (ret < 0)
			return ret;
		tmbcd2bin(&dstm);
		if ((ret = copy_to_user((struct dstime *)arg, &dstm, sizeof(dstm))) < 0) {
			printk("error copy from user!\n");
			return ret;
		}
		break;
	case GETDSALM1:
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSA1SEC, (char *)&dstm, 4, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		if (ret < 0) {
			printk("xfer error!\n");
			return ret;
		}
		if (dstm.wday & 0x40) {
			dstm.wday &= 0xF;
		}
		tmbcd2bin(&dstm);
		if ((ret = copy_to_user((struct dstime *)arg, &dstm, sizeof(dstm))) < 0) {
			printk("error copy from user!\n");
			return ret;
		}
		break;
	case GETDSALM2:
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSA2MIN, &dstm.min, 3, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		if (ret < 0) {
			printk("xfer error!\n");
			return ret;
		}
		if (dstm.wday & 0x40) {
			dstm.wday &= 0xF;
		}
		tmbcd2bin(&dstm);
		if ((ret = copy_to_user((struct dstime *)arg, &dstm, sizeof(dstm))) < 0) {
			printk("error copy from user!\n");
			return ret;
		}
		break;
	case SETDSALM1:
		if ((ret = copy_from_user(&dstm, (struct dstime *)arg, sizeof(dstm))) < 0) {
			printk("error copy from user!\n");
			return ret;
		}
		tmbin2bcd(&dstm);
		if (dstm.mday == 0 && dstm.wday == 0) {		// set A1M4
			dstm.wday = 0x80;
		} else if (dstm.mday != 0 && dstm.wday == 0) {
			dstm.wday = dstm.mday;
		} else /*if (dstm.wday != 0) */{
			dstm.wday |= 0x40;
		}
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(0, DSA1SEC, (char *)&dstm, 4, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		break;
	case SETDSALM2:
		if ((ret = copy_from_user(&dstm, (struct dstime *)arg, sizeof(dstm))) < 0) {
			printk("error copy from user!\n");
			return ret;
		}
		tmbin2bcd(&dstm);
		if (dstm.mday == 0 && dstm.wday == 0) {		// set A1M4
			dstm.wday = 0x80;
		} else if (dstm.mday != 0 && dstm.wday == 0) {
			dstm.wday = dstm.mday;
		} else /*if (dstm.wday != 0) */{
			dstm.wday |= 0x40;
		}
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(0, DSA2MIN, &dstm.min, 3, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		break;
	case STARTALM1:		//启动定时
		// first read control register
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSCTLREG, &dssta, 1, device);
		dssta |= (DSALARM1);
		// then send data
		ret = msg_xfer(0, DSCTLREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		break;
	case STOPALM1:		//关闭定时
		// first read control register
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSCTLREG, &dssta, 1, device);
		dssta &= (!DSALARM1);
		// then send data
		ret = msg_xfer(0, DSCTLREG, &dssta, 1, device);
		i2cdev = tmpi2c;
		//break;
	case CLEARINTA:		// not used
		// first read status register
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSSTAREG, &dssta, 1, device);
		dssta &= (!DSALARM1);
		// then send data
		ret = msg_xfer(0, DSSTAREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		break;
#if 0
	case RDOLDSTA:		//读取开机时时钟状态，主要用于判断是定时开机还是人为开机
		ret = put_user(old_status, (unsigned char *)arg);
		break;
#endif
	case RDDSSTATUS:		//读取当前时钟状态
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSSTAREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		ret = put_user(dssta, (unsigned char *)arg);
		break;
	case SETSPEED:
		ret = get_user(twi_clk, (unsigned int *)arg);
		if (ret < 0)
			return ret;
		twi_sclock = (10 * at91_master_clock / twi_clk);
		if ((twi_sclock % 10) >= 5) {
			twi_cldiv2 = (twi_sclock / 10) - 5;
		} else {
			twi_cldiv2 = (twi_sclock /10) -6;
		}
		twi_cldiv3 = (twi_cldiv2 + (4 - twi_cldiv2 % 4)) >> 2;
		twi->TWI_CWGR = AT91C_TWI_CKDIV1 | twi_cldiv3 | (twi_cldiv3 << 8);
		break;
	case SETI2CDEV:
		ret = get_user(i, (int *)arg);
		if (ret < 0)
			return ret;
		if (i == 0)
			i2cdev = ds1337dev;
		else
			i2cdev = framdev;
		break;
	case SETCTLREG:
		ret = get_user(dssta, (unsigned char *)arg);
		if (ret < 0)
			return ret;
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		// then send data
		ret = msg_xfer(0, DSCTLREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		break;
	case SETSTAREG:
		ret = get_user(dssta, (unsigned char *)arg);
		if (ret < 0)
			return ret;
#ifdef I2CLOCK
		spin_lock(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		// then send data
		ret = msg_xfer(0, DSSTAREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CLOCK
		spin_unlock(&usri2c_lock);
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int usri2c_write(struct file *file, const char *buff, size_t count, loff_t *offp)
{
	unsigned int device = (unsigned int)file->private_data;
	int ret = 0;
	enum devtype tmpi2c;
	char *pbuf;
	struct framdata fdata;
	if (buff == NULL)
		return -EINVAL;
	ret = copy_from_user(&fdata, buff, sizeof(fdata));
	if (ret < 0)
		return -EFAULT;
	if (fdata.size == 0)
		return 0;
	if (fdata.size > 4096 || fdata.size < 0) {
		printk("wdata too large!\n");
		return -EFBIG;
	}
	if (fdata.data == NULL)
		return -EINVAL;
	if (fdata.addr >= 2048)
		return -EINVAL;
	//printk("cnt is: %d", count);
	pbuf = (char *)kmalloc(fdata.size, GFP_KERNEL);
	if (!pbuf) {
		printk("Fram write: no memory!\n");
		return -ENOMEM;
	}
	ret = copy_from_user(pbuf, fdata.data, fdata.size);
	if (ret < 0) {
		kfree(pbuf);
		return -EFAULT;
	}
	//printk("pbuf[0] = %d\n", buff[i]);
	/* Lock the I2C bus */
#ifdef I2CSEM0
	if (down(&usri2c_lock)) {
		kfree(pbuf);
		return -ERESTARTSYS;
	}
#endif

#ifdef I2CLOCK
	spin_lock(&usri2c_lock);
#endif

	tmpi2c = i2cdev;
	i2cdev = framdev;
	//printk("cnt is: %d\n", cnt);
	ret = msg_xfer(0, fdata.addr, pbuf, fdata.size, device);
	i2cdev = tmpi2c;

#ifdef I2CSEM0
	up(&usri2c_lock);
#endif

#ifdef I2CLOCK
	spin_unlock(&usri2c_lock);
#endif

	kfree(pbuf);
	if (ret < 0) {
		return ret;
	}
	return fdata.size;
}

static int usri2c_read(struct file *file, char *buff, size_t count, loff_t *offp)
{
	unsigned int device = (unsigned int)file->private_data;
	int ret = 0;
	enum devtype tmpi2c;
	char *pbuf;
	struct framdata fdata;
	if (!buff) {
		printk("buff is NULL\n");
		return -EINVAL;
	}
	ret = copy_from_user(&fdata, buff, sizeof(fdata));
	if (ret < 0)
		return -EFAULT;
	if (fdata.size == 0)
		return 0;
	if (fdata.size > 4096 || fdata.size < 0) {
		printk("rdata too large!\n");
		return -EFBIG;
	}
	if (fdata.data == NULL)
		return -EINVAL;
	if (fdata.addr >= 2048)
		return -EINVAL;
	pbuf = (char *)kmalloc(fdata.size, GFP_KERNEL);
	if (!pbuf) {
		printk("no memory!\n");
		return -ENOMEM;
	}
#ifdef I2CSEM0
	if (down(&usri2c_lock)) {
		kfree(pbuf);
		return -ERESTARTSYS;
	}
#endif
#ifdef I2CLOCK
	spin_lock(&usri2c_lock);
#endif
	tmpi2c = i2cdev;
	i2cdev = framdev;
	//printk("xfer begin...\n");
	//printk("cnt is: %d\n", cnt);
	ret = msg_xfer(1, fdata.addr, pbuf, fdata.size, device);
	i2cdev = tmpi2c;
#ifdef I2CSEM0
	up(&usri2c_lock);
#endif
#ifdef I2CLOCK
	spin_unlock(&usri2c_lock);
#endif
	if (ret < 0) {
		kfree(pbuf);
		return ret;
	}
	ret = copy_to_user(fdata.data, pbuf, fdata.size);
	kfree(pbuf);
	if (ret < 0) {
		return -EFAULT;
	}
	return fdata.size;
}
struct file_operations usri2c_fops=
{
	.owner = THIS_MODULE,
	.open = usri2c_open,
	.ioctl = usri2c_ioctl,
	.write = usri2c_write,
	.read = usri2c_read,
	//.release:	usri2c_release,
};
		
static int usri2c_init(void)
{
	int ret;
#ifdef I2CSEM
	init_MUTEX(&usri2c_lock);
#endif
	ret = register_chrdev(USRI2C_MAJOR, "usri2c", &usri2c_fops);
	if (ret < 0) {
		printk("get device failed:fram\n");
		return -EBUSY;
	}
	if (request_irq(AT91C_ID_TWI, i2c_interrupt, 0, "i2c", NULL))
		return -EBUSY;
#if PRIORITY
	AT91_SYS->AIC_SMR[AT91C_ID_TWI] |= (PRIORITY & 0x7);/* irq 优先级 */
#endif
	devfs_mk_cdev(MKDEV(USRI2C_MAJOR, 0), S_IFCHR | S_IRUSR | S_IWUSR, "usri2c0");
	devfs_mk_cdev(MKDEV(USRI2C_MAJOR, 1), S_IFCHR | S_IRUSR | S_IWUSR, "usri2c1");
	printk("AT91 USRI2C 2.0 register success!\n");
	return ret;
}
static void usri2c_exit(void)
{
	unregister_chrdev(USRI2C_MAJOR, "usri2c");//注销
	printk("unregistering fram\n");
}

module_init(usri2c_init);
module_exit(usri2c_exit);
MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("USRI2C driver for NKTY AT91RM9200")

