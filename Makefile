################################################################################
#
#   NetCosm - a MUD server
#   Copyright (C) 2016 Franklin Wei
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
################################################################################

# Definitions
################################################################################

CC = cc
BUILDDIR = build
PLATFORM = unix
OUT = $(BUILDDIR)/$(PLATFORM).bin
PREFIX = /usr/local

SRC = $(shell cat SOURCES)
OBJ = $(patsubst %.c,$(BUILDDIR)/%.o,$(SRC))

INCLUDES = -I src/ -I export/include/

WARNFLAGS = -Wall -Wextra -Wshadow -fno-strict-aliasing

OPTFLAGS = -O2
DEBUGFLAGS = -g -fstack-protector -D_FORTIFY_SOURCE=2

CFLAGS = $(OPTFLAGS) $(DEBUGFLAGS) $(WARNFLAGS) -std=c99 $(INCLUDES)

LDFLAGS = -lev -lcrypto

HEADERS = src/*.h export/include/*.h

DEPS = $(patsubst %.c,$(BUILDDIR)/%.d,$(SRC))

# Main targets
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

# Dependencies
################################################################################

.PRECIOUS: $(BUILDDIR)/%.d
$(BUILDDIR)/%.d: %.c
	@mkdir -p `dirname $@`
	@$(CC) $(CCFLAGS) -MF"$@" -MG -MM -MP -MT"$@" -MT"$(<:.c=.o)" "$<"

-include $(DEPS)

# Helper targets
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

.PHONY: install
install: $(OUT)
	@install $(OUT) $(PREFIX)/bin/netcosm

.PHONY: copysrc
copysrc:
	@cp -R src $(BUILDDIR)
	@cp -R worlds $(BUILDDIR)

.PHONY: help
help:
	@echo "Build targets:"
	@echo "  all (default)"
	@echo "  depend"
	@echo
	@echo "Cleaning targets:"
	@echo "  clean"
	@echo "  veryclean"
	@echo
	@echo "Helper targets:"
	@echo "  setcap"
	@echo "  install"
	@echo "  copysrc"
	@echo "  help"
