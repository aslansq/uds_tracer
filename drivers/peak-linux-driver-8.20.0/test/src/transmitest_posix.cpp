/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * transmitest_posix.cpp.c - Full POSIX multithreaded transmitest version.
 *                           This version can be built with or without RT
 *                           support.
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
#include "src/parser.h"

/* POSIX */
#include <pthread.h>
#include <semaphore.h>

#ifdef RTAI
#include <rtai_posix.h>
#endif

#define DEFAULT_NODE	"/dev/pcan0"

/* set here current release for this program */
#define CURRENT_RELEASE	"Release_20190204_rt"

/* public */
HANDLE h = NULL;
const char *current_release = CURRENT_RELEASE;

/* private */
static std::list<TPCANMsg> *List;
static int nExtended = CAN_INIT_TYPE_ST;
static unsigned int dwMaxLoop = 0;

#ifdef RTAI
static RT_TASK *maint = NULL;
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
		print_diag("transmitest");

		/* be sure that CAN msgs are written on the bus */
		usleep(500000);
		CAN_Close(h);
		h = NULL;
	}

#ifdef RTAI
	/* exit from RT (must be done AFTER CAN_Close() */
	if (maint) {
		rt_task_delete(maint);
		maint = NULL;
	}
#endif
}

/*
 * void writing_task_proc(void *arg)
 *
 * real time task
 */
static void writing_task_proc(void *arg)
{
#ifdef __XENO__
	pthread_set_mode_np(0, PTHREAD_WARNSW);
	pthread_setname_np(pthread_self(), "can_writing_task");

#elif defined(RTAI)
	char task_name[7];
	RT_TASK *writing_task;

	snprintf(task_name, sizeof(task_name), "WR%04d", getpid() % 10000);
	writing_task = rt_task_init_schmod(nam2num(task_name),
					   0, 0, 0, SCHED_FIFO, 0xF);
	if (!writing_task) {
		printf("transmitest: unable to setup RT writing task\n");
		goto exit_p;
	}

	rt_make_hard_real_time();
#endif

	/* enable pthread_cancel() to break CAN_Write() by changing its default
	 * cancelability type.
	 * note:
	 * - Xenomai errno=-512 
	 * - RTAI errno=-4 */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	for (unsigned long l = 0; ; l++) {

		if (dwMaxLoop)
			if (l >= dwMaxLoop)
				break;

		std::list<TPCANMsg>::iterator iter;
		for (iter = List->begin(); iter != List->end(); iter++) {
			/* test for standard frames only */
			if ((nExtended == CAN_INIT_TYPE_EX) ||
					!(iter->MSGTYPE & MSGTYPE_EXTENDED)) {

				/* send the message */
				print_message("transmitest", &(*iter));

				int err = CAN_Write(h, &(*iter));
				if (err) {
					printf("transmitest: writing task "
						"failed writing msg: err=%d\n",
						err);

					/* stop next loop */
					l = dwMaxLoop = 1;
					break;
				}
			}
		}
	}

#ifdef RTAI
	rt_make_soft_real_time();
	rt_task_delete(writing_task);

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
	maint = rt_task_init_schmod(nam2num("MAINT"), 3, 0, 0, SCHED_FIFO, 0xF);
	if (!maint) {
		printf("transmitest: unable to setup main RT task\n");
		return -1;
	}
#endif
#endif
	return 0;
}

/* start writing task */
static int write_loop(void)
{
	pthread_attr_t thattr;
	pthread_t writing_thread;

	int err = sem_init(&tasks_sem, 0, 0);
	if (err) {
		printf("receivetest: Failed to create sync sem (errno %d)\n",
			errno);
		return err;
	}

	pthread_attr_init(&thattr);
	pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_JOINABLE);

	err = pthread_create(&writing_thread, &thattr,
				(void *(*)(void *))&writing_task_proc, NULL);
	if (err) {
		printf("transmitest: Failed to create thread (errno %d)\n",
			errno);
		sem_destroy(&tasks_sem);
		return err;
	}

	printf("transmitest: writing data to CAN... (press Ctrl-C to exit)\n");

	/* rendez-vous */
	err = sem_wait(&tasks_sem);
	if (err < 0) {
#ifdef __XENO__
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
		printf("transmitest: cancelling reading task...\n");
		pthread_cancel(writing_thread);
	}

	/* wait for the end of the writing task */
	pthread_join(writing_thread, NULL);

	sem_destroy(&tasks_sem);

	printf("transmitest: finishing\n");

	do_exit();

	return 0;
}

