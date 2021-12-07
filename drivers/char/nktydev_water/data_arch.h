/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * �ļ����ƣ�data_arch.h
 * ժ    Ҫ�������õ������ݽṹ���485�������õ��Ĳ���������
 * 			 
 * ��ǰ�汾��1.1
 * ��    �ߣ�wjzhe
 * ������ڣ�2007��4��11��
 *
 * ȡ���汾��1.0 
 * ԭ����  ��wjzhe
 * ������ڣ�2007��2��13��
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

#define SENDLEIDIFNO 0x10	// ���͹�翨�˻���Ϣ
#define SENDRUNDATA 0x20	// �����ϵ����
#define SENDRUNDATA2 0x23	// �����ϵ��������7�ֽ�ʱ��
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
#ifdef CONFIG_QH_WATER
#define SENDSYSTIME 0x30
#endif
#ifdef WATER
#define RECVWATSFLOW 0x84	// ��ȡˮ���ն˵�����ˮ
#define RECVWATAFLOW 0x85	// ���ն˵ĵ�����ˮ�ݴ����ڵ���ˮ���������ˮ��
#endif
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
#define LIMIT_MODE_FLAG 1         //  �ն����������ޱ�־λ
#define RAM_MODE_FLAG 2
#define ENTER_SECRET_FLAG 3         //�������־λ
#define ONE_PRICE_FLAG 4           // ��Ʒ�ַ�ʽ��־λ
#define CASH_TERMINAL_FLAG 5       // ���ɻ���־λ
#define MESS_TERMINAL_FLAG 6      //  �۷�����־λ 
#define REGISTER_TERMINAL_FLAG 7    // ע�����־λ

#define ACCOUNTCLASS 4
#define ACCOUNTFLAG 6
// �����ֹ���λ
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
Date1	String[8]		����	4	BCD	YYYYMMDD	
Time1	String[6]		ʱ��	3	BCD	hhmmss	
ApplySerialNo	String[20]		��ƬӦ�����кţ�ID��	4 (10)	BCD		
Times	Word		Ǯ���������	2	Hex	�޷�������
0~65535	
TransMoney	Dword		���׽���λ�Ƿ�	(��2�ֽ���Ч����2�ֽ�Ϊ0)	4	Hex	�޷�������0~4294967295	
Dealtype	Byte		�������ͱ�ʶ��	00�CEP���ѡ�01�C��ֵ��02�C�ۿ03�C�����	1	Hex	6	
POSNo	Byte		�ն˻����	1(4)	HEX
Invoiceno	Dword		�ն˽������	4	Hex	�޷�������0~4294967295	
LastMoney	Dword		Ǯ�����׺�����λ�Ƿ�	4	Hex	�޷�������0~4294967295	
TACMark	String[4]		TAC��������֤�룩	2(4)	Hex		
DealCode1	String[2]		0X99���������Ѽ�¼	�������������б�	1	Hex		
ProcessCode	Byte	�߰��ֽ�Ϊ�����̲���Ͱ��ֽ�Ϊ�����̲���	1	Hex		
Uploaded	Byte	0xFF: ����ˮδ�ϴ� 0x00�����ϴ�	1	Hex		
�ϼ�			32			
*/
typedef struct __m1_flow {
	unsigned char flow_type;		// ��ˮ����: 0X00: �ڿ�; 0X02: ��;�ο�;
									// 0X03: Ǯ�����ݴ�; 0X05: �޷�д��;
									// 0X10: �����; 0XFA: ��ֵ��¼;
									// 0XFE: ������¼; 0X99: �������Ѽ�¼
	long flow_num;					// ������ˮ��
	long consume_sum;				// ���ѽ��
		 // ��������Ϊ��2�ֽ���Ч, ��2�ֽ�Ϊ0
	usr_time date;					// ����ʱ��, BCD
	long manage_fee;				// �����
	unsigned char term_num;			// �ն˻����
	unsigned long card_num;			// ��ƬӦ�����к�, BCD
	long acc_num;			// �˺�
	unsigned long term_trade_num;	// �ն˽������
	unsigned char trade_type;		// �������ͱ�ʶ: 0x06 00 �CEP���� 
									// 01 �C��ֵ 02 �C�ۿ� 03 �C�����
	unsigned short purse_num;		// Ǯ���������
	long purse_money;				// Ǯ�����
		 // ��������Ϊ�޷�����, ��ȡ����ʱҪע��
	unsigned char notused[2];
} __attribute__ ((packed)) m1_flow;

