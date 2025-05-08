/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * receivetest.c - a small program to test the receive features of pcan driver
 *                 and the supporting shared library
 *
 * for example of realtime variant look at "receivetest_rt.c"
 *
 * Copyright (C) 2009-2020  PEAK System-Technik GmbH <www.peak-system.com>
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
#define CURRENT_RELEASE "Release_20150611_n"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   /* exit */
#include <signal.h>
#include <string.h>
#include <stdlib.h>   /* strtoul */
#include <fcntl.h>    /* O_RDWR */
#include <libpcan.h>
#include <ctype.h>

#include "src/common.h"

#define DEFAULT_NODE "/dev/pcan0"
#ifndef bool
#define bool	int
#define true	1
#define false	0
#endif

HANDLE h;
const char *current_release;
__u32 rx_msg_count = 0;

/* what has to be done at program exit */
void do_exit(int error)
{
	if (h) {
		print_diag("receivetest");
		CAN_Close(h);
	}
	printf("receivetest: finished (%d): %u message(s) received\n\n",
			error, rx_msg_count);
	exit(error);
}

/* the signal handler for manual break Ctrl-C */
void signal_handler(int signal)
{
	do_exit(0);
}

/* what has to be done at program start */
void init()
{
	/* install signal handlers */
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
}

/* open the CAN port */
int open_can(bool bDevNodeGiven, bool bTypeGiven, const char *szDevNode,
	     int nType, __u32 dwPort, __u16 wIrq)
{
	int err = 0;
	if ((bDevNodeGiven) || (!bDevNodeGiven && !bTypeGiven))
		h = LINUX_CAN_Open(szDevNode, O_RDWR);
	else {
		/* please use what is appropriate
		 * HW_DONGLE_SJA
		 * HW_DONGLE_SJA_EPP
		 * HW_ISA_SJA
		 * HW_PCI
		 * HW_PCIE_FD
		 * HW_USB
		 * HW_USB_PRO
		 * HW_USB_FD
		 * HW_USB_PRO_FD
		 * HW_USB_X6
		 */
		h = CAN_Open(nType, dwPort, wIrq);
	}

	if (!h)
		return 1;

	return err;
}

/* read from CAN forever - until manual break */
int read_loop(bool display_on, __u32 dwMaxLoop)
{
	__u32 l = 0;

	/* read in endless loop until Ctrl-C */
	for (; ;) {
		TPCANRdMsg m;
		__u32 status;

		if (LINUX_CAN_Read(h, &m)) {
			perror("receivetest: LINUX_CAN_Read()");
			return errno;
		}

		rx_msg_count++;

		if (display_on)
			print_message_ex("receivetest", &m);

		/* check if a CAN status is pending */
		if (m.Msg.MSGTYPE & MSGTYPE_STATUS) {
			status = CAN_Status(h);
			if ((int)status < 0) {
				errno = nGetLastError();
				perror("receivetest: CAN_Status()");
				return errno;
			}

			printf("receivetest: pending CAN status 0x%04x read.\n",
				(__u16)status);
		}

		if (dwMaxLoop)
			if (++l >= dwMaxLoop)
				break;
	}

	return 0;
}

static void hlpMsg(void)
{
	printf("receivetest - a small test program which receives and prints CAN messages.\n");
	printf("usage: receivetest [-b=BTR0BTR1] [-e] [-?]\n");
	printf("                   {[-f=devicenode] | {[-t=type] [-p=port [-i=irq]]}}\n");
	printf("options:\n");
	printf("-f=devicenode	path to pcan device node (default=%s)\n",
		DEFAULT_NODE);
	printf("-t=type		type of interface (pci, sp, epp, isa, pccard, usb (default=pci)\n");
	printf("-p=port		port number if applicable (default=1st port of type)\n");
	printf("-i=irq		irq number if applicable (default=irq of 1st port)\n");
	printf("-b=BTR0BTR1	bitrate code in hex (default=see /proc/pcan)\n");
	printf("-e		accept extended frames (default=standard frames only)\n");
	printf("-d=no		donot display received messages (default=yes)\n");
	printf("-n=mloop	number of loops to run before exit (default=infinite)\n");
	printf("-? or --help	displays this help\n");
	printf("\n");
}

