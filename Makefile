CC=gcc
LIBMEMIF_DIR=lib/libmemif
LIBMEMIF_INCLUDE_DIR=$(LIBMEMIF_DIR)/include
LIBMEMIF_BUILD_DIR=$(LIBMEMIF_DIR)/build
LIBMEMIF_LIB=$(LIBMEMIF_BUILD_DIR)/lib/libmemif.so
LIBMEMIF_REQUIRED_REF=v26.02
BIN_DIR=bin
BEASTIE_BIN=$(BIN_DIR)/beastie
BOY_BIN=$(BIN_DIR)/boy
CFLAGS=-Wall -O2 -I$(LIBMEMIF_INCLUDE_DIR) -Ibeastie -Iboy
LDFLAGS=-L$(LIBMEMIF_BUILD_DIR)/lib -lmemif -lpcap -lpthread

all: configure $(BEASTIE_BIN) $(BOY_BIN)
configure:
	bash ./extras/configure.sh $(LIBMEMIF_REQUIRED_REF)
debug:
	bash ./extras/debug.sh $(LIBMEMIF_REQUIRED_REF)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BEASTIE_BIN): beastie/main.c beastie/beastie.c beastie/pcap_writer.c $(LIBMEMIF_LIB) | $(BIN_DIR) check-libmemif
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BOY_BIN): boy/main.c boy/boy.c $(LIBMEMIF_LIB) | $(BIN_DIR) check-libmemif
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(LIBMEMIF_DIR)/CMakeLists.txt:
	./extras/get_libmemif.sh

$(LIBMEMIF_LIB): $(LIBMEMIF_DIR)/CMakeLists.txt
	./extras/get_libmemif.sh

check-libmemif: $(LIBMEMIF_DIR)/CMakeLists.txt
	bash ./extras/check_libmemif.sh $(LIBMEMIF_REQUIRED_REF)

clean:
	rm -rf $(BIN_DIR)
	@if [ -d "$(LIBMEMIF_BUILD_DIR)" ]; then rm -rf "$(LIBMEMIF_BUILD_DIR)"; fi

.PHONY: all clean check-libmemif configure debug
