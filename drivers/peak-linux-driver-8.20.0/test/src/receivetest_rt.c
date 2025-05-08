/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * receivetest_rt.c - a small program to test the receive features of the RTDM
 *                    version of the pcan driver.
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
 * Contact:      <linux@peak-system.com>
 * Maintainer:   Stephane Grosjean <s.grosjean@peak-system.com>
 * Contribution: Klaus Hitschler <klaus.hitschler@gmx.de>
 *               Edouard Tisserant <edouard.tisserant@lolitech.fr>
 */
/* set here current release for this program */
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

#define DEFAULT_NODE	"/dev/pcan0"
#ifndef bool
#define bool		int
#define true		1
#define false		0
#endif

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
#define SET_INIT_STATE(new_state)	current_state |= (new_state)
#define RESET_INIT_STATE(new_state)	current_state &= ~(new_state)

HANDLE h;
const char *current_release;

static unsigned int current_state = 0;
static __u32 dwMaxLoop = 0;

#ifdef XENOMAI
static RT_TASK reading_task;
static RT_SEM tasks_sem;
#endif

#ifdef RTAI
static RT_TASK *mainr = NULL;
static sem_t tasks_sem;
#endif

static void do_exit(void)
{
	if (current_state & STATE_FILE_OPENED) {
		print_diag("receivetest");
		CAN_Close(h);
		RESET_INIT_STATE(STATE_FILE_OPENED);
	}

#ifdef RTAI
	/* exit from RT (must be done AFTER CAN_Close() */
	if (mainr) {
		rt_task_delete(mainr);
		mainr = NULL;
	}
#endif
}

/* real time task */
static void reading_task_proc(void *arg)
{
	__u32 i = 0;

#ifdef XENOMAI
	/* Perform auto-init of rt_print buffers if the task doesn't do so */
	rt_print_auto_init(1);
#endif

#ifdef RTAI
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

	while (1) {
		TPCANRdMsg m;
		int err = LINUX_CAN_Read(h, &m);

		if (err) {
			printf("receivetest: reading task failed reading msg: "
				"err=%d\n", err);
			break;
		}

		/* check if a CAN status is pending */
		if (m.Msg.MSGTYPE & MSGTYPE_STATUS) {
			__u32 status = CAN_Status(h);
			if ((int)status < 0) {
				printf("receivetest: reading task failed "
					"reading status: err=%d\n", err);
				break;
			}
		}

		print_message_ex("receivetest", &m);

		if (dwMaxLoop && (++i >= dwMaxLoop))
			break;
	}

#ifdef XENOMAI
	/* rendez-vous */
	rt_sem_v(&tasks_sem);

#elif defined(RTAI)
	rt_make_soft_real_time();

	rt_task_delete(reading_task);

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
	mainr = rt_task_init_schmod(nam2num("MAINR"), 0, 0, 0, SCHED_FIFO, 0xF);
	if (!mainr) {
		printf("receivetest: unable to setup main RT task\n");
		return -1;
	}
#endif

	return 0;
}

/* start reading task */
static int read_loop(void)
{
	int err;

#ifdef XENOMAI
	err = rt_sem_create(&tasks_sem, "tasks_sem", 0, S_FIFO);
	if (err) {
		printf("receivetest: Failed to create sync sem, err %d\n", err);
		return err;
	}

	err = rt_task_create(&reading_task, NULL, 0, 50, 0);
	if (err) {
		printf("receivetest: Failed to create rt task, err %d\n", err);
		rt_sem_delete(&tasks_sem);
		return err;
	}

	err = rt_task_start(&reading_task, reading_task_proc, NULL);
	if (err) {
		printf("receivetest: Failed to start rt task, err %d\n", err);
		rt_sem_delete(&tasks_sem);
		return err;
	}

#elif defined(RTAI)
	pthread_t reading_thread;

	err = sem_init(&tasks_sem, 0, 0);
	if (err) {
		printf("receivetest: Failed to create sync sem (errno %d)\n",
			errno);
		return err;
	}

	err = pthread_create(&reading_thread, NULL,
					(void *)reading_task_proc, NULL);
	if (err) {
		printf("receivetest: Failed to create thread (errno %d)\n",
			errno);
		sem_destroy(&tasks_sem);
		return err;
	}
#endif

	SET_INIT_STATE(STATE_TASK_CREATED);

	printf("receivetest: reading data from CAN... "
				"(press Ctrl-C to exit)\n");

#ifdef XENOMAI
	/* rendez-vous */
	while (1) {
		err = rt_sem_p(&tasks_sem, TM_NONBLOCK);
		if (err < 0) {
			if (err == -EAGAIN) {
				/* enable ^C */
				if (!usleep(1000000))
					continue;
			}

			/* error or INTR: unblock the reading task and go to
			 * the rendez-vous again...
			 */
			if (current_state & STATE_FILE_OPENED) {
				print_diag("receivetest");
				CAN_Close(h);
				RESET_INIT_STATE(STATE_FILE_OPENED);

				continue;
			}
		}

		break;
	}

	rt_task_join(&reading_task);
	rt_task_delete(&reading_task);
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
			 * the rendez-vous again...
			 */
			if (current_state & STATE_FILE_OPENED) {
				print_diag("receivetest");
				CAN_Close(h);
				RESET_INIT_STATE(STATE_FILE_OPENED);

				continue;
			}
		}

		break;
	}

	/* wait for the end of the reading task */
	pthread_join(reading_thread, NULL);

	sem_destroy(&tasks_sem);
#endif

	RESET_INIT_STATE(STATE_TASK_CREATED);

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

void signal_handler(int unused)
{
}

/* here all is done */
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

	current_release = CURRENT_RELEASE;
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

	/* install signal handlers */
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

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
		/* please use what is appropriate  
		 * HW_DONGLE_SJA 
		 * HW_DONGLE_SJA_EPP 
		 * HW_ISA_SJA 
		 * HW_PCI 
		 * HW_PCIE_FD
		 */
		h = CAN_Open(nType, dwPort, wIrq);
		if (!h) {
			printf("receivetest: can't open %s device (errno=%d)\n",
				getNameOfInterface(nType), errno);
			goto error;
		}
	}

	SET_INIT_STATE(STATE_FILE_OPENED);

	/* clear status */
	// CAN_Status(h);

	/* get version info */
	err = CAN_VersionInfo(h, txt);
	if (err) {
		perror("receivetest: CAN_VersionInfo()");
		goto error;
	}

	printf("receivetest: driver version = %s\n", txt);

	/* init to a user defined bit rate */
	if (wBTR0BTR1) {
		err = CAN_Init(h, wBTR0BTR1, nExtended);
		if (err) {
			perror("receivetest: CAN_Init()");
			goto error;
		}
	}

	err = read_loop();
	if (!err)
		return 0;

error:
	do_exit();
	return -1;
}
