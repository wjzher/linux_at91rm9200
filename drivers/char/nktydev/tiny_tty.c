/*
 * Tiny TTY driver
 *
 * Copyright (C) 2002-2004 Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2 of the License.
 *
 * This driver shows how to create a minimal tty driver.  It does not rely on
 * any backing hardware, but creates a timer that emulates data being received
 * from some kind of hardware.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "sed1335.h"
#include <linux/serial_core.h>
#include <linux/console.h>

#define DRIVER_VERSION "v2.0"
#define DRIVER_AUTHOR "Greg Kroah-Hartman <greg@kroah.com>"
#define DRIVER_DESC "Tiny TTY driver"

/* Module information */
MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#define DELAY_TIME		HZ * 2	/* 2 seconds per character */
#define TINY_DATA_CHARACTER	't'

#define TINY_TTY_MAJOR		240	/* experimental range */
#define TINY_TTY_MINORS		4	/* only have 4 devices */

struct lcdinfo{
	const int num;
	const int row;
	const int column;
	volatile char *data_addr;
	volatile char *cmd_addr;
	char data[1200];
	int flag;		/* one page all? */
	int index;		/* below 1200 */
	int line;		/* below 30 */
} ;
static struct lcdinfo lcd_info = {
	.num = 1200,
	.row = 40,
	.column = 30,
};
struct tiny_serial {
	struct tty_struct	*tty;		/* pointer to the tty for this device */
	int			open_count;	/* number of times this port has been opened */
	struct semaphore	sem;		/* locks this structure */
	struct timer_list	*timer;

	/* for tiocmget and tiocmset functions */
	int			msr;		/* MSR shadow */
	int			mcr;		/* MCR shadow */

	/* for ioctl fun */
	struct serial_struct	serial;
	wait_queue_head_t	wait;
	struct async_icount	icount;
};

struct tiny_serial *tiny_table[TINY_TTY_MINORS];	/* initially all NULL */

static int lcd_map(void)
{
	lcd_info.data_addr = ioremap(SED1335_DATA_ADDR, 0x01);
	if (lcd_info.data_addr == NULL)
		return -1;
	lcd_info.cmd_addr = ioremap(SED1335_CMD_ADDR, 0x01);
	if (lcd_info.cmd_addr == NULL)
		return -1;
	return 0;
}
static void circle(void)
{
	int i;
	for (i = 0; i < 20; i++)
		;
}

/*
 * init LCD two layers display, and no cursor display.
 * RAM: 0000H - 2A2FH
 */
static int init_sed(struct lcdinfo *lcd_info)
{
/*
 * SYSTEM SET: (fOSR = 10MHz, fFR = 85Hz)
 * (1 / 10 ^ 6) * 9 * [TC/R] * 240 = 1 / 85, so [TC/R] = 54.4 (= 36H), then TC/R = 35H
 */
	writeb(SETSEDSYS, lcd_info->cmd_addr);
	circle();
	writeb(0x30, lcd_info->data_addr);
	circle();
	writeb(0x87, lcd_info->data_addr);
	circle();
	writeb(0x07, lcd_info->data_addr);
	circle();
	writeb(0x27, lcd_info->data_addr);
	circle();
	writeb(0x36, lcd_info->data_addr);
	circle();
	writeb(0xEF, lcd_info->data_addr);
	circle();
	writeb(0x28, lcd_info->data_addr);
	circle();
	writeb(0, lcd_info->data_addr);
	circle();
	writeb(SEDSCROLL, lcd_info->cmd_addr);
	circle();
	writeb(0, lcd_info->data_addr);
	circle();
	writeb(0, lcd_info->data_addr);
	circle();
	writeb(0xF0, lcd_info->data_addr);
	circle();
	writeb(0xB0, lcd_info->data_addr);
	circle();
	writeb(0x04, lcd_info->data_addr);
	circle();
	writeb(0xF0, lcd_info->data_addr);
	circle();
	writeb(SEDCSRDIR, lcd_info->cmd_addr);
	circle();
	writeb(SEDHDOT, lcd_info->cmd_addr);
	circle();
	writeb(0, lcd_info->data_addr);
	circle();
	writeb(SEDOVLAY, lcd_info->cmd_addr);
	circle();
	writeb(1, lcd_info->data_addr);
	circle();
	writeb(SEDON, lcd_info->cmd_addr);
	circle();
	writeb(0x14, lcd_info->data_addr);
	circle();
	return 0;
}

