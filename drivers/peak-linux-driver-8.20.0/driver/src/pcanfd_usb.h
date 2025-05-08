/* SPDX-License-Identifier: GPL-2.0 */
/*
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
 * Author:       Stephane Grosjean <s.grosjean@peak-system.com>
 */
#ifndef __PCAN_USBFD_H__
#define __PCAN_USBFD_H__

/*
 * INCLUDES
 */
#include <linux/types.h>
#include <linux/usb.h>

#include "src/pcan_main.h"
#include "src/pcanfd_ucan.h"

/*
 * DEFINES
 */
#define PCAN_USBPROFD_PRODUCT_ID	0x0011
#define PCAN_USBFD_PRODUCT_ID		0x0012
#define PCAN_USBCHIP_PRODUCT_ID		0x0013
#define PCAN_USBX6_PRODUCT_ID		0x0014

#ifdef LINUX_26
#define __usb_submit_urb(x)	usb_submit_urb(x, GFP_ATOMIC)
#define __usb_alloc_urb(x)	usb_alloc_urb(x, GFP_ATOMIC)
#else
#define __usb_submit_urb(x)	usb_submit_urb(x)
#define __usb_alloc_urb(x)	usb_alloc_urb(x)
#endif

/*
 * External API
 */
#ifdef __cplusplus__
extern "C" {
#endif

int pcan_usbfd_init(struct pcan_usb_interface *);
int pcan_usbfd_init_ep(struct pcan_usb_interface *usb_if);

/* PCAN-Chip specific options */
int pcan_usb_chip_get_dig_cfg(struct pcandev *dev,
			      struct pcanfd_option *opt, void *c);
int pcan_usb_chip_set_dig_cfg(struct pcandev *dev,
			      struct pcanfd_option *opt, void *c);
int pcan_usb_chip_get_dig_val(struct pcandev *dev,
			      struct pcanfd_option *opt, void *c);
int pcan_usb_chip_set_dig_val(struct pcandev *dev,
			      struct pcanfd_option *opt, void *c);
int pcan_usb_chip_set_dig_bit(struct pcandev *dev,
			      struct pcanfd_option *opt, void *c);
int pcan_usb_chip_clr_dig_bit(struct pcandev *dev,
			      struct pcanfd_option *opt, void *c);
int pcan_usb_chip_get_ana_val(struct pcandev *dev,
			      struct pcanfd_option *opt, void *c);

#ifdef __cplusplus__
};
#endif

#endif
