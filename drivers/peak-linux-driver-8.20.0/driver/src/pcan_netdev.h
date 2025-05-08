/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_netdev.h - CAN network device support defines / prototypes
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
#ifndef PCAN_NETDEV_H
#define PCAN_NETDEV_H

#include <linux/netdevice.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
#include <linux/can/dev.h>
#endif

#ifndef LINUX_26
#define netdev_priv(dev) ((dev)->priv)
#endif

struct can_bittiming *pcan_netdev_get_bittiming(struct pcandev *dev);
struct can_bittiming *pcan_netdev_get_dbittiming(struct pcandev *dev);

struct net_device_stats *pcan_netdev_get_stats(struct net_device *dev);
int pcan_netdev_register(struct pcandev *dev);
int pcan_netdev_unregister(struct pcandev *dev);
int pcan_netdev_rx(struct pcandev *dev, struct pcanfd_rxmsg *pf);

#endif /* PCAN_NETDEV_H */
