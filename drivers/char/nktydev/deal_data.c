#include <asm/types.h>
#include "uart_dev.h"
#include "data_arch.h"
#include "uart.h"
#include "deal_data.h"
#include "no_money_type.h"
#include "at91_rtc.h"


#define DEBUG
#undef DEBUG
#undef pr_debug
#ifdef DEBUG
#define pr_debug(fmt,arg...) \
	printk(fmt,##arg)
#else
#define pr_debug(fmt,arg...) \
	do { } while (0)
#endif

extern unsigned int fee[16];
extern flow pflow[FLOWANUM];
extern terminal *pterminal;

extern int flow_sum;
extern int maxflowno;
extern struct flow_info flowptr;

extern int space_remain;

extern term_ram * ptermram;
extern acc_ram * paccmain;

#ifndef CONFIG_ACCSW_USE_HASH
extern acc_ram paccsw[MAXSWACC];
#else
extern struct hashtab *hash_accsw;
#endif

#ifdef EDUREQ
extern struct edupar edup;
#endif

#ifdef M1CARD
extern black_acc *pblack;
#endif
extern struct record_info rcd_info;// ��¼�ն˺��˻���Ϣ
extern int g_tcnt;


#define SERVER_EPOCH		1998
#define is_leap(year) ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#if 0
static const unsigned short int __mon_yday[2][13] =
{
	/* Normal years.  */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* Leap years.  */  
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};
#endif
/*
 * returns day of the year [0-365]
 */
static int compute_yday(int year, int month, int mday)
{
	/* month is in the [0-11] so, we need do nothing */
	return  __mon_yday[is_leap(year)][month]+ mday - 1;
}
/*
 * returns day of the week [1-7] 7=Sunday
 *
 * Don't try to provide a year that's before 1998, please !
 */
static int compute_wday(int year, int month, int mday)
{
	int y, wday;
	int ndays = 0;

	if (year < SERVER_EPOCH) {
		printk("year < 1998, invalid date\n");
		return 7;
	}

	for (y = SERVER_EPOCH; y < year; y++) {
		ndays += 365 + (is_leap(y) ? 1 : 0);
	}
	ndays += compute_yday(year, month, mday);

	/*
	 * 4=1/1/1998 was a Thursday
	 */
	wday = (ndays + 4) % 7;
	if (!wday)
		wday = 7;
	return wday;
}

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
	if (val > 99999999/* || (int)val < 0 */) {
		printk("binl2bcd parameter error %d!\n", (int)val);
		return /* -1 */0;// ����ط�����0�ȽϺ���
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

#ifdef WATER
int record_water_flow(term_ram *ptrm);
#ifndef SENDTIME
#define SENDTIME
#endif
#endif

#if 0
static inline int _fill_date(unsigned char *dst, unsigned char *tm)
{
	*dst++ = *tm++;
	*dst++ = *tm++;
	*dst = *tm;
	return 0;
}
#endif

/*
 * terminal type: 1-byte; le_card once consume: 1-byte; food price: 32-byte; id info: 32-byte
 * return 0 is ok; -1 is send error; -2 is variable error
 */
int send_run_data(term_ram *ptrm
#ifdef CONFIG_QH_WATER
//, unsigned char *tm
#endif
)
{
	int ret, i;
	//unsigned char rdata;
#ifdef SENDTIME
	udelay(20);
#endif
	// first send 1-byte terminal type (HEX)
	ret = send_data((char *)&ptrm->pterm->term_type, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		printk("send run data time out!\n");
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->term_type;
	ptrm->add_verify += (unsigned char)ptrm->pterm->term_type;
	// sencond send 1-byte max consume once (BCD)
	// water: ������ʽ
	ret = send_data((char *)&ptrm->pterm->max_consume, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->max_consume;
	ptrm->add_verify += (unsigned char)ptrm->pterm->max_consume;
	// water: ����2-byte, ʱ��2-byte, no use 28-byte
#ifdef WATER
#ifdef CONFIG_QH_WATER
	// �廪ˮ������Ҫ��������
	//ptrm->pterm->price[5] = 0x20;
	//_fill_date(&ptrm->pterm->price[6], tm);
#endif
	ret = send_data((char *)ptrm->pterm->price, 31, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 31; i++) {
		ptrm->dif_verify ^= ptrm->pterm->price[i];
		ptrm->add_verify += ptrm->pterm->price[i];
	}
	// �����ۼӺ�У��
	ret = send_data((char*)&ptrm->add_verify, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= ptrm->add_verify;
#else
	ret = send_data((char *)ptrm->pterm->price, 32, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 32; i++) {
		ptrm->dif_verify ^= ptrm->pterm->price[i];
		ptrm->add_verify += ptrm->pterm->price[i];
	}
#endif
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
#ifdef WATER
	// pack flow and save
	record_water_flow(ptrm);
	// �����Ƿ�Ҫ��¼ˮ����ˮ, �п������ѻ�����
	memset(&ptrm->w_flow, 0, sizeof(ptrm->w_flow));
#endif
	return 0;
}

/*
 * terminal type: 1-byte; le_card once consume: 1-byte; food price: 32-byte; id info: 32-byte
 * return 0 is ok; -1 is send error; -2 is variable error
 */
int send_run_data2(term_ram *ptrm, unsigned char *tm)
{
	int ret, i;
	//unsigned char rdata;
#ifdef SENDTIME
	udelay(20);
#endif
	// first send 1-byte terminal type (HEX)
	ret = send_data((char *)&ptrm->pterm->term_type, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		printk("send run data time out!\n");
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->term_type;
	ptrm->add_verify += (unsigned char)ptrm->pterm->term_type;
	// sencond send 1-byte max consume once (BCD)
	// water: ������ʽ
	ret = send_data((char *)&ptrm->pterm->max_consume, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= (unsigned char)ptrm->pterm->max_consume;
	ptrm->add_verify += (unsigned char)ptrm->pterm->max_consume;
	// water: ����2-byte, ʱ��2-byte, no use 28-byte
#ifdef WATER
#ifdef CONFIG_QH_WATER
	// �廪ˮ������Ҫ��������
	//ptrm->pterm->price[5] = 0x20;
	//_fill_date(&ptrm->pterm->price[6], tm);
#endif
	ret = send_data((char *)ptrm->pterm->price, 31, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 31; i++) {
		ptrm->dif_verify ^= ptrm->pterm->price[i];
		ptrm->add_verify += ptrm->pterm->price[i];
	}
	// �����ۼӺ�У��
	ret = send_data((char*)&ptrm->add_verify, 1, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	ptrm->dif_verify ^= ptrm->add_verify;
#else
	ret = send_data((char *)ptrm->pterm->price, 32, ptrm->term_no);
	if (ret < 0) {		// timeout
		return ret;
	}
	for (i = 0; i < 32; i++) {
		ptrm->dif_verify ^= ptrm->pterm->price[i];
		ptrm->add_verify += ptrm->pterm->price[i];
	}
	// ����7�ֽ�ʱ��, year, wday, mon, day, hour, min, sec
	{
		int year, wday;
		year = 2000 + BCD2BIN(tm[0]);
		wday = compute_wday(year, BCD2BIN(tm[1]) - 1, BCD2BIN(tm[2]));
		ret = send_data(tm, 1, ptrm->term_no);
		if (ret < 0) {
			return ret;
		}
		ptrm->dif_verify ^= tm[0];
		ptrm->add_verify += tm[0];
		ret = send_data(&wday, 1, ptrm->term_no);
		if (ret < 0) {
			return ret;
		}
		ptrm->dif_verify ^= wday;
		ptrm->add_verify += wday;
		ret = send_data(&tm[1], 5, ptrm->term_no);
		if (ret < 0) {
			return ret;
		}
		for (i = 1; i <= 5; i++) {
			ptrm->dif_verify ^= tm[i];
			ptrm->add_verify += tm[i];
		}
	}
#endif
	if (verify_all(ptrm, CHKXOR) < 0)
		return -1;
#ifdef WATER
	// pack flow and save
	record_water_flow(ptrm);
	// �����Ƿ�Ҫ��¼ˮ����ˮ, �п������ѻ�����
	memset(&ptrm->w_flow, 0, sizeof(ptrm->w_flow));
#endif
	return 0;
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
#ifndef CONFIG_ACCSW_USE_HASH
		pacc = search_no2(cardno, paccsw, rcd_info.account_sw);
#else
		pacc = hashtab_search(hash_accsw, (void *)cardno);
#endif
	} else {
		if ((pacc->feature & 0xC0) == 0xC0) {
			// ����һ�Ż����Ŀ�
			pacc = NULL;
#ifndef CONFIG_ACCSW_USE_HASH
			pacc = search_no2(cardno, paccsw, rcd_info.account_sw);
#else
			pacc = hashtab_search(hash_accsw, (void *)cardno);
#endif
		}
	}
	return pacc;
}

#ifdef S_BID
extern struct bid_info bidinfo;

// wjzhe: renda �ر�ʱ�����Ƿ�����������ڼ����ն�������
// 		  1 -> ��ֹ; 0 -> ����ֹ
static inline int rd_bid(struct bid_info *pbid, char figure, char id, int itm)
{
	int ret = 0;
	// ��ʱ������
	if ((itm >= pbid->tms1 && itm <= pbid->tme1)
		|| (itm >= pbid->tms2 && itm <= pbid->tme2)
		|| (itm >= pbid->tms3 && itm <= pbid->tme3)
		|| (itm >= pbid->tms4 && itm <= pbid->tme4)) {
		// ��ʱ����, �����
		if (((figure == pbid->figure[0]) && (id == pbid->power_id[0]))
			|| ((figure == pbid->figure[1]) && (id == pbid->power_id[1]))
			|| ((figure == pbid->figure[2]) && (id == pbid->power_id[2]))
			) {
			ret = 1;
		}
	}
	return ret;
}
#endif
#ifdef CONFIG_QH_WATER
static inline int cal_money(int money, acc_ram *pacc, int itm, int flag)
{
	return 0;
}
#endif

#ifdef USRCFG1
#ifdef DEBUG
#define USRDBG
#endif
extern struct usr_cfg usrcfg;
extern struct term_money termoney;
static int tmoney;
static int _type;
//#define BASEMONEY 100
// ����, ֻ���������, flag��־�Ƿ����
static inline int cal_money(int money, acc_ram *pacc, int itm, int flag)
{
	int n = ((pacc->feature & 0x30) >> 4);
	struct usr_cfg *ucfg = &usrcfg;
	struct usr_tmsg *usrtm = ucfg->usrtm;
	while (usrtm->no) {
		if (itm > usrtm->begin && itm < usrtm->end) {
			
#ifdef CONFIG_ZLYY
			// ��ʼ��Ǯ,�̶�����ۿ�
			if (ucfg->usrtm[usrtm->no - 1].money[n]) {
#ifdef USRDBG
				printk("�����������Ѳʹ���������ֵ%d %d\n",
					ucfg->usrtm[usrtm->no - 1].money[n], ((pacc->feature & 0x30) >> 4) + 1);
#endif
				//money = ucfg->usrtm[usrtm->no - 1].money[n];//deleted by duyy,2013.5.29
				//modified by duyy, 2013.5.29
				if (ucfg->usrtm[usrtm->no - 1].money[n] == 0xFFFF){
					money = 0;
				} else {
					money = ucfg->usrtm[usrtm->no - 1].money[n];
				}
				break;
			}
#endif	/* CONFIG_ZLYY */
	//printk("cal_money dnflag is %d, pacc->ndisc is %d, discount is %d\n", usrcfg.dnflag, pacc->ndisc[usrtm->no - 1], usrcfg.discount[usrtm->no - 1][n]);
			// ��ʱ���ڣ������ۿ�
			if ((usrcfg.dnflag && pacc->ndisc[usrtm->no - 1] >= usrcfg.discn[usrtm->no - 1][n])
				|| (!usrcfg.dnflag && pacc->ndisc[usrtm->no - 1] < usrcfg.discn[usrtm->no - 1][n])) {
				// ��־Ϊ1ʱ���������ڵ������ô���ʱ�ۿ�
				// ��־Ϊ0ʱ������С�����ô���ʱ�ۿ�
				if (/* usrcfg.discount[n] <= 100 */1) {	// ���ۼ���
					money *= usrcfg.discount[usrtm->no - 1][n];
					if (money % 100 >= 50) {
						money /= 100;
						money++;
					} else {
						money /= 100;
					}
				}
				// ����ʱ��1
				if (!usrcfg.dnflag && flag) {
					pacc->ndisc[usrtm->no - 1]++;
				}
			} else if (usrcfg.dnflag && flag) {
				// ������ʱ��1
				pacc->ndisc[usrtm->no - 1]++;
			}
			break;
		}
		usrtm++;
	}
	return money;
}
// ����ʱ�����
static inline int is_special_v8(struct usr_tmsg *usrtm, acc_ram *pacc, int itm, int *money, int termno)
{
	while (usrtm->no) {
		if (itm > usrtm->begin && itm < usrtm->end) {
			// itm������ʱ��
			// Ǯ������?
			int m;
			unsigned char figure = ((pacc->feature & 0x30) >> 4);	// ���
			// �ж��ն��Ƿ�Ҫ��������
			if (usrtm->tnot[termno / 32] & (1 << (termno % 32))) {
				// ���ն˲�Ҫ��
#ifdef USRDBG
				printk("�ն˲�Ҫ��%08x %d\n", usrtm->tnot[termno / 32], termno);
#endif
				return 0;
			}
			// �ж��ն���������
			switch (usrtm->no) {	// ʱ�α��
				case 1:
					if (termoney.tm1[_type]) {
						tmoney = tmoney <= termoney.tm1[_type];
					}
					break;
				case 2:
					if (termoney.tm2[_type]) {
						tmoney = tmoney <= termoney.tm2[_type];
					}
					break;
				case 3:
					if (termoney.tm3[_type]) {
						tmoney = tmoney <= termoney.tm3[_type];
					}
					break;
				case 4:
					if (termoney.tm4[_type]) {
						tmoney = tmoney <= termoney.tm4[_type];
					}
					break;
				default:
					printk("bug: usrtm->no = %d\n", usrtm->no);
					tmoney = 1;
					break;
			}
#ifdef USRDBG
			printk("tmoney = %d, tm = %d, %d, %d, %d\n", tmoney,
				termoney.tm1[_type], termoney.tm2[_type],
				termoney.tm3[_type], termoney.tm4[_type]);
#endif
			
			// �Խ����ж�
			m = *money;
			if(usrtm->money[figure] == 0xFFFF){//write by duyy, 2013.6.25
				m -= 0;
			} else {
				m -= usrtm->money[figure];
			}	
			if (m < 0) {
				*money = 0;
			} else if (usrtm->imoney) {
				// Ǯ�������0
				m += usrtm->imoney;
				*money = m;
			} else {
				*money = m;
			}
#ifdef USRDBG
				printk("figure %d money %d, and usrtm->money %d\n",
					figure, *money, usrtm->money[figure]);
#endif
			
			// ������?
#ifdef USRDBG
			printk("no %d l%d nflag[no - 1] %d\n", usrtm->no, usrtm->limit[usrtm->no - 1], pacc->nflag[usrtm->no - 1]);
#endif
			if (pacc->nflag[usrtm->no - 1] >= usrtm->limit[figure]) {
#ifdef USRDBG
				printk("���������˵�ǰ����Ϊ%d %d\n", pacc->nflag[usrtm->no - 1], usrtm->limit[figure]);
#endif
				if (usrcfg.flag) {
					return 255;
				}
				return -1;
			}
			pacc->itm = itm;
			return usrtm->no;
		}
		usrtm++;
	}
	tmoney = 1;
#ifdef USRDBG
	printk("pass .... .... \n");
#endif
	return 0;
}
// ����ʱ�����
static inline int is_special(struct usr_tmsg *usrtm, acc_ram *pacc, int itm, int *money, int termno)
{
	while (usrtm->no) {
		if (itm > usrtm->begin && itm < usrtm->end) {
			// itm������ʱ��
			// Ǯ������?
			int m;
			unsigned char figure = ((pacc->feature & 0x30) >> 4);	// ���
			// �ж��ն��Ƿ�Ҫ��������
			if (usrtm->tnot[termno / 32] & (1 << (termno % 32))) {
				// ���ն˲�Ҫ��
#ifdef USRDBG
				printk("�ն˲�Ҫ��%08x %d\n", usrtm->tnot[termno / 32], termno);
#endif
				return 0;
			}
			// �ж��ն���������
			switch (usrtm->no) {	// ʱ�α��
				case 1:
					if (termoney.tm1[_type]) {
						tmoney = tmoney <= termoney.tm1[_type];
					}
					break;
				case 2:
					if (termoney.tm2[_type]) {
						tmoney = tmoney <= termoney.tm2[_type];
					}
					break;
				case 3:
					if (termoney.tm3[_type]) {
						tmoney = tmoney <= termoney.tm3[_type];
					}
					break;
				case 4:
					if (termoney.tm4[_type]) {
						tmoney = tmoney <= termoney.tm4[_type];
					}
					break;
				default:
					printk("bug: usrtm->no = %d\n", usrtm->no);
					tmoney = 1;
					break;
			}
#ifdef USRDBG
			printk("tmoney = %d, tm = %d, %d, %d, %d\n", tmoney,
				termoney.tm1[_type], termoney.tm2[_type],
				termoney.tm3[_type], termoney.tm4[_type]);
#endif
			if (money) {
				// �Խ����ж�
				m = *money;
				if(usrtm->money[figure] == 0xFFFF){//write by duyy, 2013.6.25
					m -= 0;
				} else {
					m -= usrtm->money[figure];
				}
				
				if (m < 0) {
					*money = 0;
				} else if (usrtm->imoney) {
					// Ǯ�������0
					m += usrtm->imoney;
					*money = m;
				} else {
					*money = m;
				}
#ifdef USRDBG
				printk("figure %d money %d, and usrtm->money %d\n",
					figure, *money, usrtm->money[figure]);
#endif
			}
			// ������?
#ifdef USRDBG
			printk("no %d l%d nflag[no - 1] %d\n", usrtm->no, usrtm->limit[usrtm->no - 1], pacc->nflag[usrtm->no - 1]);
#endif
			if (pacc->nflag[usrtm->no - 1] >= usrtm->limit[figure]) {
#ifdef USRDBG
				printk("���������˵�ǰ����Ϊ%d %d\n", pacc->nflag[usrtm->no - 1], usrtm->limit[figure]);
#endif
				if (usrcfg.flag) {
					return 255;
				}
				return -1;
			}
			pacc->itm = itm;
			return usrtm->no;
		}
		usrtm++;
	}
	tmoney = 1;
#ifdef USRDBG
	printk("pass .... .... \n");
#endif
	return 0;
}
// ����0, ��������, ����-1��ֹ����, û�����ݷ���1
static inline int is_spallow(struct usr_allow *ulw, int itm, acc_ram *pacc)
{
	int figure = ((pacc->feature & 0x30) >> 4) + 1;
	// ��������?
	while (ulw->figure) {
		if (ulw->figure == figure && itm >= ulw->begin && itm <= ulw->end) {
#ifdef USRDBG
			printk("OK: f tb te %d %d %d %d %d\n", ulw->figure, figure,
				ulw->begin, ulw->end, itm);
#endif
			return ulw->allow ? 0 : -1;
		}
		ulw++;
	}
	return 1;
}
// ����1��������ʱ��, -1����������, 0��������
// ��ϸ����ֵ˵�� -1 ����������ʱ�� -2 ��������
// 1 ��������ʱ�� 255 ����������
static inline int is_cfg_v8(struct usr_cfg *ucfg, acc_ram *pacc, int *money, int itm, int termno)
{
	int ret = 0;
	int flag = 0;
	// ����ʱ��...�����ж�
	ret = is_spallow(ucfg->usrlw, itm, pacc);
	if (ret == -1) {
		// ����������
#ifdef USRDBG
		printk("���ʱ�β�����%d����\n", ((pacc->feature & 0x30) >> 4) + 1);
#endif
		return -1;
	}
	
	// �Ǹ��������ʱ�ε���?
	if (ret == 0) {
#ifdef USRDBG
		printk("û�����������\n");
#endif
		flag = 1;
	}
	
	if (ret == 1) {	// û������, ���Ծ�������ǰ��ֵ
#ifdef USRDBG
		printk("����û�����������\n");
#endif
		//ret = flag;
	}

	// ���ʱ��������ʱ������?
#ifdef USRDBG
	printk("begin special\n");
#endif
	ret = is_special_v8(ucfg->usrtm, pacc, itm, money, termno);
	//printk("end special\n");
	if (ret == 0) {
		// �������Ѿͺ���
		pacc->itm = 0;
		return ret;
	} else if (ret < 0) {
		// ���������ѵ�, ����������
		if (flag) {
			return 0;
		} else {
			return -2;
		}
		//return ret;
	} else {
		// ����������
		return ret;
	}
}
// ����1��������ʱ��, -1����������, 0��������
// ��ϸ����ֵ˵�� -1 ����������ʱ�� -2 ��������
// 1 ��������ʱ�� 255 ����������
static inline int is_cfg(struct usr_cfg *ucfg, acc_ram *pacc, int *money, int itm, int termno)
{
	int ret = 0;
	int flag = 0;
	// ����ʱ��...�����ж�
	ret = is_spallow(ucfg->usrlw, itm, pacc);
	if (ret == -1) {
		// ����������
#ifdef USRDBG
		printk("���ʱ�β�����%d����\n", ((pacc->feature & 0x30) >> 4) + 1);
#endif
		return -1;
	}
	
	// �Ǹ��������ʱ�ε���?
	if (ret == 0) {
#ifdef USRDBG
		printk("û�����������\n");
#endif
		flag = 1;
	}
	
	if (ret == 1) {	// û������, ���Ծ�������ǰ��ֵ
#ifdef USRDBG
		printk("����û�����������\n");
#endif
		//ret = flag;
	}

	// ���ʱ��������ʱ������?
#ifdef USRDBG
	printk("begin special\n");
#endif
	ret = is_special(ucfg->usrtm, pacc, itm, money, termno);
	//printk("end special\n");
	if (ret == 0) {
		// �������Ѿͺ���
		pacc->itm = 0;
		return ret;
	} else if (ret < 0) {
		// ���������ѵ�, ����������
		if (flag) {
			return 0;
		} else {
			return -2;
		}
		//return ret;
	} else {
		// ����������
		return ret;
	}
}
static inline int recvflow(struct usr_cfg *ucfg, acc_ram *pacc, le_flow *ple, int itm, int termno)
{
	int ret;
	// �������ʱ������?
	unsigned char figure = ((pacc->feature & 0x30) >> 4);
	ret = is_special(ucfg->usrtm, pacc, itm, NULL, termno);
	if (ret == 0) {
		// ��������������˳���
		//pacc->money -= ple->consume_sum;
		//if (pacc->money < 0) {
		//	printk("account money below zero!!!\n");
		//	pacc->money = 0;
		//}
		return 0;
	}
	// ��ʼ��Ǯ
	if (ucfg->usrtm[ret - 1].money[figure]) {
#ifdef USRDBG
		printk("�����������Ѳʹ�������Ǯ%d %d\n",
			ucfg->usrtm[ret - 1].money[figure], ((pacc->feature & 0x30) >> 4) + 1);
#endif
		//ple->consume_sum = ucfg->usrtm[ret - 1].money[figure];//deleted by duyy,2013.5.29
		//modified by duyy, 2013.5.29
		if (ucfg->usrtm[ret - 1].money[figure] == 0xFFFF){
			ple->consume_sum = 0;
		} else {
			ple->consume_sum = ucfg->usrtm[ret - 1].money[figure];
		}
		if (ple->consume_sum < 0) {	// ����Ǹ�ֵ�������Ϊ����
			ple->consume_sum = 0;
		}
#ifdef USRDBG
		printk("�����������Ѳʹ���ʵ��ˮ%d %d\n",
			ple->consume_sum, ((pacc->feature & 0x30) >> 4) + 1);
#endif
		//pacc->money -= ucfg->usrtm->money[figure];
	} else {
#ifdef USRDBG
		printk("�����������Ѳʹ�������Ǯ%d %d\n",
			ple->consume_sum, ((pacc->feature & 0x30) >> 4) + 1);
#endif
		//pacc->money -= ple->consume_sum;
	}
	//if (pacc->money < 0) {
	//	printk("account money below zero!!!\n");
	//	pacc->money = 0;
	//}
	// ��flag
	if (usrcfg.mflag) {
		pacc->nflag[ret - 1] += ple->consume_sum;
	} else {
		pacc->nflag[ret - 1]++;
	}
#ifdef USRDBG
	printk("set nflag[%d] %d\n", ret - 1, pacc->nflag[ret - 1]);
#endif
	pacc->itm = 0;
	return 0;
}
#endif
#ifdef CONFIG_CNTVER
static inline int chk_splterm(unsigned int *pt, int termno)
{
	return (pt[termno / 32] & (1 << termno % 32));
}

static inline int is_cntcfg(struct cnt_conf *cfg, acc_ram *pacc, int *money, unsigned int itm, int termno)
{
	// ���ж��Ƿ�Ϊ�����ն�
	if (!(chk_splterm(cfg->tnot, termno))) {
		// ��ǰλû����λ
		return 0;
	}
	// �ڴ�ʱ�����
	if (itm > cfg->start && itm < cfg->end) {
		// ʱ���ж�
		int daytm = itm % (24 * 3600);
		if (daytm >= cfg->tcfg[0].start && daytm <= cfg->tcfg[0].end) {
			//printk("���ڵ�һʱ��: cnt=%d, itm=%d, tcfg=%d\n", pacc->cnt[0], pacc->itm[0], cfg->tcfg[0].itm);
			if (pacc->cnt[0] <= 0) {
				return -1;
			}
			if (pacc->itm[0] >= cfg->tcfg[0].itm) {
				return -1;
			}
		} else if (daytm >= cfg->tcfg[1].start && daytm <= cfg->tcfg[1].end) {
			//printk("���ڵڶ�ʱ��: cnt=%d, itm=%d, tcfg=%d\n", pacc->cnt[1], pacc->itm[1], cfg->tcfg[1].itm);
			if (pacc->cnt[1] <= 0) {
				return -1;
			}
			if (pacc->itm[1] >= cfg->tcfg[1].itm) {
				return -1;
			}
		} else {
			return -1;
		}
#if 0
		if (pacc->cnt <= 0) {
			return -1;
		}
		// �ж��Ƿ񳬹��˴���
		if (pacc->itm >= cfg->itm) {
			//printk("is_cntcfg: card itm >= cfg->cnt\n");
			return -1;
		}
#endif
		return 0;
	}
	return -1;
}

static inline int cntver_recvflow(le_flow* ple, struct cnt_conf *cfg, acc_ram *pacc, int termno, int itm)
{
	int flag;
	// �ж��Ƿ�Ϊ�����ն�
	if (!(chk_splterm(cfg->tnot, termno))) {
		return 0;
	}
	itm = itm % (24 * 3600);
	if (itm >= cfg->tcfg[0].start && itm <= cfg->tcfg[0].end) {
		pacc->itm[0]++;
		pacc->cnt[0]--;
		flag = 0;
		ple->flow_type = 0xAE;
	} else if (itm >= cfg->tcfg[1].start && itm <= cfg->tcfg[1].end) {
		pacc->itm[1]++;
		pacc->cnt[1]--;
		flag = 1;
		ple->flow_type = 0xAE;
	} else {
		return 0;
	}
	// ��¼��ǰ����
	if (fcntpos < FCNTVERN) {
		fcntver[fcntpos].card_num = pacc->card_num;		// �����˺�
		fcntver[fcntpos].remain[0] = pacc->cnt[0];
		fcntver[fcntpos].remain[1] = pacc->cnt[1];
		fcntver[fcntpos].acc_num = pacc->acc_num;
		fcntpos++;
	} else {
		printk("cntver_recvflow: fcntpos >= FCNTVERN\n");
	}
#if 0
	if (cfpos < FCNTVERN) {
		cfbuf[cfpos].acc_num = pacc->acc_num;
		cfbuf[cfpos].card_num = pacc->card_num;
		cfbuf[cfpos].ctm = flag;
		cfpos++;
	} else {
		printk("cntver_recvflow: cfpos >= FCNTVERN\n");
	}
#endif
	return 0;
}

#endif

static inline unsigned int _card(unsigned int cardno)
{
#ifdef CONFIG_HAIYIN
	// hID card Fx xx xx xF�м�ȡ������
	#warning "haiyin version card = ~card"
	if ((cardno & 0xF000000F) == 0xF000000F) {
		cardno = ~cardno;
		cardno |= 0xF000000F;
	}
#endif
	return cardno;
}


/*
 * ����ʱ�ε��ж�, flag = 1˵���ڴ�ʱ�����ж���
 */
int recv_le_id(term_ram *ptrm, int allow
#if defined (S_BID) || defined (USRCFG1) || defined (CONFIG_CNTVER)
, int itm
#endif
#ifdef EDUREQ
, int flag)
#else
)
#endif
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
#ifdef CONFIG_QH_WATER
	int sub_money = 0;
	char power_id = 0;
#endif
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
	//printk("recv_le_id recieve cardno over\n");//2012.12.10
	cardno = _card(cardno);
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
	} else {
#ifdef DEBUG
		//printk("find!\n");
#endif
		feature = 4;
		//printk("recv_le_id pacc->feature=%x\n", pacc->feature);//2012.3.21
		//if (ptrm->pterm->term_type & 0x20) {
		//	feature = 0;
		//} else {
		//	feature = 4;
		//}
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40	|| (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
			//printk("recv_le_id password\n");//2012.3.21
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			pr_debug("pacc: %d managefee %x\n", pacc->acc_num, pacc->managefee);
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
			}
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
#ifndef WATER
		if ( (ptrm->pterm->power_id == 0xF0) ||
			((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1����ݻ��ʻ�������ն����Ͷ�Ӧ��������������
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
			if (pacc->money < 0) {
				money = 0;
			} else {
				money = (int)pacc->money;// & 0xFFFFF;
			}
		} else {
			feature = 1;
		}
#else
#ifdef CONFIG_QH_WATER
		if (ptrm->is_limit & (1 << pacc->power_id)) {
			feature |= 1 << 6;
		}
		if (!(ptrm->is_use & (1 << pacc->power_id))) {
#ifdef DEBUG
			printk("is_use %02x, powerid=%d\n", ptrm->is_use, pacc->power_id);
#endif
			feature = 1;
		}
		// �����˻����
		sub_money = (int)pacc->sub_money;
		if (sub_money < 0) {
			sub_money = 0;
		}
		power_id = (char)pacc->power_id;
#ifdef DEBUG
		printk("$>>>>sub money: %d\n", sub_money);
#endif
#endif
		// ˮ�ؾͲ��ж��ն�������
		if (pacc->money < 0) {
			money = 0;
		} else {
			money = (int)pacc->money;// & 0xFFFFF;
		}
#endif
		if (money > 999999) {
#ifdef DEBUG
			printk("money is too large, max 999999!\n");
#endif
			money = 999999;
		}
#ifdef EDUREQ
#warning "EDUREQ revision"
		// ��ʱҪ����Ԥ�ۿ�, ��֤POS����ʾ��ȷ�����
		if (flag && (pacc->nflag >= edup.cn)
			&& (((pacc->feature & 0x30) >> 4) == (edup.figure - 1))) {
			// �۳����*rate - 1;
			money -= ptrm->money * (edup.rate - 1);
			if (money < 0)// Ǯ�п��ܱ���Ϊ0
				money = 0;
		}
#endif
///////////////////////////////////////////////////////////////////////////////////
#ifdef S_BID
		if (rd_bid(&bidinfo, ((pacc->feature & 0x30) >> 4) + 1,
			(ptrm->pterm->power_id & 0xF) + 1, itm)) {
				feature = 1;
				money = 0;
		}
#endif
#ifdef USRCFG1
		//printk("cardno %x: pacc money is %d\n", cardno, money);//write by duyy, 2013.6.25
		// �жϴ��˻��Ƿ��������
		ret = is_cfg_v8(&usrcfg, pacc, &money, itm, ptrm->term_no);//modified by duyy, 2013.6.25
		if (ret < 0/* == -1 */) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
#endif
#ifdef CONFIG_CNTVER
		ret = is_cntcfg(&cntcfg, pacc, &money, itm, ptrm->term_no);
		if (ret  < 0) {
			//printk("%d %d %08x����������\n", itm, 
			//	((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
			feature = 1;
			money = 0;
		} else if (ret) {
		}
#endif
	} 

lend:
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
#ifdef DEBUG
		printk("not allow: %d\n", cardno);
#endif
		feature = 1;
		money = 0;
	}
	//printk("send recv_le_id feture=%x\n", feature);//2012.3.21
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
#ifdef CONFIG_QH_WATER
	if (ptrm->pterm->term_type & 0x20){		// ���ɻ�
#endif
	money = binl2bcd(money) & 0xFFFFFF;
#ifdef CONFIG_QH_WATER
	}
#endif
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		tmp--;
	}
	//printk("recv_le_id send money is %x\n", money);//2012.5.7
#ifdef CONFIG_QH_WATER
	if (!(ptrm->pterm->term_type & 0x20)) {	// ֻ�����ѻ����в���
		tmp = (unsigned char *)&sub_money;
		tmp += 1;
		for (i = 0; i < 2; i++) {
			if (send_byte(*tmp, ptrm->term_no) < 0) {
				return -1;
			}
			ptrm->dif_verify ^= *tmp;
			tmp--;
		}
		// ����Ҫ������ݱ�־0~7
		if (send_byte(power_id, ptrm->term_no) < 0) {
			return -1;
		}
		ptrm->dif_verify ^= power_id;
	}
#endif
	if (verify_all(ptrm, CHKXOR) < 0) {
#ifdef DEBUG
		printk("recv le id verify chkxor error %02x\n", feature);
#endif
		return -1;
	}
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	//printk("qinghua water test: recv le id ok\n");
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
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
#ifdef DEBUG
			printk("recv chkxor data error\n");
#endif
			ret = -1;
			break;
		}
		if (ptrm->dif_verify == *tmp) {
#ifdef SENDTIME
			udelay(20);
#endif
			ret = send_byte5(0, ptrm->term_no);
		} else {
			send_byte5(0xFF, ptrm->term_no);
			ret = -1;
#ifdef DEBUG
			printk("recv chkxor data %02x, but i'm %02x\n", *tmp, ptrm->dif_verify);
#endif
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
#ifdef SENDTIME
			udelay(20);
#endif
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
#ifdef DEBUG
			printk("recv chkxor data error\n");
#endif
			ret = -1;
			break;
		}
		if (ptrm->dif_verify == *tmp) {
#ifdef SENDTIME
			udelay(20);
#endif
			ret = send_byte5(0x55, ptrm->term_no);
		} else {
			send_byte5(0xFF, ptrm->term_no);
			ret = -1;
#ifdef DEBUG
			printk("recv chkxor data %02x, but i'm %02x\n", *tmp, ptrm->dif_verify);
#endif
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
#ifdef USRCFG1
static int _cal_limit(int *limit, long money)
{
	if (usrcfg.lflag) {
		(*limit)--;
	} else {
		*limit -= money / 100 + 1;
		if (!(money % 100)) {// �պ���100ʱ, ��Ϊ��һ��
			(*limit)++;
		}
	}
	if (*limit < 0) {
		*limit = 0;
	}
	return 0;
}
#endif
/*
 * receive le card flow, flag is 0 or 1; if flag is 0, consum is HEX; or is BCD
 * but store into flow any data is HEX
 * flag: bit0 is 0, consum is HEX; 1, BCD
 *		 bit1 is 1, CHKALL, modified by wjzhe
 */
int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm
#ifdef CONFIG_CNTVER
, int itm
#elif defined EDUREQ
, int tflag
#else
, int refund
#endif
)
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
	if (refund) {
		tmp = (unsigned char *)&leflow.consume_sum;
		tmp += 2;
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
#ifdef DEBUG
			printk("%d recv leflow consume_sum high ret %d\n", ptrm->term_no, ret);
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
	}
	// second recv 2-byte consume money
	tmp = (unsigned char *)&leflow.consume_sum;
	tmp += 1;		//sizeof(leflow.consume_sum) - 1;
	for (i = 0; i < 2; i++) {
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
	//leflow.consume_sum &= 0xFFFF;
	if (flag & 0x1) {		// if BCD then convert BIN
		leflow.consume_sum = bcdl2bin(leflow.consume_sum);
	}
	// third check NXOR
	if (refund & 0x80) {
		if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
			printk("%d recv le flow verify chkxor error\n", ptrm->term_no);
#endif
			return -1;
		}
	} else {
		if (flag & 0x2) {
			if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
				printk("%d recv le flow verify chkxor error\n", ptrm->term_no);
#endif
				return -1;
			}
		} else if (flag & 0x1) {
			if (verify_all(ptrm, CHKXOR) < 0) {
#ifdef DEBUG
				printk("%d recv le flow verify chkxor error\n", ptrm->term_no);
#endif
				return -1;
			}
		} else {
			if (verify_all(ptrm, CHKNXOR) < 0) {
#ifdef DEBUG
				printk("%d recv le flow verify chkxor error\n", ptrm->term_no);
#endif
				return -1;
			}
		}
	}
	if (ptrm->flow_flag == ~ptrm->term_no) {
		goto next;
	}
	// check if receive the flow
	if (ptrm->flow_flag)
		return 0;
next:
	space_remain--;
	leflow.card_num = _card(leflow.card_num);
	// search id
	pacc = search_id(leflow.card_num);
	if (pacc == NULL) {
		//printk("no cardno: %08x", (unsigned int)leflow.card_num);
		return 0;
	}
	//printk("recv_flow\n");//2012.5.7
	//printk("consume money is %d\n", leflow.consume_sum);//2012.5.7
	// adjust money limit
#ifdef CONFIG_BD//modified by duyy for BeiDa, 2012.12.12
	if ((((pacc->consumelimit & 0xFF00) >> 8) != 0xFF) || (((pacc->consumelimit & 0x00FF) >> 0) != 0xFF)){
		pacc->consumelife += leflow.consume_sum;
	}
	//printk("consume_life is %d\n", pacc->consumelife);
#endif
	limit = pacc->feature & 0xF;
	if ((limit != 0xF) && limit) {
		// ��Ҫ��������
#ifdef USRCFG1
		_cal_limit(&limit, leflow.consume_sum);
#else
		limit -= leflow.consume_sum / 100 + 1;
		if (!(leflow.consume_sum % 100)) {// �պ���100ʱ, ��Ϊ��һ��
			limit++;
		}
		if (limit < 0)
			limit = 0;
#endif
	}

	//if (leflow.consume_sum < 0) {
	//	printk("error!!!!!!!!\n");
	//}
#ifdef EDUREQ
#if 0
	printk("tflag: %d, pacc->nflag: %d 0x%02x\n", tflag, pacc->nflag,
		((pacc->feature & 0x30) >> 4));
	if (tflag && (pacc->nflag >= edup.cn) && ptrm->money
		&& (((pacc->feature & 0x30) >> 4) == (edup.figure - 1))) {
		leflow.consume_sum *= edup.rate;
	}
#endif
	// �ڴ�ʱ����, ������ȷ, ���ն�Ҫ�ӱ���Ǯ
	if (tflag && (((pacc->feature & 0x30) >> 4) == (edup.figure - 1))
		&& ptrm->money) {
		// ����Ѿ��ﵽ���Ѵ���
		if (pacc->nflag >= edup.cn)
			leflow.consume_sum *= edup.rate;
		// �ƴ�
		pacc->nflag++;
	}
#endif
	//printk("recv_leflow first consume money is %d\n", leflow.consume_sum);//2012.5.7
#ifdef USRCFG1
	// �������Ѷ�Ӧ���ǹ̶���, ��leflow->consume_sum
	recvflow(&usrcfg, pacc, &leflow, pacc->itm, ptrm->term_no);
	//printk("recv_leflow second consume money is %d\n", leflow.consume_sum);//2012.5.7
#endif

#ifdef DEBUG
	if (refund) {
		printk("refund leflow: acc %d = %d - %d\n",
			pacc->money - leflow.consume_sum,
			pacc->money, leflow.consume_sum);
	}
#endif

#ifdef CONFIG_SUBCNT
	if (leflow.consume_sum == 0) {
		pacc->sub_flag++;
		pacc->sub_cnt--;
	}
#endif

#ifdef CONFIG_FORCE_DISC//modified by duyy, 2014.1.9,���������ն˲�֧�ִ��۱����۷��жϹ���
	ret = is_special(&usrcfg.usrtm, pacc, _cal_itmofday(tm), NULL, ptrm->term_no);
	if (ret == 0) {
		//printk("term no %d cannot disc\n", ptrm->term_no);
	} else {
		leflow.consume_sum = cal_money(leflow.consume_sum,
		pacc, _cal_itmofday(tm), 1);
	}
#else
	ret = is_special(&usrcfg.usrtm, pacc, _cal_itmofday(tm), NULL, ptrm->term_no);
	if (ret == 0) {
		//printk("term no %d cannot disc\n", ptrm->term_no);
	} else {
		if (ptrm->flow_flag == ~ptrm->term_no) {
			leflow.consume_sum = cal_money(leflow.consume_sum,
				pacc, _cal_itmofday(tm), 1);
		}
	}
#endif
	//printk("recv_leflow last consume money is %d\n", leflow.consume_sum);//2012.5.7
	//printk("before consume is %d\n", pacc->money);//2012.5.7
	pacc->money -= leflow.consume_sum;
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}
	//printk("after consume is %d\n", pacc->money);//2012.5.7
	pacc->feature &= 0xF0;
	pacc->feature |= limit;
	ptrm->term_cnt++;
	g_tcnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
	if (refund) {
		leflow.flow_type = LEREFUND;
#ifdef DEBUG
		printk("term %d, refund %d\n", ptrm->term_no, leflow.consume_sum);
#endif
	} else {
		leflow.flow_type = 0xAB;
	}
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
#ifdef CONFIG_CNTVER
	cntver_recvflow(&leflow, &cntcfg, pacc, ptrm->term_no, itm);
#endif
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
#endif

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

// ����˫У���־
int recv_take_money(term_ram *ptrm, unsigned char *tm, int flag)
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
	leflow.card_num = _card(leflow.card_num);
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
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
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
		// �����ն˽��
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
	}
#endif
	return 0;
}

