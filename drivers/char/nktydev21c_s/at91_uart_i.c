#include <linux/unistd.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm-arm/arch-at91rm9200/AT91RM9200_SYS.h>
#include <asm-arm/arch-at91rm9200/AT91RM9200_USART.h>
#include <asm/arch/pio.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/hardware.h>

#include "uart_dev.h"
#include "uart.h"

static unsigned char us0buf[256];
static int us0cnt;
static unsigned char us2buf[256];
static int us2cnt;

DECLARE_COMPLETION(us0_complete);
DECLARE_COMPLETION(us2_complete);

static irqreturn_t us0_interrupt(int irq, void *dev_id, struct pt_regs *regs)
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
static irqreturn_t us2_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	return IRQ_HANDLED;
}
/*
 * flag: 0->ctl0; 1->ctl2
 * send data to 485, return count is ok, -1 is timeout, -2 is other error
 * created by wjzhe, 10/16/2006
 */
int send_data(char *buf, size_t count, unsigned char num)	// attention: can continue sending?
{
	int i, ret = 0;
	//AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12;		// enable send data
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
	// 发送数据时不需要设置通道, 详见原理图
	if (!(num & 0x80)) {
		// open int of ...
		// 前四通道用UART0
		for (i = 0; i < count; i++) {
			uart_ctl0->US_THR = *buf++;
			uart_ctl0->US_IER = AT91C_US_TXRDY;
			if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY)) {
				ret = -1;
				goto out;
			}
		}
	} else {
		// 后四通道用UART2
		for (i = 0; i < count; i++) {
			uart_ctl2->US_THR = *buf++;
			if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY)) {
				ret = -1;
				goto out;
			}
		}
	}
	ret = count;
out:
	// 发送结束后马上切换到接收模式
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	return ret;
}
// 只发送一字节, 成功返回0
int send_byte(char buf, unsigned char num)
{
	int ret = 0;
#ifdef DISABLE_INT
	unsigned long flags;
#endif
	//AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12;		// enable send data
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
#ifdef DISABLE_INT
	local_irq_save(flags);		// 禁用中断, 保证发送数据原子的执行
#endif
	// 发送数据时不需要设置通道, 详见原理图
	if (!(num & 0x80)) {
		// 前四通道用UART0
#if 0
		/* us_cr is a read only register */
		uart_ctl0->US_CR &= !(AT91C_US_SENDA);
#endif
		uart_ctl0->US_THR = buf;
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY)) {
			ret = -1;
			goto out;
		}
	} else {
		// 后四通道用UART2
#if 0
		/* us_cr is a read only register */
		uart_ctl0->US_CR &= !(AT91C_US_SENDA);
#endif
		uart_ctl2->US_THR = buf;
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY)) {
			ret = -1;
			goto out;
		}
	}
out:
	// 发送结束后马上切换到接收模式
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
#ifdef DISABLE_INT
	local_irq_restore(flags);		// 禁用中断, 保证发送数据原子的执行
#endif
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	//udelay(1);
	return 0;
}

/*
 * flag: 0->ctl0; 1->ctl2
 * send address to 485, return 0 is ok, -1 is timeout
 * created by wjzhe, 10/16/2006
 */
int send_addr(unsigned char buf, unsigned char num)
{
	int ret = 0;
#ifdef DISABLE_INT
	unsigned long flags;
#endif
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
#ifdef DISABLE_INT
	local_irq_save(flags);		// 禁用中断, 保证发送数据原子的执行
#endif
  	if (!(num & 0x80)) {
		uart_ctl0->US_CR = AT91C_US_SENDA;
		uart_ctl0->US_THR = buf;
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY)) {
			ret = -1;
			goto err;
		}
 	} else {
		uart_ctl2->US_CR = AT91C_US_SENDA;
		uart_ctl2->US_THR = buf;
		//sel_ch(num);
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY)) {
			ret = -1;
			goto err;
		}
	}
err:
	// 发送结束后马上切换到接收模式
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
#ifdef DISABLE_INT
	local_irq_restore(flags);		// 禁用中断, 保证发送数据原子的执行
#endif
	//udelay(1);
	return ret;
}

/*
 * flag: 0->ctl0; 1->clt1
 * receive data from 485, return count is ok, -1 is timeout, -2 is other errors
 * created by wzhe, 10/16/2006
 */
int recv_data(char *buf, unsigned char num)
{
#ifdef DISABLE_INT
	unsigned long flags;
#endif
	int ret = 0;
	if (!buf)
		return -EINVAL;
#ifdef DISABLE_INT
	local_irq_save(flags);		// 禁用中断, 保证发送数据原子的执行
#endif
	// 做通道判断
	if (!(num & 0x80)) {
		// 超时等待
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_RXRDY)) {
			ret = NOTERMINAL;
			goto out;
		}
		// 判断标志位
		if(((uart_ctl0->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl0->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			*(volatile char *)buf = uart_ctl0->US_RHR;
 			uart_ctl0->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl0->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// 拷贝数据
		*(volatile char *)buf = uart_ctl0->US_RHR;
 	} else {
		// 超时等待
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_RXRDY)) {
			ret =  NOTERMINAL;
			goto out;
		}
		// 判断标志位
		if(((uart_ctl2->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl2->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			*(volatile char *)buf = uart_ctl2->US_RHR;
 			uart_ctl2->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// 拷贝数据
		*(volatile char *)buf = uart_ctl2->US_RHR;
	}
out:
#ifdef DISABLE_INT
	local_irq_restore(flags);		// 禁用中断, 保证发送数据原子的执行
#endif
	return ret;
}
#undef DISABLE_INT

