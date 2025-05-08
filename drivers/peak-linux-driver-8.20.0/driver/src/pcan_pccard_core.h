/* SPDX-License-Identifier: GPL-2.0 */
/*
 * all parts to handle the interface specific parts of pcan-pccard
 *
 * Copyright (C) 2001-2020 PEAK System-Technik GmbH <www.peak-system.com>
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
 * Maintainer:   Stephane Grosjean <s.grosjean@peak-system.com>
 * Contributors: Klaus Hitschler <klaus.hitschler@gmx.de>
 *               Edouard Tisserant <edouard.tisserant@lolitech.fr> XENOMAI
 *               Laurent Bessard <laurent.bessard@lolitech.fr> XENOMAI
 *               Oliver Hartkopp <oliver.hartkopp@volkswagen.de> socket-CAN
 */
#ifndef __PCAN_PCCARD_CORE_H__
#define __PCAN_PCCARD_CORE_H__

#include "src/pcan_common.h"

#include <asm/io.h>
#include <linux/timer.h>

#include <linux/types.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <pcmcia/cs_types.h>
#endif
#include <pcmcia/cs.h>
#endif
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

#include "src/pcan_main.h"

#define PCCARD_CHANNELS		2	/* maximum nb of channels */

typedef struct pcan_pccard {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	struct pcmcia_device *pcc_dev;
#else
	dev_link_t	link;
#endif
	u_int		basePort;	/* base of io area for all channels */
	u_int		numPort;
	u_int		commonIrq;	/* irq for all channels */
	u_int		commonPort;	/* channels commonly used port */

	struct		pcandev *dev[PCCARD_CHANNELS];

	struct pcan_adapter adapter;

	int			run_activity_timer_cyclic;
	struct timer_list	activity_timer;

	u8		cached_ccr;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
	dev_node_t	node;
#endif

} PCAN_PCCARD;

int pccard_create_all_devices(struct pcan_pccard *card);
void pccard_release_all_devices(struct pcan_pccard *card);

#endif /* __PCAN_PCCARD_CORE_H__ */
