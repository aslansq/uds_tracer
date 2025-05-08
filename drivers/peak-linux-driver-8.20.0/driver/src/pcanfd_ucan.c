/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcanfd_ucan.c - the uCAN firmware global interface
 *
 * Copyright (C) 2014-2020 PEAK System-Technik GmbH <www.peak-system.com>
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
 * Credit:       Austin Anderson <austin.anderson@advanced-space.com>
 *               (endianess issue with __le16 flags in struct ucan_tx_msg)
 */
/*#define DEBUG*/
/*#undef DEBUG*/

#include "src/pcan_common.h"
#include "src/pcan_main.h"

#include "src/pcanfd_core.h"		/* uCAN base common messages */
#include "src/pcanfd_ucan.h"		/* uCAN base common messages */

#ifdef USB_SUPPORT
#include "src/pcanfd_usb_fw.h"		/* uCAN USB devices specific messages */
#endif

#ifdef NETDEV_SUPPORT
#include "src/pcan_netdev.h"            /* for hotplug pcan_netdev_register() */
#else
#include <linux/can/dev.h>
#endif

#ifdef DEBUG
//#define DEBUG_BUS_LOAD	/* very (VERY) verbose */
#define DEBUG_BUS_MODE
#define DEBUG_RX
#define DEBUG_SLOW_BITTIMINGS
#define DEBUG_FAST_BITTIMINGS
#define DEBUG_TRACE
#else
//#define DEBUG_BUS_LOAD	/* very (VERY) verbose */
//#define DEBUG_BUS_MODE
//#define DEBUG_RX
//#define DEBUG_SLOW_BITTIMINGS
//#define DEBUG_FAST_BITTIMINGS
//#define DEBUG_TRACE
#endif

/* if defined, set the number of times an outgoing CAN message is repeated
 * in the outgoing stream. This enables to do some internal tests only!
 * This MUST not be defined in a production version!
 */
//#define UCAN_TEST_TX_BURST		5

/* If defined, do single-frame uCAN ouput records.
 * If not defined, the buffer is filled with pending frames.
 */
//#define UCAN_WRITE_ONE_FRAME_PER_PACKET

#ifndef get_can_dlc
/* some (very) old Kernels don't #define get_can_dlc() */
#define get_can_dlc(i)			(min_t(__u8, (i), 8))

#endif

/* Adapter timinig capabilities */
static const struct pcanfd_bittiming_range ucan_slow_capabilities = {

	.brp_min = 1,
	.brp_max = (1 << UCAN_TSLOW_BRP_BITS),
	.brp_inc = 1,

	.tseg1_min = 1,
	.tseg1_max = (1 << UCAN_TSLOW_TSGEG1_BITS),	// 64 or 256
	.tseg2_min = 1,
	.tseg2_max = (1 << UCAN_TSLOW_TSGEG2_BITS),	// 16 or 128
	.sjw_min = 1,
	.sjw_max = (1 << UCAN_TSLOW_SJW_BITS)		// 16 or 128
};

static const struct pcanfd_bittiming_range ucan_fast_capabilities = {

	.brp_min = 1,
	.brp_max = (1 << UCAN_TFAST_BRP_BITS),
	.brp_inc = 1,
	.tseg1_min = 1,
	.tseg1_max = (1 << UCAN_TFAST_TSGEG1_BITS),	// 16 or 32
	.tseg2_min = 1,
	.tseg2_max = (1 << UCAN_TFAST_TSGEG2_BITS),	// 8 or 16
	.sjw_min = 1,
	.sjw_max = (1 << UCAN_TFAST_SJW_BITS)		// 4 or 16
};

typedef struct __array_of_struct(pcanfd_available_clock, 6)
	pcanfd_6_clocks_device;

static const pcanfd_6_clocks_device ucan_clocks = {
	.count = 6,
	.list = {
		[0] = { .clock_Hz = 80*MHz, .clock_src = 80*MHz, },
		[1] = { .clock_Hz = 20*MHz, .clock_src = 240*MHz, },
		[2] = { .clock_Hz = 24*MHz, .clock_src = 240*MHz, },
		[3] = { .clock_Hz = 30*MHz, .clock_src = 240*MHz, },
		[4] = { .clock_Hz = 40*MHz, .clock_src = 240*MHz, },
		[5] = { .clock_Hz = 60*MHz, .clock_src = 240*MHz, },
	}
};

static const u8 pcan_fd_dlc2len[] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 12, 16, 20, 24, 32, 48, 64
};

/* get data length from can_dlc with sanitized can_dlc */
static u8 pcan_dlc2len(u8 can_dlc)
{
	return pcan_fd_dlc2len[can_dlc & 0x0F];
}

static const u8 pcan_fd_len2dlc[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,	/* 0 - 8 */
	9, 9, 9, 9,			/* 9 - 12 */
	10, 10, 10, 10,			/* 13 - 16 */
	11, 11, 11, 11,			/* 17 - 20 */
	12, 12, 12, 12,			/* 21 - 24 */
	13, 13, 13, 13, 13, 13, 13, 13,	/* 25 - 32 */
	14, 14, 14, 14, 14, 14, 14, 14,	/* 33 - 40 */
	14, 14, 14, 14, 14, 14, 14, 14,	/* 41 - 48 */
	15, 15, 15, 15, 15, 15, 15, 15,	/* 49 - 56 */
	15, 15, 15, 15, 15, 15, 15, 15	/* 57 - 64 */
};

/* map the sanitized data length to an appropriate data length code */
static u8 pcan_len2dlc(u8 len)
{
	if (len > 64)
		return 0xF;

	return pcan_fd_len2dlc[len];
}

struct pcandev *ucan_init_cmd(struct pcandev *dev)
{
	dev->ucan.cmd_len = 0;
	return dev;
}

void *ucan_add_cmd(struct pcandev *dev, int cmd_op)
{
	const int cmd_size = sizeof(u64);
	struct ucan_command *cmd;

	if (dev->ucan.cmd_len + cmd_size > dev->ucan.cmd_size) {
#ifdef DEBUG_ALL
		pr_err("%s: %s(): failed to add cmd %03xh: "
		       "device cmd buffer not large enough (%u+%u > %u)\n",
		       DEVICE_NAME, __func__, cmd_op,
		       dev->ucan.cmd_len, cmd_size, dev->ucan.cmd_size);
#endif
		return NULL;
	}

	cmd = dev->ucan.cmd_head + dev->ucan.cmd_len;

	/* unused bits should be 0 */
	*(u64 *)cmd = 0;

	cmd->opcode_channel = UCAN_CMD_OPCODE_CHANNEL(dev->can_idx, cmd_op);
	dev->ucan.cmd_len += cmd_size;

	return cmd;
}

/* uCAN commands interface functions */

void *ucan_add_cmd_nop(struct pcandev *dev)
{
	return ucan_add_cmd(dev, UCAN_CMD_NOP);
}

static void *ucan_add_cmd_reset_mode(struct pcandev *dev)
{
	return ucan_add_cmd(dev, UCAN_CMD_RESET_MODE);
}

