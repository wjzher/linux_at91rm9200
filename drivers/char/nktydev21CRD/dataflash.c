/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_gpiob.c
 * 摘    要：DATAFLASH驱动程序, 对SPI总线进行操作, 现在只支持SPI0
 * 			 调用at91_spi.c里的传输函数, 需要AT91_SPI的支持,
 *			 修改对SPI_CSR0寄存器的配置, 将DLYBCT设置为1
 *			 加入对AT45DB642D芯片的支持, 新的读命令, 程序可以自动判断芯片
 *			 SPI总线可以稳定的工作在10MHz
 * 
 * 当前版本：2.2
 * 作    者：wjzhe
 * 完成日期：2007年8月8日
 *
 * 取代版本：2.1
 * 原作者  ：wjzhe
 * 完成日期：2007年4月19日
 *
 * 取代版本：1.0
 * 原作者  ：fengyun
 * 完成日期：2006年4月5日
 */

#include <linux/init.h>
#include <linux/module.h>
#include <asm/semaphore.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/at91_spi.h>

#include <asm/arch/AT91RM9200_SPI.h>
#include <asm/arch/board.h>
#include <asm/arch/pio.h>
#include <asm/uaccess.h>

#include "dataflash.h"
#define DEBUG_DATAFLASH
#undef DEBUG_DATAFLASH
#define DEFAULT_DATAFLASH_CLK 6000000

static int at45_spi_baud;

#undef	DATAFLASH_ALWAYS_ADD_DEVICE	/* always add whole device when using partitions? */
#define DATAFLASH_MAX_DEVICES	2	/* max number of dataflash devices */
#define DATAFLASH_MAJOR 132

#define OP_READ_CONTINUOUS	0xE8
#define OP_READ_CONTINUOUS_NEW 0x0B
#define OP_READ_PAGE		0xD2
#define OP_READ_BUFFER1		0xD4
#define OP_READ_BUFFER2		0xD6
#define OP_READ_STATUS		0xD7

#define OP_ERASE_PAGE		0x81
#define OP_ERASE_BLOCK		0x50

#define OP_TRANSFER_BUF1	0x53
#define OP_TRANSFER_BUF2	0x55
#define OP_COMPARE_BUF1		0x60
#define OP_COMPARE_BUF2		0x61

#define OP_PROGRAM_VIA_BUF1	0x82
#define OP_PROGRAM_VIA_BUF2	0x85

#define DATAFLASH_DLYBCT 1

struct dataflash_local
{
	int spi;			/* SPI chip-select number */
	int isnew;			/* Flag if use new write/read command */
	unsigned int page_size;		/* number of bytes per page */
	unsigned short page_offset;	/* page offset in flash address */
};

static unsigned int spi_csr[NR_SPI_DEVICES];// 用来保存CSR[dev]的数据

/* Detected DataFlash devices */
static int nr_devices = 0;
static struct dataflash_local *dflocal;

/* ......................................................................... */

/* Allocate a single SPI transfer descriptor.  We're assuming that if multiple
   SPI transfers occur at the same time, spi_access_bus() will serialize them.
   If this is not valid, then either (i) each dataflash 'priv' structure
   needs it's own transfer descriptor, (ii) we lock this one, or (iii) use
   another mechanism.   */
static struct spi_transfer_list* spi_transfer_desc;

static df_addr ch_addr(df_addr high, df_addr low)
{
	low &= 0x7ff;
	if (low >= 1056)
		low -= 1056;
	high = high << 11;
	return high | low;
}

/*
 * Perform a SPI transfer to access the DataFlash device.
 */
static int do_spi_trans(int nr, char* tx, int tx_len, char* rx, int rx_len,
		char* txnext, int txnext_len, char* rxnext, int rxnext_len)
{
	struct spi_transfer_list* list = spi_transfer_desc;

	list->tx[0] = tx;	list->txlen[0] = tx_len;
	list->rx[0] = rx;	list->rxlen[0] = rx_len;

	list->tx[1] = txnext; 	list->txlen[1] = txnext_len;
	list->rx[1] = rxnext;	list->rxlen[1] = rxnext_len;

	list->nr_transfers = nr;

	return spi_transfer(list);
}

