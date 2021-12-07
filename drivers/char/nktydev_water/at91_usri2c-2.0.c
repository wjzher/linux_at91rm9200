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
 *			 2.1 加入对ZLG7290的支持
 * 
 * 当前版本：2.1
 * 作    者：wjzhe
 * 完成日期：2007年12月5日
 *
 * 取代版本：2.0
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

#define ZLG7290B
#define I2CSEM
#define PRIORITY 0
//#define DEBUG_I2C
#define I2CTBUF 5/* fram time before new transmit */

static int current_dev = -1;
static int i2c_enabled;
static AT91PS_TWI twi = (AT91PS_TWI) AT91C_VA_BASE_TWI;
static struct i2c_local i2c_dev[NR_I2C_DEVICES];

static enum devtype {ds1337dev,framdev
#ifdef ZLG7290B
	,zlg7290/* add support for zlg7290b */
#endif
	} i2cdev;

#ifdef I2CSEM
static struct semaphore usri2c_lock;			/* protect access to SPI bus */
#endif
/* 定义struct completion, 用于wakeup */
//struct completion i2c_complete;
DECLARE_COMPLETION(i2c_complete);

#ifdef I2CLOCK
static DEFINE_SPINLOCK(usri2c_lock);
#endif

// wait for complete timeout
#define I2CTIMEOUT
#define I2CTMJFF 10

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
		printk("I2C status is %08x, but %08x\n", (int)status, (int)device->status);
		reset_twi();
		device->err = status;
		complete(&i2c_complete);
		goto out;
	}
	if (status == AT91C_TWI_TXRDY) {
		twi->TWI_THR = *(volatile char *)device->data;
		device->data++;
	} else if (status == AT91C_TWI_RXRDY) {
		*(volatile char *)device->data = twi->TWI_RHR;
		device->data++;
	} else if (status == AT91C_TWI_TXCOMP) {
		if (twi->TWI_SR & AT91C_TWI_RXRDY) {
			char tmp;
			tmp = twi->TWI_RHR;
			//printk("clear rxrdy %02x\n", tmp);
		}
		twi->TWI_IDR = 0x3FF;// close interrupt
		complete(&i2c_complete);
		goto out;
	} else {
		printk("I2C status error %08x\n", (int)status);
		reset_twi();
		device->err = status;
		complete(&i2c_complete);
		goto out;
	}
	device->cnt--;
//cnt0:
	if (device->cnt <= 0) {
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

#if 0
#ifdef I2CSEM
	down(&usri2c_lock);
#endif
#endif
	if (i2c_enabled == 0) {
		AT91_SYS->PMC_PCER = 1 << AT91C_ID_TWI;	/* Enable Peripheral clock */
#ifdef DEBUG_I2C
		printk("I2C on\n");
#endif
	}
	i2c_enabled++;
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
		AT91_SYS->PMC_PCDR = 1 << AT91C_ID_TWI;	/* Disable Peripheral clock */
#ifdef DEBUG_I2C
		printk("I2C off\n");
#endif
	}
	udelay(I2CTBUF);// 每次传输完停5 us
#if 0
#ifdef I2CSEM
	up(&usri2c_lock);
#endif
#endif
}

