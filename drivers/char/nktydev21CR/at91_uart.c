/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�at91_uart.c
 * ժ    Ҫ�����ӷ��жϷ�ʽ�к�, ���Ӻ���������ӿ�
 *			 �ڷ��ͽ���ʱ����lock_irq_save, �����ж�
 *			 ����CR�Ĵ���д�ı�
 *			 ���Ӷ�term_ram�ṹ����³�Ա����
 *			 ��RTC����ת����һ���ļ���
 * 			 
 * ��ǰ�汾��2.3.1
 * ��    �ߣ�wjzhe
 * ������ڣ�2008��4��29��
 *
 * ȡ���汾��2.3
 * ԭ����  ��wjzhe
 * ������ڣ�2007��9��3��
 *
 * ȡ���汾��2.2
 * ԭ����  ��wjzhe
 * ������ڣ�2007��8��13��
 *
 * ȡ���汾��2.1 
 * ԭ����  ��wjzhe
 * ������ڣ�2007��6��6��
 *
 * ȡ���汾��2.0 
 * ԭ����  ��wjzhe
 * ������ڣ�2007��4��3��
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

#include "at91_rtc.h"
#include "data_arch.h"
#include "uart_dev.h"
#include "uart.h"
#include "deal_data.h"
#include "deal_purse.h"
#include "no_money_type.h"
#include "pdccfg.h"

#define FORBIDDOG
//#define FORBIDDOG_C

#define BAUDRATE 28800

#define LAPNUM 4
#define ONCERECVNO 5 

//#define CALLDOG
//#define DISABLE_INT
#undef DISABLE_INT

//#define DISCALL_INT

//#define DBGCALL

//#define U485INT

#ifndef MIN
#define	MIN(a,b)	(((a) <= (b)) ? (a) : (b))
#endif


/* ÿ�νе���ѭ��Ȧ�� */
static int lapnum;

// ����ָ��
/********������ض���***********/
/*static */AT91PS_USART uart_ctl0 = (AT91PS_USART) AT91C_VA_BASE_US0;
/*static */AT91PS_USART uart_ctl2 = (AT91PS_USART) AT91C_VA_BASE_US2;

/* ��¼�ն��������˻������ṹ�� */
struct record_info rcd_info;

/* ������������ˮ������ʣ��ռ� */
int space_remain = 0;

// �������ݻ�����ָ��
unsigned int fee[16];
acc_ram paccsw[MAXSWACC];
flow pflow[FLOWANUM];
int maxflowno = 0;
unsigned char pfbdseg[28] = {0};//add by duyy,2013.3.26
unsigned char ptmnum[16][16];//add by duyy,2013.3.26,timenum[���][�ն�]
term_tmsg term_time[7];//write by duyy, 2013.3.26
char feetable[16][64];//added by duyy, 2013.10.28,feetable[�ն�����][���ѿ�����]
char feenum = 0;//added by duyy, 2013.11.19,�������ȡ��ʽ��0��ʾ�ڿۣ�1��ʾ���
char forbidflag[16][64];//added by duyy, 2013.10.28,forbidflag[�ն�����][���ѿ�����]
/*���������Եİ汾�ű�����write by duyy, 2013.11.18*/
long managefee_flag = 0;
long forbid_flag = 0;
long timeseg_flag = 0;
int update_fee = 0;//���¹������ʾ��write by duyy, 2013.11.20
int update_frdflag = 0;//���½�ֹ�������ñ�־��ʾ��write by duyy, 2013.11.20
int update_frdseg = 0;//���½�ֹʱ����ʾ��write by duyy, 2013.11.20
unsigned char uptermdata[256] = {0};//��¼ʹ�õ��ն˻��ţ��������ݸ��£�write by duyy, 2013.11.20

term_ram * ptermram = NULL;
terminal *pterminal = NULL;
acc_ram *paccmain = NULL;
//flow *pflow = NULL;
#ifdef CONFIG_RECORD_CASHTERM
// �����¼���ɻ�״̬�ı���
const int cashterm_n = CASHBUFSZ;
int cashterm_ptr = 0;				// �洢ָ��
struct cash_status cashbuf[CASHBUFSZ];	// ����״̬�ռ�
#endif

#if 0
// ����PSAM����Ϣ
struct psam_info psaminfo;
#endif

// ������ˮ��¼ָ��
struct flow_info flowptr;

// ��¼ÿ�νкź�Ĳ�������ˮ����
int flow_sum;

static int total = 0;// ������ˮ����

// ��������״ָ̬��
static int net_status = 0;
// �����翨�����ѻ�ʹ�ñ�־
static int le_allow = 0;

// �������������ָ��
black_acc *pblack = NULL;// ֧��100,000��, ��СΪ879KB
struct black_info blkinfo;
// ����bad flow
//struct bad_flow badflow = {0};


#define LOCKUART 0

// ����������
#if 1//ndef U485INT
  #if LOCKUART
	static DEFINE_SPINLOCK(uart_lock);
  #else
	static DECLARE_MUTEX(uart_lock);
	//static struct semaphore uart_lock;
  #endif
#endif
//�豸��:	6	UART0
//			8	UART2

#define UART_MAJOR 22
#define UART0_IRQ 6
#ifdef U485INT
static int enable_usart2_int(void)
{
	uart_ctl2->US_IER = AT91C_US_TXRDY;
#if 0
	*UART2_IER = AT91C_US_TXRDY;		// usart2 INT
	*UART2_IDR = (!AT91C_US_TXRDY);
#endif
	return 0;
}

static int enable_usart0_int(void)
{
	uart_ctl0->US_IER = AT91C_US_TXRDY;
#if 0
	*UART0_IER = AT91C_US_TXRDY;		// usart0 INT
	*UART0_IDR = (!AT91C_US_TXRDY);
#endif
	return 0;
}
static int disable_usart_int(void)
{
	uart_ctl2->US_IDR = 0xFFFFFFFF;
	uart_ctl0->US_IDR = 0xFFFFFFFF;
	// ����USART�Ĵ���
	uart_ctl0->US_CR = AT91C_US_RSTSTA;
	uart_ctl0->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	uart_ctl0->US_BRGR = 0x82;
	uart_ctl0->US_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
	
	uart_ctl2->US_CR = AT91C_US_RSTSTA;
	uart_ctl2->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	uart_ctl2->US_BRGR = 0x82;	
	uart_ctl2->US_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;

#if 0
	*UART2_IER = 0;		// usart2 INT
	*UART2_IDR = 0xFFFFFFFF;
	*UART0_IER = 0;		// usart2 INT
	*UART0_IDR = 0xFFFFFFFF;
	*UART2_CR |= AT91C_US_RSTSTA;
	*UART2_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	*UART2_BRGR = 0x82;	
	*UART2_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
	
	*UART0_CR |= AT91C_US_RSTSTA;
	*UART0_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	*UART0_BRGR = 0x82;	
	*UART0_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
#endif
	return 0;
}
#endif

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
	// ��������ʱ����Ҫ����ͨ��, ���ԭ��ͼ
	if (!(num & 0x80)) {
		// ǰ��ͨ����UART0
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
		// ����ͨ����UART2
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
	// ���ͽ����������л�������ģʽ
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	udelay(1);
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	return ret;
}
// ֻ����һ�ֽ�, �ɹ�����0
int send_byte(char buf, unsigned char num)
{
	int ret = 0;
	//AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12;		// enable send data
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
	// ��������ʱ����Ҫ����ͨ��, ���ԭ��ͼ
	if (!(num & 0x80)) {
		// ǰ��ͨ����UART0
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
		// ����ͨ����UART2
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
	// ���ͽ����������л�������ģʽ
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	udelay(1);
	return ret;//0;modified by duyy,2012.9.10
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
		//sel_ch(num);
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY)) {
			ret = -1;
			goto err;
		}
	}
