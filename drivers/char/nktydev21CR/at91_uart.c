/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_uart.c
 * 摘    要：增加非中断方式叫号, 增加黑名单处理接口
 *			 在发送接收时加入lock_irq_save, 禁用中断
 *			 并且CR寄存器写改变
 *			 增加对term_ram结构体的新成员操作
 *			 将RTC操作转到另一个文件中
 * 			 
 * 当前版本：2.3.1
 * 作    者：wjzhe
 * 完成日期：2008年4月29日
 *
 * 取代版本：2.3
 * 原作者  ：wjzhe
 * 完成日期：2007年9月3日
 *
 * 取代版本：2.2
 * 原作者  ：wjzhe
 * 完成日期：2007年8月13日
 *
 * 取代版本：2.1 
 * 原作者  ：wjzhe
 * 完成日期：2007年6月6日
 *
 * 取代版本：2.0 
 * 原作者  ：wjzhe
 * 完成日期：2007年4月3日
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


/* 每次叫的轮循的圈数 */
static int lapnum;

// 定义指针
/********串口相关定义***********/
/*static */AT91PS_USART uart_ctl0 = (AT91PS_USART) AT91C_VA_BASE_US0;
/*static */AT91PS_USART uart_ctl2 = (AT91PS_USART) AT91C_VA_BASE_US2;

/* 记录终端总数和账户总数结构体 */
struct record_info rcd_info;

/* 驱动程序中流水缓冲区剩余空间 */
int space_remain = 0;

// 定义数据或数据指针
unsigned int fee[16];
acc_ram paccsw[MAXSWACC];
flow pflow[FLOWANUM];
int maxflowno = 0;
unsigned char pfbdseg[28] = {0};//add by duyy,2013.3.26
unsigned char ptmnum[16][16];//add by duyy,2013.3.26,timenum[身份][终端]
term_tmsg term_time[7];//write by duyy, 2013.3.26
char feetable[16][64];//added by duyy, 2013.10.28,feetable[终端类型][消费卡类型]
char feenum = 0;//added by duyy, 2013.11.19,管理费收取方式：0表示内扣，1表示外扣
char forbidflag[16][64];//added by duyy, 2013.10.28,forbidflag[终端类型][消费卡类型]
/*三个表单各自的版本号变量，write by duyy, 2013.11.18*/
long managefee_flag = 0;
long forbid_flag = 0;
long timeseg_flag = 0;
int update_fee = 0;//更新管理费提示，write by duyy, 2013.11.20
int update_frdflag = 0;//更新禁止消费启用标志提示，write by duyy, 2013.11.20
int update_frdseg = 0;//更新禁止时段提示，write by duyy, 2013.11.20
unsigned char uptermdata[256] = {0};//记录使用的终端机号，用于数据更新，write by duyy, 2013.11.20

term_ram * ptermram = NULL;
terminal *pterminal = NULL;
acc_ram *paccmain = NULL;
//flow *pflow = NULL;
#ifdef CONFIG_RECORD_CASHTERM
// 定义记录出纳机状态的变量
const int cashterm_n = CASHBUFSZ;
int cashterm_ptr = 0;				// 存储指针
struct cash_status cashbuf[CASHBUFSZ];	// 保存状态空间
#endif

#if 0
// 定义PSAM卡信息
struct psam_info psaminfo;
#endif

// 定义流水记录指针
struct flow_info flowptr;

// 记录每次叫号后的产生的流水总数
int flow_sum;

static int total = 0;// 现有流水总数

// 定义网络状态指针
static int net_status = 0;
// 定义光电卡允许脱机使用标志
static int le_allow = 0;

// 定义黑名单数据指针
black_acc *pblack = NULL;// 支持100,000笔, 大小为879KB
struct black_info blkinfo;
// 定义bad flow
//struct bad_flow badflow = {0};


#define LOCKUART 0

