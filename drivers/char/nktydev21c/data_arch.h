/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：data_arch.h
 * 摘    要：定义用到的数据结构体和485驱动中用到的部分命令字
 *			 新的POS机终端库结构体改变, 新增黑名单结构体
 *			 增加term_ram结构体成员, 作为终端机的参数设置标志
 * 			 更改FLASH中帐户格式, 扩充money, 2.2
 *			 增加一卡通V2支持
 * 	
 * 当前版本：2.2
 * 作    者：Duyy
 * 完成日期：2013年6月18日
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

#ifdef CONFIG_NKTYCPUCARD
#define CPUCARD
#endif
#include "dataflash.h"

#define SENDLEIDIFNO 0x10	// 发送光电卡账户信息
#define SENDRUNDATA 0x20	// 发送上电参数
#define RECVNO 0x40			// 
#define RECVLEHEXFLOW 0xF	// 接收光电卡流水(HEX)
#define RECVLEBCDFLOW 0x2	// 接收光电卡流水(BCD)
#define RECVTAKEFLOW 0x04	// 接收光电卡取款流水
#define RECVDEPFLOW 0x08	// 接收光电卡存款流水
#define NOCMD 0x80

#ifdef CONFIG_UART_V2
/* 历史新增命令 */
#ifdef CONFIG_WATER
#define RECVWATSFLOW 0x84	// 收取水控终端单次流水
#define RECVWATAFLOW 0x85	// 将终端的单次流水暂存区内的流水打包存入流水区
#endif	/* CONFIG_WATER */

/* 双账户请求余额命令 */
#define SENDLEID_DOUBLE 0x99
/* 双账户流水接收命令 */
#define RECVLEFLOW_DOUBLE 0x9A

/*出纳机和退款机新命令，双校验，write by duyy, 2013.6.18*/
#define SENDLEIDACCX 0x91	// 发送光电卡账户信息, 增加账号双校验
#define RECVDEPFLOWX 0x93	// 接收光电卡存款流水 双校验
#define RECVTAKEFLOWX 0x92	// 接收光电卡取款流水 双校验
#define RECVREFUNDFLOW 0x94	// 接收退款机流水 双校验

#define SENDRUNDATAX 0x95   // 发送上电参数 双校验//write by duyy, 2013.6.26

#define SENDLEID_TICKET 0x96// 发送双账户信息，增加账号、姓名，支持终端打小票功能，write by duyy, 2014.6.6
#define SENDLEIDACCX_TICKET 0x97// 发送双账户信息，增加账号、姓名，支持出纳终端打小票功能，write by duyy, 2014.6.6
#define SENDHEADLINE 0x98   // 发送小票打印机标题文字 双校验//write by duyy, 2014.6.9

/* 消费标志 */
#define TRMF_DISC (1 << 1)		/* 打折标志 */
#define TRMF_LIMIT (1 << 0)		/* 餐次标志 */
#define TRMF_DISCF (1 << 8)		/* 打折终端消费标志 */
#define FLOWF_SPLIT (1 << 2)	/* 流水拆分标志 */
#endif	/* CONFIG_UART_V2 */

#define LECONSUME (0x1 << 5)
#define LECHARGE (0x2 << 5)
#define LETAKE (0x3 << 5)
#define LEREFUND (0x4 << 5)//write by duyy, 2013.6.17

#define CHKXOR 0x49
#define CHKSUM 0x48
#define CHKALL 0x47
#define CHKNXOR 0x46
#define CHKCSUM 0x45

#define CONSTANT_MONEY_TERMIAL 0  //#define FRIST_ENTER_CARD_FLAG 0,modified by duyy, 2013.6.26
#define LIMIT_MODE_FLAG 1         //  终端有消费上限标志位
#define REFUND_FLAG 2			 //#define RAM_MODE_FLAG 2,modified by duyy, 2013.6.17
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
 * FlowStatus	BYTE	1	HEX	状态：0xFF未上传，0xFE已上传
 * add cpu card support, modified by wjzhe, Sep 11 2009
 */