/* ......................................................................... */

/*
 * Poll the DataFlash device until it is READY.
 */
static void dataflash_waitready(void)
{
	char* command = kmalloc(2, GFP_KERNEL);

	if (!command)
		return;

	do {
		command[0] = OP_READ_STATUS;
		command[1] = 0;

		do_spi_trans(1, command, 2, command, 2, NULL, 0, NULL, 0);
		if (command[1] == 0xFF) {
			printk("wait flash ready recv 0xFF\n");
			continue;
		}
	} while ((command[1] & 0x80) == 0);

	kfree(command);
}

/*
 * Return the status of the DataFlash device.
 */
static unsigned short dataflash_status(void)
{
	unsigned short status;
	char* command = kmalloc(2, GFP_KERNEL);

	if (!command)
		return 0;

	command[0] = OP_READ_STATUS;
	command[1] = 0;

	do_spi_trans(1, command, 2, command, 2, NULL, 0, NULL, 0);
	status = command[1];

	kfree(command);
	return status;
}

/* ......................................................................... */
/*
 * Erase blocks of flash. flag: 1-->erase page, 0-->erase block
 */
static int dataflash_erase(struct dataflash_local *priv, int pageaddr, int cnt, int flag)
{
	//struct dataflash_local *priv = ...filp->private_data;
	unsigned int addr;
	char* command;

#ifdef DEBUG_DATAFLASH
	printk("dataflash_erase: pageaddr=%d cnt=%d\n", pageaddr, cnt);
#endif

	command = kmalloc(4, GFP_KERNEL);
	if (!command)
		return -ENOMEM;
	if (flag) {
		while (cnt--) {
			addr = ch_addr(pageaddr, 0);
			/* Get flash page address */
			command[0] = OP_ERASE_PAGE;
			command[1] = (addr & 0x00FF0000) >> 16;
			command[2] = (addr & 0x0000FF00) >> 8;
			command[3] = 0;
#ifdef DEBUG_DATAFLASH
			printk("ERASE: (%x) %x %x %x [%i]\n", command[0], command[1], command[2], command[3], pageaddr);
#endif

			/* Send command to SPI device */
			spi_access_bus(priv->spi);
			do_spi_trans(1, command, 4, command, 4, NULL, 0, NULL, 0);

			dataflash_waitready();		/* poll status until ready */
			spi_release_bus(priv->spi);
			pageaddr++;
		}
	} else {
		while (cnt--) {
			addr = ch_addr(pageaddr, 0);
			/* Get flash page address */
			command[0] = OP_ERASE_BLOCK;
			command[1] = (addr & 0x00FF0000) >> 16;
			command[2] = (addr & 0x0000FF00) >> 8;
			command[3] = 0;
#ifdef DEBUG_DATAFLASH
			printk("ERASE: (%x) %x %x %x [%i]\n", command[0], command[1], command[2], command[3], pageaddr);
#endif

			/* Send command to SPI device */
			spi_access_bus(priv->spi);
			do_spi_trans(1, command, 4, command, 4, NULL, 0, NULL, 0);

			dataflash_waitready();		/* poll status until ready */
			spi_release_bus(priv->spi);
			pageaddr += 8;// 一块8页
		}
	}

	kfree(command);
	return 0;
}
/*
 * dataflash ioctl fuction: do with some especial operation
 */
