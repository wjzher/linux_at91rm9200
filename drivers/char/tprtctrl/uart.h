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

#define TPRT_REQ_CONF 0x46		// 终端初次上电时向中继器请求控制参数
#define TPRT_SEND_CONF 0x64		// 中继器下载终端的控制参数
#define TPRT_GET_FLOW 0x65		// 中继器向终端索要温度信息和继电器控制状态
#define TPRT_RECV_FLOW 0x56		// 终端完成控制后向中继器发送接收到的控制数据
								// 和继电器检测状态
#ifdef CONFIG_TPRT_NC		/* new control */
#define TPRT_SEND_CTRL 0x35		// 中继器向终端发送继电器控制信息
#define TPRT_SEND_NUM 0x75		// 中继器向电源控制终端现在该通道控制终端编号
#endif

#define CHANGETERMPAR 0x40		// 改变终端参数
#define	GETALLFLOW 0x41			// 收取全部终端流水, 要求用户空间大小
#define	GETSIGLEFLOW 0x42		// 收取单个终端流水
#define	SETATERMCONF 0x43		// 设置全部终端参数, 参数从驱动的终端库中获取
#define	SETSTERMCONF 0x44		// 设置单个终端参数, 参数从用户空间得到
#define	SETTERMCONF 0x45		// 设置单个终端参数, 参数从驱动的终端库中获取
#define START_485_NEW 0x46
#ifdef CONFIG_TPRT_NC		/* new control */
#define SENDPWCTRL 0x47
#define GETTERMSTATUS 0x48
#define SETPOWERCTRL 0x49
#endif

// 终端参数
typedef struct __tprt_param {
	unsigned char num;
	unsigned short val;		// 终端设定的温度值
	unsigned char dif;		// 温度允许偏差值
	unsigned char ctrl;		// 控制码
	unsigned char sival;	// 温度采样时间间隔（单位10秒）
	unsigned char cival;	// 继电器控制时间间隔（单位10秒）
	unsigned char bak;
	int roomid;				// 房间ID
} tprt_param;

struct uart_ptr {
	tprt_param *par;			// 终端参数表
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
