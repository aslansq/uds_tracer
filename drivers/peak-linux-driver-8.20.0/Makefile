# SPDX-License-Identifier: LGPL-2.1-only
#
# Makefile - global Makefile for all components
#
# Copyright (C) 2001-2020 PEAK System-Technik GmbH <www.peak-system.com>
#
# Contact:       <linux@peak-system.com>
# Maintainer:    Stephane Grosjean <s.grosjean@peak-system.com>
# Contributions: Klaus Hitschler <klaus.hitschler@gmx.de>
#                Pablo Yanez Trujillo <yanez@pse.de> cross-compile
#

# CROSS-COMPILING SETTINGS
#
# You need a cross-compiler. You can build one with crosstool
# http://www.kegel.com/crosstool/current/doc/crosstool-howto.html
#
# These variables work with the toolchains created by crosstool

# defines the architecture. For example 'arm' for an ARM system
#export ARCH=arm

# the path and prefix of the cross-compiler
#export CROSS_COMPILE=/home/yanez/matrix500/arm-9tdmi-linux-gnu/bin/arm-9tdmi-linux-gnu-

# MACROS AND DEFINES
PCAN_BASIC = libpcanbasic

ifneq ($(PCAN_BASIC),)
#PWD = $(shell pwd)
# @make -C $(PCAN_BASIC) PCAN_ROOT=$(PWD) $1
define make-pcanbasic
@$(MAKE) -C $(PCAN_BASIC) $1
endef
else
define make-pcanbasic
endef
endif

define do-make-driver
@$(MAKE) -C driver $1
endef

define do-make-user
@$(MAKE) -C lib $1
@$(MAKE) -C test $1
$(call make-pcanbasic, $1)
endef

define do-make
$(call do-make-driver, $1)
$(call do-make-user, $1)
endef

define make-all
$(call do-make, all)
endef

define make-clean
$(call do-make, clean)
endef

define make-install
$(call do-make, install)
endef

define make-xeno
$(call do-make, xeno)
endef

define make-rtai
$(call do-make, rtai)
endef

define make-uninstall
$(call do-make, uninstall)
endef

define make-install-user
$(call do-make, install)
endef

all:
	$(make-all)

chardev: all

# do-build lib, test (and pcanbasic) even if useless in netdev mode:
# those binaries have to exist for the install procedure to complete.
netdev:
	$(call do-make-driver, netdev)
	$(call do-make-user, all)

clean:
	$(make-clean)

install:
	$(call do-make-driver, install)
	$(call do-make-user, install)

install_with_dkms:
	$(call do-make-driver, install_with_dkms)
	$(call do-make-user, install)

uninstall:
	$(make-uninstall)

xeno:
	$(make-xeno)

rtai:
	$(make-rtai)

pcanbasic:
ifneq ($(PCAN_BASIC),)
	$(call make-pcanbasic, all)
else
	@echo Warning: PCAN_BASIC not defined in that version.
endif
# end

# DO NOT DELETE
