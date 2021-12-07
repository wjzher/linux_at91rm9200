/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：data_arch.h
 * 摘    要：定义用到的数据结构体和485驱动中用到的部分命令字
 *			 新的POS机终端库结构体改变, 新增黑名单结构体
 *			 增加term_ram结构体成员, 作为终端机的参数设置标志
 * 			 更改FLASH中帐户格式, 扩充money, 2.2
 * 			 
 * 当前版本：2.1
 * 作    者：wjzhe
 * 完成日期：2007年9月3日
 *
 * 当前版本：2.0
 * 原作者  ：wjzhe
 * 完成日期：2007年6月14日
 *
 * 取代版本：1.1 
 * 原作者  ：wjzhe
 * 完成日期：2007年4月11日
 */
#ifndef _DATA_ARCH_H_
#define _DATA_ARCH_H_
#define ONEBYTETM 382

#include "dataflash.h"
#include "pdccfg.h"
//#define FLOWPERPAGE 21
#ifdef QINGHUA
#define TICKETBIT (1 << 7)
#define TICKETMONEY (1 << 31)
#define DEPBIT (1 << 4)
#endif

#define SENDLEIDIFNO 0x10	// 发送光电卡账户信息
#ifdef QINGHUA
#define SENDLETICKET 0x1A	// 打小票, 发送光电卡账户信息
#define RECVLETICKET 0x1B	// 打小票, 接收光电卡流水
#endif
#define SENDRUNDATA 0x20	// 发送上电参数
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

#ifdef QINGHUA
#define TPLOCKBIT (1 << 4)	// 用于流水类型, 锁卡流水标识
#endif
#define LECONSUME (0x1 << 5)
#define LECHARGE (0x2 << 5)
#define LETAKE (0x3 << 5)

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


typedef struct __usr_time {//BCD
	unsigned char sec;		// 0-59
	unsigned char min;		// 0-59
	unsigned char hour;		// 0-23
	unsigned char mday;		// 1-31
	unsigned char mon;		// 1-12
	unsigned char lyear;	// 0-99
	unsigned char hyear;	// 19, 20
} __attribute__ ((packed)) usr_time;

#if 0
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
 * PosTime		CHAR	7	BCD	实时时间：YYYYMMDDHHMMSS
 * AppCardID	DWORD	4	HEX	卡片序列号或卡号
 * AccountsNo	DWORD	4	HEX	帐号
 * MoneySum		DWORD	4	HEX	当前餐次消费总额
BakReg		BYTE	2	HEX	待扩展
 * ManagMoney	BYTE	3	HEX	管理费差额 
 * PosSetNo		WORD	2	HEX	结算机被设置编号 
 * PsamNo		WORD	2	HEX	结算机中PSAM卡编号
 * PsamTrdNo	DWORD	4	HEX	结算机中PSAM卡交易序号
 * WalTrdNo		WORD	2	HEX	钱包交易序号
 * WalTrdMoney	DWORD	4	HEX	交易金额，单位为分
 * WalRemMoney	DWORD	4	HEX	钱包交易后余额，单位是分
 * FoodGroupNo	BYTE	1	HEX	菜品分组编号：00～06
 * DealType		BYTE	1	HEX	交易类型标识: 见《Mifare one s50 卡片规划》――交易类型编码
 * FlowType		BYTE	1	HEX	流水类型标识：Bit0正常流水、Bit1失败流水、Bit2强制售饭流水、Bit3-Bit7为钱包类型 
 * TacCode		BYTE	2	HEX	TAC校验码: TacCode1=PosTime +…+ FlowType TacCode2=PosTime^……^FlowType
FlowStatus	BYTE	1	HEX	状态：0xFF未上传，0xFE已上传
*/
typedef struct __m1_flow {
	unsigned char flow_type;		// Bit0正常流水、Bit1失败流水、
									// Bit2强制售饭流水、Bit3-Bit7为钱包类型
									// Bit3补贴流水
	long flow_num;					// 流水号, 由UART驱动生成
	long consume_sum;				// 消费金额-->交易金额, 单位分
	usr_time date;					// 消费时间, 秒...年
#ifndef QINGHUA
	long manage_fee;				// 管理费差额
#else
	short manage_fee;
#endif
	unsigned char tml_num;			// 终端号
	unsigned char areano;			// 区号
	unsigned long card_num;			// 卡号
	long acc_num;					// 账号
#ifdef QINGHUA
	unsigned char psam_num[3];
#else
	unsigned short psam_num;		// PSAM卡编号
#endif
	long psam_trd_num;		// PSAM卡交易序号
	unsigned short walt_num;		// 钱包交易序号
	long remain_money;				// 钱包余额
	unsigned char food_grp_num;		// 菜品分组
#ifndef QINGHUA
	unsigned char deal_type;		// 交易类型
#endif
#ifndef QINGHUA
	unsigned short tac_code;		// TAC code
#endif
#ifndef QINGHUA
	long money_sum;					// 当天消费总额
#else
	unsigned int asn;				// 清华去掉消费总额, 加ASN
#endif
#ifdef QINGHUA
	int qh_tac;
#endif
} __attribute__ ((packed)) m1_flow;