typedef struct __m1_flow {
	unsigned char flow_type;		// Bit0正常流水、Bit1失败流水、Bit2强制售饭流水、Bit3-Bit7为钱包类型
	long flow_num;					// 流水号, 由UART驱动生成
	int consume_sum;				// 消费金额-->交易金额, 单位分
	usr_time date;					// 消费时间, 秒...年
	long manage_fee;				// 管理费差额
	unsigned char tml_num;			// 终端号
	unsigned char areano;			// 区号
	unsigned int card_num;			// 卡号, CPU卡为ASN
	unsigned int acc_num;					// 账号
#ifndef CPUCARD
	unsigned short psam_num;		// PSAM卡编号
#else
	unsigned int psam_num;			// CPU PSAM编号4-byte
#endif
	long psam_trd_num;		// PSAM卡交易序号
	unsigned short walt_num;		// 钱包交易序号
	long remain_money;				// 钱包余额
	unsigned char food_grp_num;		// 菜品分组
	unsigned char deal_type;		// 交易类型
#ifndef CPUCARD
	unsigned short tac_code;		// TAC code
	long money_sum;					// 当天消费总额
#else
	unsigned int cpu_tac;			// cpu card tac code
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
	unsigned char ltype;		// 限制类型标识 bit0 餐限设置 bit 1打折流水
	unsigned char is_sub;		// 标识是否为补贴账户流水
	unsigned char notused[12];
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


#ifdef CONFIG_UART_V2
// 一卡通V2版本账户库
/* FLASH中账户存储结构 */
typedef struct __account {
	unsigned int acc_num;			// account number
	unsigned int card_num;			// card number

	/*现金账户信息 */
	int money;						// 现金账户金额
	unsigned char passwd[3];		// 3字节密码(BCD)
	unsigned short money_limit;		// 当餐消费上限(0~655.35),0xFFFF为无限模式,added by duyy, 2014.2.17
	unsigned char times_limit;		// 当餐消费次数上限(0~255),0xFF为无限模式,added by duyy, 2014.2.17
	unsigned char power_id;			// 身份类型
									// bit7换卡标识, 驱动中使用
									// bit6不允许充值标识
	unsigned char flag;				// 标志
									// bit 0: 挂失
									// bit 1: 注销
									// bit 2: 账户冻结
									// bit 3: 补贴冻结
									// bit 7..4: 管理费id
	unsigned char st_limit;			// 到达消费上限后的状态,modified by duyy, 2013.6.17
									//bit1bit0=0:现金帐户到达消费上限后禁止消费
									//bit1bit0=1:现金帐户到达消费上限后输密码继续消费现金帐户
									//bit1bit0=2:现金帐户到达消费上限后输密码继续消费补贴余额
									//bit1bit0=3:现金帐户到达消费上限后不输入密码继续消费补贴余额	
									//bit3bit2=0:补贴帐户到达消费上限后禁止消费
									//bit3bit2=1:补贴帐户到达消费上限后允许输密码消费补贴帐户
									//bit3bit2=2:补贴帐户到达消费上限后允许输密码消费现金帐户
									//bit3bit2=3:补贴帐户到达消费上限后不输密码继续消费现金帐户
									//bit4=1:允许跨账户消费;bit4=0:不允许跨账户消费
									//bit5=1:优先消费现金帐户;bit5=0：优先消费补贴帐户

	/* 补贴账户信息 */
	int sub_money;					// 补贴金额sub money, char[3] to int
	unsigned short smoney_limit;	// 当餐补贴消费上限
	unsigned char stimes_limit;		// 当餐次数消费上限
	unsigned char spower_id;		// 补贴账户身份类型

	/* 剩余消费额/次数 */
	unsigned short money_life;		// 现金帐户消费金额
	unsigned char times_life;		// 现金帐户消费次数
	unsigned short smoney_life;		// 补贴消费金额
	unsigned char stimes_life;		// 补贴消费次数

	unsigned char dic_times;		// 打折消费次数统计最大255
	char user_name[12];		// 保存用户姓名,write by duyy, 2014.6.6
	/* 透支, duyy,2013.6.17 */
	unsigned short draft;		// 现金帐户透支额度，以元为单位
	unsigned short sdraft;		// 补贴帐户透支额度，以元为单位
} __attribute__ ((packed)) /* account */acc_ram;



#define ACCF_LOSS (1 << 0)//挂失卡
#define ACCF_WOFF (1 << 1)//注销
#define ACCF_FREEZE (1 << 2)//现金帐户冻结
#define ACCF_FREEZE_SUB (1 << 3)//补贴帐户冻结
#define ACCF_MNGE_MASK (0xF << 4)//管理费

/* st_limit bit */
#define ACCST_CASH_NCM (0 << 0)      //现金帐户到达消费上限后禁止消费,modified by duyy, 2013.6.17
#define ACCST_CASH_PSWCASH (1 << 0)  //现金帐户到达消费上限后输密码继续消费现金帐户,modified by duyy, 2013.6.17
#define ACCST_CASH_PSWSUB (2 << 0)   //现金帐户到达消费上限后输密码继续消费补贴余额,modified by duyy, 2013.6.17
#define ACCST_CASH_CMSUB (3 << 0)    //现金帐户到达消费上限后不输入密码继续消费补贴余额,modified by duyy, 2013.6.17
#define ACCST_SUB_NCM (0 << 2)       //补贴帐户到达消费上限后禁止消费,modified by duyy, 2013.6.17
#define ACCST_SUB_PSWSUB (1 << 2)    //补贴帐户到达消费上限后允许输密码消费补贴帐户,modified by duyy, 2013.6.17
#define ACCST_SUB_PSWCASH (2 << 2)   //补贴帐户到达消费上限后允许输密码消费现金帐户,modified by duyy, 2013.6.17
#define ACCST_SUB_CMCASH (3 << 2)    //补贴帐户到达消费上限后不输密码继续消费现金帐户,modified by duyy, 2013.6.17
#define ACCST_ALLOW_UNIT (1 << 4)	 // 允许跨账户消费,modified by duyy, 2013.6.17
#define ACCST_CASH (1 << 5)		     // 优先消费现金账户,modified by duyy, 2013.6.17

/* power_id bit */
#define ACCP_CHANGE (1 << 7)		// 是否为换过的卡
#define ACCP_NOCASH (1 << 6)		// 不允许充值

//typedef account acc_ram;		// 驱动中的账户信息和FLASH中的一致
#else
#ifdef SMALLACOUNT
typedef struct __account {			// 12-byte
	unsigned long acc_num;			// account number
	unsigned long card_num;			// card number
	unsigned char feature;			// flag, figure, limit of meal
	unsigned char money[3];			// money
	//char no_use;
} __attribute__ ((packed)) account;
#else
typedef struct __account {			// 12-byte
	unsigned long acc_num;			// account number
	unsigned long card_num;			// card number
	unsigned char feature;			// flag, figure, limit of meal
	union {
		int money;					// money
		struct {
			unsigned char u8_money[3];
			unsigned char managefee;
		};
	};
} __attribute__ ((packed)) account;
#endif	/* SMALLACOUNT */
typedef struct __acc_ram {
	unsigned long acc_num;
	unsigned long card_num;			// card number
	long money;			// money
	unsigned char feature;			// flag, figure, limit of meal
	int managefee;					// 0-15
} /*__attribute__ ((packed))*/ acc_ram;
#endif	/* CONFIG_UART_V2 */

struct term_param {					// 16-byte
	unsigned char areano;			// terminal area no
	unsigned char term_type;		// type of terminal
	unsigned char blkuptm;			// max time of update black list	无限制=0xFF
	unsigned short purse_time;		// 钱包最小消费间隔		无限=0
	unsigned char spcl;				// bit0~2=餐次数, bit3餐限
	unsigned char psam_passwd[3];	// psam password
	unsigned char term_passwd[3];	// terminal password
	unsigned char max_consume;		// max consume
	unsigned short card_id; //卡类型,write by duyy, 2013.6.26
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
};
//终端禁止消费时段转换数据结构，write by duyy, 2013.5.8
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
	unsigned int psam_trd_num;
#ifdef CONFIG_UART_V2
	/* 终端金额输入 */
	int _tmoney;
	/* 当前消费标志 bit 0餐次餐限, bit 1打折 */
	unsigned int _con_flag;
#endif
#if 0
	int blkerr;				// 收取GA错误计数
	unsigned long jff_trdno;	// 最后一次更新黑名单的时间
#endif
#ifdef CONFIG_RTBLKCNTMODE		// 更改黑名单策略
	int nocmdcnt;			// 计数
#endif
} term_ram;

