/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_uart.c
 * 摘    要：此程序为总医院的特殊版，包含两种终端，分别采用不同的通讯协议。
 *			128-255的号码为公司生产的终端所应用的终端号，0-127的号码为A2消费机所应用的终端号。
 * 			 
 * 当前版本：3.0
 * 作    者：duyy
 * 完成日期：2012年9月6日
 *
 * 取代版本：2.0 
 * 原作者  ：wjzhe
 * 完成日期：2007年4月3日
 
 * 取代版本：1.0 
 * 原作者  ：wjzhe
 * 完成日期：2007年2月13日
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
#include "at91_rtc.h"
#include "data_arch.h"
#include "uart_dev.h"
#include "uart.h"
#include "deal_data.h"

#define BAUDRATE 38400//28800

#define LAPNUM 4

//#define U485INT

//#define UARTDBG

static int lapnum;

// 定义指针
/********串口相关定义***********/
#ifndef USER_485
static AT91PS_USART uart_ctl0 = (AT91PS_USART) AT91C_VA_BASE_US0;
static AT91PS_USART uart_ctl2 = (AT91PS_USART) AT91C_VA_BASE_US2;
#else
static volatile unsigned int * UART2_CR;
static volatile unsigned int * UART2_MR;
static volatile unsigned int * UART2_IER;
static volatile unsigned int * UART2_IDR;
static volatile unsigned int * UART2_IMR;
static volatile unsigned int * UART2_CSR;
static volatile unsigned int * UART2_RHR;
static volatile unsigned int * UART2_THR;
static volatile unsigned int * UART2_BRGR;
static volatile unsigned int * UART2_RTOR;
static volatile unsigned int * UART2_TTGR;
static volatile unsigned int * UART2_FIDI;
static volatile unsigned int * UART2_NER;
static volatile unsigned int * UART2_IF;
static volatile unsigned int * AIC_SMR8;
	
static volatile unsigned int * UART0_CR;
static volatile unsigned int * UART0_MR;
static volatile unsigned int * UART0_IER;
static volatile unsigned int * UART0_IDR;
static volatile unsigned int * UART0_IMR;
static volatile unsigned int * UART0_CSR;
static volatile unsigned int * UART0_RHR;
static volatile unsigned int * UART0_THR;
static volatile unsigned int * UART0_BRGR;
static volatile unsigned int * UART0_RTOR;
static volatile unsigned int * UART0_TTGR;
static volatile unsigned int * UART0_FIDI;
static volatile unsigned int * UART0_NER;
static volatile unsigned int * UART0_IF;
static volatile unsigned int * AIC_SMR6;
#endif

struct record_info rcd_info;

int space_remain = 0;
//定义A2终端生产序列号数组
unsigned char productnumber[128][3];

// 定义数据或数据指针
unsigned int fee[16];

#ifdef CONFIG_RECORD_CASHTERM
// 定义记录出纳机状态的变量
const int cashterm_n = CASHBUFSZ;
int cashterm_ptr;				// 存储指针
struct cash_status cashbuf[CASHBUFSZ];	// 保存状态空间
#endif

int g_tcnt = 0;

// 这是以前旧的更换区, 新的程序要改为hashtab
#ifndef CONFIG_ACCSW_USE_HASH
acc_ram paccsw[MAXSWACC];
#else
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
#endif
flow pflow[FLOWANUM];
int maxflowno = 0;

term_ram * ptermram = NULL;
terminal *pterminal = NULL;
acc_ram *paccmain = NULL;
//flow *pflow = NULL;

// 定义流水记录指针
struct flow_info flowptr;

// 记录每次叫号后的产生的流水总数
int flow_sum;

static int total = 0;// 现有流水总数

// 定义网络状态指针
static int net_status = 0;
// 定义光电卡允许脱机使用标志
static int le_allow = 0;

#ifdef EDUREQ
struct edupar edup;
#endif

#ifdef S_BID
struct bid_info bidinfo;
#endif

#ifdef USRCFG1
struct usr_cfg usrcfg;
struct term_money termoney;
#endif

#ifdef CONFIG_CNTVER
struct cnt_conf cntcfg;
struct f_cntver fcntver[FCNTVERN];
int fcntpos = 0;
//struct cntflow cfbuf[FCNTVERN];
//int cfpos = 0;
#endif

#define M1CARD
#undef M1CARD

#ifdef M1CARD
// 定义黑名单数据指针
black_acc *pblack = NULL;// 支持100,000笔, 大小为879KB
// 定义bad flow
struct bad_flow badflow = {0};
#endif

#undef U485INT
#if 0
// 定义自旋锁
#ifndef U485INT
static DEFINE_SPINLOCK(uart_lock);
#endif
#endif
// 定义mutex
static DECLARE_MUTEX(uart_lock);
//设备号:	6	UART0
//			8	UART2

#define UART_MAJOR 22
#define UART0_IRQ 6

extern short uart_poll(volatile unsigned int * reg, unsigned long bit);
extern void sel_ch(unsigned char num);

#ifdef EDUREQ
static inline int isusertm(char tm[], struct edupar *pedu)
{
	int flag = 0;
	int itm;
	itm = BCD2BIN(tm[5]);
	itm += BCD2BIN(tm[4]) * 60;
	itm += BCD2BIN(tm[3]) * 3600;
	if ((itm >= pedu->stime1 && itm <= pedu->etime1)
		|| (itm >= pedu->stime2 && itm <= pedu->etime2)) {
		flag = 1;
	}
	return flag;
}
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
	if (flag)
		cpy = copy_from_user(&m_tm, tm, sizeof(m_tm));
	else {
		m_tm = *tm;
		cpy = 0;
	}
	/*printk("Set RTC date/time is %d-%d-%d. %02d:%02d:%02d\n",
                m_tm.tm_mday, m_tm.tm_mon, m_tm.tm_year + 1900,
                m_tm.tm_hour, m_tm.tm_min, m_tm.tm_sec);*/
	if (cpy)
		ret = -EFAULT;
	else {
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
	if (flag)
		ret = copy_to_user(tm, &m_tm, sizeof(m_tm)) ? -EFAULT : 0;
	else {
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
//	unsigned char *tmp = tm;
	unsigned int time, date;
	//if ((AT91_SYS->RTC_SR & AT91C_RTC_SECEV) != AT91C_RTC_SECEV)	//no second event return ret
	//	return 0;
	do {
		time = AT91_SYS->RTC_TIMR;
		date = AT91_SYS->RTC_CALR;
	} while ((time != AT91_SYS->RTC_TIMR) || (date != AT91_SYS->RTC_CALR));
	AT91_SYS->RTC_SCCR |= AT91C_RTC_SECEV;			//clear second envent flag
	*tm++ = (date >> 8)		& 0xff;		// 年
	*tm++ = (date >> 16)	& 0x1f;		// 月
	*tm++ = (date >> 24)	& 0x3f;		// 日
	*tm++ = (time >> 16)	& 0x3f;		// 时
	*tm++ = (time >> 8)		& 0x7f;		// 分
	*tm	  = time			& 0x7f;		// 秒
/*	for (i = 0; i < 6; i++) {
		printk("tm[%d] is 0x%x\n", i, *tmp);
		tmp++;
	}*/
	return 0;
}
#ifdef U485INT
static int enable_usart2_int(void)
{
	*UART2_IER = AT91C_US_TXRDY;		// usart2 INT
	*UART2_IDR = (!AT91C_US_TXRDY);
	return 0;
}

static int enable_usart0_int(void)
{
	*UART0_IER = AT91C_US_TXRDY;		// usart0 INT
	*UART0_IDR = (!AT91C_US_TXRDY);
	return 0;
}
static int disable_usart_int(void)
{
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
	*UART0_BRGR = 0x61;//0x82;	//2012.5.9
	*UART0_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_NONE
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
	int i;
	//AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12;		// enable send data
	//以广播形式发送数据
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	// 发送数据时不需要设置通道, 详见原理图
	if (!(num & 0x80)) {
		// 前四通道用UART0
#ifndef USER_485
		uart_ctl0->US_CR &= !(AT91C_US_SENDA);
#else
		*UART0_CR &= !(AT91C_US_SENDA);// 清除地址位
#endif
		for (i = 0; i < count; i++) {
#ifndef USER_485
			uart_ctl0->US_THR = *buf++;
#else
			*UART0_THR = *buf++;
#endif
#ifndef USER_485
			if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY))
				return -1;
#else
			if (!uart_poll(UART0_CSR, AT91C_US_TXEMPTY))	// every time wait transfer completed
				return -1;
#endif
		}
	} else {
		// 后四通道用UART2
#ifndef USER_485
		uart_ctl2->US_CR &= !(AT91C_US_SENDA);
#else
		*UART2_CR &= !(AT91C_US_SENDA);// 清除地址位
#endif
		for (i = 0; i < count; i++) {
#ifndef USER_485
			uart_ctl2->US_THR = *buf++;
#else
			*UART2_THR = *buf++;
#endif
#ifndef USER_485
			if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY))
				return -1;