/* here all is done */
int main(int argc, char *argv[])
{
	char *ptr;
	int i;
	int nType = HW_PCI;
	__u32 dwPort = 0;
	__u16 wIrq = 0;
	__u16 wBTR0BTR1 = 0;
	int nExtended = CAN_INIT_TYPE_ST;
	const char  *szDevNode = DEFAULT_NODE;
	__u32 dwMaxLoop = 0;
	bool bDevNodeGiven = false;
	bool bTypeGiven = false;
	bool bDisplayOn = true;
	char txt[VERSIONSTRING_LEN];

	errno = 0;

	current_release = CURRENT_RELEASE;
	disclaimer("receivetest");

	init();

	/* decode command line arguments */
	for (i = 1; i < argc; i++) {
		char c;

		ptr = argv[i];

		while (*ptr == '-')
			ptr++;

		c = *ptr;
		ptr++;

		if (*ptr == '=')
			ptr++;

		switch(tolower(c)) {
		case 'f':
			szDevNode = ptr;
			bDevNodeGiven = true;
			break;
		case 'd':
			if (strcmp(ptr, "no") == 0)
				bDisplayOn = false;
			break;
		case 't':
			nType = getTypeOfInterface(ptr);
			if (!nType) {
				errno = EINVAL;
				printf("receivetest: unknown type of interface!\n");
				goto error;
			}
			bTypeGiven = true;
			break;
		case 'p':
			dwPort = strtoul(ptr, NULL, 16);
			break;
		case 'i':
			wIrq = (__u16)strtoul(ptr, NULL, 10);
			break;
		case 'e':
			nExtended = CAN_INIT_TYPE_EX;
			break;
		case '?':
		case 'h':
			hlpMsg();
			goto error;
			break;
		case 'b':
			wBTR0BTR1 = (__u16)strtoul(ptr, NULL, 16);
			break;
		case 'n':
			dwMaxLoop = strtoul(ptr, NULL, 0);
			break;
		default:
			errno = EINVAL;
			perror("receivetest: unknown command line argument!\n");
			goto error;
			break;
		}
	}

	/* simple command input check */
	if (bDevNodeGiven && bTypeGiven) {
		errno = EINVAL;
		perror("receivetest: device node and type together is useless");
		goto error;
	}

	/* give some information back */
	if (!bTypeGiven) {
		printf("receivetest: device node=\"%s\"\n", szDevNode);
	} else {
		printf("receivetest: type=%s", getNameOfInterface(nType));
		switch (nType) {
		case HW_USB:
		case HW_USB_PRO:
		case HW_USB_FD:
		case HW_USB_PRO_FD:
		case HW_USB_X6:
			if (dwPort)
				printf(", %d. device\n", dwPort);
			else
				printf(", standard device\n");
			break;

		default:
			if (dwPort) {
				if ((nType == HW_PCI) || (nType == HW_PCIE_FD))
					printf(", %d. PCI device", dwPort);
				else
					printf(", port=0x%08x", dwPort);
			} else
				printf(", port=default");

			if (wIrq && !(nType == HW_PCI || nType == HW_PCIE_FD))
				printf(" irq=0x%04x\n", wIrq);
			else
				printf(", irq=default\n");
			break;
		}
	}

	if (nExtended == CAN_INIT_TYPE_EX)
		printf("             Extended frames are accepted");
	else
		printf("             Only standard frames are accepted");
	if (wBTR0BTR1)
		printf(", init with BTR0BTR1=0x%04x\n", wBTR0BTR1);
	else
		printf(", init with 500 kbit/sec.\n");

	/* open CAN port */
	if ((bDevNodeGiven) || (!bDevNodeGiven && !bTypeGiven)) {
		h = LINUX_CAN_Open(szDevNode, O_RDWR);
		if (!h) {
			printf("receivetest: can't open %s\n", szDevNode);
			goto error;
		}
	} else {
		/* please use what is appropriate  
		 * HW_DONGLE_SJA 
		 * HW_DONGLE_SJA_EPP 
		 * HW_ISA_SJA 
		 * HW_PCI 
		 * HW_PCIE_FD
		 */
		h = CAN_Open(nType, dwPort, wIrq);
		if (!h) {
			printf("receivetest: can't open %s device.\n",
				getNameOfInterface(nType));
			goto error;
		}
	}

	/* clear status */
	// CAN_Status(h);

	/* get version info */
	errno = CAN_VersionInfo(h, txt);
	if (!errno)
		printf("receivetest: driver version = %s\n", txt);
	else {
		perror("receivetest: CAN_VersionInfo()");
		goto error;
	}

	/* init to a user defined bit rate */
	if (wBTR0BTR1) {
		errno = CAN_Init(h, wBTR0BTR1, nExtended);
		if (errno) {
			perror("receivetest: CAN_Init()");
			goto error;
		}
	}
	errno = read_loop(bDisplayOn, dwMaxLoop);

error:
	do_exit(errno);
	return errno;
}
