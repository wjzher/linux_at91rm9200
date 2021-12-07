#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pio.h>
#include <linux/version.h>
#include <asm/hardware.h>
#include <asm/delay.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>

#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include "gpiob.h"

#define     BUTTON_IRQ1 IRQ_EINT16
#define     BUTTON_IRQ2 IRQ_EINT17
#define DEVICE_NAME "keyin"
static int key_major=0;
#define     BUTTONMINOR 0
#define     KEY_BUF  16
    
#define BUTTONSTATUS_1      16
#define BUTTONSTATUS_2      17
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

static int at91_halt = 0;
static unsigned char key_read(void);
static int flag=0;

struct keyin {
    unsigned int buttonStatus;      //按键状态
    unsigned char buf[KEY_BUF]; //按键缓冲区
    unsigned int head,tail;         //按键缓冲区头和尾
    wait_queue_head_t wq;           //等待队列
};

static struct keyin keydev;
extern struct tiny_serial *tiny_table[4];	/* initially all NULL */

#define BUF_HEAD    (keydev.buf[keydev.head])     //缓冲区头
#define BUF_TAIL    (keydev.buf[keydev.tail])     //缓冲区尾
#define INCBUF(x,mod)   ((++(x)) & ((mod)-1))       //移动缓冲区指针