// 定义自旋锁
#if 1//ndef U485INT
  #if LOCKUART
	static DEFINE_SPINLOCK(uart_lock);
  #else
	static DECLARE_MUTEX(uart_lock);
	//static struct semaphore uart_lock;
  #endif
#endif
//设备号:	6	UART0
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
	// 配置USART寄存器
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


/*
 * 检查驱动中剩余流水空间, 返回<0流水区满, 0正常
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
	if ((tail + ONCERECVNO) <= FLOWANUM) {// 改ONCERECVNO为5, 不至于系统崩溃
		ret = FLOWANUM - tail;
	} else {
		ret = -1;
	}
	return ret;
#endif
}

#ifdef U485INT
#if 0
//UART中断响应函数
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
	if (space_remain == 0) {//flash空间剩余
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
	// 每一次叫号前将flow_sum, 清0, 以记录产生流水数量
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
		// 叫一圈
		for (i = 0; i < rcd_info.term_all; i++, prterm++) {
			if (chk_space() < 0) {
				printk("tail is %d, head is %d\n", flowptr.tail, flowptr.head);
				goto out;
			}
			//AT91_SYS->PIOC_CODR&=(!0x00001000);
			//AT91_SYS->PIOC_SODR|=0x00001000;
			sel_ch(prterm->term_no);// 任何情况下, 先将138数据位置位
			sendno = prterm->term_no & 0x7F;
			prterm->add_verify = 0;
			prterm->dif_verify = 0;
			prterm->status = STATUSUNSET;
			// 主动更新黑名单时将local_num | 0x80 ---> 注意进行跳转
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
				// 回号正确, 但没有命令
				// 在此要进行主动交互, 实时下发黑名单
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
			// 根据命令字做处理
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
				// 此次上电参数发送结束, 此时要主动下发键值
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
// UART 485 叫号函数
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
	if (space_remain <= 0) {//flash空间剩余, space_remain == 0
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
	// 每一次叫号前将flow_sum, 清0, 以记录产生流水数量
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
		// 叫一圈
		for (i = 0; i < rcd_info.term_all; i++, prterm++, udelay(ONEBYTETM)) {
			if (chk_space() < 0) {
				printk("tail is %d, head is %d\n", flowptr.tail, flowptr.head);
				goto out;
			}
			//AT91_SYS->PIOC_CODR&=(!0x00001000);
			//AT91_SYS->PIOC_SODR|=0x00001000;
			sel_ch(prterm->term_no);// 任何情况下, 先将138数据位置位
			sendno = prterm->term_no & 0x7F;
			prterm->add_verify = 0;
			prterm->dif_verify = 0;
			prterm->status = STATUSUNSET;
			// 主动更新黑名单时将local_num | 0x80 ---> 注意进行跳转
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
					continue;	// 此时段内不更新任何数据
				}
#endif
				// 回号正确, 但没有命令
				// 在此要进行主动交互, 实时下发黑名单
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
				//进行管理费、禁止消费标志、禁止消费时段信息更新，write by duyy, 2013.11.20
				if(uptermdata[prterm->term_no] & 0x01){
					//printk("uptermdata[%d] is %x\n", prterm->term_no, uptermdata[prterm->term_no]);
					if (purse_update_magfeetable(prterm) < 0){
						printk("termno %d send magfee failed\n", prterm->term_no);
						continue;
					}
					uptermdata[prterm->term_no] &= 0xFE;//管理费更新后清除标志位
				}
				if(uptermdata[prterm->term_no] & 0x02){
					//printk("uptermdata[%d] is %x\n", prterm->term_no, uptermdata[prterm->term_no]);
					if (purse_update_forbidflag(prterm) < 0){
						printk("termno %d send forbid flag failed\n", prterm->term_no);
						continue;
					}
					uptermdata[prterm->term_no] &= 0xFD;//禁止消费标志更新后清除标志位
				}
				if(uptermdata[prterm->term_no] & 0x04){
					printk("uptermdata[%d] is %x\n", prterm->term_no, uptermdata[prterm->term_no]);
					if (purse_update_noconsumetime(prterm, pfbdseg) < 0){
						printk("termno %d send forbid seg failed\n", prterm->term_no);
						continue;
					}
					uptermdata[prterm->term_no] &= 0xFB;//禁止消费时段更新后清除标志位
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
			// 根据命令字做处理
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
			/* 支持打小票功能 */
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
			/* 清华需要加TS11 POS机 */
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
				// 此次上电参数发送结束, 此时要主动下发键值
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
		驱动相关设置
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
	// 配置USART寄存器
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
	// 下面进行数据初使化
	total = 0;
	maxflowno = 0;
	flow_sum = 0;
	flowptr.head = flowptr.tail = 0;
	memset(&rcd_info, 0, sizeof(rcd_info));
	lapnum = LAPNUM;
	// 置为接收状态, wjzhe, 2010-12-10
	//AT91_SYS->PIOC_CODR &= (!0x00001000);
	//AT91_SYS->PIOC_SODR |= 0x00001000;
	return 0;
}