/*
AppCardID	DWORD	4	HEX	存折卡卡号	
PosSetNo	WORD	2	HEX	结算机编号	
PsamNo	WORD	2	HEX	结算机中PSAM卡编号	
PsamTrdNo	DWORD	4	HEX	结算机中PSAM卡交易序号	
CsmMny	DWORD	4	HEX	交易金额，单位为分            	
BakReg	BYTE	1	HEX	待扩展	0X00
FlowType	BYTE	1	HEX	流水类型标识：Bit0正常流水、Bit1失败流水、Bit2强制售饭流水	
*/
typedef struct __le_flow {
	unsigned char flow_type;		// flow type: 0xAB, 0xAC, 0xAD---消费, 充值, 取款
	long flow_num;			// 本地流水号
	long consume_sum;				// 消费金额
	usr_time date;					// 交易日期时间: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
	long manage_fee;				// 管理费
	unsigned char tml_num;			// 终端号
	unsigned char areano;			// 区号
	unsigned long card_num;			// 卡号
	long acc_num;			// 账户
	unsigned short psam_num;		// PSAM卡编号
	unsigned long psam_trd_num;		// PSAM卡交易序号
	unsigned char notused[6];
	unsigned char food_grp_num;		// 分组信息
	unsigned char notused1[7];
} __attribute__ ((packed)) le_flow;

typedef struct __flow {				// 50-byte
	unsigned char flow_type;		// Bit0正常流水、Bit1失败流水、Bit2强制售饭流水、Bit3-Bit7为钱包类型
	long flow_num;					// 流水号, 由UART驱动生成
	long consume_sum;				// 消费金额-->交易金额, 单位分
	usr_time date;					// 消费时间, 秒...年
	long manage_fee;				// 管理费差额
	unsigned char tml_num;			// 终端号
	unsigned char areano;			// 区号
	unsigned long card_num;			// 卡号
	long acc_num;					// 账号
	unsigned short psam_num;		// PSAM卡编号
	unsigned long psam_trd_num;		// PSAM卡交易序号
	unsigned short walt_num;		// 钱包交易序号
	long remain_money;				// 钱包余额
	unsigned char food_grp_num;		// 菜品分组
	unsigned char deal_type;		// 交易类型
	unsigned short tac_code;		// TAC code
	long money_sum;					// 当前餐次消费总额
} __attribute__ ((packed)) flow;

#ifdef SMALLACOUNT
typedef struct __account {			// 12-byte
	unsigned long acc_num;			// account number
	unsigned long card_num;			// card number
	unsigned char feature;			// flag, figure, limit of meal
	unsigned char money[3];			// money
	//char no_use;
} __attribute__ ((packed)) account;
#else
#define COMPLEX_S
typedef struct __account {			// 12-byte
	unsigned long acc_num;			// account number
	unsigned long card_num;			// card number
	unsigned char feature;			// flag, figure,limit of meal 
	unsigned char id_type;			// ID type，16 类,added by duyy, 2013.7.1
#ifdef COMPLEX_S
	union {
#endif
		int money;					// money
#ifdef COMPLEX_S
		struct {
			unsigned char u8_money[3];
			unsigned char managefee;
		};
	};
#endif
} __attribute__ ((packed)) account;
#endif

struct term_param {					// 16-byte
	unsigned char areano;			// terminal area no
	unsigned char term_type;		// type of terminal
	unsigned char blkuptm;			// max time of update black list	无限制=0xFF
	unsigned short purse_time;		// 钱包最小消费间隔		无限=0
	unsigned char spcl;				// bit0~2=餐次数, bit3餐限
	unsigned char psam_passwd[3];	// psam password
	unsigned char term_passwd[3];	// terminal password
	unsigned char max_consume;		// max consume
	unsigned char bakreg[2];		// 备用
	unsigned char verify;
} __attribute__ ((packed)) ;

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