// ��ȡ�����ˮ
int recv_dep_money(term_ram *ptrm, unsigned char *tm, int flag)
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
		ptrm->add_verify += *tmp;
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
	leflow.card_num = _card(leflow.card_num);
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
	managefee = fee[pacc->managefee & 0xF];		//����ѱ���
	//pr_debug("pacc->flag is: %d\n", pacc->flag ); //2012.1.13�޸ģ������˵�����Ϣ��by Duyy
 	//for (i = 0; i < 16; i++){
	//	pr_debug("fee array is: %d\n", fee[i] ); //2012.1.13�޸ģ������˵�����Ϣ��by Duyy
 	//}
	//pr_debug("manageID is: %x\n", managefee); //2012.1.13�޸ģ������˵�����Ϣ��by Duyy
	managefee = managefee * leflow.consume_sum / 100;	//��������
	//pr_debug("managefee is: %x\n", managefee); //2012.1.13�޸ģ������˵�����Ϣ��by Duyy
	//printk("pacc->money: %d, managefee: %d\n", pacc->money, managefee);
	pacc->money += leflow.consume_sum - managefee;		//����Ǯ���ܷ�
	//printk("pacc->money: %d\n", pacc->money);
	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �����洢��ˮ
#ifdef CONFIG_REFUND
	if (pacc->money == (leflow.consume_sum - managefee)) {
		leflow.flow_type = LEFCHARGE;
	} else {
#endif
		leflow.flow_type = 0xAC;
#ifdef CONFIG_REFUND
	}
