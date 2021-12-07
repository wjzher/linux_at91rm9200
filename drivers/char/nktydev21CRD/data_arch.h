/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�data_arch.h
 * ժ    Ҫ�������õ������ݽṹ���485�������õ��Ĳ���������
 *			 �µ�POS���ն˿�ṹ��ı�, �����������ṹ��
 *			 ����term_ram�ṹ���Ա, ��Ϊ�ն˻��Ĳ������ñ�־
 * 			 ����FLASH���ʻ���ʽ, ����money, 2.2
 * 			 
 * ��ǰ�汾��2.1
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��9��3��
 *
 * ��ǰ�汾��2.0
 * ԭ����  ��wjzhe
 * ������ڣ�2007��6��14��
 *
 * ȡ���汾��1.1 
 * ԭ����  ��wjzhe
 * ������ڣ�2007��4��11��
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

#define SENDLEIDIFNO 0x10	// ���͹�翨�˻���Ϣ
#ifdef QINGHUA
#define SENDLETICKET 0x1A	// ��СƱ, ���͹�翨�˻���Ϣ
#define RECVLETICKET 0x1B	// ��СƱ, ���չ�翨��ˮ
#endif
#define SENDRUNDATA 0x20	// �����ϵ����
#define RECVNO 0x40			// 
#define RECVLEHEXFLOW 0xF	// ���չ�翨��ˮ(HEX)
#define RECVLEBCDFLOW 0x2	// ���չ�翨��ˮ(BCD)
#define RECVGPFLOW 0x82		// ���շ�����ˮ
#define RECVTAKEFLOW 0x04	// ���չ�翨ȡ����ˮ
#define RECVDEPFLOW 0x08	// ���չ�翨�����ˮ
#ifdef CPUCARD
#define RECVCPUIDIFON 0x90	// ����CPU���˻���Ϣ
#define RECVCPUFLOW 0x86	// ����CPU����ˮ
#define RECVCPUFLOW1 0x84	// ����CPU����ˮ
#endif /* end CPUCARD */
#define SENDBLACK 0x52		// ���ͺ�����
#define RECVM1FLOW 0x51		// ����M1����Ǯ����ˮ
#define SENDTIME 0x50		// ����ϵͳ��ǰʱ��
#define SENDOFFPAR 0x53		// ����37 Bytes�ն˲���(�ѻ�)
#define RECVFLOWOVER 0x54	// �ն˻ش���ˮ���
#define NOCMD 0x80

#ifdef QINGHUA
#define TPLOCKBIT (1 << 4)	// ������ˮ����, ������ˮ��ʶ
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
#define LIMIT_MODE_FLAG 1         //  �ն����������ޱ�־λ
#define RAM_MODE_FLAG 2
#define ENTER_SECRET_FLAG 3         //�������־λ
#define ONE_PRICE_FLAG 4           // ��Ʒ�ַ�ʽ��־λ
#define CASH_TERMINAL_FLAG 5       // ���ɻ���־λ
#define MESS_TERMINAL_FLAG 6      //  �۷�����־λ 
#define REGISTER_TERMINAL_FLAG 7    // ע�����־λ

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
	unsigned char flow_type;		// ��ˮ����: 0X00: �ڿ�; 0X02: ��;�ο�; 0X03: Ǯ�����ݴ�; 0X05: �޷�д��; 0X10: �����; 0XFA: ��ֵ��¼; 0XFE: ������¼; 0X99: �������Ѽ�¼
	unsigned long flow_num;			// ������ˮ��
	long consume_sum;				// ���ѽ��
	usr_time date;					// ��������ʱ��: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
	long purse_money;				// Ǯ�����
	unsigned char tml_num;			// �ն˺�
	unsigned long card_num;			// �����к�
	unsigned long term_trade_num;	// �ն˽������
	unsigned char trade_type;		// �������ͱ�ʶ: 0x06
	unsigned char term_num[6];		// �ն˻����
	unsigned int tac_code;			// TAC code
	unsigned short purse_num;		// Ǯ���������
} __attribute__ ((packed)) cpu_flow;
#endif

