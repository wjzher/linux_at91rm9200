/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：data_arch.h
 * 摘    要：定义用到的数据结构体和485驱动中用到的部分命令字
 * 			 
 * 当前版本：1.1
 * 作    者：wjzhe
 * 完成日期：2007年4月11日
 *
 * 取代版本：1.0 
 * 原作者  ：wjzhe
 * 完成日期：2007年2月13日
 */
#ifndef _DATA_ARCH_H_
#define _DATA_ARCH_H_
#define ONEBYTETM 382
#ifndef CONFIG_UART_WATER
#define USRCFG1
#endif
#ifdef CONFIG_UART_WATER
#warning "Support water control"
#ifdef CONFIG_QH_WATER
#warning "QingHua water control Version"
#endif
#define WATER
#undef USRCFG1
#endif
//#define USRDBG
//#define EDUREQ
#ifdef EDUREQ
#ifndef WATER
#warning "EDU Version"
#undef USRCFG1
#endif
#endif
#ifdef CONFIG_CNTVER
#warning "New Version CntVer defined"
#undef USRCFG1
#endif

#define FLOWPERPAGE 25

#define SENDLEIDIFNO 0x10	// 发送光电卡账户信息
#define SENDRUNDATA 0x20	// 发送上电参数
#define SENDRUNDATA2 0x23	// 发送上电参数，带7字节时间
#define RECVNO 0x40			// 
#define RECVLEHEXFLOW 0xF	// 接收光电卡流水(HEX)
#define RECVLEBCDFLOW 0x2	// 接收光电卡流水(BCD)
#define RECVGPFLOW 0x82		// 接收分组流水
#define RECVTAKEFLOW 0x04	// 接收光电卡取款流水
#define RECVDEPFLOW 0x08	// 接收光电卡存款流水
#ifdef CPUCARD
#define RECVCPUIDIFON 0x90	// 接收CPU卡账户信息
#define RECVCPUFLOW 0x86	// 接收CPU卡流水
#define RECVCPUFLOW1 0x84	// 接收CPU卡流水
#endif /* end CPUCARD */
#define SENDBLACK 0x52		// 发送黑名单
#define RECVM1FLOW 0x51		// 接收M1电子钱包流水
#define SENDTIME 0x50		// 发送系统当前时间
#define SENDOFFPAR 0x53		// 发送37 Bytes终端参数(脱机)
#define RECVFLOWOVER 0x54	// 终端回传流水完毕
#define NOCMD 0x80
#ifdef CONFIG_QH_WATER
#define SENDSYSTIME 0x30
#endif
#ifdef WATER
#define RECVWATSFLOW 0x84	// 收取水控终端单次流水
#define RECVWATAFLOW 0x85	// 将终端的单次流水暂存区内的流水打包存入流水区
#endif
#define RECVREFUND 0x11		// 退款机上传流水
#define SENDLEIDINFO2 0x12	// 发送光电卡账户信息, 增加账号和时间
#define RECVLEHEXFLOW2 0x13	// 上传光电卡流水, 接收时间
#define RECVRFDLEID 0x15	// 扣款机发送光电卡账户信息, 修改校验
#define RECVRFDDEPFLOW 0x16	// 接收出纳机存款流水, 修改校验
#define RECVRFDLEFLOW 0x17	// 接收退款机消费流水, 修改校验
#define RECVRFDLEFLOW2 0x18	// 接收消费机消费流水, 修改校验
#define RECVRFDLEIDINFO 0x19	// 接收消费机账户信息, 修改校验
#define RECVLEIDINFO3 0x1A	// 接收消费机账户信息, 增加输入金额
#define REFUNDBIT 0x80


#define LECONSUME 0xAB
#define LECHARGE 0xAC
#define LETAKE 0xAD
#ifdef CONFIG_QH_WATER
#define LESUBSIDE 0xAF
#endif
#define LEREFUND 0xAA
#define LEFCHARGE 0xAE

#define CHKXOR 0x49
#define CHKSUM 0x48
#define CHKALL 0x47
#define CHKNXOR 0x46
#define CHKCSUM 0x45

#define FRIST_ENTER_CARD_FLAG 0
#define LIMIT_MODE_FLAG 1         //  终端有消费上限标志位
#define RAM_MODE_FLAG 2
#define ENTER_SECRET_FLAG 3         //输密码标志位
#define ONE_PRICE_FLAG 4           // 单品种方式标志位
#define CASH_TERMINAL_FLAG 5       // 出纳机标志位
#define MESS_TERMINAL_FLAG 6      //  售饭机标志位 
#define REGISTER_TERMINAL_FLAG 7    // 注册机标志位

#define ACCOUNTCLASS 4
#define ACCOUNTFLAG 6
// 定义禁止存款位
#define ACCDEPBIT (0x80)

