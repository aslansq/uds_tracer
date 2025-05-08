/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CAN driver for PEAK System micro-CAN based adapters
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
#ifndef UCAN_H
#define UCAN_H

/* uCAN commands opcodes list (low-order 10 bits) */
#define UCAN_CMD_NOP			0x000
#define UCAN_CMD_RESET_MODE		0x001
#define UCAN_CMD_NORMAL_MODE		0x002
#define UCAN_CMD_LISTEN_ONLY_MODE	0x003
#define UCAN_CMD_TIMING_SLOW		0x004
#define UCAN_CMD_TIMING_FAST		0x005
#define UCAN_CMD_SET_STD_FILTER		0x006
#define UCAN_CMD_RESERVED2		0x007
#define UCAN_CMD_FILTER_STD		0x008
#define UCAN_CMD_TX_ABORT		0x009
#define UCAN_CMD_WR_ERR_CNT		0x00a
#define UCAN_CMD_SET_EN_OPTION		0x00b
#define UCAN_CMD_CLR_DIS_OPTION		0x00c

#define UCAN_CMD_SET_ERR_GEN1		0x00d
#define UCAN_CMD_SET_ERR_GEN		UCAN_CMD_SET_ERR_GEN1
#define UCAN_CMD_SET_ERR_GEN2		0x00e
#define UCAN_CMD_DIS_ERR_GEN		0x00f
#define UCAN_CMD_RX_BARRIER		0x010
#define UCAN_CMD_SET_ERR_GEN_S		0x011

#define UCAN_CMD_END_OF_COLLECTION	0x3ff

/* uCAN received messages list */
#define UCAN_MSG_CAN_RX			0x0001
#define UCAN_MSG_ERROR			0x0002
#define UCAN_MSG_STATUS			0x0003
#define UCAN_MSG_BUSLOAD		0x0004

#define UCAN_MSG_CACHE_CRITICAL		0x0102

/* uCAN transmitted messages */
#define UCAN_MSG_CAN_TX			0x1000

/* uCAN Tx Pause record */
#define UCAN_MSG_CAN_TX_PAUSE		0x1002

/* uCAN command common header */
#define UCAN_CMD_OPCODE(c)		((c)->opcode_channel & 0x3ff)
#define UCAN_CMD_CHANNEL(c)		((c)->opcode_channel >> 12)
#define UCAN_CMD_OPCODE_CHANNEL(c, o)	cpu_to_le16(((c) << 12) | ((o) & 0x3ff))

struct __packed ucan_command {
	__le16	opcode_channel;
	__le16	args[3];
};

#define UCAN_TSLOW_BRP_BITS		10
#define UCAN_TFAST_BRP_BITS		10

/* current version of uCAN IP core */
#define UCAN_TSLOW_TSGEG1_BITS		8
#define UCAN_TSLOW_TSGEG2_BITS		7
#define UCAN_TSLOW_SJW_BITS		7

#define UCAN_TFAST_TSGEG1_BITS		5
#define UCAN_TFAST_TSGEG2_BITS		4
#define UCAN_TFAST_SJW_BITS		4

#define UCAN_TSLOW_BRP_MASK		((1 << UCAN_TSLOW_BRP_BITS) - 1)
#define UCAN_TSLOW_TSEG1_MASK		((1 << UCAN_TSLOW_TSGEG1_BITS) - 1)
#define UCAN_TSLOW_TSEG2_MASK		((1 << UCAN_TSLOW_TSGEG2_BITS) - 1)
#define UCAN_TSLOW_SJW_MASK		((1 << UCAN_TSLOW_SJW_BITS) - 1)

#define UCAN_TFAST_BRP_MASK		((1 << UCAN_TFAST_BRP_BITS) - 1)
#define UCAN_TFAST_TSEG1_MASK		((1 << UCAN_TFAST_TSGEG1_BITS) - 1)
#define UCAN_TFAST_TSEG2_MASK		((1 << UCAN_TFAST_TSGEG2_BITS) - 1)
#define UCAN_TFAST_SJW_MASK		((1 << UCAN_TFAST_SJW_BITS) - 1)

