/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CAN-FD extension to PEAK-System CAN products.
 *
 * Copyright (C) 2015-2020 PEAK System-Technik GmbH <www.peak-system.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Contact:      <linux@peak-system.com>
 * Author:       Stephane Grosjean <s.grosjean@peak-system.com>
 */
#ifndef __pcanfd_core_h__
#define __pcanfd_core_h__

#include "src/pcan_common.h"
#include "src/pcan_main.h"
#include "src/pcan_fops.h"

/* hidden flag used to check init settings
 * (see lib/libpcanfd.h:#define OFD_PCANFD_MASK              (~0xff000000)
 */
#define PCANFD_INIT_USER		0x80000000
#define PCANFD_INIT_BTR_NOK		0x40000000

int pcan_bittiming_normalize(struct pcan_bittiming *pbt,
			u32 clock_Hz, const struct pcanfd_bittiming_range *caps);
struct pcan_bittiming *pcan_btr0btr1_to_bittiming(struct pcan_bittiming *pbt,
						  u16 btr0btr1);
struct pcanfd_init *pcan_init_to_fd(struct pcandev *dev,
				    struct pcanfd_init *pfdi,
				    const TPCANInit *pi);
static inline void pcanfd_copy_init(struct pcanfd_init *pd,
				    const struct pcanfd_init *ps)
{
	*pd = *ps;
}

/* CAN-FD new API */
void __pcanfd_dev_reset(struct pcandev *dev);

#define pcanfd_dev_reset(d)		__pcanfd_dev_reset(d)

void pcanfd_dev_open_init(struct pcandev *dev);
int pcanfd_dev_open(struct pcandev *dev, struct pcanfd_init *pfdi);

int pcanfd_ioctl_set_init(struct pcandev *dev, struct pcanfd_init *pfdi);
int pcanfd_ioctl_get_init(struct pcandev *dev, struct pcanfd_init *pfdi);
int pcanfd_ioctl_reset(struct pcandev *dev, unsigned long flags);
int pcanfd_ioctl_get_state(struct pcandev *dev, struct pcanfd_state *pfds);
int pcanfd_ioctl_add_filter(struct pcandev *dev, struct pcanfd_msg_filter *pf);
int pcanfd_ioctl_add_filters(struct pcandev *dev,
						struct pcanfd_msg_filters *pfl);
int pcanfd_ioctl_get_filters(struct pcandev *dev,
						struct pcanfd_msg_filters *pfl);
int pcanfd_ioctl_send_msg(struct pcandev *dev, struct pcanfd_txmsg *pmsgfd,
						struct pcanusr *usr);
int pcanfd_ioctl_send_msgs(struct pcandev *dev, struct pcanfd_txmsgs *pl,
						struct pcanusr *usr);
int pcanfd_ioctl_recv_msg(struct pcandev *dev, struct pcanfd_rxmsg *pmsgfd,
						struct pcanusr *usr);
int pcanfd_ioctl_recv_msgs(struct pcandev *dev, struct pcanfd_rxmsgs *pl,
						struct pcanusr *usr);
#endif
