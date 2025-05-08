/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_fops.h - header for struct fops only
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
#ifndef __PCAN_FOPS_H__
#define __PCAN_FOPS_H__

#include <linux/kernel.h>   /* printk() */
#include <linux/file.h>

#ifdef NO_RT
extern struct file_operations pcan_fops;

#elif defined(XENOMAI3)
#include <rtdm/driver.h>

extern struct rtdm_driver pcandrv_rt;
#else
#include <rtdm/rtdm_driver.h>

extern struct rtdm_device pcandev_rt;
#endif

int pcan_alloc_tx_msgs(struct pcandev *dev);

#ifndef NETDEV_SUPPORT
int pcan_alloc_rx_msgs(struct pcandev *dev);
#endif

int pcan_open_path(struct pcandev *dev, struct pcanusr *ctx);
void pcan_release_path(struct pcandev *dev, struct pcanusr *ctx);

int pcan_ioctl_extended_status_common(struct pcandev *dev,
					TPEXTENDEDSTATUS *pes);
int pcan_ioctl_status_common(struct pcandev *dev, TPSTATUS *ps);
int pcan_ioctl_diag_common(struct pcandev *dev, TPDIAG *pd);

#endif
