#
# Makefile for NKTY DEV drivers
#

ifdef CONFIG_ACCSW_USE_HASH
CFLAGS += -Idrivers/char/nkty-common
endif

obj-$(CONFIG_AT91_UART) += at91_uart.o deal_data.o uart_dev.o 
obj-$(CONFIG_AT91_USRI2C) += at91_usri2c.o
obj-$(CONFIG_AT91_GPIOB) += at91_gpiob.o
obj-$(CONFIG_AT91_SED1335) += at91_sed1335.o
obj-$(CONFIG_AT91_DATAFLASH) += dataflash.o
obj-$(CONFIG_AT91_USRI2C_GPIO) += at91_usri2c_gpio.o
#obj-$(CONFIG_AT91_TTY) += tiny_tty.o