typedef struct __term_info {
	int term_no;
	int status;
	int money;	// 区分正负
	int flag;	// 0->消费机, 1->出纳机
} term_info;


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

#ifdef CONFIG_UART_V2
/* 特殊参数结构定义 */

/* 身份类型和终端类型对应表 */
typedef unsigned short term_allow;

/* 打折参数结构 */
typedef struct __discount {
	u16 disc_val1;		// 第一次打折比例 0xFFFF无效　0xFFFE禁止消费  0xFFFD表示比例为0,modified by duyy, 2013.6.19
	u16 const_val1;		// 第一次固定金额 0xFFFF无效　0xFFFE禁止消费  0xFFFD表示固定0元
	u16 disc_val2;		// 第二次打折比例 0xFFFF无效　0xFFFE禁止消费  0xFFFD表示比例为0
	u16 const_val2;		// 第二次固定金额 0xFFFF无效　0xFFFE禁止消费  0xFFFD表示固定0元
	u8 start_times;		// 刷卡若干次后开始打折
	u8 end_times1;		// 第一次打折次数 0表示无效
	u8 end_times2;		// 第二次打折次数 0表示无效
	u8 reserved;		// 保留
						// 有效标志 0表示无效 1表示个人账户有效
						// 2表示补贴账户有效 3表示都有效
} discount;

