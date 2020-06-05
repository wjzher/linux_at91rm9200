#ifndef _DATAFLASH_H_
#define _DATAFLASH_H_

#define SETSPICLK 0xBAD

#define BYTEOFDFPAGE 1056

//DataFlash的存储结构宏定义
//0-8191 pages for all
#define FLOW_S 0			// flow start page
#define FLOW_E 4949		// flow end page
#define BADFLOW_S 4950
#define BADFLOW_E 5029
#define BLACK_S 5030
#define BLACK_E 5889
#define BLACK_A 860
#define ACCOUNT_S 5890	// account start page, 201520
#define ACCOUNT_E 8179	// account end page 2290 pages
#define TERM_S 8180		// 终端库十页
#define TERM_E 8189
#define FLASHTIME 8190	// 2 pages
#define FLASH_ALL 8192	// all page

#define ERASE_PAGE 0xEA
#define ERASE_BLOCK 0xEB
#define WAITREADY 0xEC
#define SETBAUD 0xEE
/*
 * read and write dataflash commands
 * submitted by wjzhe, 8/31/2006
 */
#define BUF1_WR 0x84			/* Buffer 1 Write */
#define BUF2_WR 0x87			/* Buffer 2 Write */
#define BUF1_TO_MP_BE 0x83		/* Buffer 1 to Main Memory Page Program with Built-in Erase */
#define BUF2_TO_MP_BE 0x86		/* Buffer 2 to Main Memory Page Program with Built-in Erase */
#define BUF1_TO_MP 0x88			/* Buffer 1 to Main Memory Page Program without Built-in Erase */
#define BUF2_TO_MP 0x89			/* Buffer 2 to Main Memory Page Program without Built-in Erase */
#define PAGE_ERASE 0x81			/* Page Erase */
#define BLOCK_ERASE 0x50		/* Block Erase */
#define MP_THROUGH_BUF1 0x82	/* Main Memory Page Program Through Buffer 1 */
#define MP_THROUGH_BUF2 0x85	/* Main Memory Page Program Through Buffer 2 */

#define BUF1_RD 0xD4			/* Buffer 1 Read */
#define BUF2_RD 0xD6			/* Buffer 2 Read */
#define STATUS_RD 0xD7			/* Status Register Read */
#define MAIN_PAGE_RD 0xD2		/* Main Memory Page Read */
#define SEQ_ARRAY_RD 0xE8		/* Continuous Array Read */
#define SEQ_ARRAY_RD_NEW 0x0B	/* Continuous Array Read new read command for at45db642d */
#define BST_ARRAY_RD 0xE9		/* Burst Array Read with Synchronous Delay */

#define MID_RD 0x9F				/* Manufacturer and Device ID Read */

/*
 * Additional Commands
 * submitted by wjzhe, 9/5/2006
 */
#define MP_TO_BUF1 0x53			/* Main Memory Page to Buffer 1 Transfer */
#define MP_TO_BUF2 0x55			/* Main Memory Page to Buffer 2 Transfer */
#define MP_COMP_BUF1 0x60		/* Main Memory Page to Buffer 1 Compare */
#define MP_COMP_BUF2 0x61		/* Main Memory Page to Buffer 2 Compare */
#define AUTO_RW_BUF1 0x58		/* Auto Page Rewrite Through Buffer 1 */
#define AUTO_RW_BUF2 0x59		/* Auto Page Rewrite Through Buffer 2 */

#define PHY_BUF_T 0x21F00000	/* Transfer buffer Address */
#define PHY_BUF_R 0x21F80000	/* Receive buffer Address*/
#define MAX_BUF 0x80000			/* Max. Buffer */

typedef unsigned int df_addr;	/* 24-bit address */

#define	OPCODE_N 1			// the num of opcode
#define ADDR_N 3			// the num of address
#define BUF_DC_N 1			// the num of buffer read don't care byte
#define DONTCARE_N 4		// the num of other don't care bytes
#define FLASH_RDY 1 << 7	// the device is not busy
#define NOT_SAME 1 << 6		// data do not match

/*#define ch_addr(high, low)		\
({								\
	unsigned int __high = (high), __low = (low);	\
	__low &= 0x7ff;				\
	if (__low >= 1056)			\
		__low -= 1056;			\
	__high = __high << 11;		\
	__high | __low;				\
})*/

#endif
