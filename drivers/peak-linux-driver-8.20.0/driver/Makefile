# SPDX-License-Identifier: GPL-2.0
#
# Makefile for PEAK-System CAN interfaces driver
#
# Copyright (C) 2001-2020 PEAK System-Technik GmbH <www.peak-system.com>
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Contact:      <linux@peak-system.com>
# Maintainer:   Stephane Grosjean <s.grosjean@peak-system.com>
# Contributors: Klaus Hitschler <klaus.hitschler@gmx.de>
#               Grant Edwards <gedwards@rivatek.com>
#
# Usage:
#
# targets:
# depend		create dependencies from included header files
#                       - not kernel 2.6
# all			do a standard make of the driver
# clean			clean up, base for a forced new make
# mrproper		make a total clean - not kernel 2.6
# install		run as root, installs the drivers
# message		show info about build environment
# netdev                build the netdev version of pcan
# xeno                  build the RT version of pcan for Xenomai 3.x
# xeno26                build the RT version of pcan for Xenomai 2.6
# rtai                  build the RT version of pcan for RTAI
# usb_only              build the driver with only USB support
# pci_only              build the driver with only PCI/PCIe support
#
# make options:
#
# KERNEL_LOCATION=...          where your kernel sources are located if not
#                              standard location (cross-compile)
# VERBOSE 0
#         1                    switch Makefile verbosity on
#
# DKMS	NO_DKMS_SUPPORT        build and install driver without dkms
#       DKMS_SUPPORT           build and install driver with dkms
# DBG	DEBUG                  switch debug on and Makefile verbosity on
#       NO_DEBUG
# MOD   NO_MODVERSIONS
#       MODVERSIONS            compile for modversions use
# PAR   NO_PARPORT_SUBSYSTEM
#       PARPORT_SUBSYSTEM      use without parport subsystem and pcan-dongle
# USB   NO_USB_SUPPORT
#       USB_SUPPORT            for use with USB devices
# PCI   NO_PCI_SUPPORT
#       PCI_SUPPORT            for use with PCAN-PCI and PCAN-PCIe
# PCIEC NO_PCIEC_SUPPORT
#       PCIEC_SUPPORT          for use with PCI-Expresscard
# ISA   NO_ISA_SUPPORT
#       ISA_SUPPORT            for use with PCAN-ISA or PCAN-104
# DNG   NO_DONGLE_SUPPORT
#       DONGLE_SUPPORT         for use with PCAN-DONGLE
# PCC   NO_PCCARD_SUPPORT
#       PCCARD_SUPPORT         for use with PCAN-PCCARD
# NET   NO_NETDEV_SUPPORT
#       NETDEV_SUPPORT         compile for use as CAN network device (AF_CAN)
#       AUTO_NETDEV_SUPPORT    enable netdev configuration depending on kernel
#                              CONFIG_CAN (kernel 2.6.25+)
# RT    XENOMAI
#       RTAI
#       NO_RT                  compile for use on XENOMAI or RTAI RTOS
#

# override default CC = cc
CC = $(CROSS_COMPILE)gcc

DRV = pcan

# Here are the pcan options used to select what is to be compiled
PCAN_BUILD_OPTS := MOD PAR USB PCI PCIEC ISA DNG PCC NET RT

MAKE_OPTS = $(foreach o,$(PCAN_BUILD_OPTS),$(o)=$(value $(o)))
PCAN_OPTS = $(foreach o,$(PCAN_BUILD_OPTS),-D$(value $(o)))

# System tools
DEPMOD = depmod
MODPROBE = modprobe
UDEVADM = udevadm


# dkms stuff
#
# Note: if dkms is not installed then driver is manually installed under misc.
#       dkms.conf is always built, even if dkms is not used (default). This
#       enables to call "make install_with_dkms" next.
# Note: DKMS_VER is changed when the final tarball is made.
DKMS_DRV = peak-linux-driver
DKMS_VER = 8.20.0
DKMS_CONF = dkms.conf
DKMS_TREE = /var/lib/dkms
DKMS_BIN = $(shell which dkms 2> /dev/null)
ifeq ($(DKMS),DKMS_SUPPORT)
ifneq ($(DKMS_BIN),)
USE_DKMS = yes
endif
endif

#
# please modify only here if your kernel path is elsewhere located
#
KERNEL_LOCATION = $(DKMS_KERNEL_DIR)