static void *ucan_add_cmd_normal_mode(struct pcandev *dev)
{
	return ucan_add_cmd(dev, UCAN_CMD_NORMAL_MODE);
}

static void *ucan_add_cmd_listen_only_mode(struct pcandev *dev)
{
	return ucan_add_cmd(dev, UCAN_CMD_LISTEN_ONLY_MODE);
}

static void *ucan_add_cmd_timing_slow(struct pcandev *dev,
					struct pcan_bittiming *pbr)
{
	struct ucan_timing_slow *cmd = ucan_add_cmd(dev, UCAN_CMD_TIMING_SLOW);
	if (cmd) {
#ifdef DEBUG_SLOW_BITTIMINGS
		pr_info(DEVICE_NAME ": %s CAN%u = SLOW"
			"[brp=%u tseg1=%u tseg2=%u sjw=%u ts=%u]\n",
			dev->adapter->name, pcan_idx(dev)+1,
			pbr->brp, pbr->tseg1, pbr->tseg2, pbr->sjw, pbr->tsam);
#endif
		cmd->sjw_t = UCAN_TSLOW_SJW_T(pbr->sjw - 1, pbr->tsam);
		cmd->tseg1 = UCAN_TSLOW_TSEG1(pbr->tseg1 - 1);
		cmd->tseg2 = UCAN_TSLOW_TSEG2(pbr->tseg2 - 1);
		cmd->brp = UCAN_TSLOW_BRP(pbr->brp - 1);

		cmd->ewl = 96;	/* default */
	}

	return cmd;
}

static void *ucan_add_cmd_timing_fast(struct pcandev *dev,
					struct pcan_bittiming *pbr)
{
	struct ucan_timing_fast *cmd = ucan_add_cmd(dev, UCAN_CMD_TIMING_FAST);
	if (cmd) {
#ifdef DEBUG_FAST_BITTIMINGS
		pr_info(DEVICE_NAME ": %s CAN%u = FAST"
			"[brp=%u tseg1=%u tseg2=%u sjw=%u]\n",
			dev->adapter->name, pcan_idx(dev)+1,
			pbr->brp, pbr->tseg1, pbr->tseg2, pbr->sjw);
#endif
		cmd->sjw = UCAN_TFAST_SJW(pbr->sjw - 1);
		cmd->tseg1 = UCAN_TFAST_TSEG1(pbr->tseg1 - 1);
		cmd->tseg2 = UCAN_TFAST_TSEG2(pbr->tseg2 - 1);
		cmd->brp = UCAN_TFAST_BRP(pbr->brp - 1);
	}

	return cmd;
}

static void *ucan_add_cmd_tx_abort(struct pcandev *dev, u16 flags)
{
	struct ucan_tx_abort *cmd = ucan_add_cmd(dev, UCAN_CMD_TX_ABORT);
	if (cmd)
		cmd->flags = cpu_to_le16(flags);

	return cmd;
}

static void *ucan_add_cmd_rx_barrier(struct pcandev *dev)
{
	return ucan_add_cmd(dev, UCAN_CMD_RX_BARRIER);
}

static void *ucan_add_cmd_wr_err_cnt(struct pcandev *dev, u16 sel_mask,
			      u8 tx_counter, u8 rx_counter)
{
	struct ucan_wr_err_cnt *cmd = ucan_add_cmd(dev, UCAN_CMD_WR_ERR_CNT);
	if (cmd) {
		cmd->sel_mask = cpu_to_le16(sel_mask);
		cmd->tx_counter = tx_counter;
		cmd->rx_counter = rx_counter;
	}

	return cmd;
}

void *ucan_add_cmd_set_en_option(struct pcandev *dev, u16 mask, u16 ext_mask)
{
	struct ucan_option *cmd = ucan_add_cmd(dev, UCAN_CMD_SET_EN_OPTION);
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(mask=%04xh, ext_mask=%04xh)\n",
		__func__, mask, ext_mask);
#endif
	if (cmd) {
		cmd->mask = cpu_to_le16(mask);
		cmd->ext_mask = cpu_to_le16(ext_mask);
	}

	return cmd;
}

void *ucan_add_cmd_clr_dis_option(struct pcandev *dev, u16 mask, u16 ext_mask)
{
	struct ucan_option *cmd = ucan_add_cmd(dev, UCAN_CMD_CLR_DIS_OPTION);
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(mask=%04xh, ext_mask=%04xh)\n",
		__func__, mask, ext_mask);
#endif
	if (cmd) {
		cmd->mask = cpu_to_le16(mask);
		cmd->ext_mask = cpu_to_le16(ext_mask);
	}

	return cmd;
}

/* pcan interface functions */

int ucan_clr_err_counters(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	if (!ucan_add_cmd_wr_err_cnt(ucan_init_cmd(dev),
				     UCAN_WRERRCNT_TE|UCAN_WRERRCNT_RE, 0, 0))
		return -EINVAL;

	dev->tx_error_counter = 0;
	dev->rx_error_counter = 0;

	/* send the command */
	return dev->ucan.ops->send_cmd(dev);
}

/* int ucan_set_all_acceptance_filter(struct pcandev *dev)
 */
int ucan_set_all_acceptance_filter(struct pcandev *dev)
{
	struct ucan_std_filter *cmd = NULL;
	const int n = 1 << UCAN_FLTSTD_ROW_IDX_BITS;
	int i, err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* build (and send) a command for each row */
	ucan_init_cmd(dev);
	for (i = 0; i < n; i++) {

		while (1) {
			cmd =  ucan_add_cmd(dev, UCAN_CMD_SET_STD_FILTER);
			if (cmd)
				break;

			/* not enough room? 1st, send the cmds */
			err = dev->ucan.ops->send_cmd(dev);
			if (err)
				return err;

			/* next, reset the cmd buffer and retry */
			ucan_init_cmd(dev);
		}

		cmd->idx = i;
		cmd->mask = cpu_to_le32(0xffffffff);
	}

	/* send the commands */
	return dev->ucan.ops->send_cmd(dev);
}

/* int ucan_tx_abort(struct pcandev *dev, u16 flags)
 */
int ucan_tx_abort(struct pcandev *dev, u16 flags)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u, flags=%04xh)\n",
		__func__, pcan_idx(dev)+1, flags);
#endif
	if (!ucan_add_cmd_tx_abort(ucan_init_cmd(dev), flags))
		return -EINVAL;

	/* send the command */
	return dev->ucan.ops->send_cmd(dev);
}

/* int ucan_rx_barrier(struct pcandev *dev)
 */
int ucan_rx_barrier(struct pcandev *dev)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_MODE)
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	if (!ucan_add_cmd_rx_barrier(ucan_init_cmd(dev)))
		return -EINVAL;

	/* send the command */
	return dev->ucan.ops->send_cmd(dev);
}

static int ucan_convert_BTR0BTR1(u16 btr0btr1,
				 const struct pcanfd_bittiming_range *pcap,
				 struct pcan_bittiming *pbr)
{
	struct pcan_bittiming_abs at_original, at_convert;
	const u32 sysclock = 80*MHz;

