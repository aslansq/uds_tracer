/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * pcan-settings.c - a small program to get and set various parameters of
 *
 * Copyright (C) 2010-2020  PEAK System-Technik GmbH <www.peak-system.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contact:    <linux@peak-system.com>
 * Maintainer: Stephane Grosjean <s.grosjean@peak-system.com>
 * Author:     Klaus Hitschler <klaus.hitschler@gmx.de>
 */

/* set here current release for this program */
#define CURRENT_RELEASE	"Release_20181220_n"
#define PRGNAME		"pcan-settings"

#define USES_32BITS_DEVICENO

/*
 * INCLUDE
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>	/* exit */
#include <signal.h>
#include <string.h>
#include <stdlib.h>	/* strtoul */
#include <fcntl.h>	/* O_RDWR */
#include <sys/ioctl.h>

#include <pcanfd.h>	/* this program does not use libpcan */
#include <ctype.h>
#include <popt.h>
#include <err.h>

#if !defined(NO_RT) && !defined(__COBALT__)
#include <rtdm/rtdm.h>

#define __open(x, y)		rt_dev_open(x, y)
#define __errno_ioctl(x, y, z)	-rt_dev_ioctl(x, y, z)
#define __ioctl(x, y, z)	rt_dev_ioctl(x, y, z)
#define __close(fp)		rt_dev_close(fp)

#define __fprintf		rt_fprintf

#else
static inline int __open(char *pfn, int flg)
{
	int fd = open(pfn, flg);
	return (fd >= 0) ? fd : -errno;
}
#define __errno_ioctl(x, y, z)	(ioctl(x, y, z) ? errno : 0)
#define __ioctl(x, y, z)	ioctl(x, y, z)
#define __close(fp)		close(fp)

#define __fprintf		fprintf
#endif

#define DEFAULT_NODE	"/dev/pcan32"  /* currently only USB settings */
#ifndef bool
#define bool		int
#define true		1
#define false		0
#endif

/* a help to get command line interpretation separated from control */
#define SERIALNO	0x00000001
#define DEVICENO	0x00000002
#define VERBOSE		0x00000004
#define QUIET		0x00000008
#define MSD		0x00000010

int getSerialNumber(int nFileNo, unsigned int *pserialNo)
{
	int err;
	TPEXTRAPARAMS params;

	params.nSubFunction = SF_GET_SERIALNUMBER;

	err = __errno_ioctl(nFileNo, PCAN_EXTRA_PARAMS, &params);

	if (!err)
		*pserialNo = params.func.dwSerialNumber;

	return err;
}

int getDeviceNumber(int nFileNo, __uint32_t *pdeviceNo)
{
	int err;
	TPEXTRAPARAMS params;

	params.nSubFunction = SF_GET_HCDEVICENO;

	err = __errno_ioctl(nFileNo, PCAN_EXTRA_PARAMS, &params);

	if (!err)
#ifndef USES_32BITS_DEVICENO
		*pdeviceNo = (__uint32_t )params.func.ucHCDeviceNo;
#else
		*pdeviceNo = (__uint32_t )params.func.dwSerialNumber;
#endif
	return err;
}

int setDeviceNumber(int nFileNo, __uint32_t deviceNo)
{
	TPEXTRAPARAMS params;

	params.nSubFunction = SF_SET_HCDEVICENO;
#ifndef USES_32BITS_DEVICENO
	params.func.ucHCDeviceNo = (BYTE) deviceNo;
#else
	params.func.dwSerialNumber = (DWORD )deviceNo;
#endif
	return __errno_ioctl(nFileNo, PCAN_EXTRA_PARAMS, &params);
}

int set_mass_storage_mode(int nFileNo)
{
	__u32 onoff = 1;

#if 0
	/* libpcanfd version */
	int err = pcanfd_set_option(nFileNo, PCANFD_OPT_MASS_STORAGE_MODE,
				    &onoff, sizeof(onoff));
#else
	struct pcanfd_option opt = {
		.name = PCANFD_OPT_MASS_STORAGE_MODE,
		.size = sizeof(onoff),
		.value = &onoff,
	};
	int err = ioctl(nFileNo, PCANFD_SET_OPTION, &opt);
#endif
	return err;
}

