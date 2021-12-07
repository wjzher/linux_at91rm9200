/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_gpiob.c
 * 摘    要：PIOB按键驱动程序, PB0~PB5接键盘, PB7接Power键, PB6接网络
 * 			 内核更新到2.6.12后, 对GPIO的中断与以前版本不一致
 *			 程序注册7个中断来进行对按键的响应
 *			 内核版本高于2.6时一定注意中断函数的返回值, IRQ_HANDLE-->1
 *			 INIT加入关闭中断处理, 防止关机键中断, 相应的要在Main中要将
 *			 关机信号马上关闭! PIOB15为强制关机信号线, 不要随便清除
 *			 Nov 13, 2008 改进关机键判断方式, 引入内核定时器
 * 
 * 当前版本：1.3
 * 作    者：wjzhe
 * 完成日期：2008年11月13日
 *
 * 取代版本：1.2 
 * 原作者  ：wjzhe
 * 完成日期：2007年2月13日
 *
 * 取代版本：1.0 
 * 原作者  ：wjzhe
 * 完成日期：2007年2月13日
 */
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
#include "gpiob.h"

#define PB_DEBUG
#undef PB_DEBUG
// define input buffer
static unsigned char input_buf[256];
static int head, tail;
// special parameter
static int net_on;
static int at91_halt = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12))
#define NEWPIO
#include <asm/arch/board.h>
#endif

#define KEYDELAY 40

#ifdef NEWPIO

#if 0
typedef enum {
	COMMON,
	FALLING,
	LOW,
	RISING
} k_status;

struct key_status {
	unsigned long key_jif;
	k_status status;
} gpio_key[8];
#endif

// 对于2.6.12以后版本的中断函数
static irqreturn_t GPIOB0_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag = 0, key;
	key_flag = at91_get_gpio_value(AT91_PIN_PB0);
	// 只关心下降沿, 但是在按键弹起的时候也会有下降沿
	// 加入状态机, 进行识别当前按键状态
#if 0
	// 先判断当前状态
	if (gpio_key[0].status == COMMON) {
		gpio
	} else if (gpio_key[0].status == FALLING) {
	} else if (gpio_key[0].status == LOW) {
	} else {
	}
#endif
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
	if (key == input_buf[tail - 1]) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < KEYDELAY)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// 记录KEY时钟滴答
	key_jif = current_jif;
	tail &= 0xFF;
	if (tail == head) {
		head++;		// drop a input key
		head &= 0xFF;
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
	if (key == input_buf[tail - 1]) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < KEYDELAY)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// 记录KEY时钟滴答
	key_jif = current_jif;
	tail &= 0xFF;
	if (tail == head) {
		head++;		// drop a input key
		head &= 0xFF;
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
	if (key == input_buf[tail - 1]) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < KEYDELAY)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// 记录KEY时钟滴答
	key_jif = current_jif;
	tail &= 0xFF;
	if (tail == head) {
		head++;		// drop a input key
		head &= 0xFF;
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
	if (key == input_buf[tail - 1]) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < KEYDELAY)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// 记录KEY时钟滴答
	key_jif = current_jif;
	tail &= 0xFF;
	if (tail == head) {
		head++;		// drop a input key
		head &= 0xFF;
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
	if (key == input_buf[tail - 1]) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < KEYDELAY)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// 记录KEY时钟滴答
	key_jif = current_jif;
	tail &= 0xFF;
	if (tail == head) {
		head++;		// drop a input key
		head &= 0xFF;
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
	if (key == input_buf[tail - 1]) {
		// 此时要判断时钟滴答数
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < KEYDELAY)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// 记录KEY时钟滴答
	key_jif = current_jif;
	tail &= 0xFF;
	if (tail == head) {
		head++;		// drop a input key
		head &= 0xFF;
	}
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
	return IRQ_HANDLED;
}

#ifdef CONFIG_OLDHALTCHK
#warning "OLDHALTCHK...GPIOB"
static irqreturn_t GPIOB7_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag = 0;
	int i;
	// GPIOB7 线上有数据变化, 为高电平的时候则要延迟等待2秒
//#define PB_DEBUG
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
	if (at91_halt)
		return IRQ_HANDLED;
#define IDR_PB7
#ifdef IDR_PB7
	/* 关掉PB7中断 */
	AT91_SYS->PIOB_IDR = AT91C_PIO_PB7;
