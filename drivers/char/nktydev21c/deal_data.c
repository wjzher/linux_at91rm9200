/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�deal_data.c
 * ժ    Ҫ�����֧��TS11�����ݴ�����
 *			 ֧���µ��ն˻�, ��TS11������ȥ��
 *			 ֧��2.1C����������ˮ������
 *			 ֧��Ǯ���ն˺ʹ����ն˻��� v2֧�� CONFIG_UART_V2
 * 			 
 * ��ǰ�汾��3.1
 * ��    �ߣ�duyy
 * ������ڣ�2014��2��19��
 *
 * ȡ���汾��3.0
 * ��    �ߣ�duyy
 * ������ڣ�2013��6��19��
 * 			 
 * ȡ���汾��2.3
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


int send_byte5(char buf, unsigned char num)
{
	int i;
	for (i = 0; i < 5; i++) {
		send_byte(buf, num);
	}
	return 0;
}

unsigned long binl2bcd(unsigned long val)
{
	int i;
	unsigned long tmp = 0;
	if (val >= 100000000) {
		printk("binl2bcd parameter %ld error!\n", val);
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
	return;
}

acc_ram *search_no(unsigned int cardno, acc_ram *pacc, int cnt)
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

acc_ram *search_no2(const unsigned int cardno, acc_ram *pacc, int cnt)
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

#if 0
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
#endif

acc_ram *search_id(const unsigned int cardno)
{
	acc_ram *pacc;
	pacc = search_no2(cardno, paccmain, rcd_info.account_main);
	if (pacc == NULL) {
		// can not found at main account
		pacc = hashtab_search(hash_accsw, (void *)cardno);
	} else {
#ifdef CONFIG_UART_V2
		if (pacc->power_id & ACCP_CHANGE) {
			// �����Ŀ�
			pacc = hashtab_search(hash_accsw, (void *)cardno);
		}
#else
		if ((pacc->feature & 0xC0) == 0xC0) {
			// ����һ�Ż����Ŀ�
			pacc = NULL;
			pacc = hashtab_search(hash_accsw, (void *)cardno);
		}
#endif
	}
	return pacc;
}

#ifdef CONFIG_UART_V2

// ���ֽڼ���
static inline void _sub_byte3(u8 *src, int val)
{
	int tmp = 0;
	memcpy(&tmp, src, 3);
	tmp -= val;
	//if (tmp < 0) {
	//	tmp = 0;
	//}
	memcpy(src, &tmp, 3);
	return;
}

// ���ֽڼӷ�
static inline void _add_byte3(u8 *src, int val)
{
	int tmp = 0;
	memcpy(&tmp, src, 3);
	tmp += val;
	//if (tmp < 0) {
	//	tmp = 0;
	//}
	memcpy(src, &tmp, 3);
	return;
}

/* ���Ͳʹβ��޴����� */
int sub_money(acc_ram *pacc, int money)
{
	if (pacc->money_life != 0xFFFF){//��ʣ�����ѽ��Ϊ����ģʽʱ�����������д���modified by duyy, 2014.2.17
		int tmp = pacc->money_life;
		tmp -= money;
		if (tmp < 0) {
			tmp = 0;
		}
		pacc->money_life = tmp;
	}
	if (pacc->times_life != 0xFF){//��ʣ�����Ѵ���Ϊ����ģʽʱ�����������д���modified by duyy, 2014.2.17
		if (pacc->times_life) {
			pacc->times_life--;
		}
	}
	return 0;
}

int sub_smoney(acc_ram *pacc, int money)
{
	if (pacc->smoney_life != 0xFFFF){//��ʣ�����ѽ��Ϊ����ģʽʱ�����������д���modified by duyy, 2014.2.17
		int tmp = pacc->smoney_life;
		tmp -= money;
		if (tmp < 0) {
			tmp = 0;
		}
		pacc->smoney_life = tmp;
	}
	if (pacc->stimes_life != 0xFF){//��ʣ�����Ѵ���Ϊ����ģʽʱ�����������д���modified by duyy, 2014.2.17
		if (pacc->stimes_life) {
			pacc->stimes_life--;
		}
	}
	return 0;
}

/* itm�Ƿ��ڵ�ǰʱ���� */
int is_current_tmsg(unsigned int itm)
{
	user_tmsg *usrtmsg = usrcfg.tmsg;
	int id = current_id;
	// ����̿���...
	return (itm >= usrtmsg[id].begin && itm <= usrtmsg[id].end);
}
#if 0 //modified by duyy, 2013.5.8
/* �ж�ĳ���ն����Ƿ���� */
int is_term_allow(u8 acc_pid, u8 term_pid)
{
	term_allow *allow = usrcfg.tallow;
	return allow[acc_pid] & (1 << term_pid);
}

/* �жϴ�ʱ���Ƿ���� ��ֹʱ���ж� */
int is_time_allow(int itm, u8 acc_pid)
{
	int i;
	user_nallow *nallow = usrcfg.nallow;
	nallow += acc_pid;
	for (i = 0; i < 4; i++) {
		if (itm >= nallow->begin[i] && itm <= nallow->end[i]) {
			pr_debug("pid = %d, itm = %d, b = %d, e = %d\n",
				acc_pid, itm, nallow->begin[i], nallow->end[i]);
			return 0;
		}
		//pr_debug("pid = %d, itm = %d, b = %d, e = %d\n",
		//	acc_pid, itm, nallow->begin[i], nallow->end[i]);
	}
	return 1;
}
#endif
/* �ۿ��ն��Ƿ���� */
int is_disc_allow(int termno, u32 *t_mask)
{
	// �ж��ն��Ƿ�Ҫ��������,termnoӦ�ж�һ�·�Χ���ǲ�����0��255֮��
	return (t_mask[termno >> 5] & (1 << (termno & 0x1F)));
}

/* ������۽��,��������val���ǰٷֱ����ݣ������Ҫ��100�õ���λ */
static int _cal_disc_money(int in_money, u16 val)
{
	in_money *= val;
	if (in_money % 100 >= 50) {
		in_money /= 100;
		in_money++;
	} else {
		in_money /= 100;
	}
	return in_money;
}
/* �������ǰ���,��������val���ǰٷֱ�����*/
static int _cal_disc_unproportion(int in_money, int val)
{
	in_money *= 100;
	in_money = in_money / val;
	return in_money;
}

int _check_overdraft(int real_money, int *money, int overdraft)
{
	*money -= real_money;
	if (overdraft){
		//�п�͸֧���
		int draft = overdraft * 100;
		//printk("draft is %d\n", draft);
		if (*money < (-draft)){
			*money += real_money;
			return -1;
		}
	} else{
		if (*money < 0){
			*money += real_money;
			return -1;
		}
	}
	return 0;
}
int _check_overdraft_proportion(int *dif_money, int *real_money, int *money, int overdraft)
{
	*money -= *real_money;
	if (overdraft){
		//�п�͸֧���
		int draft = overdraft * 100;
		//printk("draft is %d\n", draft);
		if (*money < (-draft)){
			*money += *real_money;
			*dif_money = *real_money - (*money + draft);
			*real_money = (*money + draft);
			*money = -draft;
			return -1;
		}
	} else{
		if (*money < 0){
			*money += *real_money;
			*dif_money = *real_money - *money;
			*real_money = *money;
			*money = 0;
			return -1;
		}
	}
	return 0;
}

/*
 * ֻ�ֽ���ã����޼��
 */
static int _check_limit(acc_ram *pacc, int t_money)
{
#if 0//modified by duyy, 2014.2.17
	if (pacc->money_life < t_money) {
		pr_debug("%x _check_limit money_life %d failed\n",
			pacc->card_num, pacc->money_life);
		return -1;
	}
	if (pacc->times_life == 0) {
		pr_debug("%x _check_limit times_life %d failed\n",
			 pacc->card_num, pacc->times_life);
		return -1;
	}
	return 0;
#endif
	//����������ģʽ��modified by duyy, 2014.2.17
	if (pacc->times_life == 0xFF){
		if ((pacc->money_life == 0xFFFF) || (t_money <= pacc->money_life)){
			return 0;
		} else {
			return -1;
		}
	} else if (pacc->times_life) {
		if ((pacc->money_life == 0xFFFF) || (t_money <= pacc->money_life)){
			return 0;
		} else {
			return -1;
		}
	} else {
		return -1;
	}

}

/*
 * ˫�˻����޼��, ����˫�˻�����ֻ��������
 */
static int _check_limit_sub(acc_ram *pacc, int t_money)
{
#if 0//modified by duyy, 2014.2.17
	if (pacc->smoney_life < t_money) {
		return -1;
	}
	if (pacc->stimes_life == 0) {
		return -1;
	}
	return 0;
#endif
	//����������ģʽ��modified by duyy, 2014.2.17
	if (pacc->stimes_life == 0xFF) {
		if ((pacc->smoney_life == 0xFFFF) || (t_money <= pacc->smoney_life)){
			return 0;
		} else {
			return -1;
		}
	} else if (pacc->stimes_life) {
		if ((pacc->smoney_life == 0xFFFF) || (t_money <= pacc->smoney_life)){
			return 0;
		} else {
			return -1;
		}
	} else {
		return -1;	
	}
}

/*
 * �ֽ��ʻ����޼��, ���ɿ��˻�����
 */
static int _check_cash_value(int real_money, acc_ram *pacc)
{
	//���ж����Ѵ��������ж����ѽ������,modified by duyy, 2014.2.17
	if (pacc->times_life == 0xFF){
		if ((pacc->money_life == 0xFFFF) || (real_money <= pacc->money_life)){
			return 0;
		} 
	} else if (pacc->times_life) {
		//ʣ�����ѽ��Ϊ���޻�ǰ����ֵС��ʣ�����ѽ�����������ѣ�modified by duyy,2014.2.17
		if ((pacc->money_life == 0xFFFF) || (real_money <= pacc->money_life)){
			return 0;
		}  
	} 
	//���������޺��жϣ�modified by duyy, 2014.2.17
	if ((pacc->st_limit & 0x3) == 0x1) {
		//�����������ֽ��ʻ�
		return -1;
	} else if ((pacc->st_limit & 0x3) == 0x2) {
		//���������Ѳ����ʻ� 
 		return -2;
	} else if ((pacc->st_limit & 0x3) == 0x3) {
		//�����������Ѳ����ʻ� 
 		return -3;
	} else {
		//�������޽�ֹ����
		return -4;
	}
	
}

/*
 * �����ʻ����޼��, ���ɿ��˻�����
 */
static int _check_sub_value(int real_money, acc_ram *pacc)
{
	//���ж����Ѵ��������ж����ѽ������,modified by duyy, 2014.2.17
	if (pacc->stimes_life == 0xFF) {
		if ((pacc->smoney_life == 0xFFFF) || (real_money <= pacc->smoney_life)){
			return 0;
		} 
	} else if (pacc->stimes_life) {
		//ʣ�����ѽ��Ϊ���޻�ǰ����ֵС��ʣ�����ѽ�����������ѣ�modified by duyy,2014.2.17
		if ((pacc->smoney_life == 0xFFFF) || (real_money <= pacc->smoney_life)){
			return 0;
		}  
	} 
#if 0//modified by duyy, 2014.2.17
	if ((real_money <= pacc->smoney_life) && pacc->stimes_life) {
		return 0;
	} else 
#endif
	if ((pacc->st_limit & 0xC) == 0x4) {
		//���������Ѳ����ʻ�
		return -1;
	} else if ((pacc->st_limit & 0xC) == 0x8) {
		//�����������ֽ��ʻ�
 		return -2;
	} else if ((pacc->st_limit & 0xC) == 0xC) {
		//�������������ֽ��ʻ�
 		return -3;
	} else {
		//�������޽�ֹ����
		return -4;
	}
	
}

/*
 * �ֽ��ʻ����޼��, ���˻�����
 */
static int _check_cash_unit(int *real_money, int *dif_money, acc_ram *pacc)
{
	
	if ((*real_money <= pacc->money_life) && pacc->times_life) {
		return 0;
	} else if ((pacc->st_limit & 0x3) == 0x1) {
		//�����������ֽ��ʻ�
		return -1;
	} else if ((pacc->st_limit & 0x3) == 0x2) {
		//���������Ѳ����ʻ�
		//���ѽ��У�������������
		*dif_money = *real_money - pacc->money_life;
		*real_money = pacc->money_life;
 		return -2;
	} else if ((pacc->st_limit & 0x3) == 0x3) {
		//�����������Ѳ����ʻ�
		//���ѽ��У�������������
		*dif_money = *real_money - pacc->money_life;
		*real_money = pacc->money_life;
 		return -3;
	} else {
		//�������޽�ֹ����
		return -4;
	}
	
}

/*
 * �����ʻ����޼��, ���˻�����
 */
static int _check_sub_unit(int *real_money, int *dif_money, acc_ram *pacc)
{
	
	if ((*real_money <= pacc->smoney_life) && pacc->stimes_life) {
		return 0;
	} else if ((pacc->st_limit & 0xC) == 0x4) {
		//���������Ѳ����ʻ�
		return -1;
	} else if ((pacc->st_limit & 0xC) == 0x8) {
		//�����������ֽ��ʻ�
		//���ѽ��У�������������
		*dif_money = *real_money - pacc->smoney_life;
		*real_money = pacc->smoney_life;
		return -2;
	} else if ((pacc->st_limit & 0xC) == 0xC) {
		//�������������ֽ��ʻ�
		//���ѽ��У�������������
		*dif_money = *real_money - pacc->smoney_life;
		*real_money = pacc->smoney_life;
		return -3;
	} else {
		//�������޽�ֹ����
		return -4;
	}
	
}

/*
 * �ֽ��ʻ��ۿ��ж�
 * money ������� ʵ�����ѽ��
 * ptrm ������������������
 * pacc �˻�ָ��
 * power_id �ʻ��������
 * proportion ��ǰ���۱�����0xFFFF��ʾ������Ч
 */
int _cal_disc_proportion(int *money, term_ram *ptrm, acc_ram *pacc, int power_id, int *proportion)
{
	int in_money = ptrm->_tmoney;
	discount *disc = &usrcfg.tmsg[current_id].disc[power_id & 0xF];
	u32 *t_mask = usrcfg.tmsg[current_id].t_mask;
	int times;

	*money = ptrm->_tmoney;

	if (!is_disc_allow(ptrm->term_no, t_mask)) {
		pr_debug("term %d disc no allow: id = %d, t_mask = %08x %08x %08x %08x %08x %08x %08x %08x\n",
			ptrm->term_no, current_id, usrcfg.tmsg[current_id].t_mask[0], usrcfg.tmsg[current_id].t_mask[1],
			usrcfg.tmsg[current_id].t_mask[2], usrcfg.tmsg[current_id].t_mask[3],
			usrcfg.tmsg[current_id].t_mask[4], usrcfg.tmsg[current_id].t_mask[5],
			usrcfg.tmsg[current_id].t_mask[6], usrcfg.tmsg[current_id].t_mask[7]);
		return 0;
	}
	
	if (in_money == 0) {
		pr_debug("warning input money = %d\n", in_money);
	}

	ptrm->_con_flag |= TRMF_DISCF;
	
	pr_debug("_cal_disc_proportion: cid = %d, pid = %d, stm = %d, etm1 = %d, etm2 = %d, dic_times = %d\n",
		current_id, power_id & 0xF, disc->start_times, disc->end_times1,
		disc->end_times2, pacc->dic_times);
	
	// �жϴ���ˢ������
	if (pacc->dic_times < disc->start_times) {
		pr_debug("dic_times %d < %d, no disc\n", pacc->dic_times, disc->start_times);
		return 0;
	}

	// ȷ�����ڵڼ��δ���
	times = pacc->dic_times - disc->start_times;	// 0 -> n
	if (times < disc->end_times1) {
		if ((disc->disc_val1 != 0xFFFF) && (disc->disc_val1 != 0xFFFE) && (disc->disc_val1 != 0)) {
			// ������Ч
			if (disc->disc_val1 == 0xFFFD){//���۱���Ϊ0
				*money = 0xFFFFFF;
			} else {
				*money = _cal_disc_money(in_money, disc->disc_val1);
			}
			*proportion = disc->disc_val1;
			ptrm->_con_flag |= TRMF_DISC;
		} else {
			//modified by duyy, 2012.5.30
			if (disc->disc_val1 == 0xFFFE){
				//���۽�ֹ
				pr_debug("disc1 val1 = 0xFFFE\n");
				return -1;
			} else {
				//������Ч
				*proportion = 0xFFFF;
				pr_debug("disc1 const1 = 0xFFFF\n");
			}
		}
	} else if (times < disc->end_times2) {
		if ((disc->disc_val2 != 0xFFFF) && (disc->disc_val2 != 0xFFFE) && (disc->disc_val2 != 0)) {
			// ������Ч
			if (disc->disc_val2 == 0xFFFD){//���۱���Ϊ0
				*money = 0xFFFFFF;
			} else {
				*money = _cal_disc_money(in_money, disc->disc_val2);
			}
			*proportion = disc->disc_val2;
			ptrm->_con_flag |= TRMF_DISC;
		} else {
			//modified by duyy, 2012.5.30
			if (disc->disc_val2 == 0xFFFE){
				pr_debug("disc1 val2 = 0xFFFE\n");
				return -1;
			} else {
				//������Ч
				*proportion = 0xFFFF;
				pr_debug("disc1 const2 = 0xFFFF\n");
			}
		}
	} else {
		pr_debug("pacc %d disc times = %d, start = %d, e1 = %d, e2 = %d\n",
			pacc->acc_num, pacc->dic_times, disc->start_times, disc->end_times1,
			disc->end_times2);
	}
	return 0;
}

/*
 * �ֽ��ʻ��ۿ��ж�
 * money ������� ʵ�����ѽ��
 * ptrm ������������������
 * pacc �˻�ָ��
 */
int _cal_disc_value(int *money, term_ram *ptrm, acc_ram *pacc, int power_id)
{
	int in_money = ptrm->_tmoney;
	discount *disc = &usrcfg.tmsg[current_id].disc[power_id & 0xF];
	u32 *t_mask = usrcfg.tmsg[current_id].t_mask;
	int times;

	*money = ptrm->_tmoney;

	if (!is_disc_allow(ptrm->term_no, t_mask)) {
		pr_debug("term %d disc no allow: id = %d, t_mask = %08x %08x %08x %08x %08x %08x %08x %08x\n",
			ptrm->term_no, current_id, usrcfg.tmsg[current_id].t_mask[0], usrcfg.tmsg[current_id].t_mask[1],
			usrcfg.tmsg[current_id].t_mask[2], usrcfg.tmsg[current_id].t_mask[3],
			usrcfg.tmsg[current_id].t_mask[4], usrcfg.tmsg[current_id].t_mask[5],
			usrcfg.tmsg[current_id].t_mask[6], usrcfg.tmsg[current_id].t_mask[7]);
		return 0;
	}
	
	if (in_money == 0) {
		pr_debug("warning input money = %d\n", in_money);
	}

	ptrm->_con_flag |= TRMF_DISCF;
	
	pr_debug("_cal_disc_value: cid = %d, pid = %d, stm = %d, etm1 = %d, etm2 = %d, dic_times = %d\n",
		current_id, power_id & 0xF, disc->start_times, disc->end_times1,
		disc->end_times2, pacc->dic_times);
	
	// �жϴ���ˢ������
	if (pacc->dic_times < disc->start_times) {
		pr_debug("dic_times %d < %d, no disc\n", pacc->dic_times, disc->start_times);
		return 0;
	}

	// ȷ�����ڵڼ��δ���
	times = pacc->dic_times - disc->start_times;	// 0 -> n
	if (times < disc->end_times1) {
		if ((disc->const_val1 != 0xFFFF) && (disc->const_val1 != 0xFFFE) && disc->const_val1) {
			// �̶������Ч
			if (disc->const_val1 == 0xFFFD){//�̶����Ϊ0
				*money = 0xFFFFFF;
			} else {
				*money = disc->const_val1;
			}
			ptrm->_con_flag |= TRMF_DISC;
		} else {
			//modified by duyy, 2012.5.30
			if (disc->const_val1 == 0xFFFE) {//bug,2012.10.26
				pr_debug("disc1 const1 = 0xFFFE\n");
				return -1;
			} else {
				pr_debug("disc1 const1 = 0xFFFF\n");
			}
		}
	} else if (times < disc->end_times2) {
		if ((disc->const_val2 != 0xFFFF) && (disc->const_val2 != 0xFFFE) && disc->const_val2) {
			// �̶������Ч
			if (disc->const_val2 == 0xFFFD){//�̶����Ϊ0
				*money = 0xFFFFFF;
			} else {
				*money = disc->const_val2;
			}
			ptrm->_con_flag |= TRMF_DISC;
		} else {
			//modified by duyy, 2012.5.30
			if (disc->const_val2 == 0xFFFE) {
				pr_debug("disc1 const2 = 0xFFFE\n");
				return -1;
			} else {
				pr_debug("disc1 const2 = 0xFFFF\n");
			}
		}
	} else {
		pr_debug("pacc %d disc times = %d, start = %d, e1 = %d, e2 = %d\n",
			pacc->acc_num, pacc->dic_times, disc->start_times, disc->end_times1,
			disc->end_times2);
	}
	return 0;
}

/*
 * �ֽ��ʻ��ۿ��ж�
 * money ������� ʵ�����ѽ��
 * ptrm ������������������
 * pacc �˻�ָ��
 */
int _cal_disc(int *money, term_ram *ptrm, acc_ram *pacc, int power_id)
{
	int in_money = ptrm->_tmoney;
	discount *disc = &usrcfg.tmsg[current_id].disc[power_id & 0xF];
	u32 *t_mask = usrcfg.tmsg[current_id].t_mask;
	int times;

	*money = ptrm->_tmoney;

	if (!is_disc_allow(ptrm->term_no, t_mask)) {
		pr_debug("term %d disc no allow: id = %d, t_mask = %08x %08x %08x %08x %08x %08x %08x %08x\n",
			ptrm->term_no, current_id, usrcfg.tmsg[current_id].t_mask[0], usrcfg.tmsg[current_id].t_mask[1],
			usrcfg.tmsg[current_id].t_mask[2], usrcfg.tmsg[current_id].t_mask[3],
			usrcfg.tmsg[current_id].t_mask[4], usrcfg.tmsg[current_id].t_mask[5],
			usrcfg.tmsg[current_id].t_mask[6], usrcfg.tmsg[current_id].t_mask[7]);
		return 0;
	}
	
	if (in_money == 0) {
		pr_debug("warning input money = %d\n", in_money);
	}

	ptrm->_con_flag |= TRMF_DISCF;
	
	pr_debug("_cal_disc: cid = %d, pid = %d, stm = %d, etm1 = %d, etm2 = %d, dic_times = %d\n",
		current_id, power_id & 0xF, disc->start_times, disc->end_times1,
		disc->end_times2, pacc->dic_times);
	
	// �жϴ���ˢ������
	if (pacc->dic_times < disc->start_times) {
		pr_debug("dic_times %d < %d, no disc\n", pacc->dic_times, disc->start_times);
		return 0;
	}

	// ȷ�����ڵڼ��δ���
	times = pacc->dic_times - disc->start_times;	// 0 -> n
	if (times < disc->end_times1) {
		if ((disc->disc_val1 != 0xFFFF) && (disc->disc_val1 != 0xFFFE) && disc->disc_val1) {
			// ������Ч
			if (disc->disc_val1 == 0xFFFD){//���۱���Ϊ0
				*money = 0xFFFFFF;
			} else {
				*money = _cal_disc_money(in_money, disc->disc_val1);
			}
			ptrm->_con_flag |= TRMF_DISC;
		} else if ((disc->const_val1 != 0xFFFF) && (disc->const_val1 != 0xFFFE) && disc->const_val1) {
			// �̶������Ч
			if (disc->const_val1 == 0xFFFD){//�̶����Ϊ0
				*money = 0xFFFFFF;
			} else {
				*money = disc->const_val1;
			}
			ptrm->_con_flag |= TRMF_DISC;
		} else {
			//modified by duyy, 2012.5.30
			if (disc->disc_val1 == 0xFFFE){
				pr_debug("disc1 val1 = 0xFFFE\n");
				return -1;
			} else if (disc->const_val1 == 0xFFFE) {//bug,2012.10.26
				pr_debug("disc1 const1 = 0xFFFE\n");
				return -1;
			} else {
				pr_debug("disc1 const1 = 0xFFFF\n");
			}
		}
	} else if (times < disc->end_times2) {
		if ((disc->disc_val2 != 0xFFFF) && (disc->disc_val2 != 0xFFFE) && disc->disc_val2) {
			// ������Ч
			if (disc->disc_val2 == 0xFFFD){//���۱���Ϊ0
				*money = 0xFFFFFF;
			} else {
				*money = _cal_disc_money(in_money, disc->disc_val2);
			}
			ptrm->_con_flag |= TRMF_DISC;
		} else if ((disc->const_val2 != 0xFFFF) && (disc->const_val2 != 0xFFFE) && disc->const_val2) {
			// �̶������Ч
			if (disc->const_val2 == 0xFFFD){//�̶����Ϊ0
				*money = 0xFFFFFF;
			} else {
				*money = disc->const_val2;
			}
			ptrm->_con_flag |= TRMF_DISC;
		} else {
			//modified by duyy, 2012.5.30
			if (disc->disc_val2 == 0xFFFE){
				pr_debug("disc1 val2 = 0xFFFE\n");
				return -1;
			} else if (disc->const_val2 == 0xFFFE) {
				pr_debug("disc1 const2 = 0xFFFE\n");
				return -1;
			} else {
				pr_debug("disc1 const2 = 0xFFFF\n");
			}
		}
	} else {
		pr_debug("pacc %d disc times = %d, start = %d, e1 = %d, e2 = %d\n",
			pacc->acc_num, pacc->dic_times, disc->start_times, disc->end_times1,
			disc->end_times2);
	}
	return 0;
}
/*
 * terminal type: 1-byte; le_card once consume: 1-byte; food price: 32-byte; id info: 32-byte
 * return 0 is ok; -1 is send error; -2 is variable error
 */
int send_run_data(term_ram *ptrm)
{
	int ret, i;
	unsigned char tmp;
	unsigned char code[32] = {0};
	// first send 1-byte terminal type (HEX)
	ret = send_data((char *)&ptrm->pterm->param.term_type, 1, ptrm->term_no);
	if (ret < 0) {
		pr_debug("send run data time out!\n");
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->param.term_type;
	ptrm->add_verify += (unsigned char)ptrm->pterm->param.term_type;
	// sencond send 1-byte max consume once (BCD)
	tmp = BIN2BCD(ptrm->pterm->param.max_consume);
	ret = send_data((char *)&tmp, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)tmp;
	ptrm->add_verify += (unsigned char)tmp;
	// cash code or price
	if (ptrm->pterm->param.term_type & (1 << CASH_TERMINAL_FLAG)) {
		memcpy(code, ptrm->pterm->param.term_passwd,
			sizeof(ptrm->pterm->param.term_passwd));
	} else {
		memcpy(code, ptrm->pterm->price, sizeof(code));
	}
	ret = send_data((char *)code, 32, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 32; i++) {
		ptrm->dif_verify ^= code[i];
		ptrm->add_verify += code[i];
	}
	if (verify_all(ptrm, CHKXOR) < 0) {
		pr_debug("send run data verify all error\n");
		return -1;
	}
	return 0;
}


/*
 * ˫�˻�Ҫ�������,modified by duyy, 2013.6.18
 */
int recv_leid_double(term_ram *ptrm, int allow, int itm)
{
	unsigned int cardno;
	acc_ram *pacc = NULL;
	int feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0, real_money = 0;
	int smoney = 0, real_smoney = 0;
	int flag = 1, sflag = 1;		// �������ѱ�־
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
	u32 passwd = 0xFFFFFFFF;
	int proportion = 0, sproportion = 0;
	int dif_money = 0, d_money = 0;
	int termmoney = 0;
	ptrm->_con_flag = 0;
	
	tmp += 3;
	for (i = 0; i < 4; i++, tmp--) {			// receive 4-byte card number
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(7 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	ptrm->_tmoney = 0;
	tmp = (u8 *)&ptrm->_tmoney;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(3 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
		feature = 1;
		feature |= (1 << 6);//modified by duyy, 2013.6.18
		money = 0;
		pr_debug("%x not allow\n", cardno);
		goto acc_end;
	}
	
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
		feature |= (1 << 4);
		pr_debug("%x not find\n", cardno);
		goto acc_end;
	}
	// �����������ƽ����˻��ж�
	if (pacc->flag & ACCF_LOSS) {
		// ���ǹ�ʧ��
		feature = 1;
		feature |= (1 << 1);//modified by duyy, 2013.6.18
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// �ֽ��ʻ����ᣬwrite by duyy, 2013.5.8
		flag = 0;
	}
	if (pacc->flag & ACCF_FREEZE_SUB){
		// �������ᣬwrite by duyy, 2013.5.8
		sflag = 0;
	}
	if (pacc->flag & ACCF_WOFF) {
		// ע����
		feature = 1;
		feature |= (1 << 4);//modified by duyy, 2013.6.18
		//printk("%x cancel\n", cardno);
		goto acc_end;
	}
	// passwd copy
	memcpy(&passwd, pacc->passwd, sizeof(pacc->passwd));
	//printk("password is %x\n", passwd);
	//printk("acc limit:money is %d, times is %d\n", pacc->money_life, pacc->times_life);
	//printk("sub limit:money is %d, times is %d\n", pacc->smoney_life, pacc->stimes_life);
	
	// �ʹα�־
	ptrm->_con_flag |= TRMF_LIMIT;	/* ˮ���ն�ʱȥ��, bug */
	
	//���ֽ��ʻ�����ʹ��ʱ���Ž��н�ֹ����ʱ���жϣ�modified by duyy, 2014.1.15
	if (flag){
		//��ݡ��նˡ�ʱ��ƥ���жϣ�write by duyy, 2013.5.8
		timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//��ݡ��ն˶�Ӧ��ʱ�����
		//printk("double: cardno:%ld timenum = %x\n", cardno, timenum);
		if (timenum & 0x80){//modified by duyy, 2013.5.8,���λbit7��ʾ��ֹ���ѣ�����Ϊ�������ѣ��жϽ�ֹʱ��
			flag = 0;
		}
		else {
			// �ֽ��˻�����ʹ��
			flag = 1;
			for (i = 0; i < 7; i++){
				if (timenum & 0x1){
					if(itm >= term_time[i].begin && itm <= term_time[i].end){
						// �����˻���ֹʹ��
						flag = 0;
						break;
					}
				}
				timenum = timenum >> 1;
			}
		}
	}

	//�������ʻ�����ʹ��ʱ���Ž��н�ֹ����ʱ���жϣ�modified by duyy, 2014.1.15
	if (sflag){
		//��ݡ��նˡ�ʱ��ƥ���жϣ�write by duyy, 2013.5.8
		timenum = ptmnum[pacc->spower_id & 0xF][ptrm->pterm->power_id & 0xF];//��ݡ��ն˶�Ӧ��ʱ�����
		//printk("sub: cardno:%ld timenum = %x\n", cardno, timenum);
		if (timenum & 0x80){//modified by duyy, 2013.5.8,���λbit7��ʾ��ֹ���ѣ�����Ϊ�������ѣ��жϽ�ֹʱ��
			sflag = 0;
		}
		else {
			// �����˻�����ʹ��
			sflag = 1;
			for (i = 0; i < 7; i++){
				if (timenum & 0x1){
					if(itm >= term_time[i].begin && itm <= term_time[i].end){
						// �����˻���ֹʹ��
						sflag = 0;
						break;
					}
				}
				timenum = timenum >> 1;
			}
		}
	}
	if (!flag && !sflag) {
		// ������������
		feature = 1;
		feature |= (1 << 8);//modified by duyy, 2013.6.18
		//printk("%x: flag = %d, sflag = %d\n", cardno, flag, sflag);
		goto acc_end;
	}
	//printk("%x: flag = %d, sflag = %d\n", cardno, flag, sflag);
	//�ֽ��ʻ����
	money = pacc->money;
	//�����ʻ����
	memcpy(&smoney, pacc->sub_money, sizeof(pacc->sub_money));
	if (pacc->sub_money[2] & 0x80){//�������Ϊ��ʱ����Ӧת��
		smoney = ~smoney;
		smoney += 1;
		smoney = smoney & 0xFFFFFF;
		smoney = -smoney;
	}
	//printk("pacc->money is %d\n", pacc->money);
	//printk("pacc: money is %d, smoney is %d\n", money, smoney);
	//printk("%x: st_limit is %x\n", cardno, pacc->st_limit);
	
	if (flag && sflag) {
		if (pacc->st_limit & ACCST_ALLOW_UNIT) {
			// ������˻�ʹ��
			feature |= (1 << 14);//˫�ʻ���������
			if (pacc->st_limit & ACCST_CASH) {
				//�����ֽ��ʻ�����
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.1.13
				if (ptrm->pterm->param.spcl & (1 << 7)){
					//printk("Welfare terminal %d, money is %d, termmoney is %d\n", ptrm->term_no, money, ptrm->_tmoney);
					//�����ն�
					if ((!money) || (!pacc->money_life) || (money < pacc->money_life)){
						//�����˻�ûǮʱ��ֹ����
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
					//���ٶԸ����˻��������жϣ������жϵ������ѽ�����ޣ�modified by duyy, 2014.3.13
					if (ptrm->_tmoney < pacc->money_life){
						//�����ն�������С�ڵ������ѽ��ʱ,�����ѽ��Ϊ�����������
						ptrm->_tmoney = pacc->money_life;
					}
				} else {
					//printk("common terminal %d, pacc->money_life is %d, termmoney is %d\n",
					//			ptrm->term_no, pacc->money_life, ptrm->_tmoney);
					//��ͨ�ն�
					if (money){//�˻���Ǯ���ҵ������ѽ��δ������ʱ�жϵ�ǰʵ�����Ѷwrite by duyy, 2014.3.13
						if (pacc->money_life){//���ٶԸ����˻��������жϣ������жϵ������ѽ�����ޣ�modified by duyy, 2014.3.13
							//�����˻���Ǯ
							if (ptrm->_tmoney < pacc->money_life){
								//�����ն�������С�ڸ����˻����ʱ,�����ѽ��Ϊ�����˻����
								ptrm->_tmoney = pacc->money_life;
							}
						}
					}
				}

#endif
				//�����ֽ��ʻ�ʵ������ֵ
				ret = _cal_disc_proportion(&real_money, ptrm, pacc, pacc->power_id, &proportion);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("proportion is %d\n", proportion);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_money == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_money;
				}
				//ret = _check_cash_unit(&termmoney, &dif_money, pacc);
				ret = _check_cash_value(termmoney, pacc);
				if (ret < 0){
					if (ret == -1){
						//�ж��ʻ�����Ƿ����
						ret = _check_overdraft(termmoney, &money, pacc->draft);
						if (ret < 0){
							feature = 1;
							feature |= (1 << 9);//�ֽ��ʻ����㣬��ֹ����
							goto acc_end;
						}
						feature |= (1 << 11);	//�����������ֽ��ʻ�
						goto acc_end;
					} else if (ret == -2){
						feature |= (1 << 11);	//���������Ѳ����ʻ�
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.3.13
						if (money < 0){//��֧���˻�͸֧
							real_money = 0;
							dif_money = termmoney;
						} else if (money < pacc->money_life){
							dif_money = termmoney - money;
							real_money = money;
							money = 0;
						} else {
							dif_money = termmoney - pacc->money_life;
							real_money = pacc->money_life;
							money -= pacc->money_life;
						}
						ptrm->_tmoney = dif_money;
#else					
				#if 0
						if (proportion == 0xFFFD){
							real_money = 0;
							goto sub_proportion;
						}
						ret = _check_overdraft_proportion(&d_money, &termmoney, &money, pacc->draft);
						if (ret < 0){
							dif_money += d_money;
						}
						if (proportion == 0xFFFF){
							ptrm->_tmoney = dif_money;
							real_money = termmoney;
						} else {
							if (proportion){
								ptrm->_tmoney = _cal_disc_unproportion(dif_money, proportion);	
							} else {
								ptrm->_tmoney = dif_money;
							}	
							real_money = termmoney;
						}			
				#endif
						real_money = 0;
#endif
						goto sub_proportion;
					} else if (ret == -3){		//�����������Ѳ����ʻ�
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.3.13
						if (money < 0){//��֧���˻�͸֧
							real_money = 0;
							dif_money = termmoney;
						} else if (money < pacc->money_life){
							dif_money = termmoney - money;
							real_money = money;
							money = 0;
						} else {
							dif_money = termmoney - pacc->money_life;
							real_money = pacc->money_life;
							money -= pacc->money_life;
						}
						ptrm->_tmoney = dif_money;
#else					
				#if 0
						if (proportion == 0xFFFD){
							real_money = 0;
							goto sub_proportion;
						}
						ret = _check_overdraft_proportion(&d_money, &termmoney, &money, pacc->draft);
						if (ret < 0){
							dif_money += d_money;
						}
						if (proportion == 0xFFFF){
							ptrm->_tmoney = dif_money;
							real_money = termmoney;
						} else {
							if (proportion){
								ptrm->_tmoney = _cal_disc_unproportion(dif_money, proportion);	
							} else {
								ptrm->_tmoney = dif_money;
							}
							real_money = termmoney;
						}	
				#endif 
						real_money = 0;
#endif
						goto sub_proportion;
					} else {
						feature = 1;
						feature |= (1 << 7);	//�����������޽�ֹ����
						goto acc_end;
					}
				} 
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft_proportion(&dif_money, &termmoney, &money, pacc->draft);
				if (ret < 0){
					if (proportion == 0xFFFF){
						ptrm->_tmoney = dif_money;
						real_money = termmoney;
					} else {
						if (proportion && (proportion != 0xFFFD)){
							ptrm->_tmoney = _cal_disc_unproportion(dif_money, proportion);	
						} else {
							ptrm->_tmoney = dif_money;
						}
						real_money = termmoney;
					}
				} else {
					goto acc_end;
				}			
sub_proportion:			
				//printk("ptrm->_tmoney is %d\n", ptrm->_tmoney);
				//�絽�����ʻ�����
				//���㲹���ʻ�ʵ������ֵ
				ret = _cal_disc_proportion(&real_smoney, ptrm, pacc, pacc->spower_id, &sproportion);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("sub proportion is %d\n", sproportion);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_smoney == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_smoney;
				}
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 10);//�����ʻ����㣬��ֹ����
					goto acc_end;
				}
				if (_check_limit_sub(pacc, termmoney) < 0) {
					// �������ƣ��ж��Ƿ����������������
					if (pacc->st_limit & ACCST_SUB_PSWSUB) {
						feature |= (1 << 11);//����������
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}
			} else {
				//���Ȳ����ʻ�����
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.1.13
				if (ptrm->pterm->param.spcl & (1 << 7)){
					//printk("Welfare terminal %d, smoney is %d, pacc->smoney_life is %d, termmoney is %d\n", 
					//			ptrm->term_no, smoney, pacc->smoney_life,ptrm->_tmoney);
					//�����ն�
					if ((!smoney) || (!pacc->smoney_life) || (smoney < pacc->smoney_life)){
						//�����˻�ûǮʱ��ֹ����
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
					//���ٶԲ����˻��������жϣ������жϵ������ѽ�����ޣ�modified by duyy, 2014.3.13
					if (ptrm->_tmoney < pacc->smoney_life){
						//�����ն�������С�ڵ������ѽ��ʱ,�����ѽ��Ϊ�����������
						ptrm->_tmoney = pacc->smoney_life;
					}
				} else {
					//printk("common terminal %d, pacc->smoney_life is %d, termmoney is %d\n",
					//			ptrm->term_no, pacc->smoney_life, ptrm->_tmoney);
					//��ͨ�ն�
					if (smoney){//�����˻���Ǯ���ҵ������ѽ��δ������ʱ�жϵ�ǰʵ�����Ѷwrite by duyy, 2014.3.13
						if (pacc->smoney_life){//���ٶԲ����˻��������жϣ������жϵ������ѽ�����ޣ�modified by duyy, 2014.3.13
							//�����˻���Ǯ
							if (ptrm->_tmoney < pacc->smoney_life){
								//�����ն�������С�ڲ����˻����ʱ,�����ѽ��Ϊ�����˻����
								ptrm->_tmoney = pacc->smoney_life;
							}
						}
					}
				}
#endif
				//���㲹���ʻ�ʵ������ֵ
				ret = _cal_disc_proportion(&real_smoney, ptrm, pacc, pacc->spower_id, &sproportion);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("sub proportion is %d\n", sproportion);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_smoney == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_smoney;
				}
				//ret = _check_sub_unit(&termmoney, &dif_money, pacc);
				ret = _check_sub_value(termmoney, pacc);
				if (ret < 0){
					if (ret == -1){
						//�ж��ʻ�����Ƿ����
						ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
						if (ret < 0){
							feature = 1;
							feature |= (1 << 10);//�����ʻ����㣬��ֹ����
							goto acc_end;
						}
						feature |= (1 << 11);	//���������Ѳ����ʻ�
						goto acc_end;
					} else if (ret == -2){
						feature |= (1 << 11);	//�����������ֽ��ʻ�
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.3.13
						if (smoney < 0){//��֧���˻�͸֧,��֧�ִ�������
							real_smoney = 0;
							dif_money = termmoney;
						} else if (smoney < pacc->smoney_life){
							dif_money = termmoney - smoney;
							real_smoney = smoney;
							smoney = 0;
						} else {
							dif_money = termmoney - pacc->smoney_life;
							real_smoney = pacc->smoney_life;
							smoney -= pacc->smoney_life;
						}
						ptrm->_tmoney = dif_money;
#else
				#if 0
						if (sproportion == 0xFFFD){
							real_smoney = 0;
							goto cash_proportion;
						}
						ret = _check_overdraft_proportion(&d_money, &termmoney, &smoney, pacc->sdraft);
						if (ret < 0){
							dif_money += d_money;
						}
						if (sproportion == 0xFFFF){
							ptrm->_tmoney = dif_money;
							real_smoney = termmoney;
						} else {
							if (sproportion){
								ptrm->_tmoney = _cal_disc_unproportion(dif_money, sproportion);
							}else {
								ptrm->_tmoney = dif_money;
							}
							real_smoney = termmoney;
						}			
				#endif
						real_smoney = 0;
#endif
						goto cash_proportion;
					} else if (ret == -3){    //�������������ֽ��ʻ�
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.3.13
						if (smoney < 0){//��֧���˻�͸֧����֧�ִ�������
							real_smoney = 0;
							dif_money = termmoney;
						} else if (smoney < pacc->smoney_life){
							dif_money = termmoney - smoney;
							real_smoney = smoney;
							smoney = 0;
						} else {
							dif_money = termmoney - pacc->smoney_life;
							real_smoney = pacc->smoney_life;
							smoney -= pacc->smoney_life;
						}
						ptrm->_tmoney = dif_money;
#else
				#if 0
						if (sproportion == 0xFFFD){
							real_smoney = 0;
							goto cash_proportion;
						}
						ret = _check_overdraft_proportion(&d_money, &termmoney, &smoney, pacc->sdraft);
						if (ret < 0){
							dif_money += d_money;
						}
						if (sproportion == 0xFFFF){
							ptrm->_tmoney = dif_money;
							real_smoney = termmoney;
						} else {
							if (sproportion){
								ptrm->_tmoney = _cal_disc_unproportion(dif_money, sproportion);
							} else {
								ptrm->_tmoney = dif_money;
							}
							real_smoney = termmoney;
						}
				#endif
						real_smoney = 0;
#endif
						goto cash_proportion;
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft_proportion(&dif_money, &termmoney, &smoney, pacc->sdraft);
				if (ret < 0){
					if (sproportion == 0xFFFF){
						ptrm->_tmoney = dif_money;
						real_smoney = termmoney;
					} else {	
						if (sproportion && (sproportion != 0xFFFD)){
							ptrm->_tmoney = _cal_disc_unproportion(dif_money, sproportion);
						}else {
							ptrm->_tmoney = dif_money;
						}
						real_smoney = termmoney;
					}				
				} else {
					goto acc_end;
				}				
cash_proportion:	
				//printk("cash ptrm->_tmoney is %d\n", ptrm->_tmoney);
				//�絽�ֽ��ʻ�����
				//�����ֽ��ʻ�ʵ������ֵ
				ret = _cal_disc_proportion(&real_money, ptrm, pacc, pacc->power_id, &proportion);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("cash proportion is %d\n", proportion);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_money == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_money;
				}
				//printk("term money is %d", termmoney);
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &money, pacc->draft);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 9);//�ֽ��ʻ����㣬��ֹ����
					goto acc_end;
				}		
				// �ж��ֽ��˻����ޣ�����־������λ
				if (_check_limit(pacc, termmoney) < 0) {
					// �������ƣ��ж��Ƿ����������������
					if (pacc->st_limit & ACCST_CASH_PSWCASH) {
						feature |= (1 << 11);	//����������
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}		
			}
		} else {
			// ��������˻�ʹ��
			feature |= (1 << 14);//˫�ʻ���������
			if (pacc->st_limit & ACCST_CASH) {
				//�����ֽ��ʻ�����
				//�����ֽ��ʻ�ʵ������ֵ
				ret = _cal_disc_value(&real_money, ptrm, pacc, pacc->power_id);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_money == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_money;
				}
				//printk("cash cousme money is %d\n", termmoney);
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &money, pacc->draft);
				if (ret < 0){
					real_money = 0;
					goto sub_value;
				}
				ret = _check_cash_value(termmoney, pacc);
				if (ret < 0){
					if (ret == -1){
						feature |= (1 << 11);	//�����������ֽ��ʻ�
						goto acc_end;
					} else if (ret == -2){
						feature |= (1 << 11);	//���������Ѳ����ʻ�
						money += termmoney;//���ֽ��ʻ���ȥ��Ǯ�ټӻ���
						real_money = 0;
						goto sub_value;
					} else if (ret == -3){
						//�����������Ѳ����ʻ�
						money += termmoney;//���ֽ��ʻ���ȥ��Ǯ�ټӻ���
						real_money = 0;
						goto sub_value;
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				} else {
					//��������
					goto acc_end;
				}
sub_value:				
				//�絽�����ʻ�����
				//���㲹���ʻ�ʵ������ֵ
				ret = _cal_disc_value(&real_smoney, ptrm, pacc, pacc->spower_id);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_smoney == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_smoney;
				}
				//printk("sub consume money is %d\n", termmoney);
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 10);//�����ʻ����㣬��ֹ����
					goto acc_end;
				}
				if (_check_limit_sub(pacc, termmoney) < 0) {
					// �������ƣ��ж��Ƿ����������������
					if (pacc->st_limit & ACCST_SUB_PSWSUB) {
						feature |= (1 << 11);//����������
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}
			} else {
				//���Ȳ����ʻ�����
				//���㲹���ʻ�ʵ������ֵ
				ret = _cal_disc_value(&real_smoney, ptrm, pacc, pacc->spower_id);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("sub real money is %d\n", real_smoney);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_smoney == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_smoney;
				}
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
				if (ret < 0){
					real_smoney = 0;
					goto cash_value;
				}
				ret = _check_sub_value(termmoney, pacc);
				if (ret < 0){
					if (ret == -1){
						feature |= (1 << 11);	//���������Ѳ����ʻ�
						goto acc_end;
					} else if (ret == -2){
						feature |= (1 << 11);	//�����������ֽ��ʻ�
						smoney += termmoney;//�Ѳ����ʻ���ȥ��Ǯ�ټӻ���
						real_smoney = 0;
						goto cash_value;
					} else if (ret == -3){
						//�������������ֽ��ʻ�
						smoney += termmoney;//�Ѳ����ʻ���ȥ��Ǯ�ټӻ���
						real_smoney = 0;
						goto cash_value;
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				} else {
					//��������
					goto acc_end;
				}
cash_value:
				//�絽�ֽ��ʻ�����
				//�����ֽ��ʻ�ʵ������ֵ
				ret = _cal_disc_value(&real_money, ptrm, pacc, pacc->power_id);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_money == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_money;
				}
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &money, pacc->draft);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 9);//�ֽ��ʻ����㣬��ֹ����
					goto acc_end;
				}		
				// �ж��ֽ��˻����ޣ�����־������λ
				if (_check_limit(pacc, termmoney) < 0) {
					// �������ƣ��ж��Ƿ����������������
					if (pacc->st_limit & ACCST_CASH_PSWCASH) {
						feature |= (1 << 11);	//����������
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}		
			}
		}
	} else if (flag) {
#ifdef CONFIG_GGGS//�ֹܹ�˾����˫����ʾ��modified by duyy, 2014.2.14
		feature |= (1 << 14);//˫�ʻ���������
#else
		feature |= (1 << 12);//�ֽ��ʻ���������λ
#endif
		//printk("single cash, term money is %d\n", ptrm->_tmoney);
		/* �ۿ��жϣ��������Ѷ� */
		ret = _cal_disc(&real_money, ptrm, pacc, pacc->power_id);
		if (ret < 0){
			feature = 1;
			feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
			goto acc_end;
		}
		//�ж�ʵ�����Ѷ��Ƿ�0����
		if (real_money == 0xFFFFFF){
			termmoney = 0;
		} else {
			termmoney = real_money;
		}
		//�ж��ʻ�����Ƿ����
		ret = _check_overdraft(termmoney, &money, pacc->draft);
		if (ret < 0){
			feature = 1;
			feature |= (1 << 9);//�ֽ��ʻ����㣬��ֹ����
			goto acc_end;
		}	
		
		// �ж��ֽ��˻����ޣ�����־������λ
		if (_check_limit(pacc, termmoney) < 0) {
			// �������ƣ��ж��Ƿ����������������
			if (pacc->st_limit & ACCST_CASH_PSWCASH) {
				feature |= (1 << 11);	//����������
			} else {
				feature = 1;
				feature |= (1 << 7);//�����������޽�ֹ����
				goto acc_end;
			}
		}		
	} else if (sflag) {
#ifdef CONFIG_GGGS//�ֹܹ�˾����˫����ʾ��modified by duyy, 2014.2.14
		feature |= (1 << 14);//˫�ʻ���������
#else
		feature |= (1 << 13);//�����ʻ���������λ
#endif
		//printk("single sub\n");
		/* �ۿ��жϣ��������Ѷ� */
		ret = _cal_disc(&real_smoney, ptrm, pacc, pacc->spower_id);
		if (ret < 0){
			feature = 1;
			feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
			goto acc_end;
		}
		//�ж�ʵ�����Ѷ��Ƿ�0����
		if (real_smoney == 0xFFFFFF){
			termmoney = 0;
		} else {
			termmoney = real_smoney;
		}
		//�ж��ʻ�����Ƿ����
		ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
		if (ret < 0){
			feature = 1;
			feature |= (1 << 10);//�����ʻ����㣬��ֹ����
			goto acc_end;
		}
		// �ж��ֽ��˻����ޣ�����־������λ
		if (_check_limit_sub(pacc, termmoney) < 0) {
			// �������ƣ��ж��Ƿ����������������
			if (pacc->st_limit & ACCST_SUB_PSWSUB) {
				feature |= (1 << 11);//����������
			} else {
				feature = 1;
				feature |= (1 << 7);//�����������޽�ֹ����
				goto acc_end;
			}
		}
	}
	
