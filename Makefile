CC = gcc
OUT = build
PLATFORM = unix

NETCOSM_SRC = $(shell cat SOURCES)
NETCOSM_OBJ := $(NETCOSM_SRC:.c=.o)

CFLAGS = -Og -g -I src/ -I target/$(PLATFORM) -Wall -Wextra -Wshadow
LDFLAGS = -lgcrypt

HEADERS = src/netcosm.h src/hash.h src/telnet.h src/userdb.h

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