#endif
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
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
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
		// �����ն˽��
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
	}
#endif
	return 0;
}
#ifdef CONFIG_CNTVER
#if 0
acc_ram *search_acc(unsigned long acc_num, acc_ram *pacc, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++) {
		if (pacc[i].acc_num == acc_num) {
			pacc += i;
			goto end;
		}
	}
	pacc = NULL;
end:
	return pacc;
}

acc_ram *search_id2(unsigned int acc_num)
{
	acc_ram *pacc;
	pacc = search_acc(acc_num, paccmain, rcd_info.account_main);
	if (pacc == NULL) {
		// can not found at main account
#ifndef CONFIG_ACCSW_USE_HASH
		pacc = search_acc(acc_num, paccsw, rcd_info.account_sw);
#else
		acc_ram *pasw = kmalloc(sizeof(acc_ram) * rcd_info.account_sw, GFP_KERNEL);
		hashtab_getall(hash_accsw, pasw, rcd_info.account_sw);
		pacc = search_acc(acc_num, pasw, rcd_info.account_sw);
		kfree(pasw);
#endif
	} else {
		if ((pacc->feature & 0xC0) == 0xC0) {
			// ����һ�Ż����Ŀ�
			pacc = NULL;
#ifndef CONFIG_ACCSW_USE_HASH
			pacc = search_acc(acc_num, paccsw, rcd_info.account_sw);
#else
{
			acc_ram *pasw = kmalloc(sizeof(acc_ram) * rcd_info.account_sw, GFP_KERNEL);
			hashtab_getall(hash_accsw, pasw, rcd_info.account_sw);
			pacc = search_acc(acc_num, pasw, rcd_info.account_sw);
			kfree(pasw);
}
			//pacc = hashtab_search(hash_accsw, (void *)cardno);
#endif
		}
	}
	return pacc;
}
#endif
int deal_cnt_flow(struct cntflow *pcf, int n)
{
	acc_ram *pacc = NULL;
	int i;
	for (i = 0; i < n; i++, pcf++) {
		pacc = search_id(pcf->card_num);
		if (pacc == NULL) {
			printk("deal_cnt_flow: can not find %x\n", pcf->card_num);
#if 0
			// search from acc
			pacc = search_id2(pcf->card_num);
			if (pacc == NULL) {
				printk("deal_cnt_flow: can not find acc %d\n", pcf->acc_num);
				continue;
			}
#endif
			continue;
		}
		pacc->cnt[(pcf->ctm - 1) & 0x1]--;
		//printk("deal cnt flow: %x, %d, cnt=%d %d\n", pacc->card_num, pcf->ctm, pacc->cnt[0], pacc->cnt[1]);
		if (fcntpos < FCNTVERN) {
			fcntver[fcntpos].card_num = pacc->card_num;
			fcntver[fcntpos].remain[0] = pacc->cnt[0];
			fcntver[fcntpos].remain[1] = pacc->cnt[1];
			fcntver[fcntpos].acc_num = pacc->acc_num;
			fcntpos++;
		} else {
			printk("deal_cnt_flow cntver_recvflow: fcntpos >= FCNTVERN\n");
		}
	}
	return 0;
}
#endif
extern int get_itm(void);

