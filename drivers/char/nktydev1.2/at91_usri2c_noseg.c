/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_usri2c.c
 * 摘    要：对I2C接口编程, 目前只支持铁电和时钟芯片DS1337
 *			 对铁电的操作用read和write实现, 对DS1337的操作用ioctl实现
 *			 1.1 使用自旋锁保护临界区
 * 
 * 当前版本：1.1
 * 作    者：wjzhe
 * 完成日期：2007年4月4日
 *
 * 取代版本：1.0 
 * 原作者  ：wjzhe
 * 完成日期：2007年2月6日
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
#include "usri2c.h"

#undef I2CLOCK
#define I2CSEM

static AT91PS_TWI twi = (AT91PS_TWI) AT91C_VA_BASE_TWI;
static enum devtype {ds1337dev, framdev} i2cdev;
#ifdef I2CSEM
static struct semaphore usri2c_lock;			/* protect access to SPI bus */
#endif

#ifdef I2CLOCK
static DEFINE_SPINLOCK(usri2c_lock);
#endif

static short poll(unsigned long bit) {
	int loop_cntr = 10000;
	do {
		udelay(10);
	} while (!(twi->TWI_SR & bit) && (--loop_cntr > 0));
	return (loop_cntr > 0);
}

static void reset_twi(void)
{
	twi->TWI_CR = AT91C_TWI_SWRST;		// software reset
	twi->TWI_IDR = 0x3ff;				// disable all interrupt
	twi->TWI_CR = AT91C_TWI_MSEN | AT91C_TWI_SVDIS;		// Master Transfer Enabled
	twi->TWI_CWGR = AT91C_TWI_CKDIV1 | AT91C_TWI_CLDIV3 | (AT91C_TWI_CLDIV3 << 8);
}

static int msg_xfer(int flag, unsigned int addr, char *buf, unsigned int count)
{
	//int ret, i;
	unsigned int blockaddr;
	if (buf == NULL) {
		printk("xfer buff is NULL\n");
		return -EINVAL;
	}
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
	twi->TWI_CR |= AT91C_TWI_START;
	if (flag & 0x1) {		// read
		while (count--) {
			if (!count)
				twi->TWI_CR |= AT91C_TWI_STOP;
			if (!poll(AT91C_TWI_RXRDY)) {
				printk(KERN_ERR "rxrdy time out! :%d\n", count);
				reset_twi();
				return -1;
			}
			*(volatile char *)buf++ = twi->TWI_RHR;
		}
		if (!poll(AT91C_TWI_TXCOMP)) {
			printk("rxcomp timeout!\n");
			reset_twi();
			return -1;
		}
	} else {		// write
		while (count--) {
			twi->TWI_THR = *(volatile char *)buf++;
			if (!count)
				twi->TWI_CR |= AT91C_TWI_STOP;
			if (!poll(AT91C_TWI_TXRDY)) {
				printk(KERN_ERR "txrdy time out! :%d\n", count);
				reset_twi();
				return -1;
			}
		}
		if (!poll(AT91C_TWI_TXCOMP)) {
			printk("txcomp timeout!\n");
			reset_twi();
			return -1;
		}
	}
	udelay(5);// 每次传输完停5 us
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
	AT91_SYS->PMC_PCER |= 1 << AT91C_ID_TWI;		// enable TWI CLK
	//AT91_SYS->PMC_PCDR &= (!0x00001000);
	//ret = msg_xfer(1, DSSTAREG, &old_status, 1);
	return ret;
}

static int usri2c_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
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
		ret = msg_xfer(0, DSSECREG, (char *)&dstm, sizeof(dstm));
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
		ret = msg_xfer(1, DSSECREG, (char *)&dstm, sizeof(dstm));
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
		ret = msg_xfer(1, DSA1SEC, (char *)&dstm, 4);
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
		ret = msg_xfer(1, DSA2MIN, &dstm.min, 3);
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
		ret = msg_xfer(0, DSA1SEC, (char *)&dstm, 4);
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
		ret = msg_xfer(0, DSA2MIN, &dstm.min, 3);
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
		ret = msg_xfer(1, DSCTLREG, &dssta, 1);
		dssta |= (DSALARM1);
		// then send data
		ret = msg_xfer(0, DSCTLREG, &dssta, 1);
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
		ret = msg_xfer(1, DSCTLREG, &dssta, 1);
		dssta &= (!DSALARM1);
		// then send data
		ret = msg_xfer(0, DSCTLREG, &dssta, 1);
		i2cdev = tmpi2c;
		//break;
	case CLEARINTA:		// not used
		// first read status register
		tmpi2c = i2cdev;
		i2cdev = ds1337dev;
		ret = msg_xfer(1, DSSTAREG, &dssta, 1);
		dssta &= (!DSALARM1);
		// then send data
		ret = msg_xfer(0, DSSTAREG, &dssta, 1);
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
		ret = msg_xfer(1, DSSTAREG, &dssta, 1);
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
		ret = msg_xfer(0, DSCTLREG, &dssta, 1);
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
		ret = msg_xfer(0, DSSTAREG, &dssta, 1);
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
	if (fdata.size > 4096) {
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

#ifdef I2CLOCK
	spin_lock(&usri2c_lock);
#endif

	tmpi2c = i2cdev;
	i2cdev = framdev;
	//printk("cnt is: %d\n", cnt);
	ret = msg_xfer(0, fdata.addr, pbuf, fdata.size);
	i2cdev = tmpi2c;

#ifdef I2CSEM
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
	if (fdata.size > 4096) {
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
#ifdef I2CLOCK
	spin_lock(&usri2c_lock);
#endif
	tmpi2c = i2cdev;
	i2cdev = framdev;
	//printk("xfer begin...\n");
	//printk("cnt is: %d\n", cnt);
	ret = msg_xfer(1, fdata.addr, pbuf, fdata.size);
	i2cdev = tmpi2c;
#ifdef I2CSEM
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
	printk("fram register success!\n");
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

