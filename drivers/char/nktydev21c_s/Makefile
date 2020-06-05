#
# Makefile for NKTY DEV drivers
#

obj-$(CONFIG_AT91_UART) += at91_uart.o deal_data.o uart_dev.o deal_purse.o
ifeq ($(CONFIG_USRI2C_INT), y)
  obj-$(CONFIG_AT91_USRI2C) += at91_usri2c-2.0.o
else
  obj-$(CONFIG_AT91_USRI2C) += at91_usri2c.o
endif
obj-$(CONFIG_AT91_GPIOB) += at91_gpiob.o
obj-$(CONFIG_AT91_DATAFLASH) += dataflash.o
obj-$(CONFIG_AT91_FLASH) += at91_flash.o
#obj-$(CONFIG_AT91_TTY) += tiny_tty.o
