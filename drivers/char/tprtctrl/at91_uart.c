/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�at91_uart.c
 * ժ    Ҫ�����ӷ��жϷ�ʽ�к�, ���Ӻ���������ӿ�
 *			 �ڷ��ͽ���ʱ����lock_irq_save, �����ж�
 *			 ����CR�Ĵ���д�ı�
 *			 ���Ӷ�term_ram�ṹ����³�Ա����
 *			 ��RTC����ת����һ���ļ���
 * 			 �����¿�ͨѶ, ȥ�����ͨѶ
 * ��ǰ�汾��2.4
 * ��    �ߣ�wjzhe
 * ������ڣ�2008��8��19��
 *
 * ȡ���汾��2.3.1
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
#include "uart_dev.h"
#include "uart.h"
#include "tprt_arch.h"
#include "tprt_com.h"
#include "power_cal.h"

#define BAUDRATE 28800

#define LAPNUM 4
#define ONCERECVNO 5 

#ifndef MIN
#define	MIN(a,b)	(((a) <= (b)) ? (a) : (b))
#endif

/* ÿ�νе���ѭ��Ȧ�� */
static int lapnum;

// ����ָ��
/********������ض���***********/
/*static */AT91PS_USART uart_ctl0 = (AT91PS_USART) AT91C_VA_BASE_US0;
/*static */AT91PS_USART uart_ctl2 = (AT91PS_USART) AT91C_VA_BASE_US2;


tprt_data tprtdata;
static term_pkg termpkg;		// ���������յ�������
//tprt_ram * ptermram = NULL;
//tprt_term *pterminal = NULL;
#ifdef CONFIG_TPRT_NC
power_ctrl_knl pwctrl[PCTRLN];
#endif
// ��¼ÿ�νкź�Ĳ�������ˮ����
int flow_sum;

static int total = 0;// ������ˮ����

static DECLARE_MUTEX(uart_lock);
//�豸��:	6	UART0
//			8	UART2

#define UART_MAJOR 22
#ifdef U485INT
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
void usart_delay(int n)
{
	int i;
	for (i = 0; i < n; i++) {
		udelay(ONEBYTETM);
	}
}


/*
 * ���������ʣ����ˮ�ռ�, ����<0��ˮ����, 0����
 */
