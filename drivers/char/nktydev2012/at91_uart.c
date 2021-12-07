/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�at91_uart.c
 * ժ    Ҫ�����ӷ��жϷ�ʽ�к�, ���Ӻ����������ӿ�
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
 * ��    �ߣ�wjzhe
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
#include <linux/time.h>

#include <linux/rtc.h>
#include <asm/uaccess.h>
#include <asm/rtc.h>

#include <asm/mach/time.h>

#include "at91_rtc.h"
#include "data_arch.h"
#include "uart_dev.h"
#include "uart.h"
#include "deal_data.h"
#include "deal_purse.h"
#include "no_money_type.h"

#define FORBIDDOG
//#define FORBIDDOG_C

#define BAUDRATE 28800

#define LAPNUM 4

#undef DISABLE_INT


#ifndef MIN
#define	MIN(a,b)	(((a) <= (b)) ? (a) : (b))
#endif


/* ÿ�νе���ѭ��Ȧ�� */
static int lapnum;

// ����ָ��
/********������ض���***********/
/*static */
AT91PS_USART uart_ctl0 = (AT91PS_USART) AT91C_VA_BASE_US0;
/*static */
AT91PS_USART uart_ctl2 = (AT91PS_USART) AT91C_VA_BASE_US2;

/* ��¼�ն��������˻������ṹ�� */
struct record_info rcd_info;

/* ������������ˮ������ʣ��ռ� */
int space_remain = 0;

// �������ݻ�����ָ��
unsigned int fee[16];

// �������˻��⻻Ϊhash
//acc_ram paccsw[MAXSWACC];
struct hashtab *hash_accsw;
static u32 def_hash_value(struct hashtab *h, void *key)
{
	unsigned int i;
	//memcpy(&k, key, sizeof(k));
	i = (unsigned int)key;
	//val = (((k << 17) | (k >> 15)) ^ k) + (k * 17) + (k * 13 * 29);
    i += ~(i << 9);
    i ^=  ((i >> 14) | (i << 18)); /* >>> */
    i +=  (i << 4);
    i ^=  ((i >> 10) | (i << 22)); /* >>> */
	return i % h->size;
#if 0
	unsigned int val, k;
	//memcpy(&k, key, sizeof(k));
	k = (unsigned int)key;
	val = (((k << 17) | (k >> 15)) ^ k) + (k * 17) + (k * 13 * 29);
	return val % h->size;
#endif
}

static int def_hash_cmp(struct hashtab *h, void *key1, void *key2)
{
	unsigned int k1, k2;
	k1 = (unsigned int)key1;
	k2 = (unsigned int)key2;
	return !(k1 == k2);
	//return memcmp(key1, key2, sizeof(int));
}


flow pflow[FLOWANUM];
int maxflowno = 0;
unsigned char pfbdseg[28] = {0};//add by duyy,2013.5.8
unsigned char ptmnum[16][16];//add by duyy,2013.5.8,timenum[����][�ն�]
term_tmsg term_time[7];//write by duyy, 2013.5.8
term_ram * ptermram = NULL;
terminal *pterminal = NULL;
acc_ram *paccmain = NULL;

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
#ifdef CONFIG_RECORD_CASHTERM
// �����¼���ɻ�״̬�ı���
const int cashterm_n = CASHBUFSZ;
int cashterm_ptr;				// �洢ָ��
struct cash_status cashbuf[CASHBUFSZ];	// ����״̬�ռ�
#endif

#ifdef CONFIG_UART_V2
// �����������ѿ���
user_cfg usrcfg;
// ȫ�ֱ���֧��
int current_id;		// ��ǰ�ʹ�ID 0~3

#endif
// ����
static DECLARE_MUTEX(uart_lock);

#define UART_MAJOR 22
#if 0
#define UART0_IRQ 6
#endif
/*
 * Set RTC time
 * when flag = 1, use copy_from_user
 * if success, return 0, else return error number
 * version 1.0
 */
int set_RTC_time(struct rtc_time *tm, int flag)
{
	int ret = 0;
	struct rtc_time m_tm;
	int cpy = 1;
	if (flag) {
		cpy = copy_from_user(&m_tm, tm, sizeof(m_tm));
	} else {
		m_tm = *tm;
		cpy = 0;
	}
	if (cpy) {
		ret = -EFAULT;
	} else {
		int m_year = m_tm.tm_year + 1900;
		if (m_year < EPOCH
			|| (unsigned) m_tm.tm_mon >= 12
			|| m_tm.tm_mday <1
			|| m_tm.tm_mday > (days_in_mo[m_tm.tm_mon] + (m_tm.tm_mon == 1 && is_leap(m_year)))
			|| (unsigned) m_tm.tm_hour >= 24
			|| (unsigned) m_tm.tm_min >= 60
			|| (unsigned) m_tm.tm_sec >= 60)
			ret = -EINVAL;
		else
		{
			AT91_SYS->RTC_CR |= (AT91C_RTC_UPDCAL | AT91C_RTC_UPDTIM);	//set register -> can update time and date
			while ((AT91_SYS->RTC_SR & 0x01) != 0x01);					//wait ACKUPD
			AT91_SYS->RTC_TIMR = BIN2BCD(m_tm.tm_sec) << 0 | BIN2BCD(m_tm.tm_min) << 8	//update time
				| BIN2BCD(m_tm.tm_hour) << 16;
			AT91_SYS->RTC_CALR = BIN2BCD((m_tm.tm_year + 1900) / 100)					//update date
				| BIN2BCD(m_tm.tm_year % 100) << 8
				| BIN2BCD(m_tm.tm_mon + 1) << 16
				| BIN2BCD(m_tm.tm_wday) << 21				/* day of the week [1-7], Sunday=7 */
				| BIN2BCD(m_tm.tm_mday) << 24;
			/* Restart Time/Calendar */
			AT91_SYS->RTC_CR &= ~(AT91C_RTC_UPDCAL | AT91C_RTC_UPDTIM);		//close update application
			AT91_SYS->RTC_SCCR |= AT91C_RTC_ACKUPD;							//clear ACKUPD
		}
	}
	return ret;
}

