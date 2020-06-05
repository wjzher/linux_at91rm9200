/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：deal_data.c
 * 摘    要：完成支持TS11的数据处理函数
 *			 支持新的终端机, 将TS11处理函数去掉
 *			 支持2.1C网络异区流水处理函数
 * 			 
 * 当前版本：2.3
 * 作    者：wjzhe
 * 完成日期：2007年9月10日
 *
 * 取代版本：2.2
 * 原作者  ：wjzhe
 * 完成日期：2007年8月13日
 *
 * 取代版本：2.0 
 * 原作者  ：wjzhe
 * 完成日期：2007年6月6日
 *
 * 取代版本：1.0 
 * 原作者  ：wjzhe
 * 完成日期：2006年12月3日
 */
#include "uart_dev.h"
#include "data_arch.h"
#include "uart.h"
#include "deal_data.h"
#include "no_money_type.h"
#include "at91_rtc.h"
#undef DEBUG

#if 0
extern black_acc *pblack;
#endif

int send_byte5(char buf, unsigned char num)
{
	int i;
	for (i = 0; i < 2; i++) {
		send_byte(buf, num);
	}
	return 0;
}

unsigned long binl2bcd(unsigned long val)
{
	int i;
	unsigned long tmp = 0;
	if (val >= 100000000) {
		printk("binl2bcd parameter error!\n");
		return -1;
	}
	for (i = 0; i < sizeof(val) * 2; i++) {
		tmp |= (val % 10) << (i * 4);
		val /= 10;
	}
	return tmp;
}

void usart_delay(int n)
{
	int i;
	for (i = 0; i < n; i++) {
		udelay(ONEBYTETM);
	}
}

acc_ram *search_no(unsigned long cardno, acc_ram *pacc, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++) {
		if (pacc[i].card_num == cardno) {
			pacc += i;
			goto end;
		}
	}
	pacc = NULL;
end:
	return pacc;
}
acc_ram *search_no2(const unsigned long cardno, acc_ram *pacc, int cnt)
{
	int head, tail, mid;
	if (cnt <= 0 || !pacc)		// cnt < 0, there is no account
		return NULL;
	head = 0;
	tail = cnt - 1;
	mid = (tail + head) / 2;
	do {
		if (pacc[mid].card_num == cardno) {
			pacc += mid;
			goto end;
		} else if (pacc[mid].card_num > cardno) {
			tail = mid;
			mid = (tail + head) / 2;
		} else {
			head = mid;
			mid = (tail + head) / 2 + 1;
		}
	} while (head != tail && tail != (head + 1)/*mid <= tail && mid >= head*/);
	if (pacc[head].card_num == cardno) {
		pacc += head;
		goto end;
	}
	if (pacc[tail].card_num == cardno) {
		pacc += tail;
		goto end;
	}
	pacc = NULL;
end:
	return pacc;
}

acc_ram *search_id(const unsigned long cardno)
{
	acc_ram *pacc;
	pacc = search_no2(cardno, paccmain, rcd_info.account_main);
	if (pacc == NULL) {
		// can not found at main account
		pacc = search_no2(cardno, paccsw, rcd_info.account_sw);
	} else {
		if ((pacc->feature & 0xC0) == 0xC0) {
			// 这是一张换过的卡
			pacc = NULL;
			pacc = search_no2(cardno, paccsw, rcd_info.account_sw);
		}
	}
	return pacc;
}

#ifdef TS11
/*
 * terminal type: 1-byte; le_card once consume: 1-byte; food price: 32-byte; id info: 32-byte
 * return 0 is ok; -1 is send error; -2 is variable error
 */