#else
			if (!uart_poll(UART2_CSR, AT91C_US_TXEMPTY))
				return -1;
#endif
		}
	}
	//选择终端所在通道接收数据
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	return count;
}
// 只发送一字节, 成功返回0
int send_byte(char buf, unsigned char num)
{
	//AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12;		// enable send data
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
	// 发送数据时不需要设置通道, 详见原理图
	if (!(num & 0x80)) {
		// 前四通道用UART0
#ifndef USER_485
		uart_ctl0->US_CR &= !(AT91C_US_SENDA);
		uart_ctl0->US_THR = buf;
#else
		*UART0_CR &= !(AT91C_US_SENDA);// 清除地址位
		*UART0_THR = buf;
#endif
#ifndef USER_485
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY))
			return -1;
#else
		if (!uart_poll(UART0_CSR, AT91C_US_TXEMPTY))	// every time wait transfer completed
			return -1;
#endif
	} else {
		// 后四通道用UART2
#ifndef USER_485
		uart_ctl2->US_CR &= !(AT91C_US_SENDA);
		uart_ctl2->US_THR = buf;
#else
		*UART2_CR &= !(AT91C_US_SENDA);// 清除地址位
		*UART2_THR = buf;
#endif
#ifndef USER_485
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY))
			return -1;
#else
		if (!uart_poll(UART2_CSR, AT91C_US_TXEMPTY))
			return -1;
#endif
	}
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
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
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
  	if (!(num & 0x80)) {
#ifndef USER_485
		uart_ctl0->US_CR |= AT91C_US_SENDA;
		uart_ctl0->US_THR = buf;
#else
		*UART0_CR |= AT91C_US_SENDA;		// set SENDA
		*UART0_THR = buf;
#endif
#ifndef USER_485
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY))
			return -1;
#else
		if (!uart_poll(UART0_CSR, AT91C_US_TXEMPTY))
			return -1;
#endif
 	} else {
#ifndef USER_485
		uart_ctl2->US_CR |= AT91C_US_SENDA;
		uart_ctl2->US_THR = buf;
#else
		*UART2_CR |= AT91C_US_SENDA;		// set SENDA
		*UART2_THR = buf;
#endif
		//sel_ch(num);
#ifndef USER_485
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY))
			return -1;
#else
		if (!uart_poll(UART2_CSR, AT91C_US_TXEMPTY))
			return -1;
#endif
	}
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	udelay(1);
	return 0;
}

/*
 * flag: 0->ctl0; 1->clt1
 * receive data from 485, return count is ok, -1 is timeout, -2 is other errors
 * created by wzhe, 10/16/2006
 */
int recv_data(char *buf, unsigned char num)
{
	if (!buf)
		return -EINVAL;
	//AT91_SYS->PIOC_SODR |= AT91C_PIO_PC12;
	//udelay(1);
	if (!(num & 0x80)) {
#ifndef USER_485
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_RXRDY)) {
			return NOTERMINAL;
		}
#else
		if (!uart_poll(UART0_CSR, AT91C_US_RXRDY)) {
			return NOTERMINAL;
		}
