#ifndef _UART_H_
#define _UART_H_

// error type
#define NOTERMINAL -1
#define NOCOM -2
#define STATUSUNSET -3
#define ERRVERIFY	-4
#define ERRTERM		-5
#define ERRSEND		-6
#define TNORMAL 0

#define ONEBYTETM 382

#define SETFLOWTAIL 0xF1
#define SETFLOWHEAD 0xF5
#define SETACCMAINPTR 0xF2
#define SETACCSWPTR 0xF3
#define SETRECORD 0xF4
#define INITUART 0xAB
#define START_485 0xF6
#define SETREMAIN 0xF7

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

#define GETFLOWTAIL 0xE0
#define CLEARFLOWBUF 0xE1
#define GETFLOWTOTAL 0xE2

#define TPRT_REQ_CONF 0x46		// �ն˳����ϵ�ʱ���м���������Ʋ���
#define TPRT_SEND_CONF 0x64		// �м��������ն˵Ŀ��Ʋ���
#define TPRT_GET_FLOW 0x65		// �м������ն���Ҫ�¶���Ϣ�ͼ̵�������״̬
#define TPRT_RECV_FLOW 0x56		// �ն���ɿ��ƺ����м������ͽ��յ��Ŀ�������
								// �ͼ̵������״̬
#ifdef CONFIG_TPRT_NC		/* new control */
#define TPRT_SEND_CTRL 0x35		// �м������ն˷��ͼ̵���������Ϣ
#define TPRT_SEND_NUM 0x75		// �м������Դ�����ն����ڸ�ͨ�������ն˱��
#endif

#define CHANGETERMPAR 0x40		// �ı��ն˲���
#define	GETALLFLOW 0x41			// ��ȡȫ���ն���ˮ, Ҫ���û��ռ��С
#define	GETSIGLEFLOW 0x42		// ��ȡ�����ն���ˮ
#define	SETATERMCONF 0x43		// ����ȫ���ն˲���, �������������ն˿��л�ȡ
#define	SETSTERMCONF 0x44		// ���õ����ն˲���, �������û��ռ�õ�
#define	SETTERMCONF 0x45		// ���õ����ն˲���, �������������ն˿��л�ȡ
#define START_485_NEW 0x46
#ifdef CONFIG_TPRT_NC		/* new control */
#define SENDPWCTRL 0x47
#define GETTERMSTATUS 0x48
#define SETPOWERCTRL 0x49
#endif

// �ն˲���
typedef struct __tprt_param {
	unsigned char num;
	unsigned short val;		// �ն��趨���¶�ֵ
	unsigned char dif;		// �¶�����ƫ��ֵ
	unsigned char ctrl;		// ������
	unsigned char sival;	// �¶Ȳ���ʱ��������λ10�룩
	unsigned char cival;	// �̵�������ʱ��������λ10�룩
	unsigned char bak;
	int roomid;				// ����ID
} tprt_param;

struct uart_ptr {
	tprt_param *par;			// �ն˲�����
	int cnt;
};

#ifdef CONFIG_WENKONG_DEBUG
#undef pr_debug
#define pr_debug(fmt,arg...) \
	printk(KERN_INFO fmt,##arg)
#else
#undef DEBUG
#undef pr_debug
#define pr_debug(fmt,arg...) \
	do { } while (0)
#endif

#endif
