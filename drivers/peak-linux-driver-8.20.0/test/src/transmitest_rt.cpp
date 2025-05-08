/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * transmitest_rt.cpp - a simple RT program to test CAN transmits.
 *
 * Copyright (C) 2008-2020  PEAK System-Technik GmbH <www.peak-system.com>
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
// set here current release for this program
#define CURRENT_RELEASE "Release_20190125_r"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   // exit
#include <signal.h>
#include <string.h>
#include <stdlib.h>   // strtoul
#include <fcntl.h>    // O_RDWR

#include <libpcan.h>
#include <src/common.h>
#include <ctype.h>
#include <src/parser.h>

#define DEFAULT_NODE "/dev/pcan0"

HANDLE h;
const char *current_release;
std::list<TPCANMsg> *List;
int nExtended = CAN_INIT_TYPE_ST;

#include <sys/mman.h>

#ifdef XENOMAI
#include <task.h>
#include <trank/rtdk.h>
#include <sem.h>
#endif

#ifdef RTAI
#include <sys/poll.h>
#include <rtai_lxrt.h>
#include <rtai_posix.h>
#endif

#define STATE_FILE_OPENED		1
#define STATE_TASK_CREATED		2
#define SET_INIT_STATE(new_state)	current_state |= new_state
#define RESET_INIT_STATE(new_state)	current_state &= ~new_state

static unsigned int current_state = 0;
static unsigned int dwMaxLoop = 0;

#ifdef XENOMAI
static RT_TASK writing_task;
static RT_SEM tasks_sem;
#endif

#ifdef RTAI
static RT_TASK *maint = NULL;
static sem_t tasks_sem;
#endif

// SPECIFICS FUNCTIONS FOR REALTIME
static void do_exit(void)
{
	if (current_state & STATE_FILE_OPENED) {
		print_diag("transmitest");

		/* be sure that CAN msgs are written on the bus */
		usleep(500000);
		CAN_Close(h);
		RESET_INIT_STATE(STATE_FILE_OPENED);
	}

#ifdef RTAI
	/* exit from RT (must be done AFTER CAN_Close() */
	if (maint) {
		rt_task_delete(maint);
		maint = NULL;
	}
#endif
}

// real time task
static void writing_task_proc(void *arg)
{
#ifdef XENOMAI
	/* Perform auto-init of rt_print buffers if the task doesn't do so */
	rt_print_auto_init(1);
#endif

#ifdef RTAI
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

	for (unsigned long l = 0; ; l++) {

		if (dwMaxLoop)
			if (l >= dwMaxLoop)
				break;

		std::list<TPCANMsg>::iterator iter;
		for (iter = List->begin(); iter != List->end(); iter++) {
			// test for standard frames only
			if ((nExtended == CAN_INIT_TYPE_EX) ||
					!(iter->MSGTYPE & MSGTYPE_EXTENDED)) {
				// send the message
				print_message("transmitest", &(*iter));

				int err = CAN_Write(h, &(*iter));
				if (err) {
					printf("transmitest: writing task "
						"failed writing msg: err=%d\n",
						err);

					// stop next loop
					l = dwMaxLoop = 1;
					break;
				}
			}
		}
	}

#ifdef XENOMAI
	/* rendez-vous */
	rt_sem_v(&tasks_sem);

#elif defined(RTAI)
	rt_make_soft_real_time();

	rt_task_delete(writing_task);

exit_p:
	/* rendez-vous */
	sem_post(&tasks_sem);
#endif
}

static int init(void)
{
	mlockall(MCL_CURRENT | MCL_FUTURE);

	/* Initialize LXRT */
#ifdef RTAI
	maint = rt_task_init_schmod(nam2num("MAINT"), 3, 0, 0, SCHED_FIFO, 0xF);
	if (!maint) {
		printf("transmitest: unable to setup main RT task\n");
		return -1;
	}
#endif

	return 0;
}

