/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * receivetest_posix.c - Full POSIX multithreaded receivetest version.
 *                       This version can be built with or without RT support.
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
 * Contact: <linux@peak-system.com>
 * Author:  Stephane Grosjean <s.grosjean@peak-system.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>

#include <libpcan.h>

#include "src/common.h"

/* POSIX */
#include <pthread.h>
#include <semaphore.h>

#ifdef RTAI
#include <rtai_posix.h>
#endif

#define DEFAULT_NODE	"/dev/pcan0"

/* set here current release for this program */
#define CURRENT_RELEASE "Release_20190204_rt"

#ifndef bool
#define bool		int
#define true		1
#define false		0
#endif

/* public */
HANDLE h = NULL;
const char *current_release = CURRENT_RELEASE;

/* private */
static __u32 dwMaxLoop = 0;

#ifdef RTAI
static RT_TASK *mainr = NULL;
#endif

static sem_t tasks_sem;

/*
 * void do_exit(void)
 *
 *	Do clean everything before exit().
 */
static void do_exit(void)
{
	if (h) {
		print_diag("receivetest");
		CAN_Close(h);
		h = NULL;
	}

#ifdef RTAI
	/* exit from RT (must be done AFTER CAN_Close() */
	if (mainr) {
		rt_task_delete(mainr);
		mainr = NULL;
	}
#endif
}

/*
 * void reading_task_proc(void *arg)
 *
 * real time task
 */
static void reading_task_proc(void *arg)
{
	__u32 i = 0;

#ifdef XENOMAI
	pthread_setmode_np(0, PTHREAD_WARNSW, NULL);
	pthread_setname_np(pthread_self(), "can_reading_task");

#elif defined(RTAI)
	char task_name[7];
	RT_TASK *reading_task;

	snprintf(task_name, sizeof(task_name), "RD%04d", getpid() % 10000);
	reading_task = rt_task_init_schmod(nam2num(task_name),
					   0, 0, 0, SCHED_FIFO, 0xF);
	if (!reading_task) {
		printf("receivetest: unable to setup RT reading task\n");
		goto exit_p;
	}

	rt_make_hard_real_time();
#endif

	/* enable pthread_cancel() to break CAN_Read() by changing its default
	 * cancelability type.
	 * note:
	 * - Xenomai errno=-512 
	 * - RTAI errno=-4 */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	while (1) {
		TPCANMsg m;

		int err = CAN_Read(h, &m);

		/* -EINTR if task has been canceled */
		if (err) {
			printf("receivetest: reading task failed reading msg: "
				"err=%d\n", err);
			break;
		}

		/* check if a CAN status is pending */
		if (m.MSGTYPE & MSGTYPE_STATUS) {
			__u32 status = CAN_Status(h);
			if ((int)status < 0) {
				printf("receivetest: reading task failed "
					"reading status: err=%d\n", err);
				break;
			}
		}

		print_message("receivetest", &m);

		if (dwMaxLoop && (++i >= dwMaxLoop))
			break;
	}

#ifdef RTAI
	rt_make_soft_real_time();
	rt_task_delete(reading_task);

exit_p:
#endif
	/* rendez-vous */
	sem_post(&tasks_sem);
}

static int init(void)
{
#ifdef NO_RT
	/* nothing to do in non-RT */
#else
	mlockall(MCL_CURRENT | MCL_FUTURE);

#ifdef RTAI
	/* Initialize LXRT */
	mainr = rt_task_init_schmod(nam2num("MAINR"), 0, 0, 0, SCHED_FIFO, 0xF);
	if (!mainr) {
		printf("receivetest: unable to setup main RT task\n");
		return -1;
	}
#endif
#endif
	return 0;
}

/* start reading task */
static int read_loop(void)
{
	pthread_attr_t thattr;
	pthread_t reading_thread;

	int err = sem_init(&tasks_sem, 0, 0);
	if (err) {
		printf("receivetest: Failed to create sync sem (errno %d)\n",
			errno);
		return err;
	}

	pthread_attr_init(&thattr);
	pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_JOINABLE);

	err = pthread_create(&reading_thread, &thattr,
					(void *)reading_task_proc, NULL);
	if (err) {
		printf("receivetest: Failed to create thread (errno %d)\n",
			errno);
		sem_destroy(&tasks_sem);
		return err;
	}

	printf("receivetest: reading data from CAN... "
				"(press Ctrl-C to exit)\n");

	/* rendez-vous */
	err = sem_wait(&tasks_sem);
	if (err < 0) {
#ifdef XENOMAI
		/* see xenomai 3, xenomai/kernel/cobalt/rtdm/fd.c: */

		/*
		 * In dual kernel mode, the linux-side fdtable and the
		 * RTDM ->close() handler are asynchronously managed,
		 * i.e. the handler execution may be deferred after the
		 * regular file descriptor was removed from the fdtable
		 * if some refs on rtdm_fd are still pending.
		 */

		/* This means that we can't call CAN_Close() while the
		 * other task is blocked reading... */
#endif
		printf("receivetest: cancelling reading task...\n");
		pthread_cancel(reading_thread);
	}

	/* wait for the end of the reading task */
	pthread_join(reading_thread, NULL);

	sem_destroy(&tasks_sem);

	printf("receivetest: finishing\n");

	do_exit();

	return 0;
}