#
# probe various standard kernel locations
#
ifeq ($(KERNEL_LOCATION),)
define KERNEL_LOCATION_1
$(shell UR=`uname -r`; for d in \
	/usr/src/linux-headers-$$UR \
	/usr/src/linux-$$UR \
	/lib/modules/$$UR/build \
	/usr/src/linux \
	; do if [ -d $$d ]; then echo $$d; break; elif [ -h $$d ]; then readlink -f $$d; break; fi; done)
endef
ifeq ($(KERNEL_LOCATION_1),)
$(error "Unable to automatically find the Kernel headers.\
These are mandatory to build the pcan driver. Please set the KERNEL_LOCATION\
variable to the root directory of your own built Kernel.")
else
KERNEL_LOCATION = $(KERNEL_LOCATION_1)
endif
endif

#
# Get Linux OS release
#
-include $(DESTDIR)/etc/os-release
OS_RELEASE_ID = $(NAME)
ifeq ($(OS_RELEASE_ID),)
OS_RELEASE_ID = UNKNOWN_LINUX
endif
OS_RELEASE_VER = $(VERSION_ID)
ifeq ($(OS_RELEASE_VER),)
OS_RELEASE_VER = X.Y.Z
endif

#
# determine where to find the kernel built header files
#
define HEADER_PATH
$(shell if [ -d $(KERNEL_LOCATION)/include/generated ]; then \
		echo "$(KERNEL_LOCATION)/include/generated"; \
	else \
		echo "$(KERNEL_LOCATION)/include/linux"; \
	fi)
endef

#
# whole makefile is based on availability of version.h file
# do a forced stop if it is not available
#
# >= 3.7: linux/version.h no more in include but in include/generated/uapi
#
define VERSION_H_DIR
$(shell if [ -d $(HEADER_PATH)/uapi ]; then \
		echo "$(HEADER_PATH)/uapi"; \
	else \
		echo "$(KERNEL_LOCATION)/include"; \
	fi)
endef

HASVERSION_H = $(shell [ -f $(VERSION_H_DIR)/linux/version.h ] && echo yes)

ifneq ($(HASVERSION_H),yes)
#$(error "Can't find $(VERSION_H_DIR)/linux/version.h")
KERNEL_VERSION := $(shell uname -r)
else

#
# removed old version retrival for better cross compile support
# old KERNEL_VERSION := $(shell uname -r)
#
KERNEL_VERSION := $(shell cpp -E -dM -I$(KERNEL_LOCATION)/include \
                              $(VERSION_H_DIR)/linux/version.h | \
                              grep UTS_RELEASE | sed -e 's;[^"]*"\(.*\)";\1;g')

ifeq ($(KERNEL_VERSION),)
KERNEL_VERSION := $(shell cpp -E -dM -I$(KERNEL_LOCATION)/include \
                              $(HEADER_PATH)/utsrelease.h | \
                              grep UTS_RELEASE | sed -e 's;[^"]*"\(.*\)";\1;g')
endif
endif

ifeq ($(KERNEL_VERSION),)
$(error "Unable to read Kernel version string from $(KERNEL_LOCATION)")
endif

#
# where the driver should be installed - change here for cross install
#
ifeq ($(KERNEL_LOCATION),)
MODULES_DIR = $(DESTDIR)/lib/modules/$(shell uname -r)
else
MODULES_DIR = $(DESTDIR)/lib/modules/$(KERNEL_VERSION)
endif

INSTALL_LOCATION = $(MODULES_DIR)/misc

#
# get extracted kernel VERSION and PATCHLEVEL for comparison
#
# KERNEL_VERSION = x.y[.z][-name]
#
# Examples:
# KERNEL_VERSION = 1.2
# KERNEL_VERSION = 1.02
# KERNEL_VERSION = 1.2-name
# KERNEL_VERSION = 1.2.0003
# KERNEL_VERSION = 1.2.3-name
KVERSION := $(shell echo $(KERNEL_VERSION) | sed -e 's/^\([1-9][0-9]*\)\..*/\1/' )
KPATCHLEVEL := $(shell echo $(KERNEL_VERSION) | sed -e 's/^$(KVERSION)\.0*\([0-9][0-9]*\).*/\1/' )
SUBLEVELTMP := $(shell echo $(KERNEL_VERSION) | sed -e 's/^$(KVERSION)\.0*$(KPATCHLEVEL)\.0*\([0-9][0-9]*\).*/\1/' )

ifeq ($(SUBLEVELTMP),$(KERNEL_VERSION))
SUBLEVEL=0
else
SUBLEVEL=$(SUBLEVELTMP)
endif

