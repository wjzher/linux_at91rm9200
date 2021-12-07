/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_sed1335.c
 * 摘    要：
 * 
 * 当前版本：1.1
 * 作    者：wjzhe
 * 完成日期：2007年4月3日
 *
 * 取代版本：1.0 
 * 原作者  ：wjzhe
 * 完成日期：2007年2月26日
 */
#include <linux/unistd.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm-arm/arch-at91rm9200/AT91RM9200_SYS.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <asm/delay.h>
#include "sed1335.h"

static volatile unsigned int *smc3_phy_address = NULL;
static volatile unsigned char *sed_data_addr = NULL;
static volatile unsigned char *sed_cmd_addr = NULL;

static void circle(void)
{
	int i;
	for (i = 0; i < 20; i++)
		;
}

static int setup_sed1335(void)
{
	sed_data_addr = ioremap(SED1335_DATA_ADDR, 0x01);
	if (sed_data_addr == NULL)
		return -1;
	sed_cmd_addr = ioremap(SED1335_CMD_ADDR, 0x01);
	if (sed_cmd_addr == NULL)
		return -1;
	smc3_phy_address = ioremap(SMC3_ADDR, 0x04);
	if (smc3_phy_address == NULL)
		return -1;
	*smc3_phy_address = 0x00004fc0;//4fc0
	return 0;
}

static int clear_lcd(void)
{
	int i;
	circle();
	*sed_cmd_addr = 0x4C;
	circle();
	*sed_cmd_addr = SEDCRSW;
	circle();
	*sed_data_addr = 0x00;
	circle();
	*sed_data_addr = 0x00;
	circle();
	*sed_cmd_addr = SEDMWRITE;
	for (i = 0; i < SEDSAD2; i++) {
		circle();
		*sed_data_addr = ' ';
	}
#ifdef LCDDBG
	printk("clear layer1\n");
#endif
	for (i = 0; i < (320 * 240 / 8)/*(320 * 240 / 8 + SEDSAD2)*//*0x10000*/; i++) {	
		circle();
		*sed_data_addr = 0x00;
	}
#ifdef LCDDBG
	printk("layer clear over\n");
#endif
	return 0;
}


/*
 * init LCD two layers display, and no cursor display.
 * RAM: 0000H - 2A2FH
 */
static int init_sed(void)
{
/*
 * SYSTEM SET: (fOSR = 10MHz, fFR = 85Hz)
 * (1 / 10 ^ 6) * 9 * [TC/R] * 240 = 1 / 85, so [TC/R] = 54.4 (= 36H), then TC/R = 35H
 */
	circle();
	*sed_cmd_addr = SETSEDSYS;
	circle();
	*sed_data_addr = 0x30;		// 8-line per character, single-panel drive
	circle();
	*sed_data_addr = 0x87;
	circle();
	*sed_data_addr = 0x07;		// 9-pixel character height, 07H is better, now 8-pixel character
	circle();
	*sed_data_addr = 0x27;		// 40
	circle();
	*sed_data_addr = 0x36;	// TC/R must be greater than or equal to C/R + 4
	circle();
	*sed_data_addr = 0xEF;	// Sets the height, in lines, of a frame 0xEF + 1 = 240
	circle();
	*sed_data_addr = 0x28;		// set AP, 27 + 1, two bytes
	circle();
	*sed_data_addr = 0x00;
	circle();
	*sed_cmd_addr = SEDSCROLL;
	circle();
	*sed_data_addr = 0x00;		// set sad1 0000H
	circle();
	*sed_data_addr = 0x00;
	circle();
	*sed_data_addr = 0xF0;		// 240-line
	circle();
	*sed_data_addr = 0xB0;		// set sad2 04B0H, 1200 bytes
	circle();
	*sed_data_addr = 0x04;
	circle();
	*sed_data_addr = 0xF0;		// 240-line
	circle();
	*sed_cmd_addr = SEDCSRDIR;
	
	circle();
	*sed_cmd_addr = SEDHDOT;// While the SCROLL command only allows scrolling by characters, HDOT SCR allows the screen to be scrolled horizontally by pixels.
	circle();
	*sed_data_addr = 0x00;

	circle();
	*sed_cmd_addr = SEDOVLAY;
	circle();
	*sed_data_addr = 0x01;		// Composition Method: Exclusive-OR
	circle();
	*sed_cmd_addr = SEDON;
	circle();
	*sed_data_addr = 0x14;
	//circle();
	//*sed_cmd_addr = SEDON;
	return 0;
}