/*
 * Read RTC time
 * when flag = 1, use copy_to_user
 * if success, return 0, else return error number
 * version 1.0
 */
static int read_RTC_time(struct rtc_time *tm, int flag)
{
	int ret = 0;
	struct rtc_time m_tm;
	unsigned int time, date;
	memset(&m_tm, 0, sizeof(m_tm));
	// ����������, ��֤û����仯
	do {
		time = AT91_SYS->RTC_TIMR;
		date = AT91_SYS->RTC_CALR;
	} while ((time != AT91_SYS->RTC_TIMR) || (date != AT91_SYS->RTC_CALR));
	m_tm.tm_sec = BCD2BIN((time & AT91C_RTC_SEC) >> 0);			// second
	m_tm.tm_min = BCD2BIN((time & AT91C_RTC_MIN) >> 8);			// minute
	m_tm.tm_hour = BCD2BIN((time & AT91C_RTC_HOUR) >> 16);		// hour
	m_tm.tm_year = BCD2BIN(date & AT91C_RTC_CENT) * 100;		/* century */
	m_tm.tm_year += BCD2BIN((date & AT91C_RTC_YEAR) >> 8);		/* year */
	m_tm.tm_wday = BCD2BIN((date & AT91C_RTC_DAY) >> 21);	/* day of the week [1-7], Sunday=7 */
	m_tm.tm_mon = BCD2BIN((date & AT91C_RTC_MONTH) >> 16) - 1;
	m_tm.tm_mday = BCD2BIN((date & AT91C_RTC_DATE) >> 24);
	m_tm.tm_yday = __mon_yday[is_leap(m_tm.tm_year)][m_tm.tm_mon] + m_tm.tm_mday-1;
	m_tm.tm_year = m_tm.tm_year - 1900;
	if (flag) {
		ret = copy_to_user(tm, &m_tm, sizeof(m_tm)) ? -EFAULT : 0;
	} else {
		*tm = m_tm;
		ret = 0;
	}
	//printk("RTC_SR = 0x%08x\n", AT91_SYS->RTC_SR);
	return ret;
}

/*
 * read time only second event happend
 */
static int read_time(unsigned char *tm)
{
	unsigned long time, date;
	//if ((AT91_SYS->RTC_SR & AT91C_RTC_SECEV) != AT91C_RTC_SECEV)	//no second event return ret
	//	return 0;
	do {
		time = AT91_SYS->RTC_TIMR;
		date = AT91_SYS->RTC_CALR;
		//i++;
	} while ((time != AT91_SYS->RTC_TIMR) || (date != AT91_SYS->RTC_CALR));
	AT91_SYS->RTC_SCCR |= AT91C_RTC_SECEV;			//clear second envent flag
	*tm++ = (date >> 8)		& 0xff;		// ��
	*tm++ = (date >> 16)	& 0x1f;		// ��
	*tm++ = (date >> 24)	& 0x3f;		// ��
	*tm++ = (time >> 16)	& 0x3f;		// ʱ
	*tm++ = (time >> 8)		& 0x7f;		// ��
	*tm	  = time			& 0x7f;		// ��
	return 0;
}

static int update_time(struct rtc_time *rtctm)
{
	int err;
	err = rtc_valid_tm(rtctm);
	if (err == 0) {
		struct timespec tv;

		tv.tv_nsec = NSEC_PER_SEC >> 1;

		rtc_tm_to_time(rtctm, &tv.tv_sec);

		do_settimeofday(&tv);

		pr_debug(	"setting the system clock to %d-%02d-%02d %02d:%02d:%02d (%u)\n",
			rtctm->tm_year + 1900, rtctm->tm_mon + 1, rtctm->tm_mday,
			rtctm->tm_hour, rtctm->tm_min, rtctm->tm_sec,
			(unsigned int) tv.tv_sec);
	} else {
		printk("%s(): invalid date/time\n", __FUNCTION__);
	}
	return 0;
}