static void hlpMsg(void)
{
	printf("transmitest - a small test program which sends CAN messages.\n");
	printf("usage:   transmitest filename {[-f=devicenode] | {[-t=type] [-p=port [-i=irq]]}} [-b=BTR0BTR1] [-e] [-?]\n");
	printf("options: filename - mandatory name of message description file.\n");
	printf("         -f - devicenode - path to devicefile, default=%s\n", DEFAULT_NODE);
	printf("         -t - type of interface, e.g. 'pci', 'sp', 'epp' ,'isa', 'pccard' or 'usb' (default: pci).\n");
	printf("         -p - port in hex notation if applicable, e.g. 0x378 (default: 1st port of type).\n");
	printf("         -i - irq in dec notation if applicable, e.g. 7 (default: irq of 1st port).\n");
	printf("         -b - BTR0BTR1 code in hex, e.g. 0x001C (default: 500 kbit).\n");
	printf("         -e - accept extended frames. (default: standard frames)\n");
	printf("         -? or --help - this help\n");
	printf("\n");
}

void signal_handler(int s)
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
	char *filename = NULL;
	const char *szDevNode = DEFAULT_NODE;
	bool bDevNodeGiven = false;
	bool bTypeGiven = false;
	parser MyParser;
	char txt[VERSIONSTRING_LEN];

	disclaimer("transmitest");

	if (init())
		goto error;

	/* decode command line arguments */
	for (i = 1; i < argc; i++) {
		char c;

		ptr = argv[i];

		if (*ptr == '-') {
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
				if (!nType) {
					printf("transmitest: unknown type of interface\n");
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
				printf("transmitest: unknown command line argument\n");
				goto error;
				break;
			}
		}
		else
			filename = ptr;
	}

	/* test for filename */
	if (filename == NULL) {
		printf("transmitest: no filename given\n");
		goto error;
	}

	/* test device node and type */
	if (bDevNodeGiven && bTypeGiven) {
		printf("transmitest: device node and type together is useless\n");
		goto error;
	}

	/* give the filename to my parser */
	MyParser.setFileName(filename);

	/* install signal handlers */
	setup_sig_handler(SIGTERM, signal_handler);
	setup_sig_handler(SIGINT, signal_handler);
	setup_sig_handler(SIGXCPU, signal_handler);

	/* tell some information to user */
	if (!bTypeGiven) {
		printf("transmitest: device node=\"%s\"\n", szDevNode);
	} else {
		printf("transmitest: type=%s", getNameOfInterface(nType));
		switch (nType) {
		case HW_USB:
		case HW_USB_PRO:
		case HW_USB_FD:
		case HW_USB_PRO_FD:
		case HW_USB_X6:
			printf(", Serial Number=default, Device Number=%d\n",
				dwPort);
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
		printf("             Extended frames are sent");
	else
		printf("             Only standard frames are sent");

	if (wBTR0BTR1)
		printf(", init with BTR0BTR1=0x%04x\n", wBTR0BTR1);
	else
		printf(", init with 500 kbit/sec.\n");

	printf("             Data will be read from \"%s\".\n", filename);

	/* get the list of data from parser */
	List = MyParser.Messages();
	if (!List) {
		errno = MyParser.nGetLastError();
		perror("transmitest: error at file read");
		goto error;
	}

	/* open CAN port */
	if ((bDevNodeGiven) || (!bDevNodeGiven && !bTypeGiven)) {
		h = LINUX_CAN_Open(szDevNode, O_RDWR);
		if (!h) {
			printf("transmitest: can't open %s (errno %d)\n",
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
			printf("transmitest: can't open %s device (errno %d)\n",
				getNameOfInterface(nType), errno);
			goto error;
		}
	}

	/* clear status */
	CAN_Status(h);

	/* get version info */
	err = CAN_VersionInfo(h, txt);
	if (err) {
		perror("transmitest: CAN_VersionInfo()");
		goto error;
	}

	printf("transmitest: driver version = %s\n", txt);

	/* init to a user defined bit rate */
	if (wBTR0BTR1) {
		err = CAN_Init(h, wBTR0BTR1, nExtended);
		if (err) {
			perror("transmitest: CAN_Init()");
			goto error;
		}
	}

	/* enter in the write loop */
	err = write_loop();

error:
	do_exit();
	return (err) ? -1 : 0;
}
