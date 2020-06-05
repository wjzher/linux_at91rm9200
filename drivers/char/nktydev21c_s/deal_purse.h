/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�deal_purse.h
 * ժ    Ҫ�����崦����POS�����ֺ����õ��ĺ���
 * 			 
 * ��ǰ�汾��1.0
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��6��14��
 *
 * ȡ���汾��
 * ԭ����  ��
 * ������ڣ�
 */
#ifndef _DEAL_PURSE_H
#define _DEAL_PURSE_H

#include "uart_dev.h"
#include "data_arch.h"
#include "uart.h"
#include "deal_data.h"
#include "uart_dev.h"

#define PURSE_REQ_PAR 0x11
#define PURSE_RET_PAR 0x12
#if 0
#define PURSE_REQ_TMSG 0x13
#define PURSE_RET_TMSG 0x14
#endif
#define PURSE_REQ_TIME 0x15
#define PURSE_RET_TIME 0x16
#define PURSE_RMD_BKIN 0x21
#define RECVPURSEFLOW 0x22

#define PURSE_PUT_BLK 0x31
#define PURSE_PUT_ABLK 0x32
#define PURSE_RET_ABLK 0x33
#define PURSE_UPDATE_KEY 0x34
#define PURSE_BCT_ERASE 0x36
#define PURSE_BCT_BLK 0x37

#define PURSE_GET_TNO 0x47
#define PURSE_RET_TNO 0x48

#define PURSE_RECV_CARDNO 0x51	// �յ���翨���Ϳ�������
#define PURSE_SEND_CARDNO 0x52	// ���ع�翨���Ϳ�������
#define PURSE_RECV_LEFLOW 0x53	// ���չ�翨��ˮ����
#define PURSE_RECV_DEP	  0x54	// ���չ�翨�����ˮ����
#define PURSE_RECV_TAKE	  0x55	// ���չ�翨ȡ����ˮ����

extern AT91PS_USART uart_ctl0;
extern AT91PS_USART uart_ctl2;

#define BWT_66J10 80
#define BWR_66J10 700

extern int compute_blk(term_ram *ptrm, struct black_info *blkinfo);
extern int purse_send_conf(term_ram *ptrm, unsigned char *tm, struct black_info *blkinfo);
extern int purse_send_time(term_ram *ptrm, unsigned char *tm);
extern int purse_recv_btno(term_ram *ptrm);
extern int purse_recv_flow(term_ram *ptrm);
extern int purse_send_bkin(term_ram *ptrm, struct black_info *blkinfo, black_acc *blkacc, int flag);
extern int purse_broadcast_blk(black_acc *pbacc, struct black_info *pbinfo, term_ram *ptrm, int cnt);
extern int purse_update_key(term_ram *ptrm);
extern int purse_get_edition(term_ram *ptrm);
extern int purse_recv_leid(term_ram *ptrm, int allow);
extern int purse_recv_leflow(term_ram *ptrm, unsigned char *tm);
extern int purse_recv_letake(term_ram *ptrm, unsigned char *tm);
extern int purse_recv_ledep(term_ram *ptrm, unsigned char *tm);

#if 0
extern psam_local *search_psam(unsigned short num, struct psam_info *psinfo);
#endif
#endif