static void tiny_timer(unsigned long timer_data)
{
	struct tiny_serial *tiny = (struct tiny_serial *)timer_data;
	struct tty_struct *tty;
	int i;
	char data[1] = {TINY_DATA_CHARACTER};
	int data_size = 1;

	if (!tiny)
		return;

	tty = tiny->tty;

	/* send the data to the tty layer for users to read.  This doesn't
	 * actually push the data through unless tty->low_latency is set */
	for (i = 0; i < data_size; ++i) {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			tty_flip_buffer_push(tty);
		tty_insert_flip_char(tty, data[i], TTY_NORMAL);
	}
	tty_flip_buffer_push(tty);

	/* resubmit the timer again */
	tiny->timer->expires = jiffies + DELAY_TIME;
	add_timer(tiny->timer);
}

static int tiny_open(struct tty_struct *tty, struct file *file)
{
	struct tiny_serial *tiny;
	struct timer_list *timer;
	int index;

	/* initialize the pointer in case something fails */
	tty->driver_data = NULL;

	/* get the serial object associated with this tty pointer */
	index = tty->index;
	tiny = tiny_table[index];
	if (tiny == NULL) {
		/* first time accessing this device, let's create it */
		tiny = kmalloc(sizeof(*tiny), GFP_KERNEL);
		if (!tiny)
			return -ENOMEM;

		init_MUTEX(&tiny->sem);
		tiny->open_count = 0;
		tiny->timer = NULL;

		tiny_table[index] = tiny;
	}

	down(&tiny->sem);

	/* save our structure within the tty structure */
	tty->driver_data = tiny;
	tiny->tty = tty;

	++tiny->open_count;
	if (tiny->open_count == 1) {
		/* this is the first time this port is opened */
		/* do any hardware initialization needed here */
		tiny->timer = NULL
		/* create our timer and submit it 
		if (!tiny->timer) {
			timer = kmalloc(sizeof(*timer), GFP_KERNEL);
			if (!timer) {
				up(&tiny->sem);
				return -ENOMEM;
			}
			tiny->timer = timer;
		}
		tiny->timer->data = (unsigned long )tiny;
		tiny->timer->expires = jiffies + DELAY_TIME;
		tiny->timer->function = tiny_timer;
		add_timer(tiny->timer)*/;
	}

	up(&tiny->sem);
	return 0;
}

static void do_close(struct tiny_serial *tiny)
{
	down(&tiny->sem);

	if (!tiny->open_count) {
		/* port was never opened */
		goto exit;
	}

	--tiny->open_count;
	if (tiny->open_count <= 0) {
		/* The port is being closed by the last user. */
		/* Do any hardware specific stuff here */

		/* shut down our timer 
		del_timer(tiny->timer);*/
	}
exit:
	up(&tiny->sem);
}

static void tiny_close(struct tty_struct *tty, struct file *file)
{
	struct tiny_serial *tiny = tty->driver_data;

	if (tiny)
		do_close(tiny);
}	

