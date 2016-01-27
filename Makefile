CC = cc
BUILDDIR = build
PLATFORM = unix
OUT = $(BUILDDIR)/$(PLATFORM).bin
PREFIX = /usr/local

SRC = $(shell cat SOURCES)
OBJ = $(patsubst %.c,$(BUILDDIR)/%.o,$(SRC))

INCLUDES = -I src/ -I export/include/

WARNFLAGS = -Wall -Wextra -Wshadow -fno-strict-aliasing

OPTFLAGS = -O3
DEBUGFLAGS = -g

CFLAGS = $(OPTFLAGS) $(DEBUGFLAGS) $(WARNFLAGS) -std=c99 $(INCLUDES)

LDFLAGS = -lev -lcrypto -lbsd

HEADERS = src/*.h export/include/*.h

DEPS = $(patsubst %.c,$(BUILDDIR)/%.d,$(SRC))

################################################################################

.PHONY: all
all: | $(OUT)

$(OUT): $(OBJ)
	@echo "LD $@"
	@$(CC) $(OBJ) $(CFLAGS) -o $@ $(LDFLAGS)

$(OBJ): | $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: %.c $(BUILDDIR)/%.d Makefile
	@echo "CC $<"
	@mkdir -p `dirname $@`
	@$(CC) -c $< $(CFLAGS) -o $@

################################################################################

.PRECIOUS: $(BUILDDIR)/%.d
$(BUILDDIR)/%.d: %.c
	@mkdir -p `dirname $@`
	@$(CC) $(CCFLAGS) -MF"$@" -MG -MM -MP -MT"$@" -MT"$(<:.c=.o)" "$<"

-include $(DEPS)

################################################################################

.PHONY: setcap
setcap:
	@echo "Setting CAP_NET_BIND_SERVICE on $(OUT)..."
	@sudo setcap 'cap_net_bind_service=+ep' "$(OUT)"

.PHONY: clean
clean:
	@echo "Cleaning build directory..."
	@rm -f $(OBJ) $(OUT)

.PHONY: veryclean
veryclean:
	@echo "Removing build directory..."
	@rm -rf $(BUILDDIR)

.PHONY: depend
depend: $(DEPS)
	@echo "Dependencies (re)generated."