int send_run_data(term_ram *ptrm)
{
	int ret, i;
	//unsigned char rdata;
	// first send 1-byte terminal type (HEX)
	ret = send_data((char *)&ptrm->pterm->term_type, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		printk("send run data time out!\n");
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->term_type;
	ptrm->add_verify += (unsigned char)ptrm->pterm->term_type;
	// sencond send 1-byte max consume once (BCD)
	ret = send_data((char *)&ptrm->pterm->max_consume, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->max_consume;
	ptrm->add_verify += (unsigned char)ptrm->pterm->max_consume;
	ret = send_data((char *)ptrm->pterm->price, 32, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 32; i++) {
		ptrm->dif_verify ^= ptrm->pterm->price[i];
		ptrm->add_verify += ptrm->pterm->price[i];
	}
#ifdef CPUCARD
	// forth send 32-byte id promise info: 0xFF
	for (i = 0; i < 32; i++) {
		ret = send_byte(0xFF, ptrm->term_no);
		if (ret < 0) {		// timeout
			return ret;
		}
		ptrm->dif_verify ^= 0xFF;
		ptrm->add_verify += 0xFF;//(unsigned char)ptrm->pterm->price[i];
	}
	// receive verify data, and confirm it
	if (verify_all(ptrm, CHKCSUM) < 0)
		return -1;
#else
	if (verify_all(ptrm, CHKXOR) < 0)
		return -1;
#endif /* CPUCARD */
	return 0;
}

int recv_le_id(term_ram *ptrm, int allow)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
	tmp += 3;
	for (i = 0; i < 4; i++) {			// receive 4-byte card number
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
		tmp--;
	}
	// search id
	pacc = search_id(cardno);
#ifdef DEBUG
	printk("search over\n");
#endif
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
	} else {
		//printk("find!\n");
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
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// 此卡达到餐限
			// 此卡要输密码
			feature |= 1 << 3;
		}
///////////////////////////////////////////////////////////////////////////////////
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
				money = (unsigned long)pacc->money & 0xFFFFF;
			}
		} else {
			feature = 1;
		}
		if (money > 999999) {
			printk("money is too large!\n");
			feature = 1;
			money = 0;
		}
///////////////////////////////////////////////////////////////////////////////////
	}
	// 判断是否允许脱机使用光电卡
	if (!allow) {
#ifdef DEBUG
		printk("not allow: %d\n", cardno);
#endif
		feature = 1;
		money = 0;
	}
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	//printk("feature is %02x\n", feature);
	money = binl2bcd(money) & 0xFFFFFF;
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		tmp--;
	}
	if (verify_all(ptrm, CHKXOR) < 0)
		return -1;
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
	return 0;
}

#ifdef CPUCARD
int recv_cpu_id(term_ram *ptrm, unsigned char *tm)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
	tmp += 3;
	for (i = 0; i < 4; i++) {			// receive 4-byte card number
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			printk("recv %d cardno error, ret:%d\n", i, ret);
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		//ptrm->add_verify += *pcardno;
		tmp--;
	}
	// 将BCD转为HEX
	//cardno = bcdl2bin(cardno);
	// search id
//	printk("card no is %08x\n", cardno);
	pacc = search_id(cardno);
	//printk("search over\n");
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		//printk("not find!\n");
		feature = 1;
	} else {
		feature = 4;
		//printk("find!\n");
		if ((pacc->feature & 0xC0) == 0x80) {
			// 这是挂失卡
			feature |= 1 << 5;
		}
		//if (pacc->feature & 0xC0 == 0x40 || pacc->feature & 0xF == 0) {
			// 此卡达到餐限
			// 此卡要输密码
		//	feature |= 1 << 3;
		//}
		//if (ptrm->pterm->power_id == 0xF0 || (pacc->feature & 0x30) == 0
		//	|| (ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4)) {
			//money[0] = pacc->money & 0xFF;
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
			//money = pacc->money & 0xFFFFF;
		//} else {
		//	feature = 1;
		///}
	}
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	//ptrm->add_verify += feature;
	if (send_data(tm, 6, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 6; i++) {
		//ptrm->add_verify += tm[i];
		ptrm->dif_verify ^= tm[i];
	}
	if (verify_all(ptrm, CHKNXOR) < 0)
		return -1;
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
	return 0;
}
#endif
/*
 * the function is only receive pos number, and send dis_virify
 * created by wjzhe, 10/19/2006
 */
int ret_no_only(term_ram *ptrm)
{
//	unsigned char tmp;
	int ret;
	/*ret = recv_data((char *)&tmp, 1, ptrm->pterm->term_num);
	if (ret < 0) {
		if (ret == -1)
			ptrm->status = -1;
		if (ret == -2)
			ptrm->status = -2;
		return ret;
	}
	if (ptrm->dif_verify == tmp) {
		ret = send_byte(0, ptrm->pterm->term_num);
	} else {
		ret = send_byte(0xF, ptrm->pterm->term_num);
	}*/
	ret = verify_all(ptrm, CHKXOR);
	return ret;
}
/*
 * check, return 0 is ok
 * created by wjzhe, 10/19/2006
 */