KVERNUM := $(shell expr $(KVERSION) \* 10000 + $(KPATCHLEVEL) \* 100 + $(SUBLEVEL))

#
# decide to use KBUILD for 2.6.x and next kernels
#
USE_KBUILD = $(shell [ $(KVERNUM) -ge 20600 ] && echo yes)

#
# common switches and defines
#
#$test makeopts start
DBG     = NO_DEBUG
MOD     = MODVERSIONS
PAR     = NO_PARPORT_SUBSYSTEM
USB     = USB_SUPPORT
PCI     = PCI_SUPPORT
PCIEC   = PCIEC_SUPPORT
DNG     = NO_DONGLE_SUPPORT
ISA     = ISA_SUPPORT
PCC     = NO_PCCARD_SUPPORT
NET     = NO_NETDEV_SUPPORT
RT      = NO_RT
#$test makeopts end

SRC     = src
TEMP    = /tmp
PWD     = $(shell pwd)

# only for installer: please modify the install parameters according to your
# configuration
PARMS   = "type=isa,sp"

#
# switch make verbosity on
#
ifeq ($(DBG),DEBUG)
VERBOSE = 1
else
VERBOSE = 0
endif

# no pci means no PCAN-Expresscard support
ifneq ($(PCI),PCI_SUPPORT)
PCIEC = NO_PCIEC_SUPPORT
endif

#
# PCAN-Expresscard support for kernels 2.4.x
#
ifneq ($(USE_KBUILD),yes)
PCIEC = NO_PCIEC_SUPPORT
endif

# native support of CONFIG_COMPAT can be disabled with:
# $ make COMPAT_SUPPORT=NO
ifeq ($(COMPAT_SUPPORT),NO)
PCAN_OPTS += -DNO_COMPAT
MAKE_OPTS += COMPAT_SUPPORT=NO
endif

# running with old devnum scheme can be enabled with:
# $ make NEW_DEVNUM_SCHEME=NO
ifeq ($(NEW_DEVNUM_SCHEME),NO)
PCAN_OPTS += -DOLD_DEVNUM_SCHEME
MAKE_OPTS += NEW_DEVNUM_SCHEME=NO
endif

# Test current system for UDEV support influence only for installation
# Note: $(UDEV) MUST NOT be empty because this value is used in a shell
#       comparison (see below)
define UDEV
$(shell if [ -d $(DESTDIR)/etc/udev/rules.d ]; then \
		echo yes; \
	else \
		echo no; \
	fi )
endef

#****************************************************************************

ifeq ($(findstring XENOMAI,$(RT)), XENOMAI)
#
# Define flags for XENOMAI installation only
#
USB   = NO_USB_SUPPORT
PCC   = NO_PCCARD_SUPPORT
NET   = NO_NETDEV_SUPPORT
PCIEC = NO_PCIEC_SUPPORT
PAR   = NO_PARPORT_SUBSYSTEM

#SKIN := xeno
SKIN :=

RT_DIR          ?= /usr/xenomai
RT_CONFIG       ?= $(RT_DIR)/bin/xeno-config
ifneq ($(SKIN),)
RT_CFLAGS       ?= $(shell $(RT_CONFIG) --$(SKIN)-cflags)
else
RT_CFLAGS       ?= $(shell $(RT_CONFIG) --skin native --cflags)
endif
endif

ifeq ($(RT), RTAI)
#
# Define flags for RTAI installation only
#
USB   = NO_USB_SUPPORT
PCC   = NO_PCCARD_SUPPORT
NET   = NO_NETDEV_SUPPORT
PCIEC = NO_PCIEC_SUPPORT
PAR   = NO_PARPORT_SUBSYSTEM

KBUILD_EXTRA_SYMBOLS ?= $(RT_DIR)/modules/Module.symvers

SKIN = module
RT_DIR          ?= /usr/realtime
RT_CONFIG       ?= $(RT_DIR)/bin/rtai-config
RT_CFLAGS       ?= $(shell $(RT_CONFIG) --$(SKIN)-cflags)

# To remove following Warning:
# <command-line>:0:0: warning: "_FORTIFY_SOURCE" redefined
RT_CFLAGS := -U_FORTIFY_SOURCE $(RT_CFLAGS)
endif

#****************************************************************************

ifeq ($(NET), AUTO_NETDEV_SUPPORT)
#
# Auto configure netdev support when kernel is compiled with CONFIG_CAN
#
HASAUTOCONF_H := $(shell [ -f $(HEADER_PATH)/autoconf.h ] && echo yes )