/*
 * ���������ֽ���ˮ:
 * ֻ���޸��˻����򻻿����˻����
 */
#ifndef CONFIG_ACCSW_USE_HASH
int deal_money(money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd)
{
	acc_ram *pacc = NULL;
	if (pnflow == NULL || pacc_m == NULL || prcd == NULL) {
		return -1;
	}
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
#ifdef USRCFG1
{
	struct usr_tmsg *usrtm = usrcfg.usrtm;
	int itm = get_itm();
	while (usrtm->no) {
		if (itm > usrtm->begin && itm < usrtm->end) {
			// itm������ʱ��
			if (usrcfg.mflag) {
				if (pnflow->Money < 0) {
					pacc->nflag[usrtm->no - 1] -= pnflow->Money;
				}
			} else {
				//pacc->nflag[usrtm->no - 1]++;//modified by duyy,2012.2.29
			}
			if(pnflow->AccountId & 0x80000000){//�����λΪ1ʱ��ʾҪͳ���������Ѵ�����write by duyy, 2014.2.26
				pacc->nflag[usrtm->no - 1]++;
			}
#ifdef USRDBG
			printk("no %d l%d nflag[no - 1] %d\n", usrtm->no, usrtm->limit, pacc->nflag[usrtm->no - 1]);
#endif
			break;
		}
		usrtm++;
	}
	if (pnflow->Money <= 0 && usrcfg.sflag) {//��������0��ˮʱҲҪ��������modified by duyy, 2014.2.26
		int limit;
		limit = pacc->feature & 0xF;
		// ��������
		if (limit != 0xF) {
			_cal_limit(&limit, -pnflow->Money);
			pacc->feature &= 0xF0;
			pacc->feature |= limit;
		}
	}
}
//	if (pnflow < 0 && usrcfg.mflag) {
//		pacc->nflag[(pacc->feature & 0x30) >> 4] -= pnflow->Money;
//	}
#endif
#ifdef CONFIG_BD//added by duyy, 2012.12.21
	if (pnflow->Money < 0){
		if ((((pacc->consumelimit & 0xFF00) >> 8) != 0xFF) || (((pacc->consumelimit & 0x00FF) >> 0) != 0xFF)){
			pacc->consumelife -= pnflow->Money;
		}
	}
#endif
#ifdef CONFIG_QH_WATER
	if (pnflow->AccountId) {
		pacc->sub_money += pnflow->Money;
	} else {
		pacc->money += pnflow->Money;
	}
#else
	pacc->money += pnflow->Money;//=================
#endif
	return 0;
}
#else
int deal_money(money_flow *pnflow, acc_ram *pacc_m, struct hashtab *hsw, struct record_info *prcd)
{
	acc_ram *pacc = NULL;
	if (pnflow == NULL || pacc_m == NULL || prcd == NULL) {
		return -1;
	}
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
		pacc = hashtab_search(hsw, (void *)pnflow->CardNo);
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->CardNo);
		}
	}
	if (pacc == NULL) {
		printk("can not find card no: %08x\n", (unsigned int)pnflow->CardNo);
		return 0;
	}
#ifdef USRCFG1
{
	struct usr_tmsg *usrtm = usrcfg.usrtm;
	int itm = get_itm();
//�ƴ���������һ�μ�һ��
//	if (pnflow->AccountId & BIT30)//modified by zhaozhe,2012.1.3
//		break;
	while (usrtm->no) {
		if (itm > usrtm->begin && itm < usrtm->end) {
			// itm������ʱ��
			if (usrcfg.mflag) {
				if (pnflow->Money < 0) {
					pacc->nflag[usrtm->no - 1] -= pnflow->Money;
				}
			} else {
				//pacc->nflag[usrtm->no - 1]++;//modified by duyy,2012.2.29
			}
			if(pnflow->AccountId & 0x80000000){//�����λΪ1ʱ��ʾҪͳ���������Ѵ�����write by duyy, 2014.2.26
				pacc->nflag[usrtm->no - 1]++;
			}
#ifdef USRDBG
			printk("no %d l%d nflag[no - 1] %d, mflag is %x\n", usrtm->no, usrtm->limit, pacc->nflag[usrtm->no - 1], usrcfg.mflag);
#endif
			break;
		}
		usrtm++;
	}
//���ܲʹΣ��д�������
	// feature 0:3		
	if (pnflow->Money <= 0 && usrcfg.sflag) {//��������0��ˮʱҲҪ��������modified by duyy, 2014.2.26
		int limit;
	//	if (/*���ƴ���*/ pnflow->AccountId & BIT31) {//modified by zhaozhe,2012.1.3
	//		break;
	//		}
		limit = pacc->feature & 0xF;
		if ((limit != 0xF) && limit) {
			_cal_limit(&limit, -pnflow->Money);
			pacc->feature &= 0xF0;
			pacc->feature |= limit;
		}
	}
}
#endif
#ifdef CONFIG_BD//added by duyy, 2012.12.21
	if (pnflow->Money < 0){
		if ((((pacc->consumelimit & 0xFF00) >> 8) != 0xFF) || (((pacc->consumelimit & 0x00FF) >> 0) != 0xFF)){
			pacc->consumelife -= pnflow->Money;
		}
		//printk("pnflow->Money is %d\n", pnflow->Money);
		//printk("pacc->consumelife is %d\n", pacc->consumelife);
	}
#endif
#ifdef CONFIG_QH_WATER
	if (pnflow->AccountId) {
		pacc->sub_money += pnflow->Money;
	} else {
		pacc->money += pnflow->Money;
	}
#else
	pacc->money += pnflow->Money;//=================
	//printk("pnflow->Money is %d\n", pnflow->Money);