#endif
	key_flag = at91_get_gpio_value(AT91_PIN_PB7);
	if(!key_flag) {			//检测为低电平
#ifdef PB_DEBUG
		printk("up GPIOB7: keyflag %d\n", key_flag);
		printk("GPIOB7 droped\n");
#endif
#ifdef IDR_PB7
		/* 打开PB7中断 */
		AT91_SYS->PIOB_IER = AT91C_PIO_PB7;
#endif
		return IRQ_HANDLED;
	}
	for (i=0; i < 250; i++) {	//判断按关机键2秒
		// add pat dog, modified by wjzhe, 2010-12-22
		AT91_SYS->ST_CR = AT91C_ST_WDRST;	/* Pat the watchdog */
		mdelay(10);
		key_flag = at91_get_gpio_value(AT91_PIN_PB7);
		if(!key_flag) {//如果在2秒之内出现低电平, 则正常退出 if(!(AT91_SYS->PIOB_PDSR & 0x80))
#ifdef PB_DEBUG
			printk("down GPIOB7\n");
			printk("GPIOB7 droped\n");
#endif
#ifdef IDR_PB7
			/* 打开PB7中断 */
			AT91_SYS->PIOB_IER = AT91C_PIO_PB7;
#endif
			return IRQ_HANDLED;			//检测为低电平
		}
	}
	at91_halt = 1;
#ifdef PB_DEBUG
	printk("TURN OFF\n");
	printk("delay_times is %d\n",i);
#endif
	// 记录当前时间
	current_jif = jiffies;
#ifdef PB_DEBUG
	//	printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
#ifdef IDR_PB7
	/* 打开PB7中断 */
	AT91_SYS->PIOB_IER = AT91C_PIO_PB7;
#endif
	// 记录KEY时钟滴答
	key_jif = current_jif;
#ifdef PB_DEBUG
	printk("key_flag is %d\n",key_flag);
#endif
#undef PB_DEBUG
	return IRQ_HANDLED;
}
#else
// 用work queue来做中断处理的底半部
// 定义一些有用的变量
static unsigned long shk_jif;//next_jif, 
#if 0
static struct work_struct work_piob7;
static int pidgo = 0;
static wait_queue_head_t piob_queue;
static DECLARE_WAIT_QUEUE_HEAD(piob_queue);
#endif
// 内核定时器
static struct timer_list halt_timer;
static int up_flag = 1;		// 抬起标志
#if 0
static int key_do_work(void *arg)
{
	while (pidgo) {
		if (time_before(jiffies, next_jif)) {
		} else {
			at91_halt = 1;
			break;
		}
	}
	return 0;
}
#endif
// 这是用定时器实现的版本, 但会有竞争发生, 异步处理有问题
static void halt_timeout(unsigned long ptr)
{
#if 0
	unsigned int key_flag;
	// 定时器到了??
	key_flag = at91_get_gpio_value(AT91_PIN_PB7);
	if (key_flag && !up_flag) {// 还没有抬起
		// 判断有点多, 也许可以简化一下
		at91_halt = 1;
	} else {
		// 已经放开了关机键
		//del_timer(&halt_timer);
	}
#endif
#define KEYSAFEUP
	if (!up_flag
#ifdef KEYSAFEUP
		&& at91_get_gpio_value(AT91_PIN_PB7)
#endif
		) {
		at91_halt = 1;
	}
	return;
}
// int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);
static irqreturn_t GPIOB7_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int key_flag = 0;
#if 0
	//static int raise;
	if (raise && time_before(jiffies, shk_jif) && shk_jif) {
		// 如果是高
#ifdef DEBUG
		printk("clear shake...\n");
#endif
		return IRQ_HANDLED;
	}
