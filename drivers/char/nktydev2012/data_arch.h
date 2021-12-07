/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�data_arch.h
 * ժ    Ҫ�������õ������ݽṹ���485�������õ��Ĳ���������
 *			 �µ�POS���ն˿�ṹ��ı�, �����������ṹ��
 *			 ����term_ram�ṹ���Ա, ��Ϊ�ն˻��Ĳ������ñ�־
 * 			 ����FLASH���ʻ���ʽ, ����money, 2.2
 *			 ����һ��ͨV2֧��
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

#ifdef CONFIG_NKTYCPUCARD
#define CPUCARD
#endif
#include "dataflash.h"

#define SENDLEIDIFNO 0x10	// ���͹�翨�˻���Ϣ
#define SENDRUNDATA 0x20	// �����ϵ����
#define SENDRUNDATA2 0x23	// �����ϵ��������7�ֽ�ʱ��
#define RECVNO 0x40			// 
#define RECVLEHEXFLOW 0xF	// ���չ�翨��ˮ(HEX)
#define RECVLEBCDFLOW 0x2	// ���չ�翨��ˮ(BCD)
#define RECVGPFLOW 0x82		// ���շ�����ˮ
#define RECVTAKEFLOW 0x04	// ���չ�翨ȡ����ˮ
#define RECVDEPFLOW 0x08	// ���չ�翨�����ˮ
#if 0
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

#ifdef CONFIG_UART_V2
/* ��ʷ�������� */
#ifdef CONFIG_WATER
#define RECVWATSFLOW 0x84	// ��ȡˮ���ն˵�����ˮ
#define RECVWATAFLOW 0x85	// ���ն˵ĵ�����ˮ�ݴ����ڵ���ˮ���������ˮ��
#endif	/* CONFIG_WATER */
#define RECVREFUND 0x11		// �˿���ϴ���ˮ
#define SENDLEIDINFO2 0x12	// ���͹�翨�˻���Ϣ, �����˺ź�ʱ��
#define RECVLEHEXFLOW2 0x13	// �ϴ���翨��ˮ, ����ʱ��
#define RECVRFDLEID 0x15	// �ۿ�����͹�翨�˻���Ϣ, �޸�У��
#define RECVRFDDEPFLOW 0x16	// ���ճ��ɻ������ˮ, �޸�У��
#define RECVRFDLEFLOW 0x17	// �����˿��������ˮ, �޸�У��
#define RECVRFDLEFLOW2 0x18	// �������ѻ�������ˮ, �޸�У��
#define RECVRFDLEIDINFO 0x19	// �������ѻ��˻���Ϣ, �޸�У��
#define RECVLEIDINFO3 0x1A	// �������ѻ��˻���Ϣ, ����������
#define REFUNDBIT 0x80

// �����������
/* ������һ��ͨ������Ŀ */
#define SENDLEIDACCX 0x91	// ���͹�翨�˻���Ϣ, �����˺�˫У��
#define RECVDEPFLOWX 0x92	// ���չ�翨�����ˮ ˫У��
#define RECVTAKEFLOWX 0x93	// ���չ�翨ȡ����ˮ ˫У��
#define RECVLEFLOWX 0x94	// ���չ�翨��ˮ ˫У��

// �˴��д�СƱ���� ��ӡ����
#define SENDLEIDNAME 0x95	// ���ʹ��ۿ��˻���Ϣ����������ʱ���
#define RECVDEPFLOWT 0x96	// ���չ�翨�����ˮ ʱ�� ˫У��
#define RECVTAKEFLOWT 0x97	// ���չ�翨ȡ����ˮ ʱ�� ˫У��

/* ��һ�˻������������ */
#define SENDLEID_SINGLE 0x98
/* ˫�˻������������ */
#define SENDLEID_DOUBLE 0x99
/* ˫�˻���ˮ�������� */
#define RECVLEFLOW_DOUBLE 0x9A
#define RECVLEFLOW_SINGLE 0x9B

/* ͸֧����֧�� */
#define SENDLEID_ZLYY 0x9D
#define RECVLEFLOW_ZLYY 0xA0	/* fixed by wjzhe 20111128 */
#define SENDTIME_ZLYY 0x9C

