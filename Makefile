CC = cc
OUT = build
PLATFORM = unix

NETCOSM_SRC = $(shell cat SOURCES)
NETCOSM_OBJ := $(NETCOSM_SRC:.c=.o)

CFLAGS = -O3 -g -I src/ -I export/include -Wall -Wextra -Wshadow	\
	-std=c99 -fno-strict-aliasing
LDFLAGS = -lgcrypt -lev

HEADERS = src/*.h export/include/*.h

all: $(OUT)/$(PLATFORM).bin Makefile SOURCES $(HEADERS)

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