err:
	// ���ͽ����������л�������ģʽ
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
	// ��ͨ���ж�
	if (!(num & 0x80)) {
		// ��ʱ�ȴ�
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_RXRDY)) {
			ret = NOTERMINAL;
			goto out;
		}
		// �жϱ�־λ
		if(((uart_ctl0->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl0->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			*(volatile char *)buf = uart_ctl0->US_RHR;
 			uart_ctl0->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl0->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// ��������
		*(volatile char *)buf = uart_ctl0->US_RHR;
 	} else {
		// ��ʱ�ȴ�
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_RXRDY)) {
			ret =  NOTERMINAL;
			goto out;
		}
		// �жϱ�־λ
		if(((uart_ctl2->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl2->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			*(volatile char *)buf = uart_ctl2->US_RHR;
 			uart_ctl2->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// ��������
		*(volatile char *)buf = uart_ctl2->US_RHR;
	}
out:
	return ret;
}


/*
 * ���������ʣ����ˮ�ռ�, ����<0��ˮ����, 0����
 */
#if 1
static inline
#endif
int chk_space(void)
{
#if 0
	int head, tail;
	head = flowptr.head;
	tail = flowptr.tail;
	tail++;
	if (tail >= FLOWANUM)
		tail -= FLOWANUM;
	if (tail == head)
		return -1;
	return 0;
#else
	int tail = flowptr.tail;
	//int head = flowptr.head;
	int ret;
	if ((tail + ONCERECVNO) <= FLOWANUM) {// ��ONCERECVNOΪ5, ������ϵͳ����
		ret = FLOWANUM - tail;
	} else {
		ret = -1;
	}
	return ret;
#endif
}

#ifdef U485INT
#if 0
//UART�ж���Ӧ����
static irqreturn_t UART_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
//#define CALLDEBUG
#ifdef FORBIDDOG_C
	unsigned long dogreg;
#endif
	int n;
	static unsigned char tm[7];
	int i, ret, allow;
	unsigned char sendno;
	unsigned char rdata = 0, commd;
	term_ram *prterm;
	static unsigned long current_jif;
	disable_usart_int();
	current_jif = jiffies;
	if (paccmain == NULL || pterminal == NULL || ptermram == NULL) {
		printk("pointer is NULL!!!ERROR!\n");
		return IRQ_RETVAL(-1);
	}
	if (rcd_info.term_all == 0) {
#ifdef CALLDEBUG
		printk("there is no term!\n");
#endif
		goto out;
	}
	if (space_remain == 0) {//flash�ռ�ʣ��
#ifdef CALLDEBUG
		printk("there is no space room\n");
#endif
		goto out;
	}
	if (lapnum <= 0 || lapnum > 10) {
		n = LAPNUM;
	} else {
		n = lapnum;
	}
	// ÿһ�νк�ǰ��flow_sum, ��0, �Լ�¼������ˮ����
	flow_sum = 0;
	tm[0] = 0x20;
	read_time(&tm[1]);
#ifdef FORBIDDOG_C
	dogreg = AT91_SYS->ST_WDMR;
	AT91_SYS->ST_WDMR = AT91C_ST_EXTEN;
#endif
	while (n) {
		prterm = ptermram;
#ifdef CALLDOG
		AT91_SYS->ST_CR = AT91C_ST_WDRST;	/* Pat the watchdog */
#endif
		// ��һȦ
		for (i = 0; i < rcd_info.term_all; i++, prterm++) {
			if (chk_space() < 0) {
				printk("tail is %d, head is %d\n", flowptr.tail, flowptr.head);
				goto out;
			}
			//AT91_SYS->PIOC_CODR&=(!0x00001000);
			//AT91_SYS->PIOC_SODR|=0x00001000;
			sel_ch(prterm->term_no);// �κ������, �Ƚ�138����λ��λ
			sendno = prterm->term_no & 0x7F;
			prterm->add_verify = 0;
			prterm->dif_verify = 0;
			prterm->status = STATUSUNSET;
			// �������º�����ʱ��local_num | 0x80 ---> ע�������ת
			//printk("send addr %d, %d\n", sendno, prterm->term_no);
			ret = send_addr(sendno, prterm->pterm->local_num);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("send addr %d error: %d\n", sendno, ret);
#endif
				prterm->status = NOTERMINAL;
				//prterm++;
				continue;
			}
			// receive data, if pos number and no cmd then end
			// if pos number and cmd then exec cmd
			// else wrong!
			ret = recv_data((char *)&rdata, prterm->term_no);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("recv term %d error: %d\n", prterm->term_no, ret);
#endif
				prterm->status = ret;
				//prterm++;
				continue;
			}
			prterm->add_verify += rdata;
			prterm->dif_verify ^= rdata;
			//printk("recvno: %d and sendno: %d\n", rdata, sendno);
			if ((rdata & 0x7F) != sendno) {
#ifdef CALLDEBUG
				printk("recvno: %d but sendno: %d\n", rdata, sendno);
#endif
				prterm->status = NOCOM;
				//prterm++;
				continue;
			}
			if (!(rdata & 0x80)) {		// no command
				// �غ���ȷ, ��û������
				// �ڴ�Ҫ������������, ʵʱ�·�������
				if (prterm->key_flag == 0) {
					ret = purse_update_key(prterm);
					if (ret == 0) {
						prterm->key_flag = 1;
					} else {
#ifdef CALLDEBUG
						printk("update key failed\n");
#endif
					}
				}
				ret = purse_send_bkin(prterm, &blkinfo, pblack, prterm->black_flag);
				if (ret < 0) {
#ifdef CALLDEBUG
					printk("real-time send black list failed\n");
#endif
				}
				//prterm->black_flag = 1;
				prterm->status = TNORMAL;
				//prterm++;
				continue;
			}
			// has command, then receive cmd
			ret = recv_data((char *)&commd, prterm->term_no);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("recv cmd %d error: %d\n", prterm->pterm->local_num, ret);
#endif
				prterm->status = ret;
				//prterm++;
				continue;
			}
			//printk("%d has cmd, and cmd is %02x\n", prterm->term_no, commd);
			prterm->add_verify += commd;
			prterm->dif_verify ^= commd;
			// ����������������
			switch (commd) {
#ifdef TS11
			case SENDLEIDIFNO:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = recv_le_id(prterm, allow);
				break;
			case SENDRUNDATA:
				ret = send_run_data(prterm);
				break;
			case RECVNO:
				ret = ret_no_only(prterm);
				break;
			case RECVLEHEXFLOW:
				//space_remain--;
				ret = recv_leflow(prterm, 0, tm);
				break;
			case RECVLEBCDFLOW:
				//space_remain--;
				ret = recv_leflow(prterm, 1, tm);
				break;
			case RECVTAKEFLOW:
				//space_remain--;
				ret = recv_take_money(prterm, tm);
				break;
			case RECVDEPFLOW:
				//space_remain--;
				ret = recv_dep_money(prterm, tm);
				break;
			case NOCMD:
				break;
#endif/* TS11 */
			case PURSE_RECV_CARDNO:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = purse_recv_leid(prterm, allow);
				break;
			case PURSE_RECV_LEFLOW:
				ret = purse_recv_leflow(prterm, tm);
				break;
			case PURSE_REQ_PAR:
				prterm->black_flag = 0;
				prterm->key_flag = 0;
				memset(&prterm->blkinfo, 0, sizeof(prterm->blkinfo));
				ret = purse_send_conf(prterm, tm, &blkinfo);
#ifdef CALLDEBUG
				printk("purse_send conf return %d\n", ret);
#endif
				// �˴��ϵ�������ͽ���, ��ʱҪ�����·���ֵ
				if (!ret) {
#if 0
					mdelay(20);
					ret = purse_update_key(prterm);
					if (!ret) {
						mdelay(10);
						ret = purse_send_bkin(prterm, &blkinfo, pblack, prterm->black_flag);
						if (ret < 0) {
							printk("real-time send black list failed\n");
						}
					}
#endif
				} else
					printk("purse_send conf return %d\n", ret);
				break;
			case PURSE_REQ_TIME:
				ret = purse_send_time(prterm, tm);
				break;
			case PURSE_RMD_BKIN:
				ret = purse_recv_btno(prterm);
				break;
			case RECVPURSEFLOW:
				ret = purse_recv_flow(prterm);
				break;
			case PURSE_RECV_DEP:
				ret = purse_recv_ledep(prterm, tm);
				break;
			case PURSE_RECV_TAKE:
				ret = purse_recv_letake(prterm, tm);
				break;
			default:			// cmd is not recognised, so status is !
				prterm->status = -2;
#ifdef CALLDEBUG
				printk("cmd %02x is not recognised\n", commd);
				usart_delay(1);
#endif
				ret = -1;
				break;
			}
			if (prterm->status == STATUSUNSET)
				prterm->status = TNORMAL;
			//prterm++;
			udelay(ONEBYTETM);
			if (space_remain == 0) {
				total += flow_sum;
				goto out;
			}
		}
		n--;
	}
	total += flow_sum;