#define SENDLEID_ZLYY2C 0x9E	/* ���ѻ����� Ҫ��server������� */
#define SENDLEID_ZLYYC 0x9F		/* ���ɻ����� */

/* ���ѱ�־ */
#define TRMF_DISC (1 << 1)		/* ���۱�־ */
#define TRMF_LIMIT (1 << 0)		/* �ʹα�־ */
#define TRMF_DISCF (1 << 8)		/* �����ն����ѱ�־ */
#define FLOWF_SPLIT (1 << 2)	/* ��ˮ��ֱ�־ */
#endif	/* CONFIG_UART_V2 */

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
 * FlowStatus	BYTE	1	HEX	״̬��0xFFδ�ϴ���0xFE���ϴ�
 * add cpu card support, modified by wjzhe, Sep 11 2009
 */
typedef struct __m1_flow {
	unsigned char flow_type;		// Bit0������ˮ��Bit1ʧ����ˮ��Bit2ǿ���۷���ˮ��Bit3-Bit7ΪǮ������
	long flow_num;					// ��ˮ��, ��UART��������
	int consume_sum;				// ���ѽ��-->���׽��, ��λ��
	usr_time date;					// ����ʱ��, ��...��
	long manage_fee;				// ����Ѳ��
	unsigned char tml_num;			// �ն˺�
	unsigned char areano;			// ����
	unsigned int card_num;			// ����, CPU��ΪASN
	unsigned int acc_num;					// �˺�
#ifndef CPUCARD
	unsigned short psam_num;		// PSAM�����
#else
	unsigned int psam_num;			// CPU PSAM���4-byte
#endif
	long psam_trd_num;		// PSAM���������
	unsigned short walt_num;		// Ǯ���������
	long remain_money;				// Ǯ�����
	unsigned char food_grp_num;		// ��Ʒ����
	unsigned char deal_type;		// ��������
#ifndef CPUCARD
	unsigned short tac_code;		// TAC code
	long money_sum;					// ���������ܶ�
#else
	unsigned int cpu_tac;			// cpu card tac code
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
	unsigned char ltype;		// �������ͱ�ʶ bit0 �������� bit 1������ˮ
	unsigned char is_sub;		// ��ʶ�Ƿ�Ϊ�����˻���ˮ
	unsigned char notused[12];
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


#ifdef CONFIG_UART_V2
// һ��ͨV2�汾�˻���
/* FLASH���˻��洢�ṹ */
typedef struct __account {
	unsigned int acc_num;			// account number
	unsigned int card_num;			// card number

	/* �����˻���Ϣ */
	int money;						// �����˻����
	unsigned char passwd[3];			// 3�ֽ�����(BCD)
	unsigned short money_limit;		// ������������(0~655.35)
	unsigned char times_limit;		// �������Ѵ�������(0~255)
	unsigned char power_id;			// �������
									// bit7������ʶ, ������ʹ��
									// bit6�������ֵ��ʶ
	unsigned char flag;				// ��־
									// bit 0: ��ʧ
									// bit 1: ע��
									// bit 2: �˻�����
									// bit 3: ��������
									// bit 7..4: �����id
	unsigned char st_limit;			// �����������޺��״̬
									// bit 0: �����ʻ������������޺���������������
									// bit 1: ���������������޺����������������
									// bit 2: ���������������޺��������Ѹ���
									// bit 3: ���������������޺����������Ѹ���
									// bit 4: ���������������޺���������
									// bit 5: ������˻�����
									// bit 6: �������Ѹ��˽��
									// bit 7: ����˫�˻�����
	
	/* �����˻���Ϣ */
	unsigned char sub_money[3];		// �������sub money
	unsigned short smoney_limit;	// ���Ͳ�����������
	unsigned char stimes_limit;		// ���ʹ�����������
	unsigned char spower_id;		// �����˻��������

	/* ʣ�����Ѷ�/���� */
	unsigned short money_life;		// �������ѽ��
	unsigned char times_life;		// �������Ѵ���
	unsigned short smoney_life;		// �������ѽ��
	unsigned char stimes_life;		// �������Ѵ���

	unsigned char dic_times;		// �������Ѵ���ͳ�����255

	/* ������͸֧, wjzhe 20111128 ��λԪ */
	short warning;				// �������
	unsigned short draft;		// ͸֧���

} __attribute__ ((packed)) /* account */acc_ram;



#define ACCF_LOSS (1 << 0)
#define ACCF_WOFF (1 << 1)
#define ACCF_FREEZE (1 << 2)
#define ACCF_FREEZE_SUB (1 << 3)
#define ACCF_MNGE_MASK (0xF << 4)

/* st_limit bit */
#define ACCST_PWDCON (1 << 0)		// �����������޺���������������
#define ACCST_PWDCON_SUB (1 << 1)	// �����������޺���������������
#define ACCST_PSNL_ALLOW (1 << 2)	// �����겹�������Ѹ���
#define ACCST_PSNL_PWD (1 << 3)		// �����������޺����������Ѹ���
#define ACCST_NO_PSNL (1 << 4)		// �����������޺���������
#define ACCST_ALLOW_UNIT (1 << 5)	// ������˻�����
#define ACCST_PERSON (1 << 6)		// ����ʹ�ø����˻�
#define ACCST_ALLOW_DOUBLE (1 << 7) // ����˫�˻�����

/* power_id bit */
#define ACCP_CHANGE (1 << 7)		// �Ƿ�Ϊ�����Ŀ�
#define ACCP_NOCASH (1 << 6)		// �������ֵ

//typedef account acc_ram;		// �����е��˻���Ϣ��FLASH�е�һ��
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
};
//�ն˽�ֹ����ʱ��ת�����ݽṹ��write by duyy, 2013.5.8
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
	unsigned int psam_trd_num;