acc_end:	
	//printk("cardno:%x: money is %d, real_money is %d\n", cardno, money, real_money);
	//printk("cardno:%x: smoney is %d, real_smoney is %d\n", cardno, smoney, real_smoney);	
 	//printk("recv_leid_double feature=%x\n", feature);//2012.5.9
	if (money > 8000000) {//modified by duyy, 2014.4.4
		printk("money is too large!\n");
		money = 8000000;
	}
	// 2�ֽ�ʵ�����Ѷ�feature
	tmp = (unsigned char *)&feature;
	tmp += 1;
	for (i = 0; i < 2; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	// 3�ֽ��ֽ��ʻ����Ѻ����money��16����
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}

	// 3�ֽ��ֽ��ʻ�ʵ�����Ѷ�real_money
	tmp = (unsigned char *)&real_money;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	// �û�����
	tmp = (unsigned char *)&passwd;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	// 3�ֽڲ����ʻ����Ѻ����smoney��16����
	tmp = (unsigned char *)&smoney;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	// 3�ֽڲ����ʻ�ʵ�����Ѷ�real_smoney
	tmp = (unsigned char *)&real_smoney;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	if (verify_all(ptrm, CHKALL) < 0)
		return -1;
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
	return 0;
}


/*
 * ����˫�˻�������ˮ
 */
