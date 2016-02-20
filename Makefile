ifeq ($(RTE_SDK),)
	$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = dpdk-switch

# all source are stored in SRCS-y
SRCS-y := src/main.c src/utils.c src/init.c

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -Wno-unused-variable

include $(RTE_SDK)/mk/rte.extapp.mk