#ifdef CONFIG_UART_V2
	/* �ն˽������ */
	int _tmoney;
	/* ��ǰ���ѱ�־ bit 0�ʹβ���, bit 1���� */
	unsigned int _con_flag;
#endif
#if 0
	int blkerr;				// ��ȡGA�������
	unsigned long jff_trdno;	// ���һ�θ��º�������ʱ��
#endif
#ifdef CONFIG_RTBLKCNTMODE		// ���ĺ���������
	int nocmdcnt;			// ����
#endif
} term_ram;

typedef struct __term_info {
	int term_no;
	int status;
	int money;	// ��������
	int flag;	// 0->���ѻ�, 1->���ɻ�
} term_info;


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

#ifdef CONFIG_UART_V2
/* ��������ṹ���� */

/* ������ͺ��ն����Ͷ�Ӧ�� */
typedef unsigned short term_allow;

/* ���۲����ṹ */
typedef struct __discount {
	u16 disc_val1;		// ��һ�δ��۱��� 0xFFFF��Ч��0xFFFE��ֹ����
	u16 const_val1;		// ��һ�ι̶���� 0xFFFF��Ч��0xFFFE��ֹ����
	u16 disc_val2;		// �ڶ��δ��۱��� 0xFFFF��Ч��0xFFFE��ֹ����
	u16 const_val2;		// �ڶ��ι̶���� 0xFFFF��Ч��0xFFFE��ֹ����
	u8 start_times;		// ˢ�����ɴκ�ʼ����
	u8 end_times1;		// ��һ�δ��۴��� 0��ʾ��Ч
	u8 end_times2;		// �ڶ��δ��۴��� 0��ʾ��Ч
	u8 reserved;		// ����
						// ��Ч��־ 0��ʾ��Ч 1��ʾ�����˻���Ч
						// 2��ʾ�����˻���Ч 3��ʾ����Ч
} discount;

/* ʱ�κʹ�����Ϣ���� */
typedef struct __user_tmsg {
	int id;						// ��� 1~4
	u32 begin;					// ��ʼʱ��
	u32 end;					// ����ʱ��
	u32 t_mask[8];		// �ն���������
	discount disc[16];			// 16����ݴ��۲���
} user_tmsg;

/* ��ݽ�ֹ����ʱ�� */
typedef struct __user_nallow {
	u32 begin[4];		// ��ʼʱ��, ����ĸ�ʱ��
	u32 end[4];			// ����ʱ��
} user_nallow;

