/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：deal_data.c
 * 摘    要：完成支持TS11的数据处理函数
 *			 支持新的终端机, 将TS11处理函数去掉
 *			 支持2.1C网络异区流水处理函数
 *			 支持钱包终端和存折终端混用 v2支持 CONFIG_UART_V2
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
			// 这是一张换过的卡
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
			// 换过的卡
			pacc = hashtab_search(hash_accsw, (void *)cardno);
		}
#else
		if ((pacc->feature & 0xC0) == 0xC0) {
			// 这是一张换过的卡
			pacc = NULL;
			pacc = hashtab_search(hash_accsw, (void *)cardno);
		}
#endif
	}
	return pacc;
}

#ifdef CONFIG_UART_V2

// 三字节减法, 不允许负数
static inline void _sub_byte3(u8 *src, int val)
{
	int tmp = 0;
	memcpy(&tmp, src, 3);
	tmp -= val;
	if (tmp < 0) {
		tmp = 0;
	}
	memcpy(src, &tmp, 3);
	return;
}

// 三字节加法, 不允许负数
static inline void _add_byte3(u8 *src, int val)
{
	int tmp = 0;
	memcpy(&tmp, src, 3);
	tmp += val;
	if (tmp < 0) {
		tmp = 0;
	}
	memcpy(src, &tmp, 3);
	return;
}

/* 当餐餐次餐限处理函数 */
int sub_money(acc_ram *pacc, int money)
{
	int tmp = pacc->money_life;
	tmp -= money;
	if (tmp < 0) {
		tmp = 0;
	}
	pacc->money_life = tmp;
	if (pacc->times_life) {
		pacc->times_life--;
	}
	return 0;
}

int sub_smoney(acc_ram *pacc, int money)
{
	int tmp = pacc->smoney_life;
	tmp -= money;
	if (tmp < 0) {
		tmp = 0;
	}
	pacc->smoney_life = tmp;
	if (pacc->stimes_life) {
		pacc->stimes_life--;
	}
	return 0;
}

/* itm是否在当前时段内 */
int is_current_tmsg(unsigned int itm)
{
	user_tmsg *usrtmsg = usrcfg.tmsg;
	int id = current_id;
	// 多进程考虑...
	return (itm >= usrtmsg[id].begin && itm <= usrtmsg[id].end);
}

/* 判断某类终端上是否可用 */
int is_term_allow(u8 acc_pid, u8 term_pid)
{
	term_allow *allow = usrcfg.tallow;
	return allow[acc_pid] & (1 << term_pid);
}

/* 判断此时段是否可用 禁止时段判断 */
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

/* 折扣终端是否可用 */
int is_disc_allow(int termno, u32 *t_mask)
{
	// 判断终端是否要进行限制,termno应判断一下范围，是不是在0－255之间
	return (t_mask[termno >> 5] & (1 << (termno & 0x1F)));
}

/* 计算打折金额 */
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

/*
 * 餐限检查
 */
static int _check_limit(acc_ram *pacc, int t_money)
{
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
}



/*
 * 餐限检查, 可跨账户消费
 */
static int _check_limit2(acc_ram *pacc, int t_money, u8 *feature, int *smoney, int *money)
{
	int sub_money = 0;
	//int _smoney = 0, _money = 0;
	memcpy(&sub_money, pacc->sub_money, sizeof(pacc->sub_money));
	*smoney = 0;
	memcpy(smoney, pacc->sub_money, sizeof(pacc->sub_money));
	*money = pacc->money;
	
	// 先判断钱到没到消费上限
	if (t_money <= pacc->smoney_life) {
		// 钱还没有到限制，判断次数有没有
		if (pacc->stimes_life) {
			// 补贴账户不受限制，判断余额
			if (t_money <= sub_money) {
				return 0;
			} else {
				// 补贴没有钱了
				int _comoney = t_money - sub_money;
#if 0
				if (pacc->st_limit & ACCST_PSNL_ALLOW) {
				} else if (pacc->st_limit & ACCST_PSNL_PWD) {
					*feature |= 1 << 3;
				} else if (pacc->st_limit & ACCST_NO_PSNL) {
					*feature = 1;
					return 0;
				}
#endif
				// 判断是否可以个人账户消费
				if (_comoney <= pacc->money_life && pacc->times_life) {
					return 0;
				} else {
					// 个人账户无消费余量
					if (pacc->st_limit & ACCST_PWDCON) {
						*feature |= 1 << 3;
					} else {
						pr_debug("_check_limit2: err 0\n");
						*feature = 1;
					}
					return 0;
				}
			}
		} else {
			// 次数到限制了
			goto next;
		}
#if 0
		// 补贴账户可正常消费, 还需要判断余额
		if (t_money <= sub_money) {
			// 够消费
			goto next;
			//return 0;
		} else {
			int _comoney = t_money - sub_money;
			if (pacc->st_limit & ACCST_PSNL_ALLOW) {
			} else if (pacc->st_limit & ACCST_PSNL_PWD) {
				*feature |= 1 << 3;
			} else if (pacc->st_limit & ACCST_NO_PSNL) {
				*feature = 1;
				return 0;
			}
			// 判断是否可以个人账户消费
			if (_comoney <= pacc->money_life && pacc->times_life) {
				return 0;
			} else {
				// 个人账户无消费余量
				if (pacc->st_limit & ACCST_PWDCON) {
					*feature |= 1 << 3;
				} else {
					pr_debug("_check_limit2: err 0\n");
					*feature = 1;
				}
				return 0;
			}
		}
#endif
	} else {
next:
		// 补贴消费已经到达上限
		if (t_money <= sub_money) {
			// 只需要消费补贴就好
			if (pacc->st_limit & ACCST_PWDCON_SUB) {
				*feature |= 1 << 3;
				return 0;
			} else {
				pr_debug("_check_limit2: err 0\n");
				*feature = 1;
				return 0;
			}
		} else {
			int _comoney;// = t_money - sub_money;
			// 需要消费个人才行, 判断是否允许
			if (pacc->st_limit & ACCST_PSNL_ALLOW) {
			} else if (pacc->st_limit & ACCST_PSNL_PWD) {
				*feature |= 1 << 3;
			} else if (pacc->st_limit & ACCST_NO_PSNL) {
				*money = 0;
				*feature = 1;
				return 0;
			}
			_comoney = t_money - sub_money;
#if 0
			if (pacc->st_limit & ACCST_NO_PSNL) {
				// 不允许消费个人
				pr_debug("_check_limit2: err 0\n");
				*feature = 1;
				return 0;
			}
			// 允许消费个人
			if (pacc->st_limit & ACCST_PSNL_PWD) {
				*feature |= 1 << 3;
			}
#endif
			// 判断个人消费是否够数
			{
				// 判断是否可以个人账户消费
				if (_comoney <= pacc->money_life && pacc->times_life) {
					return 0;
				} else {
					// 个人账户无消费余量
					if (pacc->st_limit & ACCST_PWDCON) {
						*feature |= 1 << 3;
					} else {
						pr_debug("_check_limit2: err 0\n");
						*feature = 1;
					}
					return 0;
				}
			}
		}
	}

#if 0
	int money_life;
	int times_life;
	
	/* 输密码判断 */
	if ((feature >> 6) == 3) {
		// 可跨账户消费
		money_life = pacc->smoney_life + pacc->money_life;
		times_life = pacc->stimes_life + pacc->times_life;
	} else if ((feature >> 6) == 2) {
		// 只使用个人
		money_life = pacc->money_life;
		times_life = pacc->times_life;
	} else if ((feature >> 6) == 1) {
		// 只使用补贴
		money_life = pacc->smoney_life;
		times_life = pacc->stimes_life;
	} else {
		// 不允许跨账户消费
		money_life = MAX(pacc->money_life, pacc->smoney_life);
		times_life = MAX(pacc->times_life, pacc->stimes_life);
	}
	
	if (money_life < t_money) {
		return -1;
	}
	if (times_life == 0) {
		return -1;
	}
#endif
	return 0;
}