#if 1
static int move_data(struct lcdinfo *info)
{
	char *tmp;
	// 已经到达最后一行, 需要特殊处理
	tmp = kmalloc(info->num, GFP_KERNEL);
	if (tmp == NULL) {
		return -ENOMEM;
	}
	memcpy(tmp, &info->data[info->row], info->num - info->row);
	memcpy(info->data, tmp, info->num - info->row);
	info->index = info->num - info->row;
	kfree(tmp);
	return 0;
}
#endif
static void set_lcd_addr(struct lcdinfo *info, int addr)
{
	circle();
	*info->cmd_addr = SEDCRSW;
	circle();
	*info->data_addr = addr & 0xFF;
	circle();
	*info->data_addr = addr >> 8;
}
static void set_lcd_sad1(struct lcdinfo *info, int addr)
{
	circle();
	*info->cmd_addr = SEDSCROLL;
	circle();
	*info->data_addr = addr & 0xFF;
	circle();
	*info->data_addr = addr >> 8;
	circle();
	*info->data_addr = 0xF0 - addr / info->row;
#if 0
	circle();
	*info->data_addr = 0x04;
	circle();
	*info->data_addr = 0xB0;
	circle();
	*info->data_addr = 0xF0;
	circle();
	writeb(0xF0, lcd_info->data_addr);
	circle();
	writeb(0xB0, lcd_info->data_addr);
	circle();
	writeb(0x04, lcd_info->data_addr);
	circle();
	writeb(0xF0, lcd_info->data_addr);
	circle();
#endif
}
// 应该采用自动scroll命令来输出'\n'
static int write_lcd(struct lcdinfo *info, char *buf, int cnt)
{
	int i, ret;
	int num = info->index + cnt;
	for (i = 0; i < cnt; i++, buf++) {
		if (*buf == '\r')
			continue;
		if (*buf == '\n') {
#if 0
			int j;
			// line add 1
			info->line++;
			if ((info->line < info->column) && (!info->flag)) {
				// move cursor
				info->index = info->line * info->row;
				set_lcd_addr(info, info->index);
				continue;
			} else {
				info->flag = 1;
			}
			if (info->line >= info->column)
				info->line -= info->column;
			info->index = info->line * info->row;
			// clear line
			set_lcd_addr(info, info->index);
			circle();
			*info->cmd_addr = SEDMWRITE;
			for (j = 0; j < info->row; j++) {
				circle();
				*info->data_addr = ' ';
			}
			circle();
			set_lcd_addr(info, info->index);
			// set sad1
			set_lcd_sad1(info, (info->line + 1) * info->row);
#else
			if (info->line < (info->column - 1)) {
				// just move index, and continue
				info->index = (info->line + 1) * info->row;
				info->line += 1;
				continue;
			}
			// copydata
			if ((ret = move_data(info)) < 0)
				return ret;
			memset(&info->data[(info->column - 1) * info->row], ' ', info->row);
			info->index = (info->column - 1) * info->row;
			info->line = info->column - 1;
#endif
			continue;
		}
		if (info->index >= (info->num - 1)) {
#if 0
			int j;
			// 到头了
			// clear line
			*info->cmd_addr = SEDMWRITE;
			for (j = 0; j < info->row; j++) {
				circle();
				*info->data_addr = ' ';
			}
			// set sad1
			set_lcd_sad1(info, (info->line + 1) % info->column);
			set_lcd_addr(info, info->index);
#else
			if ((ret = move_data(info)) < 0)
				return ret;
			memset(&info->data[(info->column - 1) * info->row], ' ', info->row);
			info->index = (info->column - 1) * info->row;
			info->line = info->column - 1;
#endif
		}
#if 1
		info->data[info->index] = *buf;
#else
		set_lcd_addr(info, info->index);
		*info->cmd_addr = SEDMWRITE;
		*info->data_addr = *buf;
	if (pre >= info->num) {
		pre = info->num;
	}
#endif
		info->index++;
		info->line = info->index / info->row;
	}
#if 1
	// write lcd
	if (num >= info->num)
		num = info->num;
	circle();
	*info->cmd_addr = 0x4C;
	circle();
	*info->cmd_addr = SEDCRSW;
	circle();
	*info->data_addr = 0;
	circle();
	*info->data_addr = 0;
	circle();
	*info->cmd_addr = SEDMWRITE;
	for (i = 0; i < num; i++) {
		circle();
		*info->data_addr = info->data[i];
	}
#endif
	return cnt;
}

static int tiny_write(struct tty_struct *tty, 
		      const unsigned char *buffer, int count)
{
	struct tiny_serial *tiny = tty->driver_data;
	int i;
	int retval = -EINVAL;

	//printk(KERN_DEBUG "enter %s - ", __FUNCTION__);
	if (!tiny)
		return -ENODEV;

	down(&tiny->sem);

	if (!tiny->open_count)
		/* port was not opened */
		goto exit;

	/* fake sending the data out a hardware port by
	 * writing it to the kernel debug log.
	 */
	printk(KERN_DEBUG "%s - ", __FUNCTION__);
	for (i = 0; i < count; ++i)
		printk("%02x ", buffer[i]);
	printk("\n");
	/*
	 * 将要输出的东西写入LCD
	 * write new data to lcd buffer, and write lcd
	 */
	retval = write_lcd(&lcd_info, buffer, count);
exit:
	up(&tiny->sem);
	return retval;
}

