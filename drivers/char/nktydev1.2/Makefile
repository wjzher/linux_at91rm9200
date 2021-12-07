#
# Makefile for NKTY DEV drivers
#
ifdef CONFIG_ACCSW_USE_HASH
CFLAGS += -Idrivers/char/nkty-common
endif

obj-$(CONFIG_NKTYDEV12) += nktydev12.o
nktydev-y :=

nktydev12-$(CONFIG_AT91_UART) += at91_uart.o deal_data.o uart_dev.o
nktydev12-$(CONFIG_AT91_USRI2C) += at91_usri2c.o
nktydev12-$(CONFIG_AT91_GPIOB) += at91_gpiob.o
nktydev12-$(CONFIG_AT91_SED1335) += at91_sed1335.o
nktydev12-$(CONFIG_AT91_DATAFLASH) += dataflash.o
nktydev12-$(CONFIG_AT91_USRI2C_GPIO) += at91_usri2c_gpio.o
#obj-$(CONFIG_ACCSW_USE_HASH) += hashtab.o