/*
 * PosTime		CHAR	7	BCD	ʵʱʱ�䣺YYYYMMDDHHMMSS
 * AppCardID	DWORD	4	HEX	��Ƭ���кŻ򿨺�
 * AccountsNo	DWORD	4	HEX	�ʺ�
 * MoneySum		DWORD	4	HEX	��ǰ�ʹ������ܶ�
BakReg		BYTE	2	HEX	����չ
 * ManagMoney	BYTE	3	HEX	����Ѳ�� 
 * PosSetNo		WORD	2	HEX	����������ñ�� 
 * PsamNo		WORD	2	HEX	�������PSAM�����
 * PsamTrdNo	DWORD	4	HEX	�������PSAM���������
 * WalTrdNo		WORD	2	HEX	Ǯ���������
 * WalTrdMoney	DWORD	4	HEX	���׽���λΪ��
 * WalRemMoney	DWORD	4	HEX	Ǯ�����׺�����λ�Ƿ�
 * FoodGroupNo	BYTE	1	HEX	��Ʒ�����ţ�00��06
 * DealType		BYTE	1	HEX	�������ͱ�ʶ: ����Mifare one s50 ��Ƭ�滮���D�D�������ͱ���
 * FlowType		BYTE	1	HEX	��ˮ���ͱ�ʶ��Bit0������ˮ��Bit1ʧ����ˮ��Bit2ǿ���۷���ˮ��Bit3-Bit7ΪǮ������ 
 * TacCode		BYTE	2	HEX	TACУ����: TacCode1=PosTime +��+ FlowType TacCode2=PosTime^����^FlowType
FlowStatus	BYTE	1	HEX	״̬��0xFFδ�ϴ���0xFE���ϴ�
*/
typedef struct __m1_flow {
	unsigned char flow_type;		// Bit0������ˮ��Bit1ʧ����ˮ��
									// Bit2ǿ���۷���ˮ��Bit3-Bit7ΪǮ������
									// Bit3������ˮ
	long flow_num;					// ��ˮ��, ��UART��������
	long consume_sum;				// ���ѽ��-->���׽��, ��λ��
	usr_time date;					// ����ʱ��, ��...��
#ifndef QINGHUA
	long manage_fee;				// ����Ѳ��
#else
	short manage_fee;
#endif
	unsigned char tml_num;			// �ն˺�
	unsigned char areano;			// ����
	unsigned long card_num;			// ����
	long acc_num;					// �˺�
#ifdef QINGHUA
	unsigned char psam_num[3];
#else
	unsigned short psam_num;		// PSAM�����
#endif
	long psam_trd_num;		// PSAM���������
	unsigned short walt_num;		// Ǯ���������
	long remain_money;				// Ǯ�����
	unsigned char food_grp_num;		// ��Ʒ����
#ifndef QINGHUA
	unsigned char deal_type;		// ��������
#endif
#ifndef QINGHUA
	unsigned short tac_code;		// TAC code
#endif
#ifndef QINGHUA
	long money_sum;					// ���������ܶ�
#else
	unsigned int asn;				// �廪ȥ�������ܶ�, ��ASN
#endif
#ifdef QINGHUA
	int qh_tac;
#endif
} __attribute__ ((packed)) m1_flow;

/*
AppCardID	DWORD	4	HEX	���ۿ�����	
PosSetNo	WORD	2	HEX	��������	
PsamNo	WORD	2	HEX	�������PSAM�����	
PsamTrdNo	DWORD	4	HEX	�������PSAM���������	
CsmMny	DWORD	4	HEX	���׽���λΪ��            	
BakReg	BYTE	1	HEX	����չ	0X00
FlowType	BYTE	1	HEX	��ˮ���ͱ�ʶ��Bit0������ˮ��Bit1ʧ����ˮ��Bit2ǿ���۷���ˮ	
*/
typedef struct __le_flow {
	unsigned char flow_type;		// flow type: 0xAB, 0xAC, 0xAD---����, ��ֵ, ȡ��
	long flow_num;			// ������ˮ��
	long consume_sum;				// ���ѽ��
	usr_time date;					// ��������ʱ��: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
	long manage_fee;				// �����
	unsigned char tml_num;			// �ն˺�
	unsigned char areano;			// ����
	unsigned long card_num;			// ����
	long acc_num;			// �˻�
	unsigned short psam_num;		// PSAM�����
	unsigned long psam_trd_num;		// PSAM���������
	unsigned char notused[6];
	unsigned char food_grp_num;		// ������Ϣ
	unsigned char notused1[7];
} __attribute__ ((packed)) le_flow;