/*
 * 餐限检查, 不可跨账户消费, 优先使用补贴
 */
static int _check_limit3(acc_ram *pacc, int t_money, u8 *feature)
{
	int sub_money = 0;
	memcpy(&sub_money, pacc->sub_money, sizeof(pacc->sub_money));
	
	if (t_money <= sub_money) {
		// 补贴账户够消费
		if (t_money <= pacc->smoney_life && pacc->stimes_life) {
			return 0;
		}
		if (pacc->st_limit & ACCST_PWDCON_SUB) {
			*feature |= 1 << 3;
			return 0;
		}
		if (pacc->st_limit & ACCST_NO_PSNL) {
			*feature = 1;
			return 0;
		}
		if (pacc->st_limit & ACCST_PSNL_ALLOW) {
			goto next;
		}
		if (pacc->st_limit & ACCST_PSNL_PWD) {
			*feature |= 1 << 3;
			goto next;
		}
	} else {
next:
		if (t_money <= pacc->money_life && pacc->times_life) {
			return 0;
		} else {
			if (pacc->st_limit & ACCST_PWDCON) {
				*feature |= 1 << 3;
				return 0;
			} else {
				*feature = 1;
				return 0;
			}
		}
	}
	
	return 0;
}

/*
 * 双账户餐限检查, 用于双账户消费只补贴可用
 */
static int _check_limit_sub(acc_ram *pacc, int t_money)
{
	
	if (pacc->smoney_life < t_money) {
		return -1;
	}
	if (pacc->stimes_life == 0) {
		return -1;
	}
	return 0;
}

/*
 * 折扣判断
 * money 输出参数 实际消费金额
 * ptrm 消费所发生的输入金额
 * pacc 账户指针
 */
