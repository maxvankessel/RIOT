include ../Makefile.tests_common

BOARD_INSUFFICIENT_MEMORY := msb-430 msb-430h nucleo32-f031 nucleo32-f042 \
                             nucleo32-l031 nucleo-f030 nucleo-f334 nucleo-l053 \
                             stm32f0discovery

BOARD_BLACKLIST += mips-malta

USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc
USEMODULE += gnrc_pktdump
USEMODULE += gnrc_txtsnd
USEMODULE += pppos
USEMODULE += hdlc
USEMODULE += shell
USEMODULE += shell_commands

# set pppos parameters to default values if unset
PPPOS_UART     ?= "UART_NUMOF-1"
PPPOS_BAUDRATE ?= 115200

# export pppos parameters
CFLAGS += -DPPPOS_UART="UART_DEV($(PPPOS_UART))"
CFLAGS += -DPPPOS_BAUDRATE=$(PPPOS_BAUDRATE)

include $(RIOTBASE)/Makefile.include
