# SPDX-License-Identifier: LGPL-2.1-only
#
# Makefile - Makefile for the PCAN driver test programs.
#
# Copyright (C) 2001-2020 PEAK System-Technik GmbH <www.peak-system.com>
#
# Contact:      <linux@peak-system.com>
# Maintainer:   Stephane Grosjean <s.grosjean@peak-system.com>
# Contribution: Klaus Hitschler <klaus.hitschler@gmx.de>
#

# Test applications
RECEIVETEST := receivetest
RECEIVETEST_POSIX := $(RECEIVETEST)_posix
TRANSMITEST := transmitest
TRANSMITEST_POSIX := $(TRANSMITEST)_posix
BITRATETEST := bitratetest
FILTERTEST := filtertest
PCAN_SETTINGS := pcan-settings
PCANFDTST := pcanfdtst

ALL = $(RECEIVETEST) $(TRANSMITEST) $(BITRATETEST) $(FILTERTEST) $(PCANFDTST) $(PCAN_SETTINGS) $(RECEIVETEST_POSIX) $(TRANSMITEST_POSIX)

# override default CC = cc
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++

PCANDRV_DIR := ../driver
PCANLIB_DIR := ../lib

SRC := src
INC := -I. -I$(PCANLIB_DIR) -I$(PCANDRV_DIR)
RT ?= NO_RT

BINDIR = $(DESTDIR)/usr/local/bin

# FLAGS
CFLAGS := -D$(RT) $(INC) $(OPTS_CFLAGS)

ifneq ($(DESTDIR),)
CFLAGS += --sysroot=$(DESTDIR)
endif

ifeq ($(RT),NO_RT)
CFLAGS += -g
endif

# LDLIBS before -lpcanfd means that local libpcanfd will be linked
# LDLIBS after -lpcanfd means that global libpcanfd will be linked. If not found
# then local libpcanfd will be linked.
LDFLAGS := -lpcanfd $(OPTS_LDFLAGS)

# --- note --------------------------------------------------------------------
# Because these examples have to be built BEFORE libpcan[fd] (as well as
# the pcan driver) is installed, local paths HAVE TO be added to the command
# lines of gcc (see usage of -Wl,rpath in  LDFLAGS below).
# Once libpcan[fd] as well as the pcan driver are installed, these command
# line options are useless and SHOULD NOT be added to any application.
LDFLAGS += -L$(PCANLIB_DIR)/lib -Wl,-rpath $(PCANLIB_DIR)/lib
# --- end of note -------------------------------------------------------------

# Posix examples FLAGS
POSIX_CFLAGS := $(CFLAGS)
POSIX_LDFLAGS := $(LDFLAGS)

RTDM_CFLAGS := $(CFLAGS)
RTDM_LDFLAGS := $(LDFLAGS)

# -----------------------------------------------------------------------------
ifeq ($(RT),XENOMAI)

# Xenomai directory, xeno-config and library directory
RT_DIR := /usr/xenomai
RT_CONFIG := $(RT_DIR)/bin/xeno-config

RT_LIB_DIR := $(shell $(RT_CONFIG) --library-dir)

# usage: $(call cflags,skin)
define cflags
$(shell $(RT_CONFIG) --$1 --cflags)
endef

# usage: $(call ldflags,skin,extra)
define ldflags
$(shell $(RT_CONFIG) --$1 --ldflags $2) -L$(RT_LIB_DIR) -Wl,-rpath $(RT_LIB_DIR)
endef

# "native" lib has been renamed into "alchemy" since Xenomai 3
XENO_VER := $(shell $(RT_CONFIG) --version)
XENO_VER_LIST := $(subst ., ,$(XENO_VER))
XENO_VER_MAJ := $(word 1,$(XENO_VER_LIST))
#XENO_VER_MIN := $(word 2,$(XENO_VER_LIST))

ifeq ($(XENO_VER_MAJ),3)
# Xenomai 3
XENO_NATIVE := alchemy
RT_EXTRA_LDFLAGS = --no-auto-init
else
XENO_NATIVE := native
endif

SKIN := $(XENO_NATIVE)

# Since lpcanfd links to lrtdm, some applications only need the native skin
CFLAGS += $(call cflags,$(XENO_NATIVE))
LDFLAGS += $(call ldflags,$(XENO_NATIVE),$(RT_EXTRA_LDFLAGS))

# -- Posix with Xenomai 3
POSIX_CFLAGS += $(call cflags,posix)
POSIX_LDFLAGS += $(call ldflags,posix,$(RT_EXTRA_LDFLAGS))

# -- pcan-settings doesn't use libpcanfd: need rtdm skin
RTDM_CFLAGS += $(call cflags,rtdm)
RTDM_LDFLAGS += $(call ldflags,rtdm)