int verify_all(term_ram *ptrm, unsigned char n)
{
	int ret = 0;
	unsigned char tmp[2];
	//printk("start recv chksum data:%x\n", n);
	switch (n) {
	case CHKXOR:
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
			}
			send_byte5(0xFF, ptrm->term_no);
			ret = -1;
			break;
		}
		if (ptrm->dif_verify == *tmp) {
			ret = send_byte5(0, ptrm->term_no);
		} else {
			send_byte5(0xFF, ptrm->term_no);
			ret = -1;
		}
		break;
	case CHKCSUM:
		//printk("start recv chksum data");
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
			}
			send_byte(0xFF, ptrm->term_no);
			ret = -1;
			//printk("not recv chksum data\n");
			break;
		}
		if (ptrm->add_verify == tmp[0]) {
			ret = send_byte(0, ptrm->term_no);
		} else {
			send_byte(0xFF, ptrm->term_no);
			ret = -1;
		}
		//printk("add verify: %d, recv: %d", ptrm->add_verify, tmp[0]);
		break;
	case CHKALL:
		ret = recv_data((char *)&tmp[0], ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
			}
			usart_delay(1);
			send_byte5(0xFF, ptrm->term_no);
			ret = -1;
			break;
		}
		ret = recv_data((char *)&tmp[1], ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
			}
			send_byte5(0xFF, ptrm->term_no);
			ret = -1;
			break;
		}
		if (ptrm->add_verify == tmp[0] && ptrm->dif_verify == tmp[1]) {
			ret = send_byte5(0x55, ptrm->term_no);
		} else {
			send_byte5(0xFF, ptrm->term_no);
#ifdef DEBUG
			printk("chk failed: %02x, %02x, %02x, %02x\n",
				ptrm->add_verify, ptrm->dif_verify, tmp[0], tmp[1]);
#endif
			ret = -1;
		}
		break;
	case CHKNXOR:
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
			}
			send_byte5(0xFF, ptrm->term_no);
			ret = -1;
			break;
		}
		if (ptrm->dif_verify == *tmp) {
			ret = send_byte5(0x55, ptrm->term_no);
		} else {
			send_byte5(0xFF, ptrm->term_no);
			ret = -1;
		}
		break;
	default:
#ifdef DEBUG
		printk("chksum cmd error%x\n", n);
#endif
		ret = -EINVAL;
		break;
	}
	return ret;
}
/*
 * receive le card flow, flag is 0 or 1; if flag is 0, consum is HEX; or is BCD
 * but store into flow any data is HEX
 */
int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int limit, tail;
	unsigned char *tmp;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	// first recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
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
		//ptrm->add_verify += *tmp;
		tmp--;
	}
	// second recv 2-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 1;		//sizeof(leflow.consume_sum) - 1;
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
		ptrm->dif_verify ^= *tmp;
		//ptrm->add_verify += *tmp;
		tmp--;
	}
	//leflow.consume_sum &= 0xFFFF;
	if (flag)		// if BCD then convert BIN
		leflow.consume_sum = bcdl2bin(leflow.consume_sum);
	// third check NXOR
	if (flag) {
		if (verify_all(ptrm, CHKXOR) < 0)
			return -1;
	} else {
		if (verify_all(ptrm, CHKNXOR) < 0)
			return -1;
	}
