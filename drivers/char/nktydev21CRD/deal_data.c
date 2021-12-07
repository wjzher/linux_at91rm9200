/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�deal_data.c
 * ժ    Ҫ�����֧��TS11�����ݴ�������
 *			 ֧���µ��ն˻�, ��TS11��������ȥ��
 *			 ֧��2.1C����������ˮ��������
 * 			 
 * ��ǰ�汾��2.3
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��9��10��
 *
 * ȡ���汾��2.2
 * ԭ����  ��wjzhe
 * ������ڣ�2007��8��13��
 *
 * ȡ���汾��2.0 
 * ԭ����  ��wjzhe
 * ������ڣ�2007��6��6��
 *
 * ȡ���汾��1.0 
 * ԭ����  ��wjzhe
 * ������ڣ�2006��12��3��
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
		printk("binl2bcd parameter %d error!\n", (int)val);
		return /* -1 */0;
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
acc_ram *search_acc(unsigned long accno, acc_ram *pacc, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++) {
		if (pacc[i].acc_num == accno) {
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
			// ����һ�Ż����Ŀ�
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
		feature = 1;//feature=1��ʾ��ֹ����
	} else {
		//printk("find!\n");
		feature = 4;
		//if (ptrm->pterm->term_type & 0x20) {
		//	feature = 0;
		//} else {
		//	feature = 4;
		//}
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ( (ptrm->pterm->power_id == 0xF0) ||
			 ((pacc->feature & 0x30) == 0) ||
			 ((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			 ) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1�����ݻ��ʻ��������ն����Ͷ�Ӧ��������������
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
	// �ж��Ƿ������ѻ�ʹ�ù�翨
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
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
	// ��BCDתΪHEX
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
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		//if (pacc->feature & 0xC0 == 0x40 || pacc->feature & 0xF == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
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
		// ��Ҫ��������
		limit -= leflow.consume_sum / 100 + 1;//��100����Ϊ���ѽ�����С�����ĽǺͷ�
		if (!(leflow.consume_sum % 100)) {// �պ���100ʱ, ��Ϊ��һ��
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
	// ��ˮ�ŵĴ���
	//flow_no = maxflowno;
	leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// ��ˮ��ͷβ�Ĵ���
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
				// �˴�Ӧ���ӳٴ���
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
	// ���յ��Ŀ��Ż���HEX
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
	// adjust money limit----cpu���������޴���
	// set ptrm->flow_flag
	space_remain--;
	ptrm->flow_flag = ptrm->term_no;
	// term_money plus
	if (cpuflow.flow_type == 0x99) {
		ptrm->term_money += cpuflow.consume_sum;
		ptrm->term_cnt++;
	}
	// ��ˮ�ŵĴ���
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
	if (!(ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
		// �ⲻ�ǳ��ɻ�
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
	for (i = 0; i < 2; i++) {		//����2bytesȡ��bcd�룩
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
	// �ж��ն��Ƿ��ܽ�����ˮ
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
	// ֻ��������, ���޲�����
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!\n");
		pacc->money = 0;
	}
	// �ն˲�������
	ptrm->term_money -= leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �������洢��ˮ
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
	// ������ˮ��
	leflow.flow_num = maxflowno++;
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// �޸���ˮָ��
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
	if (!(ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
		// �ⲻ�ǳ��ɻ�
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
	for (i = 0; i < 2; i++) {		//����2bytesȡ��bcd�룩
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
	// �ָ�����
	if ((pacc->feature & 0xF) != 0xF) {
		pacc->feature &= 0xF0;
		pacc->feature |= 0xE;
	}
	managefee = fee[pacc->managefee & 0xF];		//�����ѱ���
	managefee = managefee * leflow.consume_sum / 100;	//���������
	//printk("pacc->money: %d, managefee: %d\n", pacc->money, managefee);
	pacc->money += leflow.consume_sum - managefee;		//����Ǯ���ܷ�
	//printk("pacc->money: %d\n", pacc->money);
	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �������洢��ˮ
	leflow.flow_type = 0xAC;
	leflow.acc_num = pacc->acc_num;
	leflow.date.hyear = 0x20;
	tmp = &leflow.date.lyear;
	for (i = 0; i < 6; i++) {
		*tmp = *tm;
		tmp--;
		tm++;
	}
	// ��ˮ�ŵĴ���
	leflow.flow_num = maxflowno++;
	leflow.tml_num = ptrm->term_no;
	leflow.manage_fee = managefee * (-1);
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// �޸���ˮָ��
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	return 0;
}
#endif/* TS11 */

#ifdef QINGHUA
/*
 * terminal type: 1-byte; le_card once consume: 1-byte; food price: 32-byte; id info: 32-byte
 * return 0 is ok; -1 is send error; -2 is variable error
 * terminal info is new
 */
int send_run_data(term_ram *ptrm)
{
	int ret, i;
	unsigned char tmp;
	char code[32] = {0};
	// ��Ʒ�ֶ�Ʒ���ж�
	// ��Ϊֻ�г��ɻ�, ����û��Ʒ��֮��
	// first send 1-byte terminal type (HEX)
	//printk("%d type %02x\n", ptrm->term_no, ptrm->pterm->param.term_type);
	tmp = ptrm->pterm->param.term_type;
	//tmp &= ~(1 << MESS_TERMINAL_FLAG);
	//tmp |= (1 << CASH_TERMINAL_FLAG);
	//ptrm->pterm->param.term_type |= (1 << CASH_TERMINAL_FLAG);
	ret = send_data((char *)&tmp, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		printk("send run data time out!\n");
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)tmp;
	ptrm->add_verify += (unsigned char)tmp;
	// sencond send 1-byte max consume once (BCD)
	tmp = BIN2BCD(ptrm->pterm->param.max_consume);
	ret = send_data((char *)&tmp, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)tmp;
	ptrm->add_verify += (unsigned char)tmp;
	if (ptrm->pterm->param.term_type & (1 << CASH_TERMINAL_FLAG)) {
		// 4 �ֽڵ��ն�����
		code[0] = ptrm->pterm->param.term_passwd[0];// & 0xF;
		//code[1] = (ptrm->pterm->param.term_passwd[0] & 0xF0) >> 4;
		code[1] = ptrm->pterm->param.term_passwd[1];// & 0xF;
		//printk("cash terminal %d: passwd %02x%02x, original %02x%02x\n", ptrm->term_no, code[0], code[1],
		//	ptrm->pterm->param.term_passwd[0], ptrm->pterm->param.term_passwd[1]);
		//code[3] = (ptrm->pterm->param.term_passwd[1] & 0xF0) >> 4;
	} else {
		// ��СƱ����
		memcpy(code, ptrm->pterm->price, sizeof(code));
	}
	ret = send_data((char *)code/*ptrm->pterm->price*/, 32, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 32; i++) {
		ptrm->dif_verify ^= code[i];
		ptrm->add_verify += code[i];
	}
	if (verify_all(ptrm, CHKXOR) < 0) {
#if 0
		printk("term verify_all failed\n");
		printk("term %d send %02x, %02x, %02x%02x\n",
			ptrm->term_no, ptrm->pterm->param.term_type, tmp, code[0], code[1]);
#endif
		return -1;
	}
	return 0;
}

int recv_le_id(term_ram *ptrm, int allow, unsigned char *tm)
{
	static int itm;//write by duyy, 2013.7.1
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.7.1
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
	itm = _cal_itmofday(&tm[1]);//write by duyy, 2013.7.1
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
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
#if 0
		// �����жϴ�СƱ
		if (pacc->managefee & TICKETBIT) {
			// ����˻��Ǹ���
			money = 1000;
			goto over;
		}
#endif
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) { 
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
///////////////////////////////////////////////////////////////////////////////////
	#if 0//deleted by duyy, 2013.7.1
		if ( (ptrm->pterm->power_id == 0xF0) ||
			 ((pacc->feature & 0x30) == 0) ||
			 ((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			 ) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1�����ݻ��ʻ��������ն����Ͷ�Ӧ��������������
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
	#endif
//////////////////////////////////////////////////////////////////////////////////		
		//write by duyy, 2013.7.1
		if (pacc->money < 0) {
			money = 0;
		} else {
			money = (unsigned long)pacc->money & 0xFFFFF;
		}
		if (!(ptrm->pterm->param.term_type & 0x20)) {	//���ɻ����ж�ʱ�Ρ���������ƥ�䣬��ֹ��������
			timenum = ptmnum[pacc->id_type][ptrm->pterm->power_id];
			//printk("cardno:%ld timenum = %x\n", cardno, timenum);
			if (timenum & 0x80){//modified by duyy, 2013.3.28,���λbit7��ʾ��ֹ���ѣ�����Ϊ�������ѣ��жϽ�ֹʱ��
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
/////////////////////////////////////////////////////////////////////////////////
		// ���ɻ�, ��ֹĳЩ�˴��
		if ((ptrm->pterm->param.term_type & 0x20)
			&& (pacc->managefee & DEPBIT)) {		
			if (ptrm->term_no != 10) {
				// ��10���ն˲�������
				feature = 1;
			}
		}
		if (money > 999999) {
			printk("money is too large!\n");
			feature = 1;
			money = 0;
		}
///////////////////////////////////////////////////////////////////////////////////
	}
over:
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) { //modified by duyy, 2013.7.1
		for (i = 0; i < CASHBUFSZ; i++){
			//���ж���ͬ���ɻ��ŵ���Ϣ�Ƿ��д洢
			if (ptrm->term_no ==  cashbuf[i].termno){
				//���ɻ�����ͬ������ֱ�Ӹ��Ǵ洢
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
				// ��ʱ��д���ն˽����?
				cashbuf[i].term_money = ptrm->term_money;
				return 0;
			}
		}
		// ��¼�Ƿ���
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
		// ��ʱ��д���ն˽����?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
		if (cashterm_ptr == CASHBUFSZ){
			cashterm_ptr = 0;
		}
	}
#endif
	return 0;
}
// ��СƱ���˻���Ϣ
int recv_le_id2(term_ram *ptrm, int allow)
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
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
	} else {
		//printk("find!\n");
		feature = 4;
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		// �����жϴ�СƱ
		if (pacc->managefee & TICKETBIT) {
			// ����˻��Ǹ���
			money = 1000;
		} else {
			feature = 1;
		}
	}
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
		feature = 1;
		money = 0;
	}
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;
	//printk("feature is %02x\n", feature);
	money = binl2bcd(money) & 0xFFFFFF;
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	if (verify_all(ptrm, CHKALL) < 0)
		return -1;
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		for (i = 0; i < CASHBUFSZ; i++){
			//���ж���ͬ���ɻ��ŵ���Ϣ�Ƿ��д洢
			if (ptrm->term_no ==  cashbuf[i].termno){
				//���ɻ�����ͬ������ֱ�Ӹ��Ǵ洢
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
				// ��ʱ��д���ն˽����?
				cashbuf[i].term_money = ptrm->term_money;
				return 0;
			}
		}
		// ��¼�Ƿ���
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
		// ��ʱ��д���ն˽����?
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
 * receive le card flow, flag is 0 or 1; if flag is 0, consum is HEX; or is BCD
 * but store into flow any data is HEX
 * ��СƱ����ˮ
 */
int recv_leflow2(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int tail;
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
		ptrm->add_verify += *tmp;
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
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// third check NXOR
	if (verify_all(ptrm, CHKALL) < 0) {
		return -1;
	}
	// check if receive the flow
	if (ptrm->flow_flag)
		return 0;
	// save flow...
	space_remain--;
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	// ��СƱ����
	leflow.notused[0] = 1;		// not used [0] ��ʶ��СƱ
	pacc->managefee &= ~TICKETBIT;
	leflow.consume_sum = TICKETMONEY;
	ptrm->term_cnt++;
	// init leflow
	leflow.flow_type = LECONSUME;
	leflow.acc_num = pacc->acc_num;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm++;
	leflow.tml_num = ptrm->term_no;
	// ��ˮ�ŵĴ���
	//flow_no = maxflowno;
	leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// ��ˮ��ͷβ�Ĵ���
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// ��ˮ��������?
		printk("uart 485 flow buffer no space\n");
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(pflow + tail, &leflow, sizeof(flow));
	tail++;
	//if (tail == FLOWANUM)
	//	tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	return 0;
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
		if (verify_all(ptrm, CHKXOR) < 0) {
			return -1;
		}
	} else {
		if (verify_all(ptrm, CHKNXOR) < 0) {
			return -1;
		}
	}
/*	if (verify_all(ptrm, CHKNXOR) < 0)
		return -1;
*/
	// check if receive the flow
	if (ptrm->flow_flag)
		return 0;
	// save flow...
	space_remain--;
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	// ��СƱ����
	if (pacc->managefee & TICKETBIT) {
		leflow.notused[0] = 1;		// not used [0] ��ʶ��СƱ
		pacc->managefee &= ~TICKETBIT;
		leflow.consume_sum = TICKETMONEY;
		goto over;
	}
	// adjust money limit
	limit = pacc->feature & 0xF;
	if ((limit != 0xF) && limit) { 
		// ��Ҫ��������
		limit -= leflow.consume_sum / 100 + 1;//modified by duyy,2012.2.22
		if (!(leflow.consume_sum % 100)) {// �պ���100ʱ, ��Ϊ��һ��
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
	ptrm->term_money += leflow.consume_sum;
over:
	ptrm->term_cnt++;
	// init leflow
	leflow.flow_type = LECONSUME;
	leflow.acc_num = pacc->acc_num;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm++;
	leflow.tml_num = ptrm->term_no;
	// ��ˮ�ŵĴ���
	//flow_no = maxflowno;
	leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// ��ˮ��ͷβ�Ĵ���
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// ��ˮ��������?
		printk("uart 485 flow buffer no space\n");
		return -1;
	}
	//printk("recv le flow tail: %d\n", tail);
	memcpy(pflow + tail, &leflow, sizeof(flow));
	tail++;
	//if (tail == FLOWANUM)
	//	tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	return 0;
}

/*
 * the function is only receive pos number, and send dis_virify
 * created by wjzhe, 10/19/2006
 */
int ret_no_only(term_ram *ptrm)
{
//	unsigned char tmp;
	int ret;
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
/* ����ȡ����ˮ */
int recv_take_money(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i, tail;
	acc_ram *pacc;
	unsigned char *tmp;
	memset(&leflow, 0, sizeof(leflow));
	// judge terminal property
	if (!(ptrm->pterm->param.term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
		// �ⲻ�ǳ��ɻ�
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
	for (i = 0; i < 2; i++) {		//����2bytesȡ��bcd�룩
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
	// �ж��ն��Ƿ��ܽ�����ˮ
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
	// change to hex
	leflow.consume_sum = bcdl2bin(leflow.consume_sum) * 100;
	// ֻ��������, ���޲�����
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!\n");
		pacc->money = 0;
	}
	// �ն˲�������
	ptrm->term_money -= leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �������洢��ˮ
	leflow.flow_type = LETAKE;
	leflow.acc_num = pacc->acc_num;
	leflow.date.hyear = 0x20;
	tmp = &leflow.date.lyear;
	for (i = 0; i < 6; i++) {
		tm++;
		*tmp = *tm;
		tmp--;
	}
	leflow.tml_num = ptrm->term_no;
	// ������ˮ��
	leflow.flow_num = maxflowno++;
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// ��ˮ��������?
		printk("uart 485 flow buffer no space\n");
		return -1;
	}
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// �޸���ˮָ��
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
		//modified by duyy, 2013.7.1
		for (i = 0; i < CASHBUFSZ; i++){
			//���ж���ͬ���ɻ��ŵ���Ϣ�Ƿ��д洢
			if (ptrm->term_no ==  cashbuf[i].termno){
				//���ɻ�����ͬ������ֱ�Ӹ��Ǵ洢
				cashbuf[i].accno = pacc->acc_num;
				cashbuf[i].cardno = pacc->card_num;
				cashbuf[i].feature = 0;
				cashbuf[i].managefee = leflow.manage_fee;
				cashbuf[i].money = pacc->money;
				cashbuf[i].consume = leflow.consume_sum;
				cashbuf[i].status = CASH_TAKEOFF;
				cashbuf[i].termno = ptrm->term_no;
				// �����ն˽��
				cashbuf[i].term_money = ptrm->term_money;
				return 0;
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
		// �����ն˽��
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
                if (cashterm_ptr == CASHBUFSZ){
			cashterm_ptr = 0;
		}
	}
#endif
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
	if (!(ptrm->pterm->param.term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
		// �ⲻ�ǳ��ɻ�
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
	for (i = 0; i < 2; i++) {		//����2bytesȡ��bcd�룩
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
	// change to hex
	//printk("consume_sum: %04x\n", leflow.consume_sum);
	leflow.consume_sum = bcdl2bin(leflow.consume_sum) * 100;
	//printk("consume_sum: %04x, %d\n", leflow.consume_sum, leflow.consume_sum);
	// �ָ�����
	if ((pacc->feature & 0xF) != 0xF) {
		pacc->feature &= 0xF0;
		pacc->feature |= 0xE;
	}
	managefee = fee[pacc->managefee & 0xF];		//�����ѱ���
	managefee = managefee * leflow.consume_sum / 100;	//���������
	//printk("pacc->money: %d, managefee: %d\n", pacc->money, managefee);
	pacc->money += leflow.consume_sum - managefee;		//����Ǯ���ܷ�
	//printk("pacc->money: %d\n", pacc->money);
	// �ӹ����Ѵ���
	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �������洢��ˮ
	leflow.flow_type = LECHARGE;
	leflow.acc_num = pacc->acc_num;
	leflow.date.hyear = 0x20;
	tmp = &leflow.date.lyear;
	for (i = 0; i < 6; i++) {
		tm++;
		*tmp = *tm;
		tmp--;
	}
	// ��ˮ�ŵĴ���
	leflow.flow_num = maxflowno++;
	leflow.tml_num = ptrm->term_no;
	leflow.manage_fee = managefee * (-1);
	leflow.consume_sum += leflow.manage_fee;
	tail = flowptr.tail;
	if (tail >= FLOWANUM) {
		// ��ˮ��������?
		printk("uart 485 flow buffer no space\n");
		return -1;
	}
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// �޸���ˮָ��
	tail++;
#if 0
	if (tail == FLOWANUM)
		tail = 0;
#endif
	flowptr.tail = tail;
	flow_sum++;
	space_remain--;
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
		//modified by duyy, 2013.7.1
		for (i = 0; i < CASHBUFSZ; i++){
			//���ж���ͬ���ɻ��ŵ���Ϣ�Ƿ��д洢
			if (ptrm->term_no ==  cashbuf[i].termno){
				//���ɻ�����ͬ������ֱ�Ӹ��Ǵ洢
				cashbuf[i].accno = pacc->acc_num;
				cashbuf[i].cardno = pacc->card_num;
				cashbuf[i].feature = 0;
				cashbuf[i].managefee = leflow.manage_fee;
				cashbuf[i].money = pacc->money;
				cashbuf[i].consume = leflow.consume_sum;
				cashbuf[i].status = CASH_DEPOFF;
				cashbuf[i].termno = ptrm->term_no;
				// �����ն˽��
				cashbuf[i].term_money = ptrm->term_money;
				return 0;
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
		// �����ն˽��
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
		if (cashterm_ptr == CASHBUFSZ){
			cashterm_ptr = 0;
		}
	}
#endif
	return 0;
}
#endif/* QINGHUA TS11 */

/*
 * ���������ֽ���ˮ:
 * ֻ���޸��˻����򻻿����˻����
 */
int deal_money(money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd)
{
	acc_ram *pacc = NULL;
	unsigned long accno = 0;		// ������־
	if (pnflow == NULL || pacc_m == NULL || prcd == NULL) {
		return -1;
	}
	pacc = search_no2(pnflow->CardNo, pacc_m, prcd->account_main);
	if (pacc) {
		// �ҵ���, ��������ǲ��ǻ����Ŀ�
		if ((pacc->feature & 0xC0) == 0xC0) {
			// ��Ƭ�ǻ�����, ����Ҫ����������
#if 0
			printk("card is changed!\n");
#endif
			accno = pacc->acc_num;
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
		if (accno) {
			// ��Ҫ�ҵ��¿���λ��, ���˺Ų���
			pacc = search_acc(accno, pacc_s, prcd->account_sw);
			if (pacc)
				goto end;
		}
		printk("can not find card no: %08x\n", (unsigned int)pnflow->CardNo);
		return 0;
	}
end:
#ifdef QINGHUA
	// ��СƱ�˻����⴦��
	if (/*(pacc->managefee & TICKETBIT) && */(pnflow->Money == TICKETMONEY)) {
		pacc->managefee &= ~TICKETBIT;
	} else {
#endif
		pacc->money += pnflow->Money;//=================
		if (pnflow->Money < 0) {
			int limit;
			pnflow->Money = 0 - pnflow->Money;
			limit = pacc->feature & 0xF;
			if ((limit != 0xF) && limit) {
				// ��Ҫ��������
				limit -= pnflow->Money / 100 + 1;
				if (!(pnflow->Money % 100)) {// �պ���100ʱ, ��Ϊ��һ��
					limit++;
				}
				if (limit < 0) {
					limit = 0;
				}
				pacc->feature &= 0xF0;
				pacc->feature |= limit;
			}
		}
#ifdef QINGHUA
	}
#endif
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
			// �ҵ���, ��������ǲ��ǻ����Ŀ�
			if ((pacc->feature & 0xC0) == 0xC0) {
				// ��Ƭ�ǻ�����, ����Ҫ����������
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
			// �ҵ���, ��������ǲ��ǻ����Ŀ�
			if ((pacc->feature & 0xC0) == 0xC0) {
				// ��Ƭ�ǻ�����, ����Ҫ����������
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
			// �ҵ���, ��������ǲ��ǻ����Ŀ�
			if ((pacc->feature & 0xC0) == 0xC0) {
				// ��Ƭ�ǻ�����, ����Ҫ����������
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
			// �ҵ���, ��������ǲ��ǻ����Ŀ�
			if ((pacc->feature & 0xC0) == 0xC0) {
				// ��Ƭ�ǻ�����, ����Ҫ����������
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
		oldacc.id_type = pacc->id_type;//write by duyy, 2013.7.1
		oldacc.acc_num = pacc->acc_num;
		oldacc.card_num = pnflow->NewCardId;
		oldacc.managefee = pacc->managefee;// - 1;
		oldacc.money = pacc->money;
		pacc->feature &= 0x3F;
		pacc->feature |= 0xC0;
		// flag == 1������, flag == 0����
		oldacc.feature &= 0x3F;
		if (pnflow->Flag) {
			oldacc.feature |= 0x80;
		}
		prcd->account_all += 1;
		if (sort_sw(pacc_s, &oldacc, (prcd->account_sw - 1)) < 0)
			return -1;
		break;
	case NOM_CURRENT_RGE:
		// �¼ӵĿ���������ע��Ŀ�
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
		oldacc.managefee = pnflow->ManageId;// - 1;
		oldacc.money = pnflow->RemainMoney;
		tmp = pnflow->UpLimitMoney & 0xF;
		pnflow->PowerId --;
		oldacc.id_type = pnflow->PowerId;//modified by duyy, 2013.7.1
		//tmp |= (pnflow->PowerId & 0x3) << ACCOUNTCLASS;// ����
		switch (pnflow->Flag) {
		case 1:
			tmp |= 0x10 << ACCOUNTFLAG;
			break;
		case 0:
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
		// �µĺ���������, ���ں����������
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
		blkinfo.count = cnt;
#ifdef QINGHUA
	//pbacc->card_num = pnflow->CardId;
	//pbacc->opt = pnflow->BLTypeId;
#warning "QingHua revision"
		pacc = search_no2(pnflow->CardId, pacc_m, prcd->account_main);
		if (pacc) {
			// �ҵ���, ��������ǲ��ǻ����Ŀ�
			if ((pacc->feature & 0xC0) == 0xC0) {
				// ��Ƭ�ǻ�����, ����Ҫ����������
				printk("card is changed!\n");
				pacc = NULL;
				goto find28;
			}
		}
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->CardId);
find28:
			pacc = search_no2(pnflow->CardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->CardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->CardId);
			break;
		}
		if (pnflow->BLTypeId) {
			pacc->feature &= 0x3F;
			pacc->feature |= 0x80;
		} else {
			pacc->feature &= 0x7F;
		}
#endif
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
 * ���������ֽ���ˮתΪ�������ṹ
 */
int nomtoblk(black_acc *pbacc, no_money_flow *pnflow)
{
	usr_time tm;
	memset(pbacc, 0, sizeof(black_acc));
	// Ϊ�����Ч�ʲ���������ȷ����֤
	pbacc->card_num = pnflow->CardId;
	pbacc->opt = pnflow->BLTypeId;
	if (ctoutm(&tm, pnflow->FlowTime) < 0)
		printk("blk time error: %s\n", pnflow->FlowTime);
	pbacc->t_num.s_tm.hyear = tm.hyear;
	pbacc->t_num.s_tm.lyear = tm.lyear;
	pbacc->t_num.s_tm.mon = tm.mon;
	pbacc->t_num.s_tm.day = tm.mday;
	pbacc->t_num.s_tm.hour = tm.hour;
	memcpy(&pbacc->t_num.num, &pnflow->BlackFlowId, sizeof(pbacc->t_num.num));
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
 * ����M1���ն��ϵ����
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
 * ����M1����ˮ, ����Ǯ����ˮ
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
				// �˴�Ӧ���ӳٴ���
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
	// ���յ��Ŀ��Ż���HEX
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
	// adjust money limit----cpu���������޴���
	// set ptrm->flow_flag
	space_remain--;
	ptrm->flow_flag = ptrm->term_no;
	// term_money plus
	if (cpuflow.flow_type == 0x99) {
		ptrm->term_money += cpuflow.consume_sum;
		ptrm->term_cnt++;
	}
	// ��ˮ�ŵĴ���
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