int recv_leflow_double(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int smoney = 0;
	int csmoney = 0;
	int ret, i;
	int tail;
	unsigned char *tmp;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	
	// first recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	tmp += 3;		// tmp point lenum high byte
	for (i = 0; i < 4; i++, tmp--) {
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
	}
	
	// second recv 3-byte �ֽ��ʻ�ʵ�����Ѷ�
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
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
	}
	
	// second recv 3-byte ����ʵ�����ѽ��
	tmp = (unsigned char *)&smoney;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
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
	}
	
	// third check NXOR
	if (verify_all(ptrm, CHKALL) < 0)
		return -1;

	// check if receive the flow
	if (ptrm->flow_flag)
		return 0;
	space_remain--;
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		pr_debug("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	#if 0 //added by duyy, 2012.3.29
		printk("recv_leflow_double leflow.consume_sum=%ld\n", leflow.consume_sum);
		printk("recv_leflow_double smoney=%d\n", smoney);
	#endif
	// �����ֽ��ʻ��ʹβ���
	if (leflow.consume_sum) {
		if (leflow.consume_sum == 0xFFFFFF){
			csmoney = 0;
		} else {
			csmoney = leflow.consume_sum;
		}
		sub_money(pacc, csmoney);
		pacc->money -= csmoney;
		//if (pacc->money < 0) {
		//	pr_debug("account money below zero!!!\n");
		//	pacc->money = 0;
		//}
	}
		
	//���������ʻ��ʹβ���
	if (smoney) {
		if (smoney == 0xFFFFFF){
			csmoney = 0;
		} else {
			csmoney = smoney;
		}
		sub_smoney(pacc, csmoney);
		_sub_byte3(pacc->sub_money, csmoney);
	}	
	
	pr_debug("double flow %x: cm = %ld, sm = %d, m %d, %02x%02x%02x\n", pacc->card_num,
		leflow.consume_sum, smoney, pacc->money, pacc->sub_money[2], pacc->sub_money[1], pacc->sub_money[0]);
#if 0
	pacc->sub_money -= smoney;
	if (pacc->sub_money < 0) {
		pr_debug("account sub money below zero!!!\n");
		pacc->sub_money = 0;
	}
#endif
	if (ptrm->_con_flag & TRMF_DISCF) {
		pacc->dic_times++;
	}

	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum + smoney;

	leflow.flow_type = LECONSUME;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm++;
	leflow.manage_fee = ptrm->_tmoney;
	leflow.tml_num = ptrm->term_no;
	leflow.acc_num = pacc->acc_num;
	
	leflow.ltype = ptrm->_con_flag;
	
	// ˫�˻���������
	if (leflow.consume_sum && smoney) {
		// init leflow
		leflow.ltype |= FLOWF_SPLIT;
		if (leflow.consume_sum == 0xFFFFFF){
			leflow.consume_sum = 0;
		}
		// ��ˮ�ŵĴ���
		leflow.flow_num = maxflowno++;
		
		// set ptrm->flow_flag
		// ��ˮ��ͷβ�Ĵ���
		tail = flowptr.tail;
		memcpy(pflow + tail, &leflow, sizeof(flow));
		tail++;
		if (tail == FLOWANUM)
			tail = 0;
		
		flowptr.tail = tail;
		flow_sum++;

		// �����ˮ
		leflow.ltype &= ~(FLOWF_SPLIT | TRMF_LIMIT);
		if (smoney == 0xFFFFFF){
			leflow.consume_sum = 0;
		} else {
			leflow.consume_sum = smoney;
		}
		leflow.is_sub = 1;
		// ��ˮ�ŵĴ���
		leflow.flow_num = maxflowno++;
		// ��ˮ��ͷβ�Ĵ���
		tail = flowptr.tail;
		memcpy(pflow + tail, &leflow, sizeof(flow));
		tail++;
		if (tail == FLOWANUM)
			tail = 0;
		flowptr.tail = tail;
		flow_sum++;
	} else if (leflow.consume_sum) {
		// ��������
		
		if (leflow.consume_sum == 0xFFFFFF){
			leflow.consume_sum = 0;
		}
		// ��ˮ�ŵĴ���
		leflow.flow_num = maxflowno++;
		
		// ��ˮ��ͷβ�Ĵ���
		tail = flowptr.tail;
		memcpy(pflow + tail, &leflow, sizeof(flow));
		tail++;
		if (tail == FLOWANUM)
			tail = 0;
		
		flowptr.tail = tail;
		flow_sum++;
		
	} else if (smoney) {
		// ��������

		// ��ˮ�ŵĴ���
		leflow.flow_num = maxflowno++;
		if (smoney == 0xFFFFFF){
			leflow.consume_sum = 0;
		} else {
			leflow.consume_sum = smoney;
		}
		leflow.is_sub = 1;
		
		// ��ˮ��ͷβ�Ĵ���
		tail = flowptr.tail;
		memcpy(pflow + tail, &leflow, sizeof(flow));
		tail++;
		if (tail == FLOWANUM)
			tail = 0;
		
		flowptr.tail = tail;
		flow_sum++;
	}
	//printk("flow num is %ld\n", leflow.flow_num);
	ptrm->flow_flag = ptrm->term_no;
	return 0;
}