static int tiny_write_room(struct tty_struct *tty) 
{
	struct tiny_serial *tiny = tty->driver_data;
	int room = -EINVAL;

	if (!tiny)
		return -ENODEV;

	down(&tiny->sem);
	
	if (!tiny->open_count) {
		/* port was not opened */
		goto exit;
	}

	/* calculate how much room is left in the device */
	room = 40;

exit:
	up(&tiny->sem);
	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static void tiny_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	unsigned int cflag;

	cflag = tty->termios->c_cflag;

	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(tty->termios->c_iflag) == 
		     RELEVANT_IFLAG(old_termios->c_iflag))) {
			printk(KERN_DEBUG " - nothing to change...\n");
			return;
		}
	}

	/* get the byte size */
	switch (cflag & CSIZE) {
		case CS5:
			printk(KERN_DEBUG " - data bits = 5\n");
			break;
		case CS6:
			printk(KERN_DEBUG " - data bits = 6\n");
			break;
		case CS7:
			printk(KERN_DEBUG " - data bits = 7\n");
			break;
		default:
		case CS8:
			printk(KERN_DEBUG " - data bits = 8\n");
			break;
	}
	
	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			printk(KERN_DEBUG " - parity = odd\n");
		else
			printk(KERN_DEBUG " - parity = even\n");
	else
		printk(KERN_DEBUG " - parity = none\n");

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		printk(KERN_DEBUG " - stop bits = 2\n");
	else
		printk(KERN_DEBUG " - stop bits = 1\n");

	/* figure out the hardware flow control settings */
	if (cflag & CRTSCTS)
		printk(KERN_DEBUG " - RTS/CTS is enabled\n");
	else
		printk(KERN_DEBUG " - RTS/CTS is disabled\n");
	
	/* determine software flow control */
	/* if we are implementing XON/XOFF, set the start and 
	 * stop character in the device */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty))
			printk(KERN_DEBUG " - INBOUND XON/XOFF is enabled, "
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			printk(KERN_DEBUG" - INBOUND XON/XOFF is disabled");

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty))
			printk(KERN_DEBUG" - OUTBOUND XON/XOFF is enabled, "
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			printk(KERN_DEBUG" - OUTBOUND XON/XOFF is disabled");
	}

	/* get the baud rate wanted */
	printk(KERN_DEBUG " - baud rate = %d", tty_get_baud_rate(tty));
}

/* Our fake UART values */
#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_LOOP	0x04
#define MSR_CTS		0x08
#define MSR_CD		0x10
#define MSR_RI		0x20
#define MSR_DSR		0x40

