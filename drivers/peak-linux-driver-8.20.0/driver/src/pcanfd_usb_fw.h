/* SPDX-License-Identifier: GPL-2.0 */
/*
 * PCAN-USB Pro / PCAN-USB Pro FD firmware objects
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
#ifndef UCAN_USB_H
#define UCAN_USB_H

#include "src/pcanfd_ucan.h"

/*
 * Extended commands (non uCAN commands):
 *
 * Clock Modes command
 */
#define UCAN_USB_CMD_CLK_SET		0x80

#define UCAN_USB_CLK_80MHZ		0x0
#define UCAN_USB_CLK_60MHZ		0x1
#define UCAN_USB_CLK_40MHZ		0x2
#define UCAN_USB_CLK_30MHZ		0x3
#define UCAN_USB_CLK_24MHZ		0x4
#define UCAN_USB_CLK_20MHZ		0x5
#define UCAN_USB_CLK_DEF		UCAN_USB_CLK_80MHZ

struct __packed ucan_usb_clock {
	__le16	opcode_channel;

	u8	mode;
	u8	unused[5];
};

/* LED control command */
#define UCAN_USB_CMD_LED_SET		0x86

#define UCAN_USB_LED_DEV		0x00
#define UCAN_USB_LED_FAST		0x01
#define UCAN_USB_LED_SLOW		0x02
#define UCAN_USB_LED_FIXED		0x03
#define UCAN_USB_LED_OFF		0x04
#define UCAN_USB_LED_DEF		UCAN_USB_LED_DEV

struct __packed ucan_usb_led {
	__le16	opcode_channel;

	u8	mode;
	u8	unused[5];
};

/* Extended usage of uCAN commands UCAN_CMD_XXX_YY_OPTION for PCAN-USB FD */
#define UCAN_USB_OPTION_CALIBRATION	0x8000
#define UCAN_USB_OPTION_DEBUG		0x4000
#define UCAN_USB_OPTION_FAST_FWD	0x2000

struct __packed ucan_usb_option {
	__le16	opcode_channel;

	__le16	ext_mask;
	u16	unused;
	__le16	usb_mask;
};

/* Extended usage of uCAN messages for PCAN-USB Pro FD */
#define UCAN_USB_MSG_CALIBRATION	0x100

struct __packed ucan_usb_ts_msg {
	__le16	size;
	__le16	type;
	__le32	ts_low;
	__le32	ts_high;
	__le16	usb_frame_index;
	u16	unused;
};

#define UCAN_USB_MSG_OVERRUN		0x101

#define UCAN_USB_OVMSG_CHANNEL(o)	((o)->channel & 0xf)

struct __packed ucan_usb_ovr_msg {
	__le16	size;
	__le16	type;
	__le32	ts_low;
	__le32	ts_high;
	u8	channel;
	u8	unused[3];
};

#define UCAN_USB_CMD_DEVID_SET		0x81

struct __packed ucan_usb_device_id {
	__le16	opcode_channel;

	u16	unused;
	__le32	device_id;
};

#define UCAN_USB_CMD_LOCINFO_SET	0x83
#define UCAN_USB_CMD_LOCINFO_GET	0x84
#define UCAN_USB_CMD_LOCINFO_RSP	0x85

struct __packed ucan_usb_location_info {
	__le16	opcode_channel;

	u8	index;
	u8	w;
	u16	unused;
	u8	d0;
	u8	d1;
};

#define UCAN_USB_LOCINFO_LEN		250

#define UCAN_USB_MSG_DEBUG		0x200

struct __packed ucan_usb_dbg_msg {
	__le16	size;
	__le16	type;
	u8	d[64];
};

#define UCAN_USB_CMD_DEVDATA_SET	0xa0
#define UCAN_USB_CMD_DEVDATA_REQ	0xd0
#define UCAN_USB_CMD_DEVDATA_RSP	0xd1

struct __packed ucan_usb_devdata {
	__le16	opcode_channel;
	__le16	total_len;
	__le16	offset;
	__le16	id;
	u8	d[64];
};

/* these are PCAN-Chip specific cmds to control I/O pins */
#define UCAN_USB_CMD_DPIN_CFG_SET	0x8d
#define UCAN_USB_CMD_DPIN_CFG_REQ	0xca
#define UCAN_USB_CMD_DPIN_CFG_RSP	0xcb

#define UCAN_USB_CMD_DPIN_VAL_SET	0x8e
#define UCAN_USB_CMD_DPIN_VAL_REQ	0xcc
#define UCAN_USB_CMD_DPIN_VAL_RSP	0xcd

#define UCAN_USB_CMD_DPIN_SET_HIGH	0x8f
#define UCAN_USB_CMD_DPIN_SET_LOW	0x90

#define UCAN_USB_CMD_ANAL_VAL_REQ	0xce
#define UCAN_USB_CMD_ANAL_VAL_RSP	0xcf

struct __packed ucan_usb_io_ctrl {
	__le16	opcode;
	u16	reserved;

	__le32	io_val;
};

#endif