/*	if (verify_all(ptrm, CHKNXOR) < 0)
		return -1;
*/
	// check if receive the flow
	if (ptrm->flow_flag)
		return 0;
	space_remain--;
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
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
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}
	pacc->feature &= 0xF0;
	pacc->feature |= limit;
	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
	leflow.flow_type = 0xAB;
	leflow.date.hyear = 0x20;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm++;
	leflow.tml_num = ptrm->term_no;
	leflow.acc_num = pacc->acc_num;
	// 流水号的处理
	//flow_no = maxflowno;
	leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// 流水区头尾的处理
	tail = flowptr.tail;
	//printk("recv le flow tail: %d\n", tail);
	memcpy(pflow + tail, &leflow, sizeof(flow));
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	return 0;
}
#ifdef CPUCARD
int recv_cpuflow(term_ram *ptrm)
{
	cpu_flow cpuflow;
	int ret, i;
	acc_ram *pacc;
	unsigned char nouse[6];
	//unsigned char flow_data[43] = {0};
	unsigned char *tmp;
	int tail;
	//////////////////////////
	ptrm->add_verify = 0;
	//////////////////////////
	// init leflow
	// cpuflow flow_type is send by pos
/*	for (i = 0; i < 43; i++) {
		if (recv_data((char *)&flow_data[i], ptrm->term_no) < 0) {
			printk("recv cpu flow failed\n");
			return -1;
		}
		ptrm->dif_verify ^= flow_data[i];
		ptrm->add_verify += flow_data[i];
	}
	if (verify_all(ptrm, CHKALL) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}*/
	memset(&cpuflow, 0, sizeof(cpuflow));
	cpuflow.tml_num = ptrm->term_no;
	// first recv 4-byte consume money 
	tmp = (unsigned char *)&cpuflow.consume_sum;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(43 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// second recv 1 bye trade type
	tmp = (unsigned char *)&cpuflow.trade_type;
	ret = recv_data((char *)tmp, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = -1;
		}
		if (ret == -2) {		// other errors
			ptrm->status = -2;
			usart_delay(39 - i);
		}
		return ret;
	}
	ptrm->dif_verify ^= cpuflow.trade_type;
	ptrm->add_verify += cpuflow.trade_type;
	// third recv 6 byte term machine num
	tmp = (unsigned char *)cpuflow.term_num;
	//tmp += 5;
	for (i = 0; i < 6; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(38 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// fourth recv 4-byte term trade num
	tmp = (unsigned char *)&cpuflow.term_trade_num;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(32 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// fifth recv date 7-byte
	tmp = (unsigned char *)&cpuflow.date.hyear;
	//tmp += 6;
	for (i = 0; i < 7; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM) {
				// 此处应做延迟处理
				usart_delay(28 - i);
			}
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	cpuflow.date.hyear = 0x20;
	// sixth recv 4-byte TAC code
	tmp = (unsigned char *)&cpuflow.tac_code;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(21 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// seventh recv 10-byte card number
	tmp = nouse;
	for (i = 0; i < 6; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(17 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		//tmp++;
	}
	tmp = (unsigned char *)&cpuflow.card_num;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(11 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// 将收到的卡号换成HEX
	//cpuflow.card_num = bcdl2bin(cpuflow.card_num);
	
	// eighth recv 2-byte purse trade num
	tmp = (unsigned char *)&cpuflow.purse_num;
	tmp++;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(7 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// nighth recv 4-byte purse money
	tmp = (unsigned char *)&cpuflow.purse_money;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(5 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// tenth recv 1-byte flow type
	ret = recv_data((char *)&cpuflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = -1;
		}
		if (ret == -2) {		// other errors
			ptrm->status = -2;
			usart_delay(1);
		}
		return ret;
	}
	ptrm->dif_verify ^= cpuflow.flow_type;
	ptrm->add_verify += cpuflow.flow_type;
	// next check ALL
	if (verify_all(ptrm, CHKALL) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	// check if receive the flow
	if (ptrm->flow_flag)
		return 0;
	// search id
	pacc = search_id(cpuflow.card_num);
	if (pacc == NULL) {
		//printk("no cardno: %08x", cpuflow.card_num);
		return 0;
	}
	// adjust money limit----cpu卡不做餐限处理
	// set ptrm->flow_flag
	space_remain--;
	ptrm->flow_flag = ptrm->term_no;
	// term_money plus
	if (cpuflow.flow_type == 0x99) {
		ptrm->term_money += cpuflow.consume_sum;
		ptrm->term_cnt++;
	}
	// 流水号的处理
	cpuflow.flow_num = maxflowno++;
	//printk("recv cpu flow tail: %d\n", tail);
	tail = flowptr.tail;
	memcpy(pflow + tail, &cpuflow, sizeof(flow));
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	return 0;
}
#endif/* CPUCARD */
#endif/* TS11 */
unsigned long bcdl2bin(unsigned long bcd)
{
	int i, bin = 0, j, tmp;
	for (i = 0; i < (sizeof(bcd) * 2); i++) {
		tmp = (bcd & 15);
		for (j = 0; j < i; j++)
			tmp *= 10;
		bin += tmp;
		bcd >>= 4;
	}
	return bin;
}

#ifdef TS11
int recv_take_money(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i, tail;
	acc_ram *pacc;
	unsigned char *tmp;
	memset(&leflow, 0, sizeof(leflow));
	// judge terminal property
	if (!(ptrm->pterm->term_type & 0x20)) {	// 断断是否为出纳机
		// 这不是出纳机
		//printk("this is not cash terminal\n");
		usart_delay(7);
		return 0;
	}
	// first recv 4-byte card number and two-byte money
	tmp = (unsigned char *)&leflow.card_num;
	tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
				usart_delay(7 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		//ptrm->add_verify += *tmp;
		tmp--;
	}
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 1;
	for (i = 0; i < 2; i++) {		//接收2bytes取款额（bcd码）
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM)
				usart_delay(3 - i);
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		tmp--;
	}
	if (verify_all(ptrm, CHKXOR) < 0) {
		ptrm->status = NOCOM;
		return 0;
	}
	// 判断终端是否能接收流水
	if (ptrm->flow_flag) {
		ptrm->status = TNORMAL;
		return 0;
	}
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	space_remain--;
	// change to hex
	leflow.consume_sum = bcdl2bin(leflow.consume_sum) * 100;
	// 只做余额调整, 餐限不调整
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!\n");
		pacc->money = 0;
	}
	// 终端参数设置
	ptrm->term_money -= leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// 禁止再次接收流水
	// 整理并存储流水
	leflow.flow_type = 0xAD;
	leflow.acc_num = pacc->acc_num;
	leflow.date.hyear = 0x20;
	tmp = &leflow.date.lyear;
	for (i = 0; i < 6; i++) {
		*tmp = *tm;
		tmp--;
		tm++;
	}
	leflow.tml_num = ptrm->term_no;
	// 处理流水号
	leflow.flow_num = maxflowno++;
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// 修改流水指针
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	return 0;
}

int recv_dep_money(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i, tail, managefee;
	acc_ram *pacc;
	unsigned char *tmp;
	// judge terminal property
	//printk("term_type is %02x\n", ptrm->pterm->term_type);
	if (!(ptrm->pterm->term_type & 0x20)) {	// 断断是否为出纳机
		// 这不是出纳机
		//printk("this is not cash terminal\n");
		usart_delay(7);
		return 0;
	}
	memset(&leflow, 0, sizeof(leflow));
	// first recv 4-byte card number and two-byte money
	tmp = (unsigned char *)&leflow.card_num;
	tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
				usart_delay(7 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		//ptrm->add_verify += *tmp;
		tmp--;
	}
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 1;
	for (i = 0; i < 2; i++) {		//接收2bytes取款额（bcd码）
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM)
				usart_delay(3 - i);
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		tmp--;
	}
	if (verify_all(ptrm, CHKXOR) < 0) {
		ptrm->status = NOCOM;
		return 0;
	}
	if (ptrm->flow_flag) {
		printk("not allow recv flow\n");
		ptrm->status = TNORMAL;
		return 0;
	}
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	space_remain--;
	// change to hex
	//printk("consume_sum: %04x\n", leflow.consume_sum);
	leflow.consume_sum = bcdl2bin(leflow.consume_sum) * 100;
	//printk("consume_sum: %04x, %d\n", leflow.consume_sum, leflow.consume_sum);
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
	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// 禁止再次接收流水
	// 整理并存储流水
	leflow.flow_type = 0xAC;
	leflow.acc_num = pacc->acc_num;
	leflow.date.hyear = 0x20;
	tmp = &leflow.date.lyear;
	for (i = 0; i < 6; i++) {
		*tmp = *tm;
		tmp--;
		tm++;
	}
	// 流水号的处理
	leflow.flow_num = maxflowno++;
	leflow.tml_num = ptrm->term_no;
	leflow.manage_fee = managefee * (-1);
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// 修改流水指针
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	return 0;
}
#endif/* TS11 */

/*
 * 处理异区现金流水:
 * 只会修改账户区或换卡区账户金额
 */
int deal_money(money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd)
{
	acc_ram *pacc = NULL;
	if (pnflow == NULL || pacc_m == NULL || prcd == NULL) {
		return -1;
	}
	pacc = search_no2(pnflow->CardNo, pacc_m, prcd->account_main);
	if (pacc) {
		// 找到了, 但不清楚是不是换过的卡
		if ((pacc->feature & 0xC0) == 0xC0) {
			// 卡片是换过的, 还需要搜索换卡区
			printk("card is changed!\n");
			pacc = NULL;
			goto find2;
		}
	}
	if (pacc == NULL) {
		printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->CardNo);
find2:
		pacc = search_no2(pnflow->CardNo, pacc_s, prcd->account_sw);
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->CardNo);
		}
	}
	if (pacc == NULL) {
		printk("can not find card no: %08x\n", (unsigned int)pnflow->CardNo);
		return 0;
	}
	pacc->money += pnflow->Money;//=================
	return 0;
}


int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd)
{
	acc_ram *pacc;
	unsigned char tmp;
	acc_ram oldacc;
	int cnt;
	if (pnflow == NULL || pacc_m == NULL || pacc_s == NULL) {
		return -1;
	}
	switch (pnflow->Type) {
	case NOM_LOSS_CARD:
	case NOM_LOGOUT_CARD:
		pacc = search_no2(pnflow->CardId, pacc_m, prcd->account_main);
		if (pacc) {
			// 找到了, 但不清楚是不是换过的卡
			if ((pacc->feature & 0xC0) == 0xC0) {
				// 卡片是换过的, 还需要搜索换卡区
				printk("card is changed!\n");
				pacc = NULL;
				goto find20;
			}
		}
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->CardId);
find20:
			pacc = search_no2(pnflow->CardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->CardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->CardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		pacc->feature |= 0x80;
		break;
	case NOM_FIND_CARD:
		pacc = search_no2(pnflow->CardId, pacc_m, prcd->account_main);
		if (pacc) {
			// 找到了, 但不清楚是不是换过的卡
			if ((pacc->feature & 0xC0) == 0xC0) {
				// 卡片是换过的, 还需要搜索换卡区
				printk("card is changed!\n");
				pacc = NULL;
				goto find21;
			}
		}
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->CardId);
find21:
			pacc = search_no2(pnflow->CardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->CardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->CardId);
			return 0;
		}
		pacc->feature &= 0x7F;
		break;
	case NOM_SET_THIEVE:
		pacc = search_no2(pnflow->CardId, pacc_m, prcd->account_main);
		if (pacc) {
			// 找到了, 但不清楚是不是换过的卡
			if ((pacc->feature & 0xC0) == 0xC0) {
				// 卡片是换过的, 还需要搜索换卡区
				printk("card is changed!\n");
				pacc = NULL;
				goto find22;
			}
		}
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->CardId);
find22:
			pacc = search_no2(pnflow->CardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->CardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->CardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		pacc->feature |= 0x40;
		break;
	case NOM_CLR_THIEVE:
		pacc = search_no2(pnflow->CardId, pacc_m, prcd->account_main);
		if (pacc) {
			// 找到了, 但不清楚是不是换过的卡
			if ((pacc->feature & 0xC0) == 0xC0) {
				// 卡片是换过的, 还需要搜索换卡区
				printk("card is changed!\n");
				pacc = NULL;
				goto find23;
			}
		}
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->CardId);
find23:
			pacc = search_no2(pnflow->CardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->CardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->CardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		//pacc->feature |= 0x40;
		break;
	case NOM_CHANGED_CARD:
		prcd->account_sw += 1;
		if (prcd->account_sw > MAXSWACC) {
			prcd->account_sw--;
			printk("change card area full!\n");
			return -1;
		}
		pacc = search_no2(pnflow->CardId, pacc_m, prcd->account_main);
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->CardId);
			printk("this operation is change card, so return\n");
			prcd->account_sw--;
			return 0;
		}
		oldacc.feature = pacc->feature;
		oldacc.acc_num = pacc->acc_num;
		oldacc.card_num = pnflow->NewCardId;
		oldacc.managefee = pacc->managefee - 1;
		oldacc.money = pacc->money;
		pacc->feature &= 0x3F;
		pacc->feature |= 0xC0;
		prcd->account_all += 1;
		if (sort_sw(pacc_s, &oldacc, (prcd->account_sw - 1)) < 0)
			return -1;
		break;
	case NOM_CURRENT_RGE:
		// 新加的卡不能是已注册的卡
		if (search_id(pnflow->CardId)) {
			printk("new regist card exist\n");
			return -1;
		}
		prcd->account_sw += 1;
		if (prcd->account_sw >= MAXSWACC) {
			prcd->account_sw--;
			printk("change card area full!\n");
			return -1;
		}
		oldacc.card_num = pnflow->CardId;
		oldacc.acc_num = pnflow->AccountId;
		oldacc.managefee = pnflow->ManageId - 1;
		oldacc.money = pnflow->RemainMoney;
		tmp = pnflow->UpLimitMoney & 0xF;
		pnflow->PowerId --;
		tmp |= (pnflow->PowerId & 0x3) << ACCOUNTCLASS;// 身份
		switch (pnflow->Flag) {		// 挂失标志
		case 1:
			tmp |= 0x10 << ACCOUNTFLAG;
			break;
		case 7:
			tmp |= 0x0 << ACCOUNTFLAG;
			break;
		//case 6:
		//	break;
		default:
			tmp |= 0x1 << ACCOUNTFLAG;
			break;
		}
		oldacc.feature = tmp;
		if (sort_sw(pacc_s, &oldacc, (prcd->account_sw - 1)) < 0) {
			printk("add new account failed!\n");
			return 0;
		}
		prcd->account_all += 1;
		break;
	case NOM_BLACK:
		// 新的黑名单数据, 加在黑名单库后面
		if (pblack == NULL) {
			printk("system error: black list ptr is NULL\n");
			return 0;
		}
		if (blkinfo.count >= (MAXBACC - 1)) {
			printk("has no black space: count is %d\n", blkinfo.count);
			return 0;
		}
		cnt = blkinfo.count;
		nomtoblk(&pblack[cnt], pnflow);
		memcpy(&blkinfo.trade_num, &pblack[cnt].t_num, sizeof(blkinfo.trade_num));
		cnt++;
