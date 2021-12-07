#ifndef TPRT_COM_H
#define TPRT_COM_H
#include "tprt_arch.h"

extern int tprt_recv_packet_aftercmd(term_pkg *pkg);
extern int tprt_rsend_conf(tprt_ram *ptrm);
extern int tprt_recv_flow(tprt_ram *ptrm);
extern int tprt_send_num(tprt_ram *ptrm, int flag);
extern int tprt_call(tprt_data *p);
extern int tprt_update_conf(tprt_ram *ptrm);
extern int tprt_req_data(tprt_ram *ptrm, tprt_flow *pflow);

extern int tprt_recv_packet_aftercmd(term_pkg *pkg);
#ifdef CONFIG_TPRT_NC
int tprt_send_power_ctrl(tprt_ram *ptrm, tprt_power_ctrl *ctrl);
int tprt_msend_ctrl(tprt_ram *ptrm, tprt_power_ctrl *ctrl);
int tprt_rsend_power_conf(tprt_ram *ptrm);
#endif
#include "at91_rtc.h"
static inline int _fill_flow_time(char *flowtime)
{
	int ret;
	struct rtc_time tm;
	ret = read_RTC_time(&tm, 0);
	if (ret < 0) {
		printk("_fill_flow_time: read_RTC_time error\n");
		return ret;
	}
	sprintf(flowtime, "%04d-%d-%d %d:%d:%d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	return 0;
}
#include <linux/time.h>
static inline time_t _get_current_time(void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_sec;
}

#endif
