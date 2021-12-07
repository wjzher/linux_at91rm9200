#include "uart_dev.h"
#include "data_arch.h"
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
	int loop_cntr = 750;
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