/* ����������� */
typedef struct __user_cfg {
	user_tmsg tmsg[4];		// �ĸ�ʱ��
	user_nallow nallow[16];	// 16��ݲ���������ʱ��
	term_allow tallow[16];	// 16��ݶ�Ӧ16�ն�, ȫ����Ч
} user_cfg;

// ������ˮ�ṹ

// ���ֽ���ˮ�ṹ������ǵ���ע����ˮ�����ð������ṹ
typedef struct __NoMoneyFlowStruct
{
	int			OperAreaID;				//�������������
	unsigned int	OperationID;				//��������¼��
	unsigned int	OldCardId;					//������Ӧ����
	unsigned int	NewCardId;				//�����¿��� 
	int			BLTypeId;					//��������������
	int			Type;						//1�����ʧ��
											//2�����ң�
											//5�������� 
											//6�����˻�ע����
											//8���������
											//9�޸��˻�����
											//10�������ʻ�����; //write by duyy, 2013.5.8
											//11�������ʻ��ⶳ; //write by duyy, 2013.5.8
											//12�������ʻ�����;//write by duyy, 2013.5.8 
											//13�������ʻ��ⶳ//write by duyy, 2013.5.8
	char FlowTime[8];		// ����������ʱ�� YYYYMMDDhhnnss��7���ֽ����һ����0
} no_money_flow;

typedef struct __money_flow
{
	int  		AccountId; //�˻��˺�
	unsigned int 	CardNo; 	//����
	int   Money;	//���仯������λΪ��
	char  AccoType;	//������һ���˻������˫��0��ʾ������1��ʾ������
	char  LimitType;	//�������ͣ��Ƿ�������Ѵ������ѽ�0��ʾ�����룬1��ʾ���룩
	char  DisTermConsume;//�Ƿ��ڴ����ն�������
	char Reserved[1];	// ����
	unsigned int time;
} money_flow;

struct net_acc
{
	int	AccountId;			//�˺�
	unsigned int  CardId;		//����
	int 	RemainMoney;		//���۳�������޺�ģ�
	int 	Flag;				//��־����ʧ��͵�ͣ�
	int 	ManageId;			//������ѣ��������ڼ�������Ѵ�0��ʼ��
								//ͬʱ�ṩ�����ϵ���б�char[n]��
								//���λ��ʾ�Ƿ�������
	char 	Password[4];		//��������				
	int 	UpLimitMoney;		//����������
	unsigned char	UpLimitTime;	//�����Ѵ�������
	unsigned char	FreezeFlag; 	//�����־
	unsigned char	PowerId;			//���ۿ����������� 1~16
	unsigned char	PwdType;		//�������������Ƿ���������������
							//bit0=0��ʾ�����������޺󲻿���
							//bit0=1��ʾ�����������޺�������ɼ�������
							// bit 5 ��־�Ƿ�������˻�����
							// bit6 1 ��������˫�˻�
							// bit6 0 ֻ�������ѵ��˻�
							// bit7=0 ��ʾ�������Ѳ������
							// bit7=1 ��ʾ�������Ѹ������

	int 			SubRemainMoney; 	//�������
	unsigned short	SubUpLimitMoney;//������������
	unsigned short	DiscountNum ;	//��ʱ���ڴ����ն������Ѵ���
	unsigned char	SubUpLimitTimes;//�������Ѵ�������
	unsigned char	SubFreezeFlag;	//���������־
	unsigned char	SubPowerId; //���ۿ��������������� 1~16
	unsigned char	SubPwdType; //�����������޺�״̬��
							//0��ʾ�����������޺󲻿���
							//1��ʾ�����������޺�������������Ѳ�����
							//2��ʾ�����������޺�������������Ѹ������
							//3��ʾ�����������޺���������������Ѹ������
	int 		ConsumedLimitedMoney; 	//ʣ�������������������ѽ��
	unsigned short	SubConsumedLimitedMoney;	//ʣ�������Ͳ����˻������Ѳ������ѽ��
	unsigned char	ConsumedLimitedTimes; 	//����������ʣ���������Ѵ���
	unsigned char	SubConsumedLimitedTimes;		//���Ͳ����˻�������ʣ�����������Ѵ���
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
