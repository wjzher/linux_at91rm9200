/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�at91_gpiob.c
 * ժ    Ҫ��PIOB������������, PB0~PB5�Ӽ���, PB7��Power��, PB6������
 * 			 �ں˸��µ�2.6.12��, ��GPIO���ж�����ǰ�汾��һ��
 *			 ����ע��7���ж������ж԰�������Ӧ
 *			 �ں˰汾����2.6ʱһ��ע���жϺ����ķ���ֵ, IRQ_HANDLE-->1
 *			 INIT����ر��жϴ���, ��ֹ�ػ����ж�, ��Ӧ��Ҫ��Main��Ҫ��
 *			 �ػ��ź����Ϲر�! PIOB15Ϊǿ�ƹػ��ź���, ��Ҫ������
 *			 ����Թرհ���֮��ı�־ֵ, Ӧ�ó������֪��ʲô��������
 *			 �޸�һЩ�Ĵ���������
 *			 ȥ��  ʹ��PIOB15��PIOB_ODSRд��Ч
 *			 �����첽I/O����, ���п��ؼ�����ʱ�ᷢ��SIGIO�ź�
 * 
 * ��ǰ�汾��1.6
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��11��22��
 *
 * ȡ���汾��1.5
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��11��22��
 *
 * ȡ���汾��1.4
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��8��10��
 *
 * ȡ���汾��1.3 
 * ԭ����  ��wjzhe
 * ������ڣ�2007��7��19��
 *
 * ȡ���汾��1.2 
 * ԭ����  ��wjzhe
 * ������ڣ�2007��4��29��
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
#include <linux/poll.h>
#include "gpiob.h"

#undef PB_DEBUG
// define input buffer
static unsigned char input_buf[256];
static int head, tail;
// special parameter
static int net_on;
static int key_on;
static int at91_halt = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12))
#define NEWPIO
#include <asm/arch/board.h>
#endif

#define GPIOBFASYNC
#define GPIOBPOLL

#ifdef GPIOBFASYNC
struct fasync_struct *fasync_queue;
#endif

#ifdef GPIOBPOLL
#if 0
static DECLARE_WAIT_QUEUE_HEAD(gpiob_wq);
#else
// �ȴ�����
static wait_queue_head_t gpiob_wq;
// �ź���
static struct semaphore gpiob_sem;
// ����һ����־λ
static int gpio7int;
#endif
#endif