endif

# -----------------------------------------------------------------------------
ifeq ($(RT),RTAI)

# Rtai directory, rtai-config and library directory
RT_DIR := /usr/realtime
RT_CONFIG := $(RT_DIR)/bin/rtai-config

define cflags
$(shell $(RT_CONFIG) --$1-cflags)
endef

# usage: $(call ldflags,skin)
define ldflags
$(shell $(RT_CONFIG) --$1-ldflags)
endef

# User space application compiler options
#USERAPP_CFLAGS := $(call cflags,lxrt)
CFLAGS += $(call cflags,lxrt)
LDFLAGS += $(call ldflags,lxrt) -Wl,-rpath /usr/realtime/lib

# -- Posix with RTAI: same flags
POSIX_CFLAGS += $(call cflags,lxrt)
POSIX_LDFLAGS += $(call ldflags,lxrt) -Wl,-rpath /usr/realtime/lib

# -- pcan-settings: same flags
RTDM_CFLAGS += $(call cflags,lxrt)
RTDM_LDFLAGS := $(call ldflags,lxrt) -Wl,-rpath /usr/realtime/lib

endif

# -----------------------------------------------------------------------------
ifeq ($(RT),NO_RT)
POSIX_LDFLAGS += -lpthread

# Note: for ldd < 2.17, -lrt is needed for
# clock_gettime(CLOCK_REALTIME);
RT_NEEDED := $(shell ldd --version | awk '/^ldd/ { split($$NF, ver, "."); if ((ver[1] * 100 + ver[2]) < 217) print "yes"; exit }')
ifeq ($(RT_NEEDED),yes)
POSIX_LDFLAGS += -lrt
endif
endif

# -----------------------------------------------------------------------------
# some versions of g++ warns about this
CXXFLAGS := $(subst -Wstrict-prototypes,,$(CFLAGS))
POSIX_CXXFLAGS := $(subst -Wstrict-prototypes,,$(POSIX_CFLAGS))

all: banner $(ALL)

banner:
	@echo "***"
	@echo "*** Making example and test programs"
	@echo "***"
	@echo "*** $(CC) version: `$(CC) -dumpversion`"
	@echo "***"

ifeq ($(RT), NO_RT)
$(RECEIVETEST): $(SRC)/$(RECEIVETEST).c $(SRC)/common.c
else
$(RECEIVETEST): $(SRC)/$(RECEIVETEST)_rt.c $(SRC)/common.c
endif
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

ifeq ($(RT), NO_RT)
$(TRANSMITEST): $(SRC)/$(TRANSMITEST).cpp $(SRC)/common.c $(SRC)/parser.cpp
else
$(TRANSMITEST): $(SRC)/$(TRANSMITEST)_rt.cpp $(SRC)/common.c $(SRC)/parser.cpp
endif
	$(CXX) $(CXXFLAGS) $^ -lstdc++ $(LDFLAGS) -o $@
	
$(BITRATETEST): $(SRC)/$(BITRATETEST).c $(SRC)/common.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(FILTERTEST): $(SRC)/$(FILTERTEST).cpp $(SRC)/common.c
	$(CXX) $(CXXFLAGS) $^ -lstdc++ $(LDFLAGS) -o $@

$(PCAN_SETTINGS): $(SRC)/$(PCAN_SETTINGS).c
	$(CC) $(RTDM_CFLAGS) $^ -lpopt $(RTDM_LDFLAGS) -g -o $@

$(PCANFDTST): $(SRC)/$(PCANFDTST).c
	$(CC) $(POSIX_CFLAGS) $^ $(POSIX_LDFLAGS) -o $@

$(RECEIVETEST_POSIX): $(SRC)/$(RECEIVETEST_POSIX).c $(SRC)/common.c
	$(CC) $(POSIX_CFLAGS) $^ $(POSIX_LDFLAGS) -o $@

$(TRANSMITEST_POSIX): $(SRC)/$(TRANSMITEST_POSIX).cpp $(SRC)/common.c $(SRC)/parser.cpp
	$(CXX) $(POSIX_CXXFLAGS) $^ -lstdc++ $(POSIX_LDFLAGS) -o $@

clean:
	-rm -f $(SRC)/*~ $(SRC)/*.o *~ $(ALL)
	
install:
	cp $(ALL) $(BINDIR)

uninstall:
	-cd $(BINDIR); rm -f $(ALL)

xeno:
	$(MAKE) RT=XENOMAI

rtai:
	$(MAKE) RT=RTAI

# Need:
# $ sudo apt-get install gcc-multilib  g++-multilib
# $ sudo apt-get install libpopt-dev:i386
all32:
	$(MAKE) OPTS_CFLAGS=-m32 OPTS_LDFLAGS=-m32
