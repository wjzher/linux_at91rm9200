/*
 * Copyright (c) 2008, NKTY Company
 * All rights reserved.
 * 文件名称：tprt_com.c
 * 摘    要：实现和温控终端485通讯底层函数
 *
 * 当前版本：1.0.0
 * 作    者：wjzhe
 * 完成日期：2008年8月19日
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
 * 可控执行器件数：1～15。
 * 电源控制器终端编号必须为15（1～14）、31（16～30）、47（32～46）、
 * 63（48～62）、79（64～78）、95（80～95）、111（96～110）、
 * 127（112～126）、143（128～142）、159（144～158）、175（160～174）、
 * 191（176～190）、207（192～206）、223（208～222）、239（224～238）、
 * 255（240～254），温控终端不能占用
 */
// 发送数据但没有校验
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
// 发送数据并加校验
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
	// 还要发送chk[2]
	chk[1] ^= chk[0];		// 异或和还要算累加结果
	for (i = 0; i < 2; i++) {
		ctl->US_THR = chk[i];
		if (!uart_poll(&ctl->US_CSR, AT91C_US_TXEMPTY)) {
			return -1;
		}
	}
	return 0;
}
// 不验证校验
static int uart_recv_data(AT91PS_USART ctl, u8 *data, int cnt, u8 *chk)
{
	int ret = 0, i;
	u8 tmp;
	for (i = 0; i < cnt; i++) {
		// 超时等待
		if (!uart_poll(&ctl->US_CSR, AT91C_US_RXRDY)) {
			ret = NOTERMINAL;
			goto out;
		}
		// 判断标志位
		if (((ctl->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((ctl->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			tmp = ctl->US_RHR;
 			ctl->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// 拷贝数据
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
// 验证校验
static int uart_recv_data_crc(AT91PS_USART ctl, u8 *data, int cnt, u8 *chk)
{
	int ret, i;
	u8 k[2] = {0};
	u8 tmp;
	for (i = 0; i < cnt; i++) {
		// 超时等待
		if (!uart_poll(&ctl->US_CSR, AT91C_US_RXRDY)) {
			ret = NOTERMINAL;
			goto out;
		}
		// 判断标志位
		if(((ctl->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((ctl->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			tmp = ctl->US_RHR;
 			ctl->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// 拷贝数据
		*(volatile char *)data = ctl->US_RHR;
		chk[0] += *data;
		chk[1] ^= *data;
		data++;
	}
	for (i = 0; i < 2; i++) {
		// 超时等待
		if (!uart_poll(&ctl->US_CSR, AT91C_US_RXRDY)) {
			ret = NOTERMINAL;
			goto out;
		}
		// 判断标志位
		if(((ctl->US_CSR & AT91C_US_OVRE) == AT91C_US_OVRE) ||
			((ctl->US_CSR & AT91C_US_FRAME) == AT91C_US_FRAME)) {
			tmp = ctl->US_RHR;
 			ctl->US_CR |= AT91C_US_RSTSTA;		// reset status
			//uart_ctl2->US_CR |= AT91C_US_TXEN | AT91C_US_RXEN;
 			ret = NOCOM;
			goto out;
		}
		// 拷贝数据
		k[i] = ctl->US_RHR;
	}
	// 接收校验
	chk[1] ^= k[0];		// 异或校验要加累加和
	if (k[0] == chk[0] && k[1] == chk[1]) {
		// 校验通过
		ret = cnt;
	} else {
		//printk("check error %02x%02x, but we %02x%02x\n",
		//	k[0], k[1], chk[0], chk[1]);
		ret = ERRVERIFY;
	}
out:
	return ret;
}

// 负责发送数据包，自动加校验
static int tprt_send_packet(term_pkg *pkg)
//(u8 termno, u8 cmd, u8 *data, u8 len)
{
	// 用来发送数据包
	int ret = 0;
	AT91PS_USART ctl;
	//AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12;		// enable send data
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(20);
	// 发送数据时不需要设置通道, 详见原理图
	if (!(pkg->num & 0x80)) {
		// 前四通道用UART0
		ctl = uart_ctl0;
	} else {
		// 后四通道用UART2
		ctl = uart_ctl2;
	}
	memset(pkg->chk, 0, sizeof(pkg->chk));
	// 现在要发送3字节所谓的前导
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
	// 发送结束后马上切换到接收模式
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	udelay(1);
	//uart_ctl0->US_CR = AT91C_US_RXEN;
	return ret;
}

// 负责接收数据包，自动判断校验, 要求提供终端号
static int tprt_recv_packet(term_pkg *pkg)
{
	// 用来接收数据包
	int ret;
	AT91PS_USART ctl;
#ifdef DEBUG
	unsigned char num = pkg->num;
#endif
	if (!(pkg->num & 0x80)) {
		// 前四通道用UART0
		ctl = uart_ctl0;
	} else {
		// 后四通道用UART2
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

// 负责接收有命令后的数据包，自动判断校验
int tprt_recv_packet_aftercmd(term_pkg *pkg)
{
	// 用来接收数据包
	int ret;
	AT91PS_USART ctl;
	if (!(pkg->num & 0x80)) {
		// 前四通道用UART0
		ctl = uart_ctl0;
	} else {
		// 后四通道用UART2
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
 * 接收GA, 返回0正常, <0错误
 * write by wjzhe, 2008-8-20
 */
static int recv_ga(tprt_ram *ptrm)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	//memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	// GA包没有数据
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
0x88:(1)数据接收正确(2)当命令字需要操作时表接收与操作均成功；
0x99:仅在命令字需要终端执行具体操作时产生，表接收正确但操作失败；
0xAA:接收错误；0xBB：严重错误
 * flag == 0 接收正确
 * flag > 1  终端机执行具体操作, 接收正确, 但操作失败
 * flag < 0  接收错误
 * 此函数用来发送GA包, 完成一次GA操作
 */
static int send_ga(tprt_ram *ptrm, int flag)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	memset(pkg, 0, sizeof(*pkg));
	// 组织数据
	pkg->num = ptrm->termno;
	if (flag == 0) {
		pkg->cmd = 0x88;
	} else if (flag > 0) {
		pkg->cmd = 0x99;
	} else {
		pkg->cmd = 0xAA;
	}
	pkg->len = 2;
	// 发送数据
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
		ptrm->status = ERRSEND;
	}
	return ret;
}

// 主动发送终端配置参数包
int tprt_msend_conf(tprt_ram *ptrm)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	// 先初始化几个值
	memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	pkg->cmd = TPRT_SEND_CONF;
	pkg->len = 9;
	memcpy(pkg->data, &ptrm->pterm->val, 7);
	// 1-byte term no.
	// 1-byte command 0x64
	// 1-byte packet length 0x8
	// 2-byte 终端设定的温度值
	// 1-byte 温度允许偏差值
	// 1-byte 控制码
	// 1-byte 温度采样时间间隔（单位10秒）
	// 1-byte 继电器控制时间间隔（单位10秒）
	// 1-byte 备用
	// 2-byte 校验
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		printk("tprt_msend_conf: tprt_send_packet failed\n");
#endif
		ptrm->status = ERRSEND;		// send timout
		return -1;
	}
	// 收取GA
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

// 终端请求配置参数
int tprt_rsend_conf(tprt_ram *ptrm)
{
	// 外层函数已经接收完成请求
	// 发送数据包
	int ret;
	term_pkg *pkg = ptrm->pkg;
	// 先初始化几个值
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
	// 外层函数已经接收完成请求
	// 发送数据包
	int ret;
	term_pkg *pkg = ptrm->pkg;
	ret = find_powernum2(ptrm->termno);
	if (ret == -1) {
		// error
		return -1;
	}
	// 先初始化几个值
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

// 获取终端数据
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
	// 保存数据, pflow有可能NULL
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
		// 新的控制方式，终端继电器信息包含在电源控制板中
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
		// 时间呢
		_fill_flow_time(pflow->FlowTime);
	}
	// 保存温度和状态信息
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
// 收取终端流水
int tprt_recv_flow(tprt_ram *ptrm)
{
	int ret = 0;
	term_pkg *pkg = ptrm->pkg;
	short val;
	// 外层函数已经接收完成请求
	// 先保存数据，并做一些判断
	if (pkg->len != 6) {
#ifdef DEBUG
		printk("waring: %d tprt recv flow length %d\n", ptrm->termno, pkg->len);
#endif
	}
#ifdef CONFIG_TPRT_NC
	// 电源控制只接收继电器状态
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
	// 仅仅将当前信息保存到内存中, 不生成流水
	ptrm->val = 0;
	memcpy(&val, pkg->data, 2);
	ptrm->val = val;
	ptrm->ctrl = pkg->data[2];
#ifdef CONFIG_TPRT_NC
	ptrm->flow_tm = _get_current_time();
	}
#endif
	// 然后发送GA
	ret = send_ga(ptrm, ret);
	return ret;
}
// 发送叫号，并收叫号，flag表示是否有命令
// 返回<0错误，=0只有回号没有请求，>0，包含请求同时会有数据
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
	sel_ch(ptrm->termno);// 任何情况下, 先将138数据位置位
	// 发送NUM，这个是不需要加入校验的
	pkg->num = ptrm->termno;// & 0x7F; fixed by wjzhe
	pkg->num ^= (flag ? 0x80 : 0);
	// 第九位标志为1
	ret = send_addr(pkg->num, ptrm->termno);
	if (ret < 0) {
#ifdef SDEBUG
		printk("tprt_send_num: send addr failed\n");
#endif
		ptrm->status = ERRSEND;
		return -1;
	}
	// 现在要等待回号
	ret = uart_recv_data(ctl, &pkg->num, 1, pkg->chk);
	if (ret < 0) {
#ifdef SDEBUG
		printk("tprt_send_num: recv term no. failed\n");
#endif
		ptrm->status = ret;
		return ret;
	}
	// 回的号不对吗
	if ((pkg->num & 0x7F) != (termno & 0x7F)) {
#ifdef DEBUG
		printk("tprt_send_num recvno: %02x but sendno: %02x\n", pkg->num, termno);
#endif
		ptrm->status = NOCOM;
		return NOCOM;
	}
	if (flag) {
		// 如果是有命令了我们不会处理后面的数据
		return 0;
	}
	// 终端机主动请求吗((pkg->num ^ (0x80 ^ ptrm->termno)))
	if ((pkg->num ^ (0x80 ^ ptrm->termno))) {
		// 终端机回的号没有命令
		return 0;
	}
	// 终端机有命令了，现在我们继续接收数据
	// 命令和包长度
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

// 下面要做叫号函数, 只对单一终端叫号
// 这是一次正常叫号流程
static int __tprt_call(tprt_ram *ptrm)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	// 先发送机器号, 等待回应
	ret = tprt_send_num(ptrm, 0);
	if (ret < 0) {
		// 接收或者发送出了问题
		return ret;
	}
	// 根据pkg包来判断是否有命令
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

// 主动更新终端参数函数
int tprt_update_conf(tprt_ram *ptrm)
{
	int ret;
	// 带命令的叫号
	ret = tprt_send_num(ptrm, 1);
	if (ret < 0) {
		// 有可能发送没有成功, 或者回的号有问题
		//pr_debug("tprt_update_conf: send_num ret %d\n", ret);
		goto out;
	}
	// 现在开始启动更新参数流程
	ret = tprt_msend_conf(ptrm);
	if (ret < 0) {
		// 没有成功
		//pr_debug("tprt_update_conf: send_conf ret %d\n", ret);
		goto out;
	}
out:
	return ret;
}
// 主动收取终端采集的温度等信息
int tprt_req_data(tprt_ram *ptrm, tprt_flow *pflow)
{
	int ret;
	// 带命令叫号
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
// 主动发送电源控制命令
int tprt_msend_ctrl(tprt_ram *ptrm, tprt_power_ctrl *ctrl)
{
	int ret;
	term_pkg *pkg = ptrm->pkg;
	// 先初始化几个值
	memset(pkg, 0, sizeof(*pkg));
	pkg->num = ptrm->termno;
	pkg->cmd = TPRT_SEND_CTRL;
	pkg->len = 4;
	memcpy(pkg->data, &ctrl->ctrl, sizeof(ctrl->ctrl));
	// 15组继电器的控制数据（2字节，低位在前）
	ret = tprt_send_packet(pkg);
	if (ret < 0) {
#ifdef DEBUG
		printk("tprt_msend_ctrl: tprt_send_packet failed\n");
#endif
		ptrm->status = ERRSEND;		// send timout
		return -1;
	}
	// 收取GA
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

// 更新电源控制数据
int tprt_send_power_ctrl(tprt_ram *ptrm, tprt_power_ctrl *ctrl)
{
	int ret;
	// 带命令的叫号
	ret = tprt_send_num(ptrm, 1);
	if (ret < 0) {
		// 有可能发送没有成功, 或者回的号有问题
		pr_debug("tprt_send_power_ctrl: send_num ret %d\n", ret);
		goto out;
	}
	// 发送控制命令
	ret = tprt_msend_ctrl(ptrm, ctrl);
	if (ret < 0) {
		// 没有成功
		pr_debug("tprt_send_power_ctrl: send_conf ret %d\n", ret);
		goto out;
	}
out:
    return ret;
}
#endif

