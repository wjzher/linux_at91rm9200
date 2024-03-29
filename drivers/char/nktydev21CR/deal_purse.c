/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：deal_purse.c
 * 摘    要：定义处理新POS的接口函数, 所有调用的底层函数来自at91_uart.c, uart_dev.c
 *			 增加对term_ram结构体的新成员操作
 * 			 
 * 当前版本：1.1
 * 作    者：wjzhe
 * 完成日期：2007年9月3日
 *
 * 取代版本：1.0
 * 原作者  ：wjzhe
 * 完成日期：2007年6月14日
 */
#include "pdccfg.h"
#include "deal_purse.h"
#include "uart.h"
//#define DEBUG
#define E485COM 485102010
/*
 * 打印输出struct tradenum, 仅在错误输出时调用, 当前文件使用
 */
#ifdef DEBUG
static void out_tnum(struct tradenum *t_num)
{
	printk("%02x%02x %02x, %02x %02x: %d\n",
		t_num->s_tm.hyear, t_num->s_tm.lyear, t_num->s_tm.mon,
		t_num->s_tm.day, t_num->s_tm.hour, t_num->num);
}
#endif

/*
 * 计算校验值
 */
static int com_verify(term_ram *ptrm, unsigned char *buf, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++, buf++) {
		ptrm->add_verify += *buf;
		ptrm->dif_verify ^= *buf;
	}
	return 0;
}
/*
 * 打印输出struct tradenum, 仅在错误输出时调用, 当前文件使用
 */
#if 0
static void out_tnum(struct tradenum *t_num)
{
	printk("%02x%02x %02x, %02x %02x: %d\n",
		t_num->s_tm.hyear, t_num->s_tm.lyear, t_num->s_tm.mon,
		t_num->s_tm.day, t_num->s_tm.hour, t_num->num);
}
#endif
/*
 * 计算是否有黑名单操作需求, 返回需要下发的条数
 * write by wjzhe, June 12, 2007
 */
//#define DEBUG
int compute_blk(term_ram *ptrm, struct black_info *pblkinfo)
{
	//int ret;
	// 判断是否存在正确的交易号和版本号
	// 只在交易号非法或者版本号为0的情况主动攻取交易号和版本号
	if (/*(ptrm->blkinfo.trade_num.s_tm.hyear != 0x20)
		|| */(ptrm->blkinfo.edition == 0)
		/* && (ptrm->blkinfo.trade_num.num == 0)*/) {
		//if (ptrm->blkinfo.trade_num.s_tm.hyear == 0x20) {
		//}
		// term has no black info, request it
#ifdef DEBUG
		printk("term has no black info: %02x, request it\n", ptrm->blkinfo.trade_num.s_tm.hyear);
		out_tnum(&ptrm->blkinfo.trade_num);
#endif
		if (purse_get_edition(ptrm) < 0) {
#ifdef DEBUG
			printk("purse_get_edition failed\n");
#endif
			return -E485COM;
		}
	}
#if 0
	printk("globle blk info is: %d, %d\n", blkinfo->edition, blkinfo->trade_num.num);
	printk("term blk info is: %d, %d\n", ptrm->blkinfo.edition, ptrm->blkinfo.trade_num.num);
#endif
#ifdef DEBUG
	if (ptrm->blkinfo.edition != pblkinfo->edition) {
		printk("compute_blk term %d blk edition not match %d: %d\n",
			ptrm->term_no, ptrm->blkinfo.edition, pblkinfo->edition);
		//return -1;
	}
#endif
#if 0
	// 比对交易号是否和当前的一致
	ret = memcmp(&blkinfo->trade_num, &ptrm->blkinfo.trade_num, sizeof(ptrm->blkinfo.trade_num));
	printk("memcmp: %d\n", ret);
	if (ret == 0)
		return 0;
#endif
	// 必须保证交易号连续, 此返回值才正确
#ifdef DEBUG
	//printk("LOCAL: %d, TERM %d, blk %d\n", pblkinfo->trade_num.num, ptrm->blkinfo.trade_num.num, pblkinfo->trade_num.num - ptrm->blkinfo.trade_num.num);
#endif
	return pblkinfo->trade_num.num - ptrm->blkinfo.trade_num.num;
}

/*
 * 折半查找黑名单, 比对交易号大小
 * write by wjzhe, June 12, 2007
 */
static black_acc *search_blk2(const unsigned long num, black_acc *pbacc, int cnt)
{
	int head, tail, mid;
	if (cnt <= 0 || !pbacc)		// cnt < 0, there is no account
		return NULL;
	head = 0;
	tail = cnt - 1;
	mid = (tail + head) / 2;
//	printk("%d %d pbacc[%d] = %d pbacc[%d] = %d\n", num, cnt, 
//		tail, pbacc[tail].t_num.num, 0, pbacc[0].t_num.num);
	do {
		if (pbacc[mid].t_num.num == num) {
			pbacc += mid;
			goto end;
		} else if (pbacc[mid].t_num.num > num) {
			tail = mid;
			mid = (tail + head) / 2;
		} else {
			head = mid;
			mid = (tail + head) / 2 + 1;
		}
	} while (head != tail && tail != (head + 1)/*mid <= tail && mid >= head*/);
	if (pbacc[head].t_num.num == num) {
		pbacc += head;
		goto end;
	}
	if (pbacc[tail].t_num.num == num) {
		pbacc += tail;
		goto end;
	}
//	printk("no find %d %d\n", pbacc[head].t_num.num, pbacc[tail].t_num.num);
//	printk("t h %d %d\n", tail, head);
	pbacc = NULL;
end:
	return pbacc;
}

/*
 * flag: 0->ctl0; 1->ctl2
 * send data to 485, return count is ok, -1 is timeout, -2 is other error
 * created by wjzhe, 10/16/2006
 * 位时间34.7us
 */
static int send_data2(char *buf, size_t count, term_ram *ptrm)	// attention: can continue sending?
{
	int i;
	// enable send data channel
	AT91_SYS->PIOC_SODR &= (!0x00001000);
	AT91_SYS->PIOC_CODR |= 0x00001000;
	udelay(1);
	// 发送数据时不需要设置通道, 详见原理图
	if (!(ptrm->term_no & 0x80)) {
		// 前四通道用UART0
		// 不发地址位
		uart_ctl0->US_CR &= !(AT91C_US_SENDA);
		for (i = 0; i < count; i++) {
			// 将buf数据拷贝到发送缓冲
			uart_ctl0->US_THR = *buf;
			// 等待发送完毕
			if (!uart_poll(&uart_ctl0->US_CSR, AT91C_US_TXEMPTY))
				return -1;
			// 校验
			ptrm->add_verify += (unsigned char)(*buf);
			ptrm->dif_verify ^= (unsigned char)(*buf);
			buf++;
		}
	} else {
		// 后四通道用UART2
		uart_ctl2->US_CR &= !(AT91C_US_SENDA);
		for (i = 0; i < count; i++) {
			uart_ctl2->US_THR = *buf;
			if (!uart_poll(&uart_ctl2->US_CSR, AT91C_US_TXEMPTY))
				return -1;
			ptrm->add_verify += (unsigned char)(*buf);
			ptrm->dif_verify ^= (unsigned char)(*buf);
			buf++;
		}
	}
#ifdef RS485_REV
	udelay(1);
#endif
	// enable receive data channel
	AT91_SYS->PIOC_CODR &= (!0x00001000);
	AT91_SYS->PIOC_SODR |= 0x00001000;
	udelay(1);
	return count;
}

/*
 * 发送黑名单数据, N条
 * write by wjzhe, June 12, 2007
 */
static int send_blkacc(term_ram *ptrm, black_acc *pbacc, int cnt)
{
	int i, ret;
	char opt = 0;//modified by duyy, 2013.11.21
	if (cnt == 0)
		return 0;
	if (pbacc == NULL)
		return -1;
	for (i = 0; i < cnt; i++) {
		// 发送四字节卡号
		ret = send_data2((char *)&pbacc[i].card_num, 4, ptrm);
		if (ret < 0)
			goto send_blkacc_failed;
		// 发送一字节操作记录
		ret = send_data2((char *)&opt, 1, ptrm);//modified by duyy, 2013.11.21
		if (ret < 0)
			goto send_blkacc_failed;
	}
	return 0;
send_blkacc_failed:
	ptrm->status = NOCOM;
	return -1;
}

/*
 * 发黑名单交易号, 9 bytes, 将校验写入ram
 * write by wjzhe, June 12, 2007
 */
static int send_trade_num(struct tradenum *t_num, term_ram *ptrm)
{
	int ret;
	unsigned char tm[5] = {0};//write by duyy, 2013.11.21
	// 发4字节记录号
	ret =  send_data2((char *)&t_num->num, sizeof(t_num->num), ptrm);
	if (ret < 0)
		return -1;
	// 发5字节时间
//	ret = send_data2((char *)t_num->tm, sizeof(t_num->tm), ptrm);//modified by duyy, 2013.11.20
	ret = send_data2((char *)tm, 5, ptrm);
	if (ret < 0)
		return -1;
	return 0;
}

/*
 * 收黑名单交易号, 9 bytes, 将校验写入ram
 * write by wjzhe, June 12, 2007
 */