static void hlpMsg(void)
{
	printf("receivetest - a small test program which receives and prints CAN messages.\n");
	printf("usage:   receivetest {[-f=devicenode] | {[-t=type] [-p=port [-i=irq]]}} [-b=BTR0BTR1] [-e] [-?]\n");
	printf("options:\n");
	printf("   -f=devicenode name of the device file (default=%s)\n", DEFAULT_NODE);
	printf("   -t=type       type of interface, e.g. 'pci', 'sp', 'epp', 'isa', 'pccard' or 'usb' (default: pci).\n");
	printf("   -p=port       port in hex notation if applicable, e.g. 0x378 (default: 1st port of type).\n");
	printf("   -i=irq        irq number in dec notation if applicable, e.g. 7 (default: irq of 1st port).\n");
	printf("   -b=BTR0BTR1   bitrate code in hex, e.g. 0x001C (default: 500 kbit).\n");
	printf("   -e            accept extended frames. (default: standard frames)\n");
	printf("   -n=mloop      number of loops to run before exit (default=infiite)\n");
	printf("   -? or --help  this help\n");
	printf("\n");
}

static void signal_handler(int s)
{
	printf("signal %d caught\n", s);
}

/* siagaction() is thread -safe */
static int setup_sig_handler(int signum, void (*f)(int))
{
	struct sigaction act;

	memset(&act, 0, sizeof act);
	sigemptyset(&act.sa_mask);
	act.sa_handler = f;
	act.sa_flags = 0;
	return sigaction(signum, &act, NULL);
}

int main(int argc, char *argv[])
{
	char *ptr;
	int i, err;
	int nType = HW_PCI;
	__u32 dwPort = 0;
	__u16 wIrq = 0;
	__u16 wBTR0BTR1 = 0;
	int nExtended = CAN_INIT_TYPE_ST;
	const char  *szDevNode = DEFAULT_NODE;
	bool bDevNodeGiven = false;
	bool bTypeGiven = false;
	char txt[VERSIONSTRING_LEN];

	disclaimer("receivetest");

	if (init())
		goto error;

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
		case 't':
			nType = getTypeOfInterface(ptr);
			if (!nType){
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
			printf("receivetest: unknown command line argument!\n");
			goto error;
			break;
		}
	}

	/* simple command input check */
	if (bDevNodeGiven && bTypeGiven){
		printf("receivetest: device node and type together is useless\n");
		goto error;
	}

	/* install signals handler */
	setup_sig_handler(SIGTERM, signal_handler);
	setup_sig_handler(SIGINT, signal_handler);
	setup_sig_handler(SIGXCPU, signal_handler);

	/* give some information back */
	if (!bTypeGiven){
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
				if (nType == HW_PCI || nType == HW_PCIE_FD)
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
			printf("receivetest: can't open %s (errno %d)\n",
				szDevNode, errno);
			goto error;
		}
	} else {
		// please use what is appropriate  
		// HW_DONGLE_SJA 
		// HW_DONGLE_SJA_EPP 
		// HW_ISA_SJA 
		// HW_PCI 
		// HW_PCIE_FD
		h = CAN_Open(nType, dwPort, wIrq);
		if (!h) {
			printf("receivetest: can't open %s device (errno=%d)\n",
				getNameOfInterface(nType), errno);
			goto error;
		}
	}

	/* get version info */
	err = CAN_VersionInfo(h, txt);
	if (err) {
		perror("receivetest: CAN_VersionInfo()");
		goto error;
	}

	printf("receivetest: driver version = %s\n", txt);

	/* init to a user defined bit rate if provided */
	if (wBTR0BTR1) {
		err = CAN_Init(h, wBTR0BTR1, nExtended);
		if (err) {
			perror("receivetest: CAN_Init()");
			goto error;
		}
	}

	/* enter the read loop */
	err = read_loop();

error:
	do_exit();
	return (err) ? -1 : 0;
}