/* uCAN TIMING_SLOW command fields */
#define UCAN_TSLOW_SJW_T(s, t)		(((s) & UCAN_TSLOW_SJW_MASK) | \
								((!!(t)) << 7))
#define UCAN_TSLOW_TSEG2(t)		((t) & UCAN_TSLOW_TSEG2_MASK)
#define UCAN_TSLOW_TSEG1(t)		((t) & UCAN_TSLOW_TSEG1_MASK)
#define UCAN_TSLOW_BRP(b)		cpu_to_le16((b) & UCAN_TSLOW_BRP_MASK) 

struct __packed ucan_timing_slow {
	__le16	opcode_channel;

	u8	ewl;		/* Error Warning limit */
	u8	sjw_t;		/* Sync Jump Width + Triple sampling */
	u8	tseg2;		/* Timing SEGment 2 */
	u8	tseg1;		/* Timing SEGment 1 */

	__le16	brp;		/* BaudRate Prescaler */
};

/* uCAN TIMING_FAST command fields */
#define UCAN_TFAST_SJW(s)		((s) & UCAN_TFAST_SJW_MASK)
#define UCAN_TFAST_TSEG2(t)		((t) & UCAN_TFAST_TSEG2_MASK)
#define UCAN_TFAST_TSEG1(t)		((t) & UCAN_TFAST_TSEG1_MASK)
#define UCAN_TFAST_BRP(b)		cpu_to_le16((b) & UCAN_TFAST_BRP_MASK)

struct __packed ucan_timing_fast {
	__le16	opcode_channel;

	u8	unused;
	u8	sjw;		/* Sync Jump Width */
	u8	tseg2;		/* Timing SEGment 2 */
	u8	tseg1;		/* Timing SEGment 1 */

	__le16	brp;		/* BaudRate Prescaler */
};

/* (old) uCAN FILTER_STD command fields */
#define UCAN_FLTSTD_ROW_IDX_BITS	6

struct __packed ucan_filter_std {
	__le16	opcode_channel;

	__le16	idx;
	__le32	mask;		/* CAN-ID bitmask in idx range */
};

/* uCAN SET_STD_FILTER command fields */
struct __packed ucan_std_filter {
	__le16	opcode_channel;

	u8	unused;
	u8	idx;
	__le32	mask;		/* CAN-ID bitmask in idx range */
};

/* uCAN TX_ABORT commands fields */
#define UCAN_TX_ABORT_FLUSH		0x0001

struct __packed ucan_tx_abort {
	__le16	opcode_channel;

	__le16	flags;
	u32	unused;
};

/* uCAN WR_ERR_CNT command fields */
#define UCAN_WRERRCNT_TE		0x4000	/* Tx error cntr write Enable */
#define UCAN_WRERRCNT_RE		0x8000	/* Rx error cntr write Enable */

struct __packed ucan_wr_err_cnt {
	__le16	opcode_channel;

	__le16	sel_mask;
	u8	tx_counter;	/* Tx error counter new value */
	u8	rx_counter;	/* Rx error counter new value */

	u16	unused;
};

/* uCAN SET_EN_OPTION/CLR_DIS_OPTION commands fields */
#define UCAN_OPTION_ERROR		0x0001
#define UCAN_OPTION_BUSLOAD		0x0002
#define UCAN_OPTION_ISO_MODE		0x0004
#define UCAN_OPTION_LO_MODE		0x0008	/* Diag FD only */
#define UCAN_OPTION_20AB_MODE		0x0010	/* force CAN 2.0 A/B format */
#define UCAN_OPTION_TS_SOF		0x0020	/* TS at SOF instead of EOF */
#define UCAN_OPTION_SELF_ACK		0x0040	/* Send ACK when writing */
#define UCAN_OPTION_BRS_IGN		0x0080	/* Ign rx BRS frames */

