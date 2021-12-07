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

#define SETSPEED	0xA1
#define SETI2CDEV	0xA2
#define DSINTCN		0x1 << 2
#define DSALARM1	0x1 << 0
#define DSALARM2	0x1 << 1

#define FRAM_ADDR 0xA
#define DS_ADDR 0x68		// 1101000 7-bit

#define USRI2C_MAJOR 137

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

//enum i2cdev {ds1337, fram};
#endif