#endif
	return 0;
}
#endif
#ifndef CONFIG_ACCSW_USE_HASH
int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd)
{
	acc_ram *pacc;
	unsigned char tmp;
	acc_ram oldacc;
	if (pnflow == NULL || pacc_m == NULL || pacc_s == NULL) {
		return -1;
	}
	memset(&oldacc, 0, sizeof(oldacc));
	switch (pnflow->Type) {
	case NOM_LOSS_CARD:
	case NOM_LOGOUT_CARD:
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
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
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
find20:
			pacc = search_no2(pnflow->OldCardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->OldCardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		pacc->feature |= 0x80;
		break;
	case NOM_FIND_CARD:
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
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
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
find21:
			pacc = search_no2(pnflow->OldCardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->OldCardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->feature &= 0x7F;
		break;
	case NOM_SET_THIEVE:
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
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
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
find22:
			pacc = search_no2(pnflow->OldCardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->OldCardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		pacc->feature |= 0x40;
		break;
	case NOM_CLR_THIEVE:
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
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
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
find23:
			pacc = search_no2(pnflow->OldCardId, pacc_s, prcd->account_sw);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->OldCardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
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
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
		if (pacc == NULL) {
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
			printk("this operation is change card, so return\n");
			prcd->account_sw--;
			return 0;
		}
		oldacc.feature = pacc->feature;
		oldacc.acc_num = pacc->acc_num;
		oldacc.card_num = pnflow->NewCardId;
		oldacc.managefee = pacc->managefee;
		oldacc.money = pacc->money;
#ifdef CONFIG_QH_WATER
		oldacc.power_id = pacc->power_id;
		oldacc.sub_money = pacc->sub_money;
#endif
#ifdef USRCFG1
		oldacc.itm = pacc->itm;
		memcpy(oldacc.nflag, pacc->nflag, sizeof(oldacc.nflag));
#endif
#ifdef EDUREQ
		oldacc.nflag = pacc->nflag;		// BUG, EDU must cp nflag
#endif
#ifdef CONFIG_CNTVER
		oldacc.itm = pacc->itm;
		oldacc.cnt = -1;//pacc->cnt;
		oldacc.page = pacc->page;
#endif
#ifdef CONFIG_BD//added by duyy for BeiDa, 2012.12.12
		oldacc.consumelimit = pacc->consumelimit;
		oldacc.consumelife = pacc->consumelife;
#endif
		pacc->feature &= 0x3F;
		pacc->feature |= 0xC0;
		prcd->account_all += 1;
		if (sort_sw(pacc_s, &oldacc, (prcd->account_sw - 1)) < 0)
			return -1;
		break;
	case NOM_CURRENT_RGE:
		// �¼ӵĿ���������ע��Ŀ�
		if (search_id(pnflow->OldCardId)) {
			printk("new regist card exist\n");
			return -1;
		}
		prcd->account_sw += 1;
		if (prcd->account_sw >= MAXSWACC) {
			prcd->account_sw--;
			printk("change card area full!\n");
			return -1;
		}
		oldacc.card_num = pnflow->OldCardId;
		oldacc.acc_num = pnflow->AccountId;
		oldacc.managefee = (pnflow->ManageId - 1) & 0xF;//...
		// ���ӽ�ֹ���λ
		oldacc.managefee |= pnflow->ManageId & ACCDEPBIT;
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
#ifdef CONFIG_BD//added by duyy for BeiDa, 2012.12.20
		oldacc.consumelimit = (pnflow->UpLimitMoney & 0xFFFF00) >> 8;
		oldacc.consumelife = 0;
#endif		
#ifdef CONFIG_QH_WATER
		oldacc.power_id = pnflow->PowerId & 0x7;
		oldacc.sub_money = pnflow->sub_money;
#endif
#ifdef CONFIG_CNTVER
		oldacc.cnt = -1;
#endif
		if (sort_sw(pacc_s, &oldacc, (prcd->account_sw - 1)) < 0) {
			printk("add new account failed!\n");
			return 0;
		}
		prcd->account_all += 1;
		break;
	default:
		break;
	}
	return 0;
}
#else
int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m, struct hashtab *hsw, struct record_info *prcd)
{
	acc_ram *pacc;
	unsigned char tmp;
	acc_ram oldacc;
	int ret;
	if (pnflow == NULL || pacc_m == NULL) {
		return -1;
	}
	memset(&oldacc, 0, sizeof(oldacc));
	switch (pnflow->Type) {
	case NOM_LOSS_CARD:
	case NOM_LOGOUT_CARD:
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
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
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
find20:
			pacc = hashtab_search(hsw, (void *)pnflow->OldCardId);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->OldCardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		pacc->feature |= 0x80;
		break;
	case NOM_FIND_CARD:
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
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
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
find21:
			pacc = hashtab_search(hsw, (void *)pnflow->OldCardId);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->OldCardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->feature &= 0x7F;
		break;
	case NOM_SET_THIEVE:
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
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
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
find22:
			pacc = hashtab_search(hsw, (void *)pnflow->OldCardId);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->OldCardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
			return 0;
		}
		pacc->feature &= 0x3F;
		pacc->feature |= 0x40;
		break;
	case NOM_CLR_THIEVE:
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
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
			printk("other area flow: card no: %08x is not exit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
find23:
			pacc = hashtab_search(hsw, (void *)pnflow->OldCardId);
			if (pacc == NULL) {
				printk("other area flow: card no: %08x is not exit at sw acc ram!\n", (unsigned int)pnflow->OldCardId);
			}
		}
		if (pacc == NULL) {
			printk("can not find card no: %08x\n", (unsigned int)pnflow->OldCardId);
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
		pacc = search_no2(pnflow->OldCardId, pacc_m, prcd->account_main);
		if (pacc && (pacc->feature & 0xC0) == 0xC0) {
#ifdef DEBUG
			printk("NOM_CHANGED_CARD find a changed card\n");
#endif
			pacc = NULL;
		}
		if (pacc == NULL) {
#ifdef DEBUG
			printk("NOM_CHANGED_CARD: card no %08x is not exsit at main acc ram!\n", (unsigned int)pnflow->OldCardId);
#endif
			// �����������������, ����ɾ��
			pacc = hashtab_remove_d(hsw, (void *)pnflow->OldCardId);
			//pacc = hashtab_search(hsw, (void *)pnflow->CardNo);
			prcd->account_sw--;
			if (pacc == NULL) {
				// û�������, ������
#ifdef DEBUG
				printk("hash table no card %08x\n", pnflow->OldCardId);
#endif
				return 0;
			}
#ifdef DEBUG
			printk("hash table find it, and remove over\n");
#endif
			oldacc.feature = pacc->feature;
			oldacc.acc_num = pacc->acc_num;
			oldacc.card_num = pnflow->NewCardId;
			oldacc.managefee = pacc->managefee;
			oldacc.money = pacc->money;
#ifdef CONFIG_QH_WATER
			oldacc.power_id = pacc->power_id;
			oldacc.sub_money = pacc->sub_money;
#endif
#ifdef USRCFG1
			oldacc.itm = pacc->itm;
			memcpy(oldacc.nflag, pacc->nflag, sizeof(oldacc.nflag));
#endif
#ifdef EDUREQ
			oldacc.nflag = pacc->nflag;		// BUG, EDU must cp nflag
#endif
#ifdef CONFIG_CNTVER
			oldacc.itm[0] = pacc->itm[0];
			oldacc.itm[1] = pacc->itm[1];
			oldacc.cnt[0] = -1;//pacc->cnt;
			oldacc.cnt[1] = -1;
			oldacc.page = pacc->page;
#endif
#ifdef CONFIG_RDFZ
			memcpy(oldacc.user_name, pacc->user_name, sizeof(oldacc.user_name));
#endif
#ifdef CONFIG_SUBCNT
			oldacc.sub_cnt = pacc->sub_cnt;
			oldacc.sub_flag = pacc->sub_flag;
#endif
#ifdef CONFIG_ZLYY
			oldacc.m_draw = pacc->m_draw;
			oldacc.m_alarm = pacc->m_alarm;
#endif
#ifdef CONFIG_BD//added by duyy for BeiDa, 2012.12.12
			oldacc.consumelimit = pacc->consumelimit;
			oldacc.consumelife = pacc->consumelife;
#endif
			kfree(pacc);
			pacc = NULL;
		} else {
			oldacc.feature = pacc->feature;
			oldacc.acc_num = pacc->acc_num;
			oldacc.card_num = pnflow->NewCardId;
			oldacc.managefee = pacc->managefee;
			oldacc.money = pacc->money;
#ifdef CONFIG_QH_WATER
		oldacc.power_id = pacc->power_id;
		oldacc.sub_money = pacc->sub_money;
#endif
#ifdef USRCFG1
			oldacc.itm = pacc->itm;
			memcpy(oldacc.nflag, pacc->nflag, sizeof(oldacc.nflag));
#endif
#ifdef EDUREQ
			oldacc.nflag = pacc->nflag;		// BUG, EDU must cp nflag
#endif
#ifdef CONFIG_CNTVER
			oldacc.itm[0] = pacc->itm[0];
			oldacc.cnt[0] = pacc->cnt[0];
			oldacc.itm[1] = pacc->itm[1];
			oldacc.cnt[1] = pacc->cnt[1];
			oldacc.page = pacc->page;
#endif
#ifdef CONFIG_RDFZ
			memcpy(oldacc.user_name, pacc->user_name, sizeof(oldacc.user_name));
#endif
#ifdef CONFIG_SUBCNT
			oldacc.sub_cnt = pacc->sub_cnt;
			oldacc.sub_flag = pacc->sub_flag;
#endif
#ifdef CONFIG_ZLYY
			oldacc.m_draw = pacc->m_draw;
			oldacc.m_alarm = pacc->m_alarm;
#endif
#ifdef CONFIG_BD//added by duyy for BeiDa, 2012.12.12
			oldacc.consumelimit = pacc->consumelimit;
			oldacc.consumelife = pacc->consumelife;
#endif
			pacc->feature &= 0x3F;
			pacc->feature |= 0xC0;
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
#ifdef DEBUG
			printk("hash table insert card %08x acc %d\n",
				(unsigned int)acc_new->card_num, (int)acc_new->acc_num);
#endif
			ret = hashtab_insert(hsw, (void *)acc_new->card_num, acc_new);
			if (ret < 0) {
				printk("out of memory\n");
				kfree(acc_new);
				return 0;
			}
		}
		break;
	case NOM_CURRENT_RGE:
#if 0
		// �¼ӵĿ���������ע��Ŀ�
		if (search_id(pnflow->OldCardId)) {
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
		oldacc.card_num = pnflow->OldCardId;
		oldacc.acc_num = pnflow->AccountId;
		oldacc.managefee = (pnflow->ManageId - 1) & 0xF;//...
		// ���ӽ�ֹ���λ
		oldacc.managefee |= pnflow->ManageId & ACCDEPBIT;
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
#ifdef CONFIG_QH_WATER
		oldacc.power_id = pnflow->PowerId & 0x7;
		oldacc.sub_money = pnflow->sub_money;
#endif
#ifdef CONFIG_CNTVER
		oldacc.cnt[0] = -1;
		oldacc.cnt[1] = -1;
#endif
#ifdef CONFIG_RDFZ
		memset(oldacc.user_name, 0, sizeof(oldacc.user_name));
		//strncpy(oldacc.user_name, pnflow->...);
#endif
#ifdef CONFIG_SUBCNT
		oldacc.sub_cnt = (pnflow->PowerId >> 24) & 0xFF;
		oldacc.sub_flag = 0;
#endif
#ifdef CONFIG_ZLYY
		{
			short tmp = pnflow->NewCardId & 0xFFFF;
			oldacc.m_alarm = tmp * 100;
		}
		oldacc.m_draw = (pnflow->NewCardId >> 16) * 100;
#endif
#ifdef CONFIG_BD//added by duyy for BeiDa, 2012.12.12
		oldacc.consumelimit = (pnflow->UpLimitMoney & 0xFFFF00) >> 8;
		oldacc.consumelife = 0;
#endif	
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
			ret = hashtab_insert(hsw, (void *)acc_new->card_num, acc_new);
			if (ret < 0) {
				printk("out of memory\n");
				kfree(acc_new);
				return 0;
			}
		}
		break;
	default:
		break;
	}
	return 0;
}
#endif
#ifndef CONFIG_ACCSW_USE_HASH
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
#endif

#if defined WATER || defined CONFIG_QH_WATER
#define DISPATCH_FLOW

static inline void _water_change_money(term_ram *ptrm, int money)
{
	acc_ram *pacc = NULL;
	// �����ʻ����
	pacc = search_id(ptrm->w_flow.cardno);
	if (pacc == NULL) {
#ifdef DEBUG
		printk("record water flow: no card %08x\n", ptrm->w_flow.cardno);
#endif
		return;
	}
#ifdef CONFIG_QH_WATER
	if (ptrm->w_flow.sub_flag) {
		pacc->sub_money -= money;
		if (pacc->sub_money < 0) {
			pacc->sub_money = 0;
		}
	} else {
		pacc->money -= money;
		if (pacc->money < 0) {
			pacc->money = 0;
		}
	}
#else
	pacc->money -= money;
	if (pacc->money < 0) {
		pacc->money = 0;
	}
#endif
	return;	
}
//#define DEBUG
// ��¼ˮ���ն���ˮ, ���ؼ�¼����
int record_water_flow(term_ram *ptrm)
{
	int tail;
#ifndef DISPATCH_FLOW
	acc_ram *pacc = NULL;
#endif
	unsigned char *tm = (unsigned char *)ptrm->w_flow.tm;
	le_flow leflow;
	int ret;
	if (ptrm->w_flow.money <= 0) {
#ifdef DEBUG
		printk("record no money flow: card %08x, money %d\n", ptrm->w_flow.cardno, ptrm->w_flow.money);
#endif
		ret = 0;
		goto end;
	}
#ifndef DISPATCH_FLOW
	_water_change_money(ptrm, ptrm->w_flow.money);
#endif
#if 0
	// �����ʻ����
	pacc = search_id(ptrm->w_flow.cardno);
	if (pacc == NULL) {
#ifdef DEBUG
		printk("record water flow: no card %08x\n", ptrm->w_flow.cardno);
#endif
		ret = 0;
		goto end;
	}
#ifdef CONFIG_QH_WATER
	if (ptrm->w_flow.sub_flag) {
		pacc->sub_money -= ptrm->w_flow.money;
		if (pacc->sub_money < 0) {
			pacc->sub_money = 0;
		}
	} else {
		pacc->money -= ptrm->w_flow.money;
		if (pacc->money < 0) {
			pacc->money = 0;
		}
	}
#else
	pacc->money -= ptrm->w_flow.money;
	if (pacc->money < 0) {
		pacc->money = 0;
	}
#endif
#endif
	memset(&leflow, 0, sizeof(leflow));
	// init leflow
	leflow.flow_type = 0xAB;
#ifdef CONFIG_QH_WATER
	if (ptrm->w_flow.sub_flag) {
		leflow.flow_type = 0xAF;
	}
#endif
	// complete it
	leflow.date.hyear = 0x20;
	leflow.date.lyear = *tm++;
	leflow.date.mon = *tm++;
	leflow.date.mday = *tm++;
	leflow.date.hour = *tm++;
	leflow.date.min = *tm++;
	leflow.date.sec = *tm++;
	// terminal number
	leflow.tml_num = ptrm->term_no;
	// account number
	leflow.acc_num = ptrm->w_flow.accno;
	// card number
	leflow.card_num = ptrm->w_flow.cardno;
	// consum money
	leflow.consume_sum = ptrm->w_flow.money;
	// do with flow number
	leflow.flow_num = maxflowno++;
	//maxflowno = flow_no;
	// ��ˮ��ͷβ�Ĵ���
	tail = flowptr.tail;
	//printk("recv le flow tail: %d\n", tail);
	memcpy(pflow + tail, &leflow, sizeof(flow));
	tail++;
	if (tail == FLOWANUM)
		tail = 0;
	flowptr.tail = tail;
	flow_sum++;
	// �ն����Ѽ�Ǯ
	ptrm->term_money += leflow.consume_sum;
	ret = 1;
	// flow area clear
#ifdef DEBUG
	printk("record flow: card %02x, money %d\n", leflow.card_num, leflow.consume_sum);
#endif
end:
	memset(&ptrm->w_flow, 0, sizeof(ptrm->w_flow));
	return ret;
}

static void record_water_new(term_ram *ptrm, unsigned long cardno, int money)
{
	acc_ram *pacc = NULL;
	memset(&ptrm->w_flow, 0, sizeof(ptrm->w_flow));
	// search id
	pacc = search_id(cardno);
	if (pacc == NULL) {
		printk("record water new: no cardno: %08x", (unsigned int)cardno);
		return;
	}
	// card exist, record it
	ptrm->w_flow.cardno = cardno;
	ptrm->w_flow.accno = pacc->acc_num;
#ifdef CONFIG_QH_WATER
	ptrm->w_flow.money = money & 0x7FFF;
	ptrm->w_flow.sub_flag = money >> 15;
#else
	ptrm->w_flow.money = money;
#endif
#ifdef DEBUG
	printk("%d: add a new tmp: card %08x, money %d\n", ptrm->term_no, ptrm->w_flow.cardno, ptrm->w_flow.money);
#endif
	return;
}

int recv_water_single_flow(term_ram *ptrm, unsigned char *tm)
{
	int ret, i;
	unsigned char *tmp;
	unsigned long cardno;
	int money = 0;
	// first recv 4-byte card number
	tmp = (unsigned char *)&cardno;
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
	tmp = (unsigned char *)&money;
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
	// third check NXOR
	if (verify_all(ptrm, CHKNXOR) < 0)
		return -1;
	// check if receive the flow
	if (ptrm->flow_flag) {
#ifdef DEBUG
		printk("recv water single: not allow recv flow\n");
#endif
		return 0;
	}
	// money is BCD code, so change it to HEX
#ifndef CONFIG_QH_WATER
	money = bcdl2bin(money);
#endif
	// �廪ˮ����ˮֵ��λ��ǰbit15��־�����˻�����
	if (ptrm->w_flow.cardno == 0) {
		// �¿�����
#ifdef DEBUG
		printk("tmp flow no card, term %d\n", ptrm->term_no);
#endif
		// flow area cardno is zero, record new
		record_water_new(ptrm, cardno, money);
	} else if (ptrm->w_flow.cardno != cardno) {
#ifdef DEBUG
		printk("water machine changed card term %d\n", ptrm->term_no);
#endif
		// ��;����
		// card changed, record flow
		record_water_flow(ptrm);
		// and record new flow
		record_water_new(ptrm, cardno, money);
	}
#ifdef CONFIG_QH_WATER
	else if (ptrm->w_flow.sub_flag != (money >> 15)) {
		// ��ˮ���͸ı�
#ifdef DEBUG
		printk("water machine changed flow type: %d->%d\n",
			ptrm->w_flow.sub_flag, money >> 15);
#endif
		record_water_flow(ptrm);
		record_water_new(ptrm, cardno, money);
	}
#endif
	else {
		// card not changed, add money, update time
#ifdef CONFIG_QH_WATER
		ptrm->w_flow.money += money & 0x7FFF;
#else
		ptrm->w_flow.money += money;
#endif
	}
	memcpy(ptrm->w_flow.tm, tm, 6);
#ifdef DISPATCH_FLOW
#ifdef CONFIG_QH_WATER
	_water_change_money(ptrm, money & 0x7FFF);
#else
	_water_change_money(ptrm, money);
#endif
#endif
	// not allow recv flow
	ptrm->flow_flag = ptrm->term_no;
#ifdef DEBUG
	printk("%d recv single flow: card %08x, money %d, now %d\n", ptrm->term_no, cardno, money, ptrm->w_flow.money);
#endif
	return 0;
}

int recv_water_order(term_ram *ptrm)
{
	// check nxor
	if (verify_all(ptrm, CHKNXOR) < 0)
		return -1;
#if 0
	// check if receive the flow
	if (ptrm->flow_flag) {
#ifdef DEBUG
		printk("recv water single: not allow recv flow\n");
#endif
		return 0;
	}
#endif
	// pack flow and save
	record_water_flow(ptrm);
	return 0;
}

#endif

#ifdef CONFIG_QH_WATER
int send_water_time(term_ram *ptrm, unsigned char *tm)
{
	int i;
	if (send_byte(0x20, ptrm->term_no) < 0) {
		return -1;
	}
	ptrm->dif_verify ^= 0x20;
	if (send_data(tm, 6, ptrm->term_no) < 0) {
		return -1;
	}
	for (i = 0; i < 6; i++) {
		ptrm->dif_verify ^= tm[i];
	}
	if (verify_all(ptrm, CHKXOR) < 0)
		return -1;
	return 0;
}
#endif

/*
 * �����˺ź�ʱ���·�
 */
int recv_le_id2(term_ram *ptrm, int allow, unsigned char *tm, int xor)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	unsigned long acc = 0;
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
#ifdef DEBUG
		//printk("find!\n");
#endif
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
		acc = pacc->acc_num;
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
			}
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ( (ptrm->pterm->power_id == 0xF0) ||
			((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1����ݻ��ʻ�������ն����Ͷ�Ӧ��������������
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
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
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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
	if (send_byte(0x20, ptrm->term_no) < 0) {
		return -1;
	}
	ptrm->dif_verify ^= 0x20;
	ptrm->add_verify += 0x20;
	if (send_data(tm, 6, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 6; i++) {
		//ptrm->add_verify += tm[i];
		ptrm->dif_verify ^= tm[i];
		ptrm->add_verify += tm[i];
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}
/*
 * receive le card flow, flag is 0 or 1; if flag is 0, consum is HEX; or is BCD
 * but store into flow any data is HEX
 */
int recv_leflow2(term_ram *ptrm, int flag, int xor)
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
	//leflow.consume_sum &= 0xFFFF;
	if (flag)		// if BCD then convert BIN
		leflow.consume_sum = bcdl2bin(leflow.consume_sum);
	// ����7�ֽ�ʱ��
	tmp = (unsigned char *)&leflow.date.hyear;
	//tmp += 6;
	for (i = 0; i < 7; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM) {
				// �˴�Ӧ���ӳٴ���
				usart_delay(7 - i);
			}
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}
	// third check NXOR
	if (xor) {
		if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
			printk("%d recv le flow2 verify chkxor error\n", ptrm->term_no);
#endif
			return -1;
		}
	} else {
		if (flag) {
			if (verify_all(ptrm, CHKXOR) < 0) {
#ifdef DEBUG
				printk("%d recv le flow2 verify chkxor error\n", ptrm->term_no);
#endif
				return -1;
			}
		} else {
			if (verify_all(ptrm, CHKNXOR) < 0) {
#ifdef DEBUG
				printk("%d recv le flow2 verify chkxor error\n", ptrm->term_no);
#endif
				return -1;
			}
		}
	}
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
		limit -= leflow.consume_sum / 100 + 1;
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
	g_tcnt++;
	ptrm->term_money += leflow.consume_sum;
	// init leflow
	leflow.flow_type = 0xAB;
	leflow.tml_num = ptrm->term_no;
	leflow.acc_num = pacc->acc_num;
	// ��ˮ�ŵĴ���
	//flow_no = maxflowno;
	leflow.flow_num = maxflowno++;
#ifdef CONFIG_CNTVER
	cntver_recvflow(&leflow, &cntcfg, pacc, ptrm->term_no, itm);
#endif
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


/*
 * ����ʱ�ε��ж�, flag = 1˵���ڴ�ʱ�����ж���
 */
int recv_le_id3(term_ram *ptrm, int allow, int itm)
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
#ifdef DEBUG
		//printk("find!\n");
#endif
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
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
			}
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ( (ptrm->pterm->power_id == 0xF0) ||
			((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1����ݻ��ʻ�������ն����Ͷ�Ӧ��������������
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
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
///////////////////////////////////////////////////////////////////////////////////
#ifdef USRCFG1
		// �жϴ��˻��Ƿ��������
		ret = is_cfg(&usrcfg, pacc, &money, itm, ptrm->term_no);
		if (ret < 0/* == -1 */) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
#endif
	}
lend:
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
	ptrm->add_verify += feature;
	ptrm->dif_verify ^= feature;
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
	if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
		printk("recv le id verify chkxor error %02x\n", feature);
#endif
		return -1;
	}
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}

#ifdef CONFIG_QH_WATER
int recv_le_id4(term_ram *ptrm, int allow, int xor, int itm)
{
	return 0;
}
#else
/*
 * �ն˻����������
 */
int recv_le_id4(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int imy = 0;		// �ն˻�������
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
	tmp++;
	// ����������
	for (i = 0; i < 2; i++) {
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
	tmoney = imy;
	_type = ptrm->pterm->power_id & 0x3;
#ifdef USRDBG
	printk("tmoney = %d, _type = %d\n", tmoney, _type);
#endif
	// search id
	pr_debug("begin pacc search id\n");
	pacc = search_id(cardno);
	pr_debug("end pacc search id %p\n", pacc);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("pacc %d not find... %p\n", cardno, pacc);
		feature = 1;
	} else {
		feature = 4;
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
			}
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
		// �жϴ��˻��Ƿ��������
		pr_debug("pacc %d find... %p\n", cardno, pacc);
		ret = is_cfg(&usrcfg, pacc, &money, itm, ptrm->term_no);
		if (ret < 0/* == -1 */) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		pr_debug("pacc %d iscfg over\n", cardno);
		if (!tmoney) {
			feature = 1;	// ��������
		}
#endif
		//acc = pacc->acc_num;
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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
	// �޸��´����
	if (pacc) {
		imy -= cal_money(imy, pacc, itm, 0);
		money += imy;
	}
	//cal_money(&money, pacc);
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
	ptrm->flow_flag = ~ptrm->term_no;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}
#endif

/*
 * �ն˻����������, jyb�汾
 */
int recv_le_jyb(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int imy = 0;		// �ն˻�������
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
	tmp++;
	// ����������
	for (i = 0; i < 2; i++) {
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
	tmoney = imy;
	_type = ptrm->pterm->power_id & 0x3;
#ifdef USRDBG
	printk("tmoney = %d, _type = %d\n", tmoney, _type);
#endif
	//printk("recv_le_jyb\n");
	// search id
	pr_debug("begin pacc search id\n");
	pacc = search_id(cardno);
	pr_debug("end pacc search id %p\n", pacc);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("pacc %d not find... %p\n", cardno, pacc);
		feature = 1;
	} else {
		feature = 4;
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
			}
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
		// �жϴ��˻��Ƿ��������
		pr_debug("pacc %d find... %p\n", cardno, pacc);
		ret = is_cfg(&usrcfg, pacc, &money, itm, ptrm->term_no);
		if (ret < 0/* == -1 */) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		pr_debug("pacc %d iscfg over\n", cardno);
		if (!tmoney) {
			feature = 1;	// ��������
		}
#endif
		//acc = pacc->acc_num;
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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
	// �޸��´����
	if (pacc) {
		// ��ʱ����_m
		int _m = imy;
		imy = cal_money(imy, pacc, itm, 0);
		_m -= imy;
		money += _m;
	}
	//cal_money(&money, pacc);
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
	// �·�ʵ�����ѽ��
	imy = binl2bcd(imy) & 0xFFFFFF;
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
	ptrm->flow_flag = ~ptrm->term_no;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}
static inline int recvflow_gdzj(struct usr_cfg *ucfg, acc_ram *pacc, int *ple, int itm, int termno)
{
	int ret;
	// �������ʱ������?
	unsigned char figure = ((pacc->feature & 0x30) >> 4);
	ret = is_special(ucfg->usrtm, pacc, itm, NULL, termno);
	if (ret == 0) {
		// ��������������˳���
		return 0;
	}
	// ��ʼ��Ǯ
	if (ucfg->usrtm[ret - 1].money[figure]) {
#ifdef USRDBG
		printk("�����������Ѳʹ�������Ǯ%d %d\n",
			ucfg->usrtm[ret - 1].money[figure], ((pacc->feature & 0x30) >> 4) + 1);
#endif
		//modified by duyy, 2013.5.29
		if (ucfg->usrtm[ret - 1].money[figure] == 0xFFFF){
			*ple = 0;
		} else {
			*ple = ucfg->usrtm[ret - 1].money[figure];
		}
		if (*ple < 0) {	// ����Ǹ�ֵ�������Ϊ����
			*ple = 0;
		}
#ifdef USRDBG
		printk("�����������Ѳʹ���ʵ��ˮ%d %d\n",
			*ple, ((pacc->feature & 0x30) >> 4) + 1);
#endif
		//pacc->money -= ucfg->usrtm->money[figure];
	} else {
#ifdef USRDBG
		printk("�����������Ѳʹ�������Ǯ%d %d\n",
			*ple, ((pacc->feature & 0x30) >> 4) + 1);
#endif
		//pacc->money -= ple->consume_sum;
	}
	return 0;
}

/*
 * �ն˻����������, ����ְܾ汾��write by duyy, 2014.1.7
 */
int recv_le_gdzj(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int imy = 0;		// �ն˻�������
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
	tmp++;
	// ����������
	for (i = 0; i < 2; i++) {
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
	tmoney = imy;
	_type = ptrm->pterm->power_id & 0x3;
#ifdef USRDBG
	printk("tmoney = %d, _type = %d\n", tmoney, _type);
#endif
	//printk("recv_le_gdzj\n");
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("pacc %d not find... %p\n", cardno, pacc);
		feature = 1;
	} else {
		feature = 4;
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
			}
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
		// �жϴ��˻��Ƿ��������
		pr_debug("pacc %d find... %p\n", cardno, pacc);
		ret = is_cfg(&usrcfg, pacc, NULL, itm, ptrm->term_no);
		if (ret < 0/* == -1 */) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		pr_debug("pacc %d iscfg over\n", cardno);
		if (!tmoney) {
			feature = 1;	// ��������
		}
#endif
		//acc = pacc->acc_num;
	//printk("recv_le_gdzj first consume money is %d\n", imy);
#ifdef USRCFG1
	// �������Ѷ�Ӧ���ǹ̶���, ��leflow->consume_sum
	recvflow_gdzj(&usrcfg, pacc, &imy, pacc->itm, ptrm->term_no);
	//printk("recv_le_gdzj second consume money is %d\n", imy);
#endif
	ret = is_special(&usrcfg.usrtm, pacc, pacc->itm, NULL, ptrm->term_no);
	if (ret == 0) {
		//printk("term no %d cannot disc\n", ptrm->term_no);
	} else {
		imy = cal_money(imy, pacc, itm, 0);
	}
	//printk("recv_le_gdzj third consume money is %d\n", imy);
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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

	//�·�����ǰ�˻����
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
	// �·�ʵ�����ѽ��
	imy = binl2bcd(imy) & 0xFFFFFF;
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
	ptrm->flow_flag = ~ptrm->term_no;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}


/*
 * ���ӷ����˺�, ����ר��
 */
int recv_le_id5(term_ram *ptrm, int allow, int itm)
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
	//printk("recv_le_id5 recieve cardno over\n");//2012.12.10
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
	} else {
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
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
			}
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ( (ptrm->pterm->power_id == 0xF0) ||
			((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1����ݻ��ʻ�������ն����Ͷ�Ӧ��������������
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
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
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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
	// send 4-byte acc id, little Endian
	{
		unsigned int acc = 0;
		if (pacc) {
			acc = pacc->acc_num;
		}
		tmp = (unsigned char *)&acc;
		for (i = 0; i < 4; i++, tmp++) {
			if (send_byte(*tmp, ptrm->term_no) < 0) {
				return -1;
			}
			ptrm->dif_verify ^= *tmp;
			ptrm->add_verify += *tmp;
		}
	}
	if (verify_all(ptrm, CHKALL) < 0) {
#ifdef DEBUG
		printk("recv le id verify chkxor error %02x\n", feature);
#endif
		return -1;
	}
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}

#ifdef CONFIG_RDFZ
// ��ȡ�����ˮ, �˴���
int recv_dep_money_rdfz(term_ram *ptrm, unsigned char *tm, int flag)
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
		usart_delay(14);
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
				usart_delay(14 - i);
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
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			ptrm->status = ret;
			if (ret == NOCOM)
				usart_delay(10 - i);
			return ret;
		}
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}

	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
				usart_delay(8 - i);
			}
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
	leflow.card_num = _card(leflow.card_num);
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
	managefee = fee[pacc->managefee & 0xF];		//����ѱ���
	managefee = managefee * leflow.consume_sum / 100;	//��������
	//printk("pacc->money: %d, managefee: %d\n", pacc->money, managefee);
	pacc->money += leflow.consume_sum - managefee;		//����Ǯ���ܷ�
	//printk("pacc->money: %d\n", pacc->money);
	ptrm->term_money += leflow.consume_sum;
	ptrm->term_cnt++;
	ptrm->flow_flag = ptrm->term_no;	// ��ֹ�ٴν�����ˮ
	// �����洢��ˮ
