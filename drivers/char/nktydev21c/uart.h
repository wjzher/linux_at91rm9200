#ifndef _UART_H_
#define _UART_H_

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

#define SETFLOWTAIL 0xF1
#define SETFLOWHEAD 0xF5
#define SETACCMAINPTR 0xF2
#define SETACCSWPTR 0xF3
#define SETRECORD 0xF4
#define INITUART 0xAB
#define START_485 0xF6
#define SETREMAIN 0xF7
#define SETFORBIDTIME 0xF8	//added by duyy, 2013.5.8
#define SETTIMENUM 0xF9	//added by duyy, 2013.5.8
#define HEADLINE 0xFA	//added by duyy, 2014.6.9

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
#define CHGACC1 0x9F
#define ADDACCSW1 0xA0
#define SETNETSTATUS 0x35
#define BCT_BLACKLIST 0x3A
#define SETLEALLOW 0x3B
#ifdef CONFIG_RECORD_CASHTERM
#define GETCASHBUF 0x3C
#endif
#define NEWREG 0xA1
#define SETSPLPARAM 0xA2
#define INITUARTDATA 0xA3

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

// 全局变量声明
extern unsigned int fee[16];
extern flow pflow[FLOWANUM];
extern terminal *pterminal;

extern int flow_sum;
extern int maxflowno;
extern struct flow_info flowptr;

extern int space_remain;
extern term_tmsg term_time[7];//modified by duyy, 2013.5.8
extern unsigned char ptmnum[16][16];//modified by duyy, 2013.5.8
extern char headline[40];//write by duyy, 2014.6.9
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

#ifdef CONFIG_UART_V2
// 定义特殊消费控制
extern user_cfg usrcfg;
// 全局变量支持
extern int current_id;		// 当前餐次ID
#endif

#endif
