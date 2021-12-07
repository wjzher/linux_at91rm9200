#ifndef _UART_H_
#define _UART_H_
#include "at91_rtc.h"

#define FLOWANUM 3000
#define BADFLOWNUM 200
#define MAXSWACC 10000
#define MAXBACC 100000

// error type
#define NOTERMINAL -1
#define NOCOM -2
#define STATUSUNSET -3
#define ERRVERIFY	-4
#define TNORMAL 0

//应用程序与驱动程序交互命令
#define SETFLOWTAIL 0xF1
#define SETFLOWHEAD 0xF5
#define SETACCMAINPTR 0xF2
#define SETACCSWPTR 0xF3
#define SETRECORD 0xF4
#define INITUART 0xAB
#define START_485 0xF6
#define SETREMAIN 0xF7
#define SETFORBIDTIME 0xF8	//added by duyy, 2013.3.26
#define SETTIMENUM 0xF9	//added by duyy, 2013.3.26
#define SETFEETABLE 0xFA //added by duyy, 2013.10.24
#define SETFORBIDFLAG 0xFB //added by duyy, 2013.10.24
#define GETTABLEFALG 0xFC //added by duyy, 2013.11.18
#define UPDATEFEE 0xFD//added by duyy, 有管理费数据更新时向下提示数据更新，2013.11.20
#define UPDATEFLAG 0xFE//added by duyy, 有禁止消费启用标志数据更新时向下提示数据更新，2013.11.20
#define UPDATESEG 0xFF//added by duyy, 有禁止消费时段数据更新时向下提示数据更新，2013.11.20

#define GETFLOWSUM 0x98
#define GETFLOWNO 0x99
#define ADDACCSW 0x9A
#define CHGACC 0x9B
#define GETFLOW 0x9C
#define SETFLOWPTR 0x9D
#define GETFLOWPTR 0x9E
#define GETACCMAIN 0x96
#define GETACCSW 0x95
#define GETACCINFO 0x94
#define SETLAPNUM 0x90
#define SETNETSTATUS 0x35
#define BCT_BLACKLIST 0x3A
#define SETLEALLOW 0x3B
#ifdef CONFIG_RECORD_CASHTERM
#define GETCASHBUF 0x3C
#endif

#define GETFLOWTAIL 0xE0
#define CLEARFLOWBUF 0xE1
#define GETFLOWTOTAL 0xE2

struct record_info {
	int term_all;
	int account_all;
	int account_main;
	int account_sw;
};

struct flow_info {
	int head;
	int tail;
	//int 
};
struct uart_ptr {
	terminal *pterm;
	acc_ram *paccmain;
	int term_all;
	int acc_main_all;
	int max_flow_no;
	unsigned char fee[16];
	int le_allow;
	int *pnet_status;
	struct black_info blkinfo;
	black_acc *pbacc;
};
struct get_flow {
	flow *pflow;
	int cnt;
};

struct bct_data {
	black_acc *pbacc;
	struct black_info info;
	terminal *pterm;
	int term_cnt;
};

struct bad_flow {
	int num;
	int total;
	flow *pbadflow;// 非正常流水缓存区, 每次叫号结束后应用程序取出
};
// 计算一天中的时间，输入6字节时间, added by duyy, 2013.3.26
static inline int _cal_itmofday(unsigned char *tm)
{
	int itm;
	// 将读到的时间换算为整数
	itm = BCD2BIN(tm[5]);
	itm += BCD2BIN(tm[4]) * 60;
	itm += BCD2BIN(tm[3]) * 3600;
	return itm;
}
// 全局变量声明
extern unsigned int fee[16];
extern flow pflow[FLOWANUM];
extern terminal *pterminal;

extern int flow_sum;
extern int maxflowno;
extern struct flow_info flowptr;
/*三个表单各自的版本号变量，write by duyy, 2013.11.19*/
extern long managefee_flag;
extern long forbid_flag;
extern long timeseg_flag;

extern int space_remain;
extern term_tmsg term_time[7];//modified by duyy, 2013.3.26
extern unsigned char ptmnum[16][16];//modified by duyy, 2013.3.26
extern char feetable[16][64];//added by duyy, 2013.10.28,feetable[终端类型][消费卡类型]
extern char feenum;//added by duyy, 2013.11.19,管理费收取方式：0表示内扣，1表示外扣
extern char forbidflag[16][64];//added by duyy, 2013.10.28,forbidflag[终端类型][消费卡类型]
extern term_ram * ptermram;
extern acc_ram * paccmain;
extern acc_ram paccsw[MAXSWACC];
extern struct record_info rcd_info;// 记录终端和账户信息

extern black_acc *pblack;// 支持100,000笔, 大小为879KB
extern struct black_info blkinfo;

extern int recv_data(char *buf, unsigned char num);
extern int send_data(char *buf, size_t count, unsigned char num);	// attention: can continue sending?
extern int send_byte(char buf, unsigned char num);
extern int send_addr(unsigned char buf, unsigned char num);

#endif