static inline
int chk_space(void)
{
	return 0;
}
//#define CALLDEBUG
// UART 485 �кź���
static int uart_call(void)
{
//#define CALLDEBUG
	int n;
	int i, ret;
	unsigned char sendno;
	unsigned char rdata = 0, commd;
	tprt_ram *prterm;
	term_pkg *pkg;
	//...-----------------------------------
	if (tprtdata.n == 0) {
		//printk("no term!!!\n");
		goto out;
	}
	if (lapnum <= 0 || lapnum > 10) {
		n = LAPNUM;
	} else {
		n = lapnum;
	}
	// ÿһ�νк�ǰ��flow_sum, ��0, �Լ�¼������ˮ����
	flow_sum = 0;
	//tm[0] = 0x20;		// ��ʱ���ǲ���Ҫʱ���
	//read_time(&tm[1]);
	while (n) {
		prterm = tprtdata.pterm;
		// ��һȦ
		for (i = 0; i < tprtdata.n; i++, prterm++) {
			//AT91_SYS->PIOC_CODR&=(!0x00001000);
			//AT91_SYS->PIOC_SODR|=0x00001000;
			sel_ch(prterm->termno);// �κ������, �Ƚ�138����λ��λ
			// fixed by wjzhe
			sendno = prterm->termno;// & 0x7F; fixed by wjzhe
			pkg = prterm->pkg;
			// clear check
			memset(pkg->chk, 0, sizeof(pkg->chk));
			prterm->status = STATUSUNSET;
			//printk("send addr %d, %d\n", sendno, prterm->termno);
			ret = send_addr(sendno, prterm->termno);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("send addr %d error: %d\n", sendno, ret);
#endif
				prterm->status = NOTERMINAL;
				continue;
			}
			// receive data, if pos number and no cmd then end
			// if pos number and cmd then exec cmd
			// else wrong!
			ret = recv_data((char *)&rdata, prterm->termno);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("recv term %d error: %d\n", prterm->termno, ret);
#endif
				prterm->status = ret;
				continue;
			}
			pkg->chk[0] += rdata;
			pkg->chk[1] ^= rdata;
			//printk("recvno: %d and sendno: %d\n", rdata, sendno);
			if ((rdata & 0x7F) != (sendno & 0x7F)) {
#ifdef CALLDEBUG
				printk("recvno: %d but sendno: %d\n", rdata, sendno);
#endif
				prterm->status = NOCOM;
				continue;
			}
			if ((rdata ^ (0x80 ^ sendno))) {// no command fixed by wjzhe
				// �غ���ȷ, ��û������
				// �ڴ�Ҫ������������, ʵʱ�·�������
				prterm->status = TNORMAL;
				continue;
			}
			// has command, then receive cmd
			ret = recv_data((char *)&commd, prterm->termno);
			if (ret < 0) {
#ifdef CALLDEBUG
				printk("recv cmd %d error: %d\n", prterm->pterm->num, ret);
#endif
				prterm->status = ret;
				continue;
			}
			//printk("%d has cmd, and cmd is %02x\n", prterm->term_no, commd);
			pkg->chk[0] += commd;
			pkg->chk[1] ^= commd;
			// update pkg buffer
			pkg->num = prterm->termno;
			pkg->cmd = commd;
			// ����������������
			switch (commd) {
			case TPRT_REQ_CONF:
				if (tprt_recv_packet_aftercmd(pkg) < 0) {
					pr_debug("%d tprt_recv_packet_aftercmd error\n", prterm->termno);
				}
				mdelay(1);
				if (tprt_rsend_conf(prterm) < 0) {
					pr_debug("%d tprt_rsend_conf error\n", prterm->termno);
				}
				break;
			case TPRT_RECV_FLOW:
				if (tprt_recv_packet_aftercmd(pkg) < 0) {
					pr_debug("TPRT_RECV_FLOW %d tprt_recv_packet_aftercmd error\n", prterm->termno);
				}
				mdelay(1);
				if (tprt_recv_flow(prterm) < 0) {
					pr_debug("%d tprt_recv_flow error\n", prterm->termno);
				}
				break;
#ifdef CONFIG_TPRT_NC
			case TPRT_SEND_NUM:
				if (tprt_recv_packet_aftercmd(pkg) < 0) {
					pr_debug("%d tprt_recv_packet_aftercmd error\n", prterm->termno);
				}
				mdelay(1);
				if (tprt_rsend_power_conf(prterm) < 0) {
					pr_debug("%d tprt_rsend_power_conf error\n", prterm->termno);
				}
				break;
#endif
			default:			// cmd is not recognised, so status is !
				prterm->status = -2;
#ifdef CALLDEBUG
				printk("cmd %02x is not recognised\n", commd);
#endif
				usart_delay(1);
				ret = -1;
				break;
			}
			if (prterm->status == STATUSUNSET)
				prterm->status = TNORMAL;
			//prterm++;
			udelay(ONEBYTETM);
		}
		// ����Ҫ����
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
	flow_sum = 0;
	lapnum = LAPNUM;
	return 0;
}
static inline void destory_tpdata(tprt_data *p)
{
	// tprt_ram�е��ն�ָ��ֻ��һ������
	// �����ͷŵ�ʱ��, ֻҪ�ͷ�ͷ�ͺ���
	if (p == NULL) {
		return;
	}
	if (p->pterm) {
		kfree(p->pterm);
	}
	if (p->par) {
		kfree(p->par);
	}
	//kfree(p);
}
static inline void partoterm(tprt_term *ptpar, tprt_param *par)
{
	ptpar->num = par->num;
	ptpar->val = par->val;
	ptpar->dif = par->dif;
	ptpar->ctrl = par->ctrl;
	ptpar->cival = par->cival;
	ptpar->bak = par->bak;
	ptpar->sival = par->sival;
}