// error type
#define NOTERMINAL -1
#define NOCOM -2
#define STATUSUNSET -3
#define TNORMAL 0

typedef struct __usr_time {//BCD
	unsigned char sec;		// 0-59
	unsigned char min;		// 0-59
	unsigned char hour;		// 0-23
	unsigned char mday;		// 1-31
	unsigned char mon;		// 1-12
	unsigned char lyear;	// 0-99
	unsigned char hyear;	// 19, 20
} __attribute__ ((packed)) usr_time;

#ifdef CPUCARD
typedef struct __cpu_flow {
	unsigned char flow_type;		// 流水类型: 0X00: 黑卡; 0X02: 中途拔卡; 0X03: 钱包数据错; 0X05: 无法写卡; 0X10: 密码错; 0XFA: 充值记录; 0XFE: 冲正记录; 0X99: 正常消费记录
	unsigned long flow_num;			// 本地流水号
	long consume_sum;				// 消费金额
	usr_time date;					// 交易日期时间: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
	long purse_money;				// 钱包余额
	unsigned char tml_num;			// 终端号
	unsigned long card_num;			// 卡序列号
	unsigned long term_trade_num;	// 终端交易序号
	unsigned char trade_type;		// 交易类型标识: 0x06
	unsigned char term_num[6];		// 终端机编号
	unsigned int tac_code;			// TAC code
	unsigned short purse_num;		// 钱包交易序号
} __attribute__ ((packed)) cpu_flow;
#endif
/*
Date1	String[8]		日期	4	BCD	YYYYMMDD	
Time1	String[6]		时间	3	BCD	hhmmss	
ApplySerialNo	String[20]		卡片应用序列号（ID）	4 (10)	BCD		
Times	Word		钱包交易序号	2	Hex	无符号整数
0~65535	
TransMoney	Dword		交易金额，单位是分	(低2字节有效，高2字节为0)	4	Hex	无符号整数0~4294967295	
Dealtype	Byte		交易类型标识，	00–EP消费、01–充值、02–扣款、03–管理费	1	Hex	6	
POSNo	Byte		终端机编号	1(4)	HEX
Invoiceno	Dword		终端交易序号	4	Hex	无符号整数0~4294967295	
LastMoney	Dword		钱包交易后余额，单位是分	4	Hex	无符号整数0~4294967295	
TACMark	String[4]		TAC（交易验证码）	2(4)	Hex		
DealCode1	String[2]		0X99：正常消费记录	其他见错误码列表	1	Hex		
ProcessCode	Byte	高半字节为主流程步骤低半字节为子流程步骤	1	Hex		
Uploaded	Byte	0xFF: 本流水未上传 0x00：已上传	1	Hex		
合计			32			
*/
typedef struct __m1_flow {
	unsigned char flow_type;		// 流水类型: 0X00: 黑卡; 0X02: 中途拔卡;
									// 0X03: 钱包数据错; 0X05: 无法写卡;
									// 0X10: 密码错; 0XFA: 充值记录;
									// 0XFE: 冲正记录; 0X99: 正常消费记录
	long flow_num;					// 本地流水号
	long consume_sum;				// 消费金额
		 // 线上数据为低2字节有效, 高2字节为0
	usr_time date;					// 交易时间, BCD
	long manage_fee;				// 管理费
	unsigned char term_num;			// 终端机编号
	unsigned long card_num;			// 卡片应用序列号, BCD
	long acc_num;			// 账号
	unsigned long term_trade_num;	// 终端交易序号
	unsigned char trade_type;		// 交易类型标识: 0x06 00 –EP消费 
									// 01 –充值 02 –扣款 03 –管理费
	unsigned short purse_num;		// 钱包交易序号
	long purse_money;				// 钱包余额
		 // 线上数据为无符号型, 收取数据时要注意
	unsigned char notused[2];
} __attribute__ ((packed)) m1_flow;

typedef struct __le_flow {
	unsigned char flow_type;		// flow type: 0xAB, 0xAC, 0xAD---消费, 充值, 取款
	long flow_num;					// 本地流水号
	long consume_sum;				// 消费金额
	usr_time date;					// 交易日期时间: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
	long manage_fee;				// 管理费
	unsigned char tml_num;			// 终端号
	unsigned long card_num;			// 卡号
	long acc_num;			// 账户
	unsigned char notused[13];
} __attribute__ ((packed)) le_flow;