#if 0
		printk("pblack[%d] %d %d", cnt, pblack[cnt].opt, pblack[cnt].t_num.num);
#endif
#if 0
		printk("%d", cnt);
#endif
		blkinfo.count = cnt;
		break;
	default:
		break;
	}
	return 0;
}
int sort_sw(acc_ram *pacc_s, acc_ram *pacc, int cnt)
{
	int i, n;
	if (cnt >= (MAXSWACC - 1))
		return -1;
	for (i = 0; i < cnt; i++) {
		if (pacc->card_num < pacc_s[i].card_num)
			break;
	}
	n = i;
	for (i = cnt; i > n; i--) {
		//memcpy(&pacc_s[i], &pacc_s[i - 1], sizeof(acc_ram));
		pacc_s[i] = pacc_s[i - 1];
	}
	//memcpy(&pacc_s[n], pacc, sizeof(acc_ram));
	pacc_s[n] = *pacc;
	return 0;
}

/*
 * 将异区非现金流水转为黑名单结构
 */
int nomtoblk(black_acc *pbacc, no_money_flow *pnflow)
{
	usr_time tm;
	memset(pbacc, 0, sizeof(black_acc));
	// 为了提高效率不做参数正确性验证
	pbacc->card_num = pnflow->CardId;
	pbacc->opt = (unsigned char)pnflow->BLTypeId;
	if (ctoutm(&tm, pnflow->FlowTime) < 0)
		printk("blk time error: %s\n", pnflow->FlowTime);
	pbacc->t_num.s_tm.hyear = tm.hyear;
	pbacc->t_num.s_tm.lyear = tm.lyear;
	pbacc->t_num.s_tm.mon = tm.mon;
	pbacc->t_num.s_tm.day = tm.mday;
	pbacc->t_num.s_tm.hour = tm.hour;
	pbacc->t_num.num = pnflow->OperationID;
	return 0;
}
// 2006-12-2 3:12:54
// 0123456789
int ctoutm(usr_time *putm, char *tm)
{
	int tmp;
	if (putm == NULL || tm == NULL)
		return -1;
	memset(putm, 0, sizeof(usr_time));
	tmp = 0;
	while ((*tm <= '9') && (*tm >= '0')) {
		tmp *= 10;
		tmp += *tm - '0';
		tm++;
	}
	tm++;
	if (tmp < 2000) {
		printk("year must above 2000!\n");
		return -1;
	}
	putm->hyear = 0x20;
	tmp -= 2000;
	putm->lyear = BIN2BCD(tmp & 0xFF);
	tmp = 0;
	while ((*tm <= '9') && (*tm >= '0')) {
		tmp *= 10;
		tmp += *tm - '0';
		tm++;
	}
	tm++;
	if (tmp < 1 || tmp > 12) {
		printk("month error: %d\n", tmp);
		return -1;
	}
	putm->mon = BIN2BCD(tmp);
	tmp = 0;
	while ((*tm <= '9') && (*tm >= '0')) {
		tmp *= 10;
		tmp += *tm - '0';
		tm++;
	}
	tm++;
	if (tmp < 1 || tmp > 31) {
		printk("month error: %d\n", tmp);
		return -1;
	}
	putm->mday = BIN2BCD(tmp);
	tmp = 0;
	while ((*tm <= '9') && (*tm >= '0')) {
		tmp *= 10;
		tmp += *tm - '0';
		tm++;
	}
	tm++;
	if (tmp < 0 || tmp > 23) {
		printk("month error: %d\n", tmp);
		return -1;
	}
	putm->hour = BIN2BCD(tmp);
	tmp = 0;
	while ((*tm <= '9') && (*tm >= '0')) {
		tmp *= 10;
		tmp += *tm - '0';
		tm++;
	}
	tm++;
	if (tmp < 0 || tmp > 59) {
		printk("month error: %d\n", tmp);
		return -1;
	}
	putm->min = BIN2BCD(tmp);
	tmp = 0;
	while ((*tm <= '9') && (*tm >= '0')) {
		tmp *= 10;
		tmp += *tm - '0';
		tm++;
	}
	tm++;
	if (tmp < 0 || tmp > 59) {
		printk("month error: %d\n", tmp);
		return -1;
	}
	putm->sec = BIN2BCD(tmp);
	return 0;
}

