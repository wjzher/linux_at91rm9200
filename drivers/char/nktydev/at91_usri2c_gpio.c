/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_usri2c.c
 * 摘    要：对I2C接口编程, 目前只支持铁电和时钟芯片DS1337
 *			 对铁电的操作用read和write实现, 对DS1337的操作用ioctl实现
 *			 1.1 使用semaphore保护临界区
 *			 1.1.1 在传输时禁止中断
 * 			 1.2.1 加入对器件ZLG7290B的支持
 *			 1.3 采用GPIO方式读写I2C
 *
 * 当前版本：1.2.1
 * 作    者：wjzhe
 * 完成日期：2007年10月29日
 *
 * 取代版本：1.1.1
 * 作    者：wjzhe
 * 完成日期：2007年8月14日
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
#include <linux/rtc.h>
#include "usri2c.h"

#define I2CSEM

#define USRI2C_MAJOR_GPIO 137

#undef pr_debug
#if 0
#define pr_debug(fmt,arg...) \
	printk("%s %d: "fmt,__FILE__,__LINE__,##arg)
#else
#define pr_debug(fmt,arg...) \
	do { } while (0)
#endif
//static AT91PS_TWI twi = (AT91PS_TWI) AT91C_VA_BASE_TWI;

static enum devtype {ds1337dev,framdev
	,zlg7290/* add support for zlg7290b */
	} i2cdev;

#ifdef I2CSEM0
static struct semaphore usri2c_lock;			/* protect access to SPI bus */
#endif

#ifdef I2CSEM
DECLARE_MUTEX(usri2c_lock);			/* protect access to I2C bus */
#endif

#ifdef I2CLOCK
static DEFINE_SPINLOCK(usri2c_lock);
#endif
int i2c_delay = 3;

static inline int i2c_gpio_getsda(void)
{
	return at91_get_gpio_value(AT91_PIN_PA25);		// sda
}

static inline void i2c_gpio_setsda_val(int state)
{
	at91_set_gpio_value(AT91_PIN_PA25, state);		// sda
}

static void i2c_gpio_setscl_val(int state)
{
	at91_set_gpio_value(AT91_PIN_PA26, state);		// scl
}

static inline void sdalo(void)
{
	i2c_gpio_setsda_val(0);
	udelay((i2c_delay + 1) / 2);
}

static inline void sdahi(void)
{
	i2c_gpio_setsda_val(1);
	udelay((i2c_delay + 1) / 2);
}

static inline void scllo(void)
{
	i2c_gpio_setscl_val(0);
	udelay((i2c_delay + 1) / 2);
}

/*
 * Raise scl line, and do checking for delays. This is necessary for slower
 * devices.
 */
static inline int sclhi(void)
{
	i2c_gpio_setscl_val(1);
	udelay(i2c_delay);
	return 0;
} 



/* --- other auxiliary functions --------------------------------------	*/
static inline void i2c_start(void)
{
	/* assert: scl, sda are high */
	i2c_gpio_setsda_val(0);
	udelay(i2c_delay);
	scllo();
}

static inline void i2c_repstart(void)
{
	/* assert: scl is low */
	sdahi();
	sclhi();
	i2c_gpio_setsda_val(0);
	udelay(i2c_delay);
	scllo();
}


static inline void i2c_stop(void)
{
	/* assert: scl is low */
	sdalo();
	sclhi();
	i2c_gpio_setsda_val(1);
	udelay(i2c_delay);
}


static inline int i2c_inb(void)
{
	/* read byte via i2c port, without start/stop sequence	*/
	/* acknowledge is sent in i2c_read.			*/
	int i;
	unsigned char indata = 0;

	/* assert: scl is low */
	sdahi();
	for (i = 0; i < 8; i++) {
		if (sclhi() < 0) { /* timeout */
			return -ETIMEDOUT;
		}
		//indata *= 2;
		indata <<= 1;
		if (i2c_gpio_getsda())
			indata |= 0x01;
		scllo();
		//i2c_gpio_setscl_val(0);
		//udelay(i == 7 ? i2c_delay / 2 : i2c_delay);
	}
	/* assert: scl is low */
	udelay(20);
	return indata;
}

/* send a byte without start cond., look for arbitration,
   check ackn. from slave */
/* returns:
 * 1 if the device acknowledged
 * 0 if the device did not ack
 * -ETIMEDOUT if an error occurred (while raising the scl line)
 */
static inline int i2c_outb(unsigned char c)
{
	int i;
	int sb;
	int ack;
	/* assert: scl is low */
	for (i = 7; i >= 0; i--) {
		sb = (c >> i) & 1;
		i2c_gpio_setsda_val(sb);
		udelay((i2c_delay + 1) / 2);
		if (sclhi() < 0) { /* timed out */
			return -ETIMEDOUT;
		}
		/* FIXME do arbitration here:
		 * if (sb && !getsda(adap)) -> ouch! Get out of here.
		 *
		 * Report a unique code, so higher level code can retry
		 * the whole (combined) message and *NOT* issue STOP.
		 */
		scllo();
	}
	sdahi();
	if (sclhi() < 0) { /* timeout */
		return -ETIMEDOUT;
	}

	/* read ack: SDA should be pulled down by slave, or it may
	 * NAK (usually to report problems with the data we wrote).
	 */
	ack = !i2c_gpio_getsda();    /* ack: sda is pulled low -> success */
	scllo();
	udelay(20);
	return ack;
	/* assert: scl is low (sda undef) */
}
/* ----- Utility functions
 */

/* try_address tries to contact a chip for a number of
 * times before it gives up.
 * return values:
 * 1 chip answered
 * 0 chip did not answer
 * -x transmission error
 */
static inline int try_address(unsigned char addr, int retries)
{
	int i, ret = 0;
	for (i = 0; i <= retries; i++) {
		ret = i2c_outb(addr);
		if (ret == 1 || i == retries) {
			break;
		} else {
			printk("try address NACK recv\n");
		}
		//bit_dbg(3, &i2c_adap->dev, "emitting stop condition\n");
		i2c_stop();
		udelay(i2c_delay);
		yield();
		//bit_dbg(3, &i2c_adap->dev, "emitting start condition\n");
		i2c_start();
	}
	if (i && ret) {
	}
	return ret;
}

/* doAddress initiates the transfer by generating the start condition (in
 * try_address) and transmits the address in the necessary format to handle
 * reads, writes as well as 10bit-addresses.
 * addr => device address
 * flag => 1: read; 0: write.
 * returns:
 *  0 everything went okay, the chip ack'ed, or IGNORE_NAK flag was set
 * -x an error occurred (like: -EREMOTEIO if the device did not answer, or
 *	-ETIMEDOUT, for example if the lines are stuck...)
 */
static inline int bit_doAddress(unsigned char addr, int flag)
{
	int ret, retries;

	//retries = nak_ok ? 0 : i2c_adap->retries;
	retries = 2;		// 重试两次

	if (0) {	// ten bit address
		/* a ten bit address */
	} else {		/* normal 7bit address	*/
		addr = addr << 1;
		if (flag)
			addr |= 1;
		ret = try_address(addr, retries);
		if (ret != 1)
			return -ENXIO;
	}
	return 0;
}

// i2c 发送数据
static inline int sendbytes(unsigned char *data, int count)
{
	const unsigned char *temp = data;
	int retval;
	int wrcount = 0;

	while (count > 0) {
		retval = i2c_outb(*temp);

		/* OK/ACK; NAK error */
		if ((retval > 0)) {
			count--;
			temp++;
			wrcount++;

		/* A slave NAKing the master means the slave didn't like
		 * something about the data it saw.  For example, maybe
		 * the SMBus PEC was wrong.
		 */
		} else if (retval == 0) {
			return -EIO;

		/* Timeout; or (someday) lost arbitration
		 *
		 * FIXME Lost ARB implies retrying the transaction from
		 * the first message, after the "winning" master issues
		 * its STOP.  As a rule, upper layer code has no reason
		 * to know or care about this ... it is *NOT* an error.
		 */
		} else {
			return retval;
		}
	}
	return wrcount;
}

static inline int readbytes(unsigned char *buf, int len)
{
	int inval;
	int rdcount = 0;	/* counts bytes read */
	unsigned char *temp = buf;
	int count = len;

	while (count > 0) {
		inval = i2c_inb();
		*temp = inval;
		rdcount++;
#if 0
		if (inval >= 0) {
			*temp = inval;
			rdcount++;
		} else {   /* read timed out */
			break;
		}
#endif
		temp++;
		count--;
		// send ack
		if (count > 0) {
			sdalo();
		} else {			// ack
			sdahi();		// nack
		}
		sclhi();
		scllo();
		sdahi();
	}
	return rdcount;
}
//#define DISABLE_INT
static int msg_xfer(int flag, unsigned int addr, char *buf, int count)
{
	unsigned char dev_addr;
	int ret;
	if (buf == NULL) {
		printk("xfer buff is NULL\n");
		return -EINVAL;
	}
	// flag == 1, read
	// device address only one byte
	if (i2cdev == ds1337dev) {
		dev_addr = DS_ADDR;
		pr_debug("DS1337 f = %d, %d data %d\n", flag, addr, count);
		mdelay(1);
		i2c_delay = 3;
	} else if (i2cdev == zlg7290) {
		dev_addr = ZLG7290_ADDR;
		pr_debug("7290 f = %d, %d data %d\n", flag, addr, count);
		i2c_delay = 3;
		mdelay(1);
	} else {
		if (addr > 2047) {
			printk("fram addr error %d\n", addr);
		}
		dev_addr = (FRAM_ADDR << 3) | ((addr >> 8) & 0x7);
		i2c_delay = 1;
		pr_debug("fram f = %d, %d data %d\n", flag, addr, count);
#if 0
		{
			int i;
			for (i = 0; i < count; i++) {
				printk("%02x ", buf[i]);
			}
			printk("\n");
		}
#endif
	}
	// I2C开始
	i2c_start();
	// 发送地址
	ret = bit_doAddress(dev_addr, 0);
	if (ret) {
		pr_debug("bit_doAddress = %d\n", ret);
		goto bailout;
	}
	// 写入内部地址
	ret = sendbytes((unsigned char *)&addr, 1);
	if (ret != 1) {
		pr_debug("send addr %d error\n", addr);
		goto bailout;
	}
	// 开始读或写
	if (flag & 0x1) {		// read
		// 发送restart信号
		i2c_repstart();
		ret = bit_doAddress(dev_addr, flag);
		if (ret) {
			pr_debug("repstart bit_doAddress = %d\n", ret);
			goto bailout;
		}
		ret = readbytes(buf, count);
		if (ret != count) {
			pr_debug("readbytes = %d\n", ret);
			goto bailout;
		}
	} else {		// write
		ret = sendbytes(buf, count);
		if (ret != count) {
			pr_debug("sendbytes = %d\n", ret);
			goto bailout;
		}
	}
	ret = 0;
bailout:
	i2c_stop();
	udelay(3);
	return ret;
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
static inline int zlg7290_download(int num, int dp, int flash, char data)
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
	return msg_xfer(0, ZLG7290_CMDBUF1, cmd, sizeof(cmd));
}

static void at91_i2c_init(void)
{
#if 0
	AT91_CfgPIO_TWI();
	twi->TWI_IDR = 0x3ff;				// disable all interrupt
	twi->TWI_CR = AT91C_TWI_SWRST;		// software reset
	twi->TWI_CR = AT91C_TWI_MSEN | AT91C_TWI_SVDIS;		// Master Transfer Enabled
	twi->TWI_CWGR = AT91C_TWI_CKDIV1 | AT91C_TWI_CLDIV3 | (AT91C_TWI_CLDIV3 << 8);
	AT91_SYS->PMC_PCER |= 1 << AT91C_ID_TWI;		// enable TWI CLK
#endif
	// 开漏输出上拉电阻使能
	at91_set_GPIO_periph(AT91_PIN_PA25, 1);		/* TWD (SDA) */
	//printk("pa25 gpio periph ok\n");
	at91_set_multi_drive(AT91_PIN_PA25, 1);
	//printk("pa25 multi drive ok\n");
	at91_set_GPIO_periph(AT91_PIN_PA26, 1);		/* TWCK (SCL) */
	//printk("pa26 gpio periph ok\n");
	at91_set_multi_drive(AT91_PIN_PA26, 1);
	//printk("pa25 multi drive ok\n");
	gpio_direction_output(AT91_PIN_PA25, 1);
	gpio_direction_output(AT91_PIN_PA26, 1);
	return;
}

static int usri2c_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
#if 0
	AT91_CfgPIO_TWI();
	twi->TWI_IDR = 0x3ff;				// disable all interrupt
	twi->TWI_CR = AT91C_TWI_SWRST;		// software reset
	twi->TWI_CR = AT91C_TWI_MSEN | AT91C_TWI_SVDIS;		// Master Transfer Enabled
	twi->TWI_CWGR = AT91C_TWI_CKDIV1 | AT91C_TWI_CLDIV3 | (AT91C_TWI_CLDIV3 << 8);
	AT91_SYS->PMC_PCER |= 1 << AT91C_ID_TWI;		// enable TWI CLK
	//AT91_SYS->PMC_PCDR &= (!0x00001000);
	//ret = msg_xfer(1, DSSTAREG, &old_status, 1);
#endif
	at91_i2c_init();
	return ret;
}