#define PB_DEBUG
// 对于2.6.12以后版本的中断函数
static irqreturn_t GPIOB0_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag = 0, key;
	struct tiny_serial *tiny = tiny_table[0];
	key_flag = at91_get_gpio_value(AT91_PIN_PB0);
	if (key_flag) {
#ifdef PB_DEBUG
		printk("up GPIOB0\n");
		printk("GPIOB0 return\n");
#endif
		return IRQ_HANDLED;
	} else {
#ifdef PB_DEBUG
		printk("down GPIOB0\n");
#endif
	}
	// 记录当前时间
	current_jif = jiffies;
	key = key_flag = KEYUP;
	if (key == BUF_HEAD) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	// 记录KEY时钟滴答
	key_jif = current_jif;
	BUF_HEAD = KEYUP;
    keydev.head = INCBUF(keydev.head, KEY_BUF);
    flag = 1;
    wake_up_interruptible(&(keydev.wq));
	/*
	 * 有键按下, 需要做处理
	 */
	if (tiny->tty) {
		tty_insert_flip_char(tiny->tty, '<', 0);
	}
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
	return IRQ_HANDLED;
}
static irqreturn_t GPIOB1_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag = 0, key;
	struct tiny_serial *tiny = tiny_table[0];
	key_flag = at91_get_gpio_value(AT91_PIN_PB1);
	if (key_flag) {
#ifdef PB_DEBUG
		printk("up GPIOB1\n");
		printk("GPIOB1 return\n");
#endif
		return IRQ_HANDLED;
	} else {
#ifdef PB_DEBUG
		printk("down GPIOB1\n");
#endif
	}
	// 记录当前时间
	current_jif = jiffies;
	key = key_flag = KEYLEFT;
	if (key == BUF_HEAD) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	// 记录KEY时钟滴答
	key_jif = current_jif;
	BUF_HEAD = KEYLEFT;
    keydev.head = INCBUF(keydev.head, KEY_BUF);
    flag = 1;
    wake_up_interruptible(&(keydev.wq));
	/*
	 * 有键按下, 需要做处理
	 */
	if (tiny->tty) {
		tty_insert_flip_char(tiny->tty, 'i', 0);
	}
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
	return IRQ_HANDLED;
}
static irqreturn_t GPIOB2_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	struct tiny_serial *tiny = tiny_table[0];
	unsigned int key_flag = 0, key;
	key_flag = at91_get_gpio_value(AT91_PIN_PB2);
	if (key_flag) {
#ifdef PB_DEBUG
		printk("up GPIOB2\n");
		printk("GPIOB2 return\n");
#endif
		return IRQ_HANDLED;
	} else {
#ifdef PB_DEBUG
		printk("down GPIOB2\n");
#endif
	}
	// 记录当前时间
	current_jif = jiffies;
	key = key_flag = KEYOK;
	if (key == BUF_HEAD) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	// 记录KEY时钟滴答
	key_jif = current_jif;
	BUF_HEAD = KEYOK;
    keydev.head = INCBUF(keydev.head, KEY_BUF);
    flag = 1;
    wake_up_interruptible(&(keydev.wq));
	/*
	 * 有键按下, 需要做处理
	 */
	if (tiny->tty) {
		tty_insert_flip_char(tiny->tty, '\n', 0);
	}
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
	return IRQ_HANDLED;
}
static irqreturn_t GPIOB3_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	struct tiny_serial *tiny = tiny_table[0];
	unsigned int key_flag = 0, key;
	key_flag = at91_get_gpio_value(AT91_PIN_PB3);
	if (key_flag) {
#ifdef PB_DEBUG
		printk("up GPIOB3\n");
		printk("GPIOB3 return\n");
#endif
		return IRQ_HANDLED;
	} else {
#ifdef PB_DEBUG
		printk("down GPIOB3\n");
#endif
	}
	// 记录当前时间
	current_jif = jiffies;
	key = key_flag = KEYRIGHT;
	if (key == BUF_TAIL) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	// 记录KEY时钟滴答
	key_jif = current_jif;
	BUF_HEAD = KEYRIGHT;
    keydev.head = INCBUF(keydev.head, KEY_BUF);
    flag = 1;
    wake_up_interruptible(&(keydev.wq));
	/*
	 * 有键按下, 需要做处理
	 */
	if (tiny->tty) {
		tty_insert_flip_char(tiny->tty, 'u', 0);
	}
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
	return IRQ_HANDLED;
}
static irqreturn_t GPIOB4_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	struct tiny_serial *tiny = tiny_table[0];
	unsigned int key_flag = 0, key;
	key_flag = at91_get_gpio_value(AT91_PIN_PB4);
	if (key_flag) {
#ifdef PB_DEBUG
		printk("up GPIOB4\n");
		printk("GPIOB4 return\n");
#endif
		return IRQ_HANDLED;
	} else {
#ifdef PB_DEBUG
		printk("down GPIOB4\n");
#endif
	}
	// 记录当前时间
	current_jif = jiffies;
	key = key_flag = KEYDOWN;
	if (key == BUF_TAIL) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	// 记录KEY时钟滴答
	key_jif = current_jif;
	BUF_HEAD = KEYDOWN;
    keydev.head = INCBUF(keydev.head, KEY_BUF);
    flag = 1;
    wake_up_interruptible(&(keydev.wq));
	/*
	 * 有键按下, 需要做处理
	 */
	if (tiny->tty) {
		tty_insert_flip_char(tiny->tty, '3', 0);
	}
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
	return IRQ_HANDLED;
}
static irqreturn_t GPIOB5_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	struct tiny_serial *tiny = tiny_table[0];
	unsigned int key_flag = 0, key;
	key_flag = at91_get_gpio_value(AT91_PIN_PB5);
	if (key_flag) {
#ifdef PB_DEBUG
		printk("up GPIOB5\n");
		printk("GPIOB5 return\n");
#endif
		return IRQ_HANDLED;
	} else {
#ifdef PB_DEBUG
		printk("down GPIOB5\n");
#endif
	}
	// 记录当前时间
	current_jif = jiffies;
	key = key_flag = KEYCANCLE;
	if (key == BUF_TAIL) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	// 记录KEY时钟滴答
	key_jif = current_jif;
	BUF_HEAD = KEYCANCLE;
    keydev.head = INCBUF(keydev.head, KEY_BUF);
    flag = 1;
    wake_up_interruptible(&(keydev.wq));
	/*
	 * 有键按下, 需要做处理
	 */
	if (tiny->tty) {
		tty_insert_flip_char(tiny->tty, ' ', 0);
	}
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
	return IRQ_HANDLED;
}
static irqreturn_t GPIOB7_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag = 0;
	int i;
	// GPIOB7 线上有数据变化, 为高电平的时候则要延迟等待2秒
#if 0
	key_flag = at91_get_gpio_value(AT91_PIN_PB7);
	if (key_flag) {
#ifdef PB_DEBUG
		printk("up GPIOB7\n");
		printk("GPIOB7 return\n");
#endif
		return IRQ_HANDLED;
	} else {
#ifdef PB_DEBUG
		printk("down GPIOB7\n");
#endif
	}
#endif
	key_flag = at91_get_gpio_value(AT91_PIN_PB7);
	if(!key_flag) {			//检测为低电平
#ifdef PB_DEBUG
		printk("up GPIOB7: keyflag %d\n", key_flag);
		printk("GPIOB7 droped\n");
#endif
		return IRQ_HANDLED;
	}
	for (i=0; i < 250; i++) {	//判断按关机键2秒
		mdelay(10);
		key_flag = at91_get_gpio_value(AT91_PIN_PB7);
		if(!key_flag) {//如果在2秒之内出现低电平, 则正常退出 if(!(AT91_SYS->PIOB_PDSR & 0x80))
#ifdef PB_DEBUG
			printk("down GPIOB7\n");
			printk("GPIOB7 droped\n");
#endif
			return IRQ_HANDLED;			//检测为低电平
		}
	}
	at91_halt = 1;
