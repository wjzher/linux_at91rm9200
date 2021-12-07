#include "uart_dev.h"
#include "uart.h"
//AT91PS_USART ctl0 = (AT91PS_USART) AT91C_VA_BASE_US0;
//AT91PS_USART ctl2 = (AT91PS_USART) AT91C_VA_BASE_US2;

int check_BCD(unsigned char *buf, size_t cnt)
{
	int i;
	unsigned char tmp;
	for (i = 0; i < cnt; i++) {
		tmp = *buf & 0xF;
		if (tmp > 9)
			return FALSE;
		tmp = *buf >> 4;
		if (tmp > 9)
			return FALSE;
		buf++;
	}
	return TRUE;
}

/*
 * Poll the uart status register until the specified bit is set.
 * Returns 0 if timed out (750 usec)
 */
short uart_poll(volatile unsigned int * reg, unsigned long bit)
{
	int loop_cntr = 2000;
	unsigned int status;
	do {
		udelay(1);
		status = *reg & bit;
	} while ((status == 0) && (--loop_cntr > 0));
	return (loop_cntr > 0);
}
/*******************************************
 *	选择485接收端口
********************************************/
static void channel_select(unsigned char channel_number)		
{
	switch(channel_number)
	{
	case 0:
		AT91_SYS->PIOC_CODR|=0x00000007;
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_CODR|=0x00000007;
		break;
	case 1:
		AT91_SYS->PIOC_CODR|=0x00000007;
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_CODR&=(!0x00000001);
		AT91_SYS->PIOC_SODR|=0x00000001;
		break;
	case 2:
		AT91_SYS->PIOC_CODR|=0x00000007;
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_CODR&=(!0x00000002);
		AT91_SYS->PIOC_SODR|=0x00000002;
		break;
	case 3:
		AT91_SYS->PIOC_CODR|=0x00000007;
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_CODR&=(!0x00000003);
		AT91_SYS->PIOC_SODR|=0x00000003;
		break;
	case 4:
		AT91_SYS->PIOC_CODR|=0x00000007;
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_CODR&=(!0x0000004);
		AT91_SYS->PIOC_SODR|=0x00000004;
		break;
	case 5:
		AT91_SYS->PIOC_CODR|=0x00000007;
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_CODR&=(!0x00000005);
		AT91_SYS->PIOC_SODR|=0x00000005;
		break;
	case 6:
		AT91_SYS->PIOC_CODR|=0x00000007;
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_CODR&=(!0x00000006);
		AT91_SYS->PIOC_SODR|=0x00000006;
		break;
	case 7:
		AT91_SYS->PIOC_CODR|=0x00000007;
		AT91_SYS->PIOC_SODR&=(!0x00000007);
		AT91_SYS->PIOC_CODR&=(!0x00000007);
		AT91_SYS->PIOC_SODR|=0x00000007;
		break;
	}
	return;
}

int sel_ch(unsigned char machine_no)
{
	if(/*(0<=machine_no)&&*/(machine_no<32))	
	{
		channel_select(0);
	}
	if((32<=machine_no)&&(machine_no<64))	
	{
		channel_select(1);
	}
	if((64<=machine_no)&&(machine_no<96))	
	{
		channel_select(2);		////
	}
	if((96<=machine_no)&&(machine_no<128))	
	{
		channel_select(3);		
	}
	if((128<=machine_no)&&(machine_no<160))	
	{
		channel_select(4);
	}
	if((160<=machine_no)&&(machine_no<192))	
	{
		channel_select(5);
	}
	if((192<=machine_no)&&(machine_no<224))
	{
		channel_select(6);
	}
	if((224<=machine_no)/*&&(machine_no<256)*/)
	{
		channel_select(7);
	}
	return 0;
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
		// 前四通道用UART0
#if 0
		/* us_cr is a read only register */
		uart_ctl0->US_CR &= !(AT91C_US_SENDA);
#endif
		for (i = 0; i < count; i++) {
			uart_ctl0->US_THR = *buf++;
			if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY)) {
				ret = -1;
				goto out;
			}
		}
	} else {
		// 后四通道用UART2
#if 0
		/* us_cr is a read only register */
		uart_ctl0->US_CR &= !(AT91C_US_SENDA);
#endif
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
#ifdef RS485_REV
	udelay(1);
#endif
	// 发送结束后马上切换到接收模式
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	udelay(1);
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	return ret;
}
// 只发送一字节, 成功返回0
int send_byte(char buf, unsigned char num)
{
	int ret = 0;
	//AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12;		// enable send data
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
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
#ifdef RS485_REV
	udelay(1);
#endif
	// 发送结束后马上切换到接收模式
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	udelay(1);
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
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
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
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY)) {
			ret = -1;
			goto err;
		}
	}
err:
	// 发送结束后马上切换到接收模式
#ifdef RS485_REV
	udelay(1);
#endif
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	udelay(1);
	return ret;
}

/*
 * flag: 0->ctl0; 1->clt1
 * receive data from 485, return count is ok, -1 is timeout, -2 is other errors
 * created by wzhe, 10/16/2006
 */
int recv_data(char *buf, unsigned char num)
{
	int ret = 0;
	if (!buf)
		return -EINVAL;
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
	return ret;
}