int _cal_disc(int *money, term_ram *ptrm, acc_ram *pacc)
{
	int in_money = ptrm->_tmoney;
	discount *disc = &usrcfg.tmsg[current_id].disc[pacc->power_id & 0xF];
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
		current_id, pacc->power_id & 0xF, disc->start_times, disc->end_times1,
		disc->end_times2, pacc->dic_times);
	
	// 判断打折刷卡次数
	if (pacc->dic_times < disc->start_times) {
		pr_debug("dic_times %d < %d, no disc\n", pacc->dic_times, disc->start_times);
		return 0;
	}

	// 确定属于第几次打折
	times = pacc->dic_times - disc->start_times;	// 0 -> n
	if (times < disc->end_times1) {
		if ((disc->disc_val1 != 0xFFFF) && (disc->disc_val1 != 0xFFFE)) {
			// 打折有效
			*money = _cal_disc_money(in_money, disc->disc_val1);
			ptrm->_con_flag |= TRMF_DISC;
		} else if ((disc->const_val1 != 0xFFFF) && (disc->const_val1 != 0xFFFE)) {
			// 固定金额有效
			*money = disc->const_val1;
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
		if ((disc->disc_val2 != 0xFFFF) && (disc->disc_val2 != 0xFFFE)) {
			// 打折有效
			*money = _cal_disc_money(in_money, disc->disc_val2);
			ptrm->_con_flag |= TRMF_DISC;
		} else if ((disc->const_val2 != 0xFFFF) && (disc->const_val2 != 0xFFFE)) {
			// 固定金额有效
			*money = disc->const_val2;
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
 * 单一账户要余额命令
 */
int recv_leid_single(term_ram *ptrm, int allow, int itm)
{
	unsigned int cardno;
	acc_ram *pacc = NULL;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0, real_money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
	u32 passwd = 0xFFFFFFFF;

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

	// 判断是否允许脱机使用光电卡
	if (!allow) {
		pr_debug("not allow: %x\n", cardno);
		feature = 1;
		money = 0;
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
	// 根据消费限制进行账户判断
	if (pacc->flag & ACCF_LOSS) {
		// 这是挂失卡
		feature |= 1 << 5;
		pr_debug("%x ACCF_LOSS\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// 注销
		feature = 1;
		pr_debug("%x ACCF_WOFF\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// 冻结
		pr_debug("%x ACCF_FREEZE\n", cardno);
		feature = 1;
		goto acc_end;
	}
#if 0//modified by duyy, 2013.5.8
	// 此终端是否允许使用
	if (!is_term_allow(pacc->power_id & 0xF, ptrm->pterm->power_id & 0xF)) {
		feature = 1;
		pr_debug("%x is_term_allow failed\n", cardno);
		goto acc_end;
	}

	// 禁止消费的时段吗?
	if (!is_time_allow(itm, pacc->power_id & 0xF)) {
		feature = 1;
		pr_debug("%x is_time_allow failed\n", cardno);
		goto acc_end;
	}
#endif
	//printk("itm is %d\n", itm);//2013.5.13
	//身份、终端、时段匹配判断，write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//身份、终端对应的时段序号
	//printk("single: cardno:%ld timenum = %x\n", cardno, timenum);//2013.5.13
	if (timenum & 0x80){//modified by duyy, 2013.5.8,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
		feature = 1;
		pr_debug("whole day prohibit\n");
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
			pr_debug("timenum is %x\n", timenum);
		}
		//printk("continue consume\n");
	}
	// 出纳机不存在餐次餐限
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		/* 餐次餐限检查 */
		ptrm->_con_flag |= TRMF_LIMIT;	/* 水控终端时去掉, bug */
		pr_debug("recv_leid_single money=%d\n", pacc->money_life);//2012.3.28
		pr_debug("recv_leid_single time=%d\n", pacc->times_life);//2012.3.28
		if (_check_limit(pacc, ptrm->_tmoney)) {
			// 餐次到了
			if (pacc->st_limit & 1) {
				feature |= (1 << 3);
			} else {
				feature = 1;
				pr_debug("%x _check_limit failed\n", cardno);
				goto acc_end;
			}
		}
		
		/* 折扣判断 */
		ret = _cal_disc(&real_money, ptrm, pacc);//modified by duyy, 2012.10.29
		if (ret < 0){
			feature = 1;
			pr_debug("%x disc is not permmitted\n", cardno);
			goto acc_end;
		}
	}
	pr_debug("disc over\n");//added by duyy,2012.10.26
	money = pacc->money;
	if (money > 999999) {
		printk("money is too large!\n");
		money = 999999;
	}
	pr_debug("money over\n");//added by duyy,2012.10.26
	//pr_debug("password is %x %x %x\n", pacc->passwd[0], pacc->passwd[1], pacc->passwd[2]);//2012.10.26
	// passwd copy
	//memcpy(&passwd, pacc->passwd, sizeof(pacc->passwd));
	//modified by duyy, 2012.10.29
	passwd &= 0x00000000;
	passwd |= pacc->passwd[0];
	passwd |= pacc->passwd[1] << 8;
	passwd |= pacc->passwd[2] << 16;
	pr_debug("passwd is %x\n", passwd);
acc_end:
	pr_debug("%x: feature %x money %d, realmoney = %d\n",
		cardno, feature, money, real_money);
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

	// 3字节实际消费额real_money
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

	// 用户密码
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
	
	if (verify_all(ptrm, CHKALL) < 0)
		return -1;
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// 终端可以接收流水了, 保存状态
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		// 记录非法卡
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
		// 这时候写入终端金额吗?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
	}
#endif
	return 0;
}

/*
 * 双账户要余额命令
 */
int recv_leid_double(term_ram *ptrm, int allow, int itm)
{
	unsigned int cardno;
	acc_ram *pacc = NULL;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0, real_money = 0;
	int smoney = 0;
	int flag = 0, sflag = 0;		// 允许消费标志
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
	u32 passwd = 0xFFFFFFFF;

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
	
	// 判断是否允许脱机使用光电卡
	if (!allow) {
		pr_debug("not allow: %d\n", cardno);
		feature = 1;
		money = 0;
		goto acc_end;
	}
	
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("%x not find\n", cardno);
		feature = 1;
		goto acc_end;
	}
	feature = 4;
	// 根据消费限制进行账户判断
	if (pacc->flag & ACCF_LOSS) {
		// 这是挂失卡
		feature |= 1 << 5;
		pr_debug("%x ACCF_LOSS\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// 注销卡
		feature = 1;
		pr_debug("%x ACCF_WOFF\n", cardno);
		goto acc_end;
	}
	if (pacc->flag & ACCF_FREEZE) {
		// 冻结，write by duyy, 2013.5.8
		pr_debug("%x ACCF_FREEZE\n", cardno);
		feature = 1;
		goto acc_end;
	}
	if (pacc->flag & ACCF_FREEZE_SUB){
		// 补贴冻结，write by duyy, 2013.5.8
		feature = 1;
		goto acc_end;
	}
#if 0 //modified by duyy, 2013.5.8
	// 禁止消费的时段吗?
	// 此终端个人账户是否允许使用
	if (is_term_allow(pacc->power_id & 0xF, ptrm->pterm->power_id & 0xF)
		&& is_time_allow(itm, pacc->power_id & 0xF)
		&& !(pacc->flag & ACCF_FREEZE)) {
		// 个人账户允许使用
		flag = 1;
	}
	
	// 补贴账户是否允许使用
	if (is_term_allow(pacc->spower_id & 0xF, ptrm->pterm->power_id & 0xF)
		&& is_time_allow(itm, pacc->spower_id & 0xF)
		&& !(pacc->flag & ACCF_FREEZE_SUB)) {
		// 补贴账户允许使用
		sflag = 1;
	}
#endif
	//主帐户禁止消费时段判断，2013.5.8
	//身份、终端、时段匹配判断，write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//身份、终端对应的时段序号
	pr_debug("double: cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
		flag = 0;
	}
	else {
		// 个人账户允许使用
		flag = 1;
		for (i = 0; i < 7; i++){
			if (timenum & 0x1){
				if(itm >= term_time[i].begin && itm <= term_time[i].end){
					// 个人账户禁止使用
					flag = 0;
					break;
				}
			}
			timenum = timenum >> 1;
			//printk("timenum is %x\n", timenum);
		}
		//printk("continue consume\n");
	}
	//补贴帐户禁止消费时段判断，2013.5.8
	//身份、终端、时段匹配判断，write by duyy, 2013.5.8
	timenum = ptmnum[pacc->spower_id & 0xF][ptrm->pterm->power_id & 0xF];//身份、终端对应的时段序号
	pr_debug("sub: cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
		sflag = 0;
	}
	else {
		// 补贴账户允许使用
		sflag = 1;
		for (i = 0; i < 7; i++){
			if (timenum & 0x1){
				if(itm >= term_time[i].begin && itm <= term_time[i].end){
					// 补贴账户禁止使用
					sflag = 0;
					break;
				}
			}
			timenum = timenum >> 1;
			//printk("timenum is %x\n", timenum);
		}
		//printk("continue consume\n");
	}
	if (!flag && !sflag) {
		// 都不允许消费
		feature = 1;
		pr_debug("%x: flag = %d, sflag = %d, pacc->flag = %x\n",
			cardno, flag, sflag, pacc->flag);
		goto acc_end;
	}
	
	money = pacc->money;
	if (money > 999999) {
		printk("money is too large!\n");
		money = 999999;
	}

	memcpy(&smoney, pacc->sub_money, sizeof(pacc->sub_money));
	
	#if 0//added by duyy, 2012.3.29
		printk("-----recv_leid_double----\n");
		printk("flag=%x\n", pacc->flag);
		printk("st_limit=%x\n", pacc->st_limit);
		printk("money=%d\n", pacc->money);
		printk("money_life=%u\n", pacc->money_life);
		printk("times_life=%x\n", pacc->times_life);
		printk("sub_money[0]=%x\n", pacc->sub_money[0]);
		printk("sub_money[1]=%x\n", pacc->sub_money[1]);
		printk("sub_money[2]=%x\n", pacc->sub_money[2]);
		printk("smoney=%x\n", smoney);
		printk("smoney_life=%u\n", pacc->smoney_life);
		printk("stimes_life=%x\n", pacc->stimes_life);
		printk("flag=%d\n", flag);
		printk("sflag=%d\n", sflag);
	#endif
	/* 双账户消费状态 标志置位 */
	// 判断是否可以双账户消费
	if (!(pacc->st_limit & ACCST_ALLOW_DOUBLE)) {
		pr_debug("%x: ACCST_ALLOW_DOUBLE not set\n", cardno);
		// 不允许双账户使用
		if (pacc->st_limit & ACCST_PERSON) {
			pr_debug("%x: ACCST_PERSON set\n", cardno);
			// 只允许个人账户消费
			if (flag) {
				feature |= 0x80;
				
				// 判断个人账户餐限，并标志输密码位
				if (_check_limit(pacc, ptrm->_tmoney) < 0) {
					// 到达限制，判断是否允许输密码后消费
					if (pacc->st_limit & ACCST_PWDCON) {
						feature |= 1 << 3;	// set passwd bit
					} else {
						feature = 1;
						pr_debug("%x: double set and limit, and flag = %d\n", cardno, flag);
						goto acc_end;
					}
				}
			} else {
				feature = 1;
				pr_debug("%x: ACCST_PERSON set, and flag = %d\n", cardno, flag);
				goto acc_end;
			}
		} else {
			// 只允许补贴账户消费
			pr_debug("%x: ACCST_PERSON not set\n", cardno);
			if (sflag) {
				feature |= 0x40;
			} else {
				pr_debug("%x: ACCST_PERSON not set, and sflag = %d\n", cardno, sflag);
				feature = 1;
				goto acc_end;
			}
			// 仅允许补贴消费
			if (_check_limit_sub(pacc, ptrm->_tmoney) < 0) {
				if (pacc->st_limit & ACCST_PWDCON_SUB) {
					feature |= 1 << 3;
				} else {
					feature = 1;
					pr_debug("%x: double set and sub limit, and sflag = %d\n", cardno, sflag);
					goto acc_end;
				}
			}
		}
	} else {
		//printk("ACCST_ALLOW_DOUBLE set\n");
		// 允许双账户使用
		if (flag && sflag) {
			if (pacc->st_limit & ACCST_ALLOW_UNIT) {
				feature |= 0xc0;
				// 允许跨账户使用
				_check_limit2(pacc, ptrm->_tmoney, &feature, &smoney, &money);
				//printk("recv_leid_double _check_limit2\n");//2012.3.29
				if (feature == 1) {
					pr_debug("%x: _check_limit2  err feature = 1\n", cardno);
					goto acc_end;
				}
#if 0				
				if (_check_limit2(pacc, ptrm->_tmoney, feature)) {
					feature = 1;
					pr_debug("%x: _check_limit2 failed\n", cardno);
					goto acc_end;
				}
#endif
			} else {
				// 不允许跨账户使用, 优先使用补贴feature bit7~6 = 0
				_check_limit3(pacc, ptrm->_tmoney, &feature);
				//printk("recv_leid_double _check_limit3\n");//2012.3.29
				if (feature == 1) {
					pr_debug("%x: _check_limit3 err feature = 1\n", cardno);
					goto acc_end;
				}
			}
		} else {
			if (flag) {
				feature |= 0x80;//个人帐户允许消费位，write by duyy, 2013.6.4
				
				// 判断个人账户餐限，并标志输密码位
				if (_check_limit(pacc, ptrm->_tmoney) < 0) {
					// 到达限制，判断是否允许输密码后消费
					if (pacc->st_limit & ACCST_PWDCON) {
						feature |= 1 << 3;	// set passwd bit
					} else {
						feature = 1;
						pr_debug("%x: double set and limit, and flag = %d\n", cardno, flag);
						goto acc_end;
					}
				}
				
			} else if (sflag) {
				feature |= 0x40;//补贴帐户允许消费位，write by duyy, 2013.6.4

				// 仅允许补贴消费
				if (_check_limit_sub(pacc, ptrm->_tmoney) < 0) {
					if (pacc->st_limit & ACCST_PWDCON_SUB) {
						feature |= 1 << 3;
					} else {
						feature = 1;
						pr_debug("%x: double set and sub limit, and sflag = %d\n", cardno, sflag);
						goto acc_end;
					}
				}
			}
		}
	}
 	pr_debug("recv_leid_double feature=%x\n", feature);//2012.5.9
	// 餐次检查
	ptrm->_con_flag |= TRMF_LIMIT;	/* 水控终端时去掉, bug */

	/* 折扣判断 */
	ret = _cal_disc(&real_money, ptrm, pacc);
	if (ret < 0){
		feature = 1;
		pr_debug("%x disc is not permmitted\n", cardno);
		goto acc_end;
	}

	// passwd copy
	//memcpy(&passwd, pacc->passwd, sizeof(pacc->passwd));
	//modified by duyy, 2012.10.29
	passwd &= 0x00000000;
	passwd |= pacc->passwd[0];
	passwd |= pacc->passwd[1] << 8;
	passwd |= pacc->passwd[2] << 16;
acc_end:
	
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

	// 3字节实际消费额real_money
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
	
	// 用户密码
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
	
	// 补贴余额
	smoney = binl2bcd(smoney) & 0xFFFFFF;
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
	
	if (verify_all(ptrm, CHKALL) < 0)
		return -1;
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
	return 0;
}

/*
 * 出纳终端要余额命令
 */
int recv_leid_cash(term_ram *ptrm, int allow, int itm)
{
	unsigned int cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0;//, real_money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int i, ret;
	u32 passwd = 0xFFFFFFFF;
	u32 accno = 0;
	
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
	// 判断是否允许脱机使用光电卡
	if (!allow) {
		pr_debug("not allow: %d\n", cardno);
		feature = 1;
		money = 0;
		pacc = NULL;
		goto acc_end;
	}
	
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("%x not find\n", cardno);
		feature = 1;
		goto acc_end;
	}
	feature = 4;
	accno = pacc->acc_num;
	
	// 根据消费限制进行账户判断
	if (pacc->flag & ACCF_LOSS) {
		// 这是挂失卡
		feature |= 1 << 5;
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// 注销
		feature = 1;
		goto acc_end;
	}

	if ((pacc->flag & ACCF_FREEZE)
		|| (pacc->power_id & ACCP_NOCASH)) {
		// 冻结
		feature = 1;
		goto acc_end;
	}
#if 0//modified by duyy, 2013.5.8
	// 此终端是否允许使用
	if (!is_term_allow(pacc->power_id & 0xF, ptrm->pterm->power_id & 0xF)) {
		feature = 1;
		goto acc_end;
	}
	
	// 禁止消费的时段吗?
	if (!is_time_allow(itm, pacc->power_id & 0xF)) {
		feature = 1;
		goto acc_end;
	}
#endif
	//身份、终端、时段匹配判断，write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//身份、终端对应的时段序号
	pr_debug("cash: cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
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
	if (money > 999999) {
		printk("money is too large!\n");
		money = 999999;
	}

	// passwd copy
	//memcpy(&passwd, pacc->passwd, sizeof(pacc->passwd));
	//modified by duyy, 2012.10.29
	passwd &= 0x00000000;
	passwd |= pacc->passwd[0];
	passwd |= pacc->passwd[1] << 8;
	passwd |= pacc->passwd[2] << 16;
acc_end:
	
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

	// 4字节账号
	tmp = (unsigned char *)&accno;
	tmp += 3;
	for (i = 0; i < 4; i++, tmp--) {
		if (send_byte(*tmp, ptrm->term_no) < 0) {
			ptrm->status = NOCOM;
			return -1;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}

#if 0
	// 用户密码
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
#endif

	if (verify_all(ptrm, CHKALL) < 0)
		return -1;
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// 终端可以接收流水了, 保存状态
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		// 记录非法卡
		cashbuf[cashterm_ptr].feature = feature;
		cashbuf[cashterm_ptr].consume = 0;
		cashbuf[cashterm_ptr].status = CASH_CARDIN;
		cashbuf[cashterm_ptr].termno = ptrm->term_no;
		cashbuf[cashterm_ptr].cardno = cardno;
		if (pacc) {
			cashbuf[cashterm_ptr].accno = pacc->acc_num;
			cashbuf[cashterm_ptr].cardno = pacc->card_num;
#ifdef CONFIG_UART_V2
			cashbuf[cashterm_ptr].managefee = fee[(pacc->flag >> 4) & 0xF];
#else
			cashbuf[cashterm_ptr].managefee = fee[pacc->managefee & 0xF];
#endif
			cashbuf[cashterm_ptr].money = pacc->money;
		}
		// 这时候写入终端金额吗?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
	}
#endif
	return 0;
}

/*
 * 收取单账户消费流水 双校验
 */
int recv_leflow_single(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
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
	
	// second recv 3-byte consume money
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
	
	// adjust money limit
	sub_money(pacc, leflow.consume_sum);
	#if	0 //added by duyy,2012.3.28
		printk("-----recv_leflow_single account-----\n");
	//	printk("money=%d\n", pacc->money_life);
	//	printk("times=%x\n", pacc->times_life);
	#endif
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		pr_debug("account money below zero!!!\n");
		pacc->money = 0;
	}

	if (ptrm->_con_flag & TRMF_DISCF) {
		pacc->dic_times++;
	}

	ptrm->term_cnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
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
	leflow.ltype = ptrm->_con_flag;
	#if	0 //added by duyy,2012.3.28
		//printk("-----recv_leflow_single account-----\n");
		printk("ltype=%x\n", leflow.ltype);
		//printk("_con_flag=%x\n", ptrm->_con_flag);
	#endif
	// 流水号的处理
	leflow.flow_num = maxflowno++;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// 流水区头尾的处理
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	pr_debug("-----recv_leflow_single over-----\n");//2012.9.13
	flowptr.tail = tail;
	flow_sum++;
	return 0;
}

int recv_leflow_double(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int smoney = 0;
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
	
	// second recv 3-byte consume money
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
	
	// second recv 3-byte 补贴金额
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
	// adjust money limit
	if (smoney == 0) {
		sub_money(pacc, leflow.consume_sum);
	}
	if (smoney) {
		sub_smoney(pacc, smoney);
	}
	
	if (ptrm->_con_flag & TRMF_DISCF) {
		pacc->dic_times++;
	}

	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		pr_debug("account money below zero!!!\n");
		pacc->money = 0;
	}

	_sub_byte3(pacc->sub_money, smoney);
	pr_debug("double flow %x: cm = %ld, sm = %d, m %d, %02x%02x%02x\n", pacc->card_num,
		leflow.consume_sum, smoney, pacc->money, pacc->sub_money[2], pacc->sub_money[1], pacc->sub_money[0]);
#if 0
	pacc->sub_money -= smoney;
	if (pacc->sub_money < 0) {
		pr_debug("account sub money below zero!!!\n");
		pacc->sub_money = 0;
	}
#endif

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
	
	// 双账户都有消费
	if (leflow.consume_sum && smoney) {
		// init leflow
		leflow.ltype |= FLOWF_SPLIT;
		
		// 流水号的处理
		leflow.flow_num = maxflowno++;
		
		// set ptrm->flow_flag
		// 流水区头尾的处理
		tail = flowptr.tail;
		memcpy(pflow + tail, &leflow, sizeof(flow));
		tail++;
		if (tail == FLOWANUM)
			tail = 0;
		
		flowptr.tail = tail;
		flow_sum++;

		// 拆分流水
		leflow.ltype &= ~(FLOWF_SPLIT | TRMF_LIMIT);
		leflow.consume_sum = smoney;
		leflow.is_sub = 1;
		// 流水号的处理
		leflow.flow_num = maxflowno++;
		// 流水区头尾的处理
		tail = flowptr.tail;
		memcpy(pflow + tail, &leflow, sizeof(flow));
		tail++;
		if (tail == FLOWANUM)
			tail = 0;
		flowptr.tail = tail;
		flow_sum++;
	} else if (leflow.consume_sum) {
		// 个人消费

		// 流水号的处理
		leflow.flow_num = maxflowno++;
		
		// 流水区头尾的处理
		tail = flowptr.tail;
		memcpy(pflow + tail, &leflow, sizeof(flow));
		tail++;
		if (tail == FLOWANUM)
			tail = 0;
		
		flowptr.tail = tail;
		flow_sum++;
		
	} else if (smoney) {
		// 补贴消费

		// 流水号的处理
		leflow.flow_num = maxflowno++;

		leflow.consume_sum = smoney;
		leflow.is_sub = 1;
		
		// 流水区头尾的处理
		tail = flowptr.tail;
		memcpy(pflow + tail, &leflow, sizeof(flow));
		tail++;
		if (tail == FLOWANUM)
			tail = 0;
		
		flowptr.tail = tail;
		flow_sum++;
	}
	
	ptrm->flow_flag = ptrm->term_no;
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
 * 增加时段的判断, flag = 1说明在此时段中有动作
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
	pr_debug("-----recv_le id-----\n");//added by duyy,2013.5.9
	// 判断是否允许脱机使用光电卡
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
	// 根据消费限制进行账户判断
	if (pacc->flag & ACCF_LOSS) {
		// 这是挂失卡
		feature |= 1 << 5;
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// 注销
		feature = 1;
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// 冻结
		feature = 1;
		goto acc_end;
	}
#if 0//modified by duyy,2013.5.8
	// 此终端是否允许使用
	if (!is_term_allow(pacc->power_id & 0xF, ptrm->pterm->power_id & 0xF)) {
		feature = 1;
		goto acc_end;
	}

	// 禁止消费的时段吗?
	if (!is_time_allow(itm, pacc->power_id & 0xF)) {
		feature = 1;
		goto acc_end;
	}
#endif
	//身份、终端、时段匹配判断，write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//身份、终端对应的时段序号
	pr_debug("cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
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
	
	// 出纳机不存在餐次餐限
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		/* 餐次餐限检查 */
		//printk("recv_leid_id money=%d\n", pacc->money_life);//2013.3.29
		//printk("recv_leid_id time=%d\n", pacc->times_life);//2013.3.29
		//printk("recv_leid_id _tmoney=%d\n", ptrm->_tmoney);//2013.3.29
		ptrm->_con_flag |= TRMF_LIMIT;	/* 水控终端时去掉, bug */
		if (_check_limit(pacc, ptrm->_tmoney)) {
			// 餐次到了
			if (pacc->st_limit & 1) {
				feature |= (1 << 3);
			} else {
				feature = 1;
				goto acc_end;
			}
		}
	}
	
	money = pacc->money;
	if (money > 999999) {
		printk("money is too large!\n");
		money = 999999;
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
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// 终端可以接收流水了, 保存状态
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		// 记录非法卡
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
		// 这时候写入终端金额吗?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
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

	// 流水号的处理
	leflow.flow_num = maxflowno++;

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

// 增加双校验标志
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
	for (i = 0; i < 2; i++) {		//接收2bytes取款额（bcd码）
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
	leflow.flow_type = LETAKE;
	leflow.acc_num = pacc->acc_num;
	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
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
#ifdef CONFIG_RECORD_CASHTERM
	// 终端接收取款流水, 保存状态
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
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
	}
#endif
	return 0;
}

// 收取存款流水
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
	for (i = 0; i < 2; i++) {		//接收2bytes取款额（bcd码）
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
	// 恢复餐限
#if 0
	if ((pacc->feature & 0xF) != 0xF) {
		pacc->feature &= 0xF0;
		pacc->feature |= 0xE;
	}
#endif

	managefee = fee[(pacc->flag >> 4) & 0xF];		//管理费比率
	managefee = managefee * leflow.consume_sum / 100;	//计算管理费
	pacc->money += leflow.consume_sum - managefee;		//余额加钱减管费

	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// 禁止再次接收流水
	// 整理并存储流水
	leflow.flow_type = LECHARGE;
	leflow.acc_num = pacc->acc_num;
	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
		*tmp = *tm;
		tmp--;
		tm++;
	}
	// 流水号的处理
	leflow.flow_num = maxflowno++;
	leflow.tml_num = ptrm->term_no;
	leflow.manage_fee = 0 - managefee;
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// 修改流水指针
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
#ifdef CONFIG_RECORD_CASHTERM
	// 终端接收取款流水, 保存状态
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
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
	}
#endif
	return 0;
}