#endif
#ifndef USER_485
		if(((uart_ctl0->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl0->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
 			uart_ctl0->US_CR |= AT91C_US_RSTSTA;		// reset status
			*(volatile char *)buf = uart_ctl0->US_RHR;
			uart_ctl0->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			return NOCOM;
		}
		*(volatile char *)buf = uart_ctl0->US_RHR;
#else
		if(((*UART0_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((*UART0_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
 			*UART0_CR |= AT91C_US_RSTSTA;		// reset status
			*(volatile char *)buf = *UART0_RHR;
			*UART0_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			return NOCOM;
		}
		*(volatile char *)buf = *UART0_RHR;
#endif
		return 0;
 	} else {
#ifndef USER_485
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_RXRDY)) {
			return NOTERMINAL;
		}
#else
		if (!uart_poll(UART2_CSR, AT91C_US_RXRDY)) {
			return NOTERMINAL;
		}
#endif
#ifndef USER_485
		if(((uart_ctl2->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl2->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
 			uart_ctl2->US_CR |= AT91C_US_RSTSTA;		// reset status
			*(volatile char *)buf = uart_ctl2->US_RHR;
			uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			return NOCOM;
		}
		*(volatile char *)buf = uart_ctl2->US_RHR;
#else
		if(((*UART2_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((*UART2_CSR & AT91C_US_FRAME) == AT91C_US_FRAME) ) {
 			*UART2_CR |= AT91C_US_RSTSTA;		// reset status
			*(volatile char *)buf = *UART2_RHR;
			*UART2_CR = AT91C_US_TXEN | AT91C_US_RXEN;
 			return NOCOM;
		}
		*(volatile char *)buf = *UART2_RHR;
#endif
		return 0;
	}
	return 0;
}


/*
 * flag: 0->ctl0; 1->clt1
 * receive data from 485, return count is ok, -1 is timeout, -2 is other errors
 * created by wzhe, 10/16/2006
 */
int recv_data_safe(char *buf, unsigned char num)
{
	if (!buf)
		return -EINVAL;
	//AT91_SYS->PIOC_SODR |= AT91C_PIO_PC12;
	//udelay(1);
	if (!(num & 0x80)) {
		// 如果我们现在就有数据收到了, 这肯定是不正确的数据
		if (uart_ctl0->US_CSR & AT91C_US_RXRDY) {
			unsigned char tmp;
			tmp = uart_ctl0->US_RHR;
		}
		if(((uart_ctl0->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl0->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
 			uart_ctl0->US_CR |= AT91C_US_RSTSTA;		// reset status
		}
#ifndef USER_485
		if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_RXRDY)) {
			return NOTERMINAL;
		}
#else
		if (!uart_poll(UART0_CSR, AT91C_US_RXRDY)) {
			return NOTERMINAL;
		}
#endif
#ifndef USER_485
		if(((uart_ctl0->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl0->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
 			uart_ctl0->US_CR |= AT91C_US_RSTSTA;		// reset status
			*(volatile char *)buf = uart_ctl0->US_RHR;
			uart_ctl0->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			return NOCOM;
		}
		*(volatile char *)buf = uart_ctl0->US_RHR;
#else
		if(((*UART0_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((*UART0_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
 			*UART0_CR |= AT91C_US_RSTSTA;		// reset status
			*(volatile char *)buf = *UART0_RHR;
			*UART0_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			return NOCOM;
		}
		*(volatile char *)buf = *UART0_RHR;
#endif
		return 0;
 	} else {
		// 如果我们现在就有数据收到了, 这肯定是不正确的数据
		if (uart_ctl2->US_CSR & AT91C_US_RXRDY) {
			unsigned char tmp;
			tmp = uart_ctl2->US_RHR;
		}
		if(((uart_ctl2->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl2->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
 			uart_ctl2->US_CR |= AT91C_US_RSTSTA;		// reset status
		}
#ifndef USER_485
		if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_RXRDY)) {
			return NOTERMINAL;
		}
#else
		if (!uart_poll(UART2_CSR, AT91C_US_RXRDY)) {
			return NOTERMINAL;
		}
#endif
#ifndef USER_485
		if(((uart_ctl2->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((uart_ctl2->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
 			uart_ctl2->US_CR |= AT91C_US_RSTSTA;		// reset status
			*(volatile char *)buf = uart_ctl2->US_RHR;
			uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			return NOCOM;
		}
		*(volatile char *)buf = uart_ctl2->US_RHR;
#else
		if(((*UART2_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((*UART2_CSR & AT91C_US_FRAME) == AT91C_US_FRAME) ) {
 			*UART2_CR |= AT91C_US_RSTSTA;		// reset status
			*(volatile char *)buf = *UART2_RHR;
			*UART2_CR = AT91C_US_TXEN | AT91C_US_RXEN;
 			return NOCOM;
		}
		*(volatile char *)buf = *UART2_RHR;
#endif
		return 0;
	}
	return 0;
}

int chk_space(void)
{
	int head, tail;
	head = flowptr.head;
	tail = flowptr.tail;
	tail++;
	if (tail > FLOWANUM)
		tail -= FLOWANUM;
	if (tail == head)
		return -1;
	return 0;
}

int get_itm(void)
{
	int itm;
	unsigned char tm[6];
	read_time(tm);
	itm = BCD2BIN(tm[5]);
	itm += BCD2BIN(tm[4]) * 60;
	itm += BCD2BIN(tm[3]) * 3600;
	return itm;
}
//A2消费函数
int pos_call(unsigned char *productnum, term_ram *prterm)
{	
	int i, ret;
	unsigned char temp, commd;
	unsigned char settime[6];
	unsigned char productadd, productdif;
	unsigned int leng;
	unsigned char recvdata[30] = {0};
	unsigned char cardnum[3] = {0};
	unsigned char recvtime[8];
	unsigned char consutype;
	//send default data
	productadd = 0;
	productdif = 0;
	//send head data, 2 bytes
	ret = send_byte(0x06, prterm->term_no);
	if (ret < 0) {
		return 0;
	}
	productdif ^= 0x06;
	ret = send_byte(0x02, prterm->term_no);
	if (ret < 0) {
		return 0;
	}
	productdif ^= 0x02;
	//send len, 1 bytes
	ret = send_byte(0x0A, prterm->term_no);
	if (ret < 0) {
		return 0;
	}
	productdif ^= 0x0A;
	//send production serial number, 3 bytes + 1 byte check
	for (i = 0; i < 3; i++) {
		ret = send_byte(productnum[i], prterm->term_no);
		productdif ^= productnum[i];
	}
	if (ret < 0) {
		return 0;
	}
	productadd = productnum[0] + productnum[1] + productnum[2];
	//printk("pro_add is %x\n", productadd);
	ret = send_byte(productadd, prterm->term_no);
	if (ret < 0) {
		return 0;
	}
	productdif ^= productadd;
	//send res_command, 1 byte
	ret = send_byte(0x91, prterm->term_no);
	if (ret < 0) {
		return 0;
	}
	productdif ^= 0x91;
	//send xor, 1 byte
	ret = send_byte(productdif, prterm->term_no);
	if (ret < 0) {
		return 0;
	}
	//send end data, 1 byte
	ret = send_byte(0x03, prterm->term_no);
	if (ret < 0) {
		return 0;
	}
	//dif_verify is clear
	productdif = 0;
	//recieve the pos data
	for (i = 0; i < 10; i++){
		recv_data((char *)&temp, prterm->term_no);
		if (temp == 0x08){
			recvdata[0] = temp;
			goto next;	
		}
	}
	return 0;
next:
	for (i = 1; i < 26; i++){
		ret = recv_data((char *)&temp, prterm->term_no);
		recvdata[i] = temp;
	} 	
	//compare production serial number
	if ((recvdata[3] != productnum[0]) || (recvdata[4] != productnum[1]) || 
		(recvdata[5] != productnum[2]) ){
		return 0;
	}
	leng = recvdata[2] - 2;
	//printk("leng is %d\n", leng);
	for (i = 0; i < leng; i++){
		productdif ^= recvdata[i];
	} 	
	//compare xor data
	if (recvdata[leng] != productdif){
		//printk("the xor data is wrong, recv %x, but send %x\n", productdif, recvdata[leng]);
		return 0;
	}
	commd = recvdata[8];
	//printk("%x%x%x command is %x\n", productnum[0], productnum[1], productnum[2], commd);
	switch (commd){
		case 0: break;
		case 1: break;
		case 2: break;
		case 3: 
			//printk("card inspire\n");
			cardnum[0] = recvdata[9];
			cardnum[1] = recvdata[10];
			cardnum[2] = recvdata[11];
			ret = send_money(productnum, cardnum, prterm->term_no);
			//printk("card inspire over\n");
			break;
		case 4: 
			//printk("comman 4 start\n");
			//3 bytes card number
			cardnum[0] = recvdata[9];
			cardnum[1] = recvdata[10];
			cardnum[2] = recvdata[11];
			//1 byte consume type
			consutype = recvdata[19];
			//6 bytes 消费时间
			for (i = 0; i < 6; i++){
				recvtime[i] = recvdata[12 + i];
							//printk("recvtime[%d] is %x\n", i, recvtime[i]);
			}
			//2 bytes 消费金额
			for (i = 0; i < 2; i++){
				recvtime[6 + i] = recvdata[20 + i];
			}
			deal_pos(productnum, cardnum, prterm, recvtime, consutype);
			break;
		case 5:
			//read real time
			read_time(settime);
			//printk("current time is %x-%x-%x %x:%x:%x\n", settime[0], settime[1], settime[2], settime[3], settime[4], settime[5]);
			ret = set_para(productnum, prterm->term_no, prterm->pterm->max_consume, settime);
			break;
		default:			// cmd is not recognised, so status is !
	#ifdef DEBUG
	#endif
			prterm->status = -2;
			//ret = -1;
			break;
	}
	return 0;			
}

//#warning "UART NOT USE INT MODE"
//#define DEBUG
// UART 485 叫号函数
static int uart_call(void)
{
	int n;
//#if defined (S_BID) || defined (USRCFG1) || defined (CONFIG_CNTVER)
	static int itm;
//#endif
	static unsigned char tm[6];
	int i, ret, allow;
#ifdef EDUREQ
	int flag;
#endif
	unsigned char sendno;
	unsigned char rdata = 0, commd;
	term_ram *prterm;
	unsigned char produnum[3] = {0};
	//unsigned char product_number[128][3] = {0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0, 0x11,0x50,0x23, 0,0,0, 0x12,0x32,0x81};
	if (paccmain == NULL || pterminal == NULL || ptermram == NULL) {
		printk("pointer is NULL!!!ERROR!\n");
		return -1;
	}
	//printk("enter uart irq\n");
	if (lapnum <= 0 || lapnum > 10) {
		n = LAPNUM;
	} else {
		n = lapnum;
	}
	// 每一次叫号前将flow_sum, 清0, 以记录产生流水数量
	flow_sum = 0;
	read_time(tm);
	if (rcd_info.term_all == 0) {
		//printk("there is no term!\n");
		return 0;
	}
	if (space_remain == 0) {
		//printk("there is no space room\n");
		return 0;
	}
#if defined (S_BID) || defined (USRCFG1)
	// 将读到的时间换算为整数
#if 0
	itm = BCD2BIN(tm[5]);
	itm += BCD2BIN(tm[4]) * 60;
	itm += BCD2BIN(tm[3]) * 3600;
#endif
	itm = _cal_itmofday(tm);
#elif defined (CONFIG_CNTVER)
	itm = mktime(BCD2BIN(tm[0]) + 2000, BCD2BIN(tm[1])/* - 1*/,
		BCD2BIN(tm[2]), BCD2BIN(tm[3]), BCD2BIN(tm[4]),
		BCD2BIN(tm[5]));
#if 0
{
	static int outflag = 0;
	if (outflag == 0) {
		printk("current time: %d-%d-%d %d:%d:%d %d\n", BCD2BIN(tm[0]) + 2000,
			BCD2BIN(tm[1]), BCD2BIN(tm[2]), BCD2BIN(tm[3]),
			BCD2BIN(tm[4]), BCD2BIN(tm[5]), itm);
		outflag = 1;
	}
}
#endif
#endif
#ifdef EDUREQ
	flag = isusertm(tm, &edup);
#endif
	while (n) {
		prterm = ptermram;
		// 叫一圈
		for (i = 0; i < rcd_info.term_all; i++, udelay(ONEBYTETM)) {
			if (chk_space() < 0) {
				printk("tail is %d, head is %d\n", flowptr.tail, flowptr.head);
				return 0;
			}
			//printk("%d: type is %02x\n", prterm->pterm->local_num, prterm->pterm->term_type);
			//AT91_SYS->PIOC_CODR&=(!0x00001000);
			//AT91_SYS->PIOC_SODR|=0x00001000;
			sel_ch(prterm->pterm->local_num);// 任何情况下, 先将138数据位置位
			prterm->status = STATUSUNSET;
			
			//printk("term no is %d\n", prterm->pterm->local_num);
			if (prterm->pterm->local_num < 128)	{
				//A2消费机
				//定位A2消费机机器号
				produnum[0] = productnumber[prterm->term_no][0];
				produnum[1] = productnumber[prterm->term_no][1];
				produnum[2] = productnumber[prterm->term_no][2];
				if ((produnum[0] ==0x00) && (produnum[0] ==0x00) && (produnum[0] ==0x00)){
					prterm++;
					continue;
				}
				pos_call(produnum, prterm);
				
			} else {
				sendno = prterm->pterm->local_num & 0x7F;
				prterm->add_verify = 0;
				prterm->dif_verify = 0;
				//公司消费终端
				ret = send_addr(sendno, prterm->pterm->local_num);
				if (ret < 0) {
#ifdef UARTDBG
#warning "uart debug!!!"
					printk("485DBG: send addr %d error\n", prterm->pterm->local_num);
#endif
					prterm->status = NOTERMINAL;
					prterm++;
					continue;
				}
				// receive data, if pos number and no cmd then end
				// if pos number and cmd then exec cmd
				// else wrong!
				ret = recv_data_safe((char *)&rdata, prterm->term_no);
				if (ret < 0) {
#ifdef UARTDBG
					printk("485DBG: recv %d error: %d, no: %d\n", prterm->pterm->local_num, rdata, ret);
#endif
					prterm->status = ret;
					prterm++;
					continue;
				}
				prterm->add_verify += rdata;
				prterm->dif_verify ^= rdata;
				if ((rdata & 0x7F) != sendno) {
					prterm->status = NOCOM;
					prterm++;
					continue;
				}
				if (!(rdata & 0x80)) {		// no command
					prterm->status = TNORMAL;
					prterm++;
					continue;
				}
				// has command, then receive cmd
				ret = recv_data((char *)&commd, prterm->term_no);
				if (ret < 0) {
#ifdef UARTDBG
					printk("485DBG: recv %d cmd error: no %d\n", prterm->pterm->local_num, ret);
#endif
					prterm->status = ret;
					prterm++;
					continue;
				}
				prterm->add_verify += commd;
				prterm->dif_verify ^= commd;
				switch (commd) {
				case SENDLEIDIFNO:
					if ((net_status == 0) && (le_allow == 0)) {
						allow = 0;
					} else {
						allow = 1;
					}
#if defined EDUREQ
					ret = recv_le_id(prterm, allow, flag);
#elif defined (S_BID) || defined (USRCFG1) || defined (CONFIG_CNTVER)
					ret = recv_le_id(prterm, allow, itm);
#else
					ret = recv_le_id(prterm, allow);
#endif
					break;
				case SENDRUNDATA:
	//#ifdef CONFIG_QH_WATER
	//				ret = send_run_data(prterm, tm);
	//#else
					ret = send_run_data(prterm);
	//#endif
#ifdef DEBUG
					if (ret == 0) {
						printk("term: %d send run data OK!\n", prterm->term_no);
					} else {
						printk("term: %d send run data error!\n", prterm->term_no);
					}
#endif
					break;
				case SENDRUNDATA2:
					ret = send_run_data2(prterm, tm);
#ifdef DEBUG
					if (ret == 0) {
						printk("term: %d send run data OK!\n", prterm->term_no);
					} else {
						printk("term: %d send run data error!\n", prterm->term_no);
					}
#endif
					break;
				case RECVNO:
					ret = ret_no_only(prterm);
#ifdef DEBUG
					if (ret == 0) {
						printk("term: %d cmd RECVNO OK!\n", prterm->term_no);
					} else {
						printk("term: %d cmd RECVNO error!\n", prterm->term_no);
					}
#endif
					break;
				case RECVLEHEXFLOW:
					//space_remain--;
#if defined EDUREQ
					ret = recv_leflow(prterm, 0, tm, flag);
#elif defined CONFIG_CNTVER
					ret = recv_leflow(prterm, 0, tm, itm);
#else
					ret = recv_leflow(prterm, 0, tm, 0);
#endif
#ifdef DEBUG
					if (ret == 0) {
						printk("term: %d cmd RECVLEHEXFLOW OK!\n", prterm->term_no);
					} else {
						printk("term: %d cmd RECVLEHEXFLOW error!\n", prterm->term_no);
					}
#endif
					break;
				case RECVLEFLOWX:	// HEX, modified by wjzhe
					ret = recv_leflow(prterm, 0x2, tm, 0);
					break;

				case RECVLEBCDFLOW:
					//space_remain--;
#ifdef EDUREQ
					ret = recv_leflow(prterm, 1, tm, flag);
#elif defined CONFIG_CNTVER
					ret = recv_leflow(prterm, 1, tm, itm);
#else
					ret = recv_leflow(prterm, 1, tm, 0);
#endif
#ifdef DEBUG
					if (ret == 0) {
						printk("term: %d cmd RECVBCDFLOW OK!\n", prterm->term_no);
					} else {
						printk("term: %d cmd RECVBCDFLOW error!\n", prterm->term_no);
					}
#endif
					break;
				case RECVTAKEFLOW:
					//space_remain--;
					ret = recv_take_money(prterm, tm, 0);
					break;
				case RECVDEPFLOW:
					//space_remain--;
					ret = recv_dep_money(prterm, tm, 0);
					break;
#ifdef CPUCARD
				case RECVCPUIDIFON:
					ret = recv_cpu_id(prterm, tm);
					break;
				case RECVCPUFLOW:
				case RECVCPUFLOW1:
					//space_remain--;
					ret = recv_cpuflow(prterm);
					break;
#endif /* end CPUCARD */
				case NOCMD:
					break;
				case RECVREFUND:
					ret = recv_leflow(prterm, 1, tm, 1);
					break;
				case SENDLEIDINFO2:		// 发送光电卡账户信息, 增加账号和时间
					if ((net_status == 0) && (le_allow == 0)) {
						allow = 0;
					} else {
						allow = 1;
					}
					ret = recv_le_id2(prterm, allow, tm, 0);
					break;
				case SENDLEIDACCX:	// 北大发送余额和账号
					if ((net_status == 0) && (le_allow == 0)) {
						allow = 0;
					} else {
						allow = 1;
					}
					ret = recv_le_id5(prterm, allow, itm);
					break;
				case RECVLEHEXFLOW2:	// 上传光电卡流水, 接收时间
					ret = recv_leflow2(prterm, 0, 0);
					break;
				case RECVLEIDINFO3:
					if ((net_status == 0) && (le_allow == 0)) {
						allow = 0;
					} else {
						allow = 1;
					}
					ret = recv_le_id4(prterm, allow, 0, itm);
					break;
				
				case RECVRFDLEID:
					if ((net_status == 0) && (le_allow == 0)) {
						allow = 0;
					} else {
						allow = 1;
					}
					ret = recv_le_id3(prterm, allow, itm);
					break;
				case RECVRFDDEPFLOW:	// 接收出纳机存款流水, 修改校验
				case RECVDEPFLOWX:
					ret = recv_dep_money(prterm, tm, 1);
					break;
				case RECVTAKEFLOWX:		// 接收出纳机取款流水, 修改校验
					ret = recv_take_money(prterm, tm, 1);
					break;
				case RECVRFDLEFLOW:		// 接收退款机消费流水, 修改校验
					ret = recv_leflow(prterm, 1, tm, REFUNDBIT);
					break;
				case RECVRFDLEFLOW2:	// 接收消费机消费流水, 修改校验
					ret = recv_leflow2(prterm, 0, 1);
					break;
				case RECVLEIDGDZJ://实现账户0元0折扣功能，write by duyy, 2014.1.16
					if ((net_status == 0) && (le_allow == 0)) {
						allow = 0;
					} else {
						allow = 1;
					}
					ret = recv_le_gdzj(prterm, allow, 0, itm);
				break;

#ifdef CONFIG_ZLYY
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
				//write by duyy, 2012.3.15
				case SENDLEID_ZLYY3C:
					if ((net_status == 0) && (le_allow == 0)) {
						allow = 0;
					} else {
						allow = 1;
					}
					allow = 1;
					ret = recv_leid_zlyy3(prterm, allow, 1, itm);	
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
				case 0x9b:		// 支持两个版本
					ret = recv_leflow_zlyy(prterm, tm);
					break;
				case SENDTIME_ZLYY:
					ret = send_time_zlyy(prterm, tm, itm);
					break;
#endif	/* CONFIG_ZLYY */

				case RECVRFDLEIDINFO:	// 接收消费机账户信息, 修改校验
					if ((net_status == 0) && (le_allow == 0)) {
						allow = 0;
					} else {
						allow = 1;
					}
					ret = recv_le_id2(prterm, allow, tm, 1);
					break;
				default:			// cmd is not recognised, so status is !
#ifdef DEBUG
					printk("term: %d unknown cmd %02x!\n", prterm->term_no, commd);
#endif
					prterm->status = -2;
					ret = -1;
					break;
				}
#ifdef DEBUG
				printk("%d cmd is %02x, ret = %d\n", prterm->term_no, commd, ret);
#endif
			}//end if(prterm->pterm->local_num < 128)
			if (prterm->status == STATUSUNSET)
				prterm->status = TNORMAL;
			prterm++;
			//udelay(ONEBYTETM);	// add for..., fixed by wjzhe
			if (space_remain == 0)
				return 0;
		}//end for
		n--;
	}// end while
	total += flow_sum;
	return 0;
}
#undef DEBUG



/************************************************************	
		驱动相关设置
*************************************************************/
static int uart_open(struct inode *inode, struct file *filp)
{
	// config PIOC
	// reset US2 status
/*	ctl2->US_CR |= AT91C_US_RSTSTA;
	ctl2->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	ctl2->US_BRGR = AT91C_MASTER_CLOCK / 16 / BAUDRATE;// 0x82;
	// 一个停止位, 多点模式, 8位, 采用滤波
	ctl2->US_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP | AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
	// reset US0 status
	ctl0->US_CR |= AT91C_US_RSTSTA;
	ctl0->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	ctl0->US_BRGR = AT91C_MASTER_CLOCK / 16 / BAUDRATE;
	// 一个停止位, 多点模式, 8位, 采用滤波
	ctl0->US_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP | AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
//	*(volatile unsigned int *)(UART2_IER)=(AT91C_US_TXRDY);
//	*(volatile unsigned int *)(UART2_IDR)=(!(AT91C_US_TXRDY));
*/
	//ctl2->US_IER = AT91C_US_TXRDY;
	//ctl2->US_IDR = !(AT91C_US_TXRDY);
	AT91_SYS->PMC_PCER |= AT91C_ID_PIOC;

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
//	*(volatile unsigned int *)(AIC_SMR6)=0x00000027;

	AT91_SYS->PMC_PCER |= 0x00000140;

#ifndef USER_485
	uart_ctl0->US_CR |= AT91C_US_RSTSTA;
	uart_ctl0->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	uart_ctl0->US_BRGR = 0x61;//0x82; //2012.5.9
	uart_ctl0->US_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_NONE
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
	
	uart_ctl2->US_CR |= AT91C_US_RSTSTA;
	uart_ctl2->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	uart_ctl2->US_BRGR = 0x82;	
	uart_ctl2->US_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
#else
	*UART2_CR |= AT91C_US_RSTSTA;
	*UART2_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	*UART2_BRGR = 0x82;	
	*UART2_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_MULTI_DROP
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
	
	*UART0_CR |= AT91C_US_RSTSTA;
	*UART0_CR = AT91C_US_TXEN | AT91C_US_RXEN;
	*UART0_BRGR = 0x61;//0x82;	//2012.5.9
	*UART0_MR = AT91C_US_NBSTOP_1_BIT | AT91C_US_PAR_NONE
		| AT91C_US_CHRL_8_BITS | AT91C_US_FILTER;
#endif
//	*UART2_IER = AT91C_US_TXRDY;
//	*UART2_IDR = !(AT91C_US_TXRDY);
	// 下面进行数据初使化
	total = 0;
	maxflowno = 0;
	flow_sum = 0;
	flowptr.head = flowptr.tail = 0;
	memset(&rcd_info, 0, sizeof(rcd_info));
	lapnum = LAPNUM;
#ifdef CONFIG_ACCSW_USE_HASH
	// 这个时候要初始化hashtab
	hash_accsw = hashtab_create(def_hash_value, def_hash_cmp, MAXSWACC);
	if (hash_accsw == NULL) {
		return -ENOMEM;
	}
	printk(KERN_WARNING "Uart accsw hash table establish\n");
#endif
	return 0;
}

//#undef pr_debug
//#define UART_DEBUG
#ifdef UART_DEBUG
#define pr_debug(fmt,arg...) \
	printk("%s %d: "fmt,__FILE__,__LINE__,##arg)
#else
#define pr_debug(fmt,arg...) \
	do { } while (0)
#endif

static int uart_ioctl(struct inode* inode, struct file* file, unsigned int cmd, unsigned int arg)
{
	int ret = 0;
	int i, flow_head, flow_tail, flow_total;
	terminal *ptem;
	term_ram *ptrm;
	struct uart_ptr ptr;
	no_money_flow no_m_flow;
	money_flow m_flow;
	struct get_flow pget;
	// 进入临界区
	down(&uart_lock);
	switch(cmd)
	{
	case SETLAPNUM:
		lapnum = (int)arg;
		break;
	case SETPRONUM: //设置A2终端生产序列号
		memset(&productnumber, 0, sizeof(productnumber));
		memcpy(productnumber, (unsigned char*)arg, sizeof(productnumber));
		break;
	case INITUART:		// 初使化所有参数
		pr_debug("paccmain = %p, rcd_info.account_main = %d, free it\n", paccmain, rcd_info.account_main);
		if (paccmain) {
			if ((rcd_info.account_main * sizeof(acc_ram)) > SZ_128K)
				vfree(paccmain);
			else
				kfree(paccmain);
			paccmain = NULL;
		}
#ifdef M1CARD
		if (badflow.pbadflow) {
			if ((badflow.total * sizeof(flow)) > (4 * 1024)) {
				vfree(badflow.pbadflow);
			} else {
				kfree(badflow.pbadflow);
			}
		}
		memset(&badflow, 0, sizeof(badflow));
#endif
		pr_debug("copy uart_ptr...\n");
		if ((ret = copy_from_user(&ptr, (struct uart_ptr *)arg, sizeof(ptr))) < 0) {
			ret = -EFAULT;
			break;
		}
		// 初使化最大流水号
		maxflowno = ptr.max_flow_no;
		// 初使化账户库
		rcd_info.account_main = rcd_info.account_all = ptr.acc_main_all;
		rcd_info.account_sw = 0;
		pr_debug("acc ram size %d\n", rcd_info.account_main * sizeof(acc_ram));
		// 有账户库大于4K, 才用vmalloc申请空间
		if ((rcd_info.account_main * sizeof(acc_ram)) > SZ_128K) {
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
		pr_debug("copy acc ram...\n");
		if ((ret = copy_from_user(paccmain, ptr.paccmain,
			rcd_info.account_main * sizeof(acc_ram))) < 0) {
			printk("UART: copy account main failed!\n");
			ret = -EFAULT;
			break;
		}
#if 0
{
	int i;
	printk("%d\n", sizeof(acc_ram));
	for (i = 0; i < rcd_info.account_main; i++) {
#if 0
		printk("U-->%d, %d, %x, draw = %d, alarm = %d\n",
			paccmain[i].acc_num, paccmain[i].money, paccmain[i].card_num,
			paccmain[i].m_draw, paccmain[i].m_alarm);
#endif
		if (paccmain[i].card_num == 618675001) {
			printk("sizeof acc_ram = %d\n", sizeof(acc_ram));
			printk("%d: alarm = %d, draw = %d\n", (int)paccmain[i].card_num,
				paccmain[i].m_alarm, paccmain[i].m_draw);
		}
		if (paccmain[i].card_num == 500127481) {
			printk("%d: alarm = %d, draw = %d\n", (int)paccmain[i].card_num,
				paccmain[i].m_alarm, paccmain[i].m_draw);
		}
	}
	printk("\n");
}
#endif
		// 初使化管理费
		for (i = 0; i < 16; i++) {
			fee[i] = ptr.fee[i];
		}
		pr_debug("free term ram...\n");
		// 初使化终端库
		if (ptermram) {
			kfree(ptermram);
			ptermram = NULL;
		}
		pr_debug("free terminal...\n");
		if (pterminal) {
			kfree(pterminal);
			pterminal = NULL;
		}
		rcd_info.term_all = ptr.term_all;
		ptermram = (term_ram *)kmalloc(rcd_info.term_all * sizeof(term_ram), GFP_KERNEL);
		pterminal = (terminal *)kmalloc(rcd_info.term_all * sizeof(terminal), GFP_KERNEL);
		pr_debug("copy terminal...\n");
		memset(pterminal, 0, rcd_info.term_all * sizeof(terminal));
		if ((ret = copy_from_user(pterminal, ptr.pterm, rcd_info.term_all * sizeof(terminal))) < 0) {
			ret = -EFAULT;
			break;
		}
				
		ptem = pterminal;
		ptrm = ptermram;
		pr_debug("set terminal...\n");
		for (i = 0; i < rcd_info.term_all; i++) {
			ptrm->pterm = ptem;
			ptrm->term_no = ptem->local_num;
			ptrm->status = STATUSUNSET;
			ptrm->term_cnt = 0;
			ptrm->term_money = 0;
			ptrm->flow_flag = 0;
#ifdef EDUREQ
			ptrm->money = 0;
#endif
#ifdef WATER
			// 为了兼容二版水控, 这里要加入临时变量区初始化
			memset(&ptrm->w_flow, 0, sizeof(ptrm->w_flow));
#ifdef CONFIG_QH_WATER
			ptrm->is_use = ptem->price[31];		// offset of term param
			ptrm->is_limit = ptem->term_type;
#endif
#endif
			//printk("term_no is %d\n", ptrm->term_no);//2012.5.11
			ptem++;
			ptrm++;
		}
		flowptr.head = flowptr.tail = 0;
		flow_sum = 0;
		total = 0;
		// 断网光电卡处理标志
		le_allow = ptr.le_allow;
//		printk("le allow %d, %d\n", le_allow, ptr.le_allow);
		//net_status = ptr.pnet_status;
		if ((ret = get_user(net_status, (int *)ptr.pnet_status)) < 0) {
			ret = -EFAULT;
			break;
		}
		break;
	case SETRTCTIME:
		//copy_from_user(&tm, , sizeof(tm));
		set_RTC_time((struct rtc_time *)arg, 1);
		break;
	case GETRTCTIME:
		read_RTC_time((struct rtc_time *)arg, 1);
		//copy_to_user(, &tm2, sizeof(tm2));
		break;
	case START_485:
#ifdef U485INT
		enable_usart0_int();			// 开启发送中断---->uart0
#else
		//spin_lock(&uart_lock);
		if (uart_call() < 0)
			ret = -EIO;
		//spin_unlock(&uart_lock);
#endif
		break;
	case SETREMAIN:
		space_remain = *(volatile int *)arg;	// 笔
		if (space_remain > 147250) {
			printk("space remain is too large: %d\n", space_remain);
			space_remain = 0;
			ret = -ERANGE;
			break;
		}
		if (space_remain < 0) {
			space_remain = 0;
		}
		break;
	case GETFLOWSUM:
		if (arg) {
			//flow_head = flowptr.head;
			//flow_tail = flowptr.tail;
			//if (flow_tail < flow_head) {
			//	flow_total = flow_tail + FLOWANUM - flow_head;
			//} else {
			//	flow_total = flow_tail - flow_head;
			//}
			if ((ret = put_user(flow_sum, (int *)arg)) < 0) {
				ret = -EFAULT;
				break;
			}
		} else {
			ret = -EFAULT;
			break;
		}
		break;
	case GETFLOWNO:
		if (arg) {
			if ((ret = put_user(maxflowno, (int *)arg)) < 0) {
				ret = -EFAULT;
				break;
			}
		} else {
			ret = -EFAULT;
			break;
		}
		break;
	case ADDACCSW:
		// 处理异区非现金流水
		if (!arg) {
			printk("UART: no flow can deal!\n");
			break;
		}
		//pno_money_flow = (no_money_flow *)kmalloc(sizeof(no_money_flow), GFP_KERNEL);
		//if (pno_money_flow == NULL) {
		//	printk("init no money flow failed!\n");
		//	return -1;
		//}
		if ((ret = copy_from_user(&no_m_flow, (no_money_flow *)arg,
			sizeof(no_money_flow))) < 0) {
			printk("UART: copy from user no money flow failed\n");
			//kfree(pno_money_flow);
			ret = -EFAULT;
			break;
		}
		// 进行数据处理
#ifndef CONFIG_ACCSW_USE_HASH
		if (deal_no_money(&no_m_flow, paccmain, paccsw, &rcd_info) < 0) {
			printk("UART: deal no money flow error\n");
			//kfree(pno_money_flow);
			ret = -EINVAL;
			break;
		}
#else
		if (deal_no_money(&no_m_flow, paccmain, hash_accsw, &rcd_info) < 0) {
			printk("UART: deal no money flow error\n");
			//kfree(pno_money_flow);
			ret = -EINVAL;
			break;
		}
#endif
		//kfree(pno_money_flow);
		//pno_money_flow = NULL;
		ret = 0;
		break;
	case CHGACC:
		// 处理异区现金流水
		if (!arg) {
			printk("UART: no flow can deal!\n");
			break;
		}
		//pmoney_flow = (money_flow *)kmalloc(sizeof(money_flow), GFP_KERNEL);
		//if (pmoney_flow == NULL) {
		//	printk("UART: init money flow failed!\n");
		//	return -1;
		//}
		if ((ret = copy_from_user(&m_flow, (money_flow *)arg,
			sizeof(money_flow))) < 0) {
			printk("UART: copy from user money flow failed\n");
			//kfree(pmoney_flow);
			ret = -EFAULT;
			break;
		}
#ifndef CONFIG_ACCSW_USE_HASH
		// 进行数据处理
		if (deal_money(&m_flow, paccmain, paccsw, &rcd_info) < 0) {
			printk("UART: deal money flow error\n");
			//kfree(pmoney_flow);
			ret = -EINVAL;
			break;
		}
#else
		if (deal_money(&m_flow, paccmain, hash_accsw, &rcd_info) < 0) {
			printk("UART: deal money flow error\n");
			//kfree(pmoney_flow);
			ret = -EINVAL;
			break;
		}
#endif
		//kfree(pmoney_flow);
		//pmoney_flow = NULL;
		ret = 0;
		break;
	case CHGACC1:		// 批量处理异区现金流水
	{
		money_flow *pmflow = NULL;
		// 先拷贝出流水数量
		ret = copy_from_user(&i, (int *)arg, sizeof(int));
		if (ret < 0) {
			ret = -EFAULT;
			printk("UART: copy from user money flow count failed\n");
			break;
		}
		if (i <= 0) {
			printk("UART: money flow count below zero\n");
			break;
		}
		arg += sizeof(int);
		// 再得到流水指针
		ret = copy_from_user(&pmflow, (money_flow *)arg, sizeof(pmflow));
		if (ret < 0) {
			printk("UART: copy from user money flow ptr failed\n");
			break;
		}
		// 顺次处理
		while (i) {
			// 处理异区现金流水
			if ((ret = copy_from_user(&m_flow, pmflow, sizeof(money_flow))
				) < 0) {
				ret = -EFAULT;
				printk("UART: copy from user money flow failed\n");
				break;
			}
#ifndef CONFIG_ACCSW_USE_HASH
			// 进行数据处理
			if (deal_money(&m_flow, paccmain, paccsw, &rcd_info) < 0) {
				printk("UART: deal money flow error\n");
			}
#else
			if (deal_money(&m_flow, paccmain, hash_accsw, &rcd_info) < 0) {
				printk("UART: deal money flow error\n");
			}
#endif
			pmflow += 1;
			i--;
		}
	}
		break;
	case GETFLOWPTR:// 慎用
		if (!arg) {
			ret = -EINVAL;
			break;
		}
		if ((ret = copy_to_user((struct flow_info *)arg,
			&flowptr, sizeof(flowptr))) < 0) {
			ret = -EFAULT;
			break;
		}
		break;
	case SETFLOWPTR:// 慎用
		if (!arg) {
			ret = -EINVAL;
			break;
		}
		if ((ret = copy_from_user(&flowptr, (struct flow_info *)arg,
			sizeof(flowptr))) < 0) {
			ret = -EFAULT;
			break;
		}
		break;
	case GETFLOW:
		if (!arg) {
			printk("UART: no flow can deal!\n");
			break;
		}
		if ((ret = copy_from_user(&pget, (struct get_flow *)arg,
			sizeof(pget))) < 0) {
			ret = -EINVAL;
			break;
		}
		flow_head = flowptr.head;
		flow_tail = flowptr.tail;
		if (flow_tail < flow_head) {
			flow_total = flow_tail + FLOWANUM - flow_head;
		} else {
			flow_total = flow_tail - flow_head;
		}
		if (flow_total == 0) {
			printk("UART: no flow\n");
		}
		if (pget.cnt > flow_total) {
			printk("UART: cannot read so many flow: %d\n", pget.cnt);
			ret = -EINVAL;
			break;
		}
		// 将pget.cnt笔流水拷入用户空间, 进行存储
		//i = 0;
		//nleft = pget.cnt % 25;
		//pget.cnt -= nleft;
		for (i = 0; i < pget.cnt; i++) {
			//printk("flow head: %d\n", flow_head);
			if ((ret = copy_to_user(pget.pflow,
				pflow + flow_head, sizeof(flow))) < 0) {
				ret = -EFAULT;
				break;
			}
			flow_head ++;
			pget.pflow++;
			if (flow_head >= FLOWANUM) {
				flow_head -= FLOWANUM;
			}
		}
		break;
	case GETACCINFO:
		if (!arg) {
			ret = -EINVAL;
			break;
		}
		if (rcd_info.account_all != rcd_info.account_main + rcd_info.account_sw) {
			printk("UART: acc_all != acc_main + acc_sw: %d, %d, %d\n",
				rcd_info.account_all, rcd_info.account_main, rcd_info.account_sw);
			rcd_info.account_all = rcd_info.account_main + rcd_info.account_sw;
		}
		if ((ret = copy_to_user((struct record_info *)arg,
			&rcd_info, sizeof(rcd_info))) < 0) {
			ret = -EFAULT;
			break;
		}
		break;
	case GETACCMAIN:
		if (!arg || !paccmain) {
			ret = -EINVAL;
			break;
		}
		if (rcd_info.account_main == 0)
			break;
		//pbuf = (unsigned char *)paccmain;
		//for (i = 0; i < (sizeof(acc_ram) * rcd_info.account_main); i++) {
		//	if (put_user(*pbuf, (unsigned char *)arg) < 0)
				//return -1;
		//	pbuf ++;
		//	arg ++;
		//}
		//ret = 0;
		if ((ret = copy_to_user((acc_ram *)arg, paccmain,
			sizeof(acc_ram) * rcd_info.account_main)) < 0) {
			ret = -EFAULT;
			break;
		}
		// 读取帐户信息后将帐户空间清除
		if ((rcd_info.account_main * sizeof(acc_ram)) > SZ_128K)
			vfree(paccmain);
		else
			kfree(paccmain);
		paccmain = NULL;
		break;
	case GETACCSW:
		if (!arg) {
			ret = -EINVAL;
			break;
		}
		if (rcd_info.account_sw == 0) {
			break;
		}
#ifndef CONFIG_ACCSW_USE_HASH
		if ((ret = copy_to_user((acc_ram *)arg, paccsw,
			sizeof(acc_ram) * rcd_info.account_sw)) < 0) {
			ret = -EINVAL;
			break;
		}
#else
{
		acc_ram *v;
		v = kmalloc(sizeof(acc_ram) * hash_accsw->nel, GFP_KERNEL);
		if (v == NULL) {
			ret = -ENOMEM;
			break;
		}
		ret = hashtab_getall(hash_accsw, v, sizeof(acc_ram));
		if (ret != hash_accsw->nel) {
			printk("warning hashtab get all return %d, but %d\n",
				ret, hash_accsw->nel);
		}
		// 排序吗?
		ret = copy_to_user((void *)arg, v, sizeof(acc_ram) * ret);
		kfree(v);
}
#endif
		break;
	case SETNETSTATUS:
		if (!arg) {
			ret = -EINVAL;
			break;
		}
		if ((ret = get_user(net_status, (int *)arg)) < 0) {
			ret = -EFAULT;
			break;
		}
		break;
	case SETLEALLOW:		// 设置光电卡能否断网时消费
		ret = copy_from_user(&le_allow, (void *)arg, sizeof(le_allow));
		break;
#ifdef EDUREQ
	case SETEDUPAR:
		memset(&edup, 0, sizeof(edup));
		ret = copy_from_user(&edup, (void *)arg, sizeof(edup));
		if (ret < 0) {
			printk("SETEDUPAR: copy from user edup failed\n");
			goto out;
		}
#if 0
		printk("edup: %d, %d, %d, %d, %d\n",
			edup.cn, edup.figure, edup.rate, edup.stime1, edup.etime1);
#endif
		{
			term_ram *pterm;
			unsigned short list[ETERMNUM];
			pterm = ptermram;
			arg += sizeof(edup);
			ret = copy_from_user(list, (void *)arg, sizeof(list));
			if (ret < 0) {
				printk("SETEDUPAR: copy from user list failed\n");
				goto out;
			}
			for (i = 0; i < rcd_info.term_all; i++) {
				if (pterm->term_no < 256 && pterm->term_no >= 0) {
					pterm->money = list[pterm->term_no];
#if 0
					printk("term no: %d, %d, %d, %d, %d\n",
						pterm->term_no, pterm->money);
#endif
				}
				pterm++;
			}
		}
out:
		break;
#endif
#ifdef S_BID
	case SETBIDINFO:
		if ((ret = copy_from_user(&bidinfo, (struct bid_info *)arg,
			sizeof(bidinfo))) < 0)
			;
		break;
#endif
#ifdef USRCFG1
	case SETUSRCFG:
		if ((ret = copy_from_user(&usrcfg, (struct usr_cfg *)arg,
			sizeof(usrcfg))) < 0)
			;
		break;
	case SETTERMONEY:
		if ((ret = copy_from_user(&termoney, (struct term_money *)arg,
			sizeof(termoney))) < 0) {
		}
		break;
#endif
#ifdef CONFIG_RECORD_CASHTERM
	case GETCASHBUF:
		if (cashterm_ptr) {
			ret = copy_to_user((void *)arg, cashbuf,
				sizeof(struct cash_status) * cashterm_ptr);
			if (ret < 0) {
				printk("Uart ioctl GETCASHBUF copy_to_user error\n");
			} else {
				ret = cashterm_ptr;
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
		}
		break;
#endif
	case GETWATERFLOW:
#ifdef WATER
		// 返回收到的流水数??
		ret = 0;
		ptrm = ptermram;
		for (i = 0; i < rcd_info.term_all; i++, ptrm++) {
			if (record_water_flow(ptrm)) {
#ifdef DEBUG
				printk("term %d has flow, save it over\n", ptrm->term_no);
#endif
				ret++;		// 记录条数
			}
		}
		put_user(ret, (int *)arg);
#ifdef DEBUG
		printk("recv water flow: %d\n", ret);
#endif
		ret = 0;
#else
		put_user((int)0, (int *)arg);
#endif
		break;
#ifdef CONFIG_CNTVER
	case SETCNTVER:
		if ((ret = copy_from_user(&cntcfg, (void *)arg, sizeof(cntcfg))) < 0) {
			printk("uart: ioctl setcntver error\n");
		}
		break;
	case GETFCNTVER:
		if ((ret = copy_to_user((void *)arg, fcntver, sizeof(struct f_cntver) * fcntpos)) < 0) {
			printk("uart: ioctl getfcntver error\n");
		} else {
			ret = fcntpos;
			fcntpos = 0;
		}
		break;
#if 0
	case GETCNTFLOW:
		if ((ret = copy_to_user((void *)arg, cfbuf, sizeof(struct cntflow) * cfpos)) < 0) {
			printk("uart: ioctl GETCNTFLOW error\n");
		} else {
			ret = cfpos;
			cfpos = 0;
		}
		break;
#endif
	case SETCNTFLOW:
	{
		struct cnt_io cntio;
		struct cntflow *pcf;
		if ((ret = copy_from_user(&cntio, (void *)arg, sizeof(cntio))) < 0) {
			printk("uart: SETCNTFLOW cp cntio error\n");
			break;
		}
		pcf = kmalloc(sizeof(struct cntflow) * cntio.n, GFP_KERNEL);
		if (pcf == NULL) {
			ret = -ENOMEM;
			printk("uart: SETCNTFLOW kmalloc error\n");
			break;
		}
		ret = copy_from_user(pcf, cntio.p, sizeof(struct cntflow) * cntio.n);
		if (ret < 0) {
			printk("uart: SETCNTFLOW cp cntflow error\n");
			kfree(pcf);
			break;
		}
		// 处理数据
		deal_cnt_flow(pcf, cntio.n);
		kfree(pcf);
	}
		break;
#endif
	case GETTERMCNT:
		put_user(g_tcnt, (int *)arg);
		break;
	default:
		ret = -1;
		break;
	}
	// 离开临界区
	up(&uart_lock);
	return ret;
}


static int uart_write(struct file *file, const char *buff, size_t count, loff_t *offp)
{
	int cnt;
	// 写入FRAM遗留流水
	// 只允许在流水区初使化的时候用此函数
	if (count % sizeof(flow) != 0 || buff == NULL) {
		return -EINVAL;
	}
	cnt =  count / sizeof(flow);
	if (cnt >= 25) {
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
}


static int uart_read(struct file *file, char *buff, size_t count, loff_t *offp)
{
	term_info *pterm_info;
	int i;
	term_ram *ptrm = ptermram;
	unsigned int cnt;
	if (count % sizeof(term_info) != 0 || buff == NULL)
		return -EINVAL;
	if (count == 0) {
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
		if (ptrm->pterm->term_type & (1 << CASH_TERMINAL_FLAG)) {
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
#ifdef CONFIG_ACCSW_USE_HASH
	hashtab_destroy_d(hash_accsw);
	hash_accsw = NULL;
#endif
	// 释放资源, modified by wjzhe 20110214
	if (paccmain) {
		if ((rcd_info.account_main * sizeof(acc_ram)) > SZ_128K)
			vfree(paccmain);
		else
			kfree(paccmain);
		paccmain = NULL;
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
#ifdef USER_485
	UART2_CR	= ioremap(0xFFFC8000,0x04);
	UART2_MR	= ioremap(0xFFFC8004,0x04);
	UART2_IER	= ioremap(0xFFFC8008,0x04);
	UART2_IDR	= ioremap(0xFFFC800C,0x04);
	UART2_IMR	= ioremap(0xFFFC8010,0x04);
	UART2_CSR	= ioremap(0xFFFC8014,0x04);
	UART2_RHR	= ioremap(0xFFFC8018,0x04);
	UART2_THR	= ioremap(0xFFFC801C,0x04);
	UART2_BRGR	= ioremap(0xFFFC8020,0x04);
	UART2_RTOR	= ioremap(0xFFFC8024,0x04);
	UART2_TTGR	= ioremap(0xFFFC8028,0x04);
	UART2_FIDI	= ioremap(0xFFFC8040,0x04);
	UART2_NER	= ioremap(0xFFFC8044,0x04);
	UART2_IF	= ioremap(0xFFFC804C,0x04);
	AIC_SMR8	= ioremap(0xFFFFF020,0x04);
	
	UART0_CR	= ioremap(0xFFFC0000,0x04);
	UART0_MR	= ioremap(0xFFFC0004,0x04);
	UART0_IER	= ioremap(0xFFFC0008,0x04);
	UART0_IDR	= ioremap(0xFFFC000C,0x04);
	UART0_IMR	= ioremap(0xFFFC0010,0x04);
	UART0_CSR	= ioremap(0xFFFC0014,0x04);
	UART0_RHR	= ioremap(0xFFFC0018,0x04);
	UART0_THR	= ioremap(0xFFFC001C,0x04);
	UART0_BRGR	= ioremap(0xFFFC0020,0x04);
	UART0_RTOR	= ioremap(0xFFFC0024,0x04);
	UART0_TTGR	= ioremap(0xFFFC0028,0x04);
	UART0_FIDI	= ioremap(0xFFFC0040,0x04);
	UART0_NER	= ioremap(0xFFFC0044,0x04);
	UART0_IF	= ioremap(0xFFFC004C,0x04);
	AIC_SMR6	= ioremap(0xFFFFF018,0x04);
#endif
#ifdef U485INT
	res = request_irq(UART0_IRQ, UART_irq_handler, 0, "uart", NULL);
	if (res < 0) {
		printk("AT91 UART IRQ requeset failed!\n");
		return res;
	}
	enable_irq(UART0_IRQ);
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
#ifdef USER_485
	UART2_CR	= iounmap(0xFFFC8000);
	UART2_MR	= iounmap(0xFFFC8004);
	UART2_IER	= iounmap(0xFFFC8008);
	UART2_IDR	= iounmap(0xFFFC800C);
	UART2_IMR	= iounmap(0xFFFC8010);
	UART2_CSR	= iounmap(0xFFFC8014);
	UART2_RHR	= iounmap(0xFFFC8018);
	UART2_THR	= iounmap(0xFFFC801C);
	UART2_BRGR	= iounmap(0xFFFC8020);
	UART2_RTOR	= iounmap(0xFFFC8024);
	UART2_TTGR	= iounmap(0xFFFC8028);
	UART2_FIDI	= iounmap(0xFFFC8040);
	UART2_NER	= iounmap(0xFFFC8044);
	UART2_IF	= iounmap(0xFFFC804C);
	AIC_SMR8	= iounmap(0xFFFFF020);
	
	UART0_CR	= iounmap(0xFFFC0000);
	UART0_MR	= iounmap(0xFFFC0004);
	UART0_IER	= iounmap(0xFFFC0008);
	UART0_IDR	= iounmap(0xFFFC000C);
	UART0_IMR	= iounmap(0xFFFC0010);
	UART0_CSR	= iounmap(0xFFFC0014);
	UART0_RHR	= iounmap(0xFFFC0018);
	UART0_THR	= iounmap(0xFFFC001C);
	UART0_BRGR	= iounmap(0xFFFC0020);
	UART0_RTOR	= iounmap(0xFFFC0024);
	UART0_TTGR	= iounmap(0xFFFC0028);
	UART0_FIDI	= iounmap(0xFFFC0040);
	UART0_NER	= iounmap(0xFFFC0044);
	UART0_IF	= iounmap(0xFFFC004C);
	AIC_SMR6	= iounmap(0xFFFFF018);
#endif
	unregister_chrdev(UART_MAJOR, "uart");				//注销设备
}

module_init(uart_init);
module_exit(uart_exit);

MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("USART 485 driver for NKTY AT91RM9200")

