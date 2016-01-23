CC = cc
OUT = build
PLATFORM = unix
PREFIX = /usr/local

NETCOSM_SRC = $(shell cat SOURCES)
NETCOSM_OBJ := $(NETCOSM_SRC:.c=.o)

CFLAGS = -O3 -g -I src/ -I export/include -Wall -Wextra -Wshadow	\
	-std=c99 -fno-strict-aliasing

LDFLAGS = -lev -lcrypto

HEADERS = src/*.h export/include/*.h

DEPS = $(NETCOSM_SRC:.c=.d)

.PHONY: all
all: $(OUT)/$(PLATFORM).bin

%.o: %.c Makefile %.d
	@echo "CC $<"
	@$(CC) $(CFLAGS) $(OPTFLAGS) -c $< -o $@

$(OUT)/$(PLATFORM).bin: $(NETCOSM_OBJ) $(HEADERS) Makefile
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(NETCOSM_OBJ) $(CFLAGS) -o $(OUT)/$(PLATFORM).bin $(LDFLAGS)
	@make setcap

# automatically generate dependency rules

%.d : %.c
	@$(CC) $(CCFLAGS) -MF"$@" -MG -MM -MP -MT"$@" -MT"$(<:.c=.o)" "$<"

# -MF  write the generated dependency rule to a file
# -MG  assume missing headers will be generated and don't stop with an error
# -MM  generate dependency rule for prerequisite, skipping system headers
# -MP  add phony target for each header to prevent errors when header is missing
# -MT  add a target to the generated dependency

-include $(DEPS)

.PHONY: depend
depend: $(DEPS)
	@echo "Dependencies (re)generated."

.PHONY: install
install: $(OUT)/$(PLATFORM).bin
	@echo "INSTALL" $(PREFIX)/bin/netcosm
	@install $(OUT)/$(PLATFORM).bin $(PREFIX)/bin/netcosm
.PHONY: uninstall
uninstall:
	@echo "RM" $(PREFIX)/bin/netcosm
	@rm -f $(PREFIX)/bin/netcosm

.PHONY: clean
clean:
	@echo "Cleaning build directory..."
	@rm -f $(OUT)/$(PLATFORM).bin
	@rm -f $(NETCOSM_OBJ)

.PHONY: depclean
depclean:
	@echo "Cleaning dependencies..."
	@rm -f $(DEPS)

.PHONY: veryclean
veryclean: clean depclean

.PHONY: help
help:
	@echo "Cleaning targets:"
	@echo "  clean			- Remove object files"
	@echo "  depclean		- Remove dependency files"
	@echo "  veryclean		- Remove object and dependency files"
	@echo "Installation targets:"
	@echo "  install		- Install to PREFIX/bin (default /usr/local)"
	@echo "  uninstall		- Remove a previous installation"
	@echo "Build targets:"
	@echo "  all			- Build the binary"
	@echo "  depend		- Regenerate dependencies"
	@echo "  setcap		- Set CAP_NET_BIND_SERVICE on the binary"

.PHONY: setcap
setcap:
	@echo "Enabling CAP_NET_BIND_SERVICE on "$(OUT)/$(PLATFORM).bin"..."
	@sudo setcap 'cap_net_bind_service=+ep' $(OUT)/$(PLATFORM).bin
