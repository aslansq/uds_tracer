/* 
 * Simple RTAI application waiting for some message from a pcan device node.
 *
 * Copyright (C) 2001-2019  PEAK System-Technik GmbH
 *
 * linux@peak-system.com
 * www.peak-system.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>

#include <rtai_lxrt.h>
#include <libpcanfd.h>

#define rt_printf	printf

#define PCAN_DEV	"pcan0"
#define PCAN_BTR	500000
#define PCAN_MSG	5

int main(int argc, char *argv[])
{
	struct pcanfd_msg pcan_msg;
	int fd, i;
	RT_TASK *rt_main;

	rt_allow_nonroot_hrt();

	/* make current process a RT one */
	rt_main = rt_thread_init(nam2num("PCANRX"), 0, 0, SCHED_FIFO, 0xF);
	if (!rt_main) {
		perror("rt_thread_init() failed");
		exit(1);
	}
	
	/* lock all of the calling process virtual address space into RAM */
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();

	/* open the pcan device */
	fd = pcanfd_open(PCAN_DEV, OFD_BITRATE, PCAN_BTR);
	if (fd < 0) {
		perror("pcanfd_open() failed");
		exit(1);
	}

	/* loop waiting for PCAN_MSG message(s) */
	for (i = 0; i < PCAN_MSG; i++ ) {
		int err = pcanfd_recv_msg(fd, &pcan_msg);
		if (err) {
			perror("pcanfd_recv_msg() failed");
			continue;
		}

		switch (pcan_msg.type) {
		case PCANFD_TYPE_CAN20_MSG:
		case PCANFD_TYPE_CANFD_MSG:
			rt_printf("got CAN frame: id=%08xh data_len=%u",
				pcan_msg.id, pcan_msg.data_len);
			if (pcan_msg.data_len) {
				int j;
				for (j = 0; j < pcan_msg.data_len; j++)
					rt_printf(" %02x", pcan_msg.data[j]);
			}
			rt_printf("\n");
			break;
		case PCANFD_TYPE_STATUS:
			rt_printf("got STATUS: id=%u\n", pcan_msg.id);
			break;
		default:
			rt_printf("got unhandled msg: type=%u id=%u\n",
				pcan_msg.type, pcan_msg.id);
			break;
		}
	}

	/* close the device */
	pcanfd_close(fd);

	rt_task_delete(rt_main);

	return 0;
}