#if 0
/*
 * 发送M1卡终端上电参数
 * write by wjzhe, Apr. 11 2007
 */
int send_terminfo(term_ram *ptrm)
{
	int ret, i;
	//unsigned char rdata;
	// first send 1-byte terminal type (HEX)
	ret = send_data((char *)&ptrm->pterm->term_type, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		printk("send run data time out!\n");
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->term_type;
	ptrm->add_verify += (unsigned char)ptrm->pterm->term_type;
	// sencond send 1-byte max consume once (BCD)
	ret = send_data((char *)&ptrm->pterm->max_consume, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->max_consume;
	ptrm->add_verify += (unsigned char)ptrm->pterm->max_consume;
	ret = send_data((char *)ptrm->pterm->price, 32, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 32; i++) {
		ptrm->dif_verify ^= ptrm->pterm->max_consume;
		ptrm->add_verify += ptrm->pterm->price[i];
	}
	// receive verify data, and confirm it
	if (verify_all(ptrm, CHKCSUM) < 0)
		return -1;
	return 0;
}

/*
 * 接收M1卡流水, 电子钱包流水
 * write by wjzhe, Apr. 10 2007
 */
int recv_m1flow(term_ram *ptrm)
{
	m1_flow m1flow;
	int ret, i;
	acc_ram *pacc;
	unsigned char nouse[6];
	//unsigned char flow_data[43] = {0};
	unsigned char *tmp;
	int tail;
	//////////////////////////
	memset(&m1flow, 0, sizeof(m1flow));
	ptrm->add_verify = 0;

	// first recv 4-byte consume money 
	tmp = (unsigned char *)&cpuflow.consume_sum;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(43 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// second recv 1 bye trade type
	tmp = (unsigned char *)&cpuflow.trade_type;
	ret = recv_data((char *)tmp, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = -1;
		}
		if (ret == -2) {		// other errors
			ptrm->status = -2;
			usart_delay(39 - i);
		}
		return ret;
	}
	ptrm->dif_verify ^= cpuflow.trade_type;
	ptrm->add_verify += cpuflow.trade_type;
	// third recv 6 byte term machine num
	tmp = (unsigned char *)cpuflow.term_num;
	//tmp += 5;
	for (i = 0; i < 6; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(38 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp++;
	}
	// fourth recv 4-byte term trade num
	tmp = (unsigned char *)&cpuflow.term_trade_num;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(32 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// fifth recv date 7-byte
	tmp = (unsigned char *)&cpuflow.date.hyear;
	//tmp += 6;
	for (i = 0; i < 7; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM) {
				// 此处应做延迟处理
				usart_delay(28 - i);
			}
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	cpuflow.date.hyear = 0x20;
	// sixth recv 4-byte TAC code
	tmp = (unsigned char *)&cpuflow.tac_code;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(21 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// seventh recv 10-byte card number
	tmp = nouse;
	for (i = 0; i < 6; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(17 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		//tmp++;
	}
	tmp = (unsigned char *)&cpuflow.card_num;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(11 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// 将收到的卡号换成HEX
	//cpuflow.card_num = bcdl2bin(cpuflow.card_num);
	
	// eighth recv 2-byte purse trade num
	tmp = (unsigned char *)&cpuflow.purse_num;
	tmp++;
	for (i = 0; i < 2; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(7 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// nighth recv 4-byte purse money
	tmp = (unsigned char *)&cpuflow.purse_money;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {		// timeout, pos is not exist
				ptrm->status = -1;
			}
			if (ret == -2) {		// other errors
				ptrm->status = -2;
				usart_delay(5 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// tenth recv 1-byte flow type
	ret = recv_data((char *)&cpuflow.flow_type, ptrm->term_no);
	if (ret < 0) {
		if (ret == -1) {		// timeout, pos is not exist
			ptrm->status = -1;
		}
		if (ret == -2) {		// other errors
			ptrm->status = -2;
			usart_delay(1);
		}
		return ret;
	}
	ptrm->dif_verify ^= cpuflow.flow_type;
	ptrm->add_verify += cpuflow.flow_type;
	// next check ALL
	if (verify_all(ptrm, CHKALL) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	// check if receive the flow
	if (ptrm->flow_flag)
		return 0;
	// search id
	pacc = search_id(cpuflow.card_num);
	if (pacc == NULL) {
		//printk("no cardno: %08x", cpuflow.card_num);
		return 0;
	}
	// adjust money limit----cpu卡不做餐限处理
	// set ptrm->flow_flag
	space_remain--;
	ptrm->flow_flag = ptrm->term_no;
	// term_money plus
	if (cpuflow.flow_type == 0x99) {
		ptrm->term_money += cpuflow.consume_sum;
		ptrm->term_cnt++;
	}
	// 流水号的处理
	cpuflow.flow_num = maxflowno++;
	//printk("recv cpu flow tail: %d\n", tail);
	tail = flowptr.tail;
	memcpy(pflow + tail, &cpuflow, sizeof(flow));
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	return 0;
}

#endif