ifneq ($(HASAUTOCONF_H),yes)
$(error "AUTO_NETDEV_SUPPORT: Can't find $(HEADER_PATH)/autoconf.h !")
else

SOCKETCAN := $(shell cpp -dM -I$(KERNEL_LOCATION)/include $(HEADER_PATH)/autoconf.h | grep -c CONFIG_CAN)

ifeq ($(SOCKETCAN),0)
NET = NO_NETDEV_SUPPORT
else
NET = NETDEV_SUPPORT
endif

endif
endif

ifeq ($(NET), NETDEV_SUPPORT)
ifeq ($(OS_RELEASE_ID),"CentOS Linux")
EXTRA_CFLAGS += -Dndo_change_mtu=ndo_change_mtu_rh74
endif

K_4_8 := $(shell [ $(KVERNUM) -le 40800 ] && echo yes)
ifeq ($(K_4_8),yes)
# Must check if struct can_priv (include/linux/can/dev.h) auto restart mechanism
# is based on a timer or on delayed_work.
# Note: delayed_work has been included in Kernel v4.8 *BUT* this change has
# been backported in some 4.4...
DEV_H := $(KERNEL_LOCATION)/include/linux/can/dev.h
HAS_CAN_DEV_H := $(shell [ -f $(DEV_H) ] && echo yes)

ifneq ($(HAS_CAN_DEV_H),yes)
# OpenSUSE uses of some different organization tree: the source link connects
# to the header tree
DEV_H := $(dir $(KERNEL_LOCATION))source/include/linux/can/dev.h
HAS_CAN_DEV_H := $(shell [ -f $(DEV_H) ] && echo yes)
endif

ifeq ($(HAS_CAN_DEV_H),yes)
EXTRA_CFLAGS += $(shell  awk '/^struct can_priv {/ { in_can_priv=1 } /^};/ { in_can_priv=0 } /^[ \t]*struct[ \t]*timer_list[ \t]*restart_timer/ { if (in_can_priv) print "-DLINUX_CAN_RESTART_TIMER" }' $(DEV_H))
else
$(warning $(DEV_H) not found: netdev mode might fail to compile)
endif
endif
endif


#****************************************************************************
#
# preparation what to build or what to KBUILD
#
pcan-objs := $(SRC)/pcan_main.o $(SRC)/pcan_fops.o $(SRC)/pcan_fifo.o $(SRC)/pcan_filter.o 
pcan-objs += $(SRC)/pcan_parse.o $(SRC)/pcan_sja1000.o $(SRC)/pcan_common.o $(SRC)/pcan_timing.o

pcan-objs += $(SRC)/pcanfd_core.o $(SRC)/pcanfd_ucan.o

ifeq ($(USB),USB_SUPPORT)
pcan-objs += $(SRC)/pcan_usb_core.o $(SRC)/pcan_usb.o
pcan-objs += $(SRC)/pcan_usbpro.o
pcan-objs += $(SRC)/pcanfd_usb.o
endif

ifeq ($(PCI),PCI_SUPPORT)
pcan-objs += $(SRC)/pcan_pci.o
pcan-objs += $(SRC)/pcanfd_pci.o
pcan-objs += $(SRC)/pcan_pci_spi.o
endif

ifeq ($(PCIEC),PCIEC_SUPPORT)
pcan-objs += $(SRC)/pcan_pciec.o 
endif

ifeq ($(ISA),ISA_SUPPORT)
pcan-objs += $(SRC)/pcan_isa.o
endif

ifeq ($(DNG),DONGLE_SUPPORT)
pcan-objs += $(SRC)/pcan_dongle.o
endif

ifeq ($(PCC),PCCARD_SUPPORT)
pcan-objs += $(SRC)/pcan_pccard_core.o $(SRC)/pcan_pccard.o 
endif

ifeq ($(NET),NETDEV_SUPPORT)
pcan-objs += $(SRC)/pcan_netdev.o
endif

BINDIR = /usr/local/bin

ifeq ($(USE_KBUILD),yes)
#
# for local installation only: sometimes /etc/modprobe.conf.local is used
#
MODCONF := $(shell if test -d "$(DESTDIR)/etc/modprobe.d" ; then echo "$(DESTDIR)/etc/modprobe.d/$(DRV).conf" ; else echo "$(DESTDIR)/etc/modules.conf.local" ; fi)
ifeq ($(UDEV),yes)
MODINST = "install $(DRV) $(MODPROBE) --ignore-install $(DRV)"
else
MODINST = "install $(DRV) $(MODPROBE) --ignore-install $(DRV); $(BINDIR)/pcan_make_devices 2"
endif

