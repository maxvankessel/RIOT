include ../Makefile.tests_common

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

USEMODULE += gsm_quectel
USEMODULE += gsm_gprs
USEMODULE += gsm_call
USEMODULE += gsm_ppp

USEMODULE += gnrc
USEMODULE += gnrc_ppp
#USEMODULE += gnrc_ppp_ipv6
USEMODULE += gnrc_pktdump
USEMODULE += gnrc_txtsnd

USEMODULE += sock_dns
USEMODULE += gnrc_sock_udp
USEMODULE += posix

USEMODULE += gnrc_ipv6_default


CFLAGS_OPT=-O0
CFLAGS_DBG=-ggdb -g

MODEM_UART     ?= "UART_NUMOF-1"
MODEM_BAUDRATE ?= 115200

# export modem parameters
CFLAGS += -DMODEM_UART="UART_DEV($(MODEM_UART))"
CFLAGS += -DMODEM_BAUDRATE=$(MODEM_BAUDRATE)

include $(RIOTBASE)/Makefile.include