/*
 * UART io 控制函数, 不同的cmd, 执行不同的命令
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
	case SETLAPNUM:		// 设置一次叫号圈数
		lapnum = (int)arg;
		break;
	case INITUART:		// 初使化所有参数
		if (paccmain) {
			if ((rcd_info.account_main * sizeof(acc_ram)) >= (SZ_128K))
				vfree(paccmain);
			else
				kfree(paccmain);
			paccmain = NULL;
		}
		if ((ret = copy_from_user(&ptr, (struct uart_ptr *)arg, sizeof(ptr))) < 0)
			break;
		// 初使化最大流水号
		maxflowno = ptr.max_flow_no;
		// 初使化账户库
		rcd_info.account_main = rcd_info.account_all = ptr.acc_main_all;
		rcd_info.account_sw = 0;
		// 有账户库大于128K, 才用vmalloc申请空间
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
		// 拷贝数据
		if ((ret = copy_from_user(paccmain, ptr.paccmain,
			rcd_info.account_main * sizeof(acc_ram))) < 0) {
			printk("UART: copy account main failed!\n");
			break;
		}
		// 初使化管理费
		for (i = 0; i < 16; i++) {
			fee[i] = ptr.fee[i];
		}
		// 初使化终端库
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
		// 填充terminal ram结构体
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
		// 断网光电卡处理标志
		le_allow = ptr.le_allow;
//		printk("le allow %d, %d\n", le_allow, ptr.le_allow);
		//net_status = ptr.pnet_status;
		if ((ret = get_user(net_status, (int *)ptr.pnet_status)) < 0)
			break;
		// 初使化黑名单
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
	case SETFORBIDTIME://初始化终端禁止消费时段,added by duyy, 2013.3.26
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
	case SETTIMENUM:		// 设置身份、终端类型匹配时段序号,added by duyy, 2013.3.26
		//bug,原来是一个if判断语句，小于０则break，大于０则继续往下进行，会致使脱机不能断网消费,modified by duyy, 2013.7.18
		ret = copy_from_user(ptmnum, (unsigned char*)arg, sizeof(ptmnum));
		break;
	case SETFEETABLE:		// 设置消费卡类型~终端类型~管理费系数表单,added by duyy, 2013.10.28
		ret = copy_from_user(feedata, (unsigned char*)arg, sizeof(feedata));
		memset(feetable, 0, sizeof(feetable));
		memcpy(feetable, feedata, sizeof(feetable));
		feenum = feedata[1024];
		break;
	case SETFORBIDFLAG:		// 设置消费卡类型~终端类型~禁止消费时段启用标志表单,added by duyy, 2013.10.28
		memset(forbidflag, 0, sizeof(forbidflag));
		ret = copy_from_user(forbidflag, (unsigned char*)arg, sizeof(forbidflag));
		break;
	case GETTABLEFALG:	//读取三个表单版本号，write by duyy, 2013.11.19
		tabledata[0] = managefee_flag;
		tabledata[1] = forbid_flag;
		tabledata[2] = timeseg_flag;
		ret = copy_to_user((long *)arg, tabledata, sizeof(tabledata));
		break;
	case UPDATEFEE:		// 管理费数据有更新，write by duyy, 2013.11.20
		update_fee = (int)arg;
		for (i = 0; i < 256; i++){
			uptermdata[i] |= (1 << 0);//bit0置位，表示管理费要更新
		}
		break;
	case UPDATEFLAG:		//禁止消费启用标志数据有更新，write by duyy, 2013.11.20
		update_frdflag = (int)arg;
		for (i = 0; i < 256; i++){
			uptermdata[i] |= (1 << 1);//bit0置位，表示数据要更新
		}

		break;
	case UPDATESEG:		// 禁止消费时段数据有更新，write by duyy, 2013.11.20
		update_frdseg = (int)arg;
		for (i = 0; i < 256; i++){
			uptermdata[i] |= (1 << 2);//bit0置位，表示数据要更新
		}
		break;
	case SETLEALLOW:		// 设置光电卡能否断网时消费
		ret = copy_from_user(&le_allow, (void *)arg, sizeof(le_allow));
		break;
	case SETRTCTIME:		// 设置RTC
		//copy_from_user(&tm, , sizeof(tm));
		ret = set_RTC_time((struct rtc_time *)arg, 1);
		break;
	case GETRTCTIME:		// 读取RTC
		ret = read_RTC_time((struct rtc_time *)arg, 1);
		//copy_to_user(, &tm2, sizeof(tm2));
		break;
	case START_485:			// 485叫号, 用SEM进行数据保护
#ifndef U485INT
#ifdef DISCALL_INT
		local_irq_save(wtdreg);		// 禁用中断
#endif
#warning "485 not use int mode!"

		ret = uart_call();

#ifdef DISCALL_INT
		local_irq_restore(wtdreg);		// 禁用中断, 保证发送数据原子的执行
#endif
#endif
#ifdef U485INT
#warning "485 use int mode!"
		enable_usart0_int();			// 开启发送中断---->uart0
#endif
		break;
	case SETREMAIN:			// 设置系统剩余存储空间
		ret = copy_from_user(&space_remain, (int *)arg, sizeof(space_remain));
		if (ret < 0)
			break;
		//space_remain = *(volatile int *)arg;	// 笔
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
	case GETFLOWSUM:		// 获取一次叫号后产生的流水总数
		ret = put_user(flow_sum, (int *)arg);
		break;
	case GETFLOWNO:			// 获取本地最大流水号
		ret = put_user(maxflowno, (int *)arg);
		break;
	case ADDACCSW:			
		// 处理异区非现金流水
		if ((ret = copy_from_user(&no_m_flow, (no_money_flow *)arg,
			sizeof(no_money_flow))) < 0) {
			printk("UART: copy from user no money flow failed\n");
			break;
		}
		// 进行数据处理
		if ((ret = deal_no_money(&no_m_flow, paccmain, paccsw, &rcd_info)) < 0) {
			printk("UART: deal no money flow error\n");
		}
		//ret = 0;
		break;
	case CHGACC:
		// 先拷贝出流水数量
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
		// 再得到流水指针
		ret = copy_from_user(&pmflow, (money_flow *)arg, sizeof(pmflow));
		if (ret < 0) {
			printk("UART: copy from user money flow ptr failed\n");
			break;
		}
		// 顺次处理
		while (i) {
			// 处理异区现金流水
			if ((ret = copy_from_user(&m_flow, (money_flow *)pmflow,
				sizeof(money_flow))) < 0) {
				printk("UART: copy from user money flow failed\n");
				break;
			}
			// 进行数据处理
			if ((ret = deal_money(&m_flow, paccmain, paccsw, &rcd_info)) < 0) {
				printk("UART: deal money flow error\n");
			}
			pmflow += 1;
			i--;
		}
		break;
	case GETFLOWPTR:// 慎用, 得到内部流水指针
		printk("not support get flow ptr\n");
		ret = -1;
		break;
	case SETFLOWPTR:// 慎用, 设置内部流水指针
		printk("not support set flow ptr\n");
		ret = -1;
		break;
	case GETFLOW:// 取得叫号后的流水, 一共cnt笔
		if ((ret = copy_from_user(&pget, (struct get_flow *)arg,
			sizeof(pget))) < 0) {
			break;
		}
		flow_head = flowptr.head;
		flow_tail = flowptr.tail;
		// 不存在循环存储, 现有流水笔数为flow_tail
		// 可以获得如下笔数据
		pget.cnt = MIN(pget.cnt, flow_tail);
//		printk("user get flow %d, no. from %d to %d\n", pget.cnt,
//			pflow[0].flow_num, pflow[flowptr.tail - 1].flow_num);
		ret = copy_to_user(pget.pflow, pflow, sizeof(flow) * pget.cnt);
		if (ret < 0)
			break;
		ret = copy_to_user((void *)(arg + sizeof(flow *)), &pget.cnt, sizeof(pget.cnt));
		break;
	case GETACCINFO:// 获得账户库信息
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
	case GETACCMAIN:// 得到主账户库
		ret = 0;
		if (rcd_info.account_main == 0)
			break;
		if ((ret = copy_to_user((acc_ram *)arg, paccmain,
			sizeof(acc_ram) * rcd_info.account_main)) < 0)
			break;
		// 读取帐户信息后将帐户空间清除
		if ((rcd_info.account_main * sizeof(acc_ram)) > (SZ_128K))
			vfree(paccmain);
		else
			kfree(paccmain);
		paccmain = NULL;
		break;
	case GETACCSW:// 得到辅账户库
		ret = 0;
		if (rcd_info.account_sw == 0)
			break;
		ret = copy_to_user((acc_ram *)arg, paccsw,
			sizeof(acc_ram) * rcd_info.account_sw);
		break;
	case SETNETSTATUS:// 设置网络状态
		ret = get_user(net_status, (int *)arg);
		break;
	case BCT_BLACKLIST:// 广播黑名单
		// 黑名单时间很长, 需要关闭看门狗
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
			// 将必要信息拷贝到内核中
			ret = copy_from_user(&bctdata, (void *)arg, sizeof(bctdata));
			if (ret < 0)
				break;
			// 在正常进行数据处理前判断黑名单的数量是否为0
			if (bctdata.info.count == 0) {
				break;
			}
			// 申请黑名单列表空间
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
			// 拷贝黑名单
			ret = copy_from_user(pbacc, bctdata.pbacc, bctdata.info.count * sizeof(black_acc));
			if (ret < 0)
				break;
			// 构造term_ram
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
			// 广播黑名单
			ret = purse_broadcast_blk(pbacc, &bctdata.info, ptrm, bctdata.term_cnt);
			if (ret < 0)
				printk("broadcast blk failed\n");
			// 释放内存
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
 * 写入FRAM缓存流水, 只用于系统初使化时
 */
static int uart_write(struct file *file, const char *buff, size_t count, loff_t *offp)
{
	return -1;
#if 0
	// 写入FRAM遗留流水
	// 只允许在流水区初使化的时候用此函数
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
 * 程序运行过程中读取终端信息
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
	AT91_SYS->AIC_SMR[AT91C_ID_US0] |= (PRIORITY & 0x7);/* irq 优先级 */
	AT91_SYS->AIC_SMR[AT91C_ID_US2] |= (PRIORITY & 0x7);/* irq 优先级 */
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
	unregister_chrdev(UART_MAJOR, "uart");				//注销设备
}

module_init(uart_init);
module_exit(uart_exit);

MODULE_LICENSE("Proprietary")
MODULE_AUTHOR("wjzhe")
MODULE_DESCRIPTION("USART 485 driver for NKTY AT91RM9200")