static int sed1335_open(struct inode* inode, struct file* file)
{
	int ret;
	if ((ret = setup_sed1335()) < 0)
		return ret;
	init_sed();
	clear_lcd();
#ifdef LCDDBG
	printk("sed1335 opened!\n");
#endif
	return 0;
}

static int sed1335_ioctl(struct inode * inode,struct file* file, unsigned int cmd, unsigned long arg)
{
	unsigned short csraddr;
	//printk("cmd is: %04x\n", cmd);
	switch (cmd) {
	case INITSED:		// init LCD
		init_sed();
		break;
	case CLEARSED:		// clear LCD
		clear_lcd();
		break;
	case SETCURSOR:		// set cursor write addr
		circle();
		*sed_cmd_addr = SEDCRSW;
		circle();
		*sed_data_addr = arg & 0xff;
		circle();
		*sed_data_addr = (arg >> 8) & 0xff;
		break;
	case RDCURSOR:
		circle();
		*sed_cmd_addr = SEDCRSR;
		circle();
		csraddr = *sed_data_addr;
		circle();
		csraddr |= *sed_data_addr << 8;
		if (put_user(csraddr, (unsigned short *)arg) < 0)
			return -1;
		break;
	/*
	 * CURSORDIR:
	 * 	LEFT: 	4CH
	 * 	RIGHT: 	4DH
	 *	UP:		4EH
	 *	DOWN:	4FH
	 */
	case SETCURSORLEFT:		// cursor move left
		circle();
		//printk("sed SETCURSORLEFT\n");
		*sed_cmd_addr = 0x4C;
		break;
	case SETCURSORRIGHT:		// cursor move down
		circle();
		*sed_cmd_addr = 0x4D;
		break;
	case SETCURSORUP:
		circle();
		*sed_cmd_addr = 0x4E;
		break;
	case SETCURSORDOWN:
		circle();
		*sed_cmd_addr = 0x4F;
		break;
	case SETOVLAY:
		circle();
		*sed_cmd_addr = arg & 0xFF;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static ssize_t sed1335_write(struct file *file, char *buf, size_t count, loff_t *offp)
{
	int i, ret;
	char tmp;
	*sed_cmd_addr = SEDMWRITE;
	for (i = 0; i < count; i++) {
		if ((ret = get_user(tmp, buf)) < 0)
			return ret;
		circle();
		*sed_data_addr = tmp;
		buf++;
	}
	return count;
}

static ssize_t sed1335_read(struct file *file, char *buf, size_t count, loff_t *offp)
{
	int i, ret;
	char tmp;
	*sed_cmd_addr = SEDMREAD;
	for (i = 0; i < count; i++) {
		circle();
		tmp = *sed_data_addr;
		if ((ret = put_user(tmp, buf)) < 0)
			return ret;
		buf++;
	}
	return count;
}

struct file_operations sed1335_fops=
{
	.owner = THIS_MODULE,
	.open = sed1335_open,
	.ioctl = sed1335_ioctl,
	.write = sed1335_write,
	.read = sed1335_read,
};

int sed1335_init(void)
{
	int ret;
	ret = register_chrdev(SED1335_MAJOR,"sed1335",&sed1335_fops);
	if (ret < 0) {
		printk("failed:sed1335\n");
		return -EBUSY;
	}
	printk("LCD(sed1335) version 1.1!\n");
	return 0;
}

static void sed1335_exit(void)
{
	printk("unregistering sed1335\n");
	unregister_chrdev(SED1335_MAJOR, "sed1335");
}

module_init(sed1335_init);
module_exit(sed1335_exit);
MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("LCD(SED1335) driver for NKTY AT91RM9200")