typedef struct __flow {				// 42-byte
	unsigned char flow_type;		// flow type: 0x99 right; 0x0E error; 0xAB le flow
	long flow_num;					// number of flow
	long consume_sum;				// money of consume
	usr_time date;					// date: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
	long manage_fee;				// 管理费
	unsigned char tml_num;			// number of terminal machine
	unsigned long card_num;			// number of card
	long acc_num;
	unsigned long term_trade_num;	// terminal machine trade number from 999: le_flow->accnum
	unsigned char trade_type;		// trade type
	unsigned short purse_num;		// purse trade number
	long purse_money;				// money left in purse; le->manage fee
	unsigned char notused[2];		// number of terminal machine
	//unsigned int tac_code;			// TAC code
} __attribute__ ((packed)) flow;

/* 驱动中不使用此结构 */
typedef struct __account {			// 12-byte
	unsigned long acc_num;			// account number
	unsigned long card_num;			// card number
	unsigned char feature;			// flag, figure, limit of meal
	unsigned char money[3];			// money
	//char no_use;
} __attribute__ ((packed)) account;

typedef struct __terminal {			// 35-byte
	unsigned char term_type;		// type of terminal
	unsigned char local_num;		// local terminal number
	unsigned char price[32];		// food price
	unsigned char max_consume;		// max consume
	unsigned char power_id;		// 几类终端
} __attribute__ ((packed)) terminal;

typedef struct __time_seg {
	unsigned char start1[3];
	unsigned char end1[3];
	unsigned char start2[3];
	unsigned char end2[3];
	unsigned char start3[3];
	unsigned char end3[3];
	unsigned char start4[3];
	unsigned char end4[3];
	unsigned char start5[3];
	unsigned char end5[3];
	unsigned char start6[3];
	unsigned char end6[3];
} __attribute__ ((packed)) time_seg;

typedef struct __acc_ram {		// 应用程序和内核共用
	unsigned long acc_num;
	unsigned long card_num;			// card number
	long money;			// money
	unsigned char feature;			// flag, figure, limit of meal
	int managefee;					// 0-15
#if defined (EDUREQ)
	int nflag;
#endif
#ifdef CONFIG_QH_WATER		// 清华水控结构
	long sub_money;			// 补贴余额
	int power_id;				// 身份(0-7), 清华水控扩展身份
#endif
#if defined (USRCFG1)
	int nflag[4];		// uchar不能用
	int itm;
	// 增加4个时段折扣次数
	int ndisc[4];		// 折扣次数
#endif
#if defined (CONFIG_CNTVER)
	short cnt[2];					// 分两个次数统计
	unsigned short page;		// 此账户信息在flash中的位置
	int itm[2];					// 每餐消费次数
#endif

#ifdef CONFIG_SUBCNT
	int sub_cnt;	// 剩余次数
	int sub_flag;	// 计数统计
#endif

#ifdef CONFIG_RDFZ
	char user_name[12];		// 保存用户姓名
#endif
} /*__attribute__ ((packed))*/ acc_ram;

#ifdef EDUREQ
#define ETERMNUM 256
struct edupar {
	int figure;	// 身份1~4
	int cn;		// 消费次数, 从cn次起开始加倍
	int rate;	// 费率倍数
	int stime1;	// 起始时间1
	int etime1;	// 结束时间1
	int stime2;	// 起始时间2
	int etime2;	// 结束时间2
};
#endif

#if 0
typedef struct __black_acc {
	unsigned long card_num;		// 卡号
	long rec_num;				// 记录号
	__s8 opt;					// 1-->挂失, 0-->解挂
} __attribute__ ((packed)) black_acc;
#endif

typedef struct __term_ram {
	int term_no;
	terminal *pterm;
	int status;
	unsigned char dif_verify;
	unsigned char add_verify;
	int flow_flag;
	int term_money;
	int term_cnt;
#ifdef WATER
	struct __water_flow {
		unsigned long cardno;
		unsigned long accno;
		int money;
		char tm[6];
#ifdef CONFIG_QH_WATER
		int sub_flag;	// 补贴流水标志
#endif
	} w_flow;
#endif
#ifdef EDUREQ
	int money;
#endif
#ifdef CONFIG_QH_WATER
	char is_limit;
	char is_use;
#endif
	//int max_num;		// local max flow number
	//struct rtc_time *tm;
	//struct black_info term_blk;
} term_ram;

typedef struct __term_info {
	int term_no;
	int status;
	int money;	// 区分正负
	int flag;	// 0->消费机, 1->出纳机
} term_info;

typedef struct __NoMoneyFlowStruct
{
	int    OperAreaID;     //发起操作的区号
	unsigned long OperationID;    //黑名单纪录号
	unsigned long OldCardId;     //操作对应卡号
	int   BLTypeId;     //黑名单操作类型
	unsigned long NewCardId;     //换卡的卡号
	int    Type;
	// 1代表挂失；2代表解挂；
	// 3代表设偷餐；4代表解偷餐；
	// 5代表换卡；6代表账户注销；
	// 7代表当餐注册；8代表黑名单
	long   AccountId;     //当餐注册的账号
	int    RemainMoney;    //当餐注册账户余额
	int    Flag;      ////当餐注册账户状态
#ifdef CONFIG_QH_WATER
	unsigned short sub_money;
	unsigned short UpLimitMoney;
#else
	int    UpLimitMoney;    //当餐注册账户消费上限
#endif
	int    ManageId;     //当餐注册账户管理费系数
	int    PowerId;     //当餐注册账户身份类型
} no_money_flow;

