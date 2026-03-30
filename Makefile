CONFIG_MK=config.mk

SKIP_CONFIG_INCLUDE_GOALS := clean reconfigure
ifneq ($(filter $(SKIP_CONFIG_INCLUDE_GOALS),$(MAKECMDGOALS)),)
SKIP_CONFIG_INCLUDE := 1
endif

ifeq ($(SKIP_CONFIG_INCLUDE),)
ifneq ($(wildcard $(CONFIG_MK)),)
include $(CONFIG_MK)
endif
endif

CC?=gcc
BIN_DIR=bin
OBJ_ROOT=$(BIN_DIR)/obj/release
BEASTIE_BIN=$(BIN_DIR)/beastie
BOY_BIN=$(BIN_DIR)/boy
WARN_CFLAGS?=-Wall
OPT_CFLAGS?=-O2
DEBUG_OPT_CFLAGS?=-O0 -g
PROJECT_INCLUDES?=-I. -Icommon -Ibeastie -Iboy
BOY_INCLUDES?=-I. -Icommon -Iboy
EXTRA_CPPFLAGS?=
EXTRA_CFLAGS?=
EXTRA_LDFLAGS?=
PCAP_CFLAGS?=
PCAP_LIBS?=-lpcap
MEMIF_CFLAGS?=
MEMIF_LIBS?=-lmemif
VAPI_CFLAGS?=
VAPI_LIBS?=-lvapiclient
VPP_COMPAT_CPPFLAGS?=
DEPFLAGS?=-MMD -MP
CFLAGS=$(WARN_CFLAGS) $(OPT_CFLAGS) $(PROJECT_INCLUDES) $(EXTRA_CPPFLAGS) $(EXTRA_CFLAGS) $(PCAP_CFLAGS) $(MEMIF_CFLAGS)
DEBUG_CFLAGS=$(WARN_CFLAGS) $(DEBUG_OPT_CFLAGS) $(PROJECT_INCLUDES) $(EXTRA_CPPFLAGS) $(EXTRA_CFLAGS) $(PCAP_CFLAGS) $(MEMIF_CFLAGS)
BOY_CFLAGS=$(WARN_CFLAGS) $(OPT_CFLAGS) $(BOY_INCLUDES) $(EXTRA_CPPFLAGS) $(VPP_COMPAT_CPPFLAGS) $(EXTRA_CFLAGS) $(PCAP_CFLAGS) $(VAPI_CFLAGS)
BOY_DEBUG_CFLAGS=$(WARN_CFLAGS) $(DEBUG_OPT_CFLAGS) $(BOY_INCLUDES) $(EXTRA_CPPFLAGS) $(VPP_COMPAT_CPPFLAGS) $(EXTRA_CFLAGS) $(PCAP_CFLAGS) $(VAPI_CFLAGS)
LDFLAGS=$(EXTRA_LDFLAGS) $(MEMIF_LIBS) $(PCAP_LIBS) -lpthread
BOY_LDFLAGS=$(EXTRA_LDFLAGS) $(PCAP_LIBS) -lpthread $(VAPI_LIBS)
COMMON_SRCS=common/common.c common/table.c

BEASTIE_OBJS=$(OBJ_ROOT)/beastie/main.o \
	     $(OBJ_ROOT)/beastie/beastie.o \
	     $(OBJ_ROOT)/beastie/pcap_writer.o \
	     $(OBJ_ROOT)/common/common.o \
	     $(OBJ_ROOT)/common/table.o
BOY_OBJS=$(OBJ_ROOT)/boy/main.o \
	 $(OBJ_ROOT)/boy/boy.o \
	 $(OBJ_ROOT)/common/common.o \
	 $(OBJ_ROOT)/common/table.o
DEPS=$(BEASTIE_OBJS:.o=.d) $(BOY_OBJS:.o=.d)

all: $(CONFIG_MK) $(BEASTIE_BIN) $(BOY_BIN)

$(CONFIG_MK): ./configure
	./configure

reconfigure:
	./configure

debug: debug-beastie debug-boy

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_ROOT)/beastie/%.o: beastie/%.c $(CONFIG_MK) | $(BIN_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJ_ROOT)/boy/%.o: boy/%.c $(CONFIG_MK) | $(BIN_DIR)
	mkdir -p $(dir $@)
	$(CC) $(BOY_CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(OBJ_ROOT)/common/%.o: common/%.c $(CONFIG_MK) | $(BIN_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BEASTIE_BIN): $(CONFIG_MK) $(BEASTIE_OBJS) | $(BIN_DIR)
	$(CC) -o $@ $(BEASTIE_OBJS) $(LDFLAGS)

$(BOY_BIN): $(CONFIG_MK) $(BOY_OBJS) | $(BIN_DIR)
	$(CC) -o $@ $(BOY_OBJS) $(BOY_LDFLAGS)

debug-beastie: $(CONFIG_MK) beastie/main.c beastie/beastie.c beastie/pcap_writer.c $(COMMON_SRCS) | $(BIN_DIR)
	$(CC) $(DEBUG_CFLAGS) -o $(BEASTIE_BIN) beastie/main.c beastie/beastie.c beastie/pcap_writer.c $(COMMON_SRCS) $(LDFLAGS)

debug-boy: $(CONFIG_MK) boy/main.c boy/boy.c $(COMMON_SRCS) | $(BIN_DIR)
	$(CC) $(BOY_DEBUG_CFLAGS) -o $(BOY_BIN) boy/main.c boy/boy.c $(COMMON_SRCS) $(BOY_LDFLAGS)

clean:
	rm -rf $(BIN_DIR) $(CONFIG_MK)

.PHONY: all clean reconfigure debug debug-beastie debug-boy

-include $(DEPS)