// ��������������INITUART
static int init_uart_data(tprt_data *tpdata, void *arg)
{
	int i, n, ret;
	struct uart_ptr ptr;
	tprt_ram *prterm;
	tprt_term *ptpar;
	tprt_param *par;
	// �õ�ptr...
	ret = copy_from_user(&ptr, arg, sizeof(ptr));
	if (ret < 0) {
		printk("init_uart_data copy ptr error\n");
		return ret;
	}
	n = ptr.cnt;
	par = kmalloc(sizeof(*(par)) * n, GFP_KERNEL);
	if (par == NULL) {
		printk("init_uart_data no memory\n");
		return -ENOMEM;
	}
	// ���ǰ�����ȫ�����ƹ����ٽ��д���
	ret = copy_from_user(par, ptr.par, sizeof(*(par)) * n);
	if (ret < 0) {
		printk("init_uart_data copy par error\n");
		goto out;
	}
	pr_debug("init_uart_data cnt %d\n", n);
#ifdef CONFIG_TPRT_NC
	memset(pwctrl, 0, sizeof(pwctrl));
#endif
	// ��ʼ����������
	destory_tpdata(tpdata);
	// ����ռ�
	prterm = kmalloc(sizeof(*(prterm)) * n, GFP_KERNEL);
	if (prterm == NULL) {
		printk("init_uart_data alloc prterm error\n");
		goto out;
	}
	memset(prterm, 0, sizeof(*(prterm)) * n);
	ptpar = kmalloc(sizeof(*(ptpar)) * n, GFP_KERNEL);
	if (ptpar == NULL) {
		printk("init_uart_data alloc prpar error\n");
		kfree(prterm);
		goto out;
	}
	memset(ptpar, 0, sizeof(*(ptpar)) * n);
	// ��д��ptpar
	for (i = 0; i < n; i++) {
		partoterm(&ptpar[i], &par[i]);
	}
	for (i = 0; i < n; i++) {
		// ��������
		prterm[i].pterm = &ptpar[i];
		prterm[i].pkg = &termpkg;
		prterm[i].termno = par[i].num;
		prterm[i].roomid = par[i].roomid;
		prterm[i].status = STATUSUNSET;
#ifdef CONFIG_TPRT_NC
		// �����Ƿ�Ϊ��Դ���ư�
		{
			int it = find_powernum2(prterm[i].termno);
			if (it == -1) {
				it = find_power_ctrl(prterm[i].termno);
				if (it == -1) {
					panic("term %d: not power ctrl, and not normal term\n", prterm[i].termno);
				}
				prterm[i].is_power = 0;
				prterm[i].power = &pwctrl[it];
			} else {
				prterm[i].is_power = 1;
				prterm[i].power = &pwctrl[it];
			}
		}
#endif
	}
	tpdata->pterm = prterm;
	tpdata->n = n;
	tpdata->par = ptpar;
out:
	kfree(par);
	return ret;
}

static inline tprt_ram *__find_term(unsigned char termno, tprt_ram *prtm, int n)
{
	int i;
	tprt_ram *p = NULL;
	for (i = 0; i < n; i++, prtm++) {
		if (termno == prtm->termno) {
			p = prtm;
			break;
		}
	}
	return p;
}

tprt_term *find_term(unsigned char termno, tprt_ram *prtm, int n)
{
	tprt_ram *p = NULL;
	// ����Ѱ���ն˲���ָ��
	p = __find_term(termno, prtm, n);
	if (p) {
		return p->pterm;
	}
	return NULL;
}


int change_term_param(tprt_data *pdata, tprt_term *par)
{
	tprt_term *p;
	//printk("change term param...%d\n", par->num);
	p = find_term(par->num, pdata->pterm, pdata->n);
	if (p == NULL) {
		printk("can not find term %d\n", par->num);
		return -1;
	}
	memcpy(p, par, sizeof(*p));
	//partoterm(p, par);
	return 0;
}

