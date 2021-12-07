#ifndef _UART_H_
#define _UART_H_

#define FLOWANUM 300	// Ӧ�ó��������õ�, ע�����

#define BADFLOWNUM 200
#define MAXSWACC 10000

#define SETPRONUM 0x11 //����A2�ն��������к�
#define SETFLOWTAIL 0xF1
#define SETFLOWHEAD 0xF5
#define SETACCMAINPTR 0xF2
#define SETACCSWPTR 0xF3
#define SETRECORD 0xF4
#define INITUART 0xAB
#define START_485 0xF6
#define SETREMAIN 0xF7
#ifdef EDUREQ
#define SETEDUPAR 0xF8
#endif
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
#define SETNETSTATUS 0x35
#define SETLEALLOW 0x3B

#ifdef CONFIG_RECORD_CASHTERM
#define GETCASHBUF 0x3C
#endif
// �������ػ�ʱ��ȡ����ˮ����ˮ, �����ˮ���ն���ʲôҲ����
#define GETWATERFLOW 0x3D
#define GETTERMCNT 0x42

#ifdef CONFIG_CNTVER
#define SETCNTVER 0x3E
#define GETFCNTVER 0x3F
//#define GETCNTFLOW 0x40
#define SETCNTFLOW 0x41
#endif

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
};
struct get_flow {
	flow *pflow;
	int cnt;
};

struct bad_flow {
	int num;
	int total;
	flow *pbadflow;// ��������ˮ������, ÿ�νкŽ�����Ӧ�ó���ȡ��
};

extern int recv_data(char *buf, unsigned char num);
extern int send_data(char *buf, size_t count, unsigned char num);	// attention: can continue sending?
extern int send_byte(char buf, unsigned char num);
extern int send_addr(unsigned char buf, unsigned char num);

#endif