struct __packed ucan_option {
	__le16	opcode_channel;

	__le16	mask;
	u16	unused;
	__le16	ext_mask;
};

/* uCAN received messages global format */
struct __packed ucan_msg {
	__le16	size;
	__le16	type;
	__le32	ts_low;
	__le32	ts_high;
};

/* uCAN flags for CAN/CANFD messages */
#define UCAN_MSG_API_SRR		0x80	/* tx frame echo */
#define UCAN_MSG_ERROR_STATE_IND	0x40	/* error state indicator */
#define UCAN_MSG_BITRATE_SWITCH		0x20	/* bitrate switch */
#define UCAN_MSG_EXT_DATA_LEN		0x10	/* extended data length */
#define UCAN_MSG_SINGLE_SHOT		0x08
#define UCAN_MSG_HW_SRR			0x04	/* loopback */
#define UCAN_MSG_EXT_ID			0x02
#define UCAN_MSG_RTR			0x01

#define UCAN_MSG_CHANNEL(m)		((m)->channel_dlc & 0xf)
#define UCAN_MSG_DLC(m)			((m)->channel_dlc >> 4)

struct __packed ucan_rx_msg {
	__le16	size;
	__le16	type;
	__le32	ts_low;
	__le32	ts_high;
	__le32	tag_low;
	__le32	tag_high;
	u8	channel_dlc;
	u8	client;
	__le16	flags;
	__le32	can_id;
#ifdef CONFIG_UBSAN_BOUNDS_STRICT
	u8	d[];
#else
	u8	d[0];
#endif
};

/* uCAN error types */
#define UCAN_ERMSG_BIT_ERROR		0
#define UCAN_ERMSG_FORM_ERROR		1
#define UCAN_ERMSG_STUFF_ERROR		2
#define UCAN_ERMSG_OTHER_ERROR		3
#define UCAN_ERMSG_ERR_CNT_DEC		4

#define UCAN_ERMSG_CHANNEL(e)		((e)->channel_type_d & 0x0f)
#define UCAN_ERMSG_ERRTYPE(e)		(((e)->channel_type_d >> 4) & 0x07)
#define UCAN_ERMSG_D(e)			((e)->channel_type_d & 0x80)

#define UCAN_ERMSG_ERRCODE(e)		((e)->code_g & 0x7f)
#define UCAN_ERMSG_G(e)			((e)->code_g & 0x80)

struct __packed ucan_error_msg {
	__le16	size;
	__le16	type;
	__le32	ts_low;
	__le32	ts_high;
	u8	channel_type_d;
	u8	code_g;
	u8	tx_err_cnt;
	u8	rx_err_cnt;
};

#define UCAN_STMSG_CHANNEL(e)		((e)->channel_p_w_b & 0x0f)
#define UCAN_STMSG_RB(e)		((e)->channel_p_w_b & 0x10)
#define UCAN_STMSG_PASSIVE(e)		((e)->channel_p_w_b & 0x20)
#define UCAN_STMSG_WARNING(e)		((e)->channel_p_w_b & 0x40)
#define UCAN_STMSG_BUSOFF(e)		((e)->channel_p_w_b & 0x80)

struct __packed ucan_status_msg {
	__le16	size;
	__le16	type;
	__le32	ts_low;
	__le32	ts_high;
	u8	channel_p_w_b;
	u8	unused[3];
};

#define UCAN_BLMSG_CHANNEL(e)		((e)->channel & 0x0f)

struct __packed ucan_bus_load_msg {
	__le16	size;
	__le16	type;
	__le32	ts_low;
	__le32	ts_high;
	u8	channel;
	u8	unused;
	__le16	bus_load;
};

#define UCAN_CCMSG_CHANNEL(e)		((e)->channel & 0x0f)

struct __packed ucan_cache_critical_msg {
	__le16	size;
	__le16	type;
	__le32	ts_low;
	__le32	ts_high;
	u8	channel;
	u8	unused[3];
};

