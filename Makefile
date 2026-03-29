CC=gcc
BIN_DIR=bin
BEASTIE_BIN=$(BIN_DIR)/beastie
BOY_BIN=$(BIN_DIR)/boy
CFLAGS=-Wall -O2 -I. -Icommon -Ibeastie -Iboy
DEBUG_CFLAGS=-Wall -O0 -g -I. -Icommon -Ibeastie -Iboy
BOY_CFLAGS=-Wall -O2 -I. -Icommon -Iboy
BOY_DEBUG_CFLAGS=-Wall -O0 -g -I. -Icommon -Iboy
LDFLAGS=-lmemif -lpcap -lpthread
BOY_LDFLAGS=-lpcap -lpthread -lvapiclient
COMMON_SRC=common/common.c

all: check $(BEASTIE_BIN) $(BOY_BIN)
configure:
	bash ./extras/configure.sh
debug: debug-beastie debug-boy
check:
	bash ./extras/debug.sh

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BEASTIE_BIN): beastie/main.c beastie/beastie.c beastie/pcap_writer.c $(COMMON_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BOY_BIN): boy/main.c boy/boy.c $(COMMON_SRC) | $(BIN_DIR)
	$(CC) $(BOY_CFLAGS) -o $@ $^ $(BOY_LDFLAGS)

debug-beastie: beastie/main.c beastie/beastie.c beastie/pcap_writer.c $(COMMON_SRC) | $(BIN_DIR)
	$(CC) $(DEBUG_CFLAGS) -o $(BEASTIE_BIN) $^ $(LDFLAGS)

debug-boy: boy/main.c boy/boy.c $(COMMON_SRC) | $(BIN_DIR)
	$(CC) $(BOY_DEBUG_CFLAGS) -o $(BOY_BIN) $^ $(BOY_LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean check configure debug debug-beastie debug-boy
