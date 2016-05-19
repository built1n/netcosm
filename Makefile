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

SRC = $(shell cat src/SOURCES)
OBJ = $(patsubst %.c,$(BUILDDIR)/src/%.o,$(SRC))

WORLD_SRC = $(shell cat worlds/SOURCES)
WORLD_OBJ = $(patsubst %.c,$(BUILDDIR)/worlds/%.so,$(WORLD_SRC))

INCLUDES = -I src/ -I export/include/

WARNFLAGS = -Wall -Wextra -Wshadow -fno-strict-aliasing

OPTFLAGS = -O2
DEBUGFLAGS = -g

CFLAGS = $(OPTFLAGS) $(DEBUGFLAGS) $(WARNFLAGS) -std=c99 $(INCLUDES)

LDFLAGS = -lev -lcrypto -ldl

HEADERS = src/*.h export/include/*.h

DEPS = $(patsubst %.c,$(BUILDDIR)/src/%.d,$(SRC)) $(patsubst %.c,$(BUILDDIR)/worlds/%.d,$(WORLD_SRC))

# Main targets
################################################################################

.PHONY: all
all: | $(OUT) $(WORLD_OBJ)

$(OUT): $(OBJ)
	@echo "LD $@"
	@$(CC) $(OBJ) $(CFLAGS) -o $@ $(LDFLAGS) -rdynamic

$(OBJ): | $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: %.c $(BUILDDIR)/%.d Makefile
	@echo "CC $<"
	@mkdir -p `dirname $@`
	@$(CC) -c $< $(CFLAGS) -o $@

$(BUILDDIR)/%.so: %.c $(BUILDDIR)/%.d Makefile
	@echo "CC $<"
	@mkdir -p `dirname $@`
	@$(CC) $< $(CFLAGS) -shared -o $@ -D_WORLD_MODULE_ -fPIC

# Dependencies
################################################################################

.PRECIOUS: $(BUILDDIR)/%.d
$(BUILDDIR)/%.d: %.c
	@mkdir -p `dirname $@`
	@$(CC) $(CFLAGS) -MF"$@" -MG -MM -MP -MT"$@" -MT"$(BUILDDIR)/$(<:.c=.o)" "$<"

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
	@rm -f $(OBJ) $(OUT) $(WORLD_OBJ)

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