typedef struct __le_flow {
	unsigned char flow_type;		// flow type: 0xAB, 0xAC, 0xAD---����, ��ֵ, ȡ��
	long flow_num;					// ������ˮ��
	long consume_sum;				// ���ѽ��
	usr_time date;					// ��������ʱ��: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
	long manage_fee;				// �����
	unsigned char tml_num;			// �ն˺�
	unsigned long card_num;			// ����
	long acc_num;			// �˻�
	unsigned char notused[13];
} __attribute__ ((packed)) le_flow;

typedef struct __flow {				// 42-byte
	unsigned char flow_type;		// flow type: 0x99 right; 0x0E error; 0xAB le flow
	long flow_num;					// number of flow
	long consume_sum;				// money of consume
	usr_time date;					// date: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
	long manage_fee;				// �����
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

/* �����в�ʹ�ô˽ṹ */
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
	unsigned char power_id;		// �����ն�
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

typedef struct __acc_ram {		// Ӧ�ó�����ں˹���
	unsigned long acc_num;
	unsigned long card_num;			// card number
	long money;			// money
	unsigned char feature;			// flag, figure, limit of meal
	int managefee;					// 0-15
#if defined (EDUREQ)
	int nflag;
#endif
#ifdef CONFIG_QH_WATER		// �廪ˮ�ؽṹ
	long sub_money;			// �������
	int power_id;				// ���(0-7), �廪ˮ����չ���
#endif
#if defined (USRCFG1)
	int nflag[4];		// uchar������
	int itm;
	// ����4��ʱ���ۿ۴���
	int ndisc[4];		// �ۿ۴���
#endif
#if defined (CONFIG_CNTVER)
	short cnt[2];					// ����������ͳ��
	unsigned short page;		// ���˻���Ϣ��flash�е�λ��
	int itm[2];					// ÿ�����Ѵ���
#endif

#ifdef CONFIG_SUBCNT
	int sub_cnt;	// ʣ�����
	int sub_flag;	// ����ͳ��
#endif

#ifdef CONFIG_RDFZ
	char user_name[12];		// �����û�����
#endif
} /*__attribute__ ((packed))*/ acc_ram;

#ifdef EDUREQ
#define ETERMNUM 256
struct edupar {
	int figure;	// ���1~4
	int cn;		// ���Ѵ���, ��cn����ʼ�ӱ�
	int rate;	// ���ʱ���
	int stime1;	// ��ʼʱ��1
	int etime1;	// ����ʱ��1
	int stime2;	// ��ʼʱ��2
	int etime2;	// ����ʱ��2
};
#endif

#if 0
typedef struct __black_acc {
	unsigned long card_num;		// ����
	long rec_num;				// ��¼��
	__s8 opt;					// 1-->��ʧ, 0-->���
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
		int sub_flag;	// ������ˮ��־
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
	int money;	// ��������
	int flag;	// 0->���ѻ�, 1->���ɻ�
} term_info;

typedef struct __NoMoneyFlowStruct
{
	int    OperAreaID;     //�������������
	unsigned long OperationID;    //��������¼��
	unsigned long OldCardId;     //������Ӧ����
	int   BLTypeId;     //��������������
	unsigned long NewCardId;     //�����Ŀ���
	int    Type;
	// 1�����ʧ��2�����ң�
	// 3������͵�ͣ�4�����͵�ͣ�
	// 5��������6�����˻�ע����
	// 7������ע�᣻8���������
	long   AccountId;     //����ע����˺�
	int    RemainMoney;    //����ע���˻����
	int    Flag;      ////����ע���˻�״̬
#ifdef CONFIG_QH_WATER
	unsigned short sub_money;
	unsigned short UpLimitMoney;
#else
	int    UpLimitMoney;    //����ע���˻���������
#endif
	int    ManageId;     //����ע���˻������ϵ��
	int    PowerId;     //����ע���˻��������
} no_money_flow;

typedef struct __money_flow
{
	long  AccountId; //�˻�����
	unsigned long CardNo; 
	int   Money; //���仯������λΪ�� 
} money_flow;

#ifdef S_BID
/* 2008-3-13 from RD new */
struct bid_info {
	int tms1;	// ʱ�俪ʼ
	int tme1;	// ʱ�����
	int tms2;
	int tme2;
	int tms3;
	int tme3;
	int tms4;
	int tme4;
	char figure[3];	// ���
	char power_id[3];// 4���ն�
};
#define SETBIDINFO 0x36
#endif

#ifdef USRCFG1
struct usr_tmsg {
	int no;
	int begin;			// ��ʼʱ��
	int end;			// ����ʱ��
	int limit[4];			// ���ƴ���
	int money[4];		// �̶����
	int imoney;		// �ն˻�����Ľ��
	unsigned int tnot[8];		// 8��ͨ��, 32-bit
};
struct usr_allow {
	int figure;		// ���--1234,0�ǽ�����־
	int begin;		// ��ʼʱ��
	int end;		// ����ʱ��
	int allow;		// �Ƿ�����
};
struct usr_cfg {
	struct usr_tmsg usrtm[4];		// ��������4����������ʱ��
	struct usr_allow usrlw[16];
	char flag;	// ������־, ��������֮���Ƿ�������, by wjzhe, 20091208
	char mflag;	// ������־����־�Ƿ�ƽ��
	char lflag;	// ������־����־�Ƿ����ܲʹ�
	char sflag;	// ������־����־�Ƿ�ͳ��������ˮ���޲ʹ�
	/* unsigned char */int discount[4][4];		// ��������ۿ�, 301�����ĸ��ʹ�
	// ���Ӵ��۴�������
	short discn[4][4];	// [0]->�ʹ�1ʱ������ݴ���
	// ���ӱ�־�����۴���Ϊ������discn��֮ǰ�ǲ����ۣ�֮����ۣ��ۿۿɴ���100
	char dnflag;		// ��0Ϊ��
};
#define SETUSRCFG 0x37
#define SETTERMONEY 0x1001
// �����ն˽������16*4
struct term_money {
	int tm1[4];	// ʱ��1����
	int tm2[4];
	int tm3[4];
	int tm4[4];
};

#endif

#ifdef CONFIG_CNTVER
struct tmsgcfg {
	int start;		// ÿ�Ϳ�ʼʱ��
	int end;		// ÿ�ͽ���ʱ��
	short cnt[4];	// �������
	int itm;		// �ն��������Ѵ���
};
struct cnt_conf {
	int start;		// ��ʼʱ��
	int end;		// ����ʱ��
	unsigned int tnot[8];	// ������ն�
	struct tmsgcfg tcfg[2];	// ʱ��
	//char ip[16];	// ��һ������IP
};

struct f_cntver {
	unsigned int acc_num;
	unsigned int card_num;
	unsigned int time;
	short remain[2];
} __attribute__ ((packed)) ;

struct cntflow {
	unsigned int flowno;
	unsigned int card_num;	// ������Ϊ��ˮ
	unsigned char ctm;				// ���ڱ�ʶ�ĸ�ʱ�β�������ˮ
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
#endif