//////////////////////////////////=============
	//flowptr.tail = 0;
//////===================================
out:
#ifdef FORBIDDOG_C
	AT91_SYS->ST_WDMR = dogreg;
	AT91_SYS->ST_CR = AT91C_ST_WDRST;
#endif
	return IRQ_HANDLED;
}
#endif
#else
// UART 485 �кź���
static int uart_call(void)
{
//#define CALLDEBUG
#ifdef FORBIDDOG_C
	unsigned long dogreg;
#endif
	int n;
	static unsigned char tm[7];
	int i, ret, allow;
	unsigned char sendno;
	unsigned char rdata = 0, commd;
	term_ram *prterm;
	if (paccmain == NULL || pterminal == NULL || ptermram == NULL) {
		printk("pointer is NULL!!!ERROR!\n");
		return -1;
	}
	if (rcd_info.term_all == 0) {
#ifdef CALLDEBUG
		printk("there is no term!\n");
#endif
		goto out;
	}
	if (space_remain <= 0) {//flash�ռ�ʣ��, space_remain == 0
#ifdef CALLDEBUG
		printk("there is no space room\n");
#endif
		goto out;
	}
	if (lapnum <= 0 || lapnum > 10) {
		n = LAPNUM;
	} else {
		n = lapnum;
	}
	// ÿһ�νк�ǰ��flow_sum, ��0, �Լ�¼������ˮ����
	flow_sum = 0;
	tm[0] = 0x20;
	read_time(&tm[1]);
#ifdef FORBIDDOG_C
	dogreg = AT91_SYS->ST_WDMR;
	AT91_SYS->ST_WDMR = AT91C_ST_EXTEN;
#endif
	while (n) {
		prterm = ptermram;
#ifdef CALLDOG
		AT91_SYS->ST_CR = AT91C_ST_WDRST;	/* Pat the watchdog */
#endif
		// ��һȦ
		for (i = 0; i < rcd_info.term_all; i++, prterm++, udelay(ONEBYTETM)) {
			if (chk_space() < 0) {
				printk("tail is %d, head is %d\n", flowptr.tail, flowptr.head);
				goto out;
			}
			//AT91_SYS->PIOC_CODR&=(!0x00001000);
			//AT91_SYS->PIOC_SODR|=0x00001000;
			sel_ch(prterm->term_no);// �κ������, �Ƚ�138����λ��λ
			sendno = prterm->term_no & 0x7F;
			prterm->add_verify = 0;
			prterm->dif_verify = 0;
			prterm->status = STATUSUNSET;
			// �������º�����ʱ��local_num | 0x80 ---> ע�������ת
			//printk("send addr %d, %d\n", sendno, prterm->term_no);
			ret = send_addr(sendno, prterm->pterm->local_num);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("send addr %d error: %d\n", sendno, ret);
#endif
				prterm->status = NOTERMINAL;
				//prterm++;
				continue;
			}
			// receive data, if pos number and no cmd then end
			// if pos number and cmd then exec cmd
			// else wrong!
			ret = recv_data((char *)&rdata, prterm->term_no);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("recv term %d error: %d\n", prterm->term_no, ret);
#endif
				prterm->status = ret;
				//prterm++;
				continue;
			}
			prterm->add_verify += rdata;
			prterm->dif_verify ^= rdata;
			//printk("recvno: %d and sendno: %d\n", rdata, sendno);
			if ((rdata & 0x7F) != sendno) {
#ifdef CALLDEBUG
				printk("recvno: %d but sendno: %d\n", rdata, sendno);
#endif
				prterm->status = NOCOM;
				//prterm++;
				continue;
			}
			if (!(rdata & 0x80)) {		// no command
#if 0//def QHFB
#define TM1S (5 * 3600 + 0 * 60)
#define TM1E (20 * 3600 + 0 * 60)
#define TM2S (0 * 3600 + 0 * 60)
#define TM2E (0 * 3600 + 0 * 60)
#define TM3S (0 * 3600 + 0 * 60)
#define TM3E (0 * 3600 + 0 * 60)
#if 0
#define TM1S (6 * 3600 + 20 * 60)
#define TM1E (9 * 3600 + 30 * 60)
#define TM2S (10 * 3600 + 30 * 60)
#define TM2E (14 * 3600 + 0 * 60)
#define TM3S (16 * 3600 + 30 * 60)
#define TM3E (20 * 3600 + 0 * 60)
#endif
				static int itm;
				itm = BCD2BIN(tm[6]);
				itm += BCD2BIN(tm[5]) * 60;
				itm += BCD2BIN(tm[4]) * 3600;
				if ((itm >= TM1S && itm <= TM1E)
					|| (itm >= TM2S && itm <= TM2E)
					|| (itm >= TM3S && itm <= TM3E)) {
					prterm->status = TNORMAL;
					continue;	// ��ʱ���ڲ������κ�����
				}
#endif
				// �غ���ȷ, ��û������
				// �ڴ�Ҫ������������, ʵʱ�·�������
				if (prterm->key_flag == 0) {
					ret = purse_update_key(prterm);
					if (ret == 0) {
						prterm->key_flag = 1;
					} else {
#ifdef CALLDEBUG
						printk("update key failed\n");
#endif
					}
				}
				ret = purse_send_bkin(prterm, &blkinfo, pblack, prterm->black_flag);
				if (ret < 0) {
#ifdef CALLDEBUG
					printk("real-time send black list failed\n");
#endif
				}
				//���й���ѡ���ֹ���ѱ�־����ֹ����ʱ����Ϣ���£�write by duyy, 2013.11.20
				if(uptermdata[prterm->term_no] & 0x01){
					//printk("uptermdata[%d] is %x\n", prterm->term_no, uptermdata[prterm->term_no]);
					if (purse_update_magfeetable(prterm) < 0){
						printk("termno %d send magfee failed\n", prterm->term_no);
						continue;
					}
					uptermdata[prterm->term_no] &= 0xFE;//����Ѹ��º������־λ
				}
				if(uptermdata[prterm->term_no] & 0x02){
					//printk("uptermdata[%d] is %x\n", prterm->term_no, uptermdata[prterm->term_no]);
					if (purse_update_forbidflag(prterm) < 0){
						printk("termno %d send forbid flag failed\n", prterm->term_no);
						continue;
					}
					uptermdata[prterm->term_no] &= 0xFD;//��ֹ���ѱ�־���º������־λ
				}
				if(uptermdata[prterm->term_no] & 0x04){
					printk("uptermdata[%d] is %x\n", prterm->term_no, uptermdata[prterm->term_no]);
					if (purse_update_noconsumetime(prterm, pfbdseg) < 0){
						printk("termno %d send forbid seg failed\n", prterm->term_no);
						continue;
					}
					uptermdata[prterm->term_no] &= 0xFB;//��ֹ����ʱ�θ��º������־λ
					printk("after uptermdata[%d] is %x\n", prterm->term_no, uptermdata[prterm->term_no]);
				}
				//prterm->black_flag = 1;
				prterm->status = TNORMAL;
				//prterm++;
				continue;
			}
			// has command, then receive cmd
			ret = recv_data((char *)&commd, prterm->term_no);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("recv cmd %d error: %d\n", prterm->pterm->local_num, ret);
#endif
				prterm->status = ret;
				//prterm++;
				continue;
			}
			//printk("%d has cmd, and cmd is %02x\n", prterm->term_no, commd);
			prterm->add_verify += commd;
			prterm->dif_verify ^= commd;
			// ����������������
			switch (commd) {
#ifdef TS11
			case SENDLEIDIFNO:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = recv_le_id(prterm, allow);
				break;
			case SENDRUNDATA:
				ret = send_run_data(prterm);
				break;
			case RECVNO:
				ret = ret_no_only(prterm);
				break;
			case RECVLEHEXFLOW:
				//space_remain--;
				ret = recv_leflow(prterm, 0, tm);
				break;
			case RECVLEBCDFLOW:
				//space_remain--;
				ret = recv_leflow(prterm, 1, tm);
				break;
			case RECVTAKEFLOW:
				//space_remain--;
				ret = recv_take_money(prterm, tm);
				break;
			case RECVDEPFLOW:
				//space_remain--;
				ret = recv_dep_money(prterm, tm);
				break;
			case NOCMD:
				break;
#endif/* TS11 */
#ifdef QINGHUA
			/* ֧�ִ�СƱ���� */
			case SENDLETICKET:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = recv_le_id2(prterm, allow);
				break;
			case RECVLETICKET:
				ret = recv_leflow2(prterm, tm);
				break;
			/* �廪��Ҫ��TS11 POS�� */
			case SENDLEIDIFNO:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = recv_le_id(prterm, allow, tm);
				break;
			case SENDRUNDATA:
				ret = send_run_data(prterm);
				break;
			case RECVNO:
				ret = ret_no_only(prterm);
				break;
			case RECVTAKEFLOW:
				//space_remain--;
				ret = recv_take_money(prterm, tm);
				break;
			case RECVDEPFLOW:
				//space_remain--;
				ret = recv_dep_money(prterm, tm);
				break;
			case RECVLEHEXFLOW:
				//space_remain--;
				ret = recv_leflow(prterm, 0, tm);
				break;
			case RECVLEBCDFLOW:
				//space_remain--;
				ret = recv_leflow(prterm, 1, tm);
				break;
			case NOCMD:
				break;
#endif
			case PURSE_RECV_CARDNO:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = purse_recv_leid(prterm, allow, tm);
				break;
			case PURSE_RECV_CARDNO2:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = purse_recv_leid2(prterm, allow, tm);
				break;
			case PURSE_RECV_LEFLOW:
				ret = purse_recv_leflow(prterm, tm);
				break;
			case PURSE_RECV_LEFLOW2:
				ret = purse_recv_leflow2(prterm, tm);
				break;
			case PURSE_REQ_PAR:
				prterm->black_flag = 0;
				prterm->key_flag = 0;
				memset(&prterm->blkinfo, 0, sizeof(prterm->blkinfo));
				prterm->psam_trd_num = 0;	// new terminal
				ret = purse_send_conf(prterm, tm, &blkinfo);
#ifdef CALLDEBUG
				printk("purse_send conf return %d\n", ret);
#endif
				// �˴��ϵ�������ͽ���, ��ʱҪ�����·���ֵ
				if (!ret) {
#if 0
					mdelay(20);
					ret = purse_update_key(prterm);
					if (!ret) {
						mdelay(10);
						ret = purse_send_bkin(prterm, &blkinfo, pblack, prterm->black_flag);
						if (ret < 0) {
							printk("real-time send black list failed\n");
						}
					}
#endif
				} else
					printk("purse_send conf return %d\n", ret);
				break;
			case PURSE_REQ_TIME:
				ret = purse_send_time(prterm, tm);
				break;
			case PURSE_REQ_NCTM://write by duyy, 2013.5.20
				ret = purse_send_noconsumetime(prterm, pfbdseg);
				break;
			case PURSE_REQ_MAGFEE://write by duyy, 2013.11.19
				ret = purse_send_magfeetable(prterm);
				break;
			case PURSE_REQ_FORBIDTABLE://write by duyy, 2013.11.19
				ret = purse_send_forbidflag(prterm);
				break;
			case PURSE_RMD_BKIN:
				ret = purse_recv_btno(prterm);
				break;
			case RECVPURSEFLOW:
				ret = purse_recv_flow(prterm);
				break;
			case PURSE_RECV_DEP:
				ret = purse_recv_ledep(prterm, tm);
				break;
			case PURSE_RECV_TAKE:
				ret = purse_recv_letake(prterm, tm);
				break;
			case PURSE_RECV_GROUP:
				ret = purse_recv_gpflow(prterm, tm);
				break;
			default:			// cmd is not recognised, so status is !
				prterm->status = -2;
#ifdef CALLDEBUG
				printk("cmd %02x is not recognised\n", commd);
				usart_delay(1);
#endif
				ret = -1;
				break;
			}
			if (prterm->status == STATUSUNSET)
				prterm->status = TNORMAL;
			//prterm++;
			//udelay(ONEBYTETM);
			if (space_remain <= 0) {
				total += flow_sum;
				goto out;
			}
		}
		n--;
	}
	total += flow_sum;