typedef struct __acc_ram {
	unsigned long acc_num;
	unsigned long card_num;			// card number
	long money;			// money
	unsigned char feature;			// flag, figure,limit of meal
	unsigned char id_type;			// ID type，16 类,added by duyy, 2013.7.1 
	int managefee;					// 0-15
} /*__attribute__ ((packed))*/ acc_ram;

struct tradenum {
	int num;				//
	union {
		unsigned char tm[5];	//YYMDH
		struct {
			unsigned char hyear;
			unsigned char lyear;
			unsigned char mon;
			unsigned char day;
			unsigned char hour;
		} __attribute__ ((packed)) s_tm;
	} __attribute__ ((packed)) ;
} __attribute__ ((packed)) ; 

typedef struct __black_acc {
	unsigned long card_num;		// 卡号
	struct tradenum t_num;				// 记录号
	__s8 opt;					// 1-->挂失, 0-->解挂
} __attribute__ ((packed)) black_acc;

struct black_info {// 24字节
	int edition;
	struct tradenum trade_num;
	int count;
#if 0 /* ndef PSAM_LIST */
	unsigned char pwd[3];	// 3字节PSAM卡密码
#endif
};
//终端禁止消费时段转换数据结构，write by duyy, 2012.4.26
typedef struct __term_tmsg {
	u32 begin;					// 开始时间
	u32 end;					// 结束时间
} term_tmsg;
struct time_term {
	unsigned char smin;
	unsigned char shour;
	unsigned char emin;
	unsigned char ehour;
} __attribute__ ((packed)) ;

struct term_tm {
	struct time_term tm1;
	struct time_term tm2;
	struct time_term tm3;
	struct time_term tm4;
} __attribute__ ((packed)) ;

typedef struct __terminal {			// 52-byte
	unsigned char local_num;		// local terminal number
	unsigned char price[32];		// food price
	//unsigned char max_consume;		// max consume
	unsigned char power_id;		// 几类终端
	struct term_param param;
#define mfee_feature(x) managefee[x].feature
#define mfee_value(x) managefee[x].value
#define mfee(x) managefee[x].manage
	union {
		struct {
			unsigned char feature;
			unsigned char value;
		} __attribute__ ((packed)) /*s_mfee*/;
		unsigned char manage[2];
	} __attribute__ ((packed)) managefee[64];
	int fee_n;
	struct term_tm tmsg;
} __attribute__ ((packed)) terminal;

typedef struct __term_ram {
	unsigned char term_no;
	terminal *pterm;
	int status;
	unsigned char dif_verify;// 所有的校验都只是本包数据的校验
	unsigned char add_verify;
	int flow_flag;
	int term_money;
	int term_cnt;
	struct black_info blkinfo;
	int black_flag;
	int key_flag;
	long psam_trd_num;
	//int max_num;		// local max flow number
	//struct rtc_time *tm;
} term_ram;

typedef struct __term_info {
	int term_no;
	int status;
	int money;	// 区分正负
	int flag;	// 0->消费机, 1->出纳机
} term_info;

#if 0
typedef struct {
	int				OperAreaID;					//发起操作的区号
	unsigned long	OperationID;				//黑名单纪录号
	unsigned long	OldCardId;					//操作对应卡号
	int			BLTypeId;					//黑名单操作类型
	unsigned long	NewCardId;					//换卡的卡号
	int				Type;	//1代表挂失；2代表解挂；3代表设偷餐；4代表解偷餐；5代表换卡；6代表账户注销；7代表当餐注册；8代表黑名单
	long			AccountId;					//当餐注册的账号
	int				RemainMoney;				//当餐注册账户余额
	int				Flag;						////当餐注册账户状态
	int				UpLimitMoney;				//当餐注册账户消费上限
	int				ManageId;					//当餐注册账户管理费系数
	int				PowerId;					//当餐注册账户身份类型
	char        FlowTime[20];					//黑名单流水结构
} no_money_flow;
typedef struct __money_flow
{
	long  AccountId; //账户卡号
	unsigned long CardNo; 
	int   Money; //余额变化量，单位为分 
} money_flow;
#endif

struct uart_mon {
	int cnt;
	void *data;
};

#define FLOWPERPAGE (BYTEOFDFPAGE / sizeof(flow))
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

#if 0
typedef struct {
	unsigned short psamno;				//PSAM卡编号
	char		   pwd[3];			//PSAM卡密码
} psam_local;

struct psam_info {
	psam_local *psam;
	int cnt;
};
#endif
#endif