#if 0
int change_term_sparam(tprt_data *pdata, tprt_param *par)
{
}
#endif
int iorecv_term_flow(void *arg)
{
	unsigned char termno;
	tprt_flow flow;
	tprt_ram *p;
	int ret;
	ret = copy_from_user(&termno, arg, sizeof(termno));
	if (ret < 0) {
		pr_debug("iorecv_term_flow get term no failed\n");
		return -1;
	}
	p = __find_term(termno, tprtdata.pterm, tprtdata.n);
	if (p == NULL) {
		pr_debug("iorecv_term_flow can not find term %d\n", termno);
		return -1;
	}
#ifdef CONFIG_TPRT_NC
	if (p->is_power) {
		_fill_flow_time(flow.FlowTime);
		flow.RelayStatus = (p->power->s_ctrl << 16) | p->power->r_ctrl;
		flow.RoomID = p->roomid;
		flow.Temperature = 0;
		flow.TerminalID = p->termno;
		flow.TerminalStatus = p->status;
	} else {
#endif
	ret = tprt_req_data(p, &flow);
	if (ret < 0) {
		//pr_debug("iorecv_term_flow get flow error\n");
		return -1;
	}
#ifdef CONFIG_TPRT_NC
	}
#endif
	ret = copy_to_user(arg, &flow, sizeof(flow));
	return ret;
}

int iorecv_term_aflow(void *arg)
{
	tprt_ram *ptrm = tprtdata.pterm;
	int n = tprtdata.n;
	int ret, i, m;
	tprt_flow *pflow;
	// ������ռ�
	pflow = (tprt_flow *)kmalloc(sizeof(tprt_flow) * n, GFP_KERNEL);
	if (pflow == NULL) {
		return -ENOMEM;
	}
	memset(pflow, 0, sizeof(tprt_flow) * n);
	// ����ն˻�ȡ��ˮ, ֻ����ȷ����ˮ�Ż��ƶ�ָ��
	for (i = 0, m = 0; i < n; i++) {
		//pr_debug("term %d ispower %d\n", ptrm[i].termno, ptrm[i].is_power);
#ifdef CONFIG_TPRT_NC
		//int ti;
		if (ptrm[i].is_power/* (ti = find_powernum2(ptrm[i].termno)) >= 0 */) {
			pr_debug("iorecv_term_aflow: power term %d\n", ptrm[i].termno);
			// ��֯��ˮ��pflow[m]
			pflow[m].RoomID = ptrm[i].roomid;
			pflow[m].RelayStatus = (ptrm[i].power->s_ctrl << 16) | ptrm[i].power->r_ctrl;
			pflow[m].Temperature = 0;
			pflow[m].TerminalStatus = ptrm[i].status;
			pflow[m].TerminalID = ptrm[i].termno;
			_fill_flow_time(pflow[m].FlowTime);
			m++;
			continue;
		}
#endif
		ret = tprt_req_data(&ptrm[i], &pflow[m]);
		if (ret < 0) {
			//pr_debug("iorecv_term_aflow %d get flow error\n", ptrm[i].termno);
			continue;
		}
		m++;
	}
	// �����ݿ������û��ռ�, ֻ�ɹ���m���ն�
	ret = copy_to_user(arg, pflow, sizeof(tprt_flow) * m);
	if (ret < 0) {
		pr_debug("iorecv_term_aflow copy to user error\n");
	} else {
		ret = m;
	}
	// �������ͷŸղ�������ڴ�
	kfree(pflow);
	return ret;
}

// ���õ����ն��ϵ����
int ioset_term_param(void *arg)
{
	unsigned char termno;
	tprt_ram *p;
	int ret;
	ret = copy_from_user(&termno, arg, sizeof(termno));
	if (ret < 0) {
		pr_debug("ioset_term_param copy %p error\n", arg);
	}
	p = __find_term(termno, tprtdata.pterm, tprtdata.n);
	if (p == NULL) {
		pr_debug("ioset_term_param can not find %d\n", termno);
		return -ENODEV;
	}
#ifdef CONFIG_TPRT_NC
	if (p->is_power) {
		pr_debug("ioset_term_param: can not access power ctrl\n");
		return -EINVAL;
	}
#endif
	ret = tprt_update_conf(p);
	if (ret < 0) {
		pr_debug("ioset_term_param update %d conf error\n", termno);
		return -EBUSY;
	}
	return 0;
}