/* start here */
int main(int argc, char *argv[])
{
	int ret = 0;
	char *szDevNode = DEFAULT_NODE;
	unsigned int serialNo = 0;
	__uint32_t deviceNo = 0;
	poptContext context;
	int result = 0;
	int verbose = false;
	int deviceArgSet = false;

	errno = 0;

	/* interpret command line */
	struct poptOption cmdLineOpts[] = {
		{ "deviceNode", 'f',
			POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
			&szDevNode, 0,
			"Set path to PCAN device",
			"'device file path'"
		},
		{ "SerialNo", 's',
			POPT_ARG_NONE,
			NULL, SERIALNO,
			"Get serial number",
			"'non-volatile serial number'"
		},
		{ "DeviceNo", 'd',
			POPT_ARG_LONG | POPT_ARGFLAG_OPTIONAL,
			&deviceNo, DEVICENO,
			"Get or set device number",
			"'device number'"
		},
		{ "verbose", 'v',
			POPT_ARG_NONE,
			NULL, VERBOSE,
			"Make it verbose",
			""
		},
		{ "quiet", 'q',
			POPT_ARG_NONE,
			NULL, QUIET,
			"No display at all",
			""
		},
		{ "MSD", 'M',
			POPT_ARG_NONE,
			NULL, MSD,
			"Switch device in Mass Storage Device mode (root privileges needed)",
			""
		},
		POPT_AUTOHELP
		POPT_TABLEEND
	};

	context = poptGetContext(PRGNAME, argc, (const char ** )argv,
								cmdLineOpts, 0);
	while ((ret = poptGetNextOpt(context)) >= 0) {
		result |= ret;
		switch (ret) {
		case SERIALNO:
			break;
		case DEVICENO:
			deviceArgSet = (poptGetOptArg(context)) ? true : false;
			break;
		case VERBOSE:
			verbose = true;
			break;
		case QUIET:
			close(1);
			close(2);
		case MSD:
			break;
		}
	}

	errno = EINVAL;
	result &= ~VERBOSE; /* remove possible VERBOSE from result */

	/* some sanity checks */
	/* 1st illegal command line switches */
	if (ret < -1)
		err(errno, "err %d: illegal command line switches %s",
			ret, poptBadOption(context, POPT_BADOPTION_NOALIAS));

	/* 2nd: only a device node given */
	if (!result) {
		poptPrintUsage(context, stderr, 0);
		err(errno, "more than a device node required");
	}

	/* do what was intended to do */
	ret = 0;
	if (result) {
		int nFileNo;

		/* open path to device */
		if ((nFileNo = __open(szDevNode, O_RDWR)) < 0)
			err(errno, "open(%s) failed", szDevNode);

		/* get serial number */
		if ((result & SERIALNO) && !ret) {
			ret = getSerialNumber(nFileNo, &serialNo);
			if (ret)
				printf(PRGNAME ": Failed to get serial number (errno=%d)\n", errno);
			else if (verbose)
				printf(PRGNAME ": Serial Number is 0x%08x\n", serialNo);
			else
				printf("0x%08x\n", serialNo);
		}

		/* set or get device number */
		if ((result & DEVICENO) && !ret) {
			/* argument given, set value */
			if (deviceArgSet) {
				ret = setDeviceNumber(nFileNo, deviceNo);
				if (ret)
					printf(PRGNAME ": Failed to set device number (errno=%d)\n", errno);
				else if (verbose)
					printf(PRGNAME ": Device Number set to %u\n", deviceNo);
			} else {
				ret = getDeviceNumber(nFileNo, &deviceNo);
				if (ret)
					printf(PRGNAME ": Failed to get device number (errno=%d)\n", errno);
				else if (verbose)
					printf(PRGNAME ": Device Number is %u\n", deviceNo);
				else
					printf("%u\n", deviceNo);
			}
		}

		if ((result & MSD) && !ret) {
			ret = set_mass_storage_mode(nFileNo);
			if (ret) {
				printf(PRGNAME ": failed to switch \"%s\" in Mass Storage Device mode\n", szDevNode);
			} else {
				printf(PRGNAME ": Mass Storage mode successfully set\n");
				if (verbose) {
					printf("\n");
					printf("The device node \"%s\" doesn't exist anymore.\n", szDevNode);
				}

				printf("Please wait for the LED(s) of the USB device to flash, then, if not\n");
				printf("automatically done by the system, mount a VFAT filesystem on the newly\n");
				printf("detected USB Mass Storage Device \"/dev/sdX\".\n");
				if (verbose) {
					printf("\n");
					printf("For example:\n");
					printf("# mkdir -p /mnt/pcan-usb\n");
					printf("# mount -t vfat /dev/sdX /mnt/pcan-usb\n");
					printf("# ls -al /mnt/pcan-usb\n");
				}
			}
		}

		/* close path to device */
		__close(nFileNo);
	}

	if (ret < 0)
		perror(PRGNAME ": configuration @ set or get");

	poptFreeContext(context);
	return ret;
}