out:
#ifdef FORBIDDOG_C
	AT91_SYS->ST_WDMR = dogreg;
	AT91_SYS->ST_CR = AT91C_ST_WDRST;
#endif
	return 0;
}

#endif


/************************************************************	
		�����������
*************************************************************/
static int uart_open(struct inode *inode, struct file *filp)
{
	// config PIOC
	// reset US2 status
	AT91_SYS->PMC_PCER = AT91C_ID_PIOC;

	AT91_SYS->PIOC_PER|=0x00001007;
	AT91_SYS->PIOC_PDR&=(!0x00001007);

	AT91_SYS->PIOC_OER|=0x00001007;
	AT91_SYS->PIOC_ODR&=(!0x00001007);

	AT91_SYS->PIOC_IFER&=(!0x00001007);
	AT91_SYS->PIOC_IFDR|=0x00001007;

	AT91_SYS->PIOC_SODR|=0x00001007;
	AT91_SYS->PIOC_CODR&=(!0x00001007);

	AT91_SYS->PIOC_IER&=(!0x00001007);
	AT91_SYS->PIOC_IDR|=0x00001007;

	AT91_SYS->PIOC_MDDR|=0x00001007;
	AT91_SYS->PIOC_MDER&=(!0x00001007);

	AT91_SYS->PIOC_PPUER|=0x00001007;
	AT91_SYS->PIOC_PPUDR&=(!0x00001007);

	AT91_SYS->PIOC_ASR&=(!0x00001007);
	AT91_SYS->PIOC_BSR&=(!0x00001007);

	AT91_SYS->PIOC_OWDR|=0x00001007;
	AT91_SYS->PIOC_OWER&=(!0x00001007);
/*
	AT91_SYS->PIOA_PDR|=0x00C60000;
	AT91_SYS->PIOA_PER&=(!0x00C60000);

	AT91_SYS->PIOA_ODR|=0x00C60000;
	AT91_SYS->PIOA_OER&=(!0x00C60000);

	AT91_SYS->PIOA_IFER&=(!0x00C60000);
	AT91_SYS->PIOA_IFDR|=0x00C60000;

	AT91_SYS->PIOA_CODR|=0x00C60000;
	AT91_SYS->PIOA_SODR&=(!0x00C60000);

	AT91_SYS->PIOA_IER&=(!0x00C60000);
	AT91_SYS->PIOA_IDR|=0x00C60000;

	AT91_SYS->PIOA_MDDR|=0x00C60000;
	AT91_SYS->PIOA_MDER&=(!0x00C60000);

	AT91_SYS->PIOA_PPUER|=0x00C60000;
	AT91_SYS->PIOA_PPUDR&=(!0x00C60000);

	AT91_SYS->PIOA_ASR|=0x00C60000;
	AT91_SYS->PIOA_BSR&=(!0x00C60000);

	AT91_SYS->PIOA_OWDR|=0x00C60000;
	AT91_SYS->PIOA_OWER&=(!0x00C60000);
*/
#if 1
	AT91_SYS->PIOA_PDR = AT91C_PA17_TXD0 | AT91C_PA18_RXD0;
	AT91_SYS->PIOA_PDR = AT91C_PA22_RXD2 | AT91C_PA23_TXD2;
#else
	AT91_CfgPIO_USART0();
	AT91_CfgPIO_USART2();
#endif

	AT91_SYS->PMC_PCER = 0x00000140;
	// ����USART�Ĵ���
	uart_ctl0->US_CR = AT91C_US_RSTSTA;
	uart_ctl0->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	uart_ctl0->US_BRGR = 0x82;
	uart_ctl0->US_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
	
	uart_ctl2->US_CR = AT91C_US_RSTSTA;
	uart_ctl2->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	uart_ctl2->US_BRGR = 0x82;	
	uart_ctl2->US_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
	// ����������ݳ�ʹ��
	total = 0;
	maxflowno = 0;
	flow_sum = 0;
	flowptr.head = flowptr.tail = 0;
	memset(&rcd_info, 0, sizeof(rcd_info));
	lapnum = LAPNUM;
	// ��Ϊ����״̬, wjzhe, 2010-12-10
	//AT91_SYS->PIOC_CODR &= (!0x00001000);
	//AT91_SYS->PIOC_SODR |= 0x00001000;
	return 0;
}

