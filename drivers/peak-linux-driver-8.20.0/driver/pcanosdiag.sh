#!/bin/sh
#
# pcanosdiag.sh
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
# Author: Stephane Grosjean <s.grosjean@peak-system.com>
# Contact: <linux@peak-system.com>
# Copyright (c) 2001-2021 PEAK-System GmbH <www.peak-system.com>
#
# History:
#
# 1.0.6 - Stephane Grosjean <s.grosjean@peak-system.com>
# - add cat blacklist-peak.conf
#
VERSION=1.0.6

CAT=/bin/cat
DATE=/bin/date
DMESG=/usr/bin/dmesg
FIND=/usr/bin/find
GREP=/bin/grep
LSCPU=/bin/lscpu
LSB_RELEASE=/etc/lsb-release
LSMOD=/bin/lsmod
LSPCAN=/usr/local/bin/lspcan
LSPCI=/bin/lspci
LSUSB=/bin/lsusb
MODINFO=/usr/sbin/modinfo
MODPROBE=/sbin/modprobe
PS=/bin/ps
PCANINFO=/usr/local/bin/pcaninfo
RMMOD=/sbin/rmmod
UNAME=/bin/uname

SYSFS_MODPARAM=/sys/module/pcan/parameters
SYSFS_PCAN=/sys/class/pcan

#LOG=/tmp/pcanosdiag-`$DATE +"%Y%m%d-%H%M%S"`.log
LOG=/tmp/pcanosdiag-$VERSION-`$DATE +"%Y%m%d_%H%M%S"`.log

[ -z "$EUID" ] && EUID=`id -u`
step=0

run()
{
	echo --------------------------------------------------------------------------------
	step=`expr $step + 1`
	echo "STEP $step # $*"
	if test ! -x $1; then
		bin=`basename $1`
		echo "Warning: $1 not found! Trying with $bin only:"
		cmd=`echo $* | sed -e "s#$1#$bin#"`
	else
		cmd="$*"
	fi

	eval $cmd
	return $?

	if test -x $1; then
		#eval $*
		eval $cmd
		return $?
	fi
	echo "'$1' not found: can't run '$*'"
	return 2
}

#-- main

echo "$0 v$VERSION"
[ $EUID -ne 0 ] && echo "Error: you must be root" && exit 1

#-- Redirect stdout/stderr to $LOG:
# Save stdout to fd=3
exec 3<&1

# Open stdout as $LOG file for write.
exec 1>$LOG

# Redirect standard error to standard output
exec 2>&1

#-- Start
echo "$0 v$VERSION"
run $DATE +\"%F %R:%S %s\"
run $CAT /proc/uptime
run $CAT $LSB_RELEASE
run $UNAME -a
run $CAT /proc/cmdline
run $LSCPU
run $CAT /proc/interrupts
run $PS -ef

#-- USB specific
run $LSUSB
run $LSUSB -v -d 0c72:

#-- PCI/PCIe specific
run $LSPCI
run $LSPCI -M "2>" /dev/null
run $LSPCI -vv -d 1c:

#-- pcan specific
run $FIND /lib/modules -name "pcan.ko"
for p in `$FIND /lib/modules -name "pcan.ko"`; do
	run $MODINFO $p
done
run $LSMOD "|" $GREP -e "^peak"
run $LSMOD "|" $GREP -e "^pcan"
run $RMMOD pcan
run $LSMOD "|" $GREP -e "^pcan"
run $DMESG

run $CAT /etc/modprobe.d/pcan.conf
run $CAT /etc/modprobe.d/blacklist-peak.conf
run $MODPROBE pcan
run $DMESG "|" $GREP pcan
run $CAT /proc/pcan
run $LSPCAN -t -T -i
run $CAT $SYSFS_PCAN/pcan*/adapter_name
run $CAT $SYSFS_PCAN/pcan*/adapter_partnum
run $CAT $SYSFS_MODPARAM/fdusemsi
run $CAT $SYSFS_MODPARAM/usemsi
run $CAT $SYSFS_MODPARAM/rxqsize
run $CAT $SYSFS_MODPARAM/txqsize
run $CAT $SYSFS_MODPARAM/drvclkref
run $CAT $SYSFS_MODPARAM/deftsmode
run $CAT $SYSFS_MODPARAM/fast_fwd

#-- pcanbasic
run $PCANINFO --all

#-- End: restore stdout from 3 and close 3
exec 1<&3 3<&-

echo "Done."
if test ! -f $LOG; then
	echo "Error: $LOG not found!"
	exit 2
fi

echo "Please send $LOG to <support@peak-system.com>"
exit 0
