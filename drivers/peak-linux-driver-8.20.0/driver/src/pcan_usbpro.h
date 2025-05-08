/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_usbpro.h - the inner usb parts header for pcan-usb pro support
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
 * Author:       Stephane Grosjean <s.grosjean@peak-system.com>
 */
#ifndef __PCAN_USBPRO_H__
#define __PCAN_USBPRO_H__

#include <linux/types.h>
#include <linux/usb.h>

#include <src/pcan_main.h>

extern const struct pcanfd_bittiming_range sja2010_caps;
extern const pcanfd_mono_clock_device sja2010_clocks;

#ifdef LINUX_26
#define __usb_submit_urb(x) usb_submit_urb(x, GFP_ATOMIC)
#define __usb_alloc_urb(x)  usb_alloc_urb(x, GFP_ATOMIC)
#define FILL_BULK_URB(a, b, c, d, e, f, g) usb_fill_bulk_urb(a, b, c, d, e, (usb_complete_t)f, (void *)g)
#else
#define __usb_submit_urb(x) usb_submit_urb(x)
#define __usb_alloc_urb(x)  usb_alloc_urb(x)
#endif

#ifdef __cplusplus__
extern "C" {
#endif

int pcan_usbpro_init(struct pcan_usb_interface *);

#ifdef __cplusplus__
};
#endif

#endif /* __PCAN_USBPRO_H__ */
