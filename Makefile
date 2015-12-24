CC = gcc
OUT = build
PLATFORM = unix

NETCOSM_OBJ = src/server.o src/client.o src/auth.o src/telnet.o src/util.o src/room.o world.o

CFLAGS = -Og -g -I src/ -I target/$(PLATFORM) -Wall -Wextra -Wshadow -Wpedantic
LDFLAGS = -lgcrypt

all: $(OUT)/$(PLATFORM).bin Makefile

$(OUT)/$(PLATFORM).bin: $(NETCOSM_OBJ) Makefile
	mkdir -p $(OUT)
	$(CC) $(NETCOSM_OBJ) $(CFLAGS) $(LDFLAGS) -o $(OUT)/$(PLATFORM).bin

install: $(OUT)/$(PLATFORM).bin
	install $(OUT)/$(PLATFORM).bin /bin/netcosm

clean:
	rm -f $(OUT)/$(PLATFORM).bin
	rm -f $(NETCOSM_OBJ)
