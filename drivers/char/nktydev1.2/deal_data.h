#ifndef DEAL_DATA_H
#define DEAL_DATA_H

#include "data_arch.h"
#include "hashtab.h"

#ifndef BCD2BIN
#define BCD2BIN(val) (((val)&15) + ((val)>>4)*10)
#endif

// 计算一天中的时间，输入6字节时间
static inline int _cal_itmofday(unsigned char *tm)
{
	int itm;
	// 将读到的时间换算为整数
	itm = BCD2BIN(tm[5]);
	itm += BCD2BIN(tm[4]) * 60;
	itm += BCD2BIN(tm[3]) * 3600;
	return itm;
}

extern unsigned long binl2bcd(unsigned long val);
extern void usart_delay(int n);
#ifdef CONFIG_QH_WATER
int send_water_time(term_ram *ptrm, unsigned char *tm);
extern int send_run_data(term_ram *ptrm/*, unsigned char *tm*/);
#else
extern int send_run_data(term_ram *ptrm);
#endif
extern acc_ram *search_no(unsigned long cardno, acc_ram *pacc, int cnt);
extern acc_ram *search_no2(const unsigned long cardno, acc_ram *pacc, int cnt);
extern acc_ram *search_id(const unsigned long cardno);
#if defined EDUREQ
extern int recv_le_id(term_ram *ptrm, int allow, int flag);
#elif defined (S_BID) || defined (USRCFG1) || defined (CONFIG_CNTVER)
extern int recv_le_id(term_ram *ptrm, int allow, int itm);
#else
extern int recv_le_id(term_ram *ptrm, int allow);
#endif/* S_BID */
#ifdef CONFIG_CNTVER
int deal_cnt_flow(struct cntflow *pcf, int n);
#endif
extern int ret_no_only(term_ram *ptrm);
extern int verify_all(term_ram *ptrm, unsigned char n);

/*
#ifdef EDUREQ
extern int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm, int tflag);
#elif defined CONFIG_CNTVER
extern int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm, int itm);
#else
extern int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm);
#endif
*/
int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm
#ifdef CONFIG_CNTVER
, int itm
#elif defined EDUREQ
, int tflag
#else
, int refund
#endif
);

#ifdef CPUCARD
extern int recv_cpu_id(term_ram *ptrm, unsigned char *tm);
extern int recv_cpuflow(term_ram *ptrm);
#endif
extern unsigned long bcdl2bin(unsigned long bcd);
extern int recv_byte(char *buf, unsigned char num);
extern int recv_take_money(term_ram *ptrm, unsigned char *tm, int flag);
//extern int recv_dep_money(term_ram *ptrm, unsigned char *tm);
int recv_dep_money(term_ram *ptrm, unsigned char *tm, int flag);

#ifndef CONFIG_ACCSW_USE_HASH
extern int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd);
extern int deal_money(money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd);
extern int sort_sw(acc_ram *pacc_s, acc_ram *pacc, int cnt);
#else
extern int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m, struct hashtab *hsw, struct record_info *prcd);
extern int deal_money(money_flow *pnflow, acc_ram *pacc_m, struct hashtab *hsw, struct record_info *prcd);
#endif
#ifdef WATER
extern int recv_water_order(term_ram *ptrm);
extern int recv_water_single_flow(term_ram *ptrm, unsigned char *tm);
extern int record_water_flow(term_ram *ptrm);
#endif
int recv_le_id2(term_ram *ptrm, int allow, unsigned char *tm, int xor);
int recv_leflow2(term_ram *ptrm, int flag, int xor);

//#if defined (S_BID) || defined (USRCFG1) || defined (CONFIG_CNTVER)
extern int recv_le_id3(term_ram *ptrm, int allow, int itm);
//#endif
int send_run_data2(term_ram *ptrm, unsigned char *tm);

int recv_le_id4(term_ram *ptrm, int allow, int xor, int itm);
/*
 * 增加发送账号, 北大专用
 */
int recv_le_id5(term_ram *ptrm, int allow, int itm);

#ifdef CONFIG_RDFZ
int recv_dep_money_rdfz(term_ram *ptrm, unsigned char *tm, int flag);
int recv_take_money_rdfz(term_ram *ptrm, unsigned char *tm, int flag);
int recv_le_id_rdfz(term_ram *ptrm, int allow, unsigned char *tm, int xor);
#endif

#endif