	pcan_btr0btr1_to_abstract(&at_original, btr0btr1);

	if (!pcan_convert_abstract(&at_convert, &at_original, pcap, sysclock))
		return -EINVAL;

	return pcan_abstract_to_bittiming(pbr, &at_convert, pcap, sysclock);
}

/* int ucan_set_bus_on(struct pcandev *dev)
 *
 * Set uCAN device bus controller to ON.
 */
int ucan_set_bus_on(struct pcandev *dev)
{
	int err;
	void *cmd = (dev->init_settings.flags & PCANFD_INIT_LISTEN_ONLY) ?
			ucan_add_cmd_listen_only_mode(ucan_init_cmd(dev)) :
			ucan_add_cmd_normal_mode(ucan_init_cmd(dev));

#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_MODE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u): %s\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1,
		(dev->init_settings.flags & PCANFD_INIT_LISTEN_ONLY) ?
			"LISTEN_ONLY" : "NORMAL");
#endif

	if (!cmd)
		return -EINVAL;

	/* send the command */
	err = dev->ucan.ops->send_cmd(dev);
	if (!err)
		dev->flags |= PCAN_DEV_BUS_ON;

	return err;
}

/* int ucan_set_bus_off(struct pcandev *dev)
 *
 * Set uCAN device bus controller to OFF.
 */
int ucan_set_bus_off(struct pcandev *dev)
{
	int err;

#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_MODE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u): bus=%d tx=%u\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1,
		dev->bus_state, dev->tx_frames_counter);
#endif
	if (!ucan_add_cmd_reset_mode(ucan_init_cmd(dev)))
		return -EINVAL;

	/* wait a bit for last data to be written on CAN bus.
	 * This delay is mandatory when going to BUS_OFF with uCAN devices:
	 * - 5 ms is not enough if any data buffer was almost filled
	 * - 10 ms is enough for uCAN USB devices but not for uCAN PCIe devices
	 *
	 * Note: this wait MUST not be interruptible for USB devices.
	 */
	if (dev->tx_frames_counter > 0)
		mdelay(50);

	/* send the command */
	err = dev->ucan.ops->send_cmd(dev);
	if (!err)
		dev->flags &= ~(PCAN_DEV_BUS_ON|PCAN_DEV_CTRLR_FATAL);

	return err;
}

int ucan_set_options(struct pcandev *dev, u16 mask, u16 ext_mask)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u, mask=%04xh, ext_mask=%04xh)\n",
		__func__, pcan_idx(dev)+1, mask, ext_mask);
#endif
	if (!ucan_add_cmd_set_en_option(ucan_init_cmd(dev), mask, ext_mask))
		return -EINVAL;

	/* send the command */
	return dev->ucan.ops->send_cmd(dev);
}

int ucan_clr_options(struct pcandev *dev, u16 mask, u16 ext_mask)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u, mask=%04xh, ext_mask=%04xh)\n",
		__func__, pcan_idx(dev)+1, mask, ext_mask);
#endif
	if (!ucan_add_cmd_clr_dis_option(ucan_init_cmd(dev), mask, ext_mask))
		return -EINVAL;

	/* send the command */
	return dev->ucan.ops->send_cmd(dev);
}

/* int ucan_set_timing_slow(struct pcandev *dev,
 *			     struct pcan_bittiming *pbr)
 */
static int ucan_set_timing_slow(struct pcandev *dev,
				struct pcan_bittiming *pbr)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u, btr=%d brp=%d tseg1=%d tseg2=%d "
		"sjw=%d tsam=%d)\n", __func__, pcan_idx(dev)+1,
		pbr->bitrate, pbr->brp, pbr->tseg1, pbr->tseg2, pbr->sjw,
		pbr->tsam);
#endif
	if (!ucan_add_cmd_timing_slow(ucan_init_cmd(dev), pbr))
		return -EINVAL;

	/* send the command */
	return dev->ucan.ops->send_cmd(dev);
}

/* int ucan_set_timing_fast(struct pcandev *dev,
 *			     struct pcan_bittiming *pbr)
 */
static int ucan_set_timing_fast(struct pcandev *dev,
				struct pcan_bittiming *pbr)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u, btr=%d brp=%d tseg1=%d tseg2=%d "
		"sjw=%d tsam=%d)\n", __func__, pcan_idx(dev)+1,
		pbr->bitrate, pbr->brp, pbr->tseg1, pbr->tseg2, pbr->sjw,
		pbr->tsam);
#endif
	if (!ucan_add_cmd_timing_fast(ucan_init_cmd(dev), pbr))
		return -EINVAL;

	/* send the command */
	return dev->ucan.ops->send_cmd(dev);
}

static inline int ucan_set_iso_mode(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	return ucan_set_options(dev, UCAN_OPTION_ISO_MODE, 0);
}

static inline int ucan_clr_iso_mode(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	return ucan_clr_options(dev, UCAN_OPTION_ISO_MODE, 0);
}

#ifdef UCAN_OPTION_20AB_MODE
static inline int ucan_set_can20ab_mode(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	return ucan_set_options(dev, UCAN_OPTION_20AB_MODE, 0);
}

static inline int ucan_clr_can20ab_mode(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	return ucan_clr_options(dev, UCAN_OPTION_20AB_MODE, 0);
}
#endif

/* int ucan_set_BTR0BTR1(struct pcandev *dev, u16 btr0btr1)
 */
int ucan_set_BTR0BTR1(struct pcandev *dev, u16 btr0btr1)
{
	struct pcan_bittiming bitrate;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	switch (btr0btr1) {

#ifdef BTR0BTR1_1MBPS
	case BTR0BTR1_1MBPS:
		bitrate.tsam = 0;
		bitrate.sjw = 1;
		bitrate.tseg1 = 29;
		bitrate.tseg2 = 10;
		bitrate.brp = 2;
		break;
#endif
#ifdef BTR0BTR1_500KBPS
	case BTR0BTR1_500KBPS:
		bitrate.tsam = 0;
		bitrate.sjw = 1;
		bitrate.tseg1 = 34;
		bitrate.tseg2 = 5;
		bitrate.brp = 4;
		break;
#endif
	default:

		/* Need to convert this value for PCAN-USB (Pro) FD */
		if (!ucan_convert_BTR0BTR1(btr0btr1,
					&ucan_slow_capabilities, &bitrate)) {
			pr_err(DEVICE_NAME
				": unable to convert BTR0BTR1 0x%04x\n",
				btr0btr1);
			pr_err(DEVICE_NAME
				": won't be able to transfer data on CAN#%d\n",
				pcan_idx(dev)+1);

			return -EINVAL;
		}
	}

	return ucan_set_timing_slow(dev, &bitrate);
}

static int ucan_dev_reset(struct pcandev *dev)
{
	int err;

#ifdef DEBUG_BUS_MODE
	pr_info(DEVICE_NAME ": %s(%s CAN%u)\n", __func__,
		dev->adapter->name, pcan_idx(dev)+1);
#endif

	err = ucan_set_bus_off(dev);
	if (err)
		return err;

	err = ucan_clr_err_counters(dev);
	if (err)
		return err;

	return ucan_set_bus_on(dev);
}