static int recv_trade_num(struct tradenum *t_num, term_ram *ptrm)
{
	unsigned char *tmp;
	unsigned char tm[5] = {0};//write by duyy, 2013.11.21
	int i, ret;
	tmp = (unsigned char *)&t_num->num;
	// 先收4字节记录号
	for (i = 0; i < sizeof(t_num->num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d t_num->num error, ret:%d\n", i, ret);
			ptrm->status = ret;
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 再收5字节时间
//	tmp = (unsigned char *)t_num->tm;
	tmp = tm;//write by duyy, 2013.11.21
	for (i = 0; i < 5; /*sizeof(t_num->tm);*/ i++) {//modified by duyy, 2013.11.20
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d termblk.trade_num.tm error, ret:%d\n", i, ret);
			ptrm->status = ret;
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	return 0;
}

/*
 * 发送当前校验信息
 * write by wjzhe, June 12, 2007
 */
static int send_verify(term_ram *ptrm)
{
	int ret;
	// 发1字节累加校验
	ret = send_byte(ptrm->add_verify, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	// 发1字节异或校验
	ret = send_byte(ptrm->dif_verify, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	return 0;
}

/*
 * 接收并校验当前校验信息, 返回<0说明校验错误或者接收失败
 * write by wjzhe, June 12, 2007
 */
static int chk_verify(term_ram *ptrm)
{
	int ret;
	unsigned char trmadd, trmdif;
	// 先收1字节累加和
	ret = recv_data((char *)&trmadd, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		return ret;
	}
	// 再收1字节异或和
	ret = recv_data((char *)&trmdif, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		return ret;
	}
	// 验证校验和
	if ((trmadd != ptrm->add_verify) || (trmdif != ptrm->dif_verify)) {
		printk("verify %d:%d, %d:%d\n", ptrm->add_verify, trmadd,
			ptrm->dif_verify, trmdif);
		return ERRVERIFY;
	}
	return 0;
}
/*
 * 接收包长度, 入口参数为当前终端属性指针
 * 函数返回所接收的包长度值, 返回0xFF说明接收错误
 */
unsigned char recv_len(term_ram *ptrm)
{
	int ret;
	unsigned char len;
	ret = recv_data((char *)&len, ptrm->term_no);
	if (ret < 0) {
		printk("recv len error, ret:%d\n", ret);
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		return 0xFF;
	}
	ptrm->dif_verify ^= len;
	ptrm->add_verify += len;
	return len;
}
//#define TESTCODE

/*
0x88:(1)数据接收正确(2)当命令字需要操作时表接收与操作均成功；
0x99:仅在命令字需要终端执行具体操作时产生，表接收正确但操作失败；
0xAA:接收错误；0xBB：严重错误
 * flag == 0 接收正确
 * flag > 1  终端机执行具体操作, 接收正确, 但操作失败
 * flag < 0  接收错误
 * 此函数用来发送GA包, 完成一次GA操作
 */
static int send_ga(term_ram *ptrm, int flag)
{
	int ret;
	ptrm->add_verify = ptrm->dif_verify = 0;
	// 发1字节终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	if (flag == 0) {
		ret = send_byte(0x88, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->add_verify += 0x88;
		ptrm->dif_verify ^= 0x88;
	} else if (flag > 0) {
		ret = send_byte(0x99, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->add_verify += 0x99;
		ptrm->dif_verify ^= 0x99;
	} else {
		ret = send_byte(0xAA, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->add_verify += 0xAA;
		ptrm->dif_verify ^= 0xAA;
	}
	// 发包长度
	ret = send_byte(2, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify += 2;
	ptrm->dif_verify ^= 2;
	// 发校验
	ret = send_verify(ptrm);
	return ret;
}

/*
 * 接收GA, 返回0正常, <0错误
 * write by wjzhe, June 12, 2007
 */
static int recv_ga(term_ram *ptrm)
{
	int ret;
	unsigned char recvno, ga, len;
	ptrm->add_verify = ptrm->dif_verify = 0;
	// 收终端号
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
		printk("recv ga termno failed, ret: %d\n", ret);
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
		// test code
#ifdef TESTCODE
		goto next;
#else
		return ret;
#endif
	}
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("recv ga termno error, %d: %d\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		usart_delay(1);
		return -1;
	}
#ifdef TESTCODE
next:
#endif
	ptrm->add_verify += recvno;
	ptrm->dif_verify ^= recvno;
	// 收GA
	ret = recv_data((char *)&ga, ptrm->term_no);
	if (ret < 0) {
		printk("recv ga failed, ret: %d\n", ret);
		if (ret == -1) {
			ptrm->status = -1;
		}
		if (ret == -2) {
			ptrm->status = -2;
		}
#ifdef TESTCODE
		goto next1;
#else
		return ret;
#endif
	}
#ifdef TESTCODE
next1:
#endif
	switch (ga) {
		case 0x88:
		case 0x99:
			ret = 0;
			break;
		case 0xAA:
		case 0xBB:
			usart_delay(4);
#ifdef TESTCODE
			return -1;
#endif
		default:
			usart_delay(4);
#ifdef TESTCODE
			return -1;
#endif
	}
	ptrm->add_verify += ga;
	ptrm->dif_verify ^= ga;
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
#ifdef TESTCODE
		goto next2;
#else
		return -1;
#endif
	}
#ifdef TESTCODE
next2:
#endif
	// 收校验
	ret = chk_verify(ptrm);
#ifdef TESTCODE
	//printk("term %d GA: %d, 0x%02x, %d\n", ptrm->term_no, recvno, ga, len);
#endif
	return ret;
}

/*
 * 发送上电初使化数据包, 请求时间, 黑名单版本号, 及参数配置信息
 * write by wjzhe, June 8 2007
 */
int purse_send_conf(term_ram *ptrm, unsigned char *tm, struct black_info *blkinfo)
{
	struct black_info termblk;
	int ret, i;
	unsigned char len;//, nouse[2];
	unsigned short psamno;
//	psam_local *psl = NULL;
	unsigned char *tmp = (unsigned char *)&termblk.edition;
	//unsigned char verify;
	memset(&termblk, 0, sizeof(termblk));
	// 收1字节包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_conf: recv len failed\n");
		return -1;
	}
	// 收4字节黑名单版本号
	for (i = 0; i < sizeof(termblk.edition); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d termblk.edition error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收9字节交易号
	ret = recv_trade_num(&termblk.trade_num, ptrm);
	if (ret < 0)
		return -1;
	// 收PSAM卡编号
	tmp = (unsigned char *)&psamno;
	for (i = 0; i < sizeof(psamno); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d PSAM NO. error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收2字节校验和
	ret = chk_verify(ptrm);
	if (ret < 0)
		return -1;
	// save terminal black info.
	memcpy(&ptrm->blkinfo, &termblk, sizeof(termblk));
	// 如果黑名单版本号不一致则将交易号清零, 20100307
	if (ptrm->blkinfo.edition != blkinfo->edition) {
		ptrm->blkinfo.trade_num.num = 0;
	}
#if 0
	// 此时要进行搜索PSAM卡库, 将对应的密码填入终端参数中
	psl = search_psam(psamno, psinfo);
	if (psl) {// 如果有正确的数据则保存终端PSAM卡密码
		memcpy(ptrm->pterm->param.psam_passwd, psl->pwd, sizeof(psl->pwd));
	}
#endif
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	// 清空校验
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发1字节终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x12
	ret = send_byte(PURSE_RET_PAR, ptrm->term_no);
	if (ret < 0) {
		goto send_failed;
	}
	ptrm->add_verify += PURSE_RET_PAR;
	ptrm->dif_verify ^= PURSE_RET_PAR;
	// 发1字节包长度
#ifndef QINGHUA
	len = 45 + 2 * ptrm->pterm->fee_n;
#else
	len = 45 + ptrm->pterm->fee_n;
#endif
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0)
		goto send_failed;
	// 发送配置数据45+2N (时间7＋配置参数包16＋餐次设置16＋新黑名单版本号4)
	ret = send_data2(tm, 7, ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&ptrm->pterm->param, sizeof(ptrm->pterm->param), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&ptrm->pterm->tmsg, sizeof(ptrm->pterm->tmsg), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&blkinfo->edition, sizeof(blkinfo->edition), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	// 2N: 管理费系数, N对应人员类型数
#ifndef QINGHUA
	for (i = 0; i < ptrm->pterm->fee_n; i++) {
		ret = send_data2(ptrm->pterm->mfee(i), 2, ptrm);
		if (ret < 0) {
			goto send_failed;
		}
	}
#else
	ret = send_data2((char *)ptrm->pterm->managefee, ptrm->pterm->fee_n, ptrm);
	if (ret < 0) {
		goto send_failed;
	}
#endif
	// 发校验和
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_failed;
#ifdef DEBUG
	printk("send %d\n", 7+sizeof(ptrm->pterm->param)+sizeof(ptrm->pterm->tmsg)+sizeof(blkinfo->edition)+2*ptrm->pterm->fee_n+2);
	printk("send conf recv all data\n");
#endif
	return 0;
send_failed:
	ptrm->status = NOCOM;
	return -1;
}

/*
 * 发送上电参数，不含管理费系数
 * write by duyy, 2012.1.31
 */
int purse_send_confnmf(term_ram *ptrm, unsigned char *tm, struct black_info *blkinfo)
{
	struct black_info termblk;
	int ret, i;
	unsigned char len;
	unsigned short psamno;
	unsigned char *tmp = (unsigned char *)&termblk.edition;
	//unsigned char verify;
	memset(&termblk, 0, sizeof(termblk));
	// 收1字节包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_confnmf: recv len failed\n");
		return -1;
	}
	// 收4字节黑名单版本号
	for (i = 0; i < sizeof(termblk.edition); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d termblk.edition error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收9字节交易号
	ret = recv_trade_num(&termblk.trade_num, ptrm);
	if (ret < 0)
		return -1;
	// 收PSAM卡编号
	tmp = (unsigned char *)&psamno;
	for (i = 0; i < sizeof(psamno); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d PSAM NO. error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收2字节校验和
	ret = chk_verify(ptrm);
	if (ret < 0)
		return -1;
	// save terminal black info.
	memcpy(&ptrm->blkinfo, &termblk, sizeof(termblk));
	// 如果黑名单版本号不一致则将交易号清零, 20100307
	if (ptrm->blkinfo.edition != blkinfo->edition) {
		ptrm->blkinfo.trade_num.num = 0;
	}
#if 0
	// 此时要进行搜索PSAM卡库, 将对应的密码填入终端参数中
	psl = search_psam(psamno, psinfo);
	if (psl) {// 如果有正确的数据则保存终端PSAM卡密码
		memcpy(ptrm->pterm->param.psam_passwd, psl->pwd, sizeof(psl->pwd));
	}
#endif
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	// 清空校验
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发1字节终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x62
	ret = send_byte(PURSE_RET_PARNMF, ptrm->term_no);
	if (ret < 0) {
		goto send_failed;
	}
	ptrm->add_verify += PURSE_RET_PARNMF;
	ptrm->dif_verify ^= PURSE_RET_PARNMF;
	// 发1字节包长度
	len = 45;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0)
		goto send_failed;
	// 发送配置数据45 (时间7＋配置参数包16＋餐次设置16＋新黑名单版本号4)
	ret = send_data2(tm, 7, ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&ptrm->pterm->param, sizeof(ptrm->pterm->param), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&ptrm->pterm->tmsg, sizeof(ptrm->pterm->tmsg), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	ret = send_data2((char *)&blkinfo->edition, sizeof(blkinfo->edition), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	// 发校验和
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_failed;
#ifdef DEBUG
	printk("send %d\n", 7+sizeof(ptrm->pterm->param)+sizeof(ptrm->pterm->tmsg)+sizeof(blkinfo->edition)+2);
	printk("send confnmf recv all data\n");
#endif
	return 0;
send_failed:
	ptrm->status = NOCOM;
	return -1;
}

/*
 * 发送管理费系数
 * write by duyy, 2012.1.31
 */
int purse_send_managefee(term_ram *ptrm)
{
	int ret;
	unsigned char len;
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_managefee: recv len failed\n");
		return -1;
	}
	// 收2字节校验和
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("purse send managefee chk error\n");
		return -1;
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x64
	ret = send_byte(PURSE_RET_MF, ptrm->term_no);
	if (ret < 0) {
		goto send_failed;
	}
	ptrm->add_verify += PURSE_RET_MF;
	ptrm->dif_verify ^= PURSE_RET_MF;
	// 发送包长度
#ifndef QINGHUA
	len = 2 * ptrm->pterm->fee_n + 2;
#else
	len = ptrm->pterm->fee_n + 2;
#endif
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		goto send_failed;
	}
	// 2N: 管理费系数, N对应人员类型数
#ifndef QINGHUA
	for (i = 0; i < ptrm->pterm->fee_n; i++) {
		ret = send_data2(ptrm->pterm->mfee(i), 2, ptrm);
		if (ret < 0) {
			goto send_failed;
		}
	}
#else
	ret = send_data2((char *)ptrm->pterm->managefee, ptrm->pterm->fee_n, ptrm);
	if (ret < 0) {
		goto send_failed;
	}
#endif
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_failed;
	return 0;
send_failed:
	ptrm->status = NOCOM;
	return -1;
}
/*
 * 发送当前时间(YY, YY, MM, DD, HH, MM, SS)
 * write by wjzhe, June 8 2007
 */
int purse_send_time(term_ram *ptrm, unsigned char *tm)
{
	int ret;
	unsigned char len;
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// 收2字节校验和
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("purse send time chk error\n");
		return -1;
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x16
	ret = send_byte(PURSE_RET_TIME, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += PURSE_RET_TIME;
	ptrm->dif_verify ^= PURSE_RET_TIME;
	// 发送包长度
	len = 9;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发当前时间7字节
	ret = send_data2(tm, 7, ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_time_failed;
	return 0;
send_time_failed:
	ptrm->status = NOCOM;
	return -1;
}
/*
 * 发送禁止消费时间(HH, MM)
 * write by duyy, 2013.5.20
 */
int purse_send_noconsumetime(term_ram *ptrm, unsigned char *nocmtm)
{
	int ret, i, j;
	unsigned char len;
	unsigned char nctime[28] = {0};
	unsigned char timenum = 0;
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// 收2字节校验和
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("purse send time chk error\n");
		return -1;
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	if (!(ptrm->pterm->param.term_type & 0x20)) {	//出纳机不判断时段、身份类型匹配，禁止消费问题,modified 2013.4.2
		for(i = 0; i < 16; i++){//16类身份对应的禁止消费时段判断
			timenum = ptmnum[i][ptrm->pterm->power_id];
			if (timenum & 0x80){//modified by duyy, 2013.5.20,最高位bit7置1表示禁止消费，其余为允许消费，判断禁止时段
				memcpy(nctime, nocmtm, 28);
				goto over;
			}
			else {
				for (j = 0; j < 7; j++){
					if (timenum & 0x1){
						memcpy(nctime, nocmtm, 28);
						goto over;
					}
					timenum = timenum >> 1;
				}
			}
		}
		for(i = 0; i < 64; i++){//64类消费卡对应的禁止消费时段判断
			timenum = forbidflag[ptrm->pterm->power_id][i];
			if (timenum & 0x80){//modified by duyy, 2013.5.20,最高位bit7置1表示禁止消费，其余为允许消费，判断禁止时段
				memcpy(nctime, nocmtm, 28);
				goto over;
			}
			else {
				for (j = 0; j < 7; j++){
					if (timenum & 0x1){
						memcpy(nctime, nocmtm, 28);
						goto over;
					}
					timenum = timenum >> 1;
				}
			}
		}
	
	}
over:
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x16
	ret = send_byte(PURSE_RET_NCTM, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += PURSE_RET_NCTM;
	ptrm->dif_verify ^= PURSE_RET_NCTM;
	// 发送包长度
	len = 30;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发禁止消费时间28字节
	ret = send_data2(nctime, 28, ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_time_failed;
	return 0;
send_time_failed:
	ptrm->status = NOCOM;
	return -1;
}
/*
 * 发送消费卡管理费信息
 * write by duyy, 2013.11.19
 */
int purse_send_magfeetable(term_ram *ptrm)
{
	int ret, i;
	unsigned char len;
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// 收2字节校验和
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("purse send time chk error\n");
		return -1;
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x1D
	ret = send_byte(PURSE_RET_MAGFEE, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += PURSE_RET_MAGFEE;
	ptrm->dif_verify ^= PURSE_RET_MAGFEE;
	// 发送包长度
	len = 67;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发管理费收取方式1字节
	ret = send_data2(&feenum, 1, ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发消费卡管理费64字节
	for(i = 0; i < 64; i++){
		ret = send_data2(&feetable[ptrm->pterm->power_id][i], 1, ptrm);
		if (ret < 0) {
			goto send_time_failed;
		}
	}
	
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_time_failed;
	return 0;
send_time_failed:
	ptrm->status = NOCOM;
	return -1;
}
/*
 * 发送禁止消费时段启用标志表单信息
 * write by duyy, 2013.11.19
 */
int purse_send_forbidflag(term_ram *ptrm)
{
	int ret, i;
	unsigned char len;
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// 收2字节校验和
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("purse send time chk error\n");
		return -1;
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x1F
	ret = send_byte(PURSE_RET_FORBIDTABLE, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += PURSE_RET_FORBIDTABLE;
	ptrm->dif_verify ^= PURSE_RET_FORBIDTABLE;
	// 发送包长度
	len = 66;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发禁止消费时段启用标志表单信息64字节
	for(i = 0; i < 64; i++){
		ret = send_data2(&forbidflag[ptrm->pterm->power_id][i], 1, ptrm);
		if (ret < 0) {
			goto send_time_failed;
		}
	}
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_time_failed;
	return 0;
send_time_failed:
	ptrm->status = NOCOM;
	return -1;
}
/*
 * 更新消费卡管理费信息
 * write by duyy, 2013.11.19
 */
int purse_update_magfeetable(term_ram *ptrm)
{
	int ret, i;
	unsigned char len;
	unsigned char sendno, recvno;
	
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 叫号
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto send_time_failed;
	com_verify(ptrm, &sendno, sizeof(sendno));
	// 回号
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	// 判断机器号是否一致
	com_verify(ptrm, &recvno, sizeof(recvno));
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("update key return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}
	//printk("termno %d send managefee\n", ptrm->term_no);
	// 发终端号
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		printk("send termno failed\n");
		goto send_time_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x3B
	ret = send_byte(PURSE_UPDATE_MAGFEE, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += PURSE_UPDATE_MAGFEE;
	ptrm->dif_verify ^= PURSE_UPDATE_MAGFEE;
	// 发送包长度
	len = 67;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		printk("send data len failed\n");
		goto send_time_failed;
	}
	// 发管理费收取方式1字节
	ret = send_data2(&feenum, 1, ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发消费卡管理费64字节
	for(i = 0; i < 64; i++){
		ret = send_data2(&feetable[ptrm->pterm->power_id][i], 1, ptrm);
		if (ret < 0) {
			printk("send data failed\n");
			goto send_time_failed;
		}
	}
	
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_time_failed;
	// 收GA
	ret = recv_ga(ptrm);
	if (ret < 0) {
		printk("purse %d update managefee GA error\n", ptrm->term_no);
		return -1;
	}
#ifdef TESTCODE
	else {
		printk("update managefee: recv ga ok\n");
	}
#endif
	return 0;
send_time_failed:
	ptrm->status = NOCOM;
	return -1;
}
/*
 * 更新禁止消费时段启用标志表单信息
 * write by duyy, 2013.11.19
 */
int purse_update_forbidflag(term_ram *ptrm)
{
	int ret, i;
	unsigned char len;
	unsigned char sendno, recvno;
	
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 叫号
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto send_time_failed;
	com_verify(ptrm, &sendno, sizeof(sendno));
	// 回号
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {

		ptrm->status = ret;
		return ret;
	}
	// 判断机器号是否一致
	com_verify(ptrm, &recvno, sizeof(recvno));
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("update key return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}

	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x1F
	ret = send_byte(PURSE_UPDATE_FORBIDTABLE, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += PURSE_UPDATE_FORBIDTABLE;
	ptrm->dif_verify ^= PURSE_UPDATE_FORBIDTABLE;
	// 发送包长度
	len = 66;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发禁止消费时段启用标志表单信息64字节
	for(i = 0; i < 64; i++){
		ret = send_data2(&forbidflag[ptrm->pterm->power_id][i], 1, ptrm);
		if (ret < 0) {
			goto send_time_failed;
		}
	}
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_time_failed;
	// 收GA
	ret = recv_ga(ptrm);
	if (ret < 0) {
		printk("purse %d update forbidflag GA error\n", ptrm->term_no);
		return -1;
	}
#ifdef TESTCODE
	else {
		printk("update forbidflag: recv ga ok\n");
	}
#endif
	return 0;
send_time_failed:
	ptrm->status = NOCOM;
	return -1;
}
/*
 * 更新禁止消费时间(HH, MM)
 * write by duyy, 2013.11.22
 */
int purse_update_noconsumetime(term_ram *ptrm, unsigned char *nocmtm)
{
	int ret, i, j;
	unsigned char len;
	unsigned char nctime[28] = {0};
	unsigned char timenum = 0;
	unsigned char sendno, recvno;
	
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 叫号
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto send_time_failed;
	com_verify(ptrm, &sendno, sizeof(sendno));
	// 回号
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {

		ptrm->status = ret;
		return ret;
	}
	// 判断机器号是否一致
	com_verify(ptrm, &recvno, sizeof(recvno));
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("update time seg term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}
	if (!(ptrm->pterm->param.term_type & 0x20)) {	//出纳机不判断时段、身份类型匹配，禁止消费问题,modified 2013.4.2
		for(i = 0; i < 16; i++){//16类身份对应的禁止消费时段判断
			timenum = ptmnum[i][ptrm->pterm->power_id];
			if (timenum & 0x80){//modified by duyy, 2013.5.20,最高位bit7置1表示禁止消费，其余为允许消费，判断禁止时段
				memcpy(nctime, nocmtm, 28);
				goto over;
			}
			else {
				for (j = 0; j < 7; j++){
					if (timenum & 0x1){
						memcpy(nctime, nocmtm, 28);
						goto over;
					}
					timenum = timenum >> 1;
				}
			}
		}
		for(i = 0; i < 64; i++){//64类消费卡对应的禁止消费时段判断
			timenum = forbidflag[ptrm->pterm->power_id][i];
			if (timenum & 0x80){//modified by duyy, 2013.5.20,最高位bit7置1表示禁止消费，其余为允许消费，判断禁止时段
				memcpy(nctime, nocmtm, 28);
				goto over;
			}
			else {
				for (j = 0; j < 7; j++){
					if (timenum & 0x1){
						memcpy(nctime, nocmtm, 28);
						goto over;
					}
					timenum = timenum >> 1;
				}
			}
		}
	
	}
over:
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发命令字0x3D
	ret = send_byte(PURSE_UPDATE_FORBIDSEG, ptrm->term_no);
	if (ret < 0) {
		goto send_time_failed;
	}
	ptrm->add_verify += PURSE_UPDATE_FORBIDSEG;
	ptrm->dif_verify ^= PURSE_UPDATE_FORBIDSEG;
	// 发送包长度
	len = 30;
	ret = send_data2((char *)&len, sizeof(len), ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发禁止消费时间28字节
	ret = send_data2(nctime, 28, ptrm);
	if (ret < 0) {
		goto send_time_failed;
	}
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_time_failed;
	// 收GA
	ret = recv_ga(ptrm);
	if (ret < 0) {
		printk("purse %d update forbidseg GA error\n", ptrm->term_no);
		return -1;
	}
//#ifdef TESTCODE
	else {
		printk("update forbidseg: recv ga ok\n");
	}
//#endif
	return 0;
send_time_failed:
	ptrm->status = NOCOM;
	return -1;
}
/*
 * 接收终端机黑名单交易号, 返回通用应答包GA
 * 黑名单交易序列号YYMDHXXXX：YYMDH为年、月、日、时
 * XXXX为递增式序列号，会一直进行递增，更新黑名单版本后也不清零
 * write by wjzhe, June 8 2007
 */
int purse_recv_btno(term_ram *ptrm)
{
	struct tradenum t_num;
	unsigned char len;
	int ret;
	memset(&t_num, 0, sizeof(t_num));
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// 收9字节黑名单交易序列号
	ret = recv_trade_num(&t_num, ptrm);
	if (ret < 0) {
		printk("purse recv btno: recv trade num failed\n");
		return -1;
	}
	// 收2字节校验和
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("purse recv btno: chk verify failed\n");
		if (ret != ERRVERIFY) {
			usart_delay(2);
		}
		// 发GA错误包
		ret = send_ga(ptrm, ret);
		if (ret < 0) {
			goto recv_btno_failed;
		}
		return ret;
	}
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	// 发GA包
	ret = send_ga(ptrm, 0);
	if (ret < 0) {
		goto recv_btno_failed;
	}
	// 在顺利发完GA之后, 记录终端的交易号
	memcpy(&ptrm->blkinfo.trade_num, &t_num, sizeof(t_num));
	return 0;
recv_btno_failed:
	ptrm->status = NOCOM;
	return -1;
}

/*
 * 接收终端机未上传流水
 * write by wjzhe, June 8 2007
 */
int purse_recv_flow(term_ram *ptrm)
{
	int ret, i, tail;
	m1_flow m1flow;
	unsigned char *tmp;
	unsigned char nouse[4], len;
	memset(&m1flow, 0, sizeof(m1flow));
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF) {
		printk("purse_send_time: recv len failed\n");
		return -1;
	}
	// 接收48字节流水信息
	// 流水时间
#ifdef QINGHUA
	m1flow.date.hyear = 0x20;
	tmp = (unsigned char *)&m1flow.date.lyear;
	for (i = 0; i < sizeof(m1flow.date) - 1; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
#else
	tmp = (unsigned char *)&m1flow.date.hyear;
	for (i = 0; i < sizeof(m1flow.date); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
#endif
	// 卡号
	tmp = (unsigned char *)&m1flow.card_num;
	for (i = 0; i < sizeof(m1flow.card_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.card_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 账号
	tmp = (unsigned char *)&m1flow.acc_num;
	for (i = 0; i < sizeof(m1flow.acc_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.acc_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#ifndef QINGHUA
	// 当前餐次消费总额
	tmp = (unsigned char *)&m1flow.money_sum;
	for (i = 0; i < sizeof(m1flow.money_sum); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.money_sum error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 2字节待扩展
	for (i = 0; i < 2; i++) {
		ret = recv_data(&nouse[i], ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += nouse[i];
		ptrm->dif_verify ^= nouse[i];
	}
	// 管理费差额
	tmp = (unsigned char *)&m1flow.manage_fee;
	for (i = 0; i < 3; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.manage_fee error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#else
	// ASN
	tmp = (unsigned char *)&m1flow.asn;
	for (i = 0; i < sizeof(m1flow.asn); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.acc_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#if 0
	// 当前餐次消费总额, 清华只有3字节
	tmp = (unsigned char *)&m1flow.money_sum;
	for (i = 0; i < 3; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.money_sum error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#endif
	// 4字节TAC
	tmp = (unsigned char *)&m1flow.qh_tac;
	for (i = 0; i < sizeof(m1flow.qh_tac); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.qh_tac error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 管理费差额, 清华只有2字节
	tmp = (unsigned char *)&m1flow.manage_fee;
	for (i = 0; i < 2; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.manage_fee error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#endif
	// 结算机被设置编号
	tmp = (unsigned char *)&m1flow.tml_num;
	for (i = 0; i < sizeof(m1flow.tml_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.tml_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 结算机被设置区号
	tmp = (unsigned char *)&m1flow.areano;
	for (i = 0; i < sizeof(m1flow.areano); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.tml_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 结算机中PSAM卡编号
	tmp = (unsigned char *)&m1flow.psam_num;
	for (i = 0; i < sizeof(m1flow.psam_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.psam_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 结算机中PSAM卡交易序号
	tmp = (unsigned char *)&m1flow.psam_trd_num;
	for (i = 0; i < sizeof(m1flow.psam_trd_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.psam_trd_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 钱包交易序号
	tmp = (unsigned char *)&m1flow.walt_num;
	for (i = 0; i < sizeof(m1flow.walt_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.walt_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 交易金额
	tmp = (unsigned char *)&m1flow.consume_sum;
	for (i = 0; i < sizeof(m1flow.consume_sum); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.consume_sum error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 钱包交易后余额
	tmp = (unsigned char *)&m1flow.remain_money;
	for (i = 0; i < sizeof(m1flow.remain_money); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.remain_money error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 菜品分组编号
	tmp = (unsigned char *)&m1flow.food_grp_num;
	for (i = 0; i < sizeof(m1flow.food_grp_num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.food_grp_num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 交易类型标识
#ifndef QINGHUA
	tmp = (unsigned char *)&m1flow.deal_type;
	for (i = 0; i < sizeof(m1flow.deal_type); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.deal_type error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#endif
	// 流水类型
	tmp = (unsigned char *)&m1flow.flow_type;
	for (i = 0; i < sizeof(m1flow.flow_type); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.flow_type error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// tac code
#ifndef QINGHUA
	tmp = (unsigned char *)&m1flow.tac_code;
#else
	tmp = (unsigned char *)nouse;
#endif
	//for (i = 0; i < sizeof(m1flow.tac_code); i++) {
	for (i = 0; i < 2; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d m1flow.tac_code error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 是否上传标志
	for (i = 0; i < 1; i++) {
		ret = recv_data(&nouse[2], ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= nouse[2];
		ptrm->add_verify += nouse[2];
	}
	// 接收2字节校验
	if ((ret = chk_verify(ptrm)) < 0) {
		if (ret != ERRVERIFY) {
			usart_delay(2);
		} else 
		// 仅仅在校验错误的情况下返回GA
		send_ga(ptrm, ret);
		return -1;
	}
	// 进行流水存储前判断是否是未上传流水
	if (nouse[2] == 0xFE) {
#if 0
		printk("flow no.: %d; tradenum: %d\n", maxflowno + 1, m1flow.psam_trd_num);
#endif
		// 发送通用应答包(地址, 内容, 校验)
		if (send_ga(ptrm, 0) < 0) {// 此情况很难出现, 除非UART硬件有错误
			ptrm->status = NOTERMINAL;
			return -1;
		}
		return 0;
	}
	// 非锁卡流水并且为正常流水才有此判断
#ifndef QINGHUA
	if ((ptrm->psam_trd_num) && 
#ifdef QINGHUA
		!(m1flow.flow_type & TPLOCKBIT)		// 非锁卡流水
#else
		(m1flow.deal_type != 0x99)
#endif
		&& !(m1flow.flow_type & (1 << 1))) {
		if (m1flow.psam_trd_num <= ptrm->psam_trd_num) {
			// 已经上传的流水
			// 发送通用应答包(地址, 内容, 校验)
			if (send_ga(ptrm, 0) < 0) {// 此情况很难出现, 除非UART硬件有错误
				ptrm->status = NOTERMINAL;
				return -1;
			}
			return 0;
		}
	}
#endif
#if BWT_66J10
	udelay(BWT_66J10);
#endif
	//if (!(m1flow.flow_type & (1 << 1)))
	//ptrm->flow_flag = ptrm->term_no;
	// term_money plus
	// 此处没进行TAC码验证, 只是简单的判断是否是正确流水
	if (!(m1flow.flow_type & (1 << 1))) {
#ifdef QINGHUA
		if (m1flow.flow_type & TPLOCKBIT) {
#else
		if (m1flow.deal_type == 0x99) {
#endif
			// 锁卡流水, 不处理终端金额
		} else {
			// 只有正常流水且非锁卡流水才记录终端交易序号
			ptrm->psam_trd_num = m1flow.psam_trd_num;
			ptrm->term_money += m1flow.consume_sum;
			ptrm->term_cnt++;
		}
	}
	// 流水号的处理
	m1flow.flow_num = maxflowno++;
	// 管理费的处理
	{
		int posneg = 0;
		if (sizeof(m1flow.manage_fee) == sizeof(int)) {
			posneg = 0x80000000;
		} else if (sizeof(m1flow.manage_fee) == sizeof(short)) {
			posneg = 0x8000;
		}
		if (m1flow.manage_fee & posneg) {
			// 这是负的管理费, 需要特殊处理
			m1flow.manage_fee &= ~(posneg);
			m1flow.manage_fee *= -1;
		}
	}
	//printk("recv cpu flow tail: %d\n", tail);
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		printk("uart recv flow no space\n");
		// 发送通用应答包(地址, 内容, 校验)
		if (send_ga(ptrm, -1) < 0) {// 此情况很难出现, 除非UART硬件有错误
			ptrm->status = NOTERMINAL;
			return -1;
		}
		return -1;
	}
	memcpy(&pflow[tail], &m1flow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	// 发送通用应答包(地址, 内容, 校验)
	if (send_ga(ptrm, 0) < 0) {// 此情况很难出现, 除非UART硬件有错误
		ptrm->status = NOTERMINAL;
		return -1;
	}
	return 0;
}

/*
 * 实时更新终端机全部黑名单数据
 * 本交互由上位机发起，主动下传黑名单数据，终端接收后将存入FLASH中；
 * 上位机发送本命令包时要保证黑名单交易号和终端当前的交易号连续，
 * 否则终端将不存储该数据包内数据；
 * 终端返回通用应答包在数据写入FLASH之后；
 * 和定期更新黑名单数据的异同：相同点是本命令的主动方都是上位机，
 * 传送数据格式也相同，但不同点是交互流程，定期更新交互是点对点循环交互的，
 * 而本命令只有一次交互，上位机完成本交互后可以马上转到其他终端上，
 * 下一次传送数据时再次完整执行本交互流程。
 * 入口: 当前终端属性指针, 系统黑名单信息, 黑名单表头指针
 * 返回0正确, <0错误
 * write by wjzhe, June 8 2007
 */
//#define DEBUG
int purse_send_bkin(term_ram *ptrm, struct black_info *blkinfo, black_acc *blkacc, int flag)
{
#ifndef MAXBLKCNT
#define MAXBLKCNT 6 //10,modified by duyy,2012.2.8	// max 20
#endif
	int ret;
	unsigned char n, len = 0;
	unsigned char recvno, sendno;
	black_acc *phead;
	if (blkacc == NULL)
		return 0;
	// 获取要下载黑名单的条数, 只是获得简单的条数
	ret = compute_blk(ptrm, blkinfo);
	if (ret == -E485COM) {
#ifdef DEBUG
		printk("compute blk error\n");
#endif
		return -1;
	} else if (ptrm->blkinfo.edition != blkinfo->edition) {
		// 版本号不对, 要进行黑名单更新
#ifdef DEBUG
		printk("edition error send all, POS: %d, SRV: %d\n",
			ptrm->blkinfo.edition, blkinfo->edition);
#endif
		if (ret == 0) {
			// 压根没有数据??
		}
	} else if ((ret == 0) && (flag)) {
#ifdef DEBUG
		//printk("no new black\n");
#endif
		return 0;
	}
	//if (ptrm->blkinfo.trade_num.num == 0) {
		// 此时终端机中没有任何黑名单
	//	ret -= blkacc[0].t_num.num - 1;
	//}
	if (ret < 0) {
		// 并不需要下载黑名单, 终端机的交易号比这边还大
#ifdef DEBUG
		printk("less than 0 not need black\n");
#endif
		phead = NULL;
		ret = 0;
	} else if ((ptrm->blkinfo.trade_num.num > 0)
		// 终端机的版本号一致且交易号>0才会查找黑名单表
		/*&& (ptrm->blkinfo.edition == blkinfo->edition)*/) {
		phead = search_blk2(ptrm->blkinfo.trade_num.num, blkacc, blkinfo->count);
		if (phead == NULL) {
			// 此交易号不在黑名单列表中, 无法进行黑名单更新
#ifdef DEBUG
			printk("trade number is not in black list\n");
			out_tnum(&ptrm->blkinfo.trade_num);
#endif
			return -1;
		}
		// 当前得到的黑名单已经是下载后的, 需要+1
		phead += 1;
	} else if (blkinfo->count > 0) {
#ifdef DEBUG
		printk("phead = blkacc\n");
#endif
		phead = blkacc;
		ret = blkacc[blkinfo->count - 1].t_num.num - blkacc[0].t_num.num + 1;
	} else {
		phead = NULL;
		ret = 0;
	}
	// 这一次需要下发n条黑名单数据
	if (ret <= MAXBLKCNT) {
		n = ret;
		n |= 3 << 6;
		len = 4;
	} else {
		n = MAXBLKCNT;
		len = 0;
	}
#ifdef DEBUG
	if (phead) {
		printk("send : phead->no %d\n", phead->card_num);
	//	printk("send : phead->opt %02x\n", phead->opt);
	}
	printk("send %d black acc, ret %d, blkinfo->count %d, \nblkinfo->trade_num.num %d\n", n & 0x3F, ret, blkinfo->count, blkinfo->trade_num.num);
	if (n & 0x3F)
		printk("send bkin: cnt = 0x%02x, trade = %d\n", n, phead[(n & 0x3F) - 1].t_num.num);
#endif
	// 叫号
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0) {
		goto send_bkin_failed;
	}
	// 回号
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
#ifdef TESTCODE
		printk("recv data %d: ret -> %d\n", ptrm->term_no, ret);
#endif
		ptrm->status = ret;
		return ret;
	}
	// 判断机器号是否一致
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
#ifdef TESTCODE
		printk("send bkin return term no. error %02x: %02x\n", recvno, ptrm->term_no);
#endif
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
#if BWR_66J10
	//udelay(10);
#endif
	// 发1字节终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto send_bkin_failed;
	}
	com_verify(ptrm, &ptrm->term_no, sizeof(ptrm->term_no));
	// 发命令字
	ret = send_byte(PURSE_PUT_BLK, ptrm->term_no);
	if (ret < 0) {
		goto send_bkin_failed;
	}
	ptrm->add_verify += PURSE_PUT_BLK;
	ptrm->dif_verify ^= PURSE_PUT_BLK;
	// 发包长度
	len += 12 + 5 * (n & 0x3F);
	ret = send_data2(&len, sizeof(len), ptrm);
	if (ret < 0) {
#ifdef DEBUG
		printk("sendlen failed\n");
#endif
		return -1;
	}
	// 发1字节包头
	// bit5-bit0本包数据包含黑名单数量
	// bit7, bit6 == 0x3黑名单全部发送完毕
	ret = send_byte(n, ptrm->term_no);
	if (ret < 0)
		goto send_bkin_failed;
	com_verify(ptrm, &n, sizeof(n));
	// 发9字节交易号
	if (phead == NULL || ((n & 0x3F) == 0)) {
		// BUG, n == 0, fixed by wjzhe
		// 对phead进行判断!!!
		ret = send_trade_num(&blkinfo->trade_num, ptrm);
		if (ret < 0)
			goto send_bkin_failed;
	} else {
		// BUG, 如果n == 0则segment fault
		ret = send_trade_num(&phead[(n & 0x3F) - 1].t_num, ptrm);
		if (ret < 0)
			goto send_bkin_failed;
	}
/*	ret =  send_data2((char *)&phead[(n & 0x3F) - 1].t_num.num, sizeof(phead[(n & 0x3F) - 1].t_num.num), ptrm);
	if (ret < 0)
		goto send_bkin_failed;
	ret = send_data2((char *)phead[(n & 0x3F) - 1].t_num.tm, sizeof(phead[(n & 0x3F) - 1].t_num.tm), ptrm);
	if (ret < 0)
		goto send_bkin_failed;*/
	// 发5 * N字节黑名单数据
	//n &= 0x3F;
	if ((ret = send_blkacc(ptrm, phead, n & 0x3F)) < 0)
		return -1;
	// 发黑名单版本号
	if (n & 0xC0) {
		ret = send_data2((char *)&blkinfo->edition, sizeof(blkinfo->edition), ptrm);
		if (ret < 0)
			goto send_bkin_failed;
	}
	// 发2字节校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_bkin_failed;
#if BWR_66J10
	udelay(200);
#endif
	// 收GA
	if ((ret = recv_ga(ptrm)) < 0) {
		printk("send bkin recv GA error: %d\n", ret);
		return -1;
	}
#ifdef TESTCODE
	else {
		printk("send bkin: recv ga ok\n");
	}
#endif
	if ((n & 0xC0) && (!ptrm->black_flag)) {
		ptrm->black_flag = 1;
#ifdef DEBUG
		printk("send bkin set black flag 1\n");
#endif
	} else {
#ifdef DEBUG
		printk("send bkin not set black flag 1, n %d, flag %d\n", n, ptrm->black_flag);
#endif
	}
	// 做处理
	if (phead)
		memcpy(&ptrm->blkinfo.trade_num, &phead[n - 1].t_num, sizeof(phead[n - 1].t_num));
	return 0;
send_bkin_failed:
#ifdef DEBUG
	printk("send_bkin_failed\n");
#endif
	ptrm->status = NOCOM;
	return -1;
}

#if 0
/*
 * 定期更新终端机全部黑名单数据, 未完
 * write by wjzhe, June 8 2007
 */
int purse_send_bkta(term_ram *ptrm, struct black_info *blkinfo)
{
#ifndef MAXBLKCNT
#define MAXBLKCNT 20
#endif
	unsigned char recvno, cmd, *tmp;
	int ret, i;
	// 发1字节地址
	ret = send_data2((char *)&ptrm->term_no, 1, ptrm);
	if (ret < 0)
		goto send_bkta_failed;
	// 发0x32, 终端进行FLASH擦除
	ret = send_byte(PURSE_PUT_ABLK, ptrm->term_no);
	if (ret < 0)
		goto send_bkta_failed;
	ptrm->add_verify += ptrm->term_no;
	ptrm->dif_verify ^= ptrm->term_no;
	// 发校验
	ret = send_verify(ptrm);
	if (ret < 0)
		goto send_bkta_failed;
	// 收应答信息包
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// term number
	ret = recv_data(&recvno, ptrm->term_no);
	if (ret < 0)
		goto bkta_recv_failed;
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("send bkta return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify += recvno;
	ptrm->dif_verify ^= recvno;
	// command
	ret = recv_data(&cmd, ptrm->term_no);
	if (ret < 0)
		goto bkta_recv_failed;
	if (cmd != PURSE_RET_ABLK) {
		printk("ret cmd error, %02x, 33\n", cmd);
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->add_verify += cmd;
	ptrm->dif_verify ^= cmd;
	tmp = (unsigned char *)&ptrm->blkinfo.trade_num.num;
	for (i = 0; i < sizeof(ptrm->blkinfo.trade_num.num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d ptrm->blkinfo.trade_num.num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	tmp = ptrm->blkinfo.trade_num.tm;
	for (i = 0; i < sizeof(ptrm->blkinfo.trade_num.tm); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d ptrm->blkinfo.trade_num.tm error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	ret = compute_blk(ptrm, blkinfo);
	if (ret < 0) {
		return -1;
	} else if (ret == 0) {
		return 0;
	}
// 循环做以下操作
	// 发1字节包头,
	// bit5-bit0本包数据包含黑名单数量
	// bit7, bit6 == 0x3黑名单全部发送完毕
	// 发9字节交易号
	// 发5 * N字节黑名单数据
	// 发2字节校验
	// 收GA

	return 0;
send_bkta_failed:
	ptrm->status = NOCOM;
	return -1;
bkta_recv_failed:
	ptrm->status = ret;
	return ret;
}
#endif

/*
 * 广播方式更新终端机全部黑名单数据, 此过程只发数据
 * write by wjzhe, June 11 2007
 */
int purse_broadcast_blk(black_acc *pbacc, struct black_info *pbinfo, term_ram *ptrm, int cnt)
{
#ifndef MAXBLKCNT
#define MAXBLKCNT 20
#endif
#ifndef ADDTIMENUM
#define ADDTIMENUM 50000
#endif
#define FC 0
#define LC 0xFF
#define BC 0xFF
	//unsigned char vadd = 0, vdif = 0, *tmp;
	int once, left = pbinfo->count, ret, i;
	unsigned char len = 0;
	term_ram term_tmp;// 此数据为虚拟数据, 不能引用其终端属性
	if (pbinfo->count <= 0)
		return 0;
	memset(&term_tmp, 0, sizeof(term_tmp));
	// 发送擦除命令
	// 给前四通道所有机器进行广播
	term_tmp.term_no = FC;
	ret = send_addr(BC, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += BC;
	term_tmp.dif_verify ^= BC;
	// 发命令
	ret = send_byte(PURSE_BCT_ERASE, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += PURSE_BCT_ERASE;
	term_tmp.dif_verify ^= PURSE_BCT_ERASE;
	// 发包长度
	ret = send_byte(11, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += 11;
	term_tmp.dif_verify ^= 11;
	// 发交易号, 新的起始交易号
	ret = send_trade_num(&pbacc->t_num, &term_tmp);
	if (ret < 0)
		return -1;
	// 发校验
	ret = send_verify(&term_tmp);
	if (ret < 0) {
		return -1;
	}
	// 给后四通道所有机器发命令
	memset(&term_tmp, 0, sizeof(term_tmp));
	term_tmp.term_no = LC;
	ret = send_addr(BC, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += BC;
	term_tmp.dif_verify ^= BC;
	// 发命令
	ret = send_byte(PURSE_BCT_ERASE, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += PURSE_BCT_ERASE;
	term_tmp.dif_verify ^= PURSE_BCT_ERASE;
	// 发包长度
	ret = send_byte(11, term_tmp.term_no);
	if (ret < 0)
		return -1;
	term_tmp.add_verify += 11;
	term_tmp.dif_verify ^= 11;
	// 发交易号
	ret = send_trade_num(&pbacc->t_num, &term_tmp);
	if (ret < 0)
		return -1;
	// 发校验
	ret = send_verify(&term_tmp);
	if (ret < 0) {
		return -1;
	}
	// 此处需要等待一段时间
	mdelay(3000);
	// 遍历所有终端交易号, 查看是否为新的起始交易号-1, 不是则退出
	for (i = 0; i < cnt; i++) {
		memcpy(&term_tmp, ptrm, sizeof(term_tmp));
		sel_ch(term_tmp.term_no);
		udelay(5);
		if (purse_get_edition(&term_tmp) < 0) {
			printk("broad cast get terminal edition failed: %d, %d\n", i, term_tmp.term_no);
			ptrm++;
			continue;
		}
		if ((term_tmp.blkinfo.trade_num.num != (pbacc->t_num.num - 1))
			|| term_tmp.blkinfo.edition) {
			printk("erase term %d failed\n", term_tmp.term_no);
			printk("term_tmp.blkinfo.trade_num.num: %d, pbinfo->trade_num.num: %d\n",
				term_tmp.blkinfo.trade_num.num, pbinfo->trade_num.num);
			return -1;
		}
		ptrm++;
	}
/*
1: bit5-bit0本包数据包含的黑名单数量，bit7bit6=11b,黑名单全部发完；
9：BYTE8-BYTE0:更新本包数据后新的交易号；
5N:5字节/黑名单，最高字节标识解挂挂失；
(4)：（可能会有）黑名单更新结束时有版本号
*/
	while (left) {
		unsigned char n = 0;
		unsigned char cmd = 0;
		memset(&term_tmp, 0, sizeof(term_tmp));
		len = 0;
		// 剩余left笔数据, 最大一次20
		if (left > MAXBLKCNT) {
			once = MAXBLKCNT;
			n = once;
		} else {
			once = left;
			n = once;
			n |= 0xC0;
			len = 4;
		}
		// 前四通道
		// 发1字节地址信息0xFF
		term_tmp.term_no = FC;
		cmd = BC;
		ret = send_addr(cmd, term_tmp.term_no);
		if (ret < 0)
			return -1;
		com_verify(&term_tmp, &cmd, 1);
		// 发0x37
		ret = send_byte(PURSE_BCT_BLK, term_tmp.term_no);
		if (ret < 0)
			return -1;
		term_tmp.add_verify += PURSE_BCT_BLK;
		term_tmp.dif_verify ^= PURSE_BCT_BLK;
		// 发包长度
		len += 12 + 5 * (n & 0x3F);
		//len += 12 + 5 * (n & 0x3F) + ((n & 0xC0) ? 4 : 0);
		ret = send_data2(&len, sizeof(len), &term_tmp);
		if (ret < 0)
			return -1;
		// 循环发1+9+5N+(4)+2(N <= 10)
		ret = send_byte(n, term_tmp.term_no);
		if (ret < 0)
			return -1;
		com_verify(&term_tmp, &n, 1);
		// 发9字节交易号
		ret = send_trade_num(&pbacc[once - 1].t_num, &term_tmp);
		if (ret < 0)
			return -1;
		// 发5 * N字节黑名单数据
		if ((ret = send_blkacc(&term_tmp, pbacc, once)) < 0)
			return -1;
		if (n & 0xC0) {// 说明是最后一包数据
			// 发四字节版本号
			ret = send_data2((char *)&pbinfo->edition, sizeof(pbinfo->edition), &term_tmp);
			if (ret < 0)
				return -1;
		}
		// 发校验
		ret = send_verify(&term_tmp);
		if (ret < 0)
			return -1;
		// 继续进行后四通道
		// 发1字节地址信息0xFF
		memset(&term_tmp, 0, sizeof(term_tmp));
		term_tmp.term_no = LC;
		//cmd = BC;
		ret = send_addr(cmd, LC);
		if (ret < 0)
			return -1;
		com_verify(&term_tmp, &cmd, 1);
		// 发0x37
		ret = send_byte(PURSE_BCT_BLK, term_tmp.term_no);
		if (ret < 0)
			return -1;
		term_tmp.add_verify += PURSE_BCT_BLK;
		term_tmp.dif_verify ^= PURSE_BCT_BLK;
		// 发包长度
		ret = send_data2(&len, sizeof(len), &term_tmp);
		if (ret < 0)
			return -1;
		// 循环发1+9+5N+(4)+2(N <= 10)
		ret = send_byte(n, term_tmp.term_no);
		if (ret < 0)
			return -1;
		com_verify(&term_tmp, &n, 1);
		// 发9字节交易号
		ret = send_trade_num(&pbacc[once - 1].t_num, &term_tmp);
		if (ret < 0)
			return -1;
		// 发5 * N字节黑名单数据
		if ((ret = send_blkacc(&term_tmp, pbacc, once)) < 0)
			return -1;
		if (n & 0xC0) {// 说明是最后一包数据
			// 发四字节版本号
			ret = send_data2((char *)&pbinfo->edition, sizeof(pbinfo->edition), &term_tmp);
			if (ret < 0)
				return -1;
		}
		// 发校验
		ret = send_verify(&term_tmp);
		if (ret < 0)
			return -1;
		// 通讯结束, 进行本地数据处理
		pbacc += once;
		// 当黑名单超过ADDTIMENUM条后, 延迟时间改为15ms
		if (pbacc->t_num.num > ADDTIMENUM)
			mdelay(15);
		else
			mdelay(10);
		left -= once;
	}
	return 0;
}

/*
 * 强制更新单品种和快捷方式下各按键代表金额
 * write by wjzhe, June 11 2007
 */
int purse_update_key(term_ram *ptrm)
{
	unsigned char sendno, recvno;
	int ret;
{
	unsigned char price[32] = {0};
	// 键值全0则退出
	if (!memcmp(ptrm->pterm->price, price, sizeof(price))) {
		return 0;
	}
}
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 叫号
#if BWR_66J10
	//udelay(10);
#endif
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto update_key_failed;
	com_verify(ptrm, &sendno, sizeof(sendno));
	// 回号
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
#ifdef TESTCODE
		printk("purse update key recv no. %d: error %d\n", ptrm->term_no, ret);
#endif
		ptrm->status = ret;
		return ret;
	}
	// 判断机器号是否一致
	com_verify(ptrm, &recvno, sizeof(recvno));
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("update key return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}
	//usart_delay(1);
#if BWR_66J10
	//udelay(10);
#endif
	// 发1字节地址
	// 发终端号不能 | 0x80
	sendno = ptrm->term_no;
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	ret = send_byte(sendno, ptrm->term_no);
	if (ret < 0)
		goto update_key_failed;
	ptrm->add_verify += sendno;
	ptrm->dif_verify ^= sendno;
	// 发0x34命令字
	ret = send_byte(PURSE_UPDATE_KEY, ptrm->term_no);
	if (ret < 0)
		goto update_key_failed;
	ptrm->add_verify += PURSE_UPDATE_KEY;
	ptrm->dif_verify ^= PURSE_UPDATE_KEY;
	// 发包长度
	ret =  send_byte(34, ptrm->term_no);
	if (ret < 0)
		goto update_key_failed;
	ptrm->add_verify += 34;
	ptrm->dif_verify ^= 34;
	// 发包数据2+30, 2:单品种方式下单价；30:快捷方式下15个按键代表金额；
	ret = send_data2((char *)ptrm->pterm->price, 32, ptrm);
	if (ret < 0)
		goto update_key_failed;
	// 发2字节校验数据
	ret = send_verify(ptrm);
	if (ret < 0)
		goto update_key_failed;
#if BWR_66J10
	udelay(200);
#endif
	// 收GA
	ret = recv_ga(ptrm);
	if (ret < 0) {
		printk("purse %d update key recv GA error\n", ptrm->term_no);
		return -1;
	}
#ifdef TESTCODE
	else {
		printk("update key: recv ga ok\n");
	}
#endif
	return 0;
update_key_failed:
	ptrm->status = -1;
	return -1;
}

/*
 * 获得终端机黑名单版本号和交易号
 * write by wjzhe, June 11 2007
 */
int purse_get_edition(term_ram *ptrm)
{
	unsigned char sendno, recvno, cmd, *tmp, len;
	int ret, i;
	unsigned char tm[5] = {0};//write by duyy, 2013.11.21
	struct black_info info;
	memset(&info, 0, sizeof(info));
	// 发term_num | 0x80
	// 叫号
	sendno = ptrm->term_no | 0x80;
	ret = send_addr(sendno, ptrm->term_no);
	if (ret < 0)
		goto get_edition_failed;
	//com_verify(ptrm, &sendno, sizeof(sendno));
	// 收term_num, 做判断
	// 回号
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	// 判断机器号是否一致
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("get edition return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		ptrm->status = NOCOM;
		return -1;
	}
	//com_verify(ptrm, &recvno, sizeof(recvno));
	ptrm->add_verify = ptrm->dif_verify = 0;
	// 发1字节term_num
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		goto get_edition_failed;
	}
	com_verify(ptrm, &ptrm->term_no, sizeof(ptrm->term_no));
	// 发1字节0x47
	ret = send_byte(PURSE_GET_TNO, ptrm->term_no);
	if (ret < 0) {
		goto get_edition_failed;
	}
	ptrm->add_verify += PURSE_GET_TNO;
	ptrm->dif_verify ^= PURSE_GET_TNO;
	// 发包长度
	ret = send_byte(2, ptrm->term_no);
	if (ret < 0)
		goto get_edition_failed;
	ptrm->add_verify += 2;
	ptrm->dif_verify ^= 2;
	// 发2字节校验
	if (send_verify(ptrm) < 0)
		goto get_edition_failed;
//#if BWT_66J10
//	udelay(BWT_66J10);
//#endif
	ptrm->add_verify = ptrm->dif_verify = 0;
	// 收1字节term_num
	ret = recv_data((char *)&recvno, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	com_verify(ptrm, &recvno, sizeof(recvno));
	if ((recvno & 0x7F) != (ptrm->term_no & 0x7F)) {
		printk("get edition return term no. error %02x: %02x\n", recvno, ptrm->term_no);
		return -1;
	}
	// 收命令字0x48
	ret = recv_data((char *)&cmd, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = ret;
		return ret;
	}
	com_verify(ptrm, &cmd, sizeof(cmd));
	if (cmd != PURSE_RET_TNO) {
		printk("send bkin return cmd error %02x: %02x\n", cmd, ptrm->term_no);
		return -1;
	}
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF)
		return -1;
	// 收13字节数据, 版本号4字节, 黑名单交易号9字节
	// 收4字节黑名单版本号
	tmp = (unsigned char *)&info.edition;//&ptrm->blkinfo.edition;
	for (i = 0; i < sizeof(info.edition); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d termblk.edition error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收9字节交易号
	tmp = (unsigned char *)&info.trade_num.num;
	for (i = 0; i < sizeof(info.trade_num.num); i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d ptrm->blkinfo.trade_num.num error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	//tmp = info.trade_num.tm;//modified by duyy, 2013.1120
	tmp = tm;//modified by duyy, 2013.11.21
	for (i = 0; i < 5;/*sizeof(info.trade_num.tm);*/ i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d ptrm->blkinfo.trade_num.tm error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收校验2字节
	if (chk_verify(ptrm) < 0)
		return -1;
	memcpy(&ptrm->blkinfo, &info, sizeof(ptrm->blkinfo));
	return 0;
get_edition_failed:
	ptrm->status = -1;
	return -1;
}

/*
 * 新POS机光电卡交互步骤一
 * write by wjzhe, June 13, 2007
 */
int purse_recv_leid(term_ram *ptrm, int allow, unsigned char *tm)
{
	static int itm;//write by duyy, 2012.4.26
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0, len = 6;
	unsigned char timenum = 0;//write by duyy, 2013.3.26
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
#ifdef PRE_REDUCE
	int pos_money = 0;
	len = 9;
#endif
	itm = _cal_itmofday(&tm[1]);//2012.4.26
	//tmp += 3;
	ret = recv_len(ptrm);
	if (ret == 0xFF)
		return -1;
	//ret = send_byte(len, ptrm->term_no);
	//if (ret < 0)
	//	return -1;
	// receive 4-byte card number
	for (i = 0; i < 4; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(4 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#ifdef PRE_REDUCE
	// 收3字节可能消费金额
	tmp = (unsigned char *)&pos_money;
	for (i = 0; i < 4; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(4 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#endif
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv leid chk1 error\n");
		return -1;
	}
	// search id
	pacc = search_id(cardno);
#ifdef DEBUG
	printk("search res: pacc: %p, %d\n",
		pacc, (pacc) ? pacc->card_num : cardno);
#endif
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
#if 0
		printk("search over: %08x not exist\n", cardno);
		printk("0: %08x, 1: %08x, 2: %08x\n", paccmain[0].card_num,
			paccmain[1].card_num, paccmain[2].card_num);
#endif
		feature = 1;
	} else {
#ifdef PRE_REDUCE
		int m_tmp = pacc->feature & 0xF;
		pos_money /= 100;
		m_tmp -= pos_money;
		if (m_tmp < 0)
			m_tmp = 0;
		
#endif
		//printk("purse_recv_leid find!\n");
		feature = 4;
		//if (ptrm->pterm->term_type & 0x20) {
		//	feature = 0;
		//} else {
		//	feature = 4;
		//}
		if ((pacc->feature & 0xC0) == 0x80) {
			// 这是挂失卡
			feature |= 1 << 5;
		}
#ifdef PRE_REDUCE
		if (m_tmp == 0)
			feature |= 1 << 3;

#endif
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0
			) {
			// 此卡达到餐限
			// 此卡要输密码
			feature |= 1 << 3;
			//printk("input password\n");//modified,2012.2.22
		}
///////////////////////////////////////////////////////////////////////////////////
	#if 0//deleted by duyy, 2013.3.26	
		if ( (ptrm->pterm->power_id == 0xF0) ||
			 ((pacc->feature & 0x30) == 0) ||
			 ((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			 ) {
			//money[0] = pacc->money & 0xFF;// 该终端为1类终端或帐户为1类身份或帐户身份与终端类型对应，则允许其消费
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
			if (pacc->money < 0) {
				money = 0;
			} else {
				money = (unsigned long)pacc->money;
			}
			printk("normal consume\n");//2012.5.2
		} else {
			feature = 1;
		}
	#endif
//////////////////////////////////////////////////////////////////////////////////		
		//write by duyy, 2013.3.26
		if (pacc->money < 0) {
			money = 0;
		} else {
			money = (unsigned long)pacc->money;
		}
		if (!(ptrm->pterm->param.term_type & 0x20)) {	//出纳机不判断时段、身份类型匹配，禁止消费问题,modified 2013.4.2
			timenum = ptmnum[pacc->id_type][ptrm->pterm->power_id];
			//printk("cardno:%ld timenum = %x\n", cardno, timenum);
			if (timenum & 0x80){//modified by duyy, 2013.3.28,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
				feature = 1;
				//printk("whole day prohibit\n");
				goto over;
			}
			else {
				for (i = 0; i < 7; i++){
					if (timenum & 0x1){
						if(itm >= term_time[i].begin && itm <= term_time[i].end){
							feature = 1;
							//printk("term_time[%d] is prohibited, begin %d,end %d\n", i, term_time[i].begin, term_time[i].end);
							goto over;
						}
					}
					timenum = timenum >> 1;
					//printk("timenum is %x\n", timenum);
				}
				//printk("continue consume\n");
			}
		}
		
///////////////////////////////////////////////////////////////////////////////////
	}
	// 判断是否允许脱机使用光电卡
#if 0//modified by duyy, 2013.4.19
	if (!allow) {
#ifdef DEBUG
		printk("not allow: %ld\n", cardno);
#endif
		feature = 1;
		money = 0;
	}
#endif
#if 0
		if (feature == 1) {
			printk("feature: %02x\n", feature);
			feature = 4;
			money = (unsigned long)pacc->money;
		}
#endif
over:
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发终端号
	ret = send_byte(ptrm->term_no, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= ptrm->term_no;
	ptrm->add_verify += ptrm->term_no;
	// 发命令字
	ret = send_byte(PURSE_SEND_CARDNO, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= PURSE_SEND_CARDNO;
	ptrm->add_verify += PURSE_SEND_CARDNO;
	// 发包长度
	len = 7;
	ret = send_byte(len, ptrm->term_no);
	if (ret < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= len;
	ptrm->add_verify += len;
	// 发身份信息
	//printk("purse_recv_leid feature=%x\n", feature);//added by duyy,2012.4.26
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;
	//printk("feature is %02x\n", feature);
	//money = binl2bcd(money) & 0xFFFFFF;
	tmp = (unsigned char *)&money;
	//tmp += 2;
	// 发4字节余额
	for (i = 0; i < sizeof(money); i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 发校验
	if (send_verify(ptrm) < 0)
		return -1;
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// 终端可以接收流水了, 保存状态
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		for (i = 0; i < CASHBUFSZ; i++){
			//先判断相同出纳机号的信息是否有存储
			if (ptrm->term_no ==  cashbuf[i].termno){
				//出纳机号相同则数据直接覆盖存储
				cashbuf[i].feature = feature;
				cashbuf[i].consume = 0;
				cashbuf[i].status = CASH_CARDIN;
				cashbuf[i].termno = ptrm->term_no;
				cashbuf[i].cardno = cardno;
				if (pacc) {
					cashbuf[i].accno = pacc->acc_num;
					cashbuf[i].cardno = pacc->card_num;
					cashbuf[i].managefee = fee[pacc->managefee & 0xF];
					cashbuf[i].money = pacc->money;
				}
				// 这时候写入终端金额吗?
				cashbuf[i].term_money = ptrm->term_money;
				return 0;
			}
		}
		// 记录非法卡
		cashbuf[cashterm_ptr].feature = feature;
		cashbuf[cashterm_ptr].consume = 0;
		cashbuf[cashterm_ptr].status = CASH_CARDIN;
		cashbuf[cashterm_ptr].termno = ptrm->term_no;
		cashbuf[cashterm_ptr].cardno = cardno;
		if (pacc) {
			cashbuf[cashterm_ptr].accno = pacc->acc_num;
			cashbuf[cashterm_ptr].cardno = pacc->card_num;
			cashbuf[cashterm_ptr].managefee = fee[pacc->managefee & 0xF];
			cashbuf[cashterm_ptr].money = pacc->money;
		}
		// 这时候写入终端金额吗?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
		if (cashterm_ptr == CASHBUFSZ){
			cashterm_ptr = 0;
		}
	}
#endif
	return 0;
}

/*
 * receive le card flow
 * but store into flow any data is HEX
 */
int purse_recv_leflow(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int limit, tail;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF)
		return -1;
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收1字节结算机编号
	tmp = (unsigned char *)&leflow.tml_num;
	for (i = 0; i < sizeof(leflow.tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收1字节区号
	tmp = (unsigned char *)&leflow.areano;
	for (i = 0; i < sizeof(leflow.areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#ifndef QINGHUA
#error "Qinghua recv leflow: not allow recv psam_num..."
	// 收2字节PSAM卡编号
	tmp = (unsigned char *)&leflow.psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收4字节PSAM卡交易序号
	tmp = (unsigned char *)&leflow.psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#endif
	// recv 4-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
#ifdef QINGHUA
	// 清华需要添加psam卡号
	//ptrm->psam_trd_num = (int)nouse;
#endif
	// recv flow type
	ret = recv_data((char *)&leflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow.flow_type;
	ptrm->dif_verify ^= leflow.flow_type;
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	//leflow.consume_sum &= 0xFFFF;
	//if (flag)		// if BCD then convert BIN
	//	leflow.consume_sum = bcdl2bin(leflow.consume_sum);
	// third check NXOR
	//if (verify_all(ptrm, CHKNXOR) < 0)
	//	return -1;
	// check if receive the flow
	if (ptrm->flow_flag) {
		ret = send_ga(ptrm, 0);
		return ret;
	}
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		ret = send_ga(ptrm, 1);
		return ret;
	}
	// adjust money limit
	limit = pacc->feature & 0xF;
	//printk("feature=%2x\n", pacc->feature);//added by duyy,2012.5.2
	//printk("leflow limit=%d\n", limit);//added by duyy,2012.5.2
	if ((limit != 0xF) && limit) {
		// 需要调整餐限
		limit -= leflow.consume_sum / 100 + 1;
		//printk("consume_sum=%d\n", leflow.consume_sum / 500);//added by duyy,2012.2.22
		//printk("consume limit=%d\n", limit);//added by duyy,2012.2.22
		if (!(leflow.consume_sum % 100)) {
			limit++;
		}
		if (limit < 0)
			limit = 0;
		//printk("limit=%d\n", limit);//added by duyy,2012.2.22
	}
	//if (leflow.consume_sum < 0) {
	//	printk("error!!!!!!!!\n");
	//}
	// 账户库改写
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}
	pacc->feature &= 0xF0;
	//printk("pacc->feature=%2x\n", pacc->feature);//added by duyy,2012.2.24
	pacc->feature |= limit;
	//printk("limit_feature=%2x\n", pacc->feature);//added by duyy,2012.2.24

	// 终端机数据记录
	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
	leflow.flow_type |= LECONSUME;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm;
	//leflow.tml_num = ptrm->term_no;
	leflow.acc_num = pacc->acc_num;
	// 流水号的处理
	//flow_no = maxflowno;
	leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// 流水区头尾的处理
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// 流水缓存区满?
		printk("uart 485 flow buffer no space\n");
		ret = send_ga(ptrm, -1);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	ret = send_ga(ptrm, 0);
	return ret;
}

int purse_recv_ledep(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int tail, managefee;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	if (!(ptrm->pterm->param.term_type & 0x20)) {	// 断断是否为出纳机
		// 这不是出纳机
		//printk("this is not cash terminal\n");
		usart_delay(20);
		return 0;
	}
	
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF)
		return -1;
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收1字节结算机编号
	tmp = (unsigned char *)&leflow.tml_num;
	for (i = 0; i < sizeof(leflow.tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收1字节区号
	tmp = (unsigned char *)&leflow.areano;
	for (i = 0; i < sizeof(leflow.areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收2字节PSAM卡编号
	tmp = (unsigned char *)&leflow.psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收4字节PSAM卡交易序号
	tmp = (unsigned char *)&leflow.psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// recv 4-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
	// recv flow type
	ret = recv_data((char *)&leflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow.flow_type;
	ptrm->dif_verify ^= leflow.flow_type;
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	// check if receive the flow
	if (ptrm->flow_flag) {
		ret = send_ga(ptrm, 0);
		return ret;
	}
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		ret = send_ga(ptrm, 1);
		return ret;
	}
	//modified by duyy, 2013.4.18
	if (pacc->managefee & DEPBIT) {	//某些人非10号终端不能充值
		if (ptrm->term_no != 10) {//write by duyy, 2013.4.1
			// 非10号终端不能做出纳机
			usart_delay(20);
			return 0;
		}	
	}
	// 恢复餐限
	if ((pacc->feature & 0xF) != 0xF) {
		pacc->feature &= 0xF0;
		pacc->feature |= 0xE;
	}
	managefee = fee[pacc->managefee & 0xF];		//管理费比率
	managefee = managefee * leflow.consume_sum / 100;	//计算管理费
	//printk("pacc->money: %d, managefee: %d\n", pacc->money, managefee);
	pacc->money += leflow.consume_sum - managefee;		//余额加钱减管费
	//printk("pacc->money: %d\n", pacc->money);

	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
	leflow.flow_type |= LECHARGE;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm;
	leflow.acc_num = pacc->acc_num;
	// 流水号的处理
	leflow.flow_num = maxflowno++;
	leflow.manage_fee = managefee * (-1);
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// 流水区头尾的处理
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// 流水缓存区满?
		printk("recv le dep uart 485 flow buffer no space\n");
		ret = send_ga(ptrm, -1);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	ret = send_ga(ptrm, 0);
#ifdef CONFIG_RECORD_CASHTERM
	// 终端接收取款流水, 保存状态
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
		//modified by duyy, 2013.4.7
		for (i = 0; i < CASHBUFSZ; i++){
			//先判断相同出纳机号的信息是否有存储
			if (ptrm->term_no ==  cashbuf[i].termno){
				//出纳机号相同则数据直接覆盖存储
				cashbuf[i].accno = pacc->acc_num;
				cashbuf[i].cardno = pacc->card_num;
				cashbuf[i].feature = 0;
				cashbuf[i].managefee = leflow.manage_fee;
				cashbuf[i].money = pacc->money;
				cashbuf[i].consume = leflow.consume_sum;
				cashbuf[i].status = CASH_DEPOFF;
				cashbuf[i].termno = ptrm->term_no;
				// 增加终端金额
				cashbuf[i].term_money = ptrm->term_money;
				return ret;
			}
		}
		cashbuf[cashterm_ptr].accno = pacc->acc_num;
		cashbuf[cashterm_ptr].cardno = pacc->card_num;
		cashbuf[cashterm_ptr].feature = 0;
		cashbuf[cashterm_ptr].managefee = leflow.manage_fee;
		cashbuf[cashterm_ptr].money = pacc->money;
		cashbuf[cashterm_ptr].consume = leflow.consume_sum;
		cashbuf[cashterm_ptr].status = CASH_DEPOFF;
		cashbuf[cashterm_ptr].termno = ptrm->term_no;
		// 增加终端金额
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
		if (cashterm_ptr == CASHBUFSZ){
			cashterm_ptr = 0;
		}
	}
#endif
	return ret;
}

int purse_recv_letake(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int tail;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	if (!(ptrm->pterm->param.term_type & 0x20)) {	// 断断是否为出纳机
		// 这不是出纳机
		//printk("this is not cash terminal\n");
		usart_delay(20);
		return 0;
	}

	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF)
		return -1;
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收1字节结算机编号
	tmp = (unsigned char *)&leflow.tml_num;
	for (i = 0; i < sizeof(leflow.tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收1字节区号
	tmp = (unsigned char *)&leflow.areano;
	for (i = 0; i < sizeof(leflow.areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收2字节PSAM卡编号
	tmp = (unsigned char *)&leflow.psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收4字节PSAM卡交易序号
	tmp = (unsigned char *)&leflow.psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// recv 4-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
	// recv flow type
	ret = recv_data((char *)&leflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow.flow_type;
	ptrm->dif_verify ^= leflow.flow_type;
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	// check if receive the flow
	if (ptrm->flow_flag) {
		ret = send_ga(ptrm, 0);
		return ret;
	}
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		ret = send_ga(ptrm, 1);
		return ret;
	}
	//modified by duyy, 2013.4.18
	if (pacc->managefee & DEPBIT) {	//某些人非10号终端不能充值
		if (ptrm->term_no != 10) {//write by duyy, 2013.4.1
			// 非10号终端不能做出纳机
			usart_delay(20);
			return 0;
		}	
	}
	//printk("pacc->money: %d, managefee: %d\n", pacc->money, managefee);
	pacc->money -= leflow.consume_sum;		//余额减钱
	if (pacc->money < 0) {
		printk("account money below zero!\n");
		pacc->money = 0;
	}
	//printk("pacc->money: %d\n", pacc->money);

	ptrm->term_cnt++;
	ptrm->term_money -= leflow.consume_sum;
	// init leflow
	leflow.flow_type |= LETAKE;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm;
	leflow.acc_num = pacc->acc_num;
	// 流水号的处理
	leflow.flow_num = maxflowno++;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// 流水区头尾的处理
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// 流水缓存区满?
		printk("recv le dep uart 485 flow buffer no space\n");
		ret = send_ga(ptrm, -1);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	ret = send_ga(ptrm, 0);
#ifdef CONFIG_RECORD_CASHTERM
	// 终端接收取款流水, 保存状态
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
		//modified by duyy, 2013.4.7
		for (i = 0; i < CASHBUFSZ; i++){
			//先判断相同出纳机号的信息是否有存储
			if (ptrm->term_no ==  cashbuf[i].termno){
				//出纳机号相同则数据直接覆盖存储
				cashbuf[i].accno = pacc->acc_num;
				cashbuf[i].cardno = pacc->card_num;
				cashbuf[i].feature = 0;
				cashbuf[i].managefee = leflow.manage_fee;
				cashbuf[i].money = pacc->money;
				cashbuf[i].consume = leflow.consume_sum;
				cashbuf[i].status = CASH_TAKEOFF;
				cashbuf[i].termno = ptrm->term_no;
				// 增加终端金额
				cashbuf[i].term_money = ptrm->term_money;
				return ret;
			}
		}
		cashbuf[cashterm_ptr].accno = pacc->acc_num;
		cashbuf[cashterm_ptr].cardno = pacc->card_num;
		cashbuf[cashterm_ptr].feature = 0;
		cashbuf[cashterm_ptr].managefee = leflow.manage_fee;
		cashbuf[cashterm_ptr].money = pacc->money;
		cashbuf[cashterm_ptr].consume = leflow.consume_sum;
		cashbuf[cashterm_ptr].status = CASH_TAKEOFF;
		cashbuf[cashterm_ptr].termno = ptrm->term_no;
		// 增加终端金额
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
		if (cashterm_ptr == CASHBUFSZ){
			cashterm_ptr = 0;
		}
	}
#endif
	return ret;
}

/*
 * 存储分组流水函数, 传入流水指针, 消费指针, 及流水号
 * 返回处理条数, 错误返回-1, 但实际上函数不允许错误
 */
static inline int store_gpflow(le_flow *pleflow, int *money, int flowno)
{
	le_flow *pfw = pleflow;		// 要处理数据的指针
	le_flow *pinit = pleflow;	// 初始化数据的指针
	int i = 0, num = flowno;
	// 要一条一条的处理流水数据
	// 当钱有数据时进行存储数据
	while (/*(*money) && */(i < 5)) {
		// 将要处理的数据进行初始化
		if (pinit != pfw) {
			memcpy(pfw, pinit, sizeof(le_flow));
		}
		// 填充消费额
		pfw->consume_sum = *money;
		// 流水号
		pfw->flow_num = flowno;
		// group info
		pfw->food_grp_num = i + 1;
		flowno++;
		money++;
		i++;
		pfw++;
	}
	return flowno - num;
}

/*
 * 接收分组流水, 返回0正常, 其它错误
 * AppCardID	DWORD	4	HEX	存折卡卡号	
 * PosSetNo		BYTE	1	HEX	结算机被设置编号	
 * PosSecNo		BYTE	1	HEX	结算机被设置区号	
 * CsmMny		DWORD	4	HEX	交易总金额，单位为分         	
 * GroupAMny	SWORD	3	HEX	A组的消费金额（分为单位）	
 * GroupBMny	SWORD	3	HEX	B组的消费金额（分为单位）	
 * GroupCMny	SWORD	3	HEX	C组的消费金额（分为单位）	
 * GroupDMny	SWORD	3	HEX	D组的消费金额（分为单位）	
 * GroupEMny	SWORD	3	HEX	E组的消费金额（分为单位）	
 * BakReg		BYTE	1	HEX	待扩展	0X00
 * FlowType		BYTE	1	HEX	流水类型标识：Bit0消费流水、Bit1失败流水、Bit2空、Bit3出纳加钱流水、Bit4出纳取钱流水	
 */
int purse_recv_gpflow(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow[5];
	int ret, i;
	int limit, tail;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	int money[6] = {0};
	memset(leflow, 0, sizeof(leflow));
	// 收包长度
	len = recv_len(ptrm);
	if (len == 0xFF)	// len != 35 ???
		return -1;
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow[0].card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收1字节结算机编号
	tmp = (unsigned char *)&leflow[0].tml_num;
	for (i = 0; i < sizeof(leflow[0].tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收1字节区号
	tmp = (unsigned char *)&leflow[0].areano;
	for (i = 0; i < sizeof(leflow[0].areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#ifndef QINGHUA
#error "Qinghua recv gpflow: not allow recv psam_num..."
	// 收2字节PSAM卡编号
	tmp = (unsigned char *)&leflow[0].psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收4字节PSAM卡交易序号
	tmp = (unsigned char *)&leflow[0].psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#endif
	// recv 4-byte consume money
	tmp = (unsigned char *)&money[0];
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv 3 * 5 byte group consume money, recv ABCDE
	for (i = 0; i < 5; i++) {
		int k;
		tmp = (unsigned char *)&money[i + 1];
		//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
		for (k = 0; k < 3; k++) {
			ret = recv_data((char *)tmp, ptrm->term_no);
			if (ret < 0) {
				if (ret == -1) {		// timeout, pos is not exist
					ptrm->status = NOTERMINAL;
				}
				if (ret == -2) {		// other errors
					ptrm->status = NOCOM;
				}
				return ret;
			}
			ptrm->dif_verify ^= *tmp;
			ptrm->add_verify += *tmp;
			tmp++;
		}
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
	// recv flow type
	ret = recv_data((char *)&leflow[0].flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow[0].flow_type;
	ptrm->dif_verify ^= leflow[0].flow_type;
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	//leflow.consume_sum &= 0xFFFF;
	//if (flag)		// if BCD then convert BIN
	//	leflow.consume_sum = bcdl2bin(leflow.consume_sum);
	// third check NXOR
	//if (verify_all(ptrm, CHKNXOR) < 0)
	//	return -1;
	// check if receive the flow
	if (ptrm->flow_flag) {
		ret = send_ga(ptrm, 0);
		return ret;
	}
	// search id
	pacc = search_id(leflow[0].card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow[0].card_num);
		ret = send_ga(ptrm, 1);
		return ret;
	}
	// adjust money limit
	limit = pacc->feature & 0xF;
	if ((limit != 0xF) && limit) {
		// 需要调整餐限
		limit -= money[0] / 100 + 1;
		if (!(money[0] % 100)) {// 刚好满100时, 认为是一次
			limit++;
		}
		if (limit < 0)
			limit = 0;
	}
	//if (leflow.consume_sum < 0) {
	//	printk("error!!!!!!!!\n");
	//}
	// 账户库改写
	pacc->money -= money[0];
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}
	pacc->feature &= 0xF0;
	pacc->feature |= limit;
	// 终端机数据记录
	ptrm->term_cnt++;
	ptrm->term_money += money[0];
	// init leflow
	leflow[0].flow_type |= LECONSUME;
	leflow[0].date.hyear = *tm++;
	leflow[0].date.lyear = *tm++;
	leflow[0].date.mon = *tm++;
	leflow[0].date.mday = *tm++;
	leflow[0].date.hour = *tm++;
	leflow[0].date.min = *tm++;
	leflow[0].date.sec = *tm;
	//leflow.tml_num = ptrm->term_no;
	leflow[0].acc_num = pacc->acc_num;
	// 流水号的处理
	//flow_no = maxflowno;
	i = store_gpflow(leflow, &money[1], maxflowno);
	// 一共处理了i条数据, 我们需要再处理流水号之类的信息
	maxflowno += i;
	//leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// 流水区头尾的处理
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// 流水缓存区满?
		printk("uart 485 flow buffer no space\n");
		ret = send_ga(ptrm, -1);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow[0], sizeof(flow) * i);
	tail += i;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum += i;
	space_remain -= i;// 此处在剩余流水不足时会有错误, BUG!!!
	ret = send_ga(ptrm, 0);
	return ret;
}


/*
 * 新POS机光电卡交互步骤一
 * write by wjzhe, June 13, 2007
 */
int purse_recv_leid2(term_ram *ptrm, int allow, unsigned char *tm)
{
	static int itm;//write by duyy, 2012.4.26
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0, len = 6;
	unsigned char timenum = 0;//write by duyy, 2013.3.26
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
#ifdef PRE_REDUCE
	int pos_money = 0;
	len = 9;
#endif
	itm = _cal_itmofday(&tm[1]);//2012.4.26
	// receive 4-byte card number
	for (i = 0; i < 4; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(4 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#ifdef PRE_REDUCE
	// 收3字节可能消费金额
	tmp = (unsigned char *)&pos_money;
	for (i = 0; i < 4; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(4 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
#endif
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv leid chk1 error\n");
		return -1;
	}
	// search id
	pacc = search_id(cardno);
#ifdef DEBUG
	printk("search res: pacc: %p, %d\n",
		pacc, (pacc) ? pacc->card_num : cardno);
#endif
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
	} else {
#ifdef PRE_REDUCE
		int m_tmp = pacc->feature & 0xF;
		pos_money /= 100;
		m_tmp -= pos_money;
		if (m_tmp < 0)
			m_tmp = 0;
#endif
		//printk("purse_recv_leid2 find!\n");//2012.4.26
		feature = 4;
		//if (ptrm->pterm->term_type & 0x20) {
		//	feature = 0;
		//} else {
		//	feature = 4;
		//}
		if ((pacc->feature & 0xC0) == 0x80) {
			// 这是挂失卡
			feature |= 1 << 5;
		}
#ifdef PRE_REDUCE
		if (m_tmp == 0)
			feature |= 1 << 3;
#endif
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0
			) {
			// 此卡达到餐限
			// 此卡要输密码
			feature |= 1 << 3;
		}
///////////////////////////////////////////////////////////////////////////////////
	#if 0//deleted by duyy, 2013.3.26
		if ( (ptrm->pterm->power_id == 0xF0) ||
			 ((pacc->feature & 0x30) == 0) ||
			 ((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			 ) {
			//money[0] = pacc->money & 0xFF;// 该终端为1类终端或帐户为1类身份或帐户身份与终端类型对应，则允许其消费
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
			if (pacc->money < 0) {
				money = 0;
			} else {
				money = (unsigned long)pacc->money;
			}
		} else {
			feature = 1;
		}
	#endif
//////////////////////////////////////////////////////////////////////////////////		
		//write by duyy, 2013.3.26
		if (pacc->money < 0) {
			money = 0;
		} else {
			money = (unsigned long)pacc->money;
		}
		if (!(ptrm->pterm->param.term_type & 0x20)) {	//出纳机不判断时段、身份类型匹配，禁止消费问题
			timenum = ptmnum[pacc->id_type][ptrm->pterm->power_id];
			//printk("cardno:%ld timenum = %x\n", cardno, timenum);
			if (timenum & 0x80){//modified by duyy, 2013.3.28,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
				feature = 1;
				//printk("whole day prohibit\n");
				goto over;
			}
			else {
				for (i = 0; i < 7; i++){
					if (timenum & 0x1){
						if(itm >= term_time[i].begin && itm <= term_time[i].end){
							feature = 1;
							//printk("term_time[%d] is prohibited, begin %d,end %d\n", i, term_time[i].begin, term_time[i].end);
							goto over;
						}
					}
					timenum = timenum >> 1;
					//printk("timenum is %x\n", timenum);
				}
				//printk("continue consume\n");
			}
		}
///////////////////////////////////////////////////////////////////////////////////
	}
	// 判断是否允许脱机使用光电卡
#if 0//modified by duyy, 2013.4.19
	if (!allow) {
#ifdef DEBUG
		printk("not allow: %d\n", cardno);
#endif
		feature = 1;
		money = 0;
	}
#endif
over:
	ptrm->add_verify = 0;
	ptrm->dif_verify = 0;
	// 发身份信息
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;
	//printk("feature is %02x\n", feature);
	//money = binl2bcd(money) & 0xFFFFFF;
	tmp = (unsigned char *)&money;
	//tmp += 2;
	// 发4字节余额
	for (i = 0; i < sizeof(money); i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 发校验
	if (send_verify(ptrm) < 0)
		return -1;
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// 终端可以接收流水了, 保存状态
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		for (i = 0; i < CASHBUFSZ; i++){
			//先判断相同出纳机号的信息是否有存储
			if (ptrm->term_no ==  cashbuf[i].termno){
				//出纳机号相同则数据直接覆盖存储
				cashbuf[i].feature = feature;
				cashbuf[i].consume = 0;
				cashbuf[i].status = CASH_CARDIN;
				cashbuf[i].termno = ptrm->term_no;
				cashbuf[i].cardno = cardno;
				if (pacc) {
					cashbuf[i].accno = pacc->acc_num;
					cashbuf[i].cardno = pacc->card_num;
					cashbuf[i].managefee = fee[pacc->managefee & 0xF];
					cashbuf[i].money = pacc->money;
				}
				// 这时候写入终端金额吗?
				cashbuf[i].term_money = ptrm->term_money;
				return 0;
			}
		}
		// 记录非法卡
		cashbuf[cashterm_ptr].feature = feature;
		cashbuf[cashterm_ptr].consume = 0;
		cashbuf[cashterm_ptr].status = CASH_CARDIN;
		cashbuf[cashterm_ptr].termno = ptrm->term_no;
		cashbuf[cashterm_ptr].cardno = cardno;
		if (pacc) {
			cashbuf[cashterm_ptr].accno = pacc->acc_num;
			cashbuf[cashterm_ptr].cardno = pacc->card_num;
			cashbuf[cashterm_ptr].managefee = fee[pacc->managefee & 0xF];
			cashbuf[cashterm_ptr].money = pacc->money;
		}
		// 这时候写入终端金额吗?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
		if (cashterm_ptr == CASHBUFSZ){
			cashterm_ptr = 0;
		}
	}
#endif
	return 0;
}

static inline int send_byte3(char buf, unsigned char num)
{
	int i;
	for (i = 0; i < 3; i++) {
		send_byte(buf, num);
	}
	return 0;
}

/*
 * receive le card flow
 * but store into flow any data is HEX
 */
int purse_recv_leflow2(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int limit, tail;
	unsigned char *tmp, nouse, len;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	// recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	//tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// 收1字节结算机编号
	leflow.tml_num = ptrm->term_no;
	leflow.areano = ptrm->pterm->param.areano;
#if 0
	tmp = (unsigned char *)&leflow.tml_num;
	for (i = 0; i < sizeof(leflow.tml_num); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收1字节区号
	tmp = (unsigned char *)&leflow.areano;
	for (i = 0; i < sizeof(leflow.areano); i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#endif
#ifndef QINGHUA
#error "Qinghua recv leflow: not allow recv psam_num..."
	// 收2字节PSAM卡编号
	tmp = (unsigned char *)&leflow.psam_num;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
	// 收4字节PSAM卡交易序号
	tmp = (unsigned char *)&leflow.psam_trd_num;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->add_verify += *tmp;
		ptrm->dif_verify ^= *tmp;
		tmp++;
	}
#endif
	// recv 4-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	//tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// recv one no used byte
	ret = recv_data((char *)&nouse, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += nouse;
	ptrm->dif_verify ^= nouse;
#ifdef QINGHUA
	// 清华需要添加psam卡号
	//ptrm->psam_trd_num = (int)nouse;
#endif
	leflow.flow_type = 1;
#if 0
	// recv flow type
	ret = recv_data((char *)&leflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = NOTERMINAL;
		}
		if (ret == -2) {		// other errors
			ptrm->status = NOCOM;
		}
		return ret;
	}
	ptrm->add_verify += leflow.flow_type;
	ptrm->dif_verify ^= leflow.flow_type;
#endif
	ret = chk_verify(ptrm);
	if (ret < 0) {
		printk("recv le flow verify error\n");
		if (ret == ERRVERIFY) {
			ret = send_byte3(0x0, ptrm->term_no);
			//ret = send_ga(ptrm, ret);
		}
		return -1;
	}
	// check if receive the flow
	if (ptrm->flow_flag) {
		//ret = send_ga(ptrm, 0);
		ret = send_byte3(0x55, ptrm->term_no);
		return ret;
	}
	//printk("purse_recv_leflow2\n");//2012.5.2
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		//ret = send_ga(ptrm, 1);
		ret = send_byte3(0x0, ptrm->term_no);
		return ret;
	}
	// adjust money limit
	limit = pacc->feature & 0xF;
	if ((limit != 0xF) && limit) {
		// 需要调整餐限
		limit -= leflow.consume_sum / 100 + 1;
		if (!(leflow.consume_sum % 100)) {// 刚好满100时, 认为是一次
			limit++;
		}
		if (limit < 0)
			limit = 0;
	}
	//if (leflow.consume_sum < 0) {
	//	printk("error!!!!!!!!\n");
	//}
	// 账户库改写
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}
	pacc->feature &= 0xF0;
	pacc->feature |= limit;
	// 终端机数据记录
	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
	leflow.flow_type |= LECONSUME;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm;
	//leflow.tml_num = ptrm->term_no;
	leflow.acc_num = pacc->acc_num;
	// 流水号的处理
	//flow_no = maxflowno;
	leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// 流水区头尾的处理
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// 流水缓存区满?
		printk("uart 485 flow buffer no space\n");
		//ret = send_ga(ptrm, -1);
		ret = send_byte3(0x0, ptrm->term_no);
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(&pflow[tail], &leflow, sizeof(flow));
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
	//ret = send_ga(ptrm, 0);
	ret = send_byte3(0x55, ptrm->term_no);
	return ret;
}

#if 0
/*
 * 搜索PSAM卡库
 */
psam_local *search_psam(unsigned short num, struct psam_info *psinfo)
{
	psam_local *psl;
	int cnt, i;
	// 简单的进行参数验证
	if (psinfo == NULL)
		return NULL;
	cnt = psinfo->cnt;
	psl = psinfo->psam;
	if (cnt == 0 || psl == NULL)
		return NULL;
	for (i = 0; i < cnt; i++) {
		if (num == psl[i].psamno) {
			// find it, return
			return &psl[i];
		}
	}
	return NULL;
}
/*
 * 卡片特殊应用功能启用及参数下载
 * write by wjzhe, June 11 2007
 */
int purse_startup_newfunc(term_ram *ptrm)
{
	// 发1字节地址
	// 发0x34命令字
	/*
	A:卡消费时间（餐次）设置；
	B:卡消费次数限制；
	C:卡消费流水是否存储
	...
	*/
	return 0;
}

/*
 * 指定地址段内存储的黑名单数据
 * write by wjzhe, June 11 2007
 */
int purse_update_data(term_ram *ptrm, int addr, void *data, int n)
{
	return 0;
}

/*
 * 指定时间段流水或未上传流水上传
 * write by wjzhe, June 11 2007
 */
int purse_get_flow(term_ram *ptrm, unsigned char *tm_s, unsigned char *tm_e)
{
	return 0;
}
#endif