static int dataflash_ioctl(struct inode * inode,struct file* file, unsigned int cmd, unsigned long arg)
{
	int ret;
	AT91PS_SPI spi_ctl = (AT91PS_SPI) AT91C_VA_BASE_SPI;
	int baud;
	struct dataflash_local *priv = (struct dataflash_local *)file->private_data;
	unsigned int addr, cnt;
	switch (cmd) {
		case ERASE_PAGE:
			if ((ret = copy_from_user(&addr, (int *)arg, sizeof(addr))) < 0) {
				return ret;
			}
			cnt = addr & 0xFFFF;
			addr = addr >> 16;		// 此地址为页地址
			if ((addr + cnt) >= 8192)
				return -EINVAL;
			if (dataflash_erase(priv, addr, cnt, 1) < 0)
				return -1;
			break;
		case ERASE_BLOCK:
			if ((ret = copy_from_user(&addr, (int *)arg, sizeof(addr))) < 0) {
				return ret;
			}
			cnt = addr & 0xFFFF;
			addr = addr >> 16;		// 此地址为块地址
			if ((addr + cnt) >= 1024)
				return -EINVAL;
			if (dataflash_erase(priv, addr, cnt, 0) < 0)
				return -1;
			break;
		case WAITREADY:
			spi_access_bus(priv->spi);
			dataflash_waitready();		/* poll status until ready */
			spi_release_bus(priv->spi);
			break;
		case SETBAUD:
			ret = copy_from_user(&baud, (int *)arg, sizeof(baud));
			if (ret < 0)
				return ret;
			baud = at91_master_clock / (2 * baud);
			if (baud < 2)
				baud = 2;
			if (baud > 255)
				baud = 255;
			switch (priv->spi) {
				case 0:
					spi_ctl->SPI_CSR0 &= 0xFFFF00FF;
					spi_ctl->SPI_CSR0 |= baud << 8;
					break;
				case 1:
					spi_ctl->SPI_CSR1 &= 0xFFFF00FF;
					spi_ctl->SPI_CSR1 |= baud << 8;
					break;
				case 2:
					spi_ctl->SPI_CSR2 &= 0xFFFF00FF;
					spi_ctl->SPI_CSR2 |= baud << 8;
					break;
				case 3:
					spi_ctl->SPI_CSR3 &= 0xFFFF00FF;
					spi_ctl->SPI_CSR3 |= baud << 8;
					break;
				default:
					return -EINVAL;
			}
			at45_spi_baud = baud;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

/*
 * Read from the DataFlash device.
 *   from   : Start offset in flash device
 *   len    : Amount to read
 *   retlen : About of data actually read
 *   buf    : Buffer containing the data
 */
static int dataflash_read(struct file *file, char *buf, size_t count, loff_t *offp)
{
	struct dataflash_local *priv = (struct dataflash_local *)file->private_data;
	unsigned int addr;
	char* command, *rx;
	int ret;
	int len = 8;

	if (count == 0)
		return 0;
	if (count > BYTEOFDFPAGE * 3)// 目前驱动不允许读超过4页数据
		return -EINVAL;

	/* Get flash page/byte address */
	ret = copy_from_user(&addr, buf, sizeof(addr));
	if (ret < 0)
		return ret;
#ifdef DEBUG_DATAFLASH
	printk("dataflash_read: 0x%x, %d\n", addr, count);
#endif
	rx = (char *)kmalloc(count, GFP_KERNEL);
	if (!rx)
		return -ENOMEM;
	if (priv->isnew)
		len = 5;

	command = kmalloc(len, GFP_KERNEL);
	if (!command) {
		kfree(rx);
		return -ENOMEM;
	}
	if (priv->isnew) {
		command[0] = OP_READ_CONTINUOUS_NEW;
	} else {
		command[0] = OP_READ_CONTINUOUS;
	}
	command[1] = (addr & 0x00FF0000) >> 16;
	command[2] = (addr & 0x0000FF00) >> 8;
	command[3] = (addr & 0x000000FF);
#ifdef DEBUG_DATAFLASH
	printk("READ: (%x) %x %x %x\n", command[0], command[1], command[2], command[3]);
#endif

	/* Send command to SPI device */
	spi_access_bus(priv->spi);
	//dataflash_waitready();
	do_spi_trans(2, command, len, command, len, rx, count, rx, count);
	spi_release_bus(priv->spi);

	if ((ret = copy_to_user(buf, rx, count)) < 0) {
		kfree(rx);
		kfree(command);
		return ret;
	}
	kfree(rx);
	kfree(command);
	return count;
}

/*
 * Write to the DataFlash device.
 *   to     : Start offset in flash device
 *   len    : Amount to write
 *   retlen : Amount of data actually written
 *   buf    : Buffer containing the data, and addr
 */
static int dataflash_write(struct file *file, const char *buf, size_t count, loff_t *offp)
{
	struct dataflash_local *priv = (struct dataflash_local *)file->private_data;
	unsigned int addr, writelen, pageaddr;
	size_t poffset;		// 页内地址
	u_char *writebuf;
	unsigned short status;
	int res = 0;
	char* command;
	char* tmpbuf = NULL;// 似乎用来第二次传输时接收缓冲

	if (!buf)
		return -EINVAL;
	if (!count)
		return 0;
	if ((res = copy_from_user(&addr, buf, sizeof(addr))) < 0) {
		return res;
	}
#ifdef DEBUG_DATAFLASH
	printk("dataflash_write to %d: 0x%x , %d\n", priv->spi, addr, count);
#endif
	buf += sizeof(addr);
	poffset = addr & 0x7ff;
	pageaddr = addr >> priv->page_offset;
	if (count > (priv->page_size - poffset))// 只允许写一页, 不跨页
		return -EINVAL;

	command = kmalloc(4, GFP_KERNEL);
	if (!command)
		return -ENOMEM;

	writelen = count;
	writebuf = (u_char *)kmalloc(count, GFP_KERNEL);
	if (!writebuf) {
		kfree(command);
		return -ENOMEM;
	}
	// 将用户数据拷贝到内核中
	if ((res = copy_from_user(writebuf, buf, count)) < 0)
		return res;

	/* Allocate temporary buffer */
	tmpbuf = kmalloc(priv->page_size, GFP_KERNEL);
	if (!tmpbuf) {
		kfree(command);
		kfree(writebuf);
		return -ENOMEM;
	}
	// 只可能写一页所以不用循环
	/* Gain access to the SPI bus */
	spi_access_bus(priv->spi);

	/* (1) Transfer to Buffer1 */
	if (poffset) {	// 如果不是从一页的开始写数据则要把前面的数据读到BUF1
		unsigned int raddr;
		raddr = pageaddr << priv->page_offset;
		command[0] = OP_TRANSFER_BUF1;
		command[1] = (raddr & 0x00FF0000) >> 16;
		command[2] = (raddr & 0x0000FF00) >> 8;
		command[3] = 0;
#ifdef DEBUG_DATAFLASH
		printk("TRANSFER: (%x) %x %x %x\n", command[0], command[1], command[2], command[3]);
#endif
		do_spi_trans(1, command, 4, command, 4, NULL, 0, NULL, 0);
		dataflash_waitready();
	}

	/* (2) Program via Buffer1 */
	//addr = (pageaddr << priv->page_offset) + offset;
	command[0] = OP_PROGRAM_VIA_BUF1;
	command[1] = (addr & 0x00FF0000) >> 16;
	command[2] = (addr & 0x0000FF00) >> 8;
	command[3] = (addr & 0x000000FF);
#ifdef DEBUG_DATAFLASH
	printk("PROGRAM: (%x) %x %x %x\n", command[0], command[1], command[2], command[3]);
#endif
	do_spi_trans(2, command, 4, command, 4, writebuf, writelen, tmpbuf, writelen);
	dataflash_waitready();
	res = writelen;
	/* (3) Compare to Buffer1 */
	addr = pageaddr << priv->page_offset;
	command[0] = OP_COMPARE_BUF1;
	command[1] = (addr & 0x00FF0000) >> 16;
	command[2] = (addr & 0x0000FF00) >> 8;
	command[3] = 0;
#ifdef DEBUG_DATAFLASH
	printk("COMPARE: (%x) %x %x %x\n", command[0], command[1], command[2], command[3]);
#endif
	do_spi_trans(1, command, 4, command, 4, NULL, 0, NULL, 0);
	dataflash_waitready();

	/* Get result of the compare operation */
	status = dataflash_status();
	if ((status & 0x40) == 1) {
		printk("at91_dataflash: Write error on page %i\n", pageaddr);
		res = -EIO;
	}

	/* Release SPI bus */
	spi_release_bus(priv->spi);

	kfree(tmpbuf);
	kfree(command);
	kfree(writebuf);
	return res;
}

/* ......................................................................... */

/*
 * Initialize and register DataFlash device with MTD subsystem.
 */
static int add_dataflash(int channel, char *name, int IDsize,
		int nr_pages, int pagesize, int pageoffset)
{
#define NEWNAME "AT45DB642D"
	struct dataflash_local *priv = dflocal + channel;

	if (nr_devices >= DATAFLASH_MAX_DEVICES) {
		printk(KERN_ERR "at91_dataflash: Too many devices detected\n");
		return 0;
	}

	printk("%s.spi%d\n", name, channel);

	memset(priv, 0, sizeof(struct dataflash_local));

	priv->spi = channel;
	priv->page_size = pagesize;
	priv->page_offset = pageoffset;// 地址BIT有关, 不是页内地址

	if (strlen(name) == strlen(NEWNAME)) {
		if (!strcmp(name, NEWNAME)) {
#ifdef DEBUG_DATAFLASH
			printk("New device, use new command: %s, %s\n", name, NEWNAME);
#endif
			priv->isnew = 1;
		}
	}

	nr_devices++;
	printk("at91_dataflash: %s detected [spi%i] (%i bytes)\n", name, channel, nr_pages * pagesize);

	return 0;		/* add whole device */
}

static int read_flash_id(char* idbuf, int cnt, int channel)
{
	int ret;
	memset(idbuf, 0, cnt);
	idbuf[0] = MID_RD;
	ret = do_spi_trans(1, idbuf, cnt, idbuf, cnt, NULL, 0, NULL, 0);
	return ret;
}

/*
 * Detect and initialize DataFlash device connected to specified SPI channel.
 *
 *   Device            Density         ID code                 Nr Pages        Page Size       Page offset
 *   AT45DB011B        1Mbit   (128K)  xx0011xx (0x0c)         512             264             9
 *   AT45DB021B        2Mbit   (256K)  xx0101xx (0x14)         1025            264             9
 *   AT45DB041B        4Mbit   (512K)  xx0111xx (0x1c)         2048            264             9
 *   AT45DB081B        8Mbit   (1M)    xx1001xx (0x24)         4096            264             9
 *   AT45DB0161B       16Mbit  (2M)    xx1011xx (0x2c)         4096            528             10
 *   AT45DB0321B       32Mbit  (4M)    xx1101xx (0x34)         8192            528             10
 *   AT45DB0642        64Mbit  (8M)    xx1111xx (0x3c)         8192            1056            11
 *   AT45DB1282        128Mbit (16M)   xx0100xx (0x10)         16384           1056            11
 */
static int dataflash_detect(int channel)
{
	int res = 0, isnew = 0;
	unsigned short status;
	char* idbuf = kmalloc(4, GFP_KERNEL);/* one byte command and three bytes ID info. */

	spi_access_bus(channel);
	res = read_flash_id(idbuf, 4, channel);
	if (res < 0) {
		printk("can not get dataflash id\n");
	} else {
		if (idbuf[1] == 0x1F)
			printk("ATMEL ");
		if ((idbuf[2] >> 5) == 0x1) 
			printk("DATAFLASH ");
		switch (idbuf[2] & 0x1F) {
			case 0x8:
				printk("64M bit ");
				break;
			case 0x4:
				printk("32M bit ");
				break;
			case 0x2:
				printk("16M bit ");
				break;
			case 0x1:
				printk("8M bit ");
				break;
			case 0x10:
				printk("128M bit ");
				break;
			default:
				break;
		}
		if (idbuf[3] != 0xFF) {
			printk("MLC Code %02xH, Product Version Code %02xH\n",
				(idbuf[3] >> 5), idbuf[3] & 0x1F);
			isnew = 1;
		}
	}
	kfree(idbuf);

	//spi_access_bus(channel);
	status = dataflash_status();
	spi_release_bus(channel);
#ifdef DEBUG_DATAFLASH
	printk("dataflash_detect: already read status\n");
#endif
	if (status != 0xff) {			/* no dataflash device there */
		switch (status & 0x3c) {
			case 0x0c:	/* 0 0 1 1 */
				res = add_dataflash(channel, "AT45DB011B", SZ_128K, 512, 264, 9);
				break;
			case 0x14:	/* 0 1 0 1 */
				res = add_dataflash(channel, "AT45DB021B", SZ_256K, 1025, 264, 9);
				break;
			case 0x1c:	/* 0 1 1 1 */
				res = add_dataflash(channel, "AT45DB041B", SZ_512K, 2048, 264, 9);
				break;
			case 0x24:	/* 1 0 0 1 */
				res = add_dataflash(channel, "AT45DB081B", SZ_1M, 4096, 264, 9);
				break;
			case 0x2c:	/* 1 0 1 1 */
				res = add_dataflash(channel, "AT45DB161B", SZ_2M, 4096, 528, 10);
				break;
			case 0x34:	/* 1 1 0 1 */
				res = add_dataflash(channel, "AT45DB321B", SZ_4M, 8192, 528, 10);
				break;
			case 0x3c:	/* 1 1 1 1 */
				if (isnew) {
					res = add_dataflash(channel, "AT45DB642D", SZ_8M, 8192, 1056, 11);
				} else {
					res = add_dataflash(channel, "AT45DB642", SZ_8M, 8192, 1056, 11);
				}
				break;
// Currently unsupported since Atmel removed the "Main Memory Program via Buffer" commands.
//			case 0x10:	/* 0 1 0 0 */
//				res = add_dataflash(channel, "AT45DB1282", SZ_16M, 16384, 1056, 11);
//				break;
			default:
				printk(KERN_ERR "at91_dataflash: Unknown device (%x)\n", status & 0x3c);
		}
		/* add at45db642D chip, TSOP28 */
		if (status & 0x1) {
			panic("WARNING: DATAFLASH is set Binary Page Size\n");
		}
	} else {
		printk(KERN_ERR "spi %d: no such device\n", channel);		// 没有此设备返回错误
		res = -ENODEV;
	}

	return res;
}

static int dataflash_open(struct inode *inode, struct file *filp)
{
	unsigned int dev = MINOR(inode->i_rdev);
	AT91PS_SPI spi_ctl = (AT91PS_SPI) AT91C_VA_BASE_SPI;
	int res;
#ifdef DEBUG_DATAFLASH
	printk("inode->i_rdev: %d\n", dev);
#endif
	if (dev >= NR_SPI_DEVICES)
		return -ENODEV;
	/* DataFlash (SPI chip select dev) */
	if ((res = dataflash_detect(dev)) < 0) {
		return res;
	}
	// 下面要修改CSR有关DLYBCT部分, 两次传输之间留
	// 32 * DATAFLASH_DLYBCT / at91_master_clock 时间, 工作更稳定
	if (at45_spi_baud < 2)
		at45_spi_baud = 2;
	if (at45_spi_baud > 255)
		at45_spi_baud = 255;
	switch (dev) {
		case 0:
			spi_csr[dev] = spi_ctl->SPI_CSR0;
			spi_ctl->SPI_CSR0 &= 0xFF00FF;
			spi_ctl->SPI_CSR0 |= DATAFLASH_DLYBCT << 24 | at45_spi_baud << 8;
#ifdef DEBUG_DATAFLASH
			printk("CSR%d %08x, baud %d\n", dev, spi_ctl->SPI_CSR0, at45_spi_baud);
#endif
			break;
		case 1:
			spi_csr[dev] = spi_ctl->SPI_CSR1;
			spi_ctl->SPI_CSR1 &= 0xFF00FF;
			spi_ctl->SPI_CSR1 |= DATAFLASH_DLYBCT << 24 | at45_spi_baud << 8;
#ifdef DEBUG_DATAFLASH
			printk("CSR%d %08x, baud %d\n", dev, spi_ctl->SPI_CSR1, at45_spi_baud);
#endif
			break;
		case 2:
			spi_csr[dev] = spi_ctl->SPI_CSR2;
			spi_ctl->SPI_CSR2 &= 0xFF00FF;
			spi_ctl->SPI_CSR2 |= DATAFLASH_DLYBCT << 24 | at45_spi_baud << 8;
#ifdef DEBUG_DATAFLASH
			printk("CSR%d %08x, baud %d\n", dev, spi_ctl->SPI_CSR2, at45_spi_baud);
#endif
			break;
		case 3:
			spi_csr[dev] = spi_ctl->SPI_CSR3;
			spi_ctl->SPI_CSR3 &= 0xFF00FF;
			spi_ctl->SPI_CSR3 |= DATAFLASH_DLYBCT << 24 | at45_spi_baud << 8;
#ifdef DEBUG_DATAFLASH
			printk("CSR%d %08x, baud %d\n", dev, spi_ctl->SPI_CSR3, at45_spi_baud);
#endif
			break;
		default:
			return -EINVAL;
	}
	// 此时填入私有数据
	filp->private_data = dflocal + dev;
	return 0;
}
/*
 * Close the dataflash
 */
static int dataflash_close(struct inode *inode, struct file *file)
{
	unsigned int dev = MINOR(inode->i_rdev);
	AT91PS_SPI spi_ctl = (AT91PS_SPI) AT91C_VA_BASE_SPI;
	switch (dev) {
		case 0:
			spi_ctl->SPI_CSR0 = spi_csr[dev];
			break;
		case 1:
			spi_ctl->SPI_CSR1 = spi_csr[dev];
			break;
		case 2:
			spi_ctl->SPI_CSR2 = spi_csr[dev];
			break;
		case 3:
			spi_ctl->SPI_CSR3 = spi_csr[dev];
			break;
		default:
			return -EINVAL;
	}
	nr_devices--;
	return 0;
}

struct file_operations dataflash_fops=
{
	.owner = THIS_MODULE,
	.open = dataflash_open,
	.ioctl = dataflash_ioctl,
	.write = dataflash_write,
	.read = dataflash_read,
	.release = dataflash_close,
};

static int __init dataflash_init(void)
{
	int res;
	spi_transfer_desc = kmalloc(sizeof(struct spi_transfer_list), GFP_KERNEL);
	if (!spi_transfer_desc)
		return -ENOMEM;

	dflocal = kmalloc(sizeof(struct dataflash_local)
		* DATAFLASH_MAX_DEVICES, GFP_KERNEL);
	if (dflocal == NULL)
		return -ENOMEM;

	res = register_chrdev(DATAFLASH_MAJOR, "DATAFLASH", &dataflash_fops);
	if (res < 0) {
		printk(KERN_ERR "dataflash: Unable to release major %d for SPI bus\n", DATAFLASH_MAJOR);
		return -EBUSY;
	}
	at45_spi_baud = (at91_master_clock / (2 * DEFAULT_DATAFLASH_CLK));
	printk("DATAFLASH 2.1 \n");

	return 0;
}

static void __exit dataflash_exit(void)
{
	nr_devices = 0;
	kfree(spi_transfer_desc);
	kfree(dflocal);
	unregister_chrdev(DATAFLASH_MAJOR, "DATAFLASH");
}


module_init(dataflash_init);
module_exit(dataflash_exit);
MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("DATAFLASH driver for NKTY AT91RM9200")