#endif
	key_flag = at91_get_gpio_value(AT91_PIN_PB7);
	if(!key_flag) {			//检测为低电平
		// 这是一个下跳沿, 应该是结束了
		//shk_jif = 0;
		if (up_flag == 1) {
			// 已经是连续的两次下跳沿了
#ifdef DEBUG
			printk("up again...\n");
#endif
			//return IRQ_HANDLED;
		}
		// 如果还没有置位的话, 唤醒工作队列, 或者删除定时器
		if (!at91_halt) {
			//wake_up_interruptible(&piob_queue)
			del_timer(&halt_timer);
		}
		up_flag = 1;
		shk_jif = 0;
		//raise = 0;
		return IRQ_HANDLED;
	} else {
		// 检测为高电平, 是上升沿, 恰好是需要的
		//next_jif = jiffies + 2 * HZ;		// 两秒钟
		//if (next_jif == 0) {
		//	next_jif = 1;
		//}
		if (up_flag == 0) {
			// 第二次的上升沿
#ifdef DEBUG
			printk("up again...\n");
#endif
			return IRQ_HANDLED;
		}
		//raise = 1;
		shk_jif = jiffies + 5;	// 我们认为HZ是100, 提供50ms的消抖
		if (shk_jif == 0) {
			shk_jif = ~0;
		}
		init_timer(&halt_timer);
		halt_timer.function = halt_timeout;
		halt_timer.data = 0;
		halt_timer.expires = jiffies + 2 * HZ;	// 这里是2秒的定时器, 也许可以设的更长
		add_timer(&halt_timer);
		up_flag = 0;
		// 这时候启动工作队列
		//schedule_work(&work_piob7);
	}
	return IRQ_HANDLED;
}
#endif

// 因为此中断不是共享中断, 所以不需要调用
void enable_piob(void)
{
	enable_irq(AT91_PIN_PB0);
	enable_irq(AT91_PIN_PB1);
	enable_irq(AT91_PIN_PB2);
	enable_irq(AT91_PIN_PB3);
	enable_irq(AT91_PIN_PB4);
	enable_irq(AT91_PIN_PB5);
	enable_irq(AT91_PIN_PB7);
}
void disable_piob(void)
{
	disable_irq(AT91_PIN_PB0);
	disable_irq(AT91_PIN_PB1);
	disable_irq(AT91_PIN_PB2);
	disable_irq(AT91_PIN_PB3);
	disable_irq(AT91_PIN_PB4);
	disable_irq(AT91_PIN_PB5);
	disable_irq(AT91_PIN_PB7);
}
#else
static unsigned int key;
static irqreturn_t GPIOB_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag;
	// input a key, key_flag.input = 1, but piob.input = 0
	key_flag = AT91_SYS->PIOB_ISR;	//读取输入变化中断
	key_flag &= 0xff;
	if (key_flag == 0x80) {			//turn off设定关机信号有效
		int i;
		for (i=0; i < 100; i++) {	//判断按关机键2秒
			mdelay(20);
			if(!(AT91_SYS->PIOB_PDSR & 0x80))
				return IRQ_HANDLED;			//检测为低电平（无按键）
			//printk("AT91_SYS->PIOB_PDSR is %x\n",AT91_SYS->PIOB_PDSR);
		}
		at91_halt = 1;
		//printk("TURN OFF\n");
		//printk("delay_times is %d\n",i);
		return IRQ_HANDLED;
	}
	if (AT91_SYS->PIOB_PDSR & key_flag) {
		return IRQ_HANDLED;
	}
	// 记录当前时间
	current_jif = jiffies;
	key = key_flag;
	if (key == input_buf[tail - 1]) {
		// 此时要判断时钟滴答数
		//printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
		if ((current_jif - key_jif) < KEYDELAY)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// 记录KEY时钟滴答
	key_jif = current_jif;
	tail &= 0xFF;
	if (tail == head) {
		head++;		// drop a input key
		head &= 0xFF;
	}
	//printk("key_flag is %d\n",key_flag);
	return IRQ_HANDLED;
}