#ifdef PB_DEBUG
	printk("TURN OFF\n");
	printk("delay_times is %dms\n", i * 10);
#endif
	// 记录当前时间
	current_jif = jiffies;
#ifdef PB_DEBUG
	printk("current_jif = %d, key_jif = %d\n", (int)current_jif, (int)key_jif);
#endif
	// 记录KEY时钟滴答
	key_jif = current_jif;
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
#undef PB_DEBUG
	return IRQ_HANDLED;
}

static int at91_key_open(struct inode *inode,struct file *filp) 
{
    int ret;
    float rate = 2.555555, n = 8.2, res;
	int k;
    keydev.head = keydev.tail = 0;
	res = rate * n;
	k = res;
    printk("res is %d\n", res);
    printk("res is %d\n", k);
    printk("res is %08x\n", res);
    return 0;
}

static int button_release(struct inode *inode,struct file *filp)
{
    return 0;
}

static ssize_t at91_key_read(struct file *filp,char *buffer,size_t count,loff_t *ppos)
{
    unsigned char key;
	while (1) {
	    if (keydev.head != keydev.tail) {
		    key = key_read();
		    copy_to_user(buffer, (char *)&key, sizeof(unsigned char));
#ifdef DEBUG
		    printk("the button_ret is 0x%x\n", key);
#endif
		    return sizeof(unsigned char);
	    } else {
		    if(filp->f_flags & O_NONBLOCK)
		        return -EAGAIN;
#ifdef DEBUG
		    printk("sleep\n");
#endif
		    //interruptible_sleep_on(&(keydev.wq));//为安全起见,最好不要调用该睡眠函数
		    wait_event_interruptible(keydev.wq, flag);
		    flag = 0;
#ifdef DEBUG
		    printk("sleep_after\n");
#endif
		    if (signal_pending(current)) {
		        return -ERESTARTSYS;
			}
		}
	}
	//return sizeof(unsigned char);
}

static struct file_operations at91_key_fops = {
    .owner  =   THIS_MODULE,
    .open   =   at91_key_open,
    .read   =   at91_key_read,
    .release    =   button_release,
};



static int __init at91_key_init(void)
{
    int ret;
    
    ret = register_chrdev(0, DEVICE_NAME, &at91_key_fops);
    if (ret<0) {
	    printk("at91_key: can't get major number 0\n");
	    return ret;
    }
    key_major = ret;
#ifdef DEBUG
	printk("key major: %d\n", key_major);
#endif
#ifdef  CONFIG_DEVFS_FS
	devfs_mk_cdev(MKDEV(key_major, BUTTONMINOR),
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, DEVICE_NAME);
#endif
    //keydev.head=keydev.tail=0;
    keydev.buttonStatus = 0;
    init_waitqueue_head(&(keydev.wq));
#if 1
    ret = request_irq(AT91_PIN_PB0, GPIOB0_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0) {
		printk("GPIOB0: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB1, GPIOB1_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0) {
		printk("GPIOB1: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB2, GPIOB2_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0) {
		printk("GPIOB2: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB3, GPIOB3_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0) {
		printk("GPIOB3: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB4, GPIOB4_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0) {
		printk("GPIOB4: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB5, GPIOB5_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0) {
		printk("GPIOB5: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB7, GPIOB7_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0) {
		printk("GPIOB7: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
#else
    ret = request_irq(GPIOB_IRQ, GPIOB_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0)
	{
		printk("GPIOB:INTERRUPT REQUEST ERROR.\n");
		return ret;
    }
#endif
    printk(DEVICE_NAME"1.0 register!\n");
    return 0;
}

static unsigned char key_read(void)
{
    unsigned char key;
    key = BUF_TAIL;
    keydev.tail = INCBUF(keydev.tail, KEY_BUF);
    return key;
}
    
static void __exit at91_key_eixt(void)
{
 
 #ifdef CONFIG_DEVFS_FS
    devfs_remove(DEVICE_NAME);
 #endif
    unregister_chrdev(key_major,DEVICE_NAME);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wjzhe");
MODULE_DESCRIPTION ("key input driver");

module_init(at91_key_init);
module_exit(at91_key_eixt);