// ���������ն��ϵ����
int ioset_term_aparam(void *arg)
{
	tprt_ram *p = tprtdata.pterm;
	int ret, i, n = tprtdata.n;
	//pr_debug("ioset_term_aparam begin...\n");
	for (i = 0; i < n; i++, p++) {
#ifdef CONFIG_TPRT_NC
		if (p->is_power)
			continue;
#endif
		ret = tprt_update_conf(p);
		if (ret < 0) {
			//pr_debug("ioset_term_aparam update %d conf error\n", p->termno);
		}
	}
	//pr_debug("ioset_term_aparam end\n");
	return 0;
}

// ���õ����ն��ϵ����, �������ⲿ���ݵõ�����ĸ�ʽΪtprt_term
int ioset_term_sparam(void *arg)
{
	// ����û������, ����Ҫ����ٵ�����
	tprt_ram tpram;
	tprt_term term;
	term_pkg pkg;
	int ret;
	ret = copy_from_user(&term, arg, sizeof(term));
	if (ret < 0) {
		pr_debug("ioset_term_sparam get data failed %p\n", arg);
		return ret;
	}
#ifdef CONFIG_TPRT_NC
	if (is_power_num(term.num)) {
		//pr_debug("ioset_term_sparam not allow power ctrl\n");
		return -EINVAL;
	}
#endif
	// ����tpram
	memset(&tpram, 0, sizeof(tpram));
	tpram.pkg = &pkg;
	tpram.pterm = &term;
	tpram.termno = term.num;
	// ��ʼ���²���
	ret = tprt_update_conf(&tpram);
	if (ret < 0) {
		//pr_debug("tprt_update_conf %d update conf error\n", term.num);
		ret = -ENODEV;
	}
	return ret;
}

#ifdef CONFIG_TPRT_NC
int change_power_conf(void *arg)
{
	int ret, i;
	power_ctrl pwc[PCTRLN];
	ret = copy_from_user(pwc, arg, sizeof(pwc));
	if (ret < 0) {
		return ret;
	}
	for (i = 0; i < PCTRLN; i++) {
		pwctrl[i].base = pwc[i].base;
		pwctrl[i].cival = pwc[i].cival;
		pwctrl[i].is_set = pwc[i].is_set;
		pwctrl[i].num = pwc[i].num;
		pwctrl[i].tflag = pwc[i].tflag;
	}
	return 0;
}
// ����Դ���Ʒ��Ϳ�������, �������ⲿ���ݵõ������ʽtprt_power_ctrl
int iosend_power_ctrl(void *arg)
{
	// ����û������, ����Ҫ����ٵ�����
	tprt_ram tpram;
	term_pkg pkg;
	tprt_power_ctrl ctrl;
	int ret;
	ret = copy_from_user(&ctrl, arg, sizeof(ctrl));
	if (ret < 0) {
		pr_debug("iosend_power_ctrl get data failed %p\n", arg);
		return ret;
	}
	// ����tpram
	memset(&tpram, 0, sizeof(tpram));
	tpram.pkg = &pkg;
	tpram.termno = ctrl.num;
	//pr_debug("SENDPWCTRL: begin %d %04x\n", ctrl.num, ctrl.ctrl);
	// ��ʼ���²���
	ret = tprt_send_power_ctrl(&tpram, &ctrl);
	if (ret < 0) {
		//pr_debug("iosend_power_ctrl %d update conf error\n", ctrl.num);
		ret = -ENODEV;
	}
	return ret;
}
// ��ȡ��Դ���������ն˵�״̬
int iorecv_term_status(void *arg)
{
	int ret, ipt, i, ti, m;
	tprt_ram *ptrm = tprtdata.pterm;
	int n = tprtdata.n;
	term_status t_status[16];
	time_t now = _get_current_time();
	// �������ݣ��õ�ipt
	ret = copy_from_user(t_status, arg, sizeof(term_status));
	if (ret < 0) {
		pr_debug("iorecv_term_status get data failed %p\n", arg);
		return ret;
	}
	ipt = t_status[0].num;
	memset(t_status, 0xFF, sizeof(t_status));
	// ����ն˻�ȡ��ˮ
	for (i = 0, m = 0; i < n; i++) {
		// �Ƚ����ն˺Ϸ����ж�
		if (!num_in_cont(ptrm[i].termno, &cpower_cont[ipt])) {
			continue;
		}
		// ��ȡ���ݲ��������
		ti = ptrm[i].termno & 0xF;
		ret = tprt_req_data(&ptrm[i], NULL);
		if (ret < 0) {
			if (((long)now - (long)ptrm[i].flow_tm) < 600) {
				//pr_debug("iorecv_term_status %d get flow error 1\n", ptrm[i].termno);
				t_status[m].flag = 1;
			} else {
				pr_debug("iorecv_term_status %d get flow error 0: %d, %d\n",
					ptrm[i].termno, (int)now, (int)ptrm[i].flow_tm);
				t_status[m].flag = 0;
			}
		} else {
			//pr_debug("iorecv_term_status %d get flow OK\n", ptrm[i].termno);
			t_status[m].flag = 1;
		}
		// ���������Ϣ
		t_status[m].num = ptrm[i].termno;
		t_status[m].gval = ptrm[i].pterm->val;
		t_status[m].difval = ptrm[i].pterm->dif;
		t_status[m].isctrl = ptrm[i].pterm->ctrl;
		t_status[m].val = ptrm[i].val;
#ifdef DEBUG
		if (ptrm[i].power == NULL) {
			panic("iorecv_term_status: power NULL\n");
		}
#endif
		// �˴�Ҳ����ֱ����pwctrl[ipt].r_ctrl
		t_status[m].ctrl = ptrm[i].power->r_ctrl & (1 << (ti/* ptrm[i].termno & 0xF */));
		m++;
	}
	// �����ݿ������û��ռ�, ֻ�ɹ���m���ն�
	ret = copy_to_user(arg, t_status, sizeof(t_status));
	if (ret < 0) {
		pr_debug("iorecv_term_aflow copy to user error\n");
	} else {
		ret = m;
	}
	return ret;
}
#endif