#
# what's the target
#
TARGET = $(DRV).ko
obj-m := $(DRV).o

#
# add flags to standard flags
#
# Note: to get listing from Kernel module compilation:
# edit scripts/Makefile.build
# -cmd_cc_o_c = $(CC) $(c_flags) -c -o $(@D)/.tmp_$(@F) $<
# +cmd_cc_o_c = $(CC) $(c_flags) -c -Wa,-adhln=$<.lst -o $(@D)/.tmp_$(@F) $<
#
EXTRA_CFLAGS += -I$(PWD) -D$(DBG) $(PCAN_OPTS) $(RT_CFLAGS) $(DRV_CFLAGS)

# Kernel enables the '-Werror=date-time' for gcc 4.9. 
GCC_VERMAJ := $(shell $(CC) -dumpversion | cut -d. -f1)
GCC_VERMIN := $(shell $(CC) -dumpversion | cut -d. -f2)

DATE_TIME_WARNING := $(shell if [ $(GCC_VERMAJ) -gt 4 ]; then echo y; else echo n; fi)
ifeq ($(DATE_TIME_WARNING),n)
DATE_TIME_WARNING := $(shell if [ $(GCC_VERMAJ) -eq 4 ] && [ $(GCC_VERMIN) -ge 9 ]; then echo y; else echo n; fi)
endif
ifeq ($(DATE_TIME_WARNING),y)
# removing the "date-time" error is not enough since it's always a warning:
# removing any "date-time" problem is ok.
#EXTRA_CFLAGS += -Wno-error=date-time
EXTRA_CFLAGS += -Wno-date-time
endif

# checking endianess needs 'sparse' tool (apt-get install sparse)
#CMD_OPTS = C=1 CF=-D__CHECK_ENDIAN__