#ifdef CONFIG_UART_V2
// �õ�time_tʱ��
int get_time_t(u32 *tv_sec)
{
	struct rtc_time rtctm;
	unsigned long tmp;
	read_RTC_time(&rtctm, 0);
	rtc_tm_to_time(&rtctm, &tmp);
	*tv_sec = tmp;
	pr_debug("time now = %d, %d, %d\n", *tv_sec,
		*tv_sec % (3600 * 60), *tv_sec % 3600);
	return 0;
}

// �õ�һ�������
int get_day_time(void)
{
	char tm[6];
	read_time(tm);
	return _cal_itmofday(tm);
}

#endif

#if 0
// uart �жϷ�ʽ
static int enable_usart2_int(void)
{
	uart_ctl2->US_IER = AT91C_US_TXRDY;
	return 0;
}

static int enable_usart0_int(void)
{
	uart_ctl0->US_IER = AT91C_US_TXRDY;
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
#ifdef SEND_NORECV
	// ��ʼ����������, ����Ҫ���÷���
	if (!(num & 0x80)) {
		uart_ctl0->US_CR = AT91C_US_RXDIS;
	} else {
		uart_ctl2->US_CR = AT91C_US_RXDIS;
	}
#endif
	udelay(1);
	// ��������ʱ����Ҫ����ͨ��, ���ԭ��ͼ
	if (!(num & 0x80)) {
		// ǰ��ͨ����UART0
		for (i = 0; i < count; i++) {
			uart_ctl0->US_THR = *buf++;
			if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY)) {
				ret = -1;
				goto out;
			}
		}
	} else {
		// ����ͨ����UART2
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
	// ���ͽ����������л�������ģʽ
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
#ifdef SEND_NORECV
	// ���ϻ�Ϊ���շ�ʽ
	if (!(num & 0x80)) {
		uart_ctl0->US_CR = AT91C_US_RXEN;
	} else {
		uart_ctl2->US_CR = AT91C_US_RXEN;
	}
#endif
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
#ifdef SEND_NORECV
	// ��ʼ����������, ����Ҫ���÷���
	if (!(num & 0x80)) {
		uart_ctl0->US_CR = AT91C_US_RXDIS;
	} else {
		uart_ctl2->US_CR = AT91C_US_RXDIS;
	}
#endif
	udelay(1);
	// ��������ʱ����Ҫ����ͨ��, ���ԭ��ͼ
	if (!(num & 0x80)) {
		// ǰ��ͨ����UART0
		uart_ctl0->US_THR = buf;
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY)) {
			ret = -1;
			goto out;
		}
	} else {
		// ����ͨ����UART2
		uart_ctl2->US_THR = buf;
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY)) {
			ret = -1;
			goto out;
		}
	}
out:
	// ���ͽ����������л�������ģʽ
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
#ifdef SEND_NORECV
	// ���ϻ�Ϊ���շ�ʽ
	if (!(num & 0x80)) {
		uart_ctl0->US_CR = AT91C_US_RXEN;
	} else {
		uart_ctl2->US_CR = AT91C_US_RXEN;
	}
#endif
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	//udelay(1);
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
#ifdef SEND_NORECV
	// ��ʼ����������, ����Ҫ���÷���
	if (!(num & 0x80)) {
		uart_ctl0->US_CR = AT91C_US_RXDIS;
	} else {
		uart_ctl2->US_CR = AT91C_US_RXDIS;
	}
#endif
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
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
#ifdef SEND_NORECV
	// ���ϻ�Ϊ���շ�ʽ
	if (!(num & 0x80)) {
		uart_ctl0->US_CR = AT91C_US_RXEN;
	} else {
		uart_ctl2->US_CR = AT91C_US_RXEN;
	}