/*
 * UART io ���ƺ���, ��ͬ��cmd, ִ�в�ͬ������
 */
static int uart_ioctl(struct inode* inode, struct file* file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int i, flow_head, flow_tail;//, flow_total;
	terminal *ptem;
	term_ram *ptrm;
	unsigned char feedata[1025] = {0};//write by duyy, 2013.11.19

#ifdef FORBIDDOG
	unsigned long wtdreg;
#endif
	struct uart_ptr ptr;
	no_money_flow no_m_flow;
	money_flow m_flow, *pmflow = NULL;
	struct get_flow pget;
	long tabledata[3] = {0};
#if LOCKUART
	spin_lock(&uart_lock);
#else
	down(&uart_lock);
#endif/* end lockuart */

	switch(cmd)
	{
	case SETLAPNUM:		// ����һ�νк�Ȧ��
		lapnum = (int)arg;
		break;
	case INITUART:		// ��ʹ�����в���
		if (paccmain) {
			if ((rcd_info.account_main * sizeof(acc_ram)) >= (SZ_128K))
				vfree(paccmain);
			else
				kfree(paccmain);
			paccmain = NULL;
		}
		if ((ret = copy_from_user(&ptr, (struct uart_ptr *)arg, sizeof(ptr))) < 0)
			break;
		// ��ʹ�������ˮ��
		maxflowno = ptr.max_flow_no;
		// ��ʹ���˻���
		rcd_info.account_main = rcd_info.account_all = ptr.acc_main_all;
		rcd_info.account_sw = 0;
		// ���˻������128K, ����vmalloc����ռ�
		if ((rcd_info.account_main * sizeof(acc_ram)) >= (SZ_128K)) {
			paccmain = (acc_ram *)vmalloc(rcd_info.account_main * sizeof(acc_ram));
			if (paccmain == NULL) {
				printk("UART: init pacc main space failed!\n");
				ret = -ENOMEM;
				break;
			}
		} else {
			paccmain = (acc_ram *)kmalloc(rcd_info.account_main * sizeof(acc_ram), GFP_KERNEL);
			if (paccmain == NULL) {
				printk("UART: init pacc main space failed!\n");
				ret = -ENOMEM;
				break;
			}
		}
		// ��������
		if ((ret = copy_from_user(paccmain, ptr.paccmain,
			rcd_info.account_main * sizeof(acc_ram))) < 0) {
			printk("UART: copy account main failed!\n");
			break;
		}
		// ��ʹ�������
		for (i = 0; i < 16; i++) {
			fee[i] = ptr.fee[i];
		}
		// ��ʹ���ն˿�
		if (ptermram) {
			kfree(ptermram);
			ptermram = NULL;
		}
		if (pterminal) {
			kfree(pterminal);
			pterminal = NULL;
		}
		rcd_info.term_all = ptr.term_all;
		ptermram = (term_ram *)kmalloc(rcd_info.term_all * sizeof(term_ram), GFP_KERNEL);
		pterminal = (terminal *)kmalloc(rcd_info.term_all * sizeof(terminal), GFP_KERNEL);
		memset(pterminal, 0, rcd_info.term_all * sizeof(terminal));
		memset(ptermram, 0, rcd_info.term_all * sizeof(term_ram));
		if ((ret = copy_from_user(pterminal, ptr.pterm, rcd_info.term_all * sizeof(terminal))) < 0)
			break;
		ptem = pterminal;
		ptrm = ptermram;
		// ���terminal ram�ṹ��
		for (i = 0; i < rcd_info.term_all; i++) {
			ptrm->pterm = ptem;
			ptrm->term_no = ptem->local_num;
			ptrm->status = STATUSUNSET;
			ptrm->term_cnt = 0;
			ptrm->term_money = 0;
			ptrm->flow_flag = 0;
			ptem++;
			ptrm++;
		}
		flowptr.head = flowptr.tail = 0;
		flow_sum = 0;
		total = 0;
		// ������翨�����־
		le_allow = ptr.le_allow;
//		printk("le allow %d, %d\n", le_allow, ptr.le_allow);
		//net_status = ptr.pnet_status;
		if ((ret = get_user(net_status, (int *)ptr.pnet_status)) < 0)
			break;
		// ��ʹ��������
		memcpy(&blkinfo, &ptr.blkinfo, sizeof(blkinfo));
		if (pblack) {
			vfree(pblack);
			pblack = NULL;
		}
		pblack = (black_acc *)vmalloc(MAXBACC * sizeof(black_acc));
		if (pblack == NULL) {
			printk("create pblack failed: no memory\n");
			ret = -ENOMEM;
			break;
		}
		if (blkinfo.count > MAXBACC) {
			printk("black count over MAXBACC\n");
			ret = -1;
			break;
		}
		ret = copy_from_user(pblack, ptr.pbacc, sizeof(black_acc) * blkinfo.count);
		//blkinfo.count = 0;	// blk count = 0, for test
		break;
	case SETFORBIDTIME://��ʼ���ն˽�ֹ����ʱ��,added by duyy, 2013.3.26
		if ((ret = copy_from_user(pfbdseg, (unsigned char*)arg, sizeof(pfbdseg))) < 0)
			break;
	#if 0
		for (i = 0; i < 28; i++){
			printk("kernel ptermtm[%d]:%d\n", i, pfbdseg[i]);
		}
	#endif
		term_time[0].begin = pfbdseg[1] * 3600 + pfbdseg[0] * 60;
		term_time[0].end = pfbdseg[3] * 3600 + pfbdseg[2] * 60;
		term_time[1].begin = pfbdseg[5] * 3600 + pfbdseg[4] * 60;
		term_time[1].end = pfbdseg[7] * 3600 + pfbdseg[6] * 60;
		term_time[2].begin = pfbdseg[9] * 3600 + pfbdseg[8] * 60;
		term_time[2].end = pfbdseg[11] * 3600 + pfbdseg[10] * 60;
		term_time[3].begin = pfbdseg[13] * 3600 + pfbdseg[12] * 60;
		term_time[3].end = pfbdseg[15] * 3600 + pfbdseg[14] * 60;
		term_time[4].begin = pfbdseg[17] * 3600 + pfbdseg[16] * 60;
		term_time[4].end = pfbdseg[19] * 3600 + pfbdseg[18] * 60;
		term_time[5].begin = pfbdseg[21] * 3600 + pfbdseg[20] * 60;
		term_time[5].end = pfbdseg[23] * 3600 + pfbdseg[22] * 60;
		term_time[6].begin = pfbdseg[25] * 3600 + pfbdseg[24] * 60;
		term_time[6].end = pfbdseg[27] * 3600 + pfbdseg[26] * 60;
#if 0
	printk("kernel time 1: begin time=%u, end time=%u\n", term_time[0].begin, term_time[0].end);
	printk("kernel time 2: begin time=%u, end time=%u\n", term_time[1].begin, term_time[1].end);
	printk("kernel time 3: begin time=%u, end time=%u\n", term_time[2].begin, term_time[2].end);
	printk("kernel time 4: begin time=%u, end time=%u\n", term_time[3].begin, term_time[3].end);
#endif
		break;
	case SETTIMENUM:		// ������ݡ��ն�����ƥ��ʱ�����,added by duyy, 2013.3.26
		//bug,ԭ����һ��if�ж���䣬С�ڣ���break�����ڣ���������½��У�����ʹ�ѻ����ܶ�������,modified by duyy, 2013.7.18
		ret = copy_from_user(ptmnum, (unsigned char*)arg, sizeof(ptmnum));
		break;
	case SETFEETABLE:		// �������ѿ�����~�ն�����~�����ϵ����,added by duyy, 2013.10.28
		ret = copy_from_user(feedata, (unsigned char*)arg, sizeof(feedata));
		memset(feetable, 0, sizeof(feetable));
		memcpy(feetable, feedata, sizeof(feetable));
		feenum = feedata[1024];
		break;
	case SETFORBIDFLAG:		// �������ѿ�����~�ն�����~��ֹ����ʱ�����ñ�־��,added by duyy, 2013.10.28
		memset(forbidflag, 0, sizeof(forbidflag));
		ret = copy_from_user(forbidflag, (unsigned char*)arg, sizeof(forbidflag));
		break;
	case GETTABLEFALG:	//��ȡ�������汾�ţ�write by duyy, 2013.11.19
		tabledata[0] = managefee_flag;
		tabledata[1] = forbid_flag;
		tabledata[2] = timeseg_flag;
		ret = copy_to_user((long *)arg, tabledata, sizeof(tabledata));
		break;
	case UPDATEFEE:		// ����������и��£�write by duyy, 2013.11.20
		update_fee = (int)arg;
		for (i = 0; i < 256; i++){
			uptermdata[i] |= (1 << 0);//bit0��λ����ʾ�����Ҫ����
		}
		break;
	case UPDATEFLAG:		//��ֹ�������ñ�־�����и��£�write by duyy, 2013.11.20
		update_frdflag = (int)arg;
		for (i = 0; i < 256; i++){
			uptermdata[i] |= (1 << 1);//bit0��λ����ʾ����Ҫ����
		}

		break;
	case UPDATESEG:		// ��ֹ����ʱ�������и��£�write by duyy, 2013.11.20
		update_frdseg = (int)arg;
		for (i = 0; i < 256; i++){
			uptermdata[i] |= (1 << 2);//bit0��λ����ʾ����Ҫ����
		}
		break;
	case SETLEALLOW:		// ���ù�翨�ܷ����ʱ����
		ret = copy_from_user(&le_allow, (void *)arg, sizeof(le_allow));
		break;
	case SETRTCTIME:		// ����RTC
		//copy_from_user(&tm, , sizeof(tm));
		ret = set_RTC_time((struct rtc_time *)arg, 1);
		break;
	case GETRTCTIME:		// ��ȡRTC
		ret = read_RTC_time((struct rtc_time *)arg, 1);
		//copy_to_user(, &tm2, sizeof(tm2));
		break;
	case START_485:			// 485�к�, ��SEM�������ݱ���
#ifndef U485INT
#ifdef DISCALL_INT
		local_irq_save(wtdreg);		// �����ж�
#endif
#warning "485 not use int mode!"

		ret = uart_call();

#ifdef DISCALL_INT
		local_irq_restore(wtdreg);		// �����ж�, ��֤��������ԭ�ӵ�ִ��
#endif
#endif
#ifdef U485INT
#warning "485 use int mode!"
		enable_usart0_int();			// ���������ж�---->uart0
#endif
		break;
	case SETREMAIN:			// ����ϵͳʣ��洢�ռ�
		ret = copy_from_user(&space_remain, (int *)arg, sizeof(space_remain));
		if (ret < 0)
			break;
		//space_remain = *(volatile int *)arg;	// ��
#ifdef DEBUG
		if (space_remain > 147250) {
			printk("space remain is too large: %d\n", space_remain);
			space_remain = 0;
			break;
		}
#endif
		if (space_remain < 0) {
			space_remain = 0;
		}
		break;
	case GETFLOWSUM:		// ��ȡһ�νкź��������ˮ����
		ret = put_user(flow_sum, (int *)arg);
		break;
	case GETFLOWNO:			// ��ȡ���������ˮ��
		ret = put_user(maxflowno, (int *)arg);
		break;
	case ADDACCSW:			
		// �����������ֽ���ˮ
		if ((ret = copy_from_user(&no_m_flow, (no_money_flow *)arg,
			sizeof(no_money_flow))) < 0) {
			printk("UART: copy from user no money flow failed\n");
			break;
		}
		// �������ݴ���
		if ((ret = deal_no_money(&no_m_flow, paccmain, paccsw, &rcd_info)) < 0) {
			printk("UART: deal no money flow error\n");
		}
		//ret = 0;
		break;
	case CHGACC:
		// �ȿ�������ˮ����
		ret = copy_from_user(&i, (int *)arg, sizeof(int));
		if (ret < 0) {
			printk("UART: copy from user money flow count failed\n");
			break;
		}
		if (i <= 0) {
			printk("UART: money flow count below zero\n");
			break;
		}
		arg += sizeof(int);
		// �ٵõ���ˮָ��
		ret = copy_from_user(&pmflow, (money_flow *)arg, sizeof(pmflow));
		if (ret < 0) {
			printk("UART: copy from user money flow ptr failed\n");
			break;
		}
		// ˳�δ���
		while (i) {
			// ���������ֽ���ˮ
			if ((ret = copy_from_user(&m_flow, (money_flow *)pmflow,
				sizeof(money_flow))) < 0) {
				printk("UART: copy from user money flow failed\n");
				break;
			}
			// �������ݴ���
			if ((ret = deal_money(&m_flow, paccmain, paccsw, &rcd_info)) < 0) {
				printk("UART: deal money flow error\n");
			}
			pmflow += 1;
			i--;
		}
		break;
	case GETFLOWPTR:// ����, �õ��ڲ���ˮָ��
		printk("not support get flow ptr\n");
		ret = -1;
		break;
	case SETFLOWPTR:// ����, �����ڲ���ˮָ��
		printk("not support set flow ptr\n");
		ret = -1;
		break;
	case GETFLOW:// ȡ�ýкź����ˮ, һ��cnt��
		if ((ret = copy_from_user(&pget, (struct get_flow *)arg,
			sizeof(pget))) < 0) {
			break;
		}
		flow_head = flowptr.head;
		flow_tail = flowptr.tail;
		// ������ѭ���洢, ������ˮ����Ϊflow_tail
		// ���Ի�����±�����
		pget.cnt = MIN(pget.cnt, flow_tail);
//		printk("user get flow %d, no. from %d to %d\n", pget.cnt,
//			pflow[0].flow_num, pflow[flowptr.tail - 1].flow_num);
		ret = copy_to_user(pget.pflow, pflow, sizeof(flow) * pget.cnt);
		if (ret < 0)
			break;
		ret = copy_to_user((void *)(arg + sizeof(flow *)), &pget.cnt, sizeof(pget.cnt));
		break;
	case GETACCINFO:// ����˻�����Ϣ
		if (rcd_info.account_all != rcd_info.account_main + rcd_info.account_sw) {
			printk("UART: acc_all != acc_main + acc_sw: %d, %d, %d\n",
				rcd_info.account_all, rcd_info.account_main, rcd_info.account_sw);
			rcd_info.account_all = rcd_info.account_main + rcd_info.account_sw;
		}
		if ((ret = copy_to_user((struct record_info *)arg,
			&rcd_info, sizeof(rcd_info))) < 0) {
			//break;
		}
		break;
	case GETACCMAIN:// �õ����˻���
		ret = 0;
		if (rcd_info.account_main == 0)
			break;
		if ((ret = copy_to_user((acc_ram *)arg, paccmain,
			sizeof(acc_ram) * rcd_info.account_main)) < 0)
			break;
		// ��ȡ�ʻ���Ϣ���ʻ��ռ����
		if ((rcd_info.account_main * sizeof(acc_ram)) > (SZ_128K))
			vfree(paccmain);
		else
			kfree(paccmain);
		paccmain = NULL;
		break;
	case GETACCSW:// �õ����˻���
		ret = 0;
		if (rcd_info.account_sw == 0)
			break;
		ret = copy_to_user((acc_ram *)arg, paccsw,
			sizeof(acc_ram) * rcd_info.account_sw);
		break;
	case SETNETSTATUS:// ��������״̬
		ret = get_user(net_status, (int *)arg);
		break;
	case BCT_BLACKLIST:// �㲥������
		// ������ʱ��ܳ�, ��Ҫ�رտ��Ź�
		// save watch dog reg
#ifdef FORBIDDOG
		wtdreg = AT91_SYS->ST_WDMR;
		AT91_SYS->ST_WDMR = AT91C_ST_EXTEN;
#endif
		if (!arg) {
			ret = purse_broadcast_blk(pblack, &blkinfo, ptermram, rcd_info.term_all);
		} else {
			struct bct_data bctdata;
			term_ram *ptrm;
			black_acc *pbacc;
			int i;
			// ����Ҫ��Ϣ�������ں���
			ret = copy_from_user(&bctdata, (void *)arg, sizeof(bctdata));
			if (ret < 0)
				break;
			// �������������ݴ���ǰ�жϺ������������Ƿ�Ϊ0
			if (bctdata.info.count == 0) {
				break;
			}
			// ����������б�ռ�
			if (bctdata.info.count * sizeof(black_acc) >= SZ_128K) {
				pbacc = (black_acc *)vmalloc(bctdata.info.count * sizeof(black_acc));
				if (pbacc == NULL) {
					ret = -ENOMEM;
					break;
				}
			} else {
				pbacc = (black_acc *)kmalloc(bctdata.info.count * sizeof(black_acc), GFP_KERNEL);
				if (pbacc == NULL) {
					ret = -ENOMEM;
					break;
				}
			}
			// ����������
			ret = copy_from_user(pbacc, bctdata.pbacc, bctdata.info.count * sizeof(black_acc));
			if (ret < 0)
				break;
			// ����term_ram
			ptrm = (term_ram *)kmalloc(bctdata.term_cnt * sizeof(term_ram), GFP_KERNEL);
			if (ptrm == NULL) {
				ret = -ENOMEM;
				break;
			}
			memset(ptrm, 0, bctdata.term_cnt * sizeof(term_ram));
			for (i = 0; i < bctdata.term_cnt; i++) {
				ptrm[i].pterm = (terminal *)kmalloc(bctdata.term_cnt * sizeof(terminal), GFP_KERNEL);
				if (ptrm[i].pterm == NULL) {
					ret = -ENOMEM;
					break;
				}
				memset(ptrm[i].pterm, 0, sizeof(terminal));
				ret = copy_from_user(ptrm[i].pterm, &bctdata.pterm[i], sizeof(terminal));
				if (ret < 0) {
					break;
				}
				ptrm[i].term_no = bctdata.pterm[i].local_num;
				//printk("ptrm[%d]: no: %d\n", i, ptrm[i].term_no);
			}
			// �㲥������
			ret = purse_broadcast_blk(pbacc, &bctdata.info, ptrm, bctdata.term_cnt);
			if (ret < 0)
				printk("broadcast blk failed\n");
			// �ͷ��ڴ�
			if (bctdata.info.count * sizeof(black_acc) >= SZ_128K) {
				vfree(pbacc);
			} else {
				kfree(pbacc);
			}
			for (i = 0; i < bctdata.term_cnt; i++) {
				kfree(ptrm[i].pterm);
			}
			kfree(ptrm);
		}
#ifdef FORBIDDOG
		AT91_SYS->ST_WDMR = wtdreg;
		AT91_SYS->ST_CR = AT91C_ST_WDRST;
#endif
		break;
	case GETFLOWTAIL:
		ret = copy_to_user((int *)arg, &flowptr.tail, sizeof(flowptr.tail));
		break;
	case CLEARFLOWBUF:
		flowptr.tail = 0;
		break;
	case GETFLOWTOTAL:
		ret = copy_to_user((int *)arg, &total, sizeof(total));
		break;
#ifdef CONFIG_RECORD_CASHTERM
	case GETCASHBUF:
		//if (cashterm_ptr) {
			ret = copy_to_user((void *)arg, cashbuf,
				sizeof(struct cash_status) * CASHBUFSZ);
			if (ret < 0) {
				printk("Uart ioctl GETCASHBUF copy_to_user error\n");
			} else {
				ret = CASHBUFSZ;//cashterm_ptr;//modified by duyy, 2013.4.7
			}
		#if 0//modified by duyy, 2013.4.7
			//write by duyy, 2012.3.14
			for(i = 0; i < cashterm_ptr; i++){
				cashbuf[i].termno = 0;
				cashbuf[i].status = 0;
				cashbuf[i].feature = 0; 
				cashbuf[i].accno = 0;
				cashbuf[i].cardno = 0;
				cashbuf[i].money = 0;
				cashbuf[i].consume = 0;
				cashbuf[i].managefee = 0;
				cashbuf[i].term_money = 0;
			}
			cashterm_ptr = 0;
		#endif
		//}
		break;
#endif
	default:
		printk("485 has no cmd %02x\n", cmd);
		ret = -1;
		break;
	}
//out:
#if LOCKUART
	spin_unlock(&uart_lock);
#else
	up(&uart_lock);
#endif/* end lockuart */
	return ret;
}