#endif
//设置gpiob0~7中断模式，8~15输出模式
static int gpiob_open(struct inode *inode, struct file *filp)
{
	//GPIOB_MINOR = MINOR(inode->i_rdev);
	//unsigned int gpiob_isr;
	AT91_SYS->PIOB_ODR |= 0x0000FF;			//PIOB0~PIOB7输出禁用
	AT91_SYS->PIOB_OER &= (!0x0000FF);		//PIOB0~PIOB7输出使能无效
	AT91_SYS->PIOB_PER |= 0x0000FFFF;			//PIOB0~PIOB7使能
	AT91_SYS->PIOB_PDR &= (!0x0000FFFF);		//PIOB0~PIOB7禁用无效
	AT91_SYS->PIOB_IFDR &= (!0x0000FFFF);		//PIOB0~PIOB7输入滤波禁用无效
	AT91_SYS->PIOB_IFER |= 0x0000FFFF;		//PIOB0~PIOB7输入滤波使能
	AT91_SYS->PIOB_MDDR |= 0x0000FFFF;		//PIOB0~PIOB7禁用多驱动
	AT91_SYS->PIOB_MDER &= (!0x0000FFFF);		//PIOB0~PIOB7多驱动使能无效
	AT91_SYS->PIOB_PPUER |= 0x0000FFFF;		//PIOB0~PIOB7上拉使能
	AT91_SYS->PIOB_PPUDR &= (!0x0000FFFF);	//PIOB0~PIOB7上拉禁用无效
	AT91_SYS->PIOB_ASR &= (!0x0000FFFF);		//PIOB0~PIOB7外设A选择无效
	AT91_SYS->PIOB_BSR &= (!0x0000FFFF);		//PIOB0~PIOB7外设B选择无效
	AT91_SYS->PIOB_OWDR |= 0x0000FFFF;		//禁用PIOB0~PIOB7对PIOB_ODSR(输出状态REG)写
	AT91_SYS->PIOB_OWER &= (!0x0000FFFF);		//使能PIOB0~PIOB7对PIOB_ODSR写无效

	//AT91_SYS->PIOB_CODR |= 0x0000FF;		//清除PIOB0~PIOB7线上驱动的数据
	//AT91_SYS->PIOB_SODR &= (!0x0000FF);		//设置PIOB0~PIOB7线上驱动的数据无效
	//AT91_SYS->PIOB_SODR |= 0x0000FF00;
	//AT91_SYS->PIOB_CODR &= (!0x0000FF00);
	//AT91_SYS->PIOB_CODR |= 0x0000FFFF;		//清除PIOB0~PIOB7线上驱动的数据
	//AT91_SYS->PIOB_SODR &= (!0x0000FFFF);		//设置PIOB0~PIOB7线上驱动的数据无效
	//gpiob_isr = AT91_SYS->PIOB_ISR;
	AT91_SYS->PMC_PCER |= 1 << AT91C_ID_PIOB;	// start PIOB clk
	AT91_SYS->PIOB_OER |= 0x0000FF00;
	AT91_SYS->PIOB_ODR &= (!0x0000FF00);

	AT91_SYS->PIOB_IDR &= (!0x0000FF);		//PIOB0~PIOB7中断禁用无效
	AT91_SYS->PIOB_IER |= 0x0000FF;			//PIOB0~PIOB7中断使能
	//printk("current gpiob_isr: 0x%08x\n", AT91_SYS->PIOB_ISR);
	//gpiob_isr = AT91_SYS->PIOB_ISR;

	enable_piob();
	//udelay(10);
	if ((AT91_SYS->PIOB_PDSR & 0x00000040)) {
		net_on = 0;
	} else {
		net_on = 1;
	}
	// 数据初使化
	head = tail = 0;
	at91_halt = 0;
#ifdef CONFIG_OLDHALTCHK
        up_flag = 1;
#endif
#ifdef PB_DEBUG
	printk("current AIC_SMR: 0x%04x\n", AT91_SYS->AIC_SMR[GPIOB_IRQ]);
#endif
	return 0;
}

/*
 * release flash device
 */