int ucan_soft_init(struct pcandev *dev, struct pcan_version *hw_ver)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	pcan_soft_init_ex(dev,
			(const struct pcanfd_available_clocks *)&ucan_clocks,
			&ucan_slow_capabilities,
				PCAN_DEV_FD_RDY |
				PCAN_DEV_BUSLOAD_RDY |
				PCAN_DEV_ECHO_RDY |
				PCAN_DEV_SJA1000_RDY);
	
	/* Tx Pause option is available for all HW running FW >= 2.4.0 */
	if (VER_NUM(hw_ver->major,
		    hw_ver->minor,
		    hw_ver->subminor) >= VER_NUM(2, 4, 0))
		dev->features |= PCAN_DEV_TXPAUSE_RDY;

	dev->dbittiming_caps = &ucan_fast_capabilities;

	/* if global dbitrate is not 0, then consider to open in CAN-FD */
	if (dev->def_init_settings.data.bitrate) {
		dev->def_init_settings.flags |= PCANFD_INIT_FD;

		pcan_bittiming_normalize(&dev->def_init_settings.data,
					 dev->sysclock_Hz,
					 &ucan_fast_capabilities);

		/* reset default init settings with new data bitrate specs */
		pcanfd_copy_init(&dev->init_settings, &dev->def_init_settings);
	}

	/* this kind of controller can be reset */
	dev->device_reset = ucan_dev_reset;

	return 0;
}

static int ucan_set_clock_domain(struct pcandev *dev, struct pcanfd_init *pfdi)
{
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	/* set the uCAN clock domain */
	if (dev->ucan.ops->set_clk_domain) {

		err = dev->ucan.ops->set_clk_domain(dev, pfdi);

		/* looks like pfdi->clock_Hz was wrong: it must be
		 * reset to its default value and the bittimings must be
		 * computed accordingly and the clock domain reset as well.
		 */
		if (err == -EINVAL) {

			pr_err(DEVICE_NAME
				": %s CAN%u unsupported user clock %u Hz: "
				"using default %u Hz\n",
				dev->adapter->name, pcan_idx(dev)+1,
				pfdi->clock_Hz,
				ucan_clocks.list[0].clock_Hz);

			/* uCAN device default clock value */
			pfdi->clock_Hz = ucan_clocks.list[0].clock_Hz;

			/* use bitrate bps value as reference to rebuild BRP,
			 * TSEGx and SJW accroding to the default clock
			 */
			pcan_bitrate_to_bittiming(&pfdi->nominal,
							dev->bittiming_caps,
							pfdi->clock_Hz);
			if (pfdi->flags & PCANFD_INIT_FD)
				pcan_bitrate_to_bittiming(&pfdi->data,
							dev->dbittiming_caps,
							pfdi->clock_Hz);

			/* finaly, reset to default clock domain */
			err = dev->ucan.ops->set_clk_domain(dev, pfdi);
		}

#if defined(DEBUG_SLOW_BITTIMINGS) || defined(DEBUG_FAST_BITTIMINGS)
		pr_info(DEVICE_NAME ": %s CAN%u = [clk=%u]\n",
			dev->adapter->name, pcan_idx(dev)+1,
			pfdi->clock_Hz);
#endif
	}

	return err;
}

int ucan_device_open_fd(struct pcandev *dev, struct pcanfd_init *pfdi,
			u16 ext_to_set, u16 ext_to_clr)
{
	u16 opt_to_clr = 0, opt_to_set = UCAN_OPTION_ERROR;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	err = ucan_set_clock_domain(dev, pfdi);
	if (err)
		goto fail;

	/* reset error counters */
	err = ucan_clr_err_counters(dev);
	if (err)
		goto fail;

	/* clear/set options */
	if (pfdi->flags & PCANFD_INIT_BUS_LOAD_INFO)
		opt_to_set |= UCAN_OPTION_BUSLOAD;
	else
		opt_to_clr |= UCAN_OPTION_BUSLOAD;

	if (pfdi->flags & PCANFD_INIT_FD) {

		opt_to_clr |= UCAN_OPTION_20AB_MODE;

		if (pfdi->flags & PCANFD_INIT_FD_NON_ISO) {
			opt_to_clr |= UCAN_OPTION_ISO_MODE;
		} else {
			opt_to_set |= UCAN_OPTION_ISO_MODE;
		}
	} else {
		/* force CAN 2.0 A/B mode */
		opt_to_set |= UCAN_OPTION_20AB_MODE;
	}

	if (opt_to_clr || ext_to_clr) {
		err = ucan_clr_options(dev, opt_to_clr, ext_to_clr);
		if (err)
			goto fail;
	}

	if (opt_to_set || ext_to_set) {
		err = ucan_set_options(dev, opt_to_set, ext_to_set);
		if (err)
			goto fail;
	}

	if (pfdi->flags & PCANFD_INIT_FD) {
		err = ucan_set_timing_fast(dev, &pfdi->data);
		if (err)
			goto fail;
	}

	err = ucan_set_timing_slow(dev, &pfdi->nominal);
	if (err)
		goto fail;

	/* Set filter mode: accept all */
	err = ucan_set_all_acceptance_filter(dev);
fail:
	return err;
}

/*
 * int ucan_device_close(struct pcandev *dev)
 */
int ucan_device_close(struct pcandev *dev)
{
	u16 opt_to_clr = UCAN_OPTION_BUSLOAD|UCAN_OPTION_ERROR;
	int err;

	if (dev->flags & PCAN_DEV_SELF_ACK)
		opt_to_clr |= UCAN_OPTION_SELF_ACK;
	if (dev->flags & PCAN_DEV_TS_SOF)
		opt_to_clr |= UCAN_OPTION_TS_SOF;
	if (dev->flags & PCAN_DEV_BRS_IGN)
		opt_to_clr |= UCAN_OPTION_BRS_IGN;

	/* clear all options set */
	err = ucan_clr_options(dev, opt_to_clr, 0);
	if (!err)
		dev->flags &= ~(PCAN_DEV_SELF_ACK |
				PCAN_DEV_TS_SOF |
				PCAN_DEV_BRS_IGN);

	return err;
}

/* int ucan_reset_path(struct pcandev *dev)
 *
 * After reset, the CAN core stays active.
 */
int ucan_reset_path(struct pcandev *dev)
{
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	/* reset the Tx path first */
	err = ucan_tx_abort(dev, UCAN_TX_ABORT_FLUSH);
	if (err)
		goto fail;

	/* reset the Rx path next */
	err = ucan_rx_barrier(dev);
fail:
	return err;
}

/* int ucan_post_canrx_msg(struct pcandev *dev,
 *			   struct ucan_rx_msg *rm, struct pcan_timeval *ptv)
 *
 *	Default handler of incoming CAN messages
 */
