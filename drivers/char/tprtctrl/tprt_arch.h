#ifndef TPRT_ARCH_H
#define TPRT_ARCH_H
#include "power_cal.h"
// 终端数据包结构
#define TPCKMAX 32
typedef struct __term_pkg {
	unsigned char num;
	unsigned char cmd;
	unsigned char len;		// data的长度加校验
	unsigned char data[TPCKMAX];	// 此处用数组来代替指针
	unsigned char chk[2];
} __attribute__ ((packed)) term_pkg;

// flash中的信息
typedef struct __tprt_term {
	unsigned char num;
	unsigned short val;		// 终端设定的温度值
	unsigned char dif;		// 温度允许偏差值
	unsigned char ctrl;		// 控制码
	unsigned char sival;	// 温度采样时间间隔（单位10秒）
	unsigned char cival;	// 继电器控制时间间隔（单位10秒）
	unsigned char bak;
} __attribute__ ((packed)) tprt_term;

#ifdef CONFIG_TPRT_NC
// 电源控制参数
typedef struct tprt_power_param_s {
	unsigned char num;		// 终端号
	unsigned char base;		// 通道基数
	unsigned int tflag;		// 终端存在标识
	unsigned char bak;		// 保留
	unsigned char cival;	// 继电器控制时间间隔
} __attribute__ ((packed)) tprt_power_param;

typedef struct tprt_power_ctrl_s {
	unsigned char num;
	unsigned short ctrl;	// 控制码
} tprt_power_ctrl;

typedef struct term_status_s {
	unsigned char num;
	char isctrl;			// 控制码, 判断是否需要控制
	short gval;				// 目标温度
	unsigned short difval;			// 允许上下偏差
	unsigned short val;		// 温度AD采样值
	int flag;				// 标志是否为即时信息
	unsigned short ctrl;	// 当前状态
} term_status;

#endif
// 上传流水的信息
#if 0
typedef struct __tprt_flow {
	unsigned char termno;	// 终端号
	int roomid;				// 房间ID
	int val;			// 温度值
	unsigned char ctrl;	//
	unsigned char bak;
	usr_time date;		// 交易日期时间: year, mon, day, hour, min, sec (20 06 06 08 10 00 05)
} tprt_flow;
#endif

// 内存中终端的信息
typedef struct __tprt_ram {
	//struct list_head list;	// 双向链表
	tprt_term *pterm;		// 终端信息指针
	term_pkg *pkg;			// 数据包指针
	unsigned char termno;	// 终端号
	int roomid;				// 房间ID, 流水中需要这个信息
	int status;			// 通讯状态
	int term_cnt;		// 终端上传流水次数, 没有用到
	int val;			// 保存着最新温度值
	unsigned char ctrl;	// 实时的控制信息由终端提供
	// 时段外面做...
#ifdef CONFIG_TPRT_NC
	// 增加参数
	int is_power;		// 是否为电源控制板
	power_ctrl_knl *power;	// 保存电源控制数据, 如果是普通终端则保存所在电源板数据
	time_t flow_tm;		// 上一次温度正确的时间
	//tprt_ram *power;	// 属于哪个电源控制, !is_power
	//unsigned short s_ctrl;	// 需要发送的控制码
	//unsigned short r_ctrl;	// 收到的控制码, 终端流水赋值
#endif
} tprt_ram;

typedef struct __tprt_flow
{
	long		TerminalID;				//终端编号ID
	long		RoomID;			    //房间类型ID
	long	    Temperature;			//温度
	long	    TerminalStatus;		//终端通信状态
	long	    RelayStatus;			//控制信息
	char		FlowTime[20];				//温度改动时间
} tprt_flow;

typedef struct __net_rterm {
	long		TermID;				//终端编号ID
	long	    Temperature;		//温度，需要转换，除16即可得真实温度
	long	    TerminalStatus;		//终端通信状态
	long	    RelayStatus;		//控制信息
} net_rterm;

typedef struct __tprt_data {
	tprt_ram *pterm;			// 终端库
	tprt_term *par;
	int n;						// 现有的终端数
#if 0
	flow *pflow;				// 流水指针
	int maxflowno;				// 下一条流水号
	struct flow_info fptr;		// 缓冲区流水指针
	int flownum;				// 每次叫号后产生的流水总数	
#endif
} tprt_data;

#endif
