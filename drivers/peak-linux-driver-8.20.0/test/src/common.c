/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * common.c - common parts for transmittest and receivetest
 *
 * Copyright (C) 2001-2020  PEAK System-Technik GmbH <www.peak-system.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>	/* exit */
#include <signal.h>
#include <string.h>
#include <stdlib.h>	/* strtoul */

#include "src/common.h"

#ifdef RT
#define _printf		rt_printf
#else
#define _printf		printf
#endif

/* print GPL disclaimer */
void disclaimer(const char *prgName)
{
	printf("\n");
	printf("%s Version \"%s\"  (www.peak-system.com)\n", prgName, current_release);
	printf("------- Copyright (C) 2004-2009 PEAK System-Technik GmbH ------\n");
	printf("%s comes with ABSOLUTELY NO WARRANTY.     This is free\n", prgName);
	printf("software  and you are welcome  to redistribute it under certain\n");
	printf("conditions.   For   details   see    attached   COPYING   file.\n");
	printf("\n");
}

/* print out the content of a CAN message */
void print_message(const char *prompt, TPCANMsg *m)
{
	int i;

	/* print RTR, 11 or 29, CAN-Id and datalength */
	_printf("%s: %c %c 0x%08x %1d ",
			prompt,
			(m->MSGTYPE & MSGTYPE_STATUS) ? 'x' :
				((m->MSGTYPE & MSGTYPE_RTR) ? 'r' : 'm') -
				((m->MSGTYPE & MSGTYPE_SELFRECEIVE) ? 0x20 : 0),
			(m->MSGTYPE & MSGTYPE_STATUS) ? '-' :
				((m->MSGTYPE & MSGTYPE_EXTENDED) ? 'e' : 's'),
			 m->ID,
			 m->LEN);

	/* don't print any telegram contents for remote frames */
	if (!(m->MSGTYPE & MSGTYPE_RTR))
		for (i = 0; i < m->LEN; i++)
			_printf("%02x ", m->DATA[i]);

	_printf("\n");
}

void print_message_ex(const char *prompt, TPCANRdMsg *mr)
{
	_printf("%u.%u ", mr->dwTime, mr->wUsec);
	print_message(prompt, &mr->Msg);
}

/* lookup for HW_... constant out of device type string */
int getTypeOfInterface(char *szTypeName)
{
	int nType = 0;

	if (!strcmp(szTypeName, "pci"))
		nType = HW_PCI;
	else if (!strcmp(szTypeName, "pcifd"))
		nType = HW_PCIE_FD;
	else if (!strcmp(szTypeName, "isa"))
		nType = HW_ISA_SJA;
	else if (!strcmp(szTypeName, "sp"))
		nType = HW_DONGLE_SJA;
	else if (!strcmp(szTypeName, "epp"))
		nType = HW_DONGLE_SJA_EPP;
	else if (!strcmp(szTypeName, "pccard"))
		nType = HW_PCCARD;
	else if (!strcmp(szTypeName, "usb"))
		nType = HW_USB;
	else if (!strcmp(szTypeName, "usbfd"))
		nType = HW_USB_FD;

	/* these types are kept for backward compatibility only: theses strings
	 * are no more displayed nor exported by pcan v8.x */
	else if (!strcmp(szTypeName, "usbpro"))
		nType = HW_USB_PRO;
	else if (!strcmp(szTypeName, "usbpfd"))
		nType = HW_USB_PRO_FD;

	return nType;
}

/* the opposite: lookup for device string out of HW_.. constant */
const char *getNameOfInterface(int nType)
{
	switch (nType) {
	case HW_PCI:
		return "pci";
	case HW_PCIE_FD:
		return "pcifd";
	case HW_ISA_SJA:
		return "isa";
	case HW_DONGLE_SJA:
		return "sp";
	case HW_DONGLE_SJA_EPP:
		return "epp";
	case HW_PCCARD:
		return "pccard";
	case HW_USB:
	case HW_USB_PRO:
		return "usb";
	case HW_USB_FD:
	case HW_USB_PRO_FD:
	case HW_USB_X6:
		return "usbfd";
	}

	return "unknown";
}

/* print out device and channel diagnostics */
void print_diag(const char *prgName)
{
	int err;
	TPDIAG diag;

	err = LINUX_CAN_Statistics(h, &diag);
	if (err)
		printf("%s: can't read diagnostics, error %d!\n", prgName, err);
	else {
		printf("%s: type            = %s\n",
			prgName, getNameOfInterface(diag.wType));
		switch (diag.wType) {
		case HW_USB:
		case HW_USB_PRO:
		case HW_USB_FD:
		case HW_USB_PRO_FD:
		case HW_USB_X6:
			printf("             Serial Number   = 0x%08x\n",
				diag.dwBase);
			printf("             Device Number   = %d\n",
				diag.wIrqLevel);
			break;

		default:
			printf("             io              = 0x%08x\n",
				diag.dwBase);
			printf("             irq             = %d\n",
				diag.wIrqLevel);
		}
		printf("             count of reads  = %d\n",
			diag.dwReadCounter);
		printf("             count of writes = %d\n",
			diag.dwWriteCounter);
		printf("             count of errors = %d\n",
			diag.dwErrorCounter);
		printf("             count of irqs   = %d\n",
			diag.dwIRQcounter);
		printf("             last CAN status = 0x%04x\n",
			diag.wErrorFlag);
		printf("             last error      = %d\n",
			diag.nLastError);
		printf("             open paths      = %d\n",
			diag.nOpenPaths);
		printf("             driver version  = %s\n",
			diag.szVersionString);
	}
}