int ucan_post_canrx_msg(struct pcandev *dev,
			struct ucan_rx_msg *rm, struct pcan_timeval *ptv)
{
	struct pcanfd_rxmsg rx = {};
	const u16 msg_flags = le16_to_cpu(rm->flags);
	u8 dlc = UCAN_MSG_DLC(rm);

#if defined(DEBUG_TRACE) || defined(DEBUG_RX)
	pr_info(DEVICE_NAME ": %s(CAN%u): wCANStatus=%xh "
		"rx=[flags=0x%02x len=0x%02x ts32=0x%08x id=0x%08x]\n",
		__func__, pcan_idx(dev)+1,
		dev->wCANStatus, msg_flags, dlc,
		le32_to_cpu(rm->ts_low), le32_to_cpu(rm->can_id));
#endif

	/* stay in "ctrl rx queue overrun" in case of CACHE_WARNING */
	if (!(dev->flags & PCAN_DEV_CTRLR_FATAL))
		pcan_clear_status_bit(dev, CAN_ERR_QOVERRUN);

	/* ignore any CAN data while we're in BUSOFF */
	switch (dev->bus_state) {
	case PCANFD_UNKNOWN:
	case PCANFD_ERROR_BUSOFF:
#ifdef DEBUG_RX
		pr_info(DEVICE_NAME
			": %s(CAN%u): rx CAN msg discarded (bus_state=%u)\n",
			__func__, pcan_idx(dev)+1, dev->bus_state);
#endif
		return 0;
	default:
		break;
	}

	pcanfd_msg_store_dlc(&rx.msg, dlc);

	if (msg_flags & UCAN_MSG_EXT_DATA_LEN) {
		/* CAN FD frame */
		rx.msg.type = PCANFD_TYPE_CANFD_MSG;
		if (msg_flags & UCAN_MSG_BITRATE_SWITCH)
			rx.msg.flags |= PCANFD_MSG_BRS;

		if (msg_flags & UCAN_MSG_ERROR_STATE_IND)
			rx.msg.flags |= PCANFD_MSG_ESI;

		rx.msg.data_len = pcan_dlc2len(dlc);
	} else {
		/* CAN 2.0 frame */
		rx.msg.type = PCANFD_TYPE_CAN20_MSG;
		rx.msg.data_len = get_can_dlc(dlc);
	}

	rx.msg.id = le32_to_cpu(rm->can_id);
	if (ptv) {
		rx.msg.flags |= PCANFD_TIMESTAMP;
		rx.hwtv = *ptv;
	}

#ifdef PCANFD_RAWTIMESTAMP
	rx.msg.flags |= PCANFD_RAWTIMESTAMP;
	rx.msg.raw_timestamp = ((u64 )le32_to_cpu(rm->ts_high) << 32) |
							le32_to_cpu(rm->ts_low);
#endif
	if (msg_flags & UCAN_MSG_EXT_ID)
		rx.msg.flags |= PCANFD_MSG_EXT;

	if (msg_flags & UCAN_MSG_RTR)
		rx.msg.flags |= PCANFD_MSG_RTR;
	else
		memcpy(rx.msg.data, rm->d, rx.msg.data_len);

	if (msg_flags & UCAN_MSG_API_SRR) {
		rx.msg.flags |= PCANFD_MSG_ECHO;
		rx.msg.ctrlr_data[PCANFD_ECHOID] = rm->client;
	}

	if (msg_flags & UCAN_MSG_HW_SRR)
		rx.msg.flags |= PCANFD_MSG_SLF;

	if (msg_flags & UCAN_MSG_SINGLE_SHOT)
		rx.msg.flags |= PCANFD_MSG_SNG;
	
#ifdef DEBUG_RX
	dump_mem("posted pcanfd_msg", &rx, sizeof(struct pcanfd_msg));
#endif
	return pcan_xxxdev_rx(dev, &rx);
}

/* int ucan_handle_error(struct ucan_engine *ucan,
 *			 struct ucan_msg *rx_msg, void *arg)
 *				
 *
 *	Default handler of incoming error messages
 */
int ucan_handle_error(struct ucan_engine *ucan,
		      struct ucan_msg *rx_msg, void *arg)
{
	/* WARNING: ucan pointer can be NULL! */
	struct ucan_error_msg *er = (struct ucan_error_msg *)rx_msg;
	struct pcandev *dev = (struct pcandev *)arg;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(): "
		"rx=[type=0x%02x ts32=0x%08x d=%u g=%u code=0x%02x "
		"err_cnt[rx=%u tx=%u]]\n",
		__func__,
		UCAN_ERMSG_ERRTYPE(er), le32_to_cpu(er->ts_low),
		!!UCAN_ERMSG_D(er),
		!!UCAN_ERMSG_G(er),
		UCAN_ERMSG_ERRCODE(er),
		er->rx_err_cnt, er->tx_err_cnt);
#endif

	/* keep a trace of tx and rx error counters for later use */
	dev->rx_error_counter = er->rx_err_cnt;
	dev->tx_error_counter = er->tx_err_cnt;

#ifdef DEBUG
	if (dev->rx_error_counter || dev->tx_error_counter)
		pr_err(DEVICE_NAME
			": %s CAN%u: errors: rx=%u tx=%u\n",
			dev->adapter->name, dev->can_idx,
			dev->rx_error_counter, dev->tx_error_counter);
#endif

	return 0;
}

int ucan_post_error_msg(struct pcandev *dev,
		      struct ucan_error_msg *er, struct pcan_timeval *ptv)
{
	struct pcanfd_rxmsg rx = {};

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(): "
		"rx=[type=0x%02x ts32=0x%08x d=%u g=%u code=0x%02x "
		"err_cnt[rx=%u tx=%u]]\n",
		__func__,
		UCAN_ERMSG_ERRTYPE(er), le32_to_cpu(er->ts_low),
		!!UCAN_ERMSG_D(er),
		!!UCAN_ERMSG_G(er),
		UCAN_ERMSG_ERRCODE(er),
		er->rx_err_cnt, er->tx_err_cnt);
#endif

	/* call default handler first */
	ucan_handle_error(NULL, (struct ucan_msg *)er, dev);

	/* not an "error" but a status... */
	if (UCAN_ERMSG_ERRTYPE(er) > PCANFD_ERRMSG_OTHER) {
		rx.msg.type = PCANFD_TYPE_STATUS;
		rx.msg.flags = PCANFD_ERROR_BUS;
		rx.msg.id = dev->bus_state;
	} else {
		pcan_handle_error_msg(dev, &rx,
			UCAN_ERMSG_ERRTYPE(er), UCAN_ERMSG_ERRCODE(er),
			UCAN_ERMSG_D(er), UCAN_ERMSG_G(er));
	}

	if (ptv) {
		rx.msg.flags |= PCANFD_TIMESTAMP;
		rx.hwtv = *ptv;
	}

	return pcan_xxxdev_rx(dev, &rx);
}