#ifdef NEWPIO
// ����2.6.12�Ժ�汾���жϺ���
static irqreturn_t GPIOB0_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag = 0, key;
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
	// ��¼��ǰʱ��
	current_jif = jiffies;
	key = key_flag = KEYUP;
	if (key == input_buf[tail - 1]) {
		// ��ʱҪ�ж�ʱ�ӵδ���
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// ��¼KEYʱ�ӵδ�
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
	// ��¼��ǰʱ��
	current_jif = jiffies;
	key = key_flag = KEYLEFT;
	if (key == input_buf[tail - 1]) {
		// ��ʱҪ�ж�ʱ�ӵδ���
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// ��¼KEYʱ�ӵδ�
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
	// ��¼��ǰʱ��
	current_jif = jiffies;
	key = key_flag = KEYOK;
	if (key == input_buf[tail - 1]) {
		// ��ʱҪ�ж�ʱ�ӵδ���
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// ��¼KEYʱ�ӵδ�
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
	// ��¼��ǰʱ��
	current_jif = jiffies;
	key = key_flag = KEYRIGHT;
	if (key == input_buf[tail - 1]) {
		// ��ʱҪ�ж�ʱ�ӵδ���
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// ��¼KEYʱ�ӵδ�
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
	// ��¼��ǰʱ��
	current_jif = jiffies;
	key = key_flag = KEYDOWN;
	if (key == input_buf[tail - 1]) {
		// ��ʱҪ�ж�ʱ�ӵδ���
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// ��¼KEYʱ�ӵδ�
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
	// ��¼��ǰʱ��
	current_jif = jiffies;
	key = key_flag = KEYCANCLE;
	if (key == input_buf[tail - 1]) {
		// ��ʱҪ�ж�ʱ�ӵδ���
#ifdef PB_DEBUG
		printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
#endif
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// ��¼KEYʱ�ӵδ�
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
static irqreturn_t GPIOB7_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag = 0;
	int i;
	// GPIOB7 ���������ݱ仯, Ϊ�ߵ�ƽ��ʱ����Ҫ�ӳٵȴ�2��
#define PB_DEBUG
	if (at91_halt) {
		// �Ѿ��ػ���
		return IRQ_HANDLED;
	}
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
#define IDR_PB7
#ifdef IDR_PB7
	/* �ص�PB7�ж� */
	AT91_SYS->PIOB_IDR = AT91C_PIO_PB7;
#endif
#ifdef GPIOBPOLL
	// �йػ�������, ����Ҫ���ѵȴ�����
	//wake_up_interruptible(&gpiob_wq);
	gpio7int = 1;
	printk("wake_up GPIOB7 wq\n");
#endif
	key_flag = at91_get_gpio_value(AT91_PIN_PB7);
	if(!key_flag) {			//���Ϊ�͵�ƽ
#ifdef PB_DEBUG
		printk("up GPIOB7: keyflag %d\n", key_flag);
		printk("GPIOB7 droped\n");
#endif
		// ��������п����ǵ�Դ������ɵ�
		// LCD��Ҫ���³�ʼ����?
#ifdef IDR_PB7
		/* ��PB7�ж� */
		AT91_SYS->PIOB_IER = AT91C_PIO_PB7;
#endif
#ifdef GPIOBFASYNC
		if (fasync_queue) {
#ifdef PB_DEBUG
			//printk("send SIGIO...\n");
#endif
			kill_fasync(&fasync_queue, SIGIO, POLL_IN);
		}
#endif
		return IRQ_HANDLED;
	}
	for (i=0; i < 250; i++) {	//�жϰ��ػ���2��
		mdelay(10);
		key_flag = at91_get_gpio_value(AT91_PIN_PB7);
		if(!key_flag) {//�����2��֮�ڳ��ֵ͵�ƽ, �������˳� if(!(AT91_SYS->PIOB_PDSR & 0x80))
#ifdef PB_DEBUG
			printk("down GPIOB7\n");
			printk("GPIOB7 droped\n");
#endif
#ifdef IDR_PB7
			/* ��PB7�ж� */
			AT91_SYS->PIOB_IER = AT91C_PIO_PB7;
#endif
			return IRQ_HANDLED;			//���Ϊ�͵�ƽ
		}
	}
	at91_halt = 1;
	// ��¼��ǰʱ��
	current_jif = jiffies;
#ifdef PB_DEBUG
	printk("TURN OFF\n");
	printk("delay_times is %dms\n", i * 10);
	printk("current_jif = %d, key_jif = %d\n", (int)current_jif, (int)key_jif);
	printk("key_flag is %d\n",key_flag);
#endif
	// ��¼KEYʱ�ӵδ�
	key_jif = current_jif;
#ifdef IDR_PB7
	/* ��PB7�ж� */
	AT91_SYS->PIOB_IER = AT91C_PIO_PB7;
#endif
#undef IDR_PB7
#undef PB_DEBUG
	return IRQ_HANDLED;
}
// ��Ϊ���жϲ��ǹ����ж�, ���Բ���Ҫ����
// �����ж�, ���жϺ�����ӵ��ж��б���
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
// �����ж�, ���жϺ������ж��б���ɾ��
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
// 2.6.12��ǰ�汾���������жϴ�����
static unsigned int key;
static irqreturn_t GPIOB_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	static unsigned long key_jif;
	unsigned long current_jif;
	unsigned int key_flag;
	// input a key, key_flag.input = 1, but piob.input = 0
	key_flag = AT91_SYS->PIOB_ISR;	//��ȡ����仯�ж�
	key_flag &= 0xff;
	if (key_flag == 0x80) {			//turn off�趨�ػ��ź���Ч
		int i;
		for (i=0; i < 100; i++) {	//�жϰ��ػ���2��
			mdelay(20);
			if(!(AT91_SYS->PIOB_PDSR & 0x80))
				return IRQ_HANDLED;			//���Ϊ�͵�ƽ���ް�����
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
	// ��¼��ǰʱ��
	current_jif = jiffies;
	key = key_flag;
	if (key == input_buf[tail - 1]) {
		// ��ʱҪ�ж�ʱ�ӵδ���
		//printk("current_jif = %d, key_jif = %d\n", current_jif, key_jif);
		if ((current_jif - key_jif) < 30)
			return IRQ_HANDLED;
	}
	input_buf[tail++] = key_flag;
	// ��¼KEYʱ�ӵδ�
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
static inline void Cfg_piob(void)
{
	AT91_SYS->PIOB_ODR = 0x0000FF;			//PIOB0~PIOB7�������
	AT91_SYS->PIOB_PER = 0x0000FFFF;			//PIOB0~PIOB15ʹ��
	AT91_SYS->PIOB_IFER = 0x0000FFFF;		//PIOB0~PIOB15�����˲�ʹ��
	AT91_SYS->PIOB_MDDR = 0x0000FFFF;		//PIOB0~PIOB15���ö�����
	AT91_SYS->PIOB_PPUER = 0x0000FFFF;		//PIOB0~PIOB7����ʹ��
	AT91_SYS->PIOB_OWDR = 0x0000FF;		//����PIOB0~PIOB7��PIOB_ODSR(���״̬REG)д
	AT91_SYS->PIOB_OER = 0x0000FF00;
	//AT91_SYS->PIOB_OWER = AT91C_PIO_PB15;		//ʹ��PIOB15��PIOB_ODSRд��Ч

	AT91_SYS->PIOB_IER = 0x0000FF;			//PIOB0~PIOB7�ж�ʹ��
	AT91_SYS->PMC_PCER = 1 << AT91C_ID_PIOB;	// start PIOB clk
}
//����gpiob0~7�ж�ģʽ��8~15���ģʽ
static int gpiob_open(struct inode *inode, struct file *filp)
{
	//GPIOB_MINOR = MINOR(inode->i_rdev);
	//unsigned int gpiob_isr;
#if 0
	AT91_SYS->PIOB_ODR |= 0x0000FF;			//PIOB0~PIOB7�������
	AT91_SYS->PIOB_OER &= (!0x0000FF);		//PIOB0~PIOB7���ʹ����Ч
	AT91_SYS->PIOB_PER |= 0x0000FFFF;			//PIOB0~PIOB7ʹ��
	AT91_SYS->PIOB_PDR &= (!0x0000FFFF);		//PIOB0~PIOB7������Ч
	AT91_SYS->PIOB_IFDR &= (!0x0000FFFF);		//PIOB0~PIOB7�����˲�������Ч
	AT91_SYS->PIOB_IFER |= 0x0000FFFF;		//PIOB0~PIOB7�����˲�ʹ��
	AT91_SYS->PIOB_MDDR |= 0x0000FFFF;		//PIOB0~PIOB7���ö�����
	AT91_SYS->PIOB_MDER &= (!0x0000FFFF);		//PIOB0~PIOB7������ʹ����Ч
	AT91_SYS->PIOB_PPUER |= 0x0000FFFF;		//PIOB0~PIOB7����ʹ��
	AT91_SYS->PIOB_PPUDR &= (!0x0000FFFF);	//PIOB0~PIOB7����������Ч
	AT91_SYS->PIOB_ASR &= (!0x0000FFFF);		//PIOB0~PIOB7����Aѡ����Ч
	AT91_SYS->PIOB_BSR &= (!0x0000FFFF);		//PIOB0~PIOB7����Bѡ����Ч
	AT91_SYS->PIOB_OWDR |= 0x0000FFFF;		//����PIOB0~PIOB7��PIOB_ODSR(���״̬REG)д
	AT91_SYS->PIOB_OWER &= (!0x0000FFFF);		//ʹ��PIOB0~PIOB7��PIOB_ODSRд��Ч
	AT91_SYS->PMC_PCER |= 1 << AT91C_ID_PIOB;	// start PIOB clk
	AT91_SYS->PIOB_OER |= 0x0000FF00;
	AT91_SYS->PIOB_ODR &= (!0x0000FF00);

	AT91_SYS->PIOB_IDR &= (!0x0000FF);		//PIOB0~PIOB7�жϽ�����Ч
	AT91_SYS->PIOB_IER |= 0x0000FF;			//PIOB0~PIOB7�ж�ʹ��
#else
	Cfg_piob();
#endif
	//AT91_SYS->PIOB_CODR |= 0x0000FF;		//���PIOB0~PIOB7��������������
	//AT91_SYS->PIOB_SODR &= (!0x0000FF);		//����PIOB0~PIOB7����������������Ч
	//AT91_SYS->PIOB_SODR |= 0x0000FF00;
	//AT91_SYS->PIOB_CODR &= (!0x0000FF00);
	//AT91_SYS->PIOB_CODR |= 0x0000FFFF;		//���PIOB0~PIOB7��������������
	//AT91_SYS->PIOB_SODR &= (!0x0000FFFF);		//����PIOB0~PIOB7����������������Ч
	//gpiob_isr = AT91_SYS->PIOB_ISR;
	//printk("current gpiob_isr: 0x%08x\n", AT91_SYS->PIOB_ISR);
	//gpiob_isr = AT91_SYS->PIOB_ISR;

	enable_piob();
	//udelay(10);
	if ((AT91_SYS->PIOB_PDSR & 0x00000040)) {
		net_on = 0;
	} else {
		net_on = 1;
	}
	// ���ݳ�ʹ��
	head = tail = 0;
	at91_halt = 0;
	key_on = 1;
#ifdef PB_DEBUG
	printk("current AIC_SMR: 0x%04x\n", AT91_SYS->AIC_SMR[GPIOB_IRQ]);
#endif
	return 0;
}
static int gpiob_ioctl(struct inode* inode,struct file* file, unsigned int cmd, unsigned long arg)
{
#define USEGPIO
	switch(cmd)
	{
	case CLEARBIT:
#ifdef USEGPIO
		if ((at91_set_gpio_value(PIN_BASE + 0x20 + arg, 0)) < 0) {
		}
#else
		AT91_SYS->PIOB_CODR = 1 << arg;		//���PIOB[arg]�ϵ�����
#endif
		//AT91_SYS->PIOB_SODR &= (!0x00000001 << arg);	//����PIOB[arg]������Ч
		break;
	case SETBIT:
#ifdef USEGPIO
		if ((at91_set_gpio_value(PIN_BASE + 0x20 + arg, 1)) < 0) {
		}
#else
		AT91_SYS->PIOB_SODR = 1 << arg;		//����PIOB[arg]�ϵ�����
#endif
#undef USEGPIO
		//AT91_SYS->PIOB_CODR &= (!0x00000001) << arg;	//���PIOB[arg]������Ч
		break;
	case ENIOB:
		udelay(1000);
		AT91_SYS->PIOB_IER |= 0xFF;			//PIOB0~PIOB7�ж�ʹ��
		key_on = 1;
		enable_piob();
		break;
	case DISIOB:
		udelay(1000);
		AT91_SYS->PIOB_IDR |= 0xFF;
		key_on = 0;
		disable_piob();// �������жϺ�����������ɾ��!!!
		break;
	case GETNET:		// �˹��ܽ�������
		if ((AT91_SYS->PIOB_PDSR&0x00000040)) {
			net_on = 0;
		} else {
			net_on = 1;
		}
		return (put_user(net_on, (int *)arg));
	case CLEARKEY:		// ����������������м�ֵ
		head = tail = 0;
		break;
	case GETHATL:		// ȡ�ùػ���
		if (key_on == 0) {
			return -1;
		}
		if (at91_halt) {
			if (put_user(KEYONOFF, (int *)arg) < 0)
				return -EFAULT;
			// �Ѿ�ȡ�߹ػ��ź�, �����־
			at91_halt = 0;
		} else {
			put_user(0, (int *)arg);
		}
		break;
	case DISABLEKEY:	// ��ֹ�����ж�
		//AT91_SYS->PIOB_IER &= (!0x0000FF);
		udelay(1000);
		AT91_SYS->PIOB_IDR = 0x0000FF;
		key_on = 0;
		//disable_piob();// �������жϺ�����������ɾ��!!!
		break;
	case ENABLEKEY:		// ���ð����ж�
		//AT91_SYS->PIOB_IDR &= (!0x0000FF);
		udelay(1000);
		AT91_SYS->PIOB_IER = 0x0000FF;
		key_on = 1;
		//enable_piob();
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
// ��ȡ������ֵ
static int gpiob_read(struct file *file, char* buff, size_t count, loff_t *offp)
{
	int ret;
	if (key_on == 0)
		return -1;
	//printk("head: %d, tail:%d\n", head, tail);
	// �ж��Ƿ����������
	if (head == tail) {
		return 0;
	}
	// ����ֵ�����û��ռ�
	ret = put_user(input_buf[head], buff);
	if (ret < 0) {
		return ret;
	}
	// һ��ֻ�ܶ�һ������, �Ժ�����ݽ�����0
	count--;
	while (count--) {
		buff++;
		ret = put_user(0, buff);
		if (ret < 0) {
			return ret;
		}
	}
	// ���������±�
	if (head >= 0xFF) {
		head = 0;
	} else {
		head++;
	}
	return 1;
}

#ifdef GPIOBPOLL
// ��������ʵ��poll����, �Ա�Ӧ�ó���֪���Ƿ��п��ذ�������
static unsigned int gpiob_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	wait_queue_head_t *wqh = &gpiob_wq;
	struct semaphore *sem = &gpiob_sem;

	/*
	 * ��Ҫ����, ֻ����һ�������ڴ˵ȴ�
	 */
	down(sem);
	printk("poll caller...\n");
	poll_wait(filp, wqh,  wait);
	// ����ֻ�ж�, ����ֻ�ܱ�־POLLIN
	if (filp->f_mode & FMODE_READ) {
		if (gpio7int) {
			mask |= POLLIN | POLLRDNORM;	/* readable */
			gpio7int = 0;
			printk("poll can POLLIN\n");
		}
	}
	printk("poll end\n");
	up(sem);
	return mask;
}
#endif
#ifdef GPIOBFASYNC
// �˺���ʵ���첽I/O...
// �첽I/O...
static int gpiob_fasync(int fd, struct file *filp, int mode)
{
	struct fasync_struct **asyq = &fasync_queue;
	int ret;
#ifdef PB_DEBUG
	printk("fasync caller...\n");
	printk("mode %d %x\n", mode, mode);
#endif
	ret = fasync_helper(fd, filp, mode, asyq);
	if (ret < 0) {
		printk("fasync_helper return %d\n", ret);
	}
#ifdef PB_DEBUG
	printk("pointer fasync_queue = %p\n", fasync_queue);
#endif
	return ret;
}
#endif

struct file_operations gpiob_fops = {
	.owner = THIS_MODULE,
	.open = gpiob_open,
	.ioctl = gpiob_ioctl,
	.write = gpiob_write,
	.read = gpiob_read,
#ifdef GPIOBPOLL
	.poll =		gpiob_poll,
#endif
#ifdef GPIOBFASYNC
	.fasync =	gpiob_fasync,
#endif
};

static __init int gpiob_init(void)
{
	int ret;
	// ע���豸, �豸��Ϊ135
	ret = register_chrdev(GPIOB_MAJOR, "GPIOB", &gpiob_fops);				//ע���豸
	if (ret < 0) {
		printk(KERN_ERR "GPIOB: couldn't get a major %d for PIOB.\n", GPIOB_MAJOR);
		return -EIO;
	}
	// ���������ж�, һ��7��I/O
#ifdef NEWPIO
    ret = request_irq(AT91_PIN_PB0, GPIOB0_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0) {
		printk("GPIOB0: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB1, GPIOB1_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0) {
		printk("GPIOB1: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB2, GPIOB2_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0) {
		printk("GPIOB2: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB3, GPIOB3_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0) {
		printk("GPIOB3: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB4, GPIOB4_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0) {
		printk("GPIOB4: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB5, GPIOB5_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0) {
		printk("GPIOB5: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
    ret = request_irq(AT91_PIN_PB7, GPIOB7_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0) {
		printk("GPIOB7: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }
#else
    ret = request_irq(GPIOB_IRQ, GPIOB_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0)
	{
		printk("GPIOB:INTERRUPT REQUEST ERROR.\n");
		return ret;
    }
#endif
	/*ret = request_irq(GPIOB_IRQ, GPIOB_irq_handler, 0, "GPIOB", NULL);		//ע���ж�IRQ3
	if (ret < 0) {
		printk("GPIOB: INTERRUPT REQUEST ERROR.\n");
		return -EBUSY;
    }*/
#ifdef PB_DEBUG
    printk("AT91 GPIOB INTERRUPT REQUEST SUCCESS.\n");
#endif
	Cfg_piob();
	disable_piob();
	// ���뽫GPIOB15��Ϊ1
	if ((ret = at91_set_gpio_value(AT91_PIN_PB15, 1)) < 0) {
		return ret;
	}
	//���ȼ�
	AT91_SYS->AIC_SMR[GPIOB_IRQ] = AT91C_AIC_SRCTYPE_EXT_POSITIVE_EDGE;// | AT91C_AIC_PRIOR_HIGHEST;
	AT91_SYS->PIOB_IDR = 0xFFFF;
	//AT91_SYS->PIOB_IER &= (!0x0000FF);
	//AT91_SYS->PIOB_IER &= (!0x0000FF00);
	//AT91_SYS->PIOB_IDR = 0x0000FF00;
	//printk("current gpiob_isr: 0x%08x\n", AT91_SYS->PIOB_ISR);
	// ���жϽ���
	// ��ʼ���ȴ�����ͷ���ź���
#ifdef GPIOBPOLL
	init_waitqueue_head(&gpiob_wq);
	init_MUTEX(&gpiob_sem);
#endif
	printk("AT91 Key_input driver v1.4\n");
	return ret;
}
// �ͷ��豸, �˺���ʵ��ûʲô����
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
	unregister_chrdev(GPIOB_MAJOR, "GPIOB");				//ע���豸
}

module_init(gpiob_init);
module_exit(cleanup_piob);

MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("GPIOB driver for NKTY AT91RM9200")