/*
 * ˫�˻�Ҫ�������,�����˻���ʱ�䡢������Ϣ��modified by duyy, 2014.6.6
 */
int recv_leid_double_ticket(term_ram *ptrm, int allow, int itm, unsigned char *tm)
{
	unsigned int cardno;
	acc_ram *pacc = NULL;
	int feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0, real_money = 0;
	int smoney = 0, real_smoney = 0;
	int flag = 1, sflag = 1;		// �������ѱ�־
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
	u32 passwd = 0xFFFFFFFF;
	int proportion = 0, sproportion = 0;
	int dif_money = 0, d_money = 0;
	int termmoney = 0;
	unsigned long acc = 0;//write by duyy, 20174.6.6
	char name[12] = {0};//write by duyy, 2014.6.6
	ptrm->_con_flag = 0;
	
	tmp += 3;
	for (i = 0; i < 4; i++, tmp--) {			// receive 4-byte card number
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(7 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	ptrm->_tmoney = 0;
	tmp = (u8 *)&ptrm->_tmoney;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = -1;
			}
			if (ret == -2) {
				ptrm->status = -2;
				usart_delay(3 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
		feature = 1;
		feature |= (1 << 6);//modified by duyy, 2013.6.18
		money = 0;
		pr_debug("%x not allow\n", cardno);
		goto acc_end;
	}
	
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
		feature |= (1 << 4);
		pr_debug("%x not find\n", cardno);
		goto acc_end;
	}
	acc = pacc->acc_num;//write by duyy, 2014.6.6
	memcpy(name, pacc->user_name, sizeof(name));//write by duyy, 2014.6.6
	
	// �����������ƽ����˻��ж�
	if (pacc->flag & ACCF_LOSS) {
		// ���ǹ�ʧ��
		feature = 1;
		feature |= (1 << 1);//modified by duyy, 2013.6.18
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// �ֽ��ʻ����ᣬwrite by duyy, 2013.5.8
		flag = 0;
	}
	if (pacc->flag & ACCF_FREEZE_SUB){
		// �������ᣬwrite by duyy, 2013.5.8
		sflag = 0;
	}
	if (pacc->flag & ACCF_WOFF) {
		// ע����
		feature = 1;
		feature |= (1 << 4);//modified by duyy, 2013.6.18
		//printk("%x cancel\n", cardno);
		goto acc_end;
	}
	// passwd copy
	memcpy(&passwd, pacc->passwd, sizeof(pacc->passwd));
	//printk("password is %x\n", passwd);
	//printk("acc limit:money is %d, times is %d\n", pacc->money_life, pacc->times_life);
	//printk("sub limit:money is %d, times is %d\n", pacc->smoney_life, pacc->stimes_life);
	
	// �ʹα�־
	ptrm->_con_flag |= TRMF_LIMIT;	/* ˮ���ն�ʱȥ��, bug */
	
	//���ֽ��ʻ�����ʹ��ʱ���Ž��н�ֹ����ʱ���жϣ�modified by duyy, 2014.1.15
	if (flag){
		//��ݡ��նˡ�ʱ��ƥ���жϣ�write by duyy, 2013.5.8
		timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//��ݡ��ն˶�Ӧ��ʱ�����
		//printk("double: cardno:%ld timenum = %x\n", cardno, timenum);
		if (timenum & 0x80){//modified by duyy, 2013.5.8,���λbit7��ʾ��ֹ���ѣ�����Ϊ�������ѣ��жϽ�ֹʱ��
			flag = 0;
		}
		else {
			// �ֽ��˻�����ʹ��
			flag = 1;
			for (i = 0; i < 7; i++){
				if (timenum & 0x1){
					if(itm >= term_time[i].begin && itm <= term_time[i].end){
						// �����˻���ֹʹ��
						flag = 0;
						break;
					}
				}
				timenum = timenum >> 1;
			}
		}
	}

	//�������ʻ�����ʹ��ʱ���Ž��н�ֹ����ʱ���жϣ�modified by duyy, 2014.1.15
	if (sflag){
		//��ݡ��նˡ�ʱ��ƥ���жϣ�write by duyy, 2013.5.8
		timenum = ptmnum[pacc->spower_id & 0xF][ptrm->pterm->power_id & 0xF];//��ݡ��ն˶�Ӧ��ʱ�����
		//printk("sub: cardno:%ld timenum = %x\n", cardno, timenum);
		if (timenum & 0x80){//modified by duyy, 2013.5.8,���λbit7��ʾ��ֹ���ѣ�����Ϊ�������ѣ��жϽ�ֹʱ��
			sflag = 0;
		}
		else {
			// �����˻�����ʹ��
			sflag = 1;
			for (i = 0; i < 7; i++){
				if (timenum & 0x1){
					if(itm >= term_time[i].begin && itm <= term_time[i].end){
						// �����˻���ֹʹ��
						sflag = 0;
						break;
					}
				}
				timenum = timenum >> 1;
			}
		}
	}
	if (!flag && !sflag) {
		// ������������
		feature = 1;
		feature |= (1 << 8);//modified by duyy, 2013.6.18
		//printk("%x: flag = %d, sflag = %d\n", cardno, flag, sflag);
		goto acc_end;
	}
	//printk("%x: flag = %d, sflag = %d\n", cardno, flag, sflag);
	//�ֽ��ʻ����
	money = pacc->money;
	//�����ʻ����
	memcpy(&smoney, pacc->sub_money, sizeof(pacc->sub_money));
	if (pacc->sub_money[2] & 0x80){//�������Ϊ��ʱ����Ӧת��
		smoney = ~smoney;
		smoney += 1;
		smoney = smoney & 0xFFFFFF;
		smoney = -smoney;
	}
	//printk("pacc->money is %d\n", pacc->money);
	//printk("pacc: money is %d, smoney is %d\n", money, smoney);
	//printk("%x: st_limit is %x\n", cardno, pacc->st_limit);
	
	if (flag && sflag) {
		if (pacc->st_limit & ACCST_ALLOW_UNIT) {
			// ������˻�ʹ��
			feature |= (1 << 14);//˫�ʻ���������
			if (pacc->st_limit & ACCST_CASH) {
				//�����ֽ��ʻ�����
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.1.13
				if (ptrm->pterm->param.spcl & (1 << 7)){
					//printk("Welfare terminal %d, money is %d, termmoney is %d\n", ptrm->term_no, money, ptrm->_tmoney);
					//�����ն�
					if ((!money) || (!pacc->money_life) || (money < pacc->money_life)){
						//�����˻�ûǮʱ��ֹ����
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
					//���ٶԸ����˻��������жϣ������жϵ������ѽ�����ޣ�modified by duyy, 2014.3.13
					if (ptrm->_tmoney < pacc->money_life){
						//�����ն�������С�ڵ������ѽ��ʱ,�����ѽ��Ϊ�����������
						ptrm->_tmoney = pacc->money_life;
					}
				} else {
					//printk("common terminal %d, pacc->money_life is %d, termmoney is %d\n",
					//			ptrm->term_no, pacc->money_life, ptrm->_tmoney);
					//��ͨ�ն�
					if (money){//�˻���Ǯ���ҵ������ѽ��δ������ʱ�жϵ�ǰʵ�����Ѷwrite by duyy, 2014.3.13
						if (pacc->money_life){//���ٶԸ����˻��������жϣ������жϵ������ѽ�����ޣ�modified by duyy, 2014.3.13
							//�����˻���Ǯ
							if (ptrm->_tmoney < pacc->money_life){
								//�����ն�������С�ڸ����˻����ʱ,�����ѽ��Ϊ�����˻����
								ptrm->_tmoney = pacc->money_life;
							}
						}
					}
				}

#endif
				//�����ֽ��ʻ�ʵ������ֵ
				ret = _cal_disc_proportion(&real_money, ptrm, pacc, pacc->power_id, &proportion);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("proportion is %d\n", proportion);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_money == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_money;
				}
				//ret = _check_cash_unit(&termmoney, &dif_money, pacc);
				ret = _check_cash_value(termmoney, pacc);
				if (ret < 0){
					if (ret == -1){
						//�ж��ʻ�����Ƿ����
						ret = _check_overdraft(termmoney, &money, pacc->draft);
						if (ret < 0){
							feature = 1;
							feature |= (1 << 9);//�ֽ��ʻ����㣬��ֹ����
							goto acc_end;
						}
						feature |= (1 << 11);	//�����������ֽ��ʻ�
						goto acc_end;
					} else if (ret == -2){
						feature |= (1 << 11);	//���������Ѳ����ʻ�
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.3.13
						if (money < 0){//��֧���˻�͸֧
							real_money = 0;
							dif_money = termmoney;
						} else if (money < pacc->money_life){
							dif_money = termmoney - money;
							real_money = money;
							money = 0;
						} else {
							dif_money = termmoney - pacc->money_life;
							real_money = pacc->money_life;
							money -= pacc->money_life;
						}
						ptrm->_tmoney = dif_money;
#else					
				#if 0
						if (proportion == 0xFFFD){
							real_money = 0;
							goto sub_proportion;
						}
						ret = _check_overdraft_proportion(&d_money, &termmoney, &money, pacc->draft);
						if (ret < 0){
							dif_money += d_money;
						}
						if (proportion == 0xFFFF){
							ptrm->_tmoney = dif_money;
							real_money = termmoney;
						} else {
							if (proportion){
								ptrm->_tmoney = _cal_disc_unproportion(dif_money, proportion);	
							} else {
								ptrm->_tmoney = dif_money;
							}	
							real_money = termmoney;
						}			
				#endif
						real_money = 0;
#endif
						goto sub_proportion;
					} else if (ret == -3){		//�����������Ѳ����ʻ�
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.3.13
						if (money < 0){//��֧���˻�͸֧
							real_money = 0;
							dif_money = termmoney;
						} else if (money < pacc->money_life){
							dif_money = termmoney - money;
							real_money = money;
							money = 0;
						} else {
							dif_money = termmoney - pacc->money_life;
							real_money = pacc->money_life;
							money -= pacc->money_life;
						}
						ptrm->_tmoney = dif_money;
#else					
				#if 0
						if (proportion == 0xFFFD){
							real_money = 0;
							goto sub_proportion;
						}
						ret = _check_overdraft_proportion(&d_money, &termmoney, &money, pacc->draft);
						if (ret < 0){
							dif_money += d_money;
						}
						if (proportion == 0xFFFF){
							ptrm->_tmoney = dif_money;
							real_money = termmoney;
						} else {
							if (proportion){
								ptrm->_tmoney = _cal_disc_unproportion(dif_money, proportion);	
							} else {
								ptrm->_tmoney = dif_money;
							}
							real_money = termmoney;
						}	
				#endif 
						real_money = 0;
#endif
						goto sub_proportion;
					} else {
						feature = 1;
						feature |= (1 << 7);	//�����������޽�ֹ����
						goto acc_end;
					}
				} 
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft_proportion(&dif_money, &termmoney, &money, pacc->draft);
				if (ret < 0){
					if (proportion == 0xFFFF){
						ptrm->_tmoney = dif_money;
						real_money = termmoney;
					} else {
						if (proportion && (proportion != 0xFFFD)){
							ptrm->_tmoney = _cal_disc_unproportion(dif_money, proportion);	
						} else {
							ptrm->_tmoney = dif_money;
						}
						real_money = termmoney;
					}
				} else {
					goto acc_end;
				}			
sub_proportion:			
				//printk("ptrm->_tmoney is %d\n", ptrm->_tmoney);
				//�絽�����ʻ�����
				//���㲹���ʻ�ʵ������ֵ
				ret = _cal_disc_proportion(&real_smoney, ptrm, pacc, pacc->spower_id, &sproportion);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("sub proportion is %d\n", sproportion);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_smoney == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_smoney;
				}
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 10);//�����ʻ����㣬��ֹ����
					goto acc_end;
				}
				if (_check_limit_sub(pacc, termmoney) < 0) {
					// �������ƣ��ж��Ƿ����������������
					if (pacc->st_limit & ACCST_SUB_PSWSUB) {
						feature |= (1 << 11);//����������
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}
			} else {
				//���Ȳ����ʻ�����
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.1.13
				if (ptrm->pterm->param.spcl & (1 << 7)){
					//printk("Welfare terminal %d, smoney is %d, pacc->smoney_life is %d, termmoney is %d\n", 
					//			ptrm->term_no, smoney, pacc->smoney_life,ptrm->_tmoney);
					//�����ն�
					if ((!smoney) || (!pacc->smoney_life) || (smoney < pacc->smoney_life)){
						//�����˻�ûǮʱ��ֹ����
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
					//���ٶԲ����˻��������жϣ������жϵ������ѽ�����ޣ�modified by duyy, 2014.3.13
					if (ptrm->_tmoney < pacc->smoney_life){
						//�����ն�������С�ڵ������ѽ��ʱ,�����ѽ��Ϊ�����������
						ptrm->_tmoney = pacc->smoney_life;
					}
				} else {
					//printk("common terminal %d, pacc->smoney_life is %d, termmoney is %d\n",
					//			ptrm->term_no, pacc->smoney_life, ptrm->_tmoney);
					//��ͨ�ն�
					if (smoney){//�����˻���Ǯ���ҵ������ѽ��δ������ʱ�жϵ�ǰʵ�����Ѷwrite by duyy, 2014.3.13
						if (pacc->smoney_life){//���ٶԲ����˻��������жϣ������жϵ������ѽ�����ޣ�modified by duyy, 2014.3.13
							//�����˻���Ǯ
							if (ptrm->_tmoney < pacc->smoney_life){
								//�����ն�������С�ڲ����˻����ʱ,�����ѽ��Ϊ�����˻����
								ptrm->_tmoney = pacc->smoney_life;
							}
						}
					}
				}
#endif
				//���㲹���ʻ�ʵ������ֵ
				ret = _cal_disc_proportion(&real_smoney, ptrm, pacc, pacc->spower_id, &sproportion);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("sub proportion is %d\n", sproportion);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_smoney == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_smoney;
				}
				//ret = _check_sub_unit(&termmoney, &dif_money, pacc);
				ret = _check_sub_value(termmoney, pacc);
				if (ret < 0){
					if (ret == -1){
						//�ж��ʻ�����Ƿ����
						ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
						if (ret < 0){
							feature = 1;
							feature |= (1 << 10);//�����ʻ����㣬��ֹ����
							goto acc_end;
						}
						feature |= (1 << 11);	//���������Ѳ����ʻ�
						goto acc_end;
					} else if (ret == -2){
						feature |= (1 << 11);	//�����������ֽ��ʻ�
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.3.13
						if (smoney < 0){//��֧���˻�͸֧,��֧�ִ�������
							real_smoney = 0;
							dif_money = termmoney;
						} else if (smoney < pacc->smoney_life){
							dif_money = termmoney - smoney;
							real_smoney = smoney;
							smoney = 0;
						} else {
							dif_money = termmoney - pacc->smoney_life;
							real_smoney = pacc->smoney_life;
							smoney -= pacc->smoney_life;
						}
						ptrm->_tmoney = dif_money;
#else
				#if 0
						if (sproportion == 0xFFFD){
							real_smoney = 0;
							goto cash_proportion;
						}
						ret = _check_overdraft_proportion(&d_money, &termmoney, &smoney, pacc->sdraft);
						if (ret < 0){
							dif_money += d_money;
						}
						if (sproportion == 0xFFFF){
							ptrm->_tmoney = dif_money;
							real_smoney = termmoney;
						} else {
							if (sproportion){
								ptrm->_tmoney = _cal_disc_unproportion(dif_money, sproportion);
							}else {
								ptrm->_tmoney = dif_money;
							}
							real_smoney = termmoney;
						}			
				#endif
						real_smoney = 0;
#endif
						goto cash_proportion;
					} else if (ret == -3){    //�������������ֽ��ʻ�
#ifdef CONFIG_GGGS//�ֹܹ�˾�����жϸ����ն�һϵ������Ҫ��write by duyy, 2014.3.13
						if (smoney < 0){//��֧���˻�͸֧����֧�ִ�������
							real_smoney = 0;
							dif_money = termmoney;
						} else if (smoney < pacc->smoney_life){
							dif_money = termmoney - smoney;
							real_smoney = smoney;
							smoney = 0;
						} else {
							dif_money = termmoney - pacc->smoney_life;
							real_smoney = pacc->smoney_life;
							smoney -= pacc->smoney_life;
						}
						ptrm->_tmoney = dif_money;
#else
				#if 0
						if (sproportion == 0xFFFD){
							real_smoney = 0;
							goto cash_proportion;
						}
						ret = _check_overdraft_proportion(&d_money, &termmoney, &smoney, pacc->sdraft);
						if (ret < 0){
							dif_money += d_money;
						}
						if (sproportion == 0xFFFF){
							ptrm->_tmoney = dif_money;
							real_smoney = termmoney;
						} else {
							if (sproportion){
								ptrm->_tmoney = _cal_disc_unproportion(dif_money, sproportion);
							} else {
								ptrm->_tmoney = dif_money;
							}
							real_smoney = termmoney;
						}
				#endif
						real_smoney = 0;
#endif
						goto cash_proportion;
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft_proportion(&dif_money, &termmoney, &smoney, pacc->sdraft);
				if (ret < 0){
					if (sproportion == 0xFFFF){
						ptrm->_tmoney = dif_money;
						real_smoney = termmoney;
					} else {	
						if (sproportion && (sproportion != 0xFFFD)){
							ptrm->_tmoney = _cal_disc_unproportion(dif_money, sproportion);
						}else {
							ptrm->_tmoney = dif_money;
						}
						real_smoney = termmoney;
					}				
				} else {
					goto acc_end;
				}				
cash_proportion:	
				//printk("cash ptrm->_tmoney is %d\n", ptrm->_tmoney);
				//�絽�ֽ��ʻ�����
				//�����ֽ��ʻ�ʵ������ֵ
				ret = _cal_disc_proportion(&real_money, ptrm, pacc, pacc->power_id, &proportion);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("cash proportion is %d\n", proportion);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_money == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_money;
				}
				//printk("term money is %d", termmoney);
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &money, pacc->draft);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 9);//�ֽ��ʻ����㣬��ֹ����
					goto acc_end;
				}		
				// �ж��ֽ��˻����ޣ�����־������λ
				if (_check_limit(pacc, termmoney) < 0) {
					// �������ƣ��ж��Ƿ����������������
					if (pacc->st_limit & ACCST_CASH_PSWCASH) {
						feature |= (1 << 11);	//����������
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}		
			}
		} else {
			// ��������˻�ʹ��
			feature |= (1 << 14);//˫�ʻ���������
			if (pacc->st_limit & ACCST_CASH) {
				//�����ֽ��ʻ�����
				//�����ֽ��ʻ�ʵ������ֵ
				ret = _cal_disc_value(&real_money, ptrm, pacc, pacc->power_id);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_money == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_money;
				}
				//printk("cash cousme money is %d\n", termmoney);
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &money, pacc->draft);
				if (ret < 0){
					real_money = 0;
					goto sub_value;
				}
				ret = _check_cash_value(termmoney, pacc);
				if (ret < 0){
					if (ret == -1){
						feature |= (1 << 11);	//�����������ֽ��ʻ�
						goto acc_end;
					} else if (ret == -2){
						feature |= (1 << 11);	//���������Ѳ����ʻ�
						money += termmoney;//���ֽ��ʻ���ȥ��Ǯ�ټӻ���
						real_money = 0;
						goto sub_value;
					} else if (ret == -3){
						//�����������Ѳ����ʻ�
						money += termmoney;//���ֽ��ʻ���ȥ��Ǯ�ټӻ���
						real_money = 0;
						goto sub_value;
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				} else {
					//��������
					goto acc_end;
				}
sub_value:				
				//�絽�����ʻ�����
				//���㲹���ʻ�ʵ������ֵ
				ret = _cal_disc_value(&real_smoney, ptrm, pacc, pacc->spower_id);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_smoney == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_smoney;
				}
				//printk("sub consume money is %d\n", termmoney);
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 10);//�����ʻ����㣬��ֹ����
					goto acc_end;
				}
				if (_check_limit_sub(pacc, termmoney) < 0) {
					// �������ƣ��ж��Ƿ����������������
					if (pacc->st_limit & ACCST_SUB_PSWSUB) {
						feature |= (1 << 11);//����������
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}
			} else {
				//���Ȳ����ʻ�����
				//���㲹���ʻ�ʵ������ֵ
				ret = _cal_disc_value(&real_smoney, ptrm, pacc, pacc->spower_id);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//printk("sub real money is %d\n", real_smoney);
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_smoney == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_smoney;
				}
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
				if (ret < 0){
					real_smoney = 0;
					goto cash_value;
				}
				ret = _check_sub_value(termmoney, pacc);
				if (ret < 0){
					if (ret == -1){
						feature |= (1 << 11);	//���������Ѳ����ʻ�
						goto acc_end;
					} else if (ret == -2){
						feature |= (1 << 11);	//�����������ֽ��ʻ�
						smoney += termmoney;//�Ѳ����ʻ���ȥ��Ǯ�ټӻ���
						real_smoney = 0;
						goto cash_value;
					} else if (ret == -3){
						//�������������ֽ��ʻ�
						smoney += termmoney;//�Ѳ����ʻ���ȥ��Ǯ�ټӻ���
						real_smoney = 0;
						goto cash_value;
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				} else {
					//��������
					goto acc_end;
				}
cash_value:
				//�絽�ֽ��ʻ�����
				//�����ֽ��ʻ�ʵ������ֵ
				ret = _cal_disc_value(&real_money, ptrm, pacc, pacc->power_id);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
					goto acc_end;
				}
				//�ж�ʵ�����Ѷ��Ƿ�0����
				if (real_money == 0xFFFFFF){
					termmoney = 0;
				} else {
					termmoney = real_money;
				}
				//�ж��ʻ�����Ƿ����
				ret = _check_overdraft(termmoney, &money, pacc->draft);
				if (ret < 0){
					feature = 1;
					feature |= (1 << 9);//�ֽ��ʻ����㣬��ֹ����
					goto acc_end;
				}		
				// �ж��ֽ��˻����ޣ�����־������λ
				if (_check_limit(pacc, termmoney) < 0) {
					// �������ƣ��ж��Ƿ����������������
					if (pacc->st_limit & ACCST_CASH_PSWCASH) {
						feature |= (1 << 11);	//����������
					} else {
						feature = 1;
						feature |= (1 << 7);//�����������޽�ֹ����
						goto acc_end;
					}
				}		
			}
		}
	} else if (flag) {
#ifdef CONFIG_GGGS//�ֹܹ�˾����˫����ʾ��modified by duyy, 2014.2.14
		feature |= (1 << 14);//˫�ʻ���������
#else
		feature |= (1 << 12);//�ֽ��ʻ���������λ
#endif
		//printk("single cash, term money is %d\n", ptrm->_tmoney);
		/* �ۿ��жϣ��������Ѷ� */
		ret = _cal_disc(&real_money, ptrm, pacc, pacc->power_id);
		if (ret < 0){
			feature = 1;
			feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
			goto acc_end;
		}
		//�ж�ʵ�����Ѷ��Ƿ�0����
		if (real_money == 0xFFFFFF){
			termmoney = 0;
		} else {
			termmoney = real_money;
		}
		//�ж��ʻ�����Ƿ����
		ret = _check_overdraft(termmoney, &money, pacc->draft);
		if (ret < 0){
			feature = 1;
			feature |= (1 << 9);//�ֽ��ʻ����㣬��ֹ����
			goto acc_end;
		}	
		
		// �ж��ֽ��˻����ޣ�����־������λ
		if (_check_limit(pacc, termmoney) < 0) {
			// �������ƣ��ж��Ƿ����������������
			if (pacc->st_limit & ACCST_CASH_PSWCASH) {
				feature |= (1 << 11);	//����������
			} else {
				feature = 1;
				feature |= (1 << 7);//�����������޽�ֹ����
				goto acc_end;
			}
		}		
	} else if (sflag) {
#ifdef CONFIG_GGGS//�ֹܹ�˾����˫����ʾ��modified by duyy, 2014.2.14
		feature |= (1 << 14);//˫�ʻ���������
#else
		feature |= (1 << 13);//�����ʻ���������λ
#endif
		//printk("single sub\n");
		/* �ۿ��жϣ��������Ѷ� */
		ret = _cal_disc(&real_smoney, ptrm, pacc, pacc->spower_id);
		if (ret < 0){
			feature = 1;
			feature |= (1 << 5);//����������ն˲�ƥ��,��ֹ����
			goto acc_end;
		}
		//�ж�ʵ�����Ѷ��Ƿ�0����
		if (real_smoney == 0xFFFFFF){
			termmoney = 0;
		} else {
			termmoney = real_smoney;
		}
		//�ж��ʻ�����Ƿ����
		ret = _check_overdraft(termmoney, &smoney, pacc->sdraft);
		if (ret < 0){
			feature = 1;
			feature |= (1 << 10);//�����ʻ����㣬��ֹ����
			goto acc_end;
		}
		// �ж��ֽ��˻����ޣ�����־������λ
		if (_check_limit_sub(pacc, termmoney) < 0) {
			// �������ƣ��ж��Ƿ����������������
			if (pacc->st_limit & ACCST_SUB_PSWSUB) {
				feature |= (1 << 11);//����������
			} else {
				feature = 1;
				feature |= (1 << 7);//�����������޽�ֹ����
				goto acc_end;
			}
		}
	}
	
