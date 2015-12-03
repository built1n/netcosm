CC = gcc
OUT = build
PLATFORM = unix

NETCOSM_OBJ = src/server.o src/client.o src/auth.o

CFLAGS = -lgcrypt -Og -g -I src/ -I target/$(PLATFORM) -Wall

all: $(OUT)/$(PLATFORM).bin

$(OUT)/$(PLATFORM).bin: $(NETCOSM_OBJ) Makefile
	mkdir -p $(OUT)
	$(CC) $(NETCOSM_OBJ) $(CFLAGS) -o $(OUT)/$(PLATFORM).bin

install: $(OUT)/$(PLATFORM).bin
	install $(OUT)/$(PLATFORM).bin /bin/netcosm

clean:
	rm -f $(OUT)/$(PLATFORM).bin
	rm -f $(NETCOSM_OBJ)