/* 时段和打折信息描述 */
typedef struct __user_tmsg {
	int id;						// 编号 1~4
	u32 begin;					// 开始时间
	u32 end;					// 结束时间
	u32 t_mask[8];		// 终端允许掩码
	discount disc[16];			// 16个身份打折参数
} user_tmsg;

/* 身份禁止消费时段 */
typedef struct __user_nallow {
	u32 begin[4];		// 开始时间, 最多四个时间
	u32 end[4];			// 结束时间
} user_nallow;

/* 特殊参数描述 */
typedef struct __user_cfg {
	user_tmsg tmsg[4];		// 四个时段
	user_nallow nallow[16];	// 16身份不允许消费时段
	term_allow tallow[16];	// 16身份对应16终端, 全局有效
} user_cfg;

// 异区流水结构

// 非现金流水结构，如果是当餐注册流水，则用白名单结构
typedef struct __NoMoneyFlowStruct
{
	int			OperAreaID;				//发起操作的区号
	unsigned int	OperationID;				//黑名单纪录号
	unsigned int	OldCardId;					//操作对应卡号
	unsigned int	NewCardId;				//换卡新卡号 
	int			BLTypeId;					//黑名单操作类型
	int			Type;						//1代表挂失；
											//2代表解挂；
											//5代表换卡； 
											//6代表账户注销；
											//8代表黑名单
											//9修改账户密码
											//10代表主帐户冻结; //write by duyy, 2013.5.8
											//11代表主帐户解冻; //write by duyy, 2013.5.8
											//12代表补贴帐户冻结;//write by duyy, 2013.5.8 
											//13代表补贴帐户解冻//write by duyy, 2013.5.8
	char FlowTime[8];		// 黑名单交易时间 YYYYMMDDhhnnss，7个字节最后一个填0
} no_money_flow;

