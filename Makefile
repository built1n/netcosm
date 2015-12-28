CC = gcc
OUT = build
PLATFORM = unix

NETCOSM_OBJ = src/server.o src/client.o src/auth.o src/telnet.o src/util.o src/room.o worlds/test.o src/hash.o

CFLAGS = -O3 -g -I src/ -I target/$(PLATFORM) -Wall -Wextra -Wshadow -std=gnu99
LDFLAGS = -lgcrypt

HEADERS = src/netcosm.h src/hash.h src/telnet.h

all: $(OUT)/$(PLATFORM).bin Makefile $(HEADERS)

$(OUT)/$(PLATFORM).bin: $(NETCOSM_OBJ) $(HEADERS) Makefile
	@mkdir -p $(OUT)
	@echo "LD $<"
	@$(CC) $(NETCOSM_OBJ) $(CFLAGS) $(LDFLAGS) -o $(OUT)/$(PLATFORM).bin

install: $(OUT)/$(PLATFORM).bin
	@install $(OUT)/$(PLATFORM).bin /bin/netcosm

clean:
	@echo "Cleaning build directory..."
	@rm -f $(OUT)/$(PLATFORM).bin
	@rm -f $(NETCOSM_OBJ)

%.o: %.c Makefile $(HEADERS)
	@echo "CC $<"
	@$(CC) $(CFLAGS) $(OPTFLAGS) -c $< -o $@