/*
 * 终端机输入金额接收
 */
int recv_leid_zlyy(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned int cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0, real_money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int imy = 0;		// 终端机输入金额
	//unsigned long acc = 0;
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
	tmp = (unsigned char *)&imy;
	tmp += 2;
	// 接收输入金额
	for (i = 0; i < 3; i++) {
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
	ptrm->_tmoney = imy;
	//_type = ptrm->pterm->power_id & 0x3;

	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
		goto acc_end;
	}
	
	feature = 4;
	// 根据消费限制进行账户判断
	if (pacc->flag & ACCF_LOSS) {
		// 这是挂失卡
		feature |= 1 << 5;
		pr_debug("%x ACCF_LOSS\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// 注销
		feature = 1;
		pr_debug("%x ACCF_WOFF\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// 冻结
		pr_debug("%x ACCF_FREEZE\n", cardno);
		feature = 1;
		goto acc_end;
	}
#if  0//modified by duyy, 2013.5.8
	// 此终端是否允许使用
	if (!is_term_allow(pacc->power_id & 0xF, ptrm->pterm->power_id & 0xF)) {
		feature = 1;
		pr_debug("%x is_term_allow failed\n", cardno);
		goto acc_end;
	}

	// 禁止消费的时段吗?
	if (!is_time_allow(itm, pacc->power_id & 0xF)) {
		feature = 1;
		pr_debug("%x is_time_allow failed\n", cardno);
		goto acc_end;
	}
#endif
	//身份、终端、时段匹配判断，write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//身份、终端对应的时段序号
	pr_debug("zlyy: cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
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
	// 出纳机不存在餐次餐限
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		/* 餐次餐限检查 */
		ptrm->_con_flag |= TRMF_LIMIT;	/* 水控终端时去掉, bug */
		if (_check_limit(pacc, ptrm->_tmoney)) {
			// 餐次到了
			if (pacc->st_limit & 1) {
				feature |= (1 << 3);
			} else {
				feature = 1;
				pr_debug("%x _check_limit failed\n", cardno);
				goto acc_end;
			}
		}
		
		/* 折扣判断 */
		ret = _cal_disc(&real_money, ptrm, pacc);
		if (ret < 0){
			feature = 1;
			pr_debug("%x disc is not permmitted\n", cardno);
			goto acc_end;
		}
	}

	money = pacc->money;
	if (money > 999999) {
		printk("money is too large!\n");
		money = 999999;
	}

#if 0
		feature = 4;
		if ((pacc->feature & 0xC0) == 0x80) {
			// 这是挂失卡
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// 此卡达到餐限
			// 此卡要输密码
			feature |= 1 << 3;
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// 断断是否为出纳机
			// 出纳机
			feature = 1;	// 不支持出纳
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ( (ptrm->pterm->power_id == 0xF0) || ((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))) {
			if (pacc->money < 0) {
				money = 0;
			} else {
				money = (int)pacc->money;// & 0xFFFFF;
			}
		} else {
			feature = 1;
		}
		if (money > 999999) {
#ifdef DEBUG
			printk("money is too large, max 999999!\n");
#endif
			money = 999999;
		}
#ifdef USRCFG1
		// 判断此账户是否可以消费
		pr_debug("pacc %d find... %p\n", cardno, pacc);
		ret = is_cfg(&usrcfg, pacc, NULL, itm, ptrm->term_no);
		if (ret == -1) {
#ifdef USRDBG
			printk("%d %d %08x不允许消费\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == -2) {
			feature |= 1 << 4;
		} else if (ret == 255) {
			// 此卡达到餐限
			// 此卡要输密码
			feature |= 1 << 3;
		}
		pr_debug("pacc %d iscfg over\n", cardno);
		if (!tmoney) {
			feature = 1;	// 超过限制
		}
#endif
		//acc = pacc->acc_num;
///////////////////////////////////////////////////////////////////////////////////
#endif
acc_end:
	// 判断是否允许脱机使用光电卡
	if (!allow) {
		feature = 1;
		money = 0;
	}
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
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}

	// 3字节实际消费金额HEX
	//imy = cal_money(imy, pacc, itm, 0);
	tmp = (unsigned char *)&real_money;
	tmp += 2;
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}

	// 3字节余额上限HEX
	if (pacc) {
		imy = pacc->warning;
	}
	tmp = (unsigned char *)&imy;
	tmp += 2;
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	
	if (xor) {
		if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
			printk("%d recv le id verify chkxor error %02x\n", ptrm->term_no, feature);
#endif
			return -1;
		}
	} else {
		if (verify_all(ptrm, CHKXOR) < 0) {
#ifdef DEBUG
			printk("%d recv le id verify chkxor error %02x\n", ptrm->term_no, feature);
#endif
			return -1;
		}
	}
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// 终端可以接收流水了, 保存状态
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		// 记录非法卡
		cashbuf[cashterm_ptr].feature = feature;
		cashbuf[cashterm_ptr].consume = 0;
		cashbuf[cashterm_ptr].status = CASH_CARDIN;
		cashbuf[cashterm_ptr].termno = ptrm->term_no;
		cashbuf[cashterm_ptr].cardno = cardno;
		if (pacc) {
			cashbuf[cashterm_ptr].accno = pacc->acc_num;
			cashbuf[cashterm_ptr].cardno = pacc->card_num;
#ifdef CONFIG_UART_V2
			cashbuf[cashterm_ptr].managefee = fee[(pacc->flag >> 4) & 0xF];
#else
			cashbuf[cashterm_ptr].managefee = fee[pacc->managefee & 0xF];
#endif
			cashbuf[cashterm_ptr].money = pacc->money;
		}
		// 这时候写入终端金额吗?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
	}
#endif
	return 0;
}

