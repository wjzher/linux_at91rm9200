#ifndef TPRT_ARCH_H
#define TPRT_ARCH_H
#include "power_cal.h"
// �ն����ݰ��ṹ
#define TPCKMAX 32
typedef struct __term_pkg {
	unsigned char num;
	unsigned char cmd;
	unsigned char len;		// data�ĳ��ȼ�У��
	unsigned char data[TPCKMAX];	// �˴�������������ָ��
	unsigned char chk[2];
} __attribute__ ((packed)) term_pkg;

// flash�е���Ϣ
typedef struct __tprt_term {
	unsigned char num;
	unsigned short val;		// �ն��趨���¶�ֵ
	unsigned char dif;		// �¶�����ƫ��ֵ
	unsigned char ctrl;		// ������
	unsigned char sival;	// �¶Ȳ���ʱ��������λ10�룩
	unsigned char cival;	// �̵�������ʱ��������λ10�룩
	unsigned char bak;
} __attribute__ ((packed)) tprt_term;

#ifdef CONFIG_TPRT_NC
// ��Դ���Ʋ���
typedef struct tprt_power_param_s {
	unsigned char num;		// �ն˺�
	unsigned char base;		// ͨ������
	unsigned int tflag;		// �ն˴��ڱ�ʶ
	unsigned char bak;		// ����
	unsigned char cival;	// �̵�������ʱ����
} __attribute__ ((packed)) tprt_power_param;

typedef struct tprt_power_ctrl_s {
	unsigned char num;
	unsigned short ctrl;	// ������
} tprt_power_ctrl;

typedef struct term_status_s {
	unsigned char num;
	char isctrl;			// ������, �ж��Ƿ���Ҫ����
	short gval;				// Ŀ���¶�
	unsigned short difval;			// ��������ƫ��
	unsigned short val;		// �¶�AD����ֵ
	int flag;				// ��־�Ƿ�Ϊ��ʱ��Ϣ
	unsigned short ctrl;	// ��ǰ״̬
} term_status;

#endif
// �ϴ���ˮ����Ϣ
#if 0
typedef struct __tprt_flow {
	unsigned char termno;	// �ն˺�
	int roomid;				// ����ID
	int val;			// �¶�ֵ
	unsigned char ctrl;	//
	unsigned char bak;
	usr_time date;		// ��������ʱ��: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
} tprt_flow;
#endif

// �ڴ����ն˵���Ϣ
typedef struct __tprt_ram {
	//struct list_head list;	// ˫������
	tprt_term *pterm;		// �ն���Ϣָ��
	term_pkg *pkg;			// ���ݰ�ָ��
	unsigned char termno;	// �ն˺�
	int roomid;				// ����ID, ��ˮ����Ҫ�����Ϣ
	int status;			// ͨѶ״̬
	int term_cnt;		// �ն��ϴ���ˮ����, û���õ�
	int val;			// �����������¶�ֵ
	unsigned char ctrl;	// ʵʱ�Ŀ�����Ϣ���ն��ṩ
	// ʱ��������...
#ifdef CONFIG_TPRT_NC
	// ���Ӳ���
	int is_power;		// �Ƿ�Ϊ��Դ���ư�
	power_ctrl_knl *power;	// �����Դ��������, �������ͨ�ն��򱣴����ڵ�Դ������
	time_t flow_tm;		// ��һ���¶���ȷ��ʱ��
	//tprt_ram *power;	// �����ĸ���Դ����, !is_power
	//unsigned short s_ctrl;	// ��Ҫ���͵Ŀ�����
	//unsigned short r_ctrl;	// �յ��Ŀ�����, �ն���ˮ��ֵ
#endif
} tprt_ram;

typedef struct __tprt_flow
{
	long		TerminalID;				//�ն˱��ID
	long		RoomID;			    //��������ID
	long	    Temperature;			//�¶�
	long	    TerminalStatus;		//�ն�ͨ��״̬
	long	    RelayStatus;			//������Ϣ
	char		FlowTime[20];				//�¶ȸĶ�ʱ��
} tprt_flow;

typedef struct __net_rterm {
	long		TermID;				//�ն˱��ID
	long	    Temperature;		//�¶ȣ���Ҫת������16���ɵ���ʵ�¶�
	long	    TerminalStatus;		//�ն�ͨ��״̬
	long	    RelayStatus;		//������Ϣ
} net_rterm;

typedef struct __tprt_data {
	tprt_ram *pterm;			// �ն˿�
	tprt_term *par;
	int n;						// ���е��ն���
#if 0
	flow *pflow;				// ��ˮָ��
	int maxflowno;				// ��һ����ˮ��
	struct flow_info fptr;		// ��������ˮָ��
	int flownum;				// ÿ�νкź��������ˮ����	
#endif
} tprt_data;

#endif
