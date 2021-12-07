#ifndef _UART_DEV_H_
#define _UART_DEV_H_

#include <linux/unistd.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm-arm/arch-at91rm9200/AT91RM9200_SYS.h>
#include <asm-arm/arch-at91rm9200/AT91RM9200_USART.h>
#include <asm/arch/pio.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/hardware.h>
#include <linux/rtc.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
/*
 * chanal enable: set PC12; chanal disable: clear PC12
 */	//AT91_SYS->PIOC_CODR &= (!0x00001000);
//#define ch_rd() AT91_SYS->PIOC_SODR |= AT91C_PIO_PC12

//#define ch_wr() AT91_SYS->PIOC_CODR |= AT91C_PIO_PC12
	//AT91_SYS->PIOC_SODR &= (!0x00001000);
	//;
extern short uart_poll(volatile unsigned int * reg, unsigned long bit);
extern int sel_ch(unsigned char machine_no);

extern int send_data(char *buf, size_t count, unsigned char num);
extern int send_byte(char buf, unsigned char num);
extern int send_addr(unsigned char buf, unsigned char num);
extern int recv_data(char *buf, unsigned char num);

extern AT91PS_USART uart_ctl0;
extern AT91PS_USART uart_ctl2;


#endif