static int msg_xfer(int flag, unsigned int addr, char *buf, unsigned int count, short device)
{
	//int ret, i;
	struct i2c_local *dev = &i2c_dev[device];
	unsigned int blockaddr;
#ifdef ZLG7290B
	unsigned long twiclk_reg = 0;
#endif
#ifdef I2CTIMEOUT
	int ret = 0;
	unsigned long i2ctmout = I2CTMJFF;
#endif
	if (buf == NULL) {
		printk("xfer buff is NULL\n");
		return -EINVAL;
	}
	access_i2c(device);
	if (i2cdev == ds1337dev)
		twi->TWI_MMR = (DS_ADDR & 0x7F) << 16 | (flag & 0x1 ? AT91C_TWI_MREAD : 0) | AT91C_TWI_IADRSZ_1_BYTE;
#ifdef ZLG7290B
	else if (i2cdev == zlg7290) {
		unsigned int twi_clk, twi_sclock, twi_cldiv2, twi_cldiv3;
		/* must set twi clock */
		twiclk_reg = twi->TWI_CWGR;
		twi_clk = ZLG7290_CLK;
		twi_sclock = (10 * at91_master_clock / twi_clk);
		if ((twi_sclock % 10) >= 5) {
			twi_cldiv2 = (twi_sclock / 10) - 5;
		} else {
			twi_cldiv2 = (twi_sclock /10) -6;
		}
		twi_cldiv3 = (twi_cldiv2 + (4 - twi_cldiv2 % 4)) >> 2;
		twi->TWI_CWGR = AT91C_TWI_CKDIV1 | twi_cldiv3 | (twi_cldiv3 << 8);
		/* write slave addr and trans mode */
		twi->TWI_MMR = (ZLG7290_ADDR) << 16 | (flag & 0x1 ? AT91C_TWI_MREAD : 0) | AT91C_TWI_IADRSZ_1_BYTE;
	}
#endif
	else {
		if (addr == 0xFFFFFFFF) {
			printk("addr ERROR!\n");
			goto errout;
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
#ifndef I2CTIMEOUT
		wait_for_completion(&i2c_complete);
#else
		ret = wait_for_completion_timeout(&i2c_complete, i2ctmout);
		if (ret  == 0) {
			printk("i2c xfer timeout!\n");
			goto errout;
		}
#if 0

		else if (ret < 0) {
			printk("i2c xfer system error!\n");
			goto errout;
		}
#endif
#endif
		if (dev->err) {
			dev->err = 0;
			printk("I2C interrupt AT91C_TWI_RXRDY error status\n");
			goto errout;
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
#ifndef I2CTIMEOUT
		wait_for_completion(&i2c_complete);
#else
		ret = wait_for_completion_timeout(&i2c_complete, i2ctmout);
		if (ret  == 0) {
			printk("i2c xfer timeout!\n");
			goto errout;
		}
#if 0
		else if (ret < 0) {
			printk("i2c xfer system error!\n");
			goto errout;
		}
#endif
#endif
		if (dev->err) {
			printk("I2C interrupt AT91C_TWI_TXRDY error status\n");
			dev->err = 0;
			goto errout;
		}
	}
	dev->cnt = 0;
	dev->data = NULL;
	release_i2c(device);
#ifdef ZLG7290B
	if (i2cdev == zlg7290) {
		/* restore twi clock register */
		twi->TWI_CWGR = twiclk_reg;
	}
#endif
	return 0;
errout:
#ifdef ZLG7290B
	if (i2cdev == zlg7290) {
		/* restore twi clock register */
		twi->TWI_CWGR = twiclk_reg;
	}
#endif
	//reset_twi();
	release_i2c(device);
	return -1;
}

#ifdef ZLG7290B
/*
函数：zlg7290_download()
功能：下载数据并译码
参数：
addr：取值0～7，显示缓存DpRam0～DpRam7 的编号
dp：是否点亮该位的小数点，0－熄灭，1－点亮
flash：控制该位是否闪烁，0－不闪烁，1－闪烁
data：取值0～31，表示要显示的数据
返回：
0：正常
非0：访问ZLG7290 时出现异常
说明：
显示数据具体的译码方式请参见ZLG7290 的数据手册
*/
static inline int zlg7290_download(int device, int num, int dp, int flash, char data)
{
	static unsigned char usrnum[8] = {4, 5, 6, 0, 1, 2};
	unsigned char cmd[2];
	cmd[0] = usrnum[num & 0x7];//num & 0x07;
	cmd[0] |= 0x60;
	cmd[1] = data & 0x1F;
	if (dp)
		cmd[1] |= 0x80;
	if (flash)
		cmd[1] |= 0x40;
	return msg_xfer(0, ZLG7290_CMDBUF1, cmd, sizeof(cmd), device);
}
#endif

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
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(0, DSSECREG, (char *)&dstm, sizeof(dstm), device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
	case GETDSTIME:			//读取时钟时间
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSSECREG, (char *)&dstm, sizeof(dstm), device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
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
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSA1SEC, (char *)&dstm, 4, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
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
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSA2MIN, &dstm.min, 3, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
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
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(0, DSA1SEC, (char *)&dstm, 4, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
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
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(0, DSA2MIN, &dstm.min, 3, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
	case STARTALM1:		//启动定时
		// first read control register
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSCTLREG, &dssta, 1, device);
		dssta |= (DSALARM1);
		// then send data
		ret = msg_xfer(0, DSCTLREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
	case STOPALM1:		//关闭定时
		// first read control register
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSCTLREG, &dssta, 1, device);
		dssta &= (!DSALARM1);
		// then send data
		ret = msg_xfer(0, DSCTLREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		//break;
	case CLEARINTA:		// not used
		// first read status register
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSSTAREG, &dssta, 1, device);
		dssta &= (!DSALARM1);
		// then send data
		ret = msg_xfer(0, DSSTAREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
#if 0
	case RDOLDSTA:		//读取开机时时钟状态，主要用于判断是定时开机还是人为开机
		ret = put_user(old_status, (unsigned char *)arg);
		break;
#endif
	case RDDSSTATUS:		//读取当前时钟状态
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSSTAREG, &dssta, 1, device);
		i2cdev = tmpi2c;
		ret = put_user(dssta, (unsigned char *)arg);
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
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
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		twi->TWI_CWGR = AT91C_TWI_CKDIV1 | twi_cldiv3 | (twi_cldiv3 << 8);
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
	case SETI2CDEV:
		ret = get_user(i, (int *)arg);
		if (ret < 0)
			return ret;
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		if (i == 0)
			i2cdev = ds1337dev;
		else
			i2cdev = framdev;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
	case SETCTLREG:
		ret = get_user(dssta, (unsigned char *)arg);
		if (ret < 0)
			return ret;
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		// then send data
		ret = msg_xfer(0, DSCTLREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
	case SETSTAREG:
		ret = get_user(dssta, (unsigned char *)arg);
		if (ret < 0)
			return ret;
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		// then send data
		ret = msg_xfer(0, DSSTAREG, &dssta, 1, device);
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
#ifdef ZLG7290B
	case ZLG7290_INIT:
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = zlg7290;
		{
			int i;
			unsigned char zlgtmp = 0;
			// high speed flash
			ret = msg_xfer(0, ZLG7290_FLASHONOFF, &zlgtmp, sizeof(zlgtmp), device);
			if (ret < 0) {
				goto out;
			}
			// scan number
			zlgtmp = _7_SEGNUM;
			ret = msg_xfer(0, ZLG7290_SCANNUM, &zlgtmp, sizeof(zlgtmp), device);
			// clear all
			for (i = 0; i < _7_SEGNUM; i++) {
				ret = zlg7290_download(device, i, 0, 0, 0x1F);
			}
		}
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
	case ZLG7290_SHOW:
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = zlg7290;
		{
			struct zlg_data zlgdata;
			ret = copy_from_user(&zlgdata, (struct zlg_data *)arg, sizeof(zlgdata));
			if (ret < 0)
				goto out;
			ret = zlg7290_download(device, zlgdata.num, zlgdata.dp, zlgdata.flash, zlgdata.data);
		}
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
	case ZLG7290_FLASH:
#ifdef I2CSEM
		down(&usri2c_lock);
#endif
		tmpi2c = i2cdev;
		i2cdev = zlg7290;
		{
			unsigned char zlgtmp = arg / 280;
			if ((arg % 280) >= 140)
				zlgtmp += 1;
			// high speed flash
			ret = msg_xfer(0, ZLG7290_FLASHONOFF, &zlgtmp, sizeof(zlgtmp), device);
			if (ret < 0) {
				goto out;
			}
 		}
		i2cdev = tmpi2c;
#ifdef I2CSEM
		up(&usri2c_lock);
#endif
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}
#ifdef ZLG7290B
out:
#endif
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
#ifdef I2CSEM
	down(&usri2c_lock);
#endif

	tmpi2c = i2cdev;
	i2cdev = framdev;
	//printk("cnt is: %d\n", cnt);
	ret = msg_xfer(0, fdata.addr, pbuf, fdata.size, device);
	i2cdev = tmpi2c;

#ifdef I2CSEM
	up(&usri2c_lock);
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
#ifdef I2CSEM
	down(&usri2c_lock);
#endif
	tmpi2c = i2cdev;
	i2cdev = framdev;
	//printk("xfer begin...\n");
	//printk("cnt is: %d\n", cnt);
	ret = msg_xfer(1, fdata.addr, pbuf, fdata.size, device);
	i2cdev = tmpi2c;
#ifdef I2CSEM
	up(&usri2c_lock);
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