int ucan_post_status_msg(struct pcandev *dev,
			 struct ucan_status_msg *st, struct pcan_timeval *ptv)
{
	struct pcanfd_rxmsg rx;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u): bus_state=%u "
		"ts32=0x%08x RB=%u EP=%u EW=%u BO=%u]\n",
		__func__, pcan_idx(dev)+1, dev->bus_state,
		le32_to_cpu(st->ts_low), !!UCAN_STMSG_RB(st),
		!!UCAN_STMSG_PASSIVE(st), !!UCAN_STMSG_WARNING(st),
		!!UCAN_STMSG_BUSOFF(st));
#endif

	if (UCAN_STMSG_BUSOFF(st)) {
		pcan_handle_busoff(dev, &rx);
	} else if (!pcan_handle_error_status(dev, &rx,
					     UCAN_STMSG_WARNING(st),
					     UCAN_STMSG_PASSIVE(st))) {
		/* no error bit (so, no error, back to active state) */
		pcan_handle_error_active(dev, &rx);
	}

	if (rx.msg.type == PCANFD_TYPE_NOP) {
#ifdef DEBUG
		pr_info(DEVICE_NAME ": %s(CAN%u): status msg discarded (NOP)\n",
			__func__, pcan_idx(dev)+1);
#endif
		return 0;
	}

	if (ptv) {
		rx.msg.flags |= PCANFD_TIMESTAMP;
		rx.hwtv = *ptv;
	}

	return pcan_xxxdev_rx(dev, &rx);
}

/*
 * int ucan_handle_bus_load(struct ucan_engine *ucan,
 *			 struct ucan_msg *rx_msg, void *arg)
 *
 * Default handler for bus_load notification messages.
 */
int ucan_handle_bus_load(struct ucan_engine *ucan,
			 struct ucan_msg *rx_msg, void *arg)
{
	struct ucan_bus_load_msg *bl = (struct ucan_bus_load_msg *)rx_msg;
	struct pcandev *dev = (struct pcandev *)arg;

#ifdef DEBUG_BUS_LOAD
	pr_info(DEVICE_NAME ": %s(CAN%u): bus_state=%u "
		"ts32=0x%08x bus_load=%u\n",
		dev->adapter->name, pcan_idx(dev)+1, dev->bus_state,
		le32_to_cpu(bl->ts_low), le16_to_cpu(bl->bus_load));
#endif

	dev->bus_load = (10000UL * le16_to_cpu(bl->bus_load)) >> 12;

	return 0;
}

int ucan_post_bus_load_msg(struct pcandev *dev,
		       struct ucan_bus_load_msg *bl, struct pcan_timeval *ptv)
{
	struct pcanfd_rxmsg rx = {
		.msg = {
			.type = PCANFD_TYPE_STATUS,
			.id = PCANFD_BUS_LOAD,
		}
	};

#ifdef DEBUG_BUS_LOAD
	pr_info(DEVICE_NAME ": %s(%s CAN%d)\n", __func__,
		dev->adapter->name, pcan_idx(dev)+1);
#endif

	/* call default handler */
	ucan_handle_bus_load(NULL, (struct ucan_msg *)bl, dev);

	if (ptv) {
		rx.msg.flags |= PCANFD_TIMESTAMP;
		rx.hwtv = *ptv;
	}

	return pcan_xxxdev_rx(dev, &rx);
}

int ucan_post_overflow_msg(struct pcandev *dev, struct pcan_timeval *ptv)
{
	struct pcanfd_rxmsg rx = {
		.msg = {
			.type = PCANFD_TYPE_STATUS,
			.flags = PCANFD_ERROR_CTRLR,
			.id = PCANFD_RX_OVERFLOW,
		}
	};

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* do some filter to avoid overflowing rx queue with the same STATUS
	 * messages
	 */
	if (!pcan_handle_error_ctrl(dev, &rx, PCANFD_RX_OVERFLOW))
		return 0;

	if (ptv) {
		rx.msg.flags |= PCANFD_TIMESTAMP;
		rx.hwtv = *ptv;
	}

	return pcan_xxxdev_rx(dev, &rx);
}

static int ucan_handle_msg(struct ucan_engine *ucan, void *msg_addr)
{
	struct ucan_msg *msg;
	void *arg = NULL;
	int msg_size;
	u16 msg_type;
	int ci = -1, err;
	int (*msg_cb)(struct ucan_engine *, struct ucan_msg *, void *);
	struct pcandev *dev = container_of(ucan, struct pcandev, ucan);

	msg = (struct ucan_msg *)msg_addr;
	msg_size = le16_to_cpu(msg->size);
	if (!msg->size || !msg->type) {
		/* null packet found: end of list */
		msg_size = 0;
		goto lbl_return;
	}

	msg_type = le16_to_cpu(msg->type);

#ifdef DEBUG_RX
	switch (msg_type) {
#ifdef USB_SUPPORT
	case UCAN_USB_MSG_CALIBRATION:
#endif
#ifndef DEBUG_BUS_LOAD
	case UCAN_MSG_BUSLOAD:
#endif
	case UCAN_MSG_ERROR:
		break;
	default:
		dump_mem("received rec", msg, msg_size);
	}
#endif

	/* theses msgs carry a channel index */
	switch (msg_type) {

	case UCAN_MSG_CAN_RX:
		ci = UCAN_MSG_CHANNEL((struct ucan_rx_msg *)msg);
		break;

	case UCAN_MSG_ERROR:
		ci = UCAN_ERMSG_CHANNEL((struct ucan_error_msg *)msg);
		break;

	case UCAN_MSG_STATUS:
		ci = UCAN_STMSG_CHANNEL((struct ucan_status_msg *)msg);
		break;

	case UCAN_MSG_BUSLOAD:
		ci = UCAN_BLMSG_CHANNEL((struct ucan_bus_load_msg *)msg);
		break;

	case UCAN_MSG_CACHE_CRITICAL:
		ci = UCAN_CCMSG_CHANNEL((struct ucan_cache_critical_msg *)msg);
		if (ci < ucan->devs_count) {
			struct pcanfd_rxmsg s;

			dev = ucan_dev(ucan, ci);

			if (!(dev->flags & PCAN_DEV_CTRLR_FATAL)) {
				dev->flags |= PCAN_DEV_CTRLR_FATAL;

				pr_warn(DEVICE_NAME
					": %s CAN%u rx cache has reached a "
					"critical size\n",
					dev->adapter->name, ci+1);
			}

			pcan_handle_error_ctrl(dev, &s, PCANFD_RX_OVERFLOW);

#ifdef NETDEV_SUPPORT
			pcan_netdev_rx(dev, &s);
#else
			if (pcan_chardev_rx(dev, &s) > 0)
				pcan_event_signal(&dev->in_event);
#endif
			return 0;
		}
		break;

#ifdef USB_SUPPORT
	case UCAN_CMD_END_OF_COLLECTION:
	case 0xffff:
		/* such msg_type should not occur, but we never know... */
		return 0;

	case UCAN_USB_MSG_CALIBRATION:
		arg = pcan_usb_get_if(ucan_dev(ucan, 0));
		if (!arg) {

			/* protect from interrupt that might occur before
			 * initialization completion: in that rare case, this
			 * message can be silently ignored.
			 */
			goto lbl_return;
		}
		break;

	case UCAN_USB_MSG_OVERRUN:
		ci = UCAN_USB_OVMSG_CHANNEL((struct ucan_usb_ovr_msg *)msg);
		break;
#endif
	}

	if (msg_type < ucan->ops->handle_msg_size) {
		if (!ucan->ops->handle_msg_table[msg_type]) {
			pr_info(DEVICE_NAME
				": %s: unhandled rx uCAN rec %03xh: "
				"it is ignored\n",
				dev->adapter->name, msg_type);
			dump_mem("unhandled rec", msg, msg_size);

			/* stop everything */
			msg_size = -EBADMSG;

			goto lbl_return;
		}

		msg_cb = ucan->ops->handle_msg_table[msg_type];

	} else {
		if (!ucan->ops->handle_private_msg) {
			pr_err(DEVICE_NAME
			       ": %s: out of range rx uCAN rec "
			       "%03xh >= %u: it is ignored\n",
			       dev->adapter->name, msg_type,
			       ucan->ops->handle_msg_size);
			dump_mem("out of range rec", msg, msg_size);

			/* stop everything */
			msg_size = -EBADMSG;

			goto lbl_return;
		}

		msg_cb = ucan->ops->handle_private_msg;
	}

	if (ci != -1) {

		/* be sure of the index read... */
		if ((ci < 0) || (ci >= ucan->devs_count)) {
			pr_warn(DEVICE_NAME
				": %s: invalid channel %d in uCAN "
				"rec %03xh: it is ignored\n",
				dev->adapter->name, ci, msg_type);

			/* stop everything */
			msg_size = -EBADMSG;

			goto lbl_return;
		}

		arg = ucan_dev(ucan, ci);
	}

	err = msg_cb(ucan, msg, arg);
	if (err < 0)
		return err;

lbl_return:
	return msg_size;
}

