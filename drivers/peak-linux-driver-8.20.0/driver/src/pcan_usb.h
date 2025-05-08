/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_usb.h - the inner usb parts header for pcan-usb support
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
#ifndef __PCAN_USB_H__
#define __PCAN_USB_H__

#include <linux/types.h>
#include <linux/usb.h>

#include <src/pcan_main.h>

#ifdef LINUX_26
#define __usb_submit_urb(x) usb_submit_urb(x, GFP_ATOMIC)
#define __usb_alloc_urb(x)  usb_alloc_urb(x, GFP_ATOMIC)
#else
#define __usb_submit_urb(x) usb_submit_urb(x)
#define __usb_alloc_urb(x)  usb_alloc_urb(x)
#endif

#ifdef __cplusplus__
extern "C" {
#endif

#ifdef USB_SUPPORT
int pcan_usb_init(struct pcan_usb_interface *dev);
#endif

#ifdef __cplusplus__
};
#endif

#endif
