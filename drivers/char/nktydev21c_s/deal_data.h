#ifndef DEAL_DATA_H
#define DEAL_DATA_H

#include "data_arch.h"
#include "no_money_type.h"

extern unsigned long binl2bcd(unsigned long val);
extern void usart_delay(int n);
extern acc_ram *search_no(unsigned long cardno, acc_ram *pacc, int cnt);
extern acc_ram *search_no2(const unsigned long cardno, acc_ram *pacc, int cnt);
extern acc_ram *search_id(const unsigned long cardno);
#ifdef TS11
extern int send_run_data(term_ram *ptrm);
extern int recv_le_id(term_ram *ptrm, int allow);
extern int ret_no_only(term_ram *ptrm);
extern int verify_all(term_ram *ptrm, unsigned char n);
extern int recv_leflow(term_ram *ptrm, int flag, unsigned char *tm);
extern int recv_take_money(term_ram *ptrm, unsigned char *tm);
extern int recv_dep_money(term_ram *ptrm, unsigned char *tm);
#endif

#ifdef CPUCARD
extern int recv_cpu_id(term_ram *ptrm, unsigned char *tm);
extern int recv_cpuflow(term_ram *ptrm);
#endif
extern unsigned long bcdl2bin(unsigned long bcd);
//extern int recv_byte(char *buf, unsigned char num);

extern int deal_no_money(no_money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd);
extern int deal_money(money_flow *pnflow, acc_ram *pacc_m, acc_ram *pacc_s, struct record_info *prcd);
extern int sort_sw(acc_ram *pacc_s, acc_ram *pacc, int cnt);
extern int nomtoblk(black_acc *pbacc, no_money_flow *pnflow);
extern int ctoutm(usr_time *putm, char *tm);

#endif