/* int ucan_handle_msgs_buffer(struct ucan_engine *ucan, void *buffer_addr,
 *			       int buffer_len)
 */
int ucan_handle_msgs_buffer(struct ucan_engine *ucan, void *buffer_addr,
			    int buffer_len)
{
	const int msg_size_max = ALIGN(sizeof(struct ucan_rx_msg) + 64, 4);
	struct ucan_msg *rx_msg;
	int msg_len = 0;
	u8 *msg_ptr = buffer_addr;
	int msg_nb = 0, msg_size;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p, %d): end=%p\n", __func__,
		buffer_addr, buffer_len, buffer_addr+buffer_len);
#endif

	/* process any pending fragmented msg first */
	if (ucan->frag_size > 0) {
		int tail_size;

		rx_msg = (struct ucan_msg *)ucan->frag_rec;
		msg_size = le16_to_cpu(rx_msg->size);

		tail_size = msg_size - ucan->frag_size;
		memcpy(ucan->frag_rec + ucan->frag_size,
		       msg_ptr, tail_size);

		msg_size = ucan_handle_msg(ucan, ucan->frag_rec);
		if (msg_size <= 0)
			return msg_size;

		msg_ptr += tail_size;
		msg_nb++;

		msg_len += tail_size;
		ucan->frag_size = 0;
	}

	/* loop reading all the records from the incoming message */
	for (; msg_len < buffer_len; msg_len += msg_size) {
		rx_msg = (struct ucan_msg *)msg_ptr;
		msg_size = le16_to_cpu(rx_msg->size);

		/* a null packet can be found at the end of a list */
		if (!msg_size)
			break;

		/* a message can't be too large */
		if (msg_size > msg_size_max) {
			pr_err(DEVICE_NAME
			       ": too large msg size %d > %d in Rx buffer\n",
			       msg_size, msg_size_max);
			msg_size = -EBADMSG;
			break;
		}

		/* handle fragmentation */
		if ((msg_len + msg_size) > buffer_len) {

			/* fragmented msg */
			ucan->frag_size = buffer_len - msg_len;
			memcpy(ucan->frag_rec, msg_ptr, ucan->frag_size);

#ifdef DEBUG
			pr_warn(DEVICE_NAME
				": msg #%u fragmented: %u/%u bytes pending "
				"(buffer_len=%d msg_len=%d)\n",
				msg_nb+1, ucan->frag_size, msg_size,
				buffer_len, msg_len);
#endif
			break;
		}

		msg_size = ucan_handle_msg(ucan, msg_ptr);
		if (msg_size <= 0)
			break;

		msg_ptr += msg_size;
		msg_nb++;
	}

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s(): found %u msg(s) in %u bytes buffer\n",
		__func__, msg_nb, buffer_len);
#endif

	/* in case of error, dump the whole messages (list) */
	if (msg_size < 0) {
		if (msg_size != -ENOSPC) {
			pr_err(DEVICE_NAME
				": msg[addr=%p ptr=%p len=%d]\n",
				buffer_addr, msg_ptr, msg_len);
			dump_mem("received err msg", buffer_addr, buffer_len);
		}

		return msg_size;
	}

	return 0;
}

/*
 * int ucan_handle_msgs_list(struct ucan_engine *ucan, void *msg_addr,
 *				int *msg_count)
 */
int ucan_handle_msgs_list(struct ucan_engine *ucan, void *msg_addr,
			  int *msg_count)
{
	u8 *msg_ptr = msg_addr;
	int msg_len = 0;
	int msg_size = 0;
	int i;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%d)\n", __func__, *msg_count);
#endif

	/* loop reading all the records from the incoming message */
	for (i = 0; i < *msg_count; i++) {
		msg_size = ucan_handle_msg(ucan, msg_ptr);

		/* a null packet can be found at the end of a list */
		if (msg_size <= 0)
			break;

		/* next record is 32-Bit aligned */
		msg_size = ALIGN(msg_size, 4);

		msg_len += msg_size;
		msg_ptr += msg_size;
	}

	*msg_count = i;

	/* in case of error, dump the failed message,
	 * except in case of not enough space in Rx FIFO...
	 */
	if (msg_size < 0) {
		if (msg_size != -ENOSPC)
			dump_mem("received err msg",
				 msg_ptr, sizeof(struct ucan_rx_msg));

		return msg_size;
	}

	return 0;
}

/*
 * int ucan_encode_msg(struct pcandev *dev, u8 *buffer_addr, int buffer_size)
 *
 * @return:
 *
 * 	> 0		size in bytes of the copied msg
 *	-ENODATA	if Tx fifo is empty
 *	-ENOSPC		if output buffer is not large enough
 */
int ucan_encode_msg(struct pcandev *dev, u8 *buffer_addr, int buffer_size)
{
	struct ucan_tx_msg *tx_msg = (struct ucan_tx_msg *)buffer_addr;
	struct pcanfd_txmsg tx;
	int tx_msg_size, err, dlc;
	u16 tx_flags;
#ifdef UCAN_TEST_TX_BURST
	int i;
#endif

	/* first get the size of the enqueued CAN message */
	err = pcan_fifo_peek(&dev->tx_fifo, &tx);
	if (err == -ENODATA)
		return err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": Encoding msg {type=%u id=%xh dlen=%u flg=%xh}\n",
		tx.msg.type, tx.msg.id, tx.msg.data_len, tx.msg.flags);
