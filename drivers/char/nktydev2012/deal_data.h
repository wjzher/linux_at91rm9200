#ifndef DEAL_DATA_H
#define DEAL_DATA_H

#include "data_arch.h"
#include "no_money_type.h"
#include "hashtab.h"
#ifndef BCD2BIN
#define BCD2BIN(val) (((val)&15) + ((val)>>4)*10)
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

unsigned long binl2bcd(unsigned long val);
void usart_delay(int n);
acc_ram *search_no(unsigned int cardno, acc_ram *pacc, int cnt);
acc_ram *search_no2(const unsigned int cardno, acc_ram *pacc, int cnt);
acc_ram *search_id(const unsigned int cardno);

int send_run_data(term_ram *ptrm);
int recv_leid_single(term_ram *ptrm, int allow, int itm);
int recv_leid_double(term_ram *ptrm, int allow, int itm);
int recv_leid_cash(term_ram *ptrm, int allow, int itm);
int recv_leflow_single(term_ram *ptrm, unsigned char *tm);
int recv_leflow_double(term_ram *ptrm, unsigned char *tm);
int ret_no_only(term_ram *ptrm);
int verify_all(term_ram *ptrm, unsigned char n);
int recv_dep_money(term_ram *ptrm, unsigned char *tm);
int recv_take_money(term_ram *ptrm, unsigned char *tm);

int recv_take_money_v1(term_ram *ptrm, unsigned char *tm, int flag);
int recv_dep_money_v1(term_ram *ptrm, unsigned char *tm, int flag);
int recv_le_id(term_ram *ptrm, int allow, int itm);
int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm);

int is_term_allow(u8 acc_pid, u8 term_pid);

extern unsigned long bcdl2bin(unsigned long bcd);
//extern int recv_byte(char *buf, unsigned char num);

int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m, 
	struct hashtab *hsw, struct record_info *prcd);
extern int deal_money(money_flow *pnflow/*, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd*/);
extern int sort_sw(acc_ram *pacc_s, acc_ram *pacc, int cnt);
extern int nomtoblk(black_acc *pbacc, no_money_flow *pnflow);
extern int ctoutm(usr_time *putm, char *tm);
int new_reg(acc_ram *acc);

#ifdef CONFIG_UART_V2
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
int recv_leid_zlyy(term_ram *ptrm, int allow, int xor, int itm);
int send_time_zlyy(term_ram *ptrm, unsigned char *tm, int itm);
int recv_leflow_zlyy(term_ram *ptrm, unsigned char *tm);
int recv_leid_zlyy_cash(term_ram *ptrm, int allow, int xor, int itm);
int recv_leid_zlyy2(term_ram *ptrm, int allow, int xor, int itm);
#endif

extern struct hashtab *hash_accsw;

#endif