static int usri2c_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int i, ret;
	enum devtype tmpi2c;
	unsigned char dssta = 0;//, *pdstm;
	struct dstime dstm;
	//unsigned int twi_clk, twi_sclock, twi_cldiv2, twi_cldiv3;//TWI_SCLOCK, TWI_CLDIV2
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
		ret = 0;
#if 0
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
#endif
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
	case ZLG7290_INIT:
		tmpi2c = i2cdev;
		i2cdev = zlg7290;
		{
			int i;
			unsigned char zlgtmp = 0;
			// high speed flash
			ret = msg_xfer(0, ZLG7290_FLASHONOFF, &zlgtmp, sizeof(zlgtmp));
			if (ret < 0) {
				goto out;
			}
			// scan number
			zlgtmp = _7_SEGNUM;
			ret = msg_xfer(0, ZLG7290_SCANNUM, &zlgtmp, sizeof(zlgtmp));
			// clear all
			for (i = 0; i < _7_SEGNUM; i++) {
				ret = zlg7290_download(i, 0, 0, 0x1F);
			}
		}
		i2cdev = tmpi2c;
		break;
	case ZLG7290_SHOW:
		tmpi2c = i2cdev;
		i2cdev = zlg7290;
		{
			struct zlg_data zlgdata;
			ret = copy_from_user(&zlgdata, (struct zlg_data *)arg, sizeof(zlgdata));
			if (ret < 0)
				goto out;
			ret = zlg7290_download(zlgdata.num, zlgdata.dp, zlgdata.flash, zlgdata.data);
		}
		i2cdev = tmpi2c;
		break;
	default:
		ret = -EINVAL;
		break;
	}
