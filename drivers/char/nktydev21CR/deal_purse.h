/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：deal_purse.h
 * 摘    要：定义处理新POS命令字和所用到的函数
 * 			 
 * 当前版本：1.0
 * 作    者：wjzhe
 * 完成日期：2007年6月14日
 *
 * 取代版本：
 * 原作者  ：
 * 完成日期：
 */
#ifndef _DEAL_PURSE_H
#define _DEAL_PURSE_H

#include "uart_dev.h"
#include "data_arch.h"
#include "uart.h"
#include "deal_data.h"
#include "uart_dev.h"

//485叫号与终端交互命令
#define PURSE_REQ_PAR 0x11
#define PURSE_RET_PAR 0x12
#if 0
#define PURSE_REQ_TMSG 0x13
#define PURSE_RET_TMSG 0x14
#endif
#define PURSE_REQ_TIME 0x15
#define PURSE_RET_TIME 0x16
#define PURSE_REQ_NCTM 0x17//请求下载禁止时段信息，write by duyy, 2013.5.20
#define PURSE_RET_NCTM 0x18//write by duyy, 2013.5.20
#define PURSE_REQ_MAGFEE 0x1C//请求下载管理费信息，write by duyy, 2013.11.19
#define PURSE_RET_MAGFEE 0x1D//请求下载管理费信息，write by duyy, 2013.11.19
#define PURSE_REQ_FORBIDTABLE 0x1E//请求下载禁止消费时段启用标志表单信息，write by duyy, 2013.11.19
#define PURSE_RET_FORBIDTABLE 0x1F//请求下载禁止消费时段启用标志表单信息，write by duyy, 2013.11.19
#define PURSE_RMD_BKIN 0x21
#define RECVPURSEFLOW 0x22
#define PURSE_PUT_BLK 0x31
#define PURSE_PUT_ABLK 0x32
#define PURSE_RET_ABLK 0x33
#define PURSE_UPDATE_KEY 0x34
#define PURSE_BCT_ERASE 0x36
#define PURSE_BCT_BLK 0x37
#define PURSE_UPDATE_MAGFEE 0x3B //更新管理费信息，write by duyy, 2013.11.19
#define PURSE_UPDATE_FORBIDTABLE 0x3C //更新禁止消费时段启用标志表单信息，write by duyy, 2013.11.19
#define PURSE_UPDATE_FORBIDSEG 0x3D //更新禁止消费时信息，write by duyy, 2013.11.22

#define PURSE_GET_TNO 0x47
#define PURSE_RET_TNO 0x48

#define PURSE_RECV_CARDNO 0x51	// 收到光电卡发送卡号命令
#define PURSE_SEND_CARDNO 0x52	// 返回光电卡发送卡号命令
#define PURSE_RECV_LEFLOW 0x53	// 接收光电卡流水命令
#define PURSE_RECV_DEP	  0x54	// 接收光电卡存款流水命令
#define PURSE_RECV_TAKE	  0x55	// 接收光电卡取款流水命令
#define PURSE_RECV_GROUP  0x56  // 接收光电卡分组流水命令
#define PURSE_RECV_CARDNO2 0x57	// 收到光电卡发送卡号命令
#define PURSE_RECV_LEFLOW2 0x59

//added by duyy,2012.1.31, not use
#define PURSE_REQ_PARNMF 0x61  //请求上电参数命令，不含管理费系数
#define PURSE_RET_PARNMF 0x62  //向终端发送上电参数，不含管理费系数
#define PURSE_REQ_MF 0x63  //请求管理费系数
#define PURSE_RET_MF 0x64  //终端发送管理费系数

extern AT91PS_USART uart_ctl0;
extern AT91PS_USART uart_ctl2;

#define BWT_66J10 80
#define BWR_66J10 700

#define BCD2BIN(val) (((val)&15) + ((val)>>4)*10)
#define BIN2BCD(val) ((((val)/10)<<4) + (val)%10)


extern int compute_blk(term_ram *ptrm, struct black_info *blkinfo);
extern int purse_send_conf(term_ram *ptrm, unsigned char *tm, struct black_info *blkinfo);
extern int purse_send_confnmf(term_ram *ptrm, unsigned char *tm, struct black_info *blkinfo);
extern int purse_send_managefee(term_ram *ptrm);
extern int purse_send_time(term_ram *ptrm, unsigned char *tm);
extern int purse_send_noconsumetime(term_ram *ptrm, unsigned char *nocmtm);//write by duyy, 2013.5.20
extern int purse_send_magfeetable(term_ram *ptrm);//write by duyy, 2013.11.19
extern int purse_send_forbidflag(term_ram *ptrm);//write by duyy, 2013.11.19
extern int purse_update_magfeetable(term_ram *ptrm);//write by duyy, 2013.11.19
extern int purse_update_forbidflag(term_ram *ptrm);//write by duyy, 2013.11.19
extern int purse_update_noconsumetime(term_ram *ptrm, unsigned char *nocmtm);//write by duyy, 2013.11.22
extern int purse_recv_btno(term_ram *ptrm);
extern int purse_recv_flow(term_ram *ptrm);
extern int purse_send_bkin(term_ram *ptrm, struct black_info *blkinfo, black_acc *blkacc, int flag);
extern int purse_broadcast_blk(black_acc *pbacc, struct black_info *pbinfo, term_ram *ptrm, int cnt);
extern int purse_update_key(term_ram *ptrm);
extern int purse_get_edition(term_ram *ptrm);
extern int purse_recv_leid(term_ram *ptrm, int allow, unsigned char *tm);//2012.4.26
extern int purse_recv_leflow(term_ram *ptrm, unsigned char *tm);
extern int purse_recv_letake(term_ram *ptrm, unsigned char *tm);
extern int purse_recv_ledep(term_ram *ptrm, unsigned char *tm);
extern int purse_recv_gpflow(term_ram *ptrm, unsigned char *tm);
int purse_recv_leflow2(term_ram *ptrm, unsigned char *tm);
int purse_recv_leid2(term_ram *ptrm, int allow, unsigned char *tm);//2012.4.26

#if 0
extern psam_local *search_psam(unsigned short num, struct psam_info *psinfo);
#endif
#endif