static int gpiob_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static int gpiob_ioctl(struct inode* inode,struct file* file, unsigned int cmd, unsigned long arg)
{
	switch(cmd)
	{
	case CLEARBIT:
		AT91_SYS->PIOB_CODR |= 0x00000001 << arg;		//清除PIOB[arg]上的数据
		AT91_SYS->PIOB_SODR &= (!0x00000001 << arg);	//设置PIOB[arg]数据无效
		break;
	case SETBIT:
		AT91_SYS->PIOB_SODR |= 0x00000001 << arg;		//设置PIOB[arg]上的数据
		AT91_SYS->PIOB_CODR &= (!0x00000001) << arg;	//清除PIOB[arg]数据无效
		break;
	case ENIOB:
		AT91_SYS->PIOB_IER |= 0xFF;			//PIOB0~PIOB7中断使能
		enable_piob();
		break;
	case DISIOB:
		AT91_SYS->PIOB_IDR |= 0xFF;
		disable_piob();// 仅仅将中断函数从链表中删除!!!
		break;
	case GETNET:
		if ((AT91_SYS->PIOB_PDSR&0x00000040)) {
			net_on = 0;
		} else {
			net_on = 1;
		}
		return (put_user(net_on, (int *)arg));
	case CLEARKEY:
		head = tail = 0;
		break;
	case GETHATL:
		if (at91_halt) {
			put_user(KEYONOFF, (int *)arg);
			// 已经取走关机信号
			at91_halt = 0;
		} else {
			put_user(0, (int *)arg);
		}
		break;
	case DISABLEKEY:
		AT91_SYS->PIOB_IER &= (!0x0000FF);
		AT91_SYS->PIOB_IDR |= 0x0000FF;	
		break;
	case ENABLEKEY:
		AT91_SYS->PIOB_IDR &= (!0x0000FF);
		AT91_SYS->PIOB_IER |= 0x0000FF;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int gpiob_write(struct file *file, const char* buf, size_t count, loff_t *offp)
{
	return -1;		// can not write to key
}

static int gpiob_read(struct file *file, char* buff, size_t count, loff_t *offp)
{
	int ret;
	//printk("head: %d, tail:%d\n", head, tail);
	if (head == tail) {
		return 0;
	}
	ret = put_user(input_buf[head], buff);
	if (ret < 0) {
		return -1;
	}
	count--;
	while (count--) {
		buff++;
		ret = put_user(0, buff);
		if (ret < 0) {
			return -1;
		}
	}
	if (head >= 0xFF) {
		head = 0;
	} else {
		head++;
	}
	return 1;
}

struct file_operations gpiob_fops = {
	.owner = THIS_MODULE,
	.open = gpiob_open,
	.ioctl = gpiob_ioctl,
	.write = gpiob_write,
	.read = gpiob_read,
	.release = gpiob_close,
};

static __init int gpiob_init(void)
{
	int ret;
	// 注册设备, 设备号为135
	ret = register_chrdev(GPIOB_MAJOR, "GPIOB", &gpiob_fops);				//注册设备
	if (ret < 0) {
		printk(KERN_ERR "GPIOB: couldn't get a major %d for PIOB.\n", GPIOB_MAJOR);
		return -EIO;
	}
	// 进行申请中断, 一共7条I/O
#ifdef NEWPIO
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
	/*ret = request_irq(GPIOB_IRQ, GPIOB_irq_handler, 0, "GPIOB", NULL);		//注册中断IRQ3
	if (ret < 0) {
		printk("GPIOB: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }*/
#ifdef PB_DEBUG
    printk("AT91 GPIOB INTERRUPT REQUEST SUCCESS.\n");
#endif
	disable_piob();
	// 必须将GPIOB15置为1
	if ((ret = at91_set_gpio_value(AT91_PIN_PB15, 1)) < 0) {
		return ret;
	}
	//优先级最高
	AT91_SYS->AIC_SMR[GPIOB_IRQ] |= AT91C_AIC_SRCTYPE_EXT_POSITIVE_EDGE | AT91C_AIC_PRIOR_HIGHEST;
	AT91_SYS->PIOB_IDR |= 0xFF;
	AT91_SYS->PIOB_IER &= (!0x0000FF);
	AT91_SYS->PIOB_IER &= (!0x0000FF00);
	AT91_SYS->PIOB_IDR |= 0x0000FF00;
	//printk("current gpiob_isr: 0x%08x\n", AT91_SYS->PIOB_ISR);
	// 将中断禁用
	printk("AT91 Key_input driver v1.2\n");
	return ret;
}

static void __exit cleanup_piob(void)
{
#ifdef NEWPIO
	free_irq(AT91_PIN_PB0, NULL);
	free_irq(AT91_PIN_PB1, NULL);
	free_irq(AT91_PIN_PB2, NULL);
	free_irq(AT91_PIN_PB3, NULL);
	free_irq(AT91_PIN_PB4, NULL);
	free_irq(AT91_PIN_PB5, NULL);
	free_irq(AT91_PIN_PB7, NULL);
#else
	free_irq(GPIOB_IRQ, NULL);
#endif
	unregister_chrdev(GPIOB_MAJOR, "GPIOB");				//注销设备
}

module_init(gpiob_init);
module_exit(cleanup_piob);

MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("GPIOB driver for NKTY AT91RM9200")