/*
 * д��FRAM������ˮ, ֻ����ϵͳ��ʹ��ʱ
 */
static int uart_write(struct file *file, const char *buff, size_t count, loff_t *offp)
{
	return -1;
#if 0
	// д��FRAM������ˮ
	// ֻ��������ˮ����ʹ����ʱ���ô˺���
	int cnt;
	if (count % sizeof(flow) != 0 || buff == NULL) {
		return -EINVAL;
	}
	cnt =  count / sizeof(flow);
	if (cnt >= FLOWPERPAGE) {
		printk("UART Write: cnt is too large\n");
		return -EINVAL;
	}
	if (cnt == 0) {
		flowptr.tail = 0;
		total = 0;
		return 0;
	}
	if (copy_from_user(pflow, buff, count) < 0) {
		printk("UART: copy flow from user failed\n");
		return -1;
	}
	flowptr.tail = cnt;
	total = cnt;
	return count;
#endif
}

/*
 * �������й����ж�ȡ�ն���Ϣ
 */
static int uart_read(struct file *file, char *buff, size_t count, loff_t *offp)
{
	term_info *pterm_info;
	int i;
	term_ram *ptrm = ptermram;
	unsigned int cnt;
	if (count % sizeof(term_info) != 0 || buff == NULL)
		return -EINVAL;
	if (count == 0 || ptrm == NULL) {
		return 0;
	}
	pterm_info = (term_info *)kmalloc(count, GFP_KERNEL);
	if (pterm_info == NULL) {
		printk("UART: cannot init pterm_info\n");
		return -1;
	}
	cnt = count / sizeof(term_info);
	for (i = 0; i < cnt; i++) {
		pterm_info[i].term_no = ptrm->term_no;
		pterm_info[i].status = ptrm->status;
		pterm_info[i].money = ptrm->term_money;
		if (ptrm->pterm->param.term_type & (1 << CASH_TERMINAL_FLAG)) {
			pterm_info[i].flag = 1;
		} else {
			pterm_info[i].flag = 0;
		}
		ptrm++;
	}
	if (copy_to_user(buff, pterm_info, count) < 0) {
		printk("UART: copy term_info failed\n");
		kfree(pterm_info);
		return -1;
	}
	kfree(pterm_info);
	return 0;
}
/*
 * Close the uart_485
 */
