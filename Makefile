CC = cc
OUT = build
PLATFORM = unix

NETCOSM_SRC = $(shell cat SOURCES)
NETCOSM_OBJ := $(NETCOSM_SRC:.c=.o)

CFLAGS = -O3 -g -I src/ -I export/include -Wall -Wextra -Wshadow	\
	-std=c99 -fno-strict-aliasing
LDFLAGS = -lgcrypt -lev

HEADERS = src/*.h export/include/*.h

DEPS = $(NETCOSM_SRC:.c=.d)

.PHONY: all
all: $(OUT)/$(PLATFORM).bin Makefile SOURCES $(HEADERS) $(DEPS)

%.o: %.c Makefile %.d
	@echo "CC $<"
	@$(CC) $(CFLAGS) $(OPTFLAGS) -c $< -o $@

$(OUT)/$(PLATFORM).bin: $(NETCOSM_OBJ) $(HEADERS) Makefile
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(NETCOSM_OBJ) $(CFLAGS) $(LDFLAGS) -o $(OUT)/$(PLATFORM).bin

# automatically generate dependency rules

%.d : %.c
	@echo "MKDEP $<"
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
	@install $(OUT)/$(PLATFORM).bin /bin/netcosm

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
	@echo "  clean		- Remove object files"
	@echo "  depclean	- Remove dependency files"
	@echo "  veryclean	- Remove object and dependency files"
	@echo "Build targets:"