#ifdef CONFIG_REFUND
	if (pacc->money == (leflow.consume_sum - managefee)) {
		leflow.flow_type = LEFCHARGE;
	} else {
#endif
		leflow.flow_type = 0xAC;
#ifdef CONFIG_REFUND
	}
#endif
	leflow.acc_num = pacc->acc_num;
#if 0
	leflow.date.hyear = 0x20;
	tmp = &leflow.date.lyear;
	for (i = 0; i < 6; i++) {
		*tmp = *tm;
		tmp--;
		tm++;
	}
#endif
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
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˽���ȡ����ˮ, ����״̬
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
		// �����ն˽��
		cashbuf[cashterm_ptr].term_money = ptrm->term_money;
		cashterm_ptr++;
	}
#endif
	return 0;
}

// ����˫У���־
int recv_take_money_rdfz(term_ram *ptrm, unsigned char *tm, int flag)
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

	tmp = &leflow.date.hyear;
	for (i = 0; i < 7; i++) {
		ret = recv_data((char *)tmp, ptrm->term_no);
		if (ret < 0) {
			if (ret == -1) {
				ptrm->status = NOTERMINAL;
			}
			if (ret == -2) {		// other errors
				ptrm->status = NOCOM;
				usart_delay(8 - i);
			}
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
	leflow.card_num = _card(leflow.card_num);
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
	leflow.flow_type = 0xAD;
	leflow.acc_num = pacc->acc_num;
	leflow.date.hyear = 0x20;
#if 0
	tmp = &leflow.date.lyear;
	for (i = 0; i < 6; i++) {
		*tmp = *tm;
		tmp--;
		tm++;
	}
#endif
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
	}
#endif
	return 0;
}