/* uCAN transmitted message format */
#define UCAN_MSG_CHANNEL_DLC(c, d)	(((c) & 0xf) | ((d) << 4))

struct __packed ucan_tx_msg {
	__le16	size;
	__le16	type;
	__le32	tag_low;
	__le32	tag_high;
	u8	channel_dlc;
	u8	client;
	__le16	flags;
	__le32	can_id;
#ifdef CONFIG_UBSAN_BOUNDS_STRICT
	u8	d[];
#else
	u8	d[0];
#endif
};

/* uCAN Tx Pause record */
#define UCAN_TXPAUSE_DELAY_MAX		0x3ff
#define UCAN_TXPAUSE_DELAY(d)		((d) & UCAN_TXPAUSE_DELAY_MAX)

struct __packed ucan_tx_pause {
	__le16	size;
	__le16	type;
	__le16	delay;			/* pause in µs (10-low order bits) */
	__le16	reserved;
};

/* uCAN message programming interface */
struct pcandev *ucan_init_cmd(struct pcandev *dev);
void *ucan_add_cmd(struct pcandev *dev, int cmd_op);

/* pcan interface functions */
void *ucan_add_cmd_nop(struct pcandev *dev);
void *ucan_add_cmd_set_en_option(struct pcandev *dev, u16 mask, u16 ext_mask);
void *ucan_add_cmd_clr_dis_option(struct pcandev *dev, u16 mask, u16 ext_mask);

int ucan_set_bus_on(struct pcandev *dev);
int ucan_set_bus_off(struct pcandev *dev);
int ucan_set_options(struct pcandev *dev, u16 mask, u16 ext_mask);
int ucan_clr_options(struct pcandev *dev, u16 mask, u16 ext_mask);

int ucan_set_BTR0BTR1(struct pcandev *dev, u16 btr0btr1);
int ucan_clr_err_counters(struct pcandev *dev);
int ucan_set_all_acceptance_filter(struct pcandev *dev);

int ucan_set_msg_filters(struct pcandev *dev, u16 mask);
int ucan_clr_msg_filters(struct pcandev *dev, u16 mask);

int ucan_tx_abort(struct pcandev *dev, u16 flags);
int ucan_rx_barrier(struct pcandev *dev);

int ucan_soft_init(struct pcandev *dev, struct pcan_version *hw_ver);
int ucan_device_open_fd(struct pcandev *dev, struct pcanfd_init *pfdi,
			u16 ext_to_set, u16 ext_to_clr);
int ucan_device_close(struct pcandev *dev);
int ucan_reset_path(struct pcandev *dev);

/* uCAN messages builder */
int ucan_encode_msg(struct pcandev *dev, u8 *buffer_addr, int buffer_size);
int ucan_encode_msgs_buffer(struct pcandev *dev, u8 *buffer_addr,
			    int *buffer_size);

/* uCAN messages handler */
int ucan_handle_msgs_buffer(struct ucan_engine *ucan, void *msg_addr,
			    int msg_len);
int ucan_handle_msgs_list(struct ucan_engine *ucan, void *msg_addr,
			  int *msg_count);

/* uCAN message base handlers */
int ucan_post_canrx_msg(struct pcandev *dev, struct ucan_rx_msg *msg,
						struct pcan_timeval *ptv);
int ucan_handle_error(struct ucan_engine *ucan, struct ucan_msg *rx_msg,
						void *arg);
int ucan_post_error_msg(struct pcandev *dev, struct ucan_error_msg *em,
						struct pcan_timeval *ptv);
int ucan_post_status_msg(struct pcandev *dev, struct ucan_status_msg *sm,
						struct pcan_timeval *ptv);
int ucan_handle_bus_load(struct ucan_engine *ucan, struct ucan_msg *rx_msg,
						void *arg);
int ucan_post_bus_load_msg(struct pcandev *dev, struct ucan_bus_load_msg *bl,
						struct pcan_timeval *ptv);
int ucan_post_overflow_msg(struct pcandev *dev, struct pcan_timeval *ptv);

#endif