/*
 * UART io ���ƺ���, ��ͬ��cmd, ִ�в�ͬ������
 */
static int uart_ioctl(struct inode* inode, struct file* file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	// �ź���
	down(&uart_lock);
	switch(cmd)
	{
	case SETLAPNUM:		// ����һ�νк�Ȧ��
		lapnum = (int)arg;
		break;
	case SETRTCTIME:		// ����RTC
		ret = set_RTC_time((struct rtc_time *)arg, 1);
		break;
	case GETRTCTIME:		// ��ȡRTC
		ret = read_RTC_time((struct rtc_time *)arg, 1);
		break;
	case START_485:			// 485�к�, ��SEM�������ݱ���
		ret = uart_call();
		break;
	case START_485_NEW:
		printk("unknown new 485\n");
		break;
		ret = tprt_call(&tprtdata);		// Ӧ�ó�����û�е���
		break;
	case INITUART:
		// ����Ҫ��ʼ��һ����������
		ret = init_uart_data(&tprtdata, (void *)arg);
		break;
	case CHANGETERMPAR:
		// �ı䵥���ն˲���, ʱ�θ���֮�����
		{
			tprt_term tmp;
			ret = copy_from_user(&tmp, (tprt_term *)arg, sizeof(tmp));
			if (ret < 0) {
				break;
			}
#ifdef CONFIG_TPRT_NC
			if (is_power_num(tmp.num)) {
				pr_debug("error CHANGETERMPAR: try change term %d\n", tmp.num);
				ret = -EINVAL;
				break;
			}
#endif
			ret = change_term_param(&tprtdata, (tprt_term *)&tmp);
		}
		break;
#if 0
	case CHANGETERMSPAR:
		ret = change_term_sparam(&tprtdata, (tprt_param *)arg);
		break;
#endif
	//case CHANGETERMLIB:
		// ��INIT��������?
		// �ı������ն˿�, �������ɾ���ն�
	//	break;
	case GETALLFLOW:
		// ��ȡȫ���ն���ˮ, ������ص��������ն˺�Ϊ0, ��˵��ͨѶ������
		ret = iorecv_term_aflow((void *)arg);
		break;
	case GETSIGLEFLOW:
		// ��ȡ�����ն���ˮ
		ret = iorecv_term_flow((void *)arg);
		break;
	case SETATERMCONF:
		// ����ȫ���ն˲���, �������������ն˿��л�ȡ
		// ��������ܱ�֤�����ն�������ȫͬ��, ����ȷ������
		ret = ioset_term_aparam((void *)arg);
		break;
	case SETTERMCONF:		// ������û�е���
		// ���õ����ն˲���, �������������ն˿��л�ȡ
		ret = ioset_term_param((void *)arg);
		break;
	case SETSTERMCONF:
		// ���õ����ն˲���, �������û��ռ䴫��
		ret = ioset_term_sparam((void *)arg);
		break;
#ifdef CONFIG_TPRT_NC
	case SENDPWCTRL:		// ���Դ���ư巢�Ϳ�����Ϣ
		ret = iosend_power_ctrl((void *)arg);
		break;
	case GETTERMSTATUS:		// �õ������ն˵�ǰ״̬�����ڷ��Ϳ�����Ϣ
		ret = iorecv_term_status((void *)arg);
		break;
	case SETPOWERCTRL:
		ret = change_power_conf((void*)arg);
		break;
#endif
#if 0
	// �кź󲻻������ˮ
	case GETFLOWSUM:		// ��ȡһ�νкź��������ˮ����
		ret = put_user(flow_sum, (int *)arg);
		break;
	case GETFLOWNO:			// ��ȡ���������ˮ��
		ret = put_user(maxflowno, (int *)arg);
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
	case GETFLOWTAIL:
		ret = copy_to_user((int *)arg, &flowptr.tail, sizeof(flowptr.tail));
		break;
	case CLEARFLOWBUF:
		flowptr.tail = 0;
		break;
	case GETFLOWTOTAL:
		ret = copy_to_user((int *)arg, &total, sizeof(total));
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
	net_rterm *pterm_info;
	int i;
	tprt_ram *ptrm = tprtdata.pterm;
	unsigned int cnt;
	if (count % sizeof(net_rterm) != 0 || buff == NULL)
		return -EINVAL;
	if (count == 0) {
		return 0;
	}
	pterm_info = (net_rterm *)kmalloc(count, GFP_KERNEL);
	if (pterm_info == NULL) {
		printk("UART: cannot init pterm_info\n");
		return -ENOMEM;
	}
	cnt = count / sizeof(net_rterm);
	down(&uart_lock);
	for (i = 0; i < cnt; i++, ptrm++) {
		pterm_info[i].TermID = ptrm->termno;
		pterm_info[i].Temperature = ptrm->val;
#ifdef CONFIG_TPRT_NC
		if (ptrm->is_power) {
			pterm_info[i].RelayStatus = (ptrm->power->s_ctrl << 16) | ptrm->power->r_ctrl;
		} else {
#endif
		pterm_info[i].RelayStatus = ptrm->ctrl;
#ifdef CONFIG_TPRT_NC
		}
#endif
/*
		0x00��ʾ������0xFF��ʾͨ�Ź��ϣ�0x0F��ʾͨ��Ч������
#define NOTERMINAL -1
#define NOCOM -2
#define STATUSUNSET -3
#define ERRVERIFY	-4
#define ERRTERM		-5
#define ERRSEND		-6
#define TNORMAL 0
*/
		switch (ptrm->status) {
			case NOTERMINAL:
				pterm_info[i].TerminalStatus = 0xFF;
				break;
			case NOCOM:
			case ERRTERM:
			case ERRSEND:
			case ERRVERIFY:
				pterm_info[i].TerminalStatus = 0xF;
				break;
			case TNORMAL:
				pterm_info[i].TerminalStatus = 0;
				break;
			default:
				pterm_info[i].TerminalStatus = 0xFF;
				break;
		}
	}
	up(&uart_lock);
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