typedef struct __money_flow
{
	long  AccountId; //账户卡号
	unsigned long CardNo; 
	int   Money; //余额变化量，单位为分 
} money_flow;

#ifdef S_BID
/* 2008-3-13 from RD new */
struct bid_info {
	int tms1;	// 时间开始
	int tme1;	// 时间结束
	int tms2;
	int tme2;
	int tms3;
	int tme3;
	int tms4;
	int tme4;
	char figure[3];	// 身份
	char power_id[3];// 4类终端
};
#define SETBIDINFO 0x36
#endif

#ifdef USRCFG1
struct usr_tmsg {
	int no;
	int begin;			// 开始时间
	int end;			// 结束时间
	int limit[4];			// 限制次数
	int money[4];		// 固定金额
	int imoney;		// 终端机输入的金额
	unsigned int tnot[8];		// 8个通道, 32-bit
};
struct usr_allow {
	int figure;		// 身份--1234,0是结束标志
	int begin;		// 开始时间
	int end;		// 结束时间
	int allow;		// 是否允许
};
struct usr_cfg {
	struct usr_tmsg usrtm[4];		// 可以设置4个特殊消费时段
	struct usr_allow usrlw[16];
	char flag;	// 新增标志, 次数到了之后是否输密码, by wjzhe, 20091208
	char mflag;	// 新增标志，标志是否计金额
	char lflag;	// 新增标志，标志是否智能餐次
	char sflag;	// 新增标志，标志是否统计异区流水，限餐次
	/* unsigned char */int discount[4][4];		// 四类身份折扣, 301，分四个餐次
	// 增加打折次数限制
	short discn[4][4];	// [0]->餐次1时四类身份打折
	// 增加标志，打折次数为反，即discn次之前是不打折，之后打折，折扣可大于100
	char dnflag;		// 非0为反
};
#define SETUSRCFG 0x37
#define SETTERMONEY 0x1001
// 定义终端金额限制16*4
struct term_money {
	int tm1[4];	// 时段1限制
	int tm2[4];
	int tm3[4];
	int tm4[4];
};

#endif

#ifdef CONFIG_CNTVER
struct tmsgcfg {
	int start;		// 每餐开始时间
	int end;		// 每餐结束时间
	short cnt[4];	// 允许次数
	int itm;		// 终端允许消费次数
};
struct cnt_conf {
	int start;		// 起始时间
	int end;		// 结束时间
	unsigned int tnot[8];	// 特殊的终端
	struct tmsgcfg tcfg[2];	// 时段
	//char ip[16];	// 另一个区的IP
};

struct f_cntver {
	unsigned int acc_num;
	unsigned int card_num;
	unsigned int time;
	short remain[2];
} __attribute__ ((packed)) ;

struct cntflow {
	unsigned int flowno;
	unsigned int card_num;	// 卡号做为流水
	unsigned char ctm;				// 用于标识哪个时段产生的流水
}__attribute__ ((packed)) ;

struct cnt_io {
	int n;
	struct cntflow *p;
};

#define FCNTVERN 80
extern struct cnt_conf cntcfg;
extern struct f_cntver fcntver[FCNTVERN];
extern int fcntpos;
extern struct cntflow cfbuf[FCNTVERN];
extern int cfpos;
#endif

#ifdef CONFIG_RECORD_CASHTERM
// 定义记录出纳机状态的变量
struct cash_status {
	unsigned char termno;		// 终端号
	unsigned char status;		// 卡或消费状态
	unsigned char feature;		// 此帐号标识
	unsigned int accno;			// 帐号
	unsigned int cardno;		// 卡号
	int money;					// 余额
	int consume;				// 此次消费额
	int managefee;				// 管理费
	int term_money;				// 出纳机当前总金额
};
#define CASH_NORMAL 0		// 通常状态
#define CASH_CARDIN 1		// 卡插入
#define CASH_CARDOFF 2		// 卡拔走
#define CASH_TAKEOFF 3		// 完成取钱
#define CASH_DEPOFF 4		// 完成存钱
#define CASHBUFSZ 16
extern const int cashterm_n;
extern int cashterm_ptr;				// 存储指针
extern struct cash_status cashbuf[CASHBUFSZ];	// 保存状态空间
#endif
#endif