typedef struct __money_flow
{
	int  		AccountId; //账户账号
	unsigned int 	CardNo; 	//卡号
	int   Money;	//余额变化量，单位为分
	char  AccoType;	//计入哪一个账户（针对双余额，0表示个人余额，1表示补贴余额）
	char  LimitType;	//限制类型，是否计入消费次数消费金额（0表示不计入，1表示计入）
	char  DisTermConsume;//是否在打折终端上消费
	char Reserved[1];	// 保留
	unsigned int time;
} money_flow;

struct net_acc
{
	int	AccountId;			//账号
	unsigned int  CardId;		//卡号
	int 	RemainMoney;		//余额（扣除余额下限后的）
	int 	Flag;				//标志（挂失、偷餐）
	int 	ManageId;			//现金帐户存款管理费，保留（第几个管理费从0开始，
								//同时提供管理费系数列表char[n]）
								//最高位表示是否允许存款
	char 	Password[4];		//消费密码				
	int 	UpLimitMoney;		//现金帐户消费上限
	unsigned char	UpLimitTime;	//现金帐户消费次数上限
	unsigned char	FreezeFlag; 	//现金帐户冻结标志
	unsigned char	PowerId;		//存折卡现金帐户余额身份类型 1~16
	unsigned char	PwdType;	//现金帐户到达消费上限后状态//modified by duyy, 2013.6.17
								//bit1bit0=0:现金帐户到达消费上限后禁止消费
								//bit1bit0=1:现金帐户到达消费上限后输密码继续消费现金帐户
								//bit1bit0=2:现金帐户到达消费上限后输密码继续消费补贴余额
								//bit1bit0=3:现金帐户到达消费上限后不输入密码继续消费补贴余
								//bit2=1:允许跨账户消费
								//bit2=0:不允许跨账户消费
								//bit3=0:表示优先消费补贴帐户金额
								//bit3=1:表示优先消费现金帐户余额

	int 			SubRemainMoney; 	//补贴余额
	unsigned short	SubUpLimitMoney;//补贴消费上限
	unsigned short	DiscountNum ;	//本时段在打折终端上消费次数
	unsigned char	SubUpLimitTimes;//补贴消费次数上限
	unsigned char	SubFreezeFlag;	//补贴冻结标志
	unsigned char	SubPowerId; //存折卡补贴余额身份类型 1~16
	unsigned char	SubPwdType; //补贴帐户到达消费上限后状态，
							//0表示到达消费上限后禁止消费
							//1表示到达消费上限后输密码继续消费补贴额
							//2表示到达消费上限后输密码继续消费现金帐户余额
							//3表示到达消费上限后不输入密码继续消费现金帐户余额
	int 		ConsumedLimitedMoney; 	//剩余允许本餐现金帐户已消费总消费金额
	unsigned short	SubConsumedLimitedMoney;	//剩余允许本餐补贴账户已消费补贴消费金额
	unsigned char	ConsumedLimitedTimes; 	//本餐现金帐户已消费剩余允许消费次数
	unsigned char	SubConsumedLimitedTimes;		//本餐补贴账户已消费剩余允许补贴消费次数

	unsigned short  OverDraft;	// 现金帐户透支额度，以元为单位,modified by duyy, 2013.6.17
	unsigned short  SubOverDraft;	//补贴帐户透支额度，以元为单位,modified by duyy, 2013.6.17
};

#endif

//#define DEBUG//2013.5.9
#ifdef pr_debug
#undef pr_debug
#endif
#ifdef DEBUG
#define pr_debug(fmt,arg...) \
	printk("kernel %s %d: "fmt,__FILE__,__LINE__,##arg)
#else
#define pr_debug(fmt,arg...) \
	do { } while (0)
#endif


#endif
