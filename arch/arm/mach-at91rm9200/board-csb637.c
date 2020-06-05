/*
 * linux/arch/arm/mach-at91rm9200/board-csb637.c
 *
 *  Copyright (C) 2005 SAN People
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/device.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/hardware.h>
#include <asm/mach/serial_at91rm9200.h>
#include <asm/arch/board.h>

#include "generic.h"

static void __init csb637_init_irq(void)
{
	/* Initialize AIC controller */
	at91rm9200_init_irq(NULL);
	
	/* Set up the GPIO interrupts */
	at91_gpio_irq_setup(BGA_GPIO_BANKS);
}

/*
 * Serial port configuration.
 *    0 .. 3 = USART0 .. USART3
 *    4      = DBGU
 */
#define CSB637_UART_MAP		{ 4, 1, -1, -1, -1 }	/* ttyS0, ..., ttyS4 */
#define CSB637_SERIAL_CONSOLE	0			/* ttyS0 */

static void __init csb637_map_io(void)
{
	int serial[AT91C_NR_UART] = CSB637_UART_MAP;
	int i;

	at91rm9200_map_io();

	/* Initialize clocks */
	at91_main_clock = 184320000;
	at91_master_clock = 46080000;		/* peripheral clock (at91_main_clock / 4) */
	at91_pllb_clock = 0x128a3e19;		/* (3.6864 * (650+1) / 25) /2 = 47.9969 */

#ifdef CONFIG_SERIAL_AT91
	at91_console_port = CSB637_SERIAL_CONSOLE;
	memcpy(at91_serial_map, serial, sizeof(serial));

	/* Register UARTs */
	for (i = 0; i < AT91C_NR_UART; i++) {
		if (serial[i] >= 0)
			at91_register_uart(i, serial[i]);
	}
#endif
}

static struct at91_eth_data __initdata csb637_eth_data = {
	.phy_irq_pin	= AT91_PIN_PC2,
	.is_rmii	= 0,
};

static struct at91_usbh_data __initdata csb637_usbh_data = {
	.ports		= 2,
};

static struct at91_udc_data __initdata csb637_udc_data = {
	// FIXME provide pinouts
	//.vbus_pin     = AT91_PIN_PD4,
	//.pullup_pin   = AT91_PIN_PD5,
};

static void __init csb637_board_init(void)
{
	/* Ethernet */
	at91_add_device_eth(&csb637_eth_data);
	/* USB Host */
	at91_add_device_usbh(&csb637_usbh_data);
	/* USB Device */
//	at91_add_device_udc(&csb637_udc_data);
}

MACHINE_START(CSB637, "Cogent CSB637")
	MAINTAINER("SAN People")
	BOOT_MEM(AT91_SDRAM_BASE, AT91C_BASE_SYS, AT91C_VA_BASE_SYS)
	BOOT_PARAMS(AT91_SDRAM_BASE + 0x100)
	.timer	= &at91rm9200_timer,

	MAPIO(csb637_map_io)
	INITIRQ(csb637_init_irq)
	INIT_MACHINE(csb637_board_init)
MACHINE_END
