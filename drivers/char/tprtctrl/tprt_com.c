/*
 * Copyright (c) 2008, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�tprt_com.c
 * ժ    Ҫ��ʵ�ֺ��¿��ն�485ͨѶ�ײ㺯��
 *
 * ��ǰ�汾��1.0.0
 * ��    �ߣ�wjzhe
 * ������ڣ�2008��8��19��
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
#include "uart.h"
#include "tprt_arch.h"
#include "tprt_com.h"
#include "uart_dev.h"
#include "at91_rtc.h"
#include "power_cal.h"
/*
 * �ɿ�ִ����������1��15��
 * ��Դ�������ն˱�ű���Ϊ15��1��14����31��16��30����47��32��46����
 * 63��48��62����79��64��78����95��80��95����111��96��110����
 * 127��112��126����143��128��142����159��144��158����175��160��174����
 * 191��176��190����207��192��206����223��208��222����239��224��238����
 * 255��240��254�����¿��ն˲���ռ��
 */
// �������ݵ�û��У��
static int uart_send_data(AT91PS_USART ctl, u8 *data, int cnt, u8 *chk)
{
	int i;
	for (i = 0; i < cnt; i++) {
		if (chk) {
			chk[0] += *data;
			chk[1] ^= *data;
		}
		ctl->US_THR = *data++;
		if (!uart_poll(&ctl->US_CSR, AT91C_US_TXEMPTY)) {
			return -1;
		}
	}
	return 0;
}
// �������ݲ���У��
static int uart_send_data_crc(AT91PS_USART ctl, u8 *data, int cnt, u8 *chk)
{
	int i;
	for (i = 0; i < cnt; i++) {
		chk[0] += *data;
		chk[1] ^= *data;
		ctl->US_THR = *data++;
		if (!uart_poll(&ctl->US_CSR, AT91C_US_TXEMPTY)) {
			return -1;
		}
	}
	// ��Ҫ����chk[2]
	chk[1] ^= chk[0];		// ���ͻ�Ҫ���ۼӽ��
	for (i = 0; i < 2; i++) {
		ctl->US_THR = chk[i];
		if (!uart_poll(&ctl->US_CSR, AT91C_US_TXEMPTY)) {
			return -1;
		}
	}
	return 0;
}
// ����֤У��
static int uart_recv_data(AT91PS_USART ctl, u8 *data, int cnt, u8 *chk)
{
	int ret = 0, i;
	u8 tmp;
	for (i = 0; i < cnt; i++) {
		// ��ʱ�ȴ�
		if (!uart_poll(&ctl->US_CSR, AT91C_US_RXRDY)) {
			ret = NOTERMINAL;
			goto out;
		}
		// �жϱ�־λ
		if (((ctl->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((ctl->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			tmp = ctl->US_RHR;
 			ctl->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// ��������
		*(volatile char *)data = ctl->US_RHR;
		if (chk) {
			chk[0] += *data;
			chk[1] ^= *data;
		}
		data++;
	}
out:
	return ret;
}
// ��֤У��
static int uart_recv_data_crc(AT91PS_USART ctl, u8 *data, int cnt, u8 *chk)
{
	int ret, i;
	u8 k[2] = {0};
	u8 tmp;
	for (i = 0; i < cnt; i++) {
		// ��ʱ�ȴ�
		if (!uart_poll(&ctl->US_CSR, AT91C_US_RXRDY)) {
			ret = NOTERMINAL;
			goto out;
		}
		// �жϱ�־λ
		if(((ctl->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((ctl->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			tmp = ctl->US_RHR;
 			ctl->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// ��������
		*(volatile char *)data = ctl->US_RHR;
		chk[0] += *data;
		chk[1] ^= *data;
		data++;
	}
	for (i = 0; i < 2; i++) {
		// ��ʱ�ȴ�
		if (!uart_poll(&ctl->US_CSR, AT91C_US_RXRDY)) {
			ret = NOTERMINAL;
			goto out;
		}
		// �жϱ�־λ
		if(((ctl->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((ctl->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			tmp = ctl->US_RHR;
 			ctl->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// ��������
		k[i] = ctl->US_RHR;
	}
	// ����У��
	chk[1] ^= k[0];		// ���У��Ҫ���ۼӺ�
	if (k[0] == chk[0] && k[1] == chk[1]) {
		// У��ͨ��
		ret = cnt;
	} else {
		//printk("check error %02x%02x, but we %02x%02x\n",
		//	k[0], k[1], chk[0], chk[1]);
		ret = ERRVERIFY;
	}
out:
	return ret;
}

// ���������ݰ����Զ���У��
static int tprt_send_packet(term_pkg *pkg)
//(u8 termno, u8 cmd, u8 *data, u8 len)
{
	// �����������ݰ�
	int ret = 0;
	AT91PS_USART ctl;
	//AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12;		// enable send data
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(20);
	// ��������ʱ����Ҫ����ͨ��, ���ԭ��ͼ
	if (!(pkg->num & 0x80)) {
		// ǰ��ͨ����UART0
		ctl = uart_ctl0;
	} else {
		// ����ͨ����UART2
		ctl = uart_ctl2;
	}
	memset(pkg->chk, 0, sizeof(pkg->chk));
	// ����Ҫ����3�ֽ���ν��ǰ��
	ret = uart_send_data(ctl, (u8 *)pkg, 3, pkg->chk);
	if (ret < 0) {
		goto out;
	}
	ret = uart_send_data_crc(ctl, (u8 *)pkg->data, pkg->len - 2, pkg->chk);
	if (ret < 0) {
		goto out;
	}
	ret = 3 + pkg->len;
out:
	// ���ͽ����������л�������ģʽ
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	udelay(1);
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	return ret;
}

// ����������ݰ����Զ��ж�У��, Ҫ���ṩ�ն˺�
static int tprt_recv_packet(term_pkg *pkg)
{
	// �����������ݰ�
	int ret;
	AT91PS_USART ctl;
#ifdef DEBUG
	unsigned char num = pkg->num;
#endif
	if (!(pkg->num & 0x80)) {
		// ǰ��ͨ����UART0
		ctl = uart_ctl0;
	} else {
		// ����ͨ����UART2
		ctl = uart_ctl2;
	}
	memset(pkg->chk, 0, sizeof(pkg->chk));
	ret = uart_recv_data(ctl, (u8 *)pkg, 3, pkg->chk);
	if (ret < 0) {
#ifdef DEBUG
		printk("%d uart_recv_data cnt %d, recv no %d error\n", num, 3, pkg->num);
#endif
		goto out;
	}
	ret = uart_recv_data_crc(ctl, pkg->data, pkg->len - 2, pkg->chk);
	if (ret < 0) {
#ifdef DEBUG
		printk("%d uart_recv_data_crc cnt %d, no %d error\n", num, pkg->len - 2, pkg->num);
#endif
		goto out;
	}
out:
	return ret;
}

// ������������������ݰ����Զ��ж�У��
int tprt_recv_packet_aftercmd(term_pkg *pkg)
{
	// �����������ݰ�
	int ret;
	AT91PS_USART ctl;
	if (!(pkg->num & 0x80)) {
		// ǰ��ͨ����UART0
		ctl = uart_ctl0;
	} else {
		// ����ͨ����UART2
		ctl = uart_ctl2;
	}
	ret = uart_recv_data(ctl, (u8 *)&pkg->len, 1, pkg->chk);
	if (ret < 0) {
		pr_debug("uart_recv_data: %d tprt_recv_packet_aftercmd error %d\n", pkg->num, ret);
		goto out;
	}
	ret = uart_recv_data_crc(ctl, pkg->data, pkg->len - 2, pkg->chk);
	if (ret < 0) {
		pr_debug("uart_recv_data_crc: %d tprt_recv_packet_aftercmd error %d\n", pkg->num, ret);
		goto out;
	}
out:
	return ret;
}

/*
 * ����GA, ����0����, <0����
 * write by wjzhe, 2008-8-20
 */
static int recv_ga(tprt_ram *ptrm)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	//memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	// GA��û������
	ret = tprt_recv_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		printk("recv ga: recv packet error\n");
#endif
		ptrm->status = ret;
		return ret;
	}
	if ((pkg->num & 0x7F) != (ptrm->termno & 0x7F)) {
#ifdef DEBUG
		printk("recv ga: term no error pkg %d, ptrm %d\n", pkg->num, ptrm->termno);
#endif
		ptrm->status = NOCOM;
		return -1;
	}
	switch (pkg->cmd) {
		case 0x88:
		case 0x99:
			ret = 0;
			break;
		case 0xAA:
		case 0xBB:
#ifdef DEBUG
			printk("recv ga: recv cmd 0xAA 0xBB\n");
#endif
			ptrm->status = ERRTERM;
			ret = ERRTERM;
		default:
#ifdef DEBUG
			printk("recv ga: unknown cmd %02x\n", pkg->cmd);
#endif
			ptrm->status = STATUSUNSET;
			ret = STATUSUNSET;
	}
	return ret;
}

/*
0x88:(1)���ݽ�����ȷ(2)����������Ҫ����ʱ�������������ɹ���
0x99:������������Ҫ�ն�ִ�о������ʱ�������������ȷ������ʧ�ܣ�
0xAA:���մ���0xBB�����ش���
 * flag == 0 ������ȷ
 * flag > 1  �ն˻�ִ�о������, ������ȷ, ������ʧ��
 * flag < 0  ���մ���
 * �˺�����������GA��, ���һ��GA����
 */
static int send_ga(tprt_ram *ptrm, int flag)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	memset(pkg, 0, sizeof(*pkg));
	// ��֯����
	pkg->num = ptrm->termno;
	if (flag == 0) {
		pkg->cmd = 0x88;
	} else if (flag > 0) {
		pkg->cmd = 0x99;
	} else {
		pkg->cmd = 0xAA;
	}
	pkg->len = 2;
	// ��������
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
		ptrm->status = ERRSEND;
	}
	return ret;
}

// ���������ն����ò�����
int tprt_msend_conf(tprt_ram *ptrm)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	// �ȳ�ʼ������ֵ
	memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	pkg->cmd = TPRT_SEND_CONF;
	pkg->len = 9;
	memcpy(pkg->data, &ptrm->pterm->val, 7);
	// 1-byte term no.
	// 1-byte command 0x64
	// 1-byte packet length 0x8
	// 2-byte �ն��趨���¶�ֵ
	// 1-byte �¶�����ƫ��ֵ
	// 1-byte ������
	// 1-byte �¶Ȳ���ʱ��������λ10�룩
	// 1-byte �̵�������ʱ��������λ10�룩
	// 1-byte ����
	// 2-byte У��
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		printk("tprt_msend_conf: tprt_send_packet failed\n");
#endif
		ptrm->status = ERRSEND;		// send timout
		return -1;
	}
	// ��ȡGA
	ret = recv_ga(ptrm);
#ifdef DEBUG
    if (ret < 0) {
        printk("send term %d conf recv ga failed\n", ptrm->termno);
    } else {
        printk("send term %d conf recv ga OK!!!\n", ptrm->termno);
    }
#endif
    return ret;
}

// �ն��������ò���
int tprt_rsend_conf(tprt_ram *ptrm)
{
	// ��㺯���Ѿ������������
	// �������ݰ�
	int ret;
	term_pkg *pkg = ptrm->pkg;
	// �ȳ�ʼ������ֵ
	memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	pkg->cmd = TPRT_REQ_CONF;
	pkg->len = 9;		// ...??
	memcpy(pkg->data, &ptrm->pterm->val, 7);
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		printk("tprt_rsend_conf: tprt_send_packet failed\n");
#endif
		ptrm->status = ERRSEND;
	}
	return ret;
}
#ifdef CONFIG_TPRT_NC
int tprt_rsend_power_conf(tprt_ram *ptrm)
{
	// ��㺯���Ѿ������������
	// �������ݰ�
	int ret;
	term_pkg *pkg = ptrm->pkg;
	ret = find_powernum2(ptrm->termno);
	if (ret == -1) {
		// error
		return -1;
	}
	// �ȳ�ʼ������ֵ
	memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	pkg->cmd = TPRT_SEND_NUM;
	pkg->len = 8;		// ...??
	pkg->data[0] = pwctrl[ret].base;
	memcpy(pkg->data + 1, &pwctrl[ret].tflag, sizeof(pwctrl[ret].tflag));
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		printk("tprt_rsend_conf: tprt_send_packet failed\n");
#endif
		ptrm->status = ERRSEND;
	}
	return ret;
}
#endif

// ��ȡ�ն�����
int tprt_get_flow(tprt_ram *ptrm, tprt_flow *pflow)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	pkg->cmd = TPRT_GET_FLOW;
	pkg->len = 2;
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		//printk("tprt_get_flow: tprt_send_packet failed\n");
#endif
		ptrm->status = ERRSEND;
		goto out;
	}
	ret = tprt_recv_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		//printk("tprt_get_flow: tprt_recv_packet failed\n");
#endif
		ptrm->status = ret;
		goto out;
	}
	//pr_debug("tprt_get_flow: save flow....\n");
	// ��������, pflow�п���NULL
	if (pflow) {
		memset(pflow, 0, sizeof(*(pflow)));
		pflow->TerminalID = ptrm->termno;
		pflow->RoomID = ptrm->roomid;
		{
			short val;
			memcpy(&val, pkg->data, sizeof(val));
			pflow->Temperature = val;
		}
		//memcpy(&pflow->Temperature, pkg->data, sizeof(short));
#ifdef CONFIG_TPRT_NC
		// �µĿ��Ʒ�ʽ���ն˼̵�����Ϣ�����ڵ�Դ���ư���
		//pflow->RelayStatus = ptrm->power->r_ctrl & (1 << (ptrm->termno & 0xF));
		if (ptrm->power->r_ctrl & (1 << (ptrm->termno & 0xF))) {
			pflow->RelayStatus |= 0x10;
		}
		if (ptrm->power->s_ctrl & (1 << (ptrm->termno & 0xF))) {
			pflow->RelayStatus |= 0x1;
		}
		//printk("term %d recv s_ctrl 0x%x and r_ctrl 0x%x\n",
		//	ptrm->termno, ptrm->power->s_ctrl, ptrm->power->r_ctrl);
		//printk("so RelayStatus = %x\n", pflow->RelayStatus);
#else
		pflow->RelayStatus = pkg->data[2];
#endif
		// ʱ����
		_fill_flow_time(pflow->FlowTime);
	}
	// �����¶Ⱥ�״̬��Ϣ
	ptrm->val = *(unsigned short *)pkg->data;//pflow->Temperature;
#ifdef CONFIG_TPRT_NC
	ptrm->flow_tm = _get_current_time();
	if (ptrm->power) {
		//ptrm->ctrl = ptrm->power->s_ctrl & (1 << (ptrm->termno & 0xF));
		ptrm->ctrl = 0;
		if (ptrm->power->r_ctrl & (1 << (ptrm->termno & 0xF))) {
			ptrm->ctrl |= 0x10;
		}
		if (ptrm->power->s_ctrl & (1 << (ptrm->termno & 0xF))) {
			ptrm->ctrl |= 0x01;
		}
	}
#ifdef DEBUG
	else {
		panic("tprt_get_flow error power NULL\n");
	}
#endif
#else
	ptrm->ctrl = pkg->data[2];
#endif
out:
	return ret;
}
// ��ȡ�ն���ˮ
int tprt_recv_flow(tprt_ram *ptrm)
{
	int ret = 0;
	term_pkg *pkg = ptrm->pkg;
	short val;
	// ��㺯���Ѿ������������
	// �ȱ������ݣ�����һЩ�ж�
	if (pkg->len != 6) {
#ifdef DEBUG
		printk("waring: %d tprt recv flow length %d\n", ptrm->termno, pkg->len);
#endif
	}
#ifdef CONFIG_TPRT_NC
	// ��Դ����ֻ���ռ̵���״̬
	if (ptrm->is_power/* (ti = find_powernum2(ptrm->termno)) >= 0 */) {
		// save ctrl tm
		if (ptrm->power->is_set) {
			memcpy(&ptrm->power->s_ctrl, pkg->data, 2);
			memcpy(&ptrm->power->r_ctrl, pkg->data + 2, 2);
			pr_debug("~~~~~~~~~~~~term %d recv s_ctrl 0x%x and r_ctrl 0x%x\n",
				ptrm->termno, ptrm->power->s_ctrl, ptrm->power->r_ctrl);
		} else {
			pr_debug("power term %d not set\n", ptrm->termno);
		}
		if (ptrm->power->num != ptrm->termno) {
			pr_debug("power term %d not same %d\n", ptrm->termno, ptrm->power->num);
		}
	} else {
#endif
	// ��������ǰ��Ϣ���浽�ڴ���, ��������ˮ
	ptrm->val = 0;
	memcpy(&val, pkg->data, 2);
	ptrm->val = val;
	ptrm->ctrl = pkg->data[2];
#ifdef CONFIG_TPRT_NC
	ptrm->flow_tm = _get_current_time();
	}
#endif
	// Ȼ����GA
	ret = send_ga(ptrm, ret);
	return ret;
}
// ���ͽкţ����սкţ�flag��ʾ�Ƿ�������
// ����<0����=0ֻ�лغ�û������>0����������ͬʱ��������
int tprt_send_num(tprt_ram *ptrm, int flag)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	unsigned char termno = ptrm->termno;// & 0x7F; fixed by wjzhe
	AT91PS_USART ctl;
	memset(pkg, 0, sizeof(*pkg));
	if (!(ptrm->termno & 0x80)) {
		ctl = uart_ctl0;
	} else {
		ctl = uart_ctl2;
	}
	sel_ch(ptrm->termno);// �κ������, �Ƚ�138����λ��λ
	// ����NUM������ǲ���Ҫ����У���
	pkg->num = ptrm->termno;// & 0x7F; fixed by wjzhe
	pkg->num ^= (flag ? 0x80 : 0);
	// �ھ�λ��־Ϊ1
	ret = send_addr(pkg->num, ptrm->termno);
	if (ret < 0) {
#ifdef SDEBUG
		printk("tprt_send_num: send addr failed\n");
#endif
		ptrm->status = ERRSEND;
		return -1;
	}
	// ����Ҫ�ȴ��غ�
	ret = uart_recv_data(ctl, &pkg->num, 1, pkg->chk);
	if (ret < 0) {
#ifdef SDEBUG
		printk("tprt_send_num: recv term no. failed\n");
#endif
		ptrm->status = ret;
		return ret;
	}
	// �صĺŲ�����
	if ((pkg->num & 0x7F) != (termno & 0x7F)) {
#ifdef DEBUG
		printk("tprt_send_num recvno: %02x but sendno: %02x\n", pkg->num, termno);
#endif
		ptrm->status = NOCOM;
		return NOCOM;
	}
	if (flag) {
		// ����������������ǲ��ᴦ����������
		return 0;
	}
	// �ն˻�����������((pkg->num ^ (0x80 ^ ptrm->termno)))
	if ((pkg->num ^ (0x80 ^ ptrm->termno))) {
		// �ն˻��صĺ�û������
		return 0;
	}
	// �ն˻��������ˣ��������Ǽ�����������
	// ����Ͱ�����
	ret = uart_recv_data(ctl, &pkg->cmd, 2, pkg->chk);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	ret = uart_recv_data_crc(ctl, pkg->data, pkg->len - 2, pkg->chk);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	return ret;
}

// ����Ҫ���кź���, ֻ�Ե�һ�ն˽к�
// ����һ�������к�����
static int __tprt_call(tprt_ram *ptrm)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	// �ȷ��ͻ�����, �ȴ���Ӧ
	ret = tprt_send_num(ptrm, 0);
	if (ret < 0) {
		// ���ջ��߷��ͳ�������
		return ret;
	}
	// ����pkg�����ж��Ƿ�������
	switch (pkg->cmd) {
		case TPRT_REQ_CONF:
			ret = tprt_rsend_conf(ptrm);
			if (ret < 0)
				pr_debug("tprt_rsend_conf: return %d\n", ret);
			break;
		case TPRT_RECV_FLOW:
			ret = tprt_recv_flow(ptrm);
			if (ret < 0)
				pr_debug("tprt_rsend_conf: return %d\n", ret);
			break;
		case 0:
			break;
		default:
			pr_debug("unknow command %02x\n", pkg->cmd);
			break;
	}
	return ret;
}

int tprt_call(tprt_data *p)
{
	int i, n = p->n;
	int ret = 0;
	tprt_ram *ptrm = p->pterm;
	for (i = 0; i < n; i++, ptrm++) {
		ret = __tprt_call(ptrm);
	}
	return ret;
}

// ���������ն˲�������
int tprt_update_conf(tprt_ram *ptrm)
{
	int ret;
	// ������Ľк�
	ret = tprt_send_num(ptrm, 1);
	if (ret < 0) {
		// �п��ܷ���û�гɹ�, ���߻صĺ�������
		//pr_debug("tprt_update_conf: send_num ret %d\n", ret);
		goto out;
	}
	// ���ڿ�ʼ�������²�������
	ret = tprt_msend_conf(ptrm);
	if (ret < 0) {
		// û�гɹ�
		//pr_debug("tprt_update_conf: send_conf ret %d\n", ret);
		goto out;
	}
out:
	return ret;
}
// ������ȡ�ն˲ɼ����¶ȵ���Ϣ
int tprt_req_data(tprt_ram *ptrm, tprt_flow *pflow)
{
	int ret;
	// ������к�
	ret = tprt_send_num(ptrm, 1);
	if (ret < 0) {
		//pr_debug("tprt_req_data: send addr ret %d\n", ret);
		goto out;
	}
	ret = tprt_get_flow(ptrm, pflow);
	if (ret < 0) {
		//pr_debug("tprt_req_data: get flow ret %d\n", ret);
		goto out;
	}
out:
	return ret;
}
#ifdef CONFIG_TPRT_NC
// �������͵�Դ��������
int tprt_msend_ctrl(tprt_ram *ptrm, tprt_power_ctrl *ctrl)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	// �ȳ�ʼ������ֵ
	memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	pkg->cmd = TPRT_SEND_CTRL;
	pkg->len = 4;
	memcpy(pkg->data, &ctrl->ctrl, sizeof(ctrl->ctrl));
	// 15��̵����Ŀ������ݣ�2�ֽڣ���λ��ǰ��
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		printk("tprt_msend_ctrl: tprt_send_packet failed\n");
#endif
		ptrm->status = ERRSEND;		// send timout
		return -1;
	}
	// ��ȡGA
	ret = recv_ga(ptrm);
#ifdef DEBUG
    if (ret < 0) {
        printk("send term %d ctrl recv ga failed\n", ptrm->termno);
    } else {
        printk("send term %d ctrl recv ga OK!!!\n", ptrm->termno);
    }
#endif
	pr_debug("send power ctrl: %d code %04x %02x %02x\n",
		ctrl->num, ctrl->ctrl, pkg->data[0], pkg->data[1]);
    return ret;
}

// ���µ�Դ��������
int tprt_send_power_ctrl(tprt_ram *ptrm, tprt_power_ctrl *ctrl)
{
	int ret;
	// ������Ľк�
	ret = tprt_send_num(ptrm, 1);
	if (ret < 0) {
		// �п��ܷ���û�гɹ�, ���߻صĺ�������
		pr_debug("tprt_send_power_ctrl: send_num ret %d\n", ret);
		goto out;
	}
	// ���Ϳ�������
	ret = tprt_msend_ctrl(ptrm, ctrl);
	if (ret < 0) {
		// û�гɹ�
		pr_debug("tprt_send_power_ctrl: send_conf ret %d\n", ret);
		goto out;
	}
out:
    return ret;
}
#endif