/*
 * receive le card flow, flag is 0 or 1; if flag is 0, consum is HEX; or is BCD
 * but store into flow any data is HEX
 * flag: bit0 is 0, consum is HEX; 1, BCD
 */
int recv_leflow_zlyy(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i;
	int tail;
	unsigned char feature;
	unsigned char *tmp;
	acc_ram *pacc;
	memset(&leflow, 0, sizeof(leflow));
	// first recv 4-byte card number
	tmp = (unsigned char *)&leflow.card_num;
	tmp += 3;//sizeof(leflow.card_num) - 1;		// tmp point lenum high byte
	for (i = 0; i < 4; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
#ifdef DEBUG
			printk("%d recv leflow cardno %d ret %d\n", ptrm->term_no, i, ret);
#endif
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

	// second recv 3-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 2;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 3; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
#ifdef DEBUG
			printk("%d recv leflow consume_sum %d ret %d\n", ptrm->term_no, i, ret);
#endif
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

	// third check ALL
	if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
		printk("%d recv le flow verify chkxor error\n", ptrm->term_no);
#endif
		return -1;
	}
	
	if (ptrm->flow_flag == ~ptrm->term_no) {
		goto next;
	}
	// check if receive the flow
	if (ptrm->flow_flag)
		return 0;
next:
	space_remain--;
	//leflow.card_num = _card(leflow.card_num);
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		printk("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	// adjust money limit
	sub_money(pacc, leflow.consume_sum);
	#if	0 //added by duyy,2012.3.28
		printk("-----recv_leflow_zlyy account-----\n");
		printk("money=%x\n", pacc->money_life);
		printk("times=%x\n", pacc->times_life);
	#endif
	// 现在消费额应该是固定的, 改leflow->consume_sum
	/* 折扣判断 这里可能存在漏洞 */
	_cal_disc(&leflow.consume_sum, ptrm, pacc);
	//recvflow(&usrcfg, pacc, &leflow, pacc->itm, ptrm->term_no);

	pacc->money -= leflow.consume_sum;

	if (ptrm->_con_flag & TRMF_DISCF) {
		pacc->dic_times++;
	}

	//pacc->feature &= 0xF0;
	//pacc->feature |= limit;
	ptrm->term_cnt++;

	ptrm->term_money += leflow.consume_sum;
	// init leflow
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
	leflow.ltype = ptrm->_con_flag;
	#if	0 //added by duyy,2012.3.28
		printk("-----recv_leflow_zlyy account-----\n");
		printk("ltype=%x\n", leflow.ltype);
		printk("_con_flag=%x\n", ptrm->_con_flag);
	#endif
	// 流水号的处理
	leflow.flow_num = maxflowno++;
	// set ptrm->flow_flag
	ptrm->flow_flag = ptrm->term_no;
	// 流水区头尾的处理
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	
	flowptr.tail = tail;
	flow_sum++;

	return 0;
}

int send_time_zlyy(term_ram *ptrm, unsigned char *tm, int itm)
{
	unsigned char seg = 0;
	int i;

	if (is_current_tmsg(itm)) {
		seg = current_id + 1;
	}
	
	if (send_data(tm, 6, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 6; i++) {
		ptrm->add_verify += tm[i];
		ptrm->dif_verify ^= tm[i];
	}
	
	send_byte(seg, ptrm->term_no);
	ptrm->add_verify += seg;
	ptrm->dif_verify ^= seg;
	
	if (verify_all(ptrm, CHKALL) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	return 0;
}

/*
 * 终端机输入金额接收, 要求server计算余额
 */
int recv_leid_zlyy2(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned int cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0, real_money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int imy = 0;		// 终端机输入金额
	//unsigned long acc = 0;
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
	tmp = (unsigned char *)&imy;
	tmp += 2;
	// 接收输入金额
	for (i = 0; i < 3; i++) {
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
	ptrm->_tmoney = imy;

	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("pacc %d not find... %p\n", cardno, pacc);
		feature = 1;
		goto acc_end;
	}

	feature = 4;
	// 根据消费限制进行账户判断
	if (pacc->flag & ACCF_LOSS) {
		// 这是挂失卡
		feature |= 1 << 5;
		pr_debug("%x ACCF_LOSS\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// 注销
		feature = 1;
		pr_debug("%x ACCF_WOFF\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// 冻结
		pr_debug("%x ACCF_FREEZE\n", cardno);
		feature = 1;
		goto acc_end;
	}
#if 0 //modified by duyy, 2013.5.8
	// 此终端是否允许使用
	if (!is_term_allow(pacc->power_id & 0xF, ptrm->pterm->power_id & 0xF)) {
		feature = 1;
		pr_debug("%x is_term_allow failed\n", cardno);
		goto acc_end;
	}

	// 禁止消费的时段吗?
	if (!is_time_allow(itm, pacc->power_id & 0xF)) {
		feature = 1;
		pr_debug("%x is_time_allow failed\n", cardno);
		goto acc_end;
	}
#endif
	//身份、终端、时段匹配判断，write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//身份、终端对应的时段序号
	pr_debug("zlyy2: cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
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
	// 出纳机不存在餐次餐限
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		/* 餐次餐限检查 */
		ptrm->_con_flag |= TRMF_LIMIT;	/* 水控终端时去掉, bug */
		if (_check_limit(pacc, ptrm->_tmoney)) {
			// 餐次到了
			if (pacc->st_limit & 1) {
				feature |= (1 << 3);
			} else {
				feature = 1;
				pr_debug("%x _check_limit failed\n", cardno);
				goto acc_end;
			}
		}
		
		/* 折扣判断 */
		ret = _cal_disc(&real_money, ptrm, pacc);
		if (ret < 0){
			feature = 1;
			pr_debug("%x disc is not permmitted\n", cardno);
			goto acc_end;
		}
	}
	
	money = pacc->money;
	if (money > 999999) {
		printk("money is too large!\n");
		money = 999999;
	}

acc_end:
	// 判断是否允许脱机使用光电卡
	if (!allow) {
		feature = 1;
		money = 0;
	}

	// imy 实际消费金额
	if (pacc) {
		int warning = pacc->warning * 100;
		int draft = pacc->draft * 100;

		imy = real_money;
		money -= imy;
		pr_debug("money = %d draft = %d alarm = %d\n",
			money, draft, warning);
		
		if (money < (-draft)) {
			pr_debug("warning: money < - mdraw, %d, %d\n",
				money, draft);
			feature |= (1 << 7);
			money += imy;
		}
		
		if (money < warning) {
			feature |= (1 << 3);
			pr_debug("warning: money < m_alarm, %d, %d\n",
				money, warning);
		}
		
		if (money < 0) {
			feature &= ~(1 << 6);
			money = -money;
		} else {
			feature |= (1 << 6);
		}
	}
	pr_debug("feature is %x\n", feature);//2013.5.9
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;

	// 此处发送消费后余额
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

	// 3字节实际消费金额HEX
	tmp = (unsigned char *)&imy;
	tmp += 2;
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	
	if (xor) {
		if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
			printk("%d recv le id verify chkxor error %02x\n", ptrm->term_no, feature);
#endif
			return -1;
		}
	} else {
		if (verify_all(ptrm, CHKXOR) < 0) {
#ifdef DEBUG
			printk("%d recv le id verify chkxor error %02x\n", ptrm->term_no, feature);
#endif
			return -1;
		}
	}
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// 终端可以接收流水了, 保存状态
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		// 记录非法卡
		cashbuf[cashterm_ptr].feature = feature;
		cashbuf[cashterm_ptr].consume = 0;
		cashbuf[cashterm_ptr].status = CASH_CARDIN;
		cashbuf[cashterm_ptr].termno = ptrm->term_no;
		cashbuf[cashterm_ptr].cardno = cardno;
		if (pacc) {
			cashbuf[cashterm_ptr].accno = pacc->acc_num;
			cashbuf[cashterm_ptr].cardno = pacc->card_num;
#ifdef CONFIG_UART_V2
			cashbuf[cashterm_ptr].managefee = fee[(pacc->flag >> 4) & 0xF];
#else
			cashbuf[cashterm_ptr].managefee = fee[pacc->managefee & 0xF];
#endif
			cashbuf[cashterm_ptr].money = pacc->money;
		}
		// 这时候写入终端金额吗?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
	}
#endif
	return 0;
}

/*
 * 终端机输入金额接收, 出纳机
 */
int recv_leid_zlyy_cash(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned int cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	unsigned char timenum = 0;//write by duyy, 2013.5.8
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	//unsigned long acc = 0;
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
		goto acc_end;
	}

	feature = 4;
	// 根据消费限制进行账户判断
	if (pacc->flag & ACCF_LOSS) {
		// 这是挂失卡
		feature |= 1 << 5;
		pr_debug("%x ACCF_LOSS\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_WOFF) {
		// 注销
		feature = 1;
		pr_debug("%x ACCF_WOFF\n", cardno);
		goto acc_end;
	}

	if (pacc->flag & ACCF_FREEZE) {
		// 冻结
		pr_debug("%x ACCF_FREEZE\n", cardno);
		feature = 1;
		goto acc_end;
	}
#if 0 //modified by duyy, 2013.5.8
	// 此终端是否允许使用
	if (!is_term_allow(pacc->power_id & 0xF, ptrm->pterm->power_id & 0xF)) {
		feature = 1;
		pr_debug("%x is_term_allow failed\n", cardno);
		goto acc_end;
	}

	// 禁止消费的时段吗?
	if (!is_time_allow(itm, pacc->power_id & 0xF)) {
		feature = 1;
		pr_debug("%x is_time_allow failed\n", cardno);
		goto acc_end;
	}
#endif
	//身份、终端、时段匹配判断，write by duyy, 2013.5.8
	timenum = ptmnum[pacc->power_id & 0xF][ptrm->pterm->power_id & 0xF];//身份、终端对应的时段序号
	pr_debug("zlyy cash: cardno:%ld timenum = %x\n", cardno, timenum);
	if (timenum & 0x80){//modified by duyy, 2013.5.8,最高位bit7表示禁止消费，其余为允许消费，判断禁止时段
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
	// 出纳机不存在餐次餐限
	if (!(ptrm->pterm->param.term_type & 0x20)) {
#if 0
		/* 餐次餐限检查 */
		ptrm->_con_flag |= TRMF_LIMIT;	/* 水控终端时去掉, bug */
		if (_check_limit(pacc, ptrm->_tmoney)) {
			// 餐次到了
			if (pacc->st_limit & 1) {
				feature |= (1 << 3);
			} else {
				feature = 1;
				pr_debug("%x _check_limit failed\n", cardno);
				goto acc_end;
			}
		}
		
		/* 折扣判断 */
		_cal_disc(&real_money, ptrm, pacc);
#endif
		feature = 1;
		goto acc_end;
	}
	
	money = pacc->money;
	if (money > 999999) {
		printk("money is too large!\n");
		money = 999999;
	}

acc_end:
	// 判断是否允许脱机使用光电卡
	if (!allow) {
		feature = 1;
		money = 0;
	}
	pr_debug("feature is %x\n", feature);//2013.5.9
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;
	
	// 此处发送消费后余额
	tmp = (unsigned char *)&money;
	tmp += 3;
	for (i = 0; i < 4; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	
	if (xor) {
		if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
			printk("%d recv le id verify chkxor error %02x\n", ptrm->term_no, feature);
#endif
			return -1;
		}
	} else {
		if (verify_all(ptrm, CHKXOR) < 0) {
#ifdef DEBUG
			printk("%d recv le id verify chkxor error %02x\n", ptrm->term_no, feature);
#endif
			return -1;
		}
	}
	ptrm->flow_flag = 0;		// 允许终端接收流水
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// 终端可以接收流水了, 保存状态
	if ((ptrm->pterm->param.term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
		// 记录非法卡
		cashbuf[cashterm_ptr].feature = feature;
		cashbuf[cashterm_ptr].consume = 0;
		cashbuf[cashterm_ptr].status = CASH_CARDIN;
		cashbuf[cashterm_ptr].termno = ptrm->term_no;
		cashbuf[cashterm_ptr].cardno = cardno;
		if (pacc) {
			cashbuf[cashterm_ptr].accno = pacc->acc_num;
			cashbuf[cashterm_ptr].cardno = pacc->card_num;
#ifdef CONFIG_UART_V2
			cashbuf[cashterm_ptr].managefee = fee[(pacc->flag >> 4) & 0xF];
#else
			cashbuf[cashterm_ptr].managefee = fee[pacc->managefee & 0xF];
#endif
			cashbuf[cashterm_ptr].money = pacc->money;
		}
		// 这时候写入终端金额吗?
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
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

// 增加双校验标志
int recv_take_money(term_ram *ptrm, unsigned char *tm)
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
	for (i = 0; i < 4; i++, tmp--) {
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
	}
	
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 1;
	for (i = 0; i < 2; i++, tmp--) {
		ret = recv_data(tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM)
				usart_delay(3 - i);
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	
	if (verify_all(ptrm, CHKALL) < 0) {
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
		pr_debug("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	
	space_remain--;
	// change to hex
	leflow.consume_sum = bcdl2bin(leflow.consume_sum) * 100;

	// 只做余额调整, 餐限不调整
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		pr_debug("account money below zero!\n");
		pacc->money = 0;
	}
	
	// 终端参数设置
	ptrm->term_money -= leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// 禁止再次接收流水
	// 整理并存储流水
	// init leflow
	leflow.flow_type = LETAKE;
	leflow.date.hyear = *tm++;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm;
	leflow.acc_num = pacc->acc_num;
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
#ifdef CONFIG_RECORD_CASHTERM
	// 终端接收取款流水, 保存状态
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
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
	}
#endif
	return 0;
}

// 收取存款流水
int recv_dep_money(term_ram *ptrm, unsigned char *tm)
{
	le_flow leflow;
	int ret, i, tail, managefee;
	acc_ram *pacc;
	unsigned char *tmp;
	
	// terminal property
	if (!(ptrm->pterm->param.term_type & 0x20)) {
		// 这不是出纳机
		//usart_delay(7);
		return 0;
	}
	
	memset(&leflow, 0, sizeof(leflow));
	
	// first recv 4-byte card number and two-byte money
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
				usart_delay(7 - i);
			}
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 1;
	for (i = 0; i < 2; i++, tmp--) {		//接收2bytes取款额（bcd码）
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM)
				usart_delay(3 - i);
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
	}

	if (verify_all(ptrm, CHKALL) < 0) {
		ptrm->status = NOCOM;
		return 0;
	}

	if (ptrm->flow_flag) {
		pr_debug("not allow recv flow\n");
		ptrm->status = TNORMAL;
		return 0;
	}
	
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		pr_debug("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	space_remain--;
	// change to hex
	leflow.consume_sum = bcdl2bin(leflow.consume_sum) * 100;

	managefee = fee[pacc->flag >> 4];		//管理费比率
	managefee = managefee * leflow.consume_sum / 100;	//计算管理费
	
	pacc->money += leflow.consume_sum - managefee;		//余额加钱减管费
	//printk("pacc->money: %d\n", pacc->money);
	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// 禁止再次接收流水
	// 整理并存储流水
	leflow.flow_type = LECHARGE;
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
	leflow.tml_num = ptrm->term_no;
	leflow.manage_fee = 0 - managefee;
	tail = flowptr.tail;
	memcpy(pflow + tail, &leflow, sizeof(flow));
	// 修改流水指针
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
#ifdef CONFIG_RECORD_CASHTERM
	// 终端接收取款流水, 保存状态
	if (/*(ptrm->pterm->term_type & 0x20)
		&&*/ (cashterm_ptr < CASHBUFSZ) && pacc) {
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
	}
#endif
	return 0;
}

/*
 * 处理异区现金流水:
 * 只会修改账户区或换卡区账户金额
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
		// 此时应进行消费流程带时间
		if (pnflow->AccoType) {
			// 计入补贴账户
			_add_byte3(pacc->sub_money, pnflow->Money);
			//pacc->sub_money += pnflow->Money;
			if (pnflow->LimitType && is_current_tmsg(pnflow->time)) {
				if (pnflow->Money < 0){
				pnflow->Money = -pnflow->Money;
				sub_smoney(pacc, pnflow->Money);
				}
			}
			//printk("cardno %x deal smoney is %x\n", pnflow->CardNo, pnflow->Money);//added by duyy, 2012.9.13
		} else {
			// 计入个人账户
			pacc->money += pnflow->Money;
			if (pnflow->LimitType && is_current_tmsg(pnflow->time)) {
				if (pnflow->Money < 0){
				pnflow->Money = -pnflow->Money;
				sub_money(pacc, pnflow->Money);
				}
			}
			//printk("cardno %x deal money is %x\n", pnflow->CardNo, pnflow->Money);//added by duyy, 2012.9.13
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
		
		// 折半搜索主账户库
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
		// 判断是否为已经换过的卡
		if (pacc && (pacc->power_id & ACCP_CHANGE)) {
			// 已经换过的卡
			pacc = NULL;
		}
		// 不存在于主账户区或换过的卡，检查hash acc
		if (pacc == NULL) {
			// 删除hash中的数据
			pacc = hashtab_remove_d(hsw, (void *)pnflow->OldCardId);
			prcd->account_sw--;
			if (pacc == NULL) {
				// hash中的数据删除错误，此卡不在主账户也不在hash账户中
				printk("waring card %d not in main and hash\n", pnflow->OldCardId);
				return 0;
			}
			
			// 复制原有数据, 更换卡号
			memcpy(&oldacc, pacc, sizeof(oldacc));
			oldacc.card_num = pnflow->NewCardId;
			kfree(pacc);
			pacc = NULL;
		} else {
			// 主账户库里有此数据, 将其数据复制到oldacc，并标志为换过的卡
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
			// hash表中加入换过的卡信息
			if (hashtab_insert(hsw, (void *)acc_new->card_num, acc_new) < 0) {
				kfree(acc_new);
				return 0;
			}
		}
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
	// 加入到换卡区
	pacc = hashtab_remove_d(hsw, (void *)acc->card_num);
	if (pacc) {
		// 已经在换卡区了, 删除
		prcd->account_sw -= 1;
		printk("card no %08x exsit in acc sw\n", (int)acc->card_num);
		kfree(pacc);
	} else { 
		prcd->account_all += 1;
	}
	// 加入到换卡区中
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
		// 判断是否为已经换过的卡
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
		// 新加的卡不能是已注册的卡
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
		// 加入到换卡区
		pacc = hashtab_remove_d(hsw, (void *)oldacc.card_num);
		if (pacc) {
			// 已经在换卡区了, 删除
			prcd->account_sw -= 1;
			printk("card no %08x exsit in acc sw\n", (int)oldacc.card_num);
			kfree(pacc);
		} else { 
			prcd->account_all += 1;
		}
		// 加入到换卡区中
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
 * 将异区非现金流水转为黑名单结构
 */
int nomtoblk(black_acc *pbacc, no_money_flow *pnflow)
{
	usr_time tm;
	memset(pbacc, 0, sizeof(black_acc));
	// 为了提高效率不做参数正确性验证
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

