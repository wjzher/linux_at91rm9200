/*
 * Copyright (c) 2007, NKTY Company
 * All rights reserved.
 * 文件名称：at91_oldrtc.c
 * 摘    要：从以前的驱动中分离出来
 * 			 
 * 当前版本：1.0.0
 * 作    者：wjzhe
 * 完成日期：2008年4月29日
 */
#include "at91_rtc.h"
#include <asm/uaccess.h>

/*
 * Set RTC time
 * when flag = 1, use copy_from_user
 * if success, return 0, else return error number
 * version 1.0
 */
int set_RTC_time(struct rtc_time *tm, int flag)
{
	int ret = 0;
	struct rtc_time m_tm;
	int cpy = 1;
	if (flag)
		cpy = copy_from_user(&m_tm, tm, sizeof(m_tm));
	else {
		m_tm = *tm;
		cpy = 0;
	}
	/*printk("Set RTC date/time is %d-%d-%d. %02d:%02d:%02d\n",
                m_tm.tm_mday, m_tm.tm_mon, m_tm.tm_year + 1900,
                m_tm.tm_hour, m_tm.tm_min, m_tm.tm_sec);*/
	if (cpy)
		ret = -EFAULT;
	else {
		int m_year = m_tm.tm_year + 1900;
		if (m_year < EPOCH
			|| (unsigned) m_tm.tm_mon >= 12
			|| m_tm.tm_mday <1
			|| m_tm.tm_mday > (days_in_mo[m_tm.tm_mon] + (m_tm.tm_mon == 1 && is_leap(m_year)))
			|| (unsigned) m_tm.tm_hour >= 24
			|| (unsigned) m_tm.tm_min >= 60
			|| (unsigned) m_tm.tm_sec >= 60)
			ret = -EINVAL;
		else
		{
			AT91_SYS->RTC_CR |= (AT91C_RTC_UPDCAL | AT91C_RTC_UPDTIM);	//set register -> can update time and date
			while ((AT91_SYS->RTC_SR & 0x01) != 0x01);					//wait ACKUPD
			AT91_SYS->RTC_TIMR = BIN2BCD(m_tm.tm_sec) << 0 | BIN2BCD(m_tm.tm_min) << 8	//update time
				| BIN2BCD(m_tm.tm_hour) << 16;
			AT91_SYS->RTC_CALR = BIN2BCD((m_tm.tm_year + 1900) / 100)					//update date
				| BIN2BCD(m_tm.tm_year % 100) << 8
				| BIN2BCD(m_tm.tm_mon + 1) << 16
				| BIN2BCD(m_tm.tm_wday) << 21				/* day of the week [1-7], Sunday=7 */
				| BIN2BCD(m_tm.tm_mday) << 24;
			/* Restart Time/Calendar */
			AT91_SYS->RTC_CR &= ~(AT91C_RTC_UPDCAL | AT91C_RTC_UPDTIM);		//close update application
			AT91_SYS->RTC_SCCR |= AT91C_RTC_ACKUPD;							//clear ACKUPD
		}
	}
	return ret;
}
/*
 * Read RTC time
 * when flag = 1, use copy_to_user
 * if success, return 0, else return error number
 * version 1.0
 */
int read_RTC_time(struct rtc_time *tm, int flag)
{
	int ret = 0;
	struct rtc_time m_tm;
	unsigned int time, date;
	memset(&m_tm, 0, sizeof(m_tm));
	// 连续读两次, 保证没有秒变化
	do {
		time = AT91_SYS->RTC_TIMR;
		date = AT91_SYS->RTC_CALR;
	} while ((time != AT91_SYS->RTC_TIMR) || (date != AT91_SYS->RTC_CALR));
	m_tm.tm_sec = BCD2BIN((time & AT91C_RTC_SEC) >> 0);			// second
	m_tm.tm_min = BCD2BIN((time & AT91C_RTC_MIN) >> 8);			// minute
	m_tm.tm_hour = BCD2BIN((time & AT91C_RTC_HOUR) >> 16);		// hour
	m_tm.tm_year = BCD2BIN(date & AT91C_RTC_CENT) * 100;		/* century */
	m_tm.tm_year += BCD2BIN((date & AT91C_RTC_YEAR) >> 8);		/* year */
	m_tm.tm_wday = BCD2BIN((date & AT91C_RTC_DAY) >> 21);	/* day of the week [1-7], Sunday=7 */
	m_tm.tm_mon = BCD2BIN((date & AT91C_RTC_MONTH) >> 16) - 1;
	m_tm.tm_mday = BCD2BIN((date & AT91C_RTC_DATE) >> 24);
	m_tm.tm_yday = __mon_yday[is_leap(m_tm.tm_year)][m_tm.tm_mon] + m_tm.tm_mday-1;
	m_tm.tm_year = m_tm.tm_year - 1900;
	if (flag)
		ret = copy_to_user(tm, &m_tm, sizeof(m_tm)) ? -EFAULT : 0;
	else {
		*tm = m_tm;
		ret = 0;
	}
	//printk("RTC_SR = 0x%08x\n", AT91_SYS->RTC_SR);
	return ret;
}
/*
 * read time only second event happend
 */
int read_time(unsigned char *tm)
{
	unsigned long time, date;
	//if ((AT91_SYS->RTC_SR & AT91C_RTC_SECEV) != AT91C_RTC_SECEV)	//no second event return ret
	//	return 0;
#ifdef DBGCALL
	int i = 0;
#endif
	do {
		time = AT91_SYS->RTC_TIMR;
		date = AT91_SYS->RTC_CALR;
#ifdef DBGCALL
		i++;
#endif
	} while ((time != AT91_SYS->RTC_TIMR) || (date != AT91_SYS->RTC_CALR));
	AT91_SYS->RTC_SCCR |= AT91C_RTC_SECEV;			//clear second envent flag
#ifdef DBGCALL
	if (i > 1000) {
		printk("read time over 1000\n");
	}
#endif
	*tm++ = (date >> 8)		& 0xff;		// 年
	*tm++ = (date >> 16)	& 0x1f;		// 月
	*tm++ = (date >> 24)	& 0x3f;		// 日
	*tm++ = (time >> 16)	& 0x3f;		// 时
	*tm++ = (time >> 8)		& 0x7f;		// 分
	*tm	  = time			& 0x7f;		// 秒
	return 0;
}