#endif
	//udelay(1);
	return ret;
}
#define USRECVSAFE

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
#ifdef USRECVSAFE
#warning "use USART RECV SAFE Mode"
		// ���ʱ������Ѿ��������յ���, �϶��Ǵ��������
		if (uart_ctl0->US_CSR & AT91C_US_RXRDY) {
			// �ȶ�ȡ, ���ӳٵȴ�?
			char tmp;
			tmp = uart_ctl0->US_RHR;
 			//uart_ctl0->US_CR |= AT91C_US_RSTSTA;		// reset status
		}
		// �жϱ�־λ
		if(((uart_ctl2->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl2->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
				uart_ctl2->US_CR = AT91C_US_RSTSTA;		// reset status
		}
#endif
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
#ifdef USRECVSAFE
		// ���ʱ������Ѿ��������յ���, �϶��Ǵ��������
		if (uart_ctl2->US_CSR & AT91C_US_RXRDY) {
			// �ȶ�ȡ, ���ӳٵȴ�?
			char tmp;
			tmp = uart_ctl2->US_RHR;
 			//uart_ctl2->US_CR |= AT91C_US_RSTSTA;		// reset status
		}
		// �жϱ�־λ
		if(((uart_ctl2->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl2->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
				uart_ctl2->US_CR = AT91C_US_RSTSTA;		// reset status
		}
#endif
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
 * flag: 0->ctl0; 1->clt1
 * receive data from 485, return count is ok, -1 is timeout, -2 is other errors
 * created by wzhe, 10/16/2006
 */
int recv_data_safe(char *buf, unsigned char num)
{
	int ret = 0;
	if (!buf)
		return -EINVAL;
	// ��ͨ���ж�
	if (!(num & 0x80)) {
#ifdef USRECVSAFE
#warning "use USART RECV SAFE Mode"
		// ���ʱ������Ѿ��������յ���, �϶��Ǵ��������
		if (uart_ctl0->US_CSR & AT91C_US_RXRDY) {
			// �ȶ�ȡ, ���ӳٵȴ�?
			char tmp;
			tmp = uart_ctl0->US_RHR;
		}
			// �жϱ�־λ
		if(((uart_ctl0->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl0->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
				uart_ctl0->US_CR = AT91C_US_RSTSTA;		// reset status
		}
#endif
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
#ifdef USRECVSAFE
		// ���ʱ������Ѿ��������յ���, �϶��Ǵ��������
		if (uart_ctl2->US_CSR & AT91C_US_RXRDY) {
			// �ȶ�ȡ, ���ӳٵȴ�?
			char tmp;
			tmp = uart_ctl2->US_RHR;
 			//uart_ctl2->US_CR |= AT91C_US_RSTSTA;		// reset status
		}
		// �жϱ�־λ
		if(((uart_ctl2->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl2->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
				uart_ctl2->US_CR = AT91C_US_RSTSTA;		// reset status
		}
#endif
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
static inline
int chk_space(void)
{
	int tail = flowptr.tail;
	//int head = flowptr.head;
	int ret;
	if ((tail + 1) <= FLOWANUM) {
		ret = FLOWANUM - tail;
	} else {
		ret = -1;
	}
	return ret;
}

#ifdef CONFIG_UART_V2
int _reset_acc_limit(acc_ram *pacc)
{
	pacc->money_life = pacc->money_limit;
	pacc->times_life = pacc->times_limit;
	pacc->smoney_life = pacc->smoney_limit;
	pacc->stimes_life = pacc->stimes_limit;
	pacc->dic_times = 0;
	return 0;
}

// ����acc �ʹβ���
int reset_acc_limit(void)
{
	acc_ram *pacc = paccmain;
	int nacc = rcd_info.account_main;
	int i;
	
	struct hashtab *h = hash_accsw;
	u32 slots_remain = h->nel;
	struct hashtab_node *cur;
	
	// ���˻���
	for (i = 0; i < nacc; i++, pacc++) {
		_reset_acc_limit(pacc);
	}
	
	// hash�˻���
	for (i = 0; i < h->size && slots_remain; i++) {
		cur = h->htable[i];
		while (cur) {
			slots_remain--;
			_reset_acc_limit((acc_ram *)cur->datum);
			cur = cur->next;
		}
	}
	return 0;
}

// ����current_idֵ����������ڣ�������֮ǰ��ֵ
int set_current_id(int itm)
{
	int i;
	user_tmsg *tmsg = usrcfg.tmsg;
	
	for (i = 0; i < 4; i++) {
		if (itm >= tmsg[i].begin && itm <= tmsg[i].end) {
			current_id = i;
			break;
		}
	}
	return 0;
}

// ��ʼ��uart����
int uart_data_init(int acc_time)
{
	user_tmsg *tmsg = usrcfg.tmsg;
	int itm = get_day_time();

	set_current_id(itm);

	pr_debug("data init: itm = %d, acc_time = %d, cid = %d, begin = %d\n",
		itm, acc_time, current_id, tmsg[current_id].begin);
	
	if (acc_time < tmsg[current_id].begin) {
		// ����acc �ʹβ���
		reset_acc_limit();
	}
	return 0;
}

// ��鵱ǰ�ʹ�ʱ��
int check_current_id(int itm)
{
	user_tmsg *tmsg = usrcfg.tmsg;
	int id = current_id;
	
	if (itm >= tmsg[id].begin && itm <= tmsg[id].end) {
		return 0;
	}

	pr_debug("current id change...\n");
	
	set_current_id(itm);

	reset_acc_limit();
	return 0;
}
#endif


//#warning "485 call not use interrupt mode"
// UART 485 �кź���
static int uart_call(void)
{
//#define CALLDEBUG
//#define RTBLKCNTMODE
#define NCMDMAX 10
	int n;
	//static unsigned long jiff;
	static unsigned char tm[7];
	int i, ret, allow;
	unsigned char sendno;
	unsigned char rdata = 0, commd;
	term_ram *prterm;
#ifdef CONFIG_UART_V2
	static int itm;		// ��ǰʱ��һ��֮��
#endif
	if (paccmain == NULL || pterminal == NULL || ptermram == NULL) {
		pr_debug("pointer is NULL!!!ERROR!\n");
		return -1;
	}
	if (rcd_info.term_all == 0) {
		goto out;
	}
	if (space_remain == 0) {//flash�ռ�ʣ��
		pr_debug("there is no space room\n");
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
	//jiff = jiffies;
	itm = _cal_itmofday(&tm[1]);

	check_current_id(itm);

	//pr_debug("terminal %d\n", rcd_info.term_all);

	while (n) {
		prterm = ptermram;
		AT91_SYS->ST_CR = AT91C_ST_WDRST;	/* Pat the watchdog */
		// ��һȦ
		for (i = 0; i < rcd_info.term_all; i++, prterm++, udelay(ONEBYTETM)) {
			if (chk_space() < 0) {
				printk("tail is %d, head is %d\n", flowptr.tail, flowptr.head);
				goto out;
			}
			sel_ch(prterm->term_no);// �κ������, �Ƚ�138����λ��λ
			sendno = prterm->term_no & 0x7F;
			prterm->add_verify = 0;
			prterm->dif_verify = 0;
			prterm->status = STATUSUNSET;
			//pr_debug("call terminal %d\n", prterm->pterm->local_num);
			// �������º�����ʱ��local_num | 0x80 ---> ע�������ת
			ret = send_addr(sendno, prterm->pterm->local_num);
			if (ret < 0) {
				pr_debug("send addr %d error: %d\n", sendno, ret);
				prterm->status = NOTERMINAL;
				continue;
			}
			// receive data, if pos number and no cmd then end
			// if pos number and cmd then exec cmd
			// else wrong!
			ret = recv_data_safe((char *)&rdata, prterm->term_no);
			if (ret < 0) {
				//pr_debug("recv term %d error: %d\n", prterm->term_no, ret);
				prterm->status = ret;
				//prterm->status = NOTERMINAL;
				continue;
			}
			prterm->add_verify += rdata;
			prterm->dif_verify ^= rdata;
			//printk("recvno: %d and sendno: %d\n", rdata, sendno);
			if ((rdata & 0x7F) != sendno) {
				pr_debug("recvno: %d but sendno: %d\n", rdata, sendno);
				prterm->status = NOCOM;
				continue;
			}
			if ((prterm->pterm->param.term_type & (1 << REGISTER_TERMINAL_FLAG))
				&& !(rdata & 0x80)) {
				// no command, ������Ǯ���ն�
				// �غ���ȷ, ��û������
#ifdef CONFIG_RTBLKCNTMODE		// ���ĺ���������
/*
 * �޸ĺ���������, ֻ�е�N������û���������������²��п��ܽ��к���������
 * �������������ʧ�ܳ���M��֮��, �Զ�תΪ�´θ�������
 */
 				prterm->nocmdcnt++;
				if (prterm->nocmdcnt <= NCMDMAX) {
					goto _donothing;
				}
#endif
#if 0
				if ((prterm->blkerr > 5) &&
					((jiff - prterm->jff_trdno) > (HZ * 10))) {
#ifdef CALLDEBUG
					printk("blkerr: timeout and blkerr > 5\n");
#endif
					// GA��ȡ������, �����õ����������׺�
					ret = purse_get_edition(prterm);
					if (ret < 0) {
#ifdef CALLDEBUG
						printk("blkerr: get edition error %d\n", ret);
#endif
						continue;
					} else {
						prterm->jff_trdno = jiff;
					}
				}
#endif
				// �ڴ�Ҫ������������, ʵʱ�·�������
				if (prterm->key_flag == 0) {
					ret = purse_update_key(prterm);
					if (ret == 0) {
						prterm->key_flag = 1;
					} else {
						//pr_debug("update key failed\n");
					}
				}
				ret = purse_send_bkin(prterm, &blkinfo, pblack, prterm->black_flag);
				if (ret < 0) {
					//pr_debug("real-time send black list failed\n");
				} else {
#if 0
					prterm->blkerr = 0;
					prterm->jff_trdno = jiff;	// ÿһ�θ��³ɹ���Ҳ��õ��µİ汾��
#endif
				}
#ifdef CONFIG_RTBLKCNTMODE
_donothing:
#endif
				prterm->status = TNORMAL;
				continue;
			} else if (!(rdata & 0x80)) {
				// ��Ǯ���ն�
				prterm->status = TNORMAL;
				continue;
			}
			// has command, then receive cmd
			ret = recv_data((char *)&commd, prterm->term_no);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("recv cmd %d error: %d\n", prterm->pterm->local_num, ret);
#endif
				prterm->status = ret;
				continue;
			}
#ifdef CONFIG_RTBLKCNTMODE
			prterm->nocmdcnt = 0;
#endif
			prterm->add_verify += commd;
			prterm->dif_verify ^= commd;
			// ����������������
			switch (commd) {
#ifdef CONFIG_UART_V2
			/* ֧�ִ��ۿ��ն� */
			case SENDLEIDIFNO:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = recv_le_id(prterm, allow, itm);
				break;
			case RECVLEHEXFLOW:
				ret = recv_leflow(prterm, 0, tm);
				break;
			case RECVLEBCDFLOW:
				ret = recv_leflow(prterm, 1, tm);
				break;
			case RECVTAKEFLOW:
				ret = recv_take_money_v1(prterm, tm, 0);
				break;
			case RECVDEPFLOW:
				ret = recv_dep_money_v1(prterm, tm, 0);
				break;

			case SENDRUNDATA:
				// �����ն��ϵ����
				ret = send_run_data(prterm);
				break;
			case RECVNO:
				ret = ret_no_only(prterm);
				break;
			case NOCMD:
				break;
			case SENDLEID_SINGLE:
				/* ���˻��ն�Ҫ������� */
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = recv_leid_single(prterm, allow, itm);
				break;
			case SENDLEIDACCX:
				/* ���ɻ�Ҫ������� */
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = recv_leid_cash(prterm, allow, itm);
				break;
			case SENDLEID_DOUBLE:
				/* ˫�˻�Ҫ������� */
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = recv_leid_double(prterm, allow, itm);
				break;
			case RECVLEFLOW_DOUBLE:
				/* ����˫�˻���ˮ���� */
				ret = recv_leflow_double(prterm, tm);
				break;
			case RECVDEPFLOWX:
				/* ������� ˫У�� */
				ret = recv_dep_money(prterm, tm);
				break;
			case RECVTAKEFLOWX:
				/* ȡ������ ˫У�� */
				ret = recv_take_money(prterm, tm);
				break;
			case RECVLEFLOW_SINGLE:
				/* �������� ˫У�� */
				ret = recv_leflow_single(prterm, tm);
				break;
#endif		/* CONFIG_UART_V2 */

			case PURSE_RECV_CARDNO:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				ret = purse_recv_leid(prterm, allow, itm);//itm added by duyy, 2013.5.8
				break;
			case PURSE_RECV_LEFLOW:
				ret = purse_recv_leflow(prterm, tm);
				break;
			case PURSE_REQ_PAR:
				prterm->black_flag = 0;
				prterm->key_flag = 0;
#if 0
				prterm->blkerr = 0;
#endif
				memset(&prterm->blkinfo, 0, sizeof(prterm->blkinfo));
				prterm->psam_trd_num = 0;	// new terminal
				ret = purse_send_conf(prterm, tm, &blkinfo);
				pr_debug("purse_send conf return %d\n", ret);
				// �˴��ϵ�������ͽ���, ��ʱҪ�����·���ֵ
				if (!ret) {
				} else
					printk("purse_send conf return %d\n", ret);
				break;
			/*//һ��ͨ����ȥ��������������
			case PURSE_REQ_PARNMF:  //added by duyy,2012.2.13
				prterm->black_flag = 0;
				prterm->key_flag = 0;
				memset(&prterm->blkinfo, 0, sizeof(prterm->blkinfo));
				prterm->psam_trd_num = 0;	// new terminal
				ret = purse_send_confnmf(prterm, tm, &blkinfo);
				printk("purse_send conf no managefee return %d\n", ret);
				break;	
			case PURSE_REQ_MF://added by duyy,2012.2.13
				ret = purse_send_managefee(prterm);
				break;
				*/
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
				pr_debug("end purse_recv_ledep %d\n", ret);
				break;
			case PURSE_RECV_TAKE:
				ret = purse_recv_letake(prterm, tm);
				break;

#ifdef CONFIG_UART_V2
			/* ͸֧����֧�� ������ҽԺ�汾��ֲ wjzhe 20111128 */
			case SENDLEID_ZLYY:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				allow = 1;
				ret = recv_leid_zlyy(prterm, allow, 1, itm);
				break;
			case SENDLEID_ZLYY2C:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				allow = 1;
				ret = recv_leid_zlyy2(prterm, allow, 1, itm);
				break;
			case SENDLEID_ZLYYC:
				if ((net_status == 0) && (le_allow == 0)) {
					allow = 0;
				} else {
					allow = 1;
				}
				allow = 1;
				ret = recv_leid_zlyy_cash(prterm, allow, 1, itm);
				break;
			case RECVLEFLOW_ZLYY:
				ret = recv_leflow_zlyy(prterm, tm);
				break;
			case SENDTIME_ZLYY:
				ret = send_time_zlyy(prterm, tm, itm);
				break;
#endif		/* CONFIG_UART_V2 */

			default:			// cmd is not recognised, so status is !
				prterm->status = -2;
				pr_debug("cmd %02x is not recognised\n", commd);
				usart_delay(1);
				ret = -1;
				break;
			}
			if (prterm->status == STATUSUNSET)
				prterm->status = TNORMAL;
			//prterm++;
			//udelay(ONEBYTETM);
			if (space_remain == 0) {
				total += flow_sum;
				goto out;
			}
		}
		n--;
	}
	total += flow_sum;
out:
	return 0;
}

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
	// ��ʼ��hashtab
	hash_accsw = hashtab_create(def_hash_value, def_hash_cmp, MAXSWACC);
	if (hash_accsw == NULL) {
		return -ENOMEM;
	}
	printk(KERN_WARNING "Uart accsw hash table establish\n");
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
	unsigned long wtdreg;
	struct uart_ptr ptr;
	no_money_flow no_m_flow;
	money_flow m_flow;
	struct get_flow pget;

	down(&uart_lock);

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
		// ��ʹ��������
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
		if ((ret = copy_from_user(pterminal, ptr.pterm,
			rcd_info.term_all * sizeof(terminal))) < 0) {
			break;
		}
		ptem = pterminal;
		ptrm = ptermram;
		// ���terminal ram�ṹ��
		for (i = 0; i < rcd_info.term_all; i++, ptem++, ptrm++) {
			pr_debug("terminal %d register\n", ptem->local_num);
			ptrm->pterm = ptem;
			ptrm->term_no = ptem->local_num;
			ptrm->status = STATUSUNSET;
			ptrm->term_cnt = 0;
			ptrm->term_money = 0;
			ptrm->flow_flag = 0;
		}
		flowptr.head = flowptr.tail = 0;
		flow_sum = 0;
		total = 0;
		// ������翨������־
		le_allow = ptr.le_allow;
//		printk("le allow %d, %d\n", le_allow, ptr.le_allow);
		//net_status = ptr.pnet_status;
		if ((ret = get_user(net_status, (int *)ptr.pnet_status)) < 0)
			break;
		// ��ʹ��������
		memcpy(&blkinfo, &ptr.blkinfo, sizeof(blkinfo));
		if (pblack == NULL) {
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
		break;
	case SETFORBIDTIME://��ʼ���ն˽�ֹ����ʱ��,added by duyy, 2013.5.8
		if ((ret = copy_from_user(pfbdseg, (unsigned char*)arg, sizeof(pfbdseg))) < 0)
			break;
	#if 0//2013.5.13
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
		break;
	case SETTIMENUM:		// �������ݡ��ն�����ƥ��ʱ�����,added by duyy, 2013.5.8
		//bug,ԭ����һ��if�ж���䣬С�ڣ���break�����ڣ���������½��У�����ʹ�ѻ����ܶ�������,modified by duyy, 2013.7.22
		ret = copy_from_user(ptmnum, (unsigned char*)arg, sizeof(ptmnum));
		break;
	case SETLEALLOW:		// ���ù�翨�ܷ����ʱ����
		ret = copy_from_user(&le_allow, (void *)arg, sizeof(le_allow));
		break;
	case SETRTCTIME:		// ����RTC
		//copy_from_user(&tm, , sizeof(tm));
		{
			struct rtc_time tmp;
			copy_from_user(&tmp, (void *)arg, sizeof(tmp));
			ret = set_RTC_time((struct rtc_time *)arg, 0);
			update_time(&tmp);
		}
		break;
	case GETRTCTIME:		// ��ȡRTC
		ret = read_RTC_time((struct rtc_time *)arg, 1);
		//copy_to_user(, &tm2, sizeof(tm2));
		break;
	case START_485:			// 485�к�, ��SEM�������ݱ���
		ret = uart_call();
		break;
	case SETREMAIN:			// ����ϵͳʣ��洢�ռ�
		ret = copy_from_user(&space_remain, (int *)arg, sizeof(space_remain));
		if (ret < 0)
			break;
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
		if ((ret = deal_no_money(&no_m_flow, paccmain, hash_accsw, &rcd_info)) < 0) {
			printk("UART: deal no money flow error\n");
		}
		//ret = 0;
		break;
	case CHGACC:
		// ���������ֽ���ˮ
		if ((ret = copy_from_user(&m_flow, (money_flow *)arg,
			sizeof(money_flow))) < 0) {
			printk("UART: copy from user money flow failed\n");
			break;
		}
		// �������ݴ���
		if ((ret = deal_money(&m_flow/*, paccmain, hash_accsw, &rcd_info*/)) < 0) {
			printk("UART: deal money flow error\n");
		}
		break;
	case ADDACCSW1:
	{	// �ȿ�������ˮ����
		no_money_flow *pnflow = NULL;
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
		ret = copy_from_user(&pnflow, (money_flow *)arg, sizeof(pnflow));
		if (ret < 0) {
			printk("UART: copy from user money flow ptr failed\n");
			break;
		}
		// ˳�δ���
		while (i) {
			// �����������ֽ���ˮ
			if ((ret = copy_from_user(&no_m_flow, pnflow,
				sizeof(no_money_flow))) < 0) {
				printk("UART: copy from user no money flow failed\n");
				break;
			}
			// �������ݴ���
			if ((ret = deal_no_money(&no_m_flow, paccmain, hash_accsw, &rcd_info)) < 0) {
				printk("UART: deal no money flow error\n");
			}
			pnflow += 1;
			i--;
		}
	}
		break;
	case CHGACC1:
	{	// �ȿ�������ˮ����
		money_flow *pmflow = NULL;
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
			if ((ret = deal_money(&m_flow/*, paccmain, paccsw, &rcd_info*/)) < 0) {
				printk("UART: deal money flow error\n");
			}
			pmflow += 1;
			i--;
		}
	}
		break;
#ifdef CONFIG_UART_V2
	case NEWREG: 
	{	// �ȿ�������ˮ����
		acc_ram *pacc = NULL, acc_tmp;
		ret = copy_from_user(&i, (int *)arg, sizeof(int));
		if (ret < 0) {
			pr_debug("UART: copy from user new reg flow count failed\n");
			break;
		}
		if (i <= 0) {
			pr_debug("UART: new reg flow count zero\n");
			break;
		}
		arg += sizeof(int);
		// �ٵõ���ˮָ��
		ret = copy_from_user(&pacc, (money_flow *)arg, sizeof(pacc));
		if (ret < 0) {
			pr_debug("UART: copy from new reg flow ptr failed\n");
			break;
		}
		// ˳�δ���
		while (i) {
			// ���������ֽ���ˮ
			if ((ret = copy_from_user(&acc_tmp, pacc, sizeof(acc_tmp))) < 0) {
				pr_debug("UART: copy from new reg flow failed\n");
				break;
			}
			// �������ݴ���
			if ((ret = new_reg(&acc_tmp)) < 0) {
				pr_debug("UART: new reg flow error\n");
			}
			pacc += 1;
			i--;
		}
	}
		break;
	case SETSPLPARAM:
		// �����������
		if ((ret = copy_from_user(&usrcfg, (void *)arg, sizeof(usrcfg))) < 0) {
			pr_debug("UART: copy from usrcfg failed\n");;
			break;
		}

		break;
	case INITUARTDATA:
		// ��������, arg = acc_time, �˻�������ʱ��
		ret = uart_data_init(arg);
		break;
#endif
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
		if ((rcd_info.account_main * sizeof(acc_ram)) > SZ_128K)
			vfree(paccmain);
		else
			kfree(paccmain);
		paccmain = NULL;
		break;
	case GETACCSW:// �õ����˻���
		ret = 0;
		if (rcd_info.account_sw == 0)
			break;
#if 0
		ret = copy_to_user((acc_ram *)arg, paccsw,
			sizeof(acc_ram) * rcd_info.account_sw);
#endif
{
		acc_ram *v;
		//v = kmalloc(sizeof(acc_ram) * hash_accsw->nel, GFP_KERNEL);
		v = (acc_ram *)vmalloc(sizeof(acc_ram) * hash_accsw->nel);
		if (v == NULL) {
			ret = -ENOMEM;
			break;
		}
		ret = hashtab_getall(hash_accsw, v, sizeof(acc_ram));
		if (ret != hash_accsw->nel) {
			printk("warning hashtab get all return %d, but %d\n",
				ret, hash_accsw->nel);
		}
		// ������?
		ret = copy_to_user((void *)arg, v, sizeof(acc_ram) * ret);
		//kfree(v);
		vfree(v);
}

		break;
	case SETNETSTATUS:// ��������״̬
		ret = get_user(net_status, (int *)arg);
		break;
	case BCT_BLACKLIST:// �㲥������
		// ������ʱ��ܳ�, ��Ҫ�رտ��Ź�
		// save watch dog reg
		wtdreg = AT91_SYS->ST_WDMR;
		AT91_SYS->ST_WDMR = AT91C_ST_EXTEN;
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
			// ����������б��ռ�
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
		AT91_SYS->ST_WDMR = wtdreg;
		AT91_SYS->ST_CR = AT91C_ST_WDRST;
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
		if (cashterm_ptr) {
			ret = copy_to_user((void *)arg, cashbuf,
				sizeof(struct cash_status) * cashterm_ptr);
			if (ret < 0) {
				printk("Uart ioctl GETCASHBUF copy_to_user error\n");
			} else {
				ret = cashterm_ptr;
			#if 0
				//added by duyy,2012.3.9
				printk("first cashterm_ptr=%d\n", cashterm_ptr);
				for(i = 0; i < cashterm_ptr; i++){
					printk("cashbuf[%d]:termno=%c\tstatus=%c\tfeature=%c\taccno=%d\tcardno=%d\tmoney=%d\tconsume=%d\tmanagefee=%d\tterm_money=%d\n",
						i, cashbuf[i].termno, cashbuf[i].status, cashbuf[i].feature, cashbuf[i].accno, cashbuf[i].cardno, cashbuf[i].money,
						cashbuf[i].consume, cashbuf[i].managefee, cashbuf[i].term_money);
				}
			#endif	
			}
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
			//added,2012.3.9
			//printk("second cashterm_ptr=%d\n", cashterm_ptr);
		}
		break;
#endif
	default:
		printk("485 has no cmd %02x\n", cmd);
		ret = -1;
		break;
	}

//out:
	up(&uart_lock);
	return ret;
}

/*
 * д��FRAM������ˮ, ֻ����ϵͳ��ʹ��ʱ
 */
static int uart_write(struct file *file, const char *buff, size_t count, loff_t *offp)
{
	return -1;
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
	hashtab_destroy_d(hash_accsw);
	hash_accsw = NULL;
	// �ͷ���Դ, modified by wjzhe 20110214
	if (paccmain) {
		if ((rcd_info.account_main * sizeof(acc_ram)) > SZ_128K) {
			vfree(paccmain);
		} else {
			kfree(paccmain);
		}
		paccmain = NULL;
		memset(&rcd_info, 0, sizeof(rcd_info));
	}
	return 0;
}
struct file_operations uart_fops = {
	.owner = THIS_MODULE,
	.open = uart_open,
	.ioctl = uart_ioctl,
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