CMDLINE = $(MAKE) -C $(KERNEL_LOCATION) $(CMD_OPTS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" V=$(VERBOSE) modules

#
# do it in another way for kernels less than 2.6.5
# SUBDIRS will be removed from Kernel as of version 5.3. To avoid the Warning
# message added from 5.0, donot use it with Kernel >= 5.0.
#
define USE_SUBDIRS
$(shell [ $(KVERNUM) -ge 20605 ] && [ $(KVERNUM) -lt 50000 ] && echo yes)
endef

ifneq ($(USE_SUBDIRS),yes)
CMDLINE += M=$(PWD)
else
CMDLINE += SUBDIRS=$(PWD)
endif
# USE_SUBDIRS

#****************************************************************************
# do it
#
.PHONY: $(DKMS_CONF)
all: message $(DKMS_CONF)
	$(CMDLINE)

# simply to avoid boring "error" messages when built from root dir
depend:

#****************************************************************************
# !USE_KBUILD
else

#
# additional common switches and defines
#
CC      = gcc
LD      = ld
INC     = $(KERNEL_LOCATION)/include

DEP     = .depend

#
# for local installation only: sometimes /etc/modules.conf.local is used
#
MODCONF = "$(DESTDIR)/etc/modules.conf"
MODINST = "post-install $(DRV) $(BINDIR)/pcan_make_devices 2"

#
# what's the target
#
TARGET = $(DRV).o

#
# compile flags
#
CFLAGS  = -O2 -D__KERNEL__ -DMODULE -Wall -I$(INC) -I. -D$(DBG) $(PCAN_OPTS) $(RT_CFLAGS)

#
# do it
#
all: message $(TARGET) $(DKMS_CONF)

$(TARGET): $(pcan-objs)
	$(LD) -r $^ -o $@

# catch include file depencies
ifeq ($(DEP),$(wildcard $(DEP)))

depend:
	makedepend -f$(DEP) -- $(CFLAGS) -- $(addsuffix .c, $(basename $(pcan-objs))) -I$(INC)

include $(DEP)

else

depend:
	touch $(DEP)
	makedepend -f$(DEP) -- $(CFLAGS) -- $(addsuffix .c, $(basename $(pcan-objs))) -I$(INC)
endif

endif
# !USE_KBUILD
#****************************************************************************

# In case dkms is used, dkms.conf MUST be rebuilt each time the driver is
# rebuilt because it saves $(MAKE_OPTS)
$(DKMS_CONF):
	@echo "PACKAGE_NAME=\"$(DKMS_DRV)\"" > $(DKMS_CONF)
	@echo "PACKAGE_VERSION=\"$(DKMS_VER)\"" >> $(DKMS_CONF)
	@echo "CLEAN=\"make clean\"" >> $(DKMS_CONF)
	@echo "MAKE[0]=\"make DKMS_KERNEL_DIR=\$$kernel_source_dir $(MAKE_OPTS)\"" >> $(DKMS_CONF)
	@echo "BUILT_MODULE_NAME[0]=\"$(DRV)\"" >> $(DKMS_CONF)
	@echo "BUILT_MODULE_LOCATION[0]=\".\"" >> $(DKMS_CONF)
	@echo "DEST_MODULE_LOCATION[0]=\"/updates\"" >> $(DKMS_CONF)
	@echo "AUTOINSTALL=\"yes\"" >> $(DKMS_CONF)

# $ make clean
clean:
	rm -f $(SRC)/*.o $(SRC)/*~ $(SRC)/.*cmd *.o .*.o *.ko *~ .*cmd  $(DRV).mod.c Module.symvers modules.order .cache.mk $(DRV).mod

ifneq ($(USE_KBUILD),yes)
# $ make mrproper
mrproper: clean
	rm -f $(DEP)
endif

.PHONY: usb_only pci_only

usb_only:
	$(MAKE) PCI=NO_PCI_SUPPORT PAR=NO_PARPORT_SUBSYSTEM ISA=NO_ISA_SUPPORT PCC=NO_PCCARD_SUPPORT DNG=NO_DONGLE_SUPPORT

pci_only:
	$(MAKE) USB=NO_USB_SUPPORT PAR=NO_PARPORT_SUBSYSTEM ISA=NO_ISA_SUPPORT PCC=NO_PCCARD_SUPPORT DNG=NO_DONGLE_SUPPORT

#********** this entry is reserved for root access only *********************
compat:
	@if [ -f $(DESTDIR)/etc/modprobe.d/$(DRV) ] ; then \
		mv $(DESTDIR)/etc/modprobe.d/$(DRV) $(DESTDIR)/etc/modprobe.d/$(DRV).conf ; \
		echo "Info: Moved /etc/modprobe.d/$(DRV) to /etc/modprobe.d/$(DRV).conf for compatibility reason."; \
	fi

.PHONY: install install_files install_udev install_module

ifeq ($(DESTDIR_DEV),)
DESTDIR_DEV = $(DESTDIR)
endif

ifneq ($(DESTDIR),)
DEPMOD_OPTS = --basedir $(DESTDIR)
endif
ifneq ($(KERNEL_VERSION),)
DEPMOD_OPTS += $(KERNEL_VERSION)
endif


install: install_files install_udev install_module

install_files: compat
	@mkdir -p $(DESTDIR)$(BINDIR)
	@cp -f pcan_make_devices pcanosdiag.sh lspcan $(DESTDIR)$(BINDIR)
	@chmod 744 $(DESTDIR)$(BINDIR)/pcan_make_devices
	@chmod 755 $(DESTDIR)$(BINDIR)/lspcan
	@chmod 755 $(DESTDIR)$(BINDIR)/pcanosdiag.sh
	@mkdir -p $(DESTDIR_DEV)/usr/include
	@cp -f pcan.h pcanfd.h $(DESTDIR_DEV)/usr/include
	@chmod 644 $(DESTDIR_DEV)/usr/include/pcan.h $(DESTDIR_DEV)/usr/include/pcanfd.h

	@if [ ! -f $(MODCONF) ] || test -z $(shell grep -ls $(DRV) $(MODCONF)); then\
		mkdir -p $(shell dirname $(MODCONF));\
		cp $(MODCONF) $(MODCONF)~ 2> /dev/null;\
		if [ -f pcan.conf ]; then\
			cp pcan.conf $(MODCONF);\
			(echo;\
			 echo "$(MODINST)";\
			 echo;\
			) >> $(MODCONF);\
		else\
			(echo "# pcan - automatic made entry, begin --------";\
			 echo "# if required add options and remove comment";\
			 echo "# options $(DRV) $(PARMS)";\
			 echo "$(MODINST)";\
			 echo "# pcan - automatic made entry, end ----------";\
			 echo;\
			 ) > $(MODCONF);\
		fi;\
	else\
		echo "Info: Left current '$(DRV)'-entry in $(MODCONF) untouched.";\
	fi

# kvernum=$$(($(KVERSION)*10000+$(KPATCHLEVEL)*100+$(SUBLEVEL)));
install_udev:
ifeq ($(UDEV),yes)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f udev/pcan_usb_minor_check.bash $(DESTDIR)$(BINDIR)
	chmod 744 $(DESTDIR)$(BINDIR)/pcan_usb_minor_check.bash
	cp -f udev/45-pcan.rules $(DESTDIR)/etc/udev/rules.d
	@echo "Info: Copied 45-pcan.rules to $(DESTDIR)/etc/udev/rules.d."
# can't rmmod peak_pci for kernel versions < 3.4 (crash)
	@peak_pci=0;\
	kvernum=$(KVERNUM);\
	for m in $(shell awk '/^blacklist/ { print $$2 }' udev/blacklist-peak.conf); do\
		if [ $$m = "peak_pci" ] && [ $$kvernum -lt 30400 ]; then\
			peak_pci=`lsmod | grep -ce peak_pci`;\
		else\
			rmmod $$m 2> /dev/null;\
		fi;\
	done;\
	if [ $$peak_pci -ne 0 ]; then\
		echo "Info: peak_pci can't be removed from the Kernel";\
		echo "      You should reboot to complete the installation.";\
	fi
	cp -f udev/blacklist-peak.conf $(shell dirname $(MODCONF))
	chmod 644 $(shell dirname $(MODCONF))/blacklist-peak.conf
	@echo "Info: mainline drivers removed and blacklisted in"
	@echo "      $(shell dirname $(MODCONF))/blacklist-peak.conf"
ifeq ($(DESTDIR),)
	$(UDEVADM) control --reload-rules
endif
else
	@echo "Info: No UDEV installation found."
endif

#DKMS_STDERR = /dev/null
DKMS_STDERR = /tmp/$(DKMS_DRV)-$(DKMS_VER)-dkms.stderr

install_module: $(TARGET)
	@-rmmod $(DRV) 2> /dev/null || true
# be sure to remove every pcan.ko before doing depmod
	@-find $(MODULES_DIR) -name $(TARGET) -delete
ifeq ($(USE_DKMS),yes)
	@echo "- Trying dkms..."
# Must exist before dkms add
	[ -h $(DESTDIR)/usr/src/$(DKMS_DRV)-$(DKMS_VER) ] || \
		ln -s $(PWD) $(DESTDIR)/usr/src/$(DKMS_DRV)-$(DKMS_VER)

# Add driver (if it isn't)
	[ -d $(DESTDIR)$(DKMS_TREE)/$(DKMS_DRV)/$(DKMS_VER) ] || \
		$(DKMS_BIN) add $(DKMS_DRV)/$(DKMS_VER)

# Finally, install it
	$(DKMS_BIN) install --force $(DKMS_DRV)/$(DKMS_VER) -k $(KERNEL_VERSION)

#	-$(DKMS_BIN) install --force --no-clean-kernel -m $(DKMS_DRV) -v $(DKMS_VER) --kernelsourcedir $(KERNEL_LOCATION) 2> $(DKMS_STDERR) || true
else
	@echo "- manually installing $(DRV) under $(INSTALL_LOCATION)..."
	mkdir -p $(INSTALL_LOCATION)
	cp -f $(TARGET) $(INSTALL_LOCATION)/$(TARGET)
	@echo "- Building dependencies..."
	$(DEPMOD) $(DEPMOD_OPTS)
endif

dkms_status:
	@echo "Getting dkms status for $(DKMS_DRV) $(DKMS_VER) for $(KERNEL_VERSION):" && echo
	$(DKMS_BIN) status $(DKMS_DRV)/$(DKMS_VER) -k $(KERNEL_VERSION) 2> $(DKMS_STDERR) | grep -q -e "^$(DKMS_DRV), $(DKMS_VER), $(KERNEL_VERSION), .*: installed$$" && [ $$? -eq 0 ] && echo "=> Ok" || echo "=> NOk"

.PHONY: uninstall uninstall_files uninstall_module
uninstall: uninstall_module uninstall_files

uninstall_files:
	-rm -f	$(DESTDIR_DEV)/usr/include/pcan.h \
		$(DESTDIR_DEV)/usr/include/pcanfd.h \
		$(DESTDIR)$(BINDIR)/pcan_make_devices \
		$(DESTDIR)$(BINDIR)/lspcan \
		$(DESTDIR)$(BINDIR)/pcanosdiag.sh \
		$(DESTDIR)/etc/udev/rules.d/45-pcan.rules \
		$(DESTDIR)$(BINDIR)/pcan_usb_minor_check.bash \
		$(shell dirname $(MODCONF))/blacklist-peak.conf \
		$(MODCONF)

uninstall_module:
	-rmmod $(DRV) 2> /dev/null || true
	if [ -f $(INSTALL_LOCATION)/$(TARGET) ]; then\
		rm -f $(INSTALL_LOCATION)/$(TARGET);\
		$(DEPMOD) $(DEPMOD_OPTS);\
	fi
ifneq ($(DKMS_BIN),)
	[ ! -d $(DESTDIR)/var/lib/dkms/$(DKMS_DRV) ] || \
		$(DKMS_BIN) remove $(DKMS_DRV)/$(DKMS_VER) --all || true
endif
	-rm -f	$(DESTDIR)/usr/src/$(DKMS_DRV)-$(DKMS_VER) \
		$(DKMS_STDERR)
	-rm -rf $(DESTDIR)/var/lib/dkms/$(DKMS_DRV)

# informations during build of driver
.PHONY: message
message:
	@echo "***"
ifeq ($(RT),XENOMAI3)
	@echo "*** Making the Xenomai 3.x version of the $(DRV) driver"
else
ifeq ($(RT),XENOMAI)
	@echo "*** Making the Xenomai 2.6 version of the $(DRV) driver"
else
ifeq ($(RT),RTAI)
	@echo "*** Making the RTAI 5.x version of the $(DRV) driver"
else
ifeq ($(NET),NETDEV_SUPPORT)
	@echo "*** Making $(DRV) driver in netdev mode"
else
	@echo "*** Making $(DRV) driver in chardev mode"
endif
endif
endif
endif
	@echo "***"
	@echo "*** Host OS Release=$(OS_RELEASE_ID) v$(OS_RELEASE_VER)"
	@echo "*** Host machine kernel version=$(shell uname -r)"
	@echo "*** Driver kernel version=$(KERNEL_VERSION) ($(KVERSION).$(KPATCHLEVEL).$(SUBLEVEL))"
	@echo "*** Path to kernel sources=$(KERNEL_LOCATION)"
	@echo "*** use KBUILD=$(USE_KBUILD)"
	@echo "*** use DKMS=$(USE_DKMS)"
	@echo "*** $(CC) version=$(shell $(CC) -dumpversion)"
	@echo "*** gcc versions detected on this host:"
	@-echo -n "*** "; ls /usr/bin/gcc-[0-9]* 2> /dev/null
	@echo "***"

#endif
# HASVERSION_H

debug_kernel_version:
	@echo "KERNEL_LOCATION=$(KERNEL_LOCATION)"
	@echo "HEADER_PATH=$(HEADER_PATH)"
	@echo "Major=$(KVERSION) Minor=$(KPATCHLEVEL) Subminor=$(SUBLEVEL)"
	@echo "KVERNUM=$(KVERNUM)"

xeno26:
	$(MAKE) RT=XENOMAI

xeno:
	$(MAKE) RT=XENOMAI3

rtai:
	$(MAKE) RT=RTAI

chardev:
	$(MAKE) NET=NO_NETDEV_SUPPORT

netdev:
	$(MAKE) NET=NETDEV_SUPPORT

install_with_dkms:
	$(MAKE) DKMS=DKMS_SUPPORT install

debug:
	$(MAKE) DBG=DEBUG

ifneq ($(PCI),NO_PCI_SUPPORT)
debug_pci_lite debug-pci-lite:
	$(MAKE) DBG=DEBUG_PCI_LITE

debug_pci debug-pci:
	$(MAKE) DBG=DEBUG_PCI
endif

ifneq ($(USB),NO_USB_SUPPORT)
debug_usb_lite debug-usb-lite:
	$(MAKE) DBG=DEBUG_USB_LITE

debug_usb debug-usb:
	$(MAKE) DBG=DEBUG_USB
endif

RPI_MAKE_OPTS = PCI=NO_PCI_SUPPORT ISA=NO_ISA_SUPPORT DNG=NO_DONGLE_SUPPORT PCC=NO_PCCARD_SUPPORT

rpi:
	$(MAKE) $(RPI_MAKE_OPTS)

rpi_netdev:
	$(MAKE) $(RPI_MAKE_OPTS) NET=NETDEV_SUPPORT
# DO NOT DELETE