/*
 * �����˺ź�ʱ���·�
 */
int recv_le_id_rdfz(term_ram *ptrm, int allow, unsigned char *tm, int xor)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	unsigned long acc = 0;
	char name[10] = {0};
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
		//printk("not find %x\n", cardno);
		feature = 1;
	} else {
#ifdef DEBUG
		//printk("find!\n");
#endif
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
		acc = pacc->acc_num;
		memcpy(name, pacc->user_name, sizeof(name));
		if (ptrm->pterm->term_type & 0x20) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
				//printk("ACCDEPBIT not allow: %d\n", cardno);
			}
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		if ( (ptrm->pterm->power_id == 0xF0) ||
			((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1����ݻ��ʻ�������ն����Ͷ�Ӧ��������������
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
			if (pacc->money < 0) {
				money = 0;
			} else {
				money = (int)pacc->money;// & 0xFFFFF;
			}
		} else {
			feature = 1;
		}
		if (money > 999999) {
			money = 999999;
		}
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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
	if (send_byte(0x20, ptrm->term_no) < 0) {
		return -1;
	}
	ptrm->dif_verify ^= 0x20;
	ptrm->add_verify += 0x20;
	if (send_data(tm, 6, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 6; i++) {
		//ptrm->add_verify += tm[i];
		ptrm->dif_verify ^= tm[i];
		ptrm->add_verify += tm[i];
	}
	// ����10�ֽ�����
	if (send_data(name, 10, ptrm->term_no) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	for (i = 0; i < 10; i++) {
		ptrm->dif_verify ^= name[i];
		ptrm->add_verify += name[i];
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}
#endif

#ifdef CONFIG_SUBCNT
/*
 * ����ʱ�ε��ж�, flag = 1˵���ڴ�ʱ�����ж���
 */
int recv_le_id_sbc(term_ram *ptrm, int allow, int itm)
{
	unsigned long cardno;
	acc_ram *pacc = NULL;
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
	cardno = _card(cardno);
	if (itm > 14400 && itm < 36000) {
		feature = 1;
		goto lend;
	}
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		feature = 1;
	} else {
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
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			feature = 1;
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////

#if 0
		if ( (ptrm->pterm->power_id == 0xF0) ||
			((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1����ݻ��ʻ�������ն����Ͷ�Ӧ��������������
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
			if (pacc->money < 0) {
				money = 0;
			} else {
				money = (int)pacc->money;// & 0xFFFFF;
			}
		} else {
			feature = 1;
		}
#endif
		money = pacc->sub_cnt * 100;
		if (money > 999999) {
#ifdef DEBUG
			printk("money is too large, max 999999!\n");
#endif
			money = 999999;
		}
		if (pacc->sub_flag) {
			feature |= (1 << 6);
		}
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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
	if (verify_all(ptrm, CHKXOR) < 0) {
#ifdef DEBUG
		printk("recv le id verify chkxor error %02x\n", feature);
#endif
		return -1;
	}
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	//printk("qinghua water test: recv le id ok\n");
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}
#endif

#ifdef CONFIG_ZLYY
/*
 * �ն˻����������
 */
int recv_leid_zlyy(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int imy = 0;		// �ն˻�������
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
	// ����������
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
	tmoney = imy;
	_type = ptrm->pterm->power_id & 0x3;
#ifdef USRDBG
	printk("tmoney = %d, _type = %d\n", tmoney, _type);
#endif
	// search id
	//pr_debug("begin pacc search id\n");
	pacc = search_id(cardno);
	//pr_debug("end pacc search id %p\n", pacc);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("pacc %d not find... %p\n", cardno, pacc);
		feature = 1;
	} else {
		feature = 4;
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			feature = 1;	// ��֧�ֳ���
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
		// �жϴ��˻��Ƿ��������
		pr_debug("pacc %d find... %p\n", cardno, pacc);
		ret = is_cfg(&usrcfg, pacc, NULL, itm, ptrm->term_no);
		if (ret == -1) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == -2) {
			feature |= 1 << 4;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		pr_debug("pacc %d iscfg over\n", cardno);
		//������Ϣ���޸���2012.2.15
	//	if (!tmoney) {
	//		feature = 1;	// ��������
	//	}
#endif
		//acc = pacc->acc_num;
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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
	// 3�ֽ�ʵ�����ѽ��HEX
	imy = cal_money(imy, pacc, itm, 0);
	tmp = (unsigned char *)&imy;
	tmp += 2;
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		ptrm->add_verify += *tmp;
		tmp--;
	}

	// 3�ֽ��������HEX
	if (pacc) {
		imy = pacc->m_alarm;
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	leflow.card_num = _card(leflow.card_num);
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
#ifdef USRCFG1
		_cal_limit(&limit, leflow.consume_sum);
#else
		limit -= leflow.consume_sum / 100 + 1;
		if (!(leflow.consume_sum % 100)) {// �պ���100ʱ, ��Ϊ��һ��
			limit++;
		}
		if (limit < 0)
			limit = 0;
#endif
	}

#ifdef USRCFG1
	// �������Ѷ�Ӧ���ǹ̶���, ��leflow->consume_sum
	recvflow(&usrcfg, pacc, &leflow, pacc->itm, ptrm->term_no);
#endif

	pacc->money -= leflow.consume_sum;
#if 0
	if (pacc->money < 0) {
		printk("account money below zero!!!\n");
		pacc->money = 0;
	}
#endif
	pacc->feature &= 0xF0;
	pacc->feature |= limit;
	ptrm->term_cnt++;
	g_tcnt++;
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

int send_time_zlyy(term_ram *ptrm, unsigned char *tm, int itm)
{
	unsigned char seg = 0;
	int i;
	struct usr_tmsg *usrtm = usrcfg.usrtm;
	while (usrtm->no) {
		if (itm > usrtm->begin && itm < usrtm->end) {
			seg = usrtm->no;
			break;
		}
		usrtm++;
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
 * �ն˻����������, Ҫ��server�������
 */
int recv_leid_zlyy2(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int imy = 0;		// �ն˻�������
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
	// ����������
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
	tmoney = imy;
	_type = ptrm->pterm->power_id & 0x3;
#ifdef USRDBG
	printk("tmoney = %d, _type = %d\n", tmoney, _type);
#endif
	// search id
	//pr_debug("begin pacc search id\n");
	pacc = search_id(cardno);
	//pr_debug("end pacc search id %p\n", pacc);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("pacc %d not find... %p\n", cardno, pacc);
		feature = 1;
	} else {
		feature = 4;
		//printk("recv_leid_zlyy2 pacc->feature=%x\n", pacc->feature);//2012.3.21
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
			//printk("password2\n");//2012.3.21
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			feature = 1;	// ��֧�ֳ���
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ( (ptrm->pterm->power_id == 0xF0) || ((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))) {
			money = (int)pacc->money;// & 0xFFFFF;
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
		// �жϴ��˻��Ƿ��������
		pr_debug("pacc %d find... %p\n", cardno, pacc);
		ret = is_cfg(&usrcfg, pacc, NULL, itm, ptrm->term_no);
		if (ret == -1) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == -2) {
			feature |= 1 << 4;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		pr_debug("pacc %d iscfg over\n", cardno);
		//������Ϣ���޸���2012.2.15
	//	if (!tmoney) {
	//		feature = 1;	// ��������
	//	}
#endif
		//acc = pacc->acc_num;
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
		feature = 1;
		money = 0;
	}

	// imy ʵ�����ѽ��
	if (pacc) {
		imy = cal_money(imy, pacc, itm, 0);
		money -= imy;
		pr_debug("money = %d mdraw = %d m_alarm = %d\n",
			money, pacc->m_draw, pacc->m_alarm);
		if (money < (-pacc->m_draw)) {
			pr_debug("warning: money < - mdraw, %d, %d\n",
				money, pacc->m_draw);
			feature |= (1 << 7);
			money += imy;
		}
		if (money < pacc->m_alarm) {
			feature |= (1 << 3);
			pr_debug("warning: money < m_alarm, %d, %d\n",
				money, pacc->m_alarm);
		}
		if (money < 0) {
			feature &= ~(1 << 6);
			money = -money;
		} else {
			feature |= (1 << 6);
		}
	}
	//printk("send feture2=%x\n", feature);//2012.3.21
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;

	// �˴��������Ѻ����
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

	// 3�ֽ�ʵ�����ѽ��HEX
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}

/*
 * �ն˻����������, Ҫ��server�������,����ͨ����Ҳ֧��͸֧
 */
int recv_leid_zlyy3(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
	unsigned char *tmp = (unsigned char *)&cardno;
	int imy = 0;		// �ն˻�������
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
	// ����������
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
	tmoney = imy;
	_type = ptrm->pterm->power_id & 0x3;
#ifdef USRDBG
	printk("tmoney = %d, _type = %d\n", tmoney, _type);
#endif
	// search id
	//pr_debug("begin pacc search id\n");
	pacc = search_id(cardno);
	//pr_debug("end pacc search id %p\n", pacc);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("pacc %d not find... %p\n", cardno, pacc);
		feature = 1;
	} else {
		feature = 4;
		//printk("recv_leid_zlyy3 pacc->feature=%x\n", pacc->feature);//2012.3.21
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40 || (pacc->feature & 0xF) == 0) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
			//printk("password3\n");//2012.3.21
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			feature = 1;	// ��֧�ֳ���
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
		if ( (ptrm->pterm->power_id == 0xF0) || ((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))) {
			money = (int)pacc->money;// & 0xFFFFF;
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
		// �жϴ��˻��Ƿ��������
		pr_debug("pacc %d find... %p\n", cardno, pacc);
		ret = is_cfg(&usrcfg, pacc, NULL, itm, ptrm->term_no);
		if (ret == -1) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == -2) {
			feature |= 1 << 4;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		pr_debug("pacc %d iscfg over\n", cardno);
		//������Ϣ���޸���2012.2.15
	//	if (!tmoney) {
	//		feature = 1;	// ��������
	//	}
#endif
		//acc = pacc->acc_num;
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
		feature = 1;
		money = 0;
	}

	// imy ʵ�����ѽ��
	if (pacc) {
		money -= imy;
		pr_debug("money = %d mdraw = %d m_alarm = %d\n",
			money, pacc->m_draw, pacc->m_alarm);
		if (money < (-pacc->m_draw)) {
			pr_debug("warning: money < - mdraw, %d, %d\n",
				money, pacc->m_draw);
			feature |= (1 << 7);
			money += imy;
		}
		if (money < pacc->m_alarm) {
			feature |= (1 << 3);
			pr_debug("warning: money < m_alarm, %d, %d\n",
				money, pacc->m_alarm);
		}
		if (money < 0) {
			feature &= ~(1 << 6);
			money = -money;
		} else {
			feature |= (1 << 6);
		}
	}
	//printk("send feture3=%x\n", feature);//2012.3.21
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	ptrm->add_verify += feature;

	// �˴��������Ѻ����
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

	// 3�ֽ�ʵ�����ѽ��HEX
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}

/*
 * �ն˻����������, ���ɻ�
 */
int recv_leid_zlyy_cash(term_ram *ptrm, int allow, int xor, int itm)
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
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

#ifdef USRDBG
	printk("tmoney = %d, _type = %d\n", tmoney, _type);
#endif
	// search id
	//pr_debug("begin pacc search id\n");
	pacc = search_id(cardno);
	//pr_debug("end pacc search id %p\n", pacc);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		pr_debug("pacc %d not find... %p\n", cardno, pacc);
		feature = 1;
	} else {
		feature = 4;
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if (!(ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			feature = 1;
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
		money = (int)pacc->money;// & 0xFFFFF;
#if 0
		// �жϴ��˻��Ƿ��������
		pr_debug("pacc %d find... %p\n", cardno, pacc);
		ret = is_cfg(&usrcfg, pacc, NULL, itm, ptrm->term_no);
		if (ret == -1) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
		pr_debug("pacc %d iscfg over\n", cardno);
		if (!tmoney) {
			feature = 1;	// ��������
		}
#endif
		//acc = pacc->acc_num;
///////////////////////////////////////////////////////////////////////////////////
	}
lend:
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
	
	// �˴��������Ѻ����
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
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}

#endif	/* CONFIG_ZLYY */

#ifdef CONFIG_BD//added by duyy for BeiDa, 2012.12.20
/*
 * ����淢�Ϳ��ź�2�ֽ����ѽ��
 */
int recv_le_bd(term_ram *ptrm, int allow
#if defined (S_BID) || defined (USRCFG1) || defined (CONFIG_CNTVER)
, int itm
#endif
#ifdef EDUREQ
, int flag)
#else
)
#endif
{
	unsigned long cardno;
	acc_ram *pacc;
	unsigned char feature = 0;
	int money = 0;
#ifdef CONFIG_QH_WATER
	int sub_money = 0;
	char power_id = 0;
#endif

	int imy = 0;		// �ն˻�������
	int conmoney;

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
	//printk("recv_le_bd recieve cardno over\n");//2012.12.10
	tmp = (unsigned char *)&imy;
	tmp += 1;
	// ����2bytes������
	for (i = 0; i < 2; i++) {
		ret = recv_data(tmp, ptrm->term_no);
		//printk("recv_le_bd term money[%d] is %x, ret is %d\n", i, *tmp, ret);
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
	//printk("imy_BCD is %x\n", imy);
	imy = BCD2BIN((imy & 0xFF00) >> 8) * 100 + BCD2BIN((imy & 0x00FF) >> 0);
	//printk("imy is %d\n", imy);
	cardno = _card(cardno);
	// search id
	pacc = search_id(cardno);
	// if exist, then deal with data
	if (pacc == NULL) {
		// not find
		//printk("cardno %d not find\n", cardno);
		feature = 1;
	} else {
#ifdef DEBUG
		//printk("find!\n");
#endif
		feature = 4;
		//printk("recv_le_bd pacc->feature=%x\n", pacc->feature);//2012.3.21
		//if (ptrm->pterm->term_type & 0x20) {
		//	feature = 0;
		//} else {
		//	feature = 4;
		//}
		if ((pacc->feature & 0xC0) == 0x80) {
			// ���ǹ�ʧ��
			feature |= 1 << 5;
		}
		if ((pacc->feature & 0xC0) == 0x40){
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
			//printk("recv_le_id password\n");//2012.3.21
		}
		if ((ptrm->pterm->term_type & 0x20)) {	// �϶��Ƿ�Ϊ���ɻ�
			// ���ɻ�
			money = (int)pacc->money;// & 0xFFFFF;
			if (money < 0) {		// below zero, fixed by wjzhe, 20091023
				money = 0;
			} else if (money > 999999) {
				money = 999999;
			}
			// ���ӽ�ֹ���λ
			pr_debug("pacc: %d managefee %x\n", pacc->acc_num, pacc->managefee);
			if (pacc->managefee & ACCDEPBIT) {
				feature = 1;
			}
			goto lend;
		}
///////////////////////////////////////////////////////////////////////////////////
#ifndef WATER
		if ( (ptrm->pterm->power_id == 0xF0) ||
			((pacc->feature & 0x30) == 0) ||
			((ptrm->pterm->power_id & 0x0F) == ((pacc->feature & 0x30) >> 4))
			) {
			//money[0] = pacc->money & 0xFF;// ���ն�Ϊ1���ն˻��ʻ�Ϊ1����ݻ��ʻ�������ն����Ͷ�Ӧ��������������
			//money[1] = (pacc->money >> 8) & 0xFF;
			//money[2] = (pacc->money >> 16) & 0xF;
			if (pacc->money < 0) {
				money = 0;
			} else {
				money = (int)pacc->money;// & 0xFFFFF;
			}
		} else {
			//printk("feature wrong\n");
			feature = 1;
		}
#else
#ifdef CONFIG_QH_WATER
		if (ptrm->is_limit & (1 << pacc->power_id)) {
			feature |= 1 << 6;
		}
		if (!(ptrm->is_use & (1 << pacc->power_id))) {
#ifdef DEBUG
			printk("is_use %02x, powerid=%d\n", ptrm->is_use, pacc->power_id);
#endif
			feature = 1;
		}
		// �����˻����
		sub_money = (int)pacc->sub_money;
		if (sub_money < 0) {
			sub_money = 0;
		}
		power_id = (char)pacc->power_id;
#ifdef DEBUG
		printk("$>>>>sub money: %d\n", sub_money);
#endif
#endif
		// ˮ�ؾͲ��ж��ն�������
		if (pacc->money < 0) {
			money = 0;
		} else {
			money = (int)pacc->money;// & 0xFFFFF;
		}
#endif
		if (money > 999999) {
#ifdef DEBUG
			printk("money is too large, max 999999!\n");
#endif
			money = 999999;
		}

///////////////////////////////////////////////////////////////////////////////////

#ifdef USRCFG1
		// �жϴ��˻��Ƿ��������
		ret = is_cfg(&usrcfg, pacc, &money, itm, ptrm->term_no);
		if (ret < 0/* == -1 */) {
#ifdef USRDBG
			printk("%d %d %08x����������\n", itm, 
				((pacc->feature & 0x30) >> 4) + 1, pacc->card_num);
#endif
			feature = 1;
			money = 0;
		} else if (ret == 255) {
			// �˿��ﵽ����
			// �˿�Ҫ������
			feature |= 1 << 3;
		}
#endif

		printk("consumelimit is %x\n", pacc->consumelimit);//2013.5.14
		conmoney = pacc->consumelife + imy;
		if ((((pacc->consumelimit & 0xFF00) >> 8) != 0xFF) && (conmoney >= ((pacc->consumelimit & 0xFF00) >> 8) * 100)){
			
				feature = 1;
				//printk("not permit consume, conmoney is %d\n", conmoney);
			
		} 
		if (((pacc->consumelimit & 0x00FF) != 0xFF) && (conmoney >= (pacc->consumelimit & 0x00FF) * 100)){
			feature |= 1 << 3;
			//printk("password, conmoney is %d\n", conmoney);
		}		
	} 

lend:
	// �ж��Ƿ������ѻ�ʹ�ù�翨
	if (!allow) {
#ifdef DEBUG
		printk("not allow: %d\n", cardno);
#endif
		feature = 1;
		money = 0;
	}
	//printk("send recv_le_id feture=%x\n", feature);//2012.3.21
	if ((ret = send_byte(feature, ptrm->term_no)) < 0) {
		ptrm->status = NOCOM;
		return -1;
	}
	ptrm->dif_verify ^= feature;
	//printk("feature is %02x\n", feature);
#ifdef CONFIG_QH_WATER
	if (ptrm->pterm->term_type & 0x20){		// ���ɻ�
#endif
	money = binl2bcd(money) & 0xFFFFFF;
#ifdef CONFIG_QH_WATER
	}
#endif
	tmp = (unsigned char *)&money;
	tmp += 2;
	for (i = 0; i < 3; i++) {
		if (send_byte(*tmp, ptrm->term_no) < 0)
			return -1;
		ptrm->dif_verify ^= *tmp;
		tmp--;
	}
	//printk("recv_le_id send money is %d\n", money);//2012.5.7
#ifdef CONFIG_QH_WATER
	if (!(ptrm->pterm->term_type & 0x20)) {	// ֻ�����ѻ����в���
		tmp = (unsigned char *)&sub_money;
		tmp += 1;
		for (i = 0; i < 2; i++) {
			if (send_byte(*tmp, ptrm->term_no) < 0) {
				return -1;
			}
			ptrm->dif_verify ^= *tmp;
			tmp--;
		}
		// ����Ҫ������ݱ�־0~7
		if (send_byte(power_id, ptrm->term_no) < 0) {
			return -1;
		}
		ptrm->dif_verify ^= power_id;
	}
#endif
	if (verify_all(ptrm, CHKXOR) < 0) {
#ifdef DEBUG
		printk("recv le bd verify chkxor error %02x\n", feature);
#endif
		return -1;
	}
	ptrm->flow_flag = 0;		// �����ն˽�����ˮ
	//printk("qinghua water test: recv le id ok\n");
	// if not exist then send 1 and remain money 0
	// next check dis_verify, send 0 if right, send 0xF if wrong
#ifdef CONFIG_RECORD_CASHTERM
	// �ն˿��Խ�����ˮ��, ����״̬
	if ((ptrm->pterm->term_type & 0x20)
		&& (cashterm_ptr < CASHBUFSZ)) {
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
	}
#endif
	return 0;
}

#endif

