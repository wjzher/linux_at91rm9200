/*
 * linux/arch/arm/mach-at91rm9200/time.c
 *
 *  Copyright (C) 2003 SAN People
 *  Copyright (C) 2003 ATMEL
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
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/time.h>

/*
 * The ST_CRTR is updated asynchronously to the master clock.  It is therefore
 *  necessary to read it twice (with the same value) to ensure accuracy.
 */
static inline unsigned long read_CRTR(void) {
	unsigned long x1, x2;

	do {
		x1 = AT91_SYS->ST_CRTR;
		x2 = AT91_SYS->ST_CRTR;
	} while (x1 != x2);

	return x1;
}

/*
 * Returns number of microseconds since last timer interrupt.  Note that interrupts
 * will have been disabled by do_gettimeofday()
 *  'LATCH' is hwclock ticks (see CLOCK_TICK_RATE in timex.h) per jiffy.
 *  'tick' is usecs per jiffy (linux/timex.h).
 */
static unsigned long at91rm9200_gettimeoffset(void)
{
	unsigned long elapsed;

	elapsed = (read_CRTR() - AT91_SYS->ST_RTAR) & AT91C_ST_ALMV;

	return (unsigned long)(elapsed * (tick_nsec / 1000)) / LATCH;
}

/*
 * IRQ handler for the timer.
 */
static irqreturn_t at91rm9200_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	if (AT91_SYS->ST_SR & AT91C_ST_PITS) {	/* This is a shared interrupt */
		write_seqlock(&xtime_lock);

		do {
			timer_tick(regs);
			AT91_SYS->ST_RTAR = (AT91_SYS->ST_RTAR + LATCH) & AT91C_ST_ALMV;
		} while (((read_CRTR() - AT91_SYS->ST_RTAR) & AT91C_ST_ALMV) >= LATCH);

		write_sequnlock(&xtime_lock);

		return IRQ_HANDLED;
	}
	else
		return IRQ_NONE;		/* not handled */
}

static struct irqaction at91rm9200_timer_irq = {
	.name		= "AT91RM9200 Timer Tick",
	.flags		= SA_SHIRQ | SA_INTERRUPT,
	.handler	= at91rm9200_timer_interrupt
};

/*
 * Set up timer interrupt.
 */
void __init at91rm9200_timer_init(void)
{
	/* Disable all timer interrupts */
	AT91_SYS->ST_IDR = AT91C_ST_PITS | AT91C_ST_WDOVF | AT91C_ST_RTTINC | AT91C_ST_ALMS;
	(void) AT91_SYS->ST_SR;		/* Clear any pending interrupts */

	/*
	 * Make IRQs happen for the system timer.
	 */
	setup_irq(AT91C_ID_SYS, &at91rm9200_timer_irq);

	/* Set initial alarm to 0 */
	AT91_SYS->ST_RTAR = 0;

	/* Real time counter incremented every 30.51758 microseconds */
	AT91_SYS->ST_RTMR = 1;

	/* Set Period Interval timer */
	AT91_SYS->ST_PIMR = LATCH;

	/* Change the kernel's 'tick' value to 10009 usec. (the default is 10000) */
	tick_usec = (LATCH * 1000000) / CLOCK_TICK_RATE;

	/* Enable Period Interval Timer interrupt */
	AT91_SYS->ST_IER = AT91C_ST_PITS;
}

struct sys_timer at91rm9200_timer = {
	.init		= at91rm9200_timer_init,
	.offset		= at91rm9200_gettimeoffset,
};
