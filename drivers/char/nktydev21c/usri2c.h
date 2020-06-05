#ifndef _USRI2C_H_
#define _USRI2C_H_

#include <asm/arch/hardware.h>
#include <asm/arch/pio.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12))
#define NEWSCK
#include <asm/arch/board.h>
#endif
#define SETFRAMADDR 0x5A

#define WR_FRAM    0x00520100//0x00500300
#define RD_FRAM  0x00521100
#define WR_TIME    0x00680100
#define RD_TIME  0x00681100
#define DSSTAREG	0xF
#define DSCTLREG	0xE
#define DSSECREG	0
#define DSMINREG	0x1
#define DSHOURREG	0x2
#define DSWDAYREG	0x3
#define DSMDAYREG	0x4
#define DSMONREG	0x5
#define DSYEARREG	0x6
#define DSA1SEC		0x7
#define DSA1MIN		0x8
#define DSA1HOUR	0x9
#define DSA1DAY		0xA
#define DSA2MIN		0xB
#define DSA3HOUR	0xC
#define DSA4DAY		0xD

#define SETDSTIME	0xB0
#define GETDSTIME	0xB1
#define SETDSALM1	0xB2
#define SETDSALM2	0xB3
#define STARTALM1	0xB4
#define STOPALM1	0xB5
#define CLEARINTA	0xB6
#define RDDSSTATUS	0xB7
#define RDOLDSTA	0xB8
#define SETCTLREG	0xB9
#define SETSTAREG	0xBA
#define GETDSALM1	0xBB
#define GETDSALM2	0xBC

#define ZLG7290_INIT 0xC0
#define ZLG7290_SHOW 0xC1
#define ZLG7290_FLASH 0xC2

#define ZLG7290_CMDBUF1 0x7
#define ZLG7290_CMDBUF2 0x8
#define ZLG7290_FLASHONOFF 0xC// 0x00最快闪烁
#define ZLG7290_SCANNUM 0xD//数码管数量
#define ZLG7290_DPRAM 0x10//--8 registers
/*
D7 D6 D5 D4 D3 D2 D1 D0 D7 D6 D5 D4 D3 D2 D1 D0
0  1  1  1  x  x  x  x  F7 F6 F5 F4 F3 F2 F1 F0
*/
#define ZLG7290_FLASHCMD 0x70// flash one byte
#define _7_SEGNUM 6
struct zlg_data {
	int num;	// 0..7
	int dp;		// 0 & 1
	int flash;	// 0 & 1
	unsigned char data;
};

#define SETSPEED	0xA1
#define SETI2CDEV	0xA2
#define DSINTCN		0x1 << 2
#define DSALARM1	0x1 << 0
#define DSALARM2	0x1 << 1

#define FRAM_ADDR 0xA
#define DS_ADDR 0x68		// 1101000 7-bit
#define ZLG7290_ADDR 0x38	// zlg7290 slave address
#define ZLG7290_CLK 32000

#define USRI2C_MAJOR 137
#define NR_I2C_DEVICES 3
#define AT91C_TWI_CLOCK		100000
#ifdef NEWSCK
#define AT91C_TWI_SCLOCK	(10 * at91_master_clock / AT91C_TWI_CLOCK)
#else
#define AT91C_TWI_SCLOCK	(10 * AT91C_MASTER_CLOCK / AT91C_TWI_CLOCK)
#endif
#define AT91C_TWI_CKDIV1	(2 << 16)	/* TWI clock divider.  NOTE: see Errata #22 */

#if (AT91C_TWI_SCLOCK % 10) >= 5
#define AT91C_TWI_CLDIV2 ((AT91C_TWI_SCLOCK / 10) - 5)
#else
#define AT91C_TWI_CLDIV2 ((AT91C_TWI_SCLOCK / 10) - 6)
#endif
#define AT91C_TWI_CLDIV3 ((AT91C_TWI_CLDIV2 + (4 - AT91C_TWI_CLDIV2 % 4)) >> 2)

#define BCD2BIN(val) (((val)&15) + ((val)>>4)*10)
#define BIN2BCD(val) ((((val)/10)<<4) + (val)%10)

struct dstime {//BIN
	unsigned char sec;		// 00 - 59
	unsigned char min;		// 00 - 59
	unsigned char hour;		// 00 - 23
	unsigned char wday;		//  1 - 7
	unsigned char mday;		// 01 - 31
	unsigned char mon;		// 01 - 12
	unsigned char year;		// 00 - 99
} __attribute__((packed));

struct framdata {
	unsigned int addr;		// internal addr 2-byte
	char *data;
	int size;
};

struct i2c_local {
	int err;				/* error value */
	unsigned short dev_addr;
	unsigned long addr;
	unsigned long size;
	int pio_enabled;
	unsigned long status;	/* status of i2c */
	char *data;
	int cnt;
};

//enum i2cdev {ds1337, fram};
#endif