static int uart_close(struct inode *inode, struct file *file)
{
	return 0;
}
struct file_operations uart_fops = {
	.owner = THIS_MODULE,
	.open = uart_open,
	.ioctl = uart_ioctl,
//	int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
	.write = uart_write,
	.read = uart_read,
	.release = uart_close,
};

static __init int uart_init(void)
{
	int res;
	res = register_chrdev(UART_MAJOR, "uart", &uart_fops);
	if (res < 0) {
		return res;
	}
#ifdef U485INT
	res = request_irq(UART0_IRQ, UART_irq_handler, 0, "uart", NULL);
	if (res < 0) {
		printk("AT91 UART IRQ requeset failed!\n");
		return res;
	}
	enable_irq(UART0_IRQ);
#endif
#if 0
	init_MUTEX(&uart_lock);
#endif
#ifdef UARTINTMODE
	if (request_irq(AT91C_ID_US0, us0_interrupt, 0, "us0", NULL))
		return -EBUSY;
	if (request_irq(AT91C_ID_US2, us2_interrupt, 0, "us2", NULL))
		return -EBUSY;
#if PRIORITY
	AT91_SYS->AIC_SMR[AT91C_ID_US0] |= (PRIORITY & 0x7);/* irq ���ȼ� */
	AT91_SYS->AIC_SMR[AT91C_ID_US2] |= (PRIORITY & 0x7);/* irq ���ȼ� */
#endif
#endif
	printk("AT91 UART driver v2.0\n");
	return 0;
}

static __exit void uart_exit(void)
{
	//int ret;
#ifdef U485INT
	disable_irq(UART0_IRQ);
	free_irq(UART0_IRQ, NULL);
#endif
	unregister_chrdev(UART_MAJOR, "uart");				//ע���豸
}

module_init(uart_init);
module_exit(uart_exit);

MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("USART 485 driver for NKTY AT91RM9200")