// start writing task
static int write_loop(void)
{
	int err;

#ifdef XENOMAI
	err = rt_sem_create(&tasks_sem, "tasks_sem", 0, S_FIFO);
	if (err) {
		printf("transmitest: Failed to create sync sem, err %d\n", err);
		return err;
	}

	err = rt_task_create(&writing_task,NULL,0,50,0);
	if (err) {
		printf("transmitest: Failed to create rt task, err %d\n", err);
		rt_sem_delete(&tasks_sem);
		return err;
	}

	err = rt_task_start(&writing_task,&writing_task_proc,NULL);
	if (err) {
		printf("transmitest: Failed to start rt task, err %d\n", err);
		rt_sem_delete(&tasks_sem);
		return err;
	}
#endif

#ifdef RTAI
	pthread_t writing_thread;

	err = sem_init(&tasks_sem, 0, 0);
	if (err) {
		printf("receivetest: Failed to create sync sem (errno %d)\n",
			errno);
		return err;
	}

	err = pthread_create(&writing_thread, NULL,
				(void *(*)(void *))&writing_task_proc, NULL);
	if (err) {
		printf("transmitest: Failed to create thread (errno %d)\n",
			errno);
		sem_destroy(&tasks_sem);
		return errno;
	}
#endif

	SET_INIT_STATE(STATE_TASK_CREATED);

	printf("transmitest: writing data to CAN... (press Ctrl-C to exit)\n");

#ifdef XENOMAI
	/* rendez-vous */
	while (1) {
		err = rt_sem_p(&tasks_sem, TM_NONBLOCK);
		if (err == -EAGAIN) {
			/* enable to ^C */
			if (!usleep(1000000))
				continue;
		}

		break;
	}

	rt_task_join(&writing_task);
	rt_task_delete(&writing_task);
	rt_sem_delete(&tasks_sem);

#elif defined(RTAI)
	/* rendez-vous */
	while (1) {
		err = sem_trywait(&tasks_sem);
		if (err < 0) {
			if (errno == EAGAIN) {
				/* enable ^C */
				if (!usleep(1000000))
					continue;
			}

			/* error or INTR: unblock the reading task and go to
			 * the rendez-vous again... */
			if (current_state & STATE_FILE_OPENED) {
				print_diag("transmitest");
				CAN_Close(h);
				RESET_INIT_STATE(STATE_FILE_OPENED);

				continue;
			}
		}

		break;
	}

	/* wait for the end of the writing task */
	pthread_join(writing_thread, NULL);

	sem_destroy(&tasks_sem);
#endif

	RESET_INIT_STATE(STATE_TASK_CREATED);

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

void signal_handler(int unused)
{
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

	current_release = CURRENT_RELEASE;
	disclaimer("transmitest");

	if (init())
		goto error;

	// decode command line arguments
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

	// test for filename
	if (filename == NULL) {
		printf("transmitest: no filename given\n");
		goto error;
	}

	// test device node and type
	if (bDevNodeGiven && bTypeGiven) {
		printf("transmitest: device node and type together is useless\n");
		goto error;
	}

	// give the filename to my parser
	MyParser.setFileName(filename);

	/* install signal handlers */
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	// tell some information to user
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

	SET_INIT_STATE(STATE_FILE_OPENED);

	/* clear status */
	CAN_Status(h);

	// get version info
	err = CAN_VersionInfo(h, txt);
	if (err) {
		perror("transmitest: CAN_VersionInfo()");
		goto error;
	}

	printf("transmitest: driver version = %s\n", txt);

	// init to a user defined bit rate
	if (wBTR0BTR1) {
		err = CAN_Init(h, wBTR0BTR1, nExtended);
		if (err) {
			perror("transmitest: CAN_Init()");
			goto error;
		}
	}

	// enter in the write loop
	err = write_loop();
	if (!err)
		return 0;

error:
	do_exit();
	return -1;
}
