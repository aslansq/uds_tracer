#****************************************************************************
# Copyright (C) 2001-2019  PEAK System-Technik GmbH
#
# linux@peak-system.com
# www.peak-system.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# Maintainer: Stephane Grosjean (s.grosjean@peak-system.com)
#****************************************************************************

#****************************************************************************
#
# Makefile - Makefile for RTAI demo programms.
#
# $Id$
#
#****************************************************************************

# override default CC = cc
CC = $(CROSS_COMPILE)gcc

PCANDRV_DIR := ../../driver
PCANLIB_DIR := ../../lib

SRC := .
INC := -I. -I$(PCANLIB_DIR) -I$(PCANDRV_DIR)
RT = RTAI

# RTAI directory, rtai-config and library directory
RT_DIR := /usr/realtime
RT_CONFIG := $(RT_DIR)/bin/rtai-config

# User space application compiler options
SKIN := lxrt
USERAPP_CFLAGS := $(shell $(RT_CONFIG) --$(SKIN)-cflags)
USERAPP_LDFLAGS := $(shell $(RT_CONFIG) --$(SKIN)-ldflags)

LDLIBS := -L$(PCANLIB_DIR)/lib

CFLAGS := -D$(RT) $(INC) $(USERAPP_CFLAGS)
LDFLAGS := $(USERAPP_LDFLAGS) $(LDLIBS)

ALL = rtai_rx rtai_tx

all: $(ALL)

rtai_tx: rtai_tx.c
	$(CC) $(CFLAGS) $^ -lpcanfd $(LDFLAGS) -o $@

rtai_rx: rtai_rx.c
	$(CC) $(CFLAGS) $^ -lpcanfd $(LDFLAGS) -o $@

clean:
	-rm -f $(SRC)/*~ $(SRC)/*.o *~ $(ALL)