acc_end:	
	//printk("cardno:%x: money is %d, real_money is %d\n", cardno, money, real_money);
	//printk("cardno:%x: smoney is %d, real_smoney is %d\n", cardno, smoney, real_smoney);	
 	//printk("recv_leid_double feature=%x\n", feature);//2012.5.9
	if (money > 8000000) {//modified by duyy, 2014.4.4
		printk("money is too large!\n");
		money = 8000000;
	}
	// 2�ֽ�ʵ�����Ѷ�feature
	tmp = (unsigned char *)&feature;
	tmp += 1;
	for (i = 0; i < 2; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	// 3�ֽ��ֽ��ʻ����Ѻ����money��16����
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}

	// 3�ֽ��ֽ��ʻ�ʵ�����Ѷ�real_money
	tmp = (unsigned char *)&real_money;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	// �û�����
	tmp = (unsigned char *)&passwd;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	// 3�ֽڲ����ʻ����Ѻ����smoney��16����
	tmp = (unsigned char *)&smoney;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	// 3�ֽڲ����ʻ�ʵ�����Ѷ�real_smoney
	tmp = (unsigned char *)&real_smoney;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	// ����4�ֽ��˺�
	tmp = (unsigned char *)&acc;
	tmp += 3;
	for (i = 0; i < 4; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	// ����7�ֽ�ʱ��
	if (send_data(tm, 7, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 7; i++) {
		//ptrm->add_verify += tm[i];
		ptrm->dif_verify ^= tm[i];
		ptrm->add_verify += tm[i];
	}
	// ����12�ֽ�����
	if (send_data(name, 12, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 12; i++) {
		ptrm->dif_verify ^= name[i];
		ptrm->add_verify += name[i];
	}

	if (verify_all(ptrm, CHKALL) < 0)
		return -1;
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
	return 0;
}

/*
 * the function is only receive pos number, and send dis_virify
 * created by wjzhe, 10/19/2006
 */
int ret_no_only(term_ram *ptrm)
{
	return verify_all(ptrm, CHKXOR);
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
		pr_debug("ptrm add_verify is %2x,dif_verify is %2x, tmp[0] is %2x, tmp[1] is %2x\n",
			ptrm->add_verify, ptrm->dif_verify, tmp[0], tmp[1]);
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
 * ����ʱ�ε��ж�, flag = 1˵���ڴ�ʱ�����ж���
 */
int recv_le_id(term_ram *ptrm, int allow, int itm)
{
	unsigned int cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0;
	unsigned char *tmp;
	int i, ret;
	
	ptrm->_tmoney = 0;

	tmp = (unsigned char *)&cardno;
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
	//printk("-----recv_le id-----\n");//added by duyy,2013.6.18
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
		pr_debug("not allow: %d\n", cardno);
		feature = 1;
		money = 0;
		pacc = NULL;
		goto acc_end;
	}
	
	// search id
	pacc = search_id(cardno);
	//pr_debug("search over\n");
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("%x not find\n", cardno);
		feature = 1;
		goto acc_end;
	}
	feature = 4;
	// �����������ƽ����˻��ж�
	if (pacc->flag & ACCF_LOSS) {
		// ���ǹ�ʧ��
		feature |= 1 << 5;
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// ע��
		feature = 1;
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// ����
		feature = 1;
		goto acc_end;
	}

	//��ݡ��նˡ�ʱ��ƥ���жϣ�write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//��ݡ��ն˶�Ӧ��ʱ�����
	pr_debug("cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,���λbit7��1��ʾ��ֹ���ѣ���0��ʾ����λ�������ѣ��жϽ�ֹʱ��
		feature = 1;
		//printk("whole day prohibit\n");
		goto acc_end;
	}
	else {
		for (i = 0; i < 7; i++){
			if (timenum & 0x1){
				if(itm >= term_time[i].begin && itm <= term_time[i].end){
					feature = 1;
					pr_debug("term_time[%d] is prohibited, begin %d,end %d\n", i, term_time[i].begin, term_time[i].end);
					goto acc_end;
				}
			}
			timenum = timenum >> 1;
			//printk("timenum is %x\n", timenum);
		}
		//printk("continue consume\n");
	}
	
	// ���ɻ����˿�������ڲʹβ���,modified by duyy, 2013.6.18
	if ((!(ptrm->pterm->param.term_type & 0x20)) && (!(ptrm->pterm->param.term_type & 0x4))) {
		/* �ʹβ��޼�� */
		ptrm->_con_flag |= TRMF_LIMIT;	/* ˮ���ն�ʱȥ��, bug */
		if (_check_limit(pacc, ptrm->_tmoney)) {
			// �ʹε���
			if (pacc->st_limit & 1) {
				feature |= (1 << 3);
			} else {
				feature = 1;
				goto acc_end;
			}
		}
	}
	
	money = pacc->money;
	if (money > 8000000) {//modified by duyy, 2014.6.13
		printk("money is too large!\n");
		money = 8000000;
	}

acc_end:
	pr_debug("feature is %x\n", feature);//2013.5.9
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;

	money = binl2bcd(money) & 0xFFFFFF;
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	if (verify_all(ptrm, CHKXOR) < 0) {
		return -1;
	}
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {//modified by duyy,2013.6.21
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
					cashbuf[i].managefee = fee[(pacc->flag >> 4) & 0xF];
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
			cashbuf[cashterm_ptr].managefee = fee[(pacc->flag >> 4) & 0xF];
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
 * flag: bit0 is 0, consum is HEX; 1, BCD
 *		 bit1 is 1, CHKALL, modified by wjzhe
 */
int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int tail;
	unsigned char *tmp;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	
	// first recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			pr_debug("%d recv leflow cardno %d ret %d\n", ptrm->term_no, i, ret);
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
			pr_debug("%d recv leflow consume_sum %d ret %d\n", ptrm->term_no, i, ret);
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
	
	if (flag & 0x1) {		// if BCD then convert BIN
		leflow.consume_sum = bcdl2bin(leflow.consume_sum);
	}
	
	// third check NXOR
	if (flag & 0x2) {
		if (verify_all(ptrm, CHKALL) < 0) {
			pr_debug("%d recv le flow verify chkxor error\n", ptrm->term_no);
			return -1;
		}
	} else if (flag & 0x1) {
		if (verify_all(ptrm, CHKXOR) < 0) {
			pr_debug("%d recv le flow verify chkxor error\n", ptrm->term_no);
			return -1;
		}
	} else {
		if (verify_all(ptrm, CHKNXOR) < 0) {
			pr_debug("%d recv le flow verify chkxor error\n", ptrm->term_no);
			return -1;
		}
	}
	
	// check if receive the flow
	if (ptrm->flow_flag) {
		return 0;
	}
	
	space_remain--;

	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		pr_debug("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	
	// adjust money limit
	sub_money(pacc, leflow.consume_sum);

	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}

	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum;

	leflow.flow_type = LECONSUME;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm++;
	leflow.tml_num = ptrm->term_no;
	leflow.acc_num = pacc->acc_num;

	// ��ˮ�ŵĴ���
	leflow.flow_num = maxflowno++;

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

// ����˫У���־
int recv_take_money_v1(term_ram *ptrm, unsigned char *tm, int flag)
{
	le_flow leflow;
	int ret, i, tail;
	acc_ram *pacc;
	unsigned char *tmp;
	memset(&leflow, 0, sizeof(leflow));
	// terminal property
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		//usart_delay(7);
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
		ptrm->add_verify += *tmp;
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
		ptrm->add_verify += *tmp;
		tmp--;
	}
	if (flag) {
		if (verify_all(ptrm, CHKALL) < 0) {
			ptrm->status = NOCOM;
			return 0;
		}
	} else {
		if (verify_all(ptrm, CHKXOR) < 0) {
			ptrm->status = NOCOM;
			return 0;
		}
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
	// �����洢��ˮ
	leflow.flow_type = LETAKE;
	leflow.acc_num = pacc->acc_num;
	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
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
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
		//modified by duyy, 2013.6.21
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

// ��ȡ�����ˮ
int recv_dep_money_v1(term_ram *ptrm, unsigned char *tm, int flag)
{
	le_flow leflow;
	int ret, i, tail, managefee;
	acc_ram *pacc;
	unsigned char *tmp;
	// terminal property
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		//usart_delay(7);
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
		ptrm->add_verify += *tmp;
		tmp--;
	}
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 1;
	for (i = 0; i < 2; i++) {		//����2bytes���bcd�룩
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM)
				usart_delay(3 - i);
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	if (flag) {
		if (verify_all(ptrm, CHKALL) < 0) {
			ptrm->status = NOCOM;
			return 0;
		}
	} else {
		if (verify_all(ptrm, CHKXOR) < 0) {
			ptrm->status = NOCOM;
			return 0;
		}
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
	leflow.consume_sum = bcdl2bin(leflow.consume_sum) * 100;
	// �ָ�����
#if 0
	if ((pacc->feature & 0xF) != 0xF) {
		pacc->feature &= 0xF0;
		pacc->feature |= 0xE;
	}
#endif

	managefee = fee[(pacc->flag >> 4) & 0xF];		//����ѱ���
	managefee = managefee * leflow.consume_sum / 100;	//��������
	pacc->money += leflow.consume_sum - managefee;		//����Ǯ���ܷ�

	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �����洢��ˮ
	leflow.flow_type = LECHARGE;
	leflow.acc_num = pacc->acc_num;
	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
		*tmp = *tm;
		tmp--;
		tm++;
	}
	// ��ˮ�ŵĴ���
	leflow.flow_num = maxflowno++;
	leflow.tml_num = ptrm->term_no;
	leflow.manage_fee = 0 - managefee;
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// �޸���ˮָ��
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
		//modified by duyy, 20136.21
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

/*
 * ���ɻ����˿��Ҫ�����������˺š�ʱ�䡢������Ϣ��write by duyy, 2014.6.6
 */
int recv_le_id_double_ticket(term_ram *ptrm, int allow, int itm, unsigned char *tm)
{
	unsigned int cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0;
	unsigned char *tmp;
	int i, ret;
	unsigned long acc = 0;//write by duyy, 20174.6.6
	char name[12] = {0};//write by duyy, 2014.6.6

	ptrm->_tmoney = 0;

	tmp = (unsigned char *)&cardno;
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
	//printk("-----recv_le id double-----\n");//added by duyy,2013.6.18
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
		pr_debug("not allow: %d\n", cardno);
		feature = 1;
		money = 0;
		pacc = NULL;
		goto acc_end;
	}
	
	// search id
	pacc = search_id(cardno);
	//pr_debug("search over\n");
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("%x not find\n", cardno);
		feature = 1;
		goto acc_end;
	}
	
	acc = pacc->acc_num;//write by duyy, 2014.6.6
	memcpy(name, pacc->user_name, sizeof(name));//write by duyy, 2014.6.6

	feature = 4;
	// �����������ƽ����˻��ж�
	if (pacc->flag & ACCF_LOSS) {
		// ���ǹ�ʧ��
		feature |= 1 << 5;
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// ע��
		feature = 1;
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// ����
		feature = 1;
		goto acc_end;
	}
	//��ݡ��նˡ�ʱ��ƥ���жϣ�write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//��ݡ��ն˶�Ӧ��ʱ�����
	pr_debug("cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,���λbit7��1��ʾ��ֹ���ѣ���0��ʾ����λ�������ѣ��жϽ�ֹʱ��
		feature = 1;
		//printk("whole day prohibit\n");
		goto acc_end;
	}
	else {
		for (i = 0; i < 7; i++){
			if (timenum & 0x1){
				if(itm >= term_time[i].begin && itm <= term_time[i].end){
					feature = 1;
					pr_debug("term_time[%d] is prohibited, begin %d,end %d\n", i, term_time[i].begin, term_time[i].end);
					goto acc_end;
				}
			}
			timenum = timenum >> 1;
			//printk("timenum is %x\n", timenum);
		}
		//printk("continue consume\n");
	}

	money = pacc->money;
	if (money > 8000000) {//modified by duyy, 2014.6.13
		printk("money is too large!\n");
		money = 8000000;
	}
acc_end:
	pr_debug("feature is %x\n", feature);//2013.5.9
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;

	//money = binl2bcd(money) & 0xFFFFFF;
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	// ����4�ֽ��˺�
	tmp = (unsigned char *)&acc;
	tmp += 3;
	for (i = 0; i < 4; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	// ����7�ֽ�ʱ��
	if (send_data(tm, 7, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 7; i++) {
		//ptrm->add_verify += tm[i];
		ptrm->dif_verify ^= tm[i];
		ptrm->add_verify += tm[i];
	}
	// ����12�ֽ�����
	if (send_data(name, 12, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 12; i++) {
		ptrm->dif_verify ^= name[i];
		ptrm->add_verify += name[i];
	}

	if (verify_all(ptrm, CHKALL) < 0) {
		return -1;
	}
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {//modified by duyy,2013.6.21
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
					cashbuf[i].managefee = fee[(pacc->flag >> 4) & 0xF];
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
			cashbuf[cashterm_ptr].managefee = fee[(pacc->flag >> 4) & 0xF];
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
 * ���ɻ����˿��Ҫ������write by duyy, 2013.6.18
 */
int recv_le_id_double(term_ram *ptrm, int allow, int itm)
{
	unsigned int cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0;
	unsigned char *tmp;
	int i, ret;
	
	ptrm->_tmoney = 0;

	tmp = (unsigned char *)&cardno;
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
	//printk("-----recv_le id double-----\n");//added by duyy,2013.6.18
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
		pr_debug("not allow: %d\n", cardno);
		feature = 1;
		money = 0;
		pacc = NULL;
		goto acc_end;
	}
	
	// search id
	pacc = search_id(cardno);
	//pr_debug("search over\n");
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("%x not find\n", cardno);
		feature = 1;
		goto acc_end;
	}
	feature = 4;
	// �����������ƽ����˻��ж�
	if (pacc->flag & ACCF_LOSS) {
		// ���ǹ�ʧ��
		feature |= 1 << 5;
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// ע��
		feature = 1;
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// ����
		feature = 1;
		goto acc_end;
	}
	//��ݡ��նˡ�ʱ��ƥ���жϣ�write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//��ݡ��ն˶�Ӧ��ʱ�����
	pr_debug("cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,���λbit7��1��ʾ��ֹ���ѣ���0��ʾ����λ�������ѣ��жϽ�ֹʱ��
		feature = 1;
		//printk("whole day prohibit\n");
		goto acc_end;
	}
	else {
		for (i = 0; i < 7; i++){
			if (timenum & 0x1){
				if(itm >= term_time[i].begin && itm <= term_time[i].end){
					feature = 1;
					pr_debug("term_time[%d] is prohibited, begin %d,end %d\n", i, term_time[i].begin, term_time[i].end);
					goto acc_end;
				}
			}
			timenum = timenum >> 1;
			//printk("timenum is %x\n", timenum);
		}
		//printk("continue consume\n");
	}

	money = pacc->money;
	if (money > 8000000) {//modified by duyy, 2014.6.13
		printk("money is too large!\n");
		money = 8000000;
	}

acc_end:
	pr_debug("feature is %x\n", feature);//2013.5.9
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;

	//money = binl2bcd(money) & 0xFFFFFF;
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	if (verify_all(ptrm, CHKALL) < 0) {
		return -1;
	}
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {//modified by duyy,2013.6.21
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
					cashbuf[i].managefee = fee[(pacc->flag >> 4) & 0xF];
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
			cashbuf[cashterm_ptr].managefee = fee[(pacc->flag >> 4) & 0xF];
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

// ����ȡ�����������˫У���־,write by duyy, 2013.6.18
int recv_take_money_double(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i, tail;
	acc_ram *pacc;
	unsigned char *tmp;
	memset(&leflow, 0, sizeof(leflow));
	// terminal property
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		//usart_delay(7);
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
		ptrm->add_verify += *tmp;
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
		ptrm->add_verify += *tmp;
		tmp--;
	}
	
	if (verify_all(ptrm, CHKALL) < 0) {
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
	// �����洢��ˮ
	leflow.flow_type = LETAKE;
	leflow.acc_num = pacc->acc_num;
	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
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
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
		//modified by duyy, 2013.6.21
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

// ��ȡ�����ˮ,˫У�飬write by duyy, 2013.6.18
int recv_dep_money_double(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i, tail, managefee;
	int real_money = 0;
	int money = 0;
	acc_ram *pacc;
	unsigned char *tmp;
	// terminal property
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		//usart_delay(7);
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
		ptrm->add_verify += *tmp;
		tmp--;
	}
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 1;
	for (i = 0; i < 2; i++) {		//����2bytes���bcd�룩
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM)
				usart_delay(3 - i);
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	
	if (verify_all(ptrm, CHKALL) < 0) {
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
	leflow.consume_sum = bcdl2bin(leflow.consume_sum) * 100;
	
	managefee = fee[(pacc->flag >> 4) & 0xF];		//����ѱ���
	managefee = managefee * leflow.consume_sum / 100;	//��������
	pacc->money += leflow.consume_sum - managefee;		//����Ǯ���ܷ�
	money = pacc->money;
	
	ptrm->dif_verify = 0;//���У������
	real_money = leflow.consume_sum - managefee; //ʵ�ʿ��ô����
	real_money /= 100;
	real_money = binl2bcd(real_money) & 0xFFFF;
	
	tmp = (unsigned char *)&real_money;
	tmp += 1;
	//����ʵ�ʿ��ô����,BCD��
	for (i = 0; i < 2; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
	}

	tmp = (unsigned char *)&money;
	tmp += 2;
	//�����˻����,16����
	for (i = 0; i < 3; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
	}
	//�������У��
	if (send_byte(ptrm->dif_verify, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �����洢��ˮ
	leflow.flow_type = LECHARGE;
	leflow.acc_num = pacc->acc_num;
	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
		*tmp = *tm;
		tmp--;
		tm++;
	}
	// ��ˮ�ŵĴ���
	leflow.flow_num = maxflowno++;
	leflow.tml_num = ptrm->term_no;
	leflow.manage_fee = 0 - managefee;
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// �޸���ˮָ��
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
		//modified by duyy, 2013.6.21
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

// �����˿����ˮ��write by duyy, 2013.6.18
int recv_refund_flow(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i, tail;
	acc_ram *pacc;
	unsigned char *tmp;
	memset(&leflow, 0, sizeof(leflow));
	// terminal property
	if (!(ptrm->pterm->param.term_type & 0x4)) {
		//usart_delay(7);
		return 0;
	}
	//printk("refund flow\n");//2013.6.17
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
		ptrm->add_verify += *tmp;
		tmp--;
	}
	
	if (verify_all(ptrm, CHKALL) < 0) {
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
	
	leflow.consume_sum = pacc->money;
	// ֻ��������, ���޲�����
	pacc->money = 0;
	
	// �ն˲�������
	ptrm->term_money -= leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �����洢��ˮ
	leflow.flow_type = LEREFUND;
	leflow.acc_num = pacc->acc_num;
	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
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
/*
 * terminal type: 1-byte; le_card once consume: 1-byte; food price: 32-byte; card id: 2-byte; time: 7-byte; time seg: 1-byte
 * return 0 is ok; -1 is send error; -2 is variable error
 */
int send_run_data_double(term_ram *ptrm, unsigned char *tm)
{
	int ret, i;
	unsigned char tmp;
	unsigned char code[32] = {0};
	unsigned char cardid[2] = {0};
	// first send 1-byte terminal type (HEX)
	ret = send_data((char *)&ptrm->pterm->param.term_type, 1, ptrm->term_no);
	if (ret < 0) {
		pr_debug("send run data time out!\n");
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->param.term_type;
	ptrm->add_verify += (unsigned char)ptrm->pterm->param.term_type;
	// sencond send 1-byte max consume once (BCD)
	tmp = BIN2BCD(ptrm->pterm->param.max_consume);
	ret = send_data((char *)&tmp, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)tmp;
	ptrm->add_verify += (unsigned char)tmp;
	// cash code or price
	if (ptrm->pterm->param.term_type & (1 << CASH_TERMINAL_FLAG)) {
		memcpy(code, ptrm->pterm->param.term_passwd,
			sizeof(ptrm->pterm->param.term_passwd));
	} else {
		memcpy(code, ptrm->pterm->price, sizeof(code));
	}
	ret = send_data((char *)code, 32, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 32; i++) {
		ptrm->dif_verify ^= code[i];
		ptrm->add_verify += code[i];
	}
	//send 2-byte card id
	cardid[1] = (ptrm->pterm->param.card_id & 0xFF);
	cardid[0] = (ptrm->pterm->param.card_id & 0xFF00) >> 8;
	ret = send_data((char *)cardid, 2, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 2; i++) {
		ptrm->dif_verify ^= cardid[i];
		ptrm->add_verify += cardid[i];
	}
	//send 7-byte time
	ret = send_data((char *)tm, 7, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 7; i++) {
		ptrm->dif_verify ^= tm[i];
		ptrm->add_verify += tm[i];
	}
	//send 1-byte current consume time seg
	tmp = current_id;
	ret = send_data((char *)&tmp, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)tmp;
	ptrm->add_verify += (unsigned char)tmp;
	
	ret = send_data((char *)&ptrm->dif_verify, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ret = send_data((char *)&ptrm->add_verify, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
#if 0
	if (verify_all(ptrm, CHKALL) < 0) {
		pr_debug("send run data verify all error\n");
		return -1;
	}
#endif
	return 0;
}
/*����СƱ��ӡ���������֣�write by duyy,2014.6.9*/
int send_ticket_headline(term_ram *ptrm)
{
	int ret, i;
	char hdline[40] = {0};
	
	memcpy(hdline, headline, sizeof(hdline));
	ret = send_data((char *)hdline, 40, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 40; i++) {
		ptrm->dif_verify ^= hdline[i];
		ptrm->add_verify += hdline[i];
	}
	
	ret = send_data((char *)&ptrm->dif_verify, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ret = send_data((char *)&ptrm->add_verify, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
#if 0
	if (verify_all(ptrm, CHKALL) < 0) {
		pr_debug("send run data verify all error\n");
		return -1;
	}
#endif
	return 0;
}

#endif/* CONFIG_UART_V2 */


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

/*
 * ���������ֽ���ˮ:
 * ֻ���޸��˻����򻻿����˻����
 */
int deal_money(money_flow *pnflow/* , acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd */)
{
	acc_ram *pacc = NULL;
	if (pnflow == NULL/* || pacc_m == NULL || prcd == NULL */) {
		return -1;
	}
	pacc = search_id(pnflow->CardNo);
	if (pacc) {
#ifdef CONFIG_UART_V2
		// ��ʱӦ�����������̴�ʱ��
		if (pnflow->AccoType) {
			// ���벹���˻�
			//printk("sub money is %d, pnflow money is %d\n", pacc->sub_money, pnflow->Money);
			//printk("pacc->smoney_life is %d, pacc->stimes_life is %d\n", pacc->smoney_life, pacc->stimes_life);
			_add_byte3(pacc->sub_money, pnflow->Money);
			//pacc->sub_money += pnflow->Money;
			if (pnflow->LimitType && is_current_tmsg(pnflow->time)) {
				if (pnflow->Money < 0){
				pnflow->Money = -pnflow->Money;
				sub_smoney(pacc, pnflow->Money);
				}
			}
			//printk("cardno %x limittype is %x\n", pnflow->CardNo, pnflow->LimitType);//added by duyy, 2012.9.13
			//printk("pacc->smoney_life is %d, pacc->stimes_life is %d\n", pacc->smoney_life, pacc->stimes_life);
		} else {
			// ��������˻�
			//printk("money is %d, pnflow money is %d\n", pacc->money, pnflow->Money);
			//printk("pacc->money_life is %d, pacc->times_life is %d\n", pacc->money_life, pacc->times_life);
			pacc->money += pnflow->Money;
			if (pnflow->LimitType && is_current_tmsg(pnflow->time)) {
				if (pnflow->Money < 0){
				pnflow->Money = -pnflow->Money;
				sub_money(pacc, pnflow->Money);
				}
			}
			//printk("cardno %x limittype is %x\n", pnflow->CardNo, pnflow->Money);//added by duyy, 2012.9.13
			//printk("pacc->money_life is %d, pacc->times_life is %d\n", pacc->money_life, pacc->times_life);
		}
		
#else
		pacc->money += pnflow->Money;//=================
#endif
	} else {
		pr_debug("deal money error %x\n", pnflow->CardNo);
	}
#if 0
	pacc = search_no2(pnflow->CardNo, pacc_m, prcd->account_main);
	if (pacc) {
		// �ҵ���, ��������ǲ��ǻ����Ŀ�
		if ((pacc->feature & 0xC0) == 0xC0) {
			// ��Ƭ�ǻ�����, ����Ҫ����������
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
#endif
	return 0;
}

#ifdef CONFIG_UART_V2
int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m, 
	struct hashtab *hsw, struct record_info *prcd)
{
	acc_ram *pacc;
	acc_ram oldacc;
	int cnt;
	if (pnflow == NULL || pacc_m == NULL || hsw == NULL) {
		return -1;
	}
	switch (pnflow->Type) {
	case NOM_LOSS_CARD:
		pacc = search_id(pnflow->OldCardId);
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->flag |= ACCF_LOSS;
		break;
	case NOM_LOGOUT_CARD:
		pacc = search_id(pnflow->OldCardId);
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->flag |= ACCF_WOFF;
		break;
	case NOM_FIND_CARD:
		pacc = search_id(pnflow->OldCardId);
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->flag &= ~(ACCF_LOSS);
		break;
	case NOM_CHANGED_CARD:
		prcd->account_sw += 1;
		if (prcd->account_sw > MAXSWACC) {
			prcd->account_sw--;
			printk("change card area full!\n");
			return -1;
		}
		
		// �۰��������˻���
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
		// �ж��Ƿ�Ϊ�Ѿ������Ŀ�
		if (pacc && (pacc->power_id & ACCP_CHANGE)) {
			// �Ѿ������Ŀ�
			pacc = NULL;
		}
		// �����������˻����򻻹��Ŀ������hash acc
		if (pacc == NULL) {
			// ɾ��hash�е�����
			pacc = hashtab_remove_d(hsw, (void *)pnflow->OldCardId);
			prcd->account_sw--;
			if (pacc == NULL) {
				// hash�е�����ɾ�����󣬴˿��������˻�Ҳ����hash�˻���
				printk("waring card %d not in main and hash\n", pnflow->OldCardId);
				return 0;
			}
			
			// ����ԭ������, ��������
			memcpy(&oldacc, pacc, sizeof(oldacc));
			oldacc.card_num = pnflow->NewCardId;
			kfree(pacc);
			pacc = NULL;
		} else {
			// ���˻������д�����, �������ݸ��Ƶ�oldacc������־Ϊ�����Ŀ�
			memcpy(&oldacc, pacc, sizeof(oldacc));
			oldacc.card_num = pnflow->NewCardId;
			pacc->power_id |= ACCP_CHANGE;
			prcd->account_all += 1;
		}
		{
			acc_ram *acc_new;
			acc_new = kmalloc(sizeof(*acc_new), GFP_KERNEL);
			if (acc_new == NULL) {
				return 0;
			}
			memcpy(acc_new, &oldacc, sizeof(*acc_new));
			// hash���м��뻻���Ŀ���Ϣ
			if (hashtab_insert(hsw, (void *)acc_new->card_num, acc_new) < 0) {
				kfree(acc_new);
				return 0;
			}
		}
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
		break;
	case NOM_CHGPASSWD:
		pacc = search_id(pnflow->OldCardId);
		if (pacc == NULL) {
			pr_debug("NOM_CHGPASSWD card %d error\n", pnflow->OldCardId);
			return 0;
		}
		memcpy(pacc->passwd, &pnflow->NewCardId, sizeof(pacc->passwd));
		break;
	case NOM_FREEZE_CARD://wtite by duyy, 2013.5.6
		pacc = search_id(pnflow->OldCardId);
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->flag |= ACCF_FREEZE;
		break;
	case NOM_UNFREEZE_CARD://wtite by duyy, 2013.5.6
		pacc = search_id(pnflow->OldCardId);
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->flag &= ~(ACCF_FREEZE);
		break;
	case NOM_SUBFREEZE_CARD://wtite by duyy, 2013.5.6
		pacc = search_id(pnflow->OldCardId);
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->flag |= ACCF_FREEZE_SUB;
		break;
	case NOM_SUBUNFREEZE_CARD://wtite by duyy, 2013.5.6
		pacc = search_id(pnflow->OldCardId);
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->flag &= ~(ACCF_FREEZE_SUB);
		break;
	default:
		printk("no money flow: unknown type %d\n", pnflow->Type);
		break;
	}
	return 0;
}

int new_reg(acc_ram *acc)
{
	struct record_info *prcd = &rcd_info;
	struct hashtab *hsw = hash_accsw;
	acc_ram *pacc;
	prcd->account_sw += 1;
	if (prcd->account_sw >= MAXSWACC) {
		prcd->account_sw--;
		printk("change card area full!\n");
		return -1;
	}
	// ���뵽������
	pacc = hashtab_remove_d(hsw, (void *)acc->card_num);
	if (pacc) {
		// �Ѿ��ڻ�������, ɾ��
		prcd->account_sw -= 1;
		printk("card no %08x exsit in acc sw\n", (int)acc->card_num);
		kfree(pacc);
	} else { 
		prcd->account_all += 1;
	}
	// ���뵽��������
	{
		int ret;
		acc_ram *acc_new;
		acc_new = kmalloc(sizeof(acc_ram), GFP_KERNEL);
		if (acc_new == NULL) {
			pr_debug("out of memory\n");
			return 0;
		}
		memcpy(acc_new, acc, sizeof(*acc_new));
		ret = hashtab_insert(hsw, (void *)acc_new->card_num, acc_new);
		if (ret < 0) {
			pr_debug("hash insert out of memory\n");
			kfree(acc_new);
			return 0;
		}
	}
	return 0;
}
#else
int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m,
	struct hashtab *hsw, struct record_info *prcd)
{
	acc_ram *pacc;
	unsigned char tmp;
	acc_ram oldacc;
	int cnt;
	if (pnflow == NULL || pacc_m == NULL || hsw == NULL) {
		return -1;
	}
	switch (pnflow->Type) {
	case NOM_LOSS_CARD:
	case NOM_LOGOUT_CARD:
		pacc = search_id(pnflow->CardId);
#if 0
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
#endif
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->CardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		pacc->feature |= 0x80;
		break;
	case NOM_FIND_CARD:
		pacc = search_id(pnflow->CardId);
#if 0
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
#endif
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->CardId);
			return 0;
		}
		pacc->feature &= 0x7F;
		break;
	case NOM_SET_THIEVE:
		pacc = search_id(pnflow->CardId);
#if 0
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
#endif
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->CardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		pacc->feature |= 0x40;
		break;
	case NOM_CLR_THIEVE:
		pacc = search_id(pnflow->CardId);
#if 0
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
#endif
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
		// �ж��Ƿ�Ϊ�Ѿ������Ŀ�
		if (pacc && (pacc->feature & 0xC0)) {
			pacc = NULL;
		}
		if (pacc == NULL) {
			pacc = hashtab_remove_d(hsw, (void *)pnflow->OldCardId);
			prcd->account_sw--;
			if (pacc == NULL) {
				return 0;
			}
			oldacc.feature = pacc->feature;
			oldacc.acc_num = pacc->acc_num;
			oldacc.card_num = pnflow->NewCardId;
			oldacc.managefee = pacc->managefee;
			oldacc.money = pacc->money;
			kfree(pacc);
			pacc = NULL;
		} else {
			oldacc.feature = pacc->feature;
			oldacc.acc_num = pacc->acc_num;
			oldacc.card_num = pnflow->NewCardId;
			oldacc.managefee = pacc->managefee - 1;
			oldacc.money = pacc->money;
			pacc->feature &= 0x3F;
			pacc->feature |= 0xC0;
			prcd->account_all += 1;
		}
		{
			acc_ram *acc_new;
			acc_new = kmalloc(sizeof(*acc_new), GFP_KERNEL);
			if (acc_new == NULL) {
				return 0;
			}
			memcpy(acc_new, &oldacc, sizeof(*acc_new));
			if (hashtab_insert(hsw, (void *)acc_new->card_num, acc_new) < 0) {
				kfree(acc_new);
				return 0;
			}
		}
		break;
	case NOM_CURRENT_RGE:
#if 0
		// �¼ӵĿ���������ע��Ŀ�
		if (search_id(pnflow->CardId)) {
			printk("new regist card exist\n");
			return -1;
		}
#endif
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
		tmp |= (pnflow->PowerId & 0x3) << ACCOUNTCLASS;// ���
		switch (pnflow->Flag) {		// ��ʧ��־
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
		// ���뵽������
		pacc = hashtab_remove_d(hsw, (void *)oldacc.card_num);
		if (pacc) {
			// �Ѿ��ڻ�������, ɾ��
			prcd->account_sw -= 1;
			printk("card no %08x exsit in acc sw\n", (int)oldacc.card_num);
			kfree(pacc);
		} else { 
			prcd->account_all += 1;
		}
		// ���뵽��������
		{
			acc_ram *acc_new;
			acc_new = kmalloc(sizeof(acc_ram), GFP_KERNEL);
			if (acc_new == NULL) {
				printk("out of memory\n");
				return 0;
			}
			memcpy(acc_new, &oldacc, sizeof(*acc_new));
			if (hashtab_insert(hsw, (void *)acc_new->card_num, acc_new) < 0) {
				kfree(acc_new);
				return 0;
			}
		}
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
		break;
	default:
		break;
	}
	return 0;
}
#endif

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
#ifdef CONFIG_UART_V2
	pbacc->card_num = pnflow->OldCardId;
#else
	pbacc->card_num = pnflow->CardId;
#endif
	pbacc->opt = pnflow->BLTypeId;
	if (ctoutm(&tm, pnflow->FlowTime) < 0)
		printk("blk time error: %s\n", pnflow->FlowTime);
	pbacc->t_num.s_tm.hyear = tm.hyear;
	pbacc->t_num.s_tm.lyear = tm.lyear;
	pbacc->t_num.s_tm.mon = tm.mon;
	pbacc->t_num.s_tm.day = tm.mday;
	pbacc->t_num.s_tm.hour = tm.hour;
	memcpy(&pbacc->t_num.num, &pnflow->OperationID, sizeof(pbacc->t_num.num));
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