out:
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
	//printk("pbuf[0] = %d\n", fdata.data[0]);
	//printk("pbuf[1] = %d\n", fdata.data[1]);
	//printk("pbuf[2] = %d\n", fdata.data[2]);
	/* Lock the I2C bus */
#ifdef I2CSEM
	down(&usri2c_lock);
#endif

	tmpi2c = i2cdev;
	i2cdev = framdev;
	//printk("cnt is: %d\n", cnt);
	ret = msg_xfer(0, fdata.addr, pbuf, fdata.size);
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
#ifdef I2CSEM0
	init_MUTEX(&usri2c_lock);
#endif
	ret = register_chrdev(USRI2C_MAJOR_GPIO, "usri2c_gpio", &usri2c_fops);
	if (ret < 0) {
		printk("get device failed:fram\n");
		return -EBUSY;
	}
	printk("usri2c gpio register success!\n");
	return ret;
}
static void usri2c_exit(void)
{
	unregister_chrdev(USRI2C_MAJOR_GPIO, "usri2c_gpio");//注销
	printk("unregistering fram\n");
}
static int dstortc(struct rtc_time *prtm, struct dstime *pdtm)
{
	memset(prtm, 0, sizeof(struct rtc_time));
	prtm->tm_year = 100 + pdtm->year;
	prtm->tm_mon = pdtm->mon - 1;
	prtm->tm_mday = pdtm->mday;
	prtm->tm_wday = pdtm->wday;
	prtm->tm_hour = pdtm->hour;
	prtm->tm_min = pdtm->min;
	prtm->tm_sec = pdtm->sec;
	return 0;
}

// 用来获取ds1337的时间
int getds1337tm(struct rtc_time *rtctm)
{
	int ret;
	struct dstime dstm = {0};
	enum devtype tmpi2c = i2cdev;
	at91_i2c_init();
#ifdef I2CSEM
	down(&usri2c_lock);
#endif
	i2cdev = ds1337dev;
	ret = msg_xfer(1, DSSECREG, (char *)&dstm, sizeof(dstm));
	i2cdev = tmpi2c;
#ifdef I2CSEM
	up(&usri2c_lock);
#endif
	if (ret < 0)
		return ret;
	tmbcd2bin(&dstm);
	dstortc(rtctm, &dstm);
	return 0;
}

module_init(usri2c_init);
module_exit(usri2c_exit);
MODULE_LICENSE("GPL")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("USRI2C gpio driver for NKTY AT91RM9200")