static int tiny_tiocmget(struct tty_struct *tty, struct file *file)
{
	struct tiny_serial *tiny = tty->driver_data;

	unsigned int result = 0;
	unsigned int msr = tiny->msr;
	unsigned int mcr = tiny->mcr;

	result = ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |	/* DTR is set */
             ((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |	/* RTS is set */
             ((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |	/* LOOP is set */
             ((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |	/* CTS is set */
             ((msr & MSR_CD)   ? TIOCM_CAR  : 0) |	/* Carrier detect is set*/
             ((msr & MSR_RI)   ? TIOCM_RI   : 0) |	/* Ring Indicator is set */
             ((msr & MSR_DSR)  ? TIOCM_DSR  : 0);	/* DSR is set */

	return result;
}

static int tiny_tiocmset(struct tty_struct *tty, struct file *file,
                         unsigned int set, unsigned int clear)
{
	struct tiny_serial *tiny = tty->driver_data;
	unsigned int mcr = tiny->mcr;

	if (set & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		mcr |= MCR_RTS;

	if (clear & TIOCM_RTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		mcr &= ~MCR_RTS;

	/* set the new MCR value in the device */
	tiny->mcr = mcr;
	return 0;
}

static int tiny_read_proc(char *page, char **start, off_t off, int count,
                          int *eof, void *data)
{
	struct tiny_serial *tiny;
	off_t begin = 0;
	int length = 0;
	int i;

	length += sprintf(page, "tinyserinfo:1.0 driver:%s\n", DRIVER_VERSION);
	for (i = 0; i < TINY_TTY_MINORS && length < PAGE_SIZE; ++i) {
		tiny = tiny_table[i];
		if (tiny == NULL)
			continue;

		length += sprintf(page+length, "%d\n", i);
		if ((length + begin) > (off + count))
			goto done;
		if ((length + begin) < off) {
			begin += length;
			length = 0;
		}
	}
	*eof = 1;
done:
	if (off >= (length + begin))
		return 0;
	*start = page + (off-begin);
	return (count < begin+length-off) ? count : begin + length-off;
}

#define tiny_ioctl tiny_ioctl_tiocgserial
static int tiny_ioctl(struct tty_struct *tty, struct file *file,
                      unsigned int cmd, unsigned long arg)
{
	struct tiny_serial *tiny = tty->driver_data;

	if (cmd == TIOCGSERIAL) {
		struct serial_struct tmp;

		if (!arg)
			return -EFAULT;

		memset(&tmp, 0, sizeof(tmp));

		tmp.type		= tiny->serial.type;
		tmp.line		= tiny->serial.line;
		tmp.port		= tiny->serial.port;
		tmp.irq			= tiny->serial.irq;
		tmp.flags		= ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
		tmp.xmit_fifo_size	= tiny->serial.xmit_fifo_size;
		tmp.baud_base		= tiny->serial.baud_base;
		tmp.close_delay		= 5*HZ;
		tmp.closing_wait	= 30*HZ;
		tmp.custom_divisor	= tiny->serial.custom_divisor;
		tmp.hub6		= tiny->serial.hub6;
		tmp.io_type		= tiny->serial.io_type;

		if (copy_to_user((void __user *)arg, &tmp, sizeof(struct serial_struct)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}
#undef tiny_ioctl

#define tiny_ioctl tiny_ioctl_tiocmiwait
static int tiny_ioctl(struct tty_struct *tty, struct file *file,
                      unsigned int cmd, unsigned long arg)
{
	struct tiny_serial *tiny = tty->driver_data;

	if (cmd == TIOCMIWAIT) {
		DECLARE_WAITQUEUE(wait, current);
		struct async_icount cnow;
		struct async_icount cprev;

		cprev = tiny->icount;
		while (1) {
			add_wait_queue(&tiny->wait, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			remove_wait_queue(&tiny->wait, &wait);

			/* see if a signal woke us up */
			if (signal_pending(current))
				return -ERESTARTSYS;

			cnow = tiny->icount;
			if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
			    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
				return -EIO; /* no change => error */
			if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
			    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
			    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
			    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
				return 0;
			}
			cprev = cnow;
		}

	}
	return -ENOIOCTLCMD;
}
#undef tiny_ioctl

#define tiny_ioctl tiny_ioctl_tiocgicount
static int tiny_ioctl(struct tty_struct *tty, struct file *file,
                      unsigned int cmd, unsigned long arg)
{
	struct tiny_serial *tiny = tty->driver_data;

	if (cmd == TIOCGICOUNT) {
		struct async_icount cnow = tiny->icount;
		struct serial_icounter_struct icount;

		icount.cts	= cnow.cts;
		icount.dsr	= cnow.dsr;
		icount.rng	= cnow.rng;
		icount.dcd	= cnow.dcd;
		icount.rx	= cnow.rx;
		icount.tx	= cnow.tx;
		icount.frame	= cnow.frame;
		icount.overrun	= cnow.overrun;
		icount.parity	= cnow.parity;
		icount.brk	= cnow.brk;
		icount.buf_overrun = cnow.buf_overrun;

		if (copy_to_user((void __user *)arg, &icount, sizeof(icount)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}
#undef tiny_ioctl

/* the real tiny_ioctl function.  The above is done to get the small functions in the book */
static int tiny_ioctl(struct tty_struct *tty, struct file *file,
                      unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case TIOCGSERIAL:
		return tiny_ioctl_tiocgserial(tty, file, cmd, arg);
	case TIOCMIWAIT:
		return tiny_ioctl_tiocmiwait(tty, file, cmd, arg);
	case TIOCGICOUNT:
		return tiny_ioctl_tiocgicount(tty, file, cmd, arg);
	}

	return -ENOIOCTLCMD;
}

static struct tty_operations serial_ops = {
	.open = tiny_open,
	.close = tiny_close,
	.write = tiny_write,
	.write_room = tiny_write_room,
	.set_termios = tiny_set_termios,
};

static struct tty_driver *tiny_tty_driver;
static int clear_lcd(struct lcdinfo *info)
{
	int i;
	circle();
	*info->cmd_addr = 0x4C;
	circle();
	*info->cmd_addr = SEDCRSW;
	circle();
	*info->data_addr = 0x00;
	circle();
	*info->data_addr = 0x00;
	circle();
	*info->cmd_addr = SEDMWRITE;
	for (i = 0; i < SEDSAD2; i++) {
		circle();
		*info->data_addr = ' ';
	}
#ifdef LCDDBG
	printk("clear layer1\n");
#endif
	for (i = 0; i < (320 * 240 / 8)/*(320 * 240 / 8 + SEDSAD2)*//*0x10000*/; i++) {	
		circle();
		*info->data_addr = 0x00;
	}
#ifdef LCDDBG
	printk("layer clear over\n");
#endif
	return 0;
}

static int __init tiny_init(void)
{
	int retval;
	int i;

	/* allocate the tty driver */
	tiny_tty_driver = alloc_tty_driver(TINY_TTY_MINORS);
	if (!tiny_tty_driver)
		return -ENOMEM;

	/* initialize the tty driver */
	tiny_tty_driver->owner = THIS_MODULE;
	tiny_tty_driver->driver_name = "tiny_tty";
	tiny_tty_driver->name = "ttty";
	tiny_tty_driver->devfs_name = "ttty";
	tiny_tty_driver->major = 4;
	tiny_tty_driver->minor_start = 72;
	tiny_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	tiny_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	tiny_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	tiny_tty_driver->init_termios = tty_std_termios;
	tiny_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_set_operations(tiny_tty_driver, &serial_ops);// set operations

	/* hack to make the book purty, yet still use these functions in the
	 * real driver.  They really should be set up in the serial_ops
	 * structure above... */
	tiny_tty_driver->read_proc = tiny_read_proc;
	tiny_tty_driver->tiocmget = tiny_tiocmget;
	tiny_tty_driver->tiocmset = tiny_tiocmset;
	tiny_tty_driver->ioctl = tiny_ioctl;

	/* register the tty driver */
	retval = tty_register_driver(tiny_tty_driver);// 注册驱动
	if (retval) {
		printk(KERN_ERR "failed to register tiny tty driver");
		put_tty_driver(tiny_tty_driver);
		return retval;
	}

	// 此时要做一些LCD初使化工作
	//AT91_SYS->EBI_SMC2_CSR[3] = 0x00004fc0;//4fc0
	if (lcd_map() < 0) {
		printk("at91 lcd map failed\n");
		return -1;
	}
	init_sed(&lcd_info);
	clear_lcd(&lcd_info);
	lcd_info.index = 0;
	lcd_info.flag = 0;
	lcd_info.line = 0;
	for (i = 0; i < TINY_TTY_MINORS; ++i)
		tty_register_device(tiny_tty_driver, i, NULL);// 注册设备

	printk(KERN_INFO DRIVER_DESC " " DRIVER_VERSION "\n");
	return retval;
}

static void __exit tiny_exit(void)
{
	struct tiny_serial *tiny;
	int i;

	for (i = 0; i < TINY_TTY_MINORS; ++i)
		tty_unregister_device(tiny_tty_driver, i);
	tty_unregister_driver(tiny_tty_driver);

	/* shut down all of the timers and free the memory */
	for (i = 0; i < TINY_TTY_MINORS; ++i) {
		tiny = tiny_table[i];
		if (tiny) {
			/* close the port */
			while (tiny->open_count)
				do_close(tiny);

			/* shut down our timer and free the memory */
			del_timer(tiny->timer);
			kfree(tiny->timer);
			kfree(tiny);
			tiny_table[i] = NULL;
		}
	}
}

#if 1
/*
 * Interrupts are disabled on entering
 */
static void at91_console_write(struct console *co, const char *s, u_int count)
{
	tiny_write(*tiny_tty_driver->ttys, s, count);
}

/*
 * If the port was already initialised (eg, by a boot loader), try to determine
 * the current setup.
 */

static int __init at91_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	return 0;
}
static struct tty_driver *at91_console_device(struct console *co, int *index)
{
	*index = co->index;
	return tiny_tty_driver;
}

static struct console at91_console = {
	.name		= "ttty",
	.write		= at91_console_write,
	.device		= at91_console_device,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

#define AT91_CONSOLE_DEVICE	&at91_console

static int  __init at91_console_init(void)
{
	at91_console.index = 0;
	register_console(&at91_console);
	return 0;
}
console_initcall(at91_console_init);

#else
#define AT91_CONSOLE_DEVICE	NULL
#endif



#if 0

/*
 * Interrupts are disabled on entering
 */
static void lcd_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = at91_ports + co->index;
	unsigned int status, i, imr;

	/*
	 *	First, save IMR and then disable interrupts
	 */
	imr = UART_GET_IMR(port);	/* get interrupt mask */
	UART_PUT_IDR(port, AT91C_US_RXRDY | AT91C_US_TXRDY);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++) {
		do {
			status = UART_GET_CSR(port);
		} while (!(status & AT91C_US_TXRDY));
		UART_PUT_CHAR(port, s[i]);
		if (s[i] == '\n') {
			do {
				status = UART_GET_CSR(port);
			} while (!(status & AT91C_US_TXRDY));
			UART_PUT_CHAR(port, '\r');
		}
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore IMR
	 */
	do {
		status = UART_GET_CSR(port);
	} while (!(status & AT91C_US_TXRDY));
	UART_PUT_IER(port, imr);	/* set interrupts back the way they were */
}

/*
 * If the port was already initialised (eg, by a boot loader), try to determine
 * the current setup.
 */
static void __init lcd_console_get_options(struct uart_port *port, int *baud, int *parity, int *bits)
{
	unsigned int mr, quot;

// TODO: CR is a write-only register
//	unsigned int cr;
//
//	cr = UART_GET_CR(port) & (AT91C_US_RXEN | AT91C_US_TXEN);
//	if (cr == (AT91C_US_RXEN | AT91C_US_TXEN)) {
//		/* ok, the port was enabled */
//	}

	mr = UART_GET_MR(port) & AT91C_US_CHRL;
	if (mr == AT91C_US_CHRL_8_BITS)
		*bits = 8;
	else
		*bits = 7;

	mr = UART_GET_MR(port) & AT91C_US_PAR;
	if (mr == AT91C_US_PAR_EVEN)
		*parity = 'e';
	else if (mr == AT91C_US_PAR_ODD)
		*parity = 'o';

	quot = UART_GET_BRGR(port);
	*baud = port->uartclk / (16 * (quot));
}

static int __init lcd_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	port = uart_get_console(at91_ports, AT91C_NR_UART, co);

	/*
	 * Enable the serial console, in-case bootloader did not do it.
	 */
	AT91_SYS->PMC_PCER = 1 << port->irq;		/* enable clock */
	UART_PUT_IDR(port, -1);				/* disable interrupts */
	UART_PUT_CR(port, AT91C_US_RSTSTA | AT91C_US_RSTRX);
	UART_PUT_CR(port, AT91C_US_TXEN | AT91C_US_RXEN);
	
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		at91_console_get_options(port, &baud, &parity, &bits);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

extern struct uart_driver at91_uart;

static struct console lcd_console = {
	.name		= "ttyL",
	.write		= lcd_console_write,
	.device		= lcd_console_device,
	.setup		= lcd_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &at91_uart,
};

#define AT91_CONSOLE_DEVICE	&at91_console

static int  __init lcd_console_init(void)
{
	at91_init_ports();

	at91_console.index = at91_console_port;
	register_console(&lcd_console);
	return 0;
}
console_initcall(at91_console_init);

#endif


module_init(tiny_init);
module_exit(tiny_exit);