#endif

	dlc = pcan_len2dlc(tx.msg.data_len);
	err = tx_msg_size = ALIGN(sizeof(*tx_msg)+ pcan_dlc2len(dlc), 4);

#ifdef UCAN_MSG_CAN_TX_PAUSE
	if (dev->tx_iframe_delay_us)
		err += ALIGN(sizeof(struct ucan_tx_pause), 4);
#endif

#ifdef UCAN_TEST_TX_BURST
	err *= UCAN_TEST_TX_BURST;
#endif
	/* if not enough room to entirely copy it, stop here */
	if (err > buffer_size) {
#ifdef DEBUG
		pr_info(DEVICE_NAME ": %s(): "
			"%u bytes left too small for storing %u bytes\n",
			__func__, buffer_size, err);
#endif
		return -ENOSPC;
	}

	/* really read the message (NULL avoid 2nd useless memcpy()) */
	pcan_txfifo_get(dev, NULL);

#ifdef UCAN_TEST_TX_BURST
	err /= UCAN_TEST_TX_BURST;
	for (i = 0; i < UCAN_TEST_TX_BURST; i++ ) {
#endif

		/* DON'T memset here! useless and no guarantee that cmd
		 * will be finished before we put our values!
		 */
		tx_msg->type = cpu_to_le16(UCAN_MSG_CAN_TX);
		tx_msg->size = cpu_to_le16(tx_msg_size);

		if (tx.msg.flags & PCANFD_MSG_EXT) {
			tx_flags = UCAN_MSG_EXT_ID;
			tx_msg->can_id = cpu_to_le32(tx.msg.id & CAN_EFF_MASK);
		} else {
			tx_flags = 0;
			tx_msg->can_id = cpu_to_le32(tx.msg.id & CAN_SFF_MASK);
		}

		/* echo: application self-received frame: it's like
		 * PCANFD_MSG_SLF but with an application specific flag and
		 * an 8 bit application specific value.
		 */
		if (tx.msg.flags & PCANFD_MSG_ECHO) {
			tx_flags |= UCAN_MSG_HW_SRR|UCAN_MSG_API_SRR;
			tx_msg->client = tx.msg.ctrlr_data[PCANFD_ECHOID];

		/* self:
		 * frame written on the bus and copied in rx path too.
		 */
		} else if (tx.msg.flags & PCANFD_MSG_SLF) {
			tx_flags |= UCAN_MSG_HW_SRR;
		}

		/* Single-Shot */
		if (tx.msg.flags & PCANFD_MSG_SNG)
			tx_flags |= UCAN_MSG_SINGLE_SHOT;
			
		switch (tx.msg.type) {

		case PCANFD_TYPE_CANFD_MSG:
			/* CAN-FD frames */
			tx_flags |= UCAN_MSG_EXT_DATA_LEN;

			if (tx.msg.flags & PCANFD_MSG_BRS)
				tx_flags |= UCAN_MSG_BITRATE_SWITCH;

			if (tx.msg.flags & PCANFD_MSG_ESI)
				tx_flags |= UCAN_MSG_ERROR_STATE_IND;

			break;

		case PCANFD_TYPE_CAN20_MSG:
			/* CAN 2.0 frames */
			if (tx.msg.flags & PCANFD_MSG_RTR)
				tx_flags |= UCAN_MSG_RTR;

			if (dlc == PCANFD_CAN20_MAXDATALEN) {
				u8 tmp_dlc = pcanfd_msg_read_dlc(&tx.msg);
				if (tmp_dlc > dlc)
					dlc = tmp_dlc;
			}
			break;
		}

		tx_msg->channel_dlc = UCAN_MSG_CHANNEL_DLC(dev->can_idx, dlc);
		tx_msg->flags = cpu_to_le16(tx_flags);

		memcpy(tx_msg->d, tx.msg.data, tx.msg.data_len);

#ifdef UCAN_MSG_CAN_TX_PAUSE
		if (dev->tx_iframe_delay_us) {
			struct ucan_tx_pause *p;

			if (dev->tx_iframe_delay_us > UCAN_TXPAUSE_DELAY_MAX)
				dev->tx_iframe_delay_us = 
						UCAN_TXPAUSE_DELAY_MAX;

			p = (struct ucan_tx_pause *)(buffer_addr + tx_msg_size);
			p->type = cpu_to_le16(UCAN_MSG_CAN_TX_PAUSE);
			p->size = cpu_to_le16(sizeof(*p));
			p->delay = cpu_to_le16(dev->tx_iframe_delay_us);
			p->reserved = 0;
		}
#endif /* UCAN_MSG_CAN_TX_PAUSE */

#ifdef UCAN_TEST_TX_BURST
		tx_msg->d[0] = (u8 )i;
		buffer_addr += err;
		tx_msg = (struct ucan_tx_msg *)buffer_addr;
	}

	err *= UCAN_TEST_TX_BURST;
#endif

	return err;
}

/*
 * int ucan_encode_msgs_buffer(struct pcandev *dev,
 *				u8 *buffer_addr, int *buffer_size)
 *
 * Read msgs from CAN Tx fifo and encode them into the given buffer.
 *
 *	-ENODATA	if no more data in CAN fifo,
 *	-ENOSPC		if *buffer_size is not large enough to store a TX_x
 *			record
 *			any other -ERR.
 *	>= 0		if output buffer is full of TX_x records,
 */
int ucan_encode_msgs_buffer(struct pcandev *dev, u8 *buffer_addr,
				int *buffer_size)
{
	int msg_len, msg_count;
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(buffer_size=%d)\n", __func__, *buffer_size);
#endif

	for (msg_len = msg_count = 0; msg_len < *buffer_size; ) {
		err = ucan_encode_msg(dev, buffer_addr + msg_len,
					   *buffer_size - msg_len);
		if (err < 0) {
#ifdef DEBUG
			pr_info(DEVICE_NAME ": %s(): err %d while encoding msg "
				"*buffer_size=%d msg_len=%d msg_count=%d\n",
				__func__, err,
				*buffer_size, msg_len, msg_count);
#endif
			break;
		}

		/* to be sure to not count other msgs than CAN frames */
		if (!err)
			continue;

		msg_len += err;
		msg_count++;

#ifdef UCAN_WRITE_ONE_FRAME_PER_PACKET
		err = -ENODATA;
		break;
#endif
	}

	if (!msg_len) {
		*buffer_size = 0;
		return -ENODATA;
	}

	/* if the entire packet is not filled, set the size of last msg to 0
	 * to mark end-of-rec
	 */
	if (msg_len < *buffer_size) {
		*(u32 *)(buffer_addr + msg_len) = 0;
		msg_len += sizeof(u32);
	}

	/* set the whole size of the packet to send */
	*buffer_size = msg_len;

	dev->tx_frames_counter += msg_count;

#ifdef DEBUG
	dump_mem("encoded buffer", buffer_addr, *buffer_size);
#endif

	return !err ? msg_count : err;
}
