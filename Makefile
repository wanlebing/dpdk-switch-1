# Default target, can be overriden by command line or environment
RTE_SDK = /home/plal/Studia/dpdk
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

CC=gcc

# binary name
APP = vswitch

# all source are stored in SRCS-y
SRCS-y := main.c switch.c port.c list.c actions.c control.c

CFLAGS += -g
#CFLAGS += $(WERROR_FLAGS) -Wno-unused-variable

include $(RTE_SDK)/mk/rte.extapp.mk