typedef struct __flow {				// 50-byte
	unsigned char flow_type;		// Bit0������ˮ��Bit1ʧ����ˮ��Bit2ǿ���۷���ˮ��Bit3-Bit7ΪǮ������
	long flow_num;					// ��ˮ��, ��UART��������
	long consume_sum;				// ���ѽ��-->���׽��, ��λ��
	usr_time date;					// ����ʱ��, ��...��
	long manage_fee;				// ����Ѳ��
	unsigned char tml_num;			// �ն˺�
	unsigned char areano;			// ����
	unsigned long card_num;			// ����
	long acc_num;					// �˺�
	unsigned short psam_num;		// PSAM�����
	unsigned long psam_trd_num;		// PSAM���������
	unsigned short walt_num;		// Ǯ���������
	long remain_money;				// Ǯ�����
	unsigned char food_grp_num;		// ��Ʒ����
	unsigned char deal_type;		// ��������
	unsigned short tac_code;		// TAC code
	long money_sum;					// ��ǰ�ʹ������ܶ�
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
	unsigned char id_type;			// ID type��16 ��,added by duyy, 2013.7.1
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
	unsigned char blkuptm;			// max time of update black list	������=0xFF
	unsigned short purse_time;		// Ǯ����С���Ѽ��		����=0
	unsigned char spcl;				// bit0~2=�ʹ���, bit3����
	unsigned char psam_passwd[3];	// psam password
	unsigned char term_passwd[3];	// terminal password
	unsigned char max_consume;		// max consume
	unsigned char bakreg[2];		// ����
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
	unsigned char id_type;			// ID type��16 ��,added by duyy, 2013.7.1 
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
	unsigned long card_num;		// ����
	struct tradenum t_num;				// ��¼��
	__s8 opt;					// 1-->��ʧ, 0-->���
} __attribute__ ((packed)) black_acc;

struct black_info {// 24�ֽ�
	int edition;
	struct tradenum trade_num;
	int count;
#if 0 /* ndef PSAM_LIST */
	unsigned char pwd[3];	// 3�ֽ�PSAM������
#endif
};
//�ն˽�ֹ����ʱ��ת�����ݽṹ��write by duyy, 2012.4.26
typedef struct __term_tmsg {
	u32 begin;					// ��ʼʱ��
	u32 end;					// ����ʱ��
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
	unsigned char power_id;		// �����ն�
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
	unsigned char dif_verify;// ���е�У�鶼ֻ�Ǳ������ݵ�У��
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
	int money;	// ��������
	int flag;	// 0->���ѻ�, 1->���ɻ�
} term_info;

#if 0
typedef struct {
	int				OperAreaID;					//�������������
	unsigned long	OperationID;				//��������¼��
	unsigned long	OldCardId;					//������Ӧ����
	int			BLTypeId;					//��������������
	unsigned long	NewCardId;					//�����Ŀ���
	int				Type;	//1�����ʧ��2�����ң�3������͵�ͣ�4�����͵�ͣ�5��������6�����˻�ע����7������ע�᣻8���������
	long			AccountId;					//����ע����˺�
	int				RemainMoney;				//����ע���˻����
	int				Flag;						////����ע���˻�״̬
	int				UpLimitMoney;				//����ע���˻���������
	int				ManageId;					//����ע���˻������ϵ��
	int				PowerId;					//����ע���˻��������
	char        FlowTime[20];					//��������ˮ�ṹ
} no_money_flow;
typedef struct __money_flow
{
	long  AccountId; //�˻�����
	unsigned long CardNo; 
	int   Money; //���仯������λΪ�� 
} money_flow;
#endif

struct uart_mon {
	int cnt;
	void *data;
};

#define FLOWPERPAGE (BYTEOFDFPAGE / sizeof(flow))
#ifdef CONFIG_RECORD_CASHTERM
// �����¼���ɻ�״̬�ı���
struct cash_status {
	unsigned char termno;		// �ն˺�
	unsigned char status;		// ��������״̬
	unsigned char feature;		// ���ʺű�ʶ
	unsigned int accno;			// �ʺ�
	unsigned int cardno;		// ����
	int money;					// ���
	int consume;				// �˴����Ѷ�
	int managefee;				// �����
	int term_money;				// ���ɻ���ǰ�ܽ��
};
#define CASH_NORMAL 0		// ͨ��״̬
#define CASH_CARDIN 1		// ������
#define CASH_CARDOFF 2		// ������
#define CASH_TAKEOFF 3		// ���ȡǮ
#define CASH_DEPOFF 4		// ��ɴ�Ǯ
#define CASHBUFSZ 16
extern const int cashterm_n;
extern int cashterm_ptr;				// �洢ָ��
extern struct cash_status cashbuf[CASHBUFSZ];	// ����״̬�ռ�
#endif

#if 0
typedef struct {
	unsigned short psamno;				//PSAM�����
	char		   pwd[3];			//PSAM������
} psam_local;

struct psam_info {
	psam_local *psam;
	int cnt;
};
#endif
#endif
