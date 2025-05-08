/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_netdev.c - CAN network device support functions
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
 *               Oliver Hartkopp <oliver.hartkopp@volkswagen.de> socket-CAN
 */
/* #define DEBUG */
/*#undef DEBUG*/

#include "src/pcan_common.h"
#include <linux/sched.h>
#include <linux/skbuff.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
#include <linux/can/skb.h>
#endif
#include "src/pcan_main.h"
#include "src/pcan_fifo.h"
#include "src/pcan_netdev.h"
#include "src/pcanfd_core.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
/* pcan_netdev_register() use alloc_candev() instead of alloc_netdev() */
#define USES_ALLOC_CANDEV

/* using alloc_candev() also means:
 * - don't care about LINUX_CAN_RESTART_TIMER (restart is handled by
 *   can_restart_work)
 */
#endif

/* If defined, then donot use linux-can can_restart() mechanism to prevent
 * race condition around CARRIER_ON (see comments around
 * FIX_CAN_RESTART_CARRIER_ON) but our own one.
 */
#define FIX_CAN_RESTART_CARRIER_ON

#ifdef USES_ALLOC_CANDEV
#include <linux/can/dev.h>

#ifdef FIX_CAN_RESTART_CARRIER_ON
#define USES_PCAN_RESTART
#endif

#else
#define USES_PCAN_RESTART
#endif

#ifdef DEBUG
#define DEBUG_TX
#define DEBUG_RX
#define DEBUG_OPEN
#define DEBUG_DEFCLK
#else
//#define DEBUG_TX
//#define DEBUG_RX
//#define DEBUG_OPEN
//#define DEBUG_DEFCLK
#endif

#define CAN_NETDEV_NAME		"can%d"

static char *assign  = NULL;
module_param(assign, charp, 0444);
MODULE_PARM_DESC(assign, "assignment for netdevice names to CAN devices");

static char *defclk = NULL;
module_param(defclk, charp, 0444);
MODULE_PARM_DESC(defclk, "default clock in Hz used by channels (0=default)");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
/* Mainline Kernel removed restart_timer from 4.8 *BUT* Canonical has decided
 * to backport the change in their 4.4.0-59.
 * -DLINUX_CAN_RESTART_TIMER should be decided by Makefile.
 */
#undef LINUX_CAN_RESTART_TIMER
#endif

#define pcan_priv	pcanusr

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
/* Note: Kernel 3.6 is the first one in which CAN-FD has been added.
 * Code below has been imported from linux-3.6/include/linux/can.h */
#define CAN_MAX_DLEN		8

#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)
static inline bool can_is_canfd_skb(const struct sk_buff *skb)
{
	return skb->len == CANFD_MTU;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
/* convert a timeval to ktime_t format: */
static inline ktime_t timeval_to_ktime(struct timeval tv)
{
	return ktime_set(tv.tv_sec, tv.tv_usec * NSEC_PER_USEC);
}
#endif

struct can_bittiming *pcan_netdev_get_bittiming(struct pcandev *dev)
{
	struct net_device *ndev = dev->netdev;
	if (ndev) {
		struct pcan_priv *priv = netdev_priv(ndev);
		return &priv->can.bittiming;
	}

	return NULL;
}

struct can_bittiming *pcan_netdev_get_dbittiming(struct pcandev *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
	struct net_device *ndev = dev->netdev;
	if (ndev) {
		struct pcan_priv *priv = netdev_priv(ndev);
		return &priv->can.data_bittiming;
	}
#endif
	return NULL;
}

/* Mainline drivers don't set any default nominal nor data bitrate.
 * Therefore, this function is useless.
 */

static void pcan_copy_bt_from_netdev(struct pcan_bittiming *pp,
					const struct can_bittiming *pc)
{
	pp->bitrate = pc->bitrate;
	pp->sample_point = pc->sample_point * 10;
	pp->tq = pc->tq;
	pp->tseg1 = pc->prop_seg + pc->phase_seg1;
	pp->tseg2 = pc->phase_seg2;
	pp->sjw = pc->sjw;
	pp->brp = pc->brp;
}

/* AF_CAN netdevice: open device */
static int pcan_netdev_open(struct net_device *dev)
{
	struct pcan_priv *priv = netdev_priv(dev);
	struct pcandev *pdev = priv->dev;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s)\n", __func__, dev->name);
#endif
	err = open_candev(dev);
	if (err)
		return err;

	pdev->init_settings.flags &=  ~(PCANFD_INIT_FD |
					PCANFD_INIT_FD_NON_ISO |
					PCANFD_INIT_LISTEN_ONLY |
					PCANFD_INIT_STD_MSG_ONLY |
					PCANFD_INIT_BUS_LOAD_INFO);

	pdev->init_settings.clock_Hz = priv->can.clock.freq;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	memset(&pdev->init_settings.nominal, '\0',
					sizeof(struct pcan_bittiming));

	pcan_copy_bt_from_netdev(&pdev->init_settings.nominal,
					&priv->can.bittiming);

	memset(&pdev->init_settings.data, '\0',
					sizeof(struct pcan_bittiming));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
	/* CAN_CTRLMODE_FD only exists from 3.15 */
	if (priv->can.ctrlmode & CAN_CTRLMODE_FD) {
		pdev->init_settings.flags |= PCANFD_INIT_FD;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,5)
		if (priv->can.ctrlmode & CAN_CTRLMODE_FD_NON_ISO)
			pdev->init_settings.flags |= PCANFD_INIT_FD_NON_ISO;
#endif
		pcan_copy_bt_from_netdev(&pdev->init_settings.data,
					&priv->can.data_bittiming);
	}
#endif
#endif

	if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
		pdev->init_settings.flags |= PCANFD_INIT_LISTEN_ONLY;

	/* yes we will do read and write */
	priv->open_flags = O_RDWR;

	err = pcan_open_path(pdev, priv);
	if (err)
		return -ENODEV;

	netif_start_queue(dev);

	return 0;
}

static struct pcandev *pcan_netdev_get_dev(struct pcan_priv *priv)
{
	struct pcandev *pdev = priv->dev;

	if (pdev) {
#if defined(DEBUG_TRACE) || defined(DEBUG_OPEN)
		pr_info(DEVICE_NAME ": %s(%s): plugged=%u\n",
			__func__, (pdev->netdev) ? pdev->netdev->name : "NULL",
			pdev->is_plugged);
#endif

		/* if we are unregistering the dev, then it is in the list. */
		if (pdev->netdev)

			/* check whether this device is always linked. */
			if (!pcan_is_device_in_list(pdev))
				return NULL;

		/* if the device is plugged out */
		if (!pdev->is_plugged)
			return NULL;
	}

	return pdev;
}

/* AF_CAN netdevice: close device */
static int pcan_netdev_close(struct net_device *dev)
{
	struct pcan_priv *priv = netdev_priv(dev);
	struct pcandev *pdev = pcan_netdev_get_dev(priv);

#if defined(DEBUG_TRACE) || defined(DEBUG_OPEN)
	pr_info(DEVICE_NAME ": %s(%s): pdev=%p pdev->netdev=%p vs. ndev=%p\n",
		__func__, dev->name, pdev, (pdev) ? pdev->netdev : NULL, dev);
#endif

	if (pdev)
		pcan_release_path(pdev, priv);

	netif_stop_queue(dev);
	close_candev(dev);

	priv->can.state = CAN_STATE_STOPPED;

	return 0;
}

/* AF_CAN netdevice: get statistics for device */
struct net_device_stats *pcan_netdev_get_stats(struct net_device *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
	struct pcan_priv *priv = netdev_priv(dev);

	/* TODO: read statistics from chip */
	return &priv->stats;
#else
	return &dev->stats;
#endif
}

/* AF_CAN netdevice: transmit handler for device */
static int pcan_netdev_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct pcan_priv *priv = netdev_priv(dev);
	struct pcandev *pdev = pcan_netdev_get_dev(priv);
	struct net_device_stats *stats = pcan_netdev_get_stats(dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	struct canfd_frame *cf = (struct canfd_frame *)skb->data;
#else
	struct can_frame *cf = (struct can_frame *)skb->data;
#endif
	pcan_lock_irqsave_ctxt lck_ctx;
	struct pcanfd_txmsg f;
	int err;

#if defined(DEBUG_TRACE) || defined(DEBUG_TX)
	pr_info(DEVICE_NAME ": %s(id=%xh dlc=%u "
		"[%02x %02x %02x %02x %02x %02x %02x %02x] "
		") < %s tx queue\n",
		__func__, cf->can_id,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
		cf->len,
#else
		cf->can_dlc,
#endif
		cf->data[0], cf->data[1], cf->data[2], cf->data[3],
		cf->data[4], cf->data[5], cf->data[6], cf->data[7],
		dev->name);
#endif

	/* if the device is plugged out */
	if (!pdev) {
		stats->tx_dropped++;
		goto free_out;
	}

	/* convert SocketCAN CAN frame to PCAN FIFO compatible format */
	memset(&f, '\0', sizeof(f));

	f.msg.type = PCANFD_TYPE_CAN20_MSG;
	f.msg.flags = PCANFD_MSG_STD;

	if (cf->can_id & CAN_RTR_FLAG)
		f.msg.flags |= PCANFD_MSG_RTR;
	if (cf->can_id & CAN_EFF_FLAG)
		f.msg.flags |= PCANFD_MSG_EXT;
	f.msg.id = cf->can_id & CAN_ERR_MASK;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
	if (can_is_canfd_skb(skb)) {

		f.msg.type = PCANFD_TYPE_CANFD_MSG;

		if (cf->flags & CANFD_ESI)
			f.msg.flags |= PCANFD_MSG_ESI;
		if (cf->flags & CANFD_BRS)
			f.msg.flags |= PCANFD_MSG_BRS;

#ifdef CAN_CTRLMODE_CC_LEN8_DLC
	} else if (cf->len == PCANFD_CAN20_MAXDATALEN) {
		pcanfd_msg_store_dlc(&f.msg,
				can_get_cc_dlc((struct can_frame *)skb->data,
					       priv->can.ctrlmode));

#endif
	}

	f.msg.data_len = cf->len;
#else
	f.msg.data_len = cf->can_dlc;
#endif

	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK) {

		/* Note: 
		 * - _SLF is supported by all devices (Self Receive Request)
		 * - _ECHO has been added for CANFD to add a user bit to the
		 *   _SLF frame.
		 * In socket-can, _SLF is used for echo management and 
		 * _SLF+_ECHO is used for loopbacked frames.
		 * In pcan, _ECHO = _SLF + userbit for CANFD equipments.
		 * In order to do like it is done in peak_xxx mainline drivers,
		 * _ECHO could be only used here *BUT* it is only handled by
		 * the uCAN module. Thus, should set both bits here.
		 */
		f.msg.flags |= PCANFD_MSG_SLF|PCANFD_MSG_ECHO;
	}

	if (priv->can.ctrlmode & CAN_CTRLMODE_ONE_SHOT)
		f.msg.flags |= PCANFD_MSG_SNG;

	memcpy(f.msg.data, cf->data, f.msg.data_len);

	/* put data into fifo */
	err = pcan_fifo_put(&pdev->tx_fifo, &f);
	if (err < 0) {
		pr_err(DEVICE_NAME ": Tx fifo full: frame %x dropped: "
			"%s net queue stopped\n", f.msg.id, dev->name);

		/* stop netdev queue when PCAN FIFO is full */
		stats->tx_fifo_errors++; /* just for informational purposes */
		netif_stop_queue(dev);

		stats->tx_dropped++;
		goto free_out;
	}

#ifdef DEBUG_TX
	pr_info(DEVICE_NAME ": %xh dlc=%d "
		"[%02x %02x %02x %02x %02x %02x %02x %02x] "
		"> %s CAN%u\n",
		f.msg.id, f.msg.data_len,
		f.msg.data[0], f.msg.data[1], f.msg.data[2], f.msg.data[3],
		f.msg.data[4], f.msg.data[5], f.msg.data[6], f.msg.data[7],
		pdev->adapter->name, pcan_idx(pdev)+1);
#endif
	/* if we just put the 1st message (=the fifo was empty), we can start
	 * writing on hardware if it is ready for doing this.
	 */
	pcan_lock_get_irqsave(&pdev->isr_lock, lck_ctx);

		/* if can device ready to send, start writing */
		__pcan_dev_start_writing(pdev, NULL);

	pcan_lock_put_irqrestore(&pdev->isr_lock, lck_ctx);

	stats->tx_packets++;
	stats->tx_bytes += f.msg.data_len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
	netdev_get_tx_queue(dev, 0)->trans_start = jiffies;
#endif

	/* stop Tx queue if we reach hi-water level */
	if (pcan_fifo_ratio(&pdev->tx_fifo) > txqhiwat) {
		netif_stop_queue(dev);
#ifdef DEBUG_TX
		pr_info(DEVICE_NAME ": Tx fifo hi-water reached: "
			"%s net queue stopped\n", dev->name);
#endif
	}

free_out:
	dev_kfree_skb(skb);

	return 0;
}

/* AF_CAN netdevice: receive function (put can_frame to netdev queue) */
int pcan_netdev_rx(struct pcandev *dev, struct pcanfd_rxmsg *pqm)
{
	struct pcanfd_msg *pf = &pqm->msg;
	struct net_device *ndev = dev->netdev;
	struct pcan_priv *priv;
	struct net_device_stats *stats;
	struct sk_buff *skb;
	u8 *prx_cnt, *ptx_cnt;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
	struct can_frame cf, *pcf = &cf;
#else
	struct canfd_frame cf, *pcf = &cf;
#endif
#ifdef FIX_CAN_RESTART_CARRIER_ON
	enum can_state prev_can_state;
#endif
	int ld, lf = sizeof(cf);

#if defined(DEBUG_TRACE) || defined(DEBUG_RX)
	pr_info(DEVICE_NAME ": %s(type=%d id=%xh flgs=%xh dlc=%u "
		"[%02x %02x %02x %02x %02x %02x %02x %02x] "
		") < %s CAN%u\n",
		__func__, pf->type, pf->id, pf->flags, pf->data_len,
		pf->data[0], pf->data[1], pf->data[2], pf->data[3],
		pf->data[4], pf->data[5], pf->data[6], pf->data[7],
		dev->adapter->name, pcan_idx(dev)+1);
#endif

	/* under high busload condition, interrupts may occur before everything
	 * has been completed.
	 */
	if (!ndev)
		return 0;

	priv = netdev_priv(ndev);

	stats = pcan_netdev_get_stats(ndev);

	switch (pf->type) {

	case PCANFD_TYPE_NOP:
		/* ignored */
		return 0;


	case PCANFD_TYPE_CANFD_MSG:
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
		/* Kernels < 3.6 don't know anything about CAN-FD... */
		return -EINVAL;

#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0)
		skb = dev_alloc_skb(lf);
#else
		if (!(priv->can.ctrlmode & CAN_CTRLMODE_FD)) {
			pr_err("%s: CANFD frame discarded (%s not CAN-FD)\n",
					DEVICE_NAME, ndev->name);
			return 0;
		}

		/* handle CAN-FD when kernel is ok for this */
		skb = alloc_canfd_skb(ndev, &pcf);
#endif
		break;

	case PCANFD_TYPE_STATUS:
		switch (pf->id) {
		case PCANFD_ERROR_ACTIVE:

#ifdef FIX_CAN_RESTART_CARRIER_ON
			prev_can_state = priv->can.state;
#endif

			/* event not converted. Moreover, sure that state was
			 * not ERROR_ACTIVE
			 */
			priv->can.state = CAN_STATE_ERROR_ACTIVE;

#ifdef FIX_CAN_RESTART_CARRIER_ON
			/* if we're back here because of restart-ms then
			 * do call netif_carrier_on() now
			 */
			if ((prev_can_state == CAN_STATE_BUS_OFF) &&
						    (priv->can.restart_ms))
				netif_carrier_on(ndev);
#endif

			/* netif_wake_queue() reschedules Tx queue */
			netif_wake_queue(ndev);

			/* fall through */
			fallthrough;
		case PCANFD_UNKNOWN:
		case PCANFD_BUS_ERROR:
		case PCANFD_BUS_LOAD:
			return 0;
		}

		/* fall through */
		fallthrough;
	case PCANFD_TYPE_ERROR_MSG:
	case PCANFD_TYPE_CAN20_MSG:
	default:
		lf = sizeof(struct can_frame);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
		skb = dev_alloc_skb(lf);
#else
		/* using 3.9+ version enables to forget this:
		 *
		 * can_skb_reserve(skb);
		 * can_skb_prv(skb)->ifindex = ndev->ifindex;
		 *
		 * which is mandatory when using dev_alloc_skb() for Kernels
		 * 3.9+
		 */
		skb = alloc_can_skb(ndev, (struct can_frame **)&pcf);
#endif
		break;
	}

	if (!skb)
		return -ENOMEM;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	skb->dev = ndev;
	skb->protocol = htons(ETH_P_CAN);
	skb->pkt_type = PACKET_BROADCAST;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
#endif

	memset(pcf, '\0', lf);

	switch (pf->type) {

	case PCANFD_TYPE_CAN20_MSG:
	case PCANFD_TYPE_CANFD_MSG:

		/* use hw timestamp if given, only relevant for CAN frames: */

		/* Consider netdev hw timestamps as RAW timestamps. Therefore,
		 * give socket application the raw value given by the device.
		 */
		if (dev->features & PCAN_DEV_HWTS_RDY) {
			struct skb_shared_hwtstamps *hwts;
			struct timeval ts;

			ts.tv_usec = do_div(pqm->hwtv.ts_us, USEC_PER_SEC);
			ts.tv_sec = (__kernel_time_t )pqm->hwtv.ts_us;

			hwts = skb_hwtstamps(skb);
			hwts->hwtstamp = timeval_to_ktime(ts);
		}
		break;
	}

	switch (pf->type) {

	case PCANFD_TYPE_ERROR_MSG:

		priv->can.can_stats.bus_error++;
		stats->rx_errors++;

		pcf->can_id = CAN_ERR_FLAG | CAN_ERR_PROT | CAN_ERR_BUSERROR;
		ld = CAN_ERR_DLC;

		switch (pf->id) {
		case PCANFD_ERRMSG_BIT:
			pcf->data[2] = CAN_ERR_PROT_BIT;
			break;
		case PCANFD_ERRMSG_FORM:
			pcf->data[2] = CAN_ERR_PROT_FORM;
			break;
		case PCANFD_ERRMSG_STUFF:
			pcf->data[2] = CAN_ERR_PROT_STUFF;
			break;
		case PCANFD_ERRMSG_OTHER:
		default:
			break;
		}

		/* set error location */
		pcf->data[3] = pf->data[0];

		/* Error occurred during transmission? */
		if (!(pf->flags & PCANFD_ERRMSG_RX))
			pcf->data[2] |= CAN_ERR_PROT_TX;

		pcf->data[6] = dev->tx_error_counter;
		pcf->data[7] = dev->rx_error_counter;

		break;

	case PCANFD_TYPE_STATUS:

		/* use device counters instead of data bytes saved into pf->data
		 * because these counters are copied into pf->data[] just 
		 * before being pushed into chardev rx fifo. Thus, pf->data[]
		 * don't contain any rx/tx err counters!
		 */
		prx_cnt = &dev->rx_error_counter;
		ptx_cnt = &dev->tx_error_counter;

		pcf->can_id |= CAN_ERR_FLAG;
		ld = CAN_ERR_DLC;

		switch (pf->id) {
		case PCANFD_ERROR_BUSOFF:
			if (priv->can.state == CAN_STATE_BUS_OFF) {
				kfree_skb(skb);
				return 0;
			}

			can_bus_off(ndev);
			
			/* this is not done by native linux-can drivers.
			 * looks like it MUST be for PCAN-USB
			 */
			netif_stop_queue(ndev);

			priv->can.can_stats.bus_off++;
			priv->can.state = CAN_STATE_BUS_OFF;
			pcf->can_id |= CAN_ERR_BUSOFF_NETDEV;

			break;

		case PCANFD_ERROR_PASSIVE:
			if (priv->can.state == CAN_STATE_ERROR_PASSIVE) {
				kfree_skb(skb);
				return 0;
			}

			priv->can.state = CAN_STATE_ERROR_PASSIVE;
			priv->can.can_stats.error_passive++;
			pcf->can_id |= CAN_ERR_CRTL;
			if (*prx_cnt > 127)
				pcf->data[1] |= CAN_ERR_CRTL_RX_PASSIVE;
			if (*ptx_cnt > 127)
				pcf->data[1] |= CAN_ERR_CRTL_TX_PASSIVE;
			break;

		case PCANFD_ERROR_WARNING:
			if (priv->can.state == CAN_STATE_ERROR_WARNING) {
				kfree_skb(skb);
				return 0;
			}

			priv->can.state = CAN_STATE_ERROR_WARNING;
			priv->can.can_stats.error_warning++;

			pcf->can_id |= CAN_ERR_CRTL;
			if (*prx_cnt > 96)
				pcf->data[1] |= CAN_ERR_CRTL_RX_WARNING;
			if (*ptx_cnt > 96)
				pcf->data[1] |= CAN_ERR_CRTL_TX_WARNING;
			break;

		case PCANFD_RX_OVERFLOW:
			if (pf->flags & PCANFD_ERROR_PROTOCOL) {
				pcf->can_id |= CAN_ERR_PROT;
				pcf->data[2] |= CAN_ERR_PROT_OVERLOAD;
			} else {
				pcf->can_id |= CAN_ERR_CRTL;
				pcf->data[1] |= CAN_ERR_CRTL_RX_OVERFLOW;

				stats->rx_over_errors++;
				stats->rx_errors++;
			}
			break;
		case PCANFD_TX_OVERFLOW:
			pcf->can_id |= CAN_ERR_CRTL;
			pcf->data[1] |= CAN_ERR_CRTL_TX_OVERFLOW;
			break;
		}
		break;

	case PCANFD_TYPE_CAN20_MSG:
		pcf->can_id = pf->id & CAN_ERR_MASK;
		if (pf->flags & PCANFD_MSG_RTR)
			pcf->can_id |= CAN_RTR_FLAG;
		if (pf->flags & PCANFD_MSG_EXT)
			pcf->can_id |= CAN_EFF_FLAG;

		ld = pf->data_len;

#ifdef CAN_CTRLMODE_CC_LEN8_DLC
		/* Note: can_frame_set_cc_len() sets
		 * cf->len = can_cc_dlc2len(dlc);
		 */
		can_frame_set_cc_len((struct can_frame *)pcf,
				     pcanfd_msg_read_dlc(pf),
				     priv->can.ctrlmode);
#endif
		memcpy(pcf->data, pf->data, ld);
		break;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	case PCANFD_TYPE_CANFD_MSG:
		if (pf->flags & PCANFD_MSG_ESI)
			pcf->flags |= CANFD_ESI;
		if (pf->flags & PCANFD_MSG_BRS)
			pcf->flags |= CANFD_BRS;

		pcf->can_id = pf->id & CAN_ERR_MASK;
		if (pf->flags & PCANFD_MSG_EXT)
			pcf->can_id |= CAN_EFF_FLAG;

		if (pf->flags & PCANFD_MSG_RTR) {
			pcf->can_id |= CAN_RTR_FLAG;

			/* prevent from counting nor copying data bytes */
			ld = 0;
		} else {
			ld = pf->data_len;
			if (ld > CANFD_MAX_DLEN)
				ld = CANFD_MAX_DLEN;

			memcpy(pcf->data, pf->data, ld);
		}
		break;
#endif
	default:
		pr_err(DEVICE_NAME ": %s() unsupported pcan msg type %d\n",
			__func__, pf->type);
		kfree_skb(skb);
		return -EINVAL;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
	pcf->can_dlc = ld;
#else
	pcf->len = ld;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	/* if the frame has been allocated using dev_alloc_skb(), MUST
	 * copy the local frame object into the skb data */
	memcpy(skb_put(skb, lf), &cf, lf);

#elif LINUX_VERSION_CODE == KERNEL_VERSION(4,1,0) \
   || LINUX_VERSION_CODE == KERNEL_VERSION(4,1,1)
	/* mandatory for Kernels 4.1.[01] */
	__net_timestamp(skb);
#endif

#ifdef DEBUG_RX
	pr_info(DEVICE_NAME ": id=%xh dlc=%u "
		"[%02x %02x %02x %02x %02x %02x %02x %02x] "
		"> %s rx queue\n",
		pcf->can_id,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
		pcf->len,
#else
		pcf->can_dlc,
#endif
		pcf->data[0], pcf->data[1], pcf->data[2], pcf->data[3],
		pcf->data[4], pcf->data[5], pcf->data[6], pcf->data[7],
		ndev->name);
#endif
	netif_rx(skb);

	/* "do not increase rx stats when genereting CAN rx error msg frame"
	 * (see linux-can)
	 */
	if (!(pcf->can_id & CAN_ERR_FLAG)) {
		stats->rx_packets++;
		stats->rx_bytes += ld;
	}

	return 1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
static int pcan_netdev_change_mtu(struct net_device *netdev, int new_mtu)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ":%s(new_mtu=%d) old_mtu=%d\n",
		__func__, new_mtu, netdev->mtu);
#endif
	/* Do not allow changing the MTU while running */
	if (netdev->flags & IFF_UP)
		return -EBUSY;

	/* allow change of MTU according to the CANFD ability of the device */
	if (new_mtu != CAN_MTU) {
		if (new_mtu != CANFD_MTU)
			return -EINVAL;
	}

	netdev->mtu = new_mtu;
	return 0;
}
#endif
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 28)
static const struct net_device_ops pcan_netdev_ops = {
	.ndo_open	= pcan_netdev_open,
	.ndo_start_xmit	= pcan_netdev_tx,
	.ndo_stop	= pcan_netdev_close,
	.ndo_get_stats	= pcan_netdev_get_stats,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
	.ndo_change_mtu = pcan_netdev_change_mtu,
#else
	.ndo_change_mtu = can_change_mtu,
#endif
#endif
};
#endif

#ifndef USES_ALLOC_CANDEV
/* AF_CAN netdevice: initialize data structure (should do what can_setup() in
 * drivers/net/can/dev.c does
 */
static void pcan_netdev_init(struct net_device *dev)
{
	dev->type = ARPHRD_CAN;
	dev->hard_header_len = 0;
#ifdef CAN_MTU
	dev->mtu = CAN_MTU;
#else
	dev->mtu = sizeof(struct can_frame);
#endif
	dev->addr_len = 0;
	dev->tx_queue_len = 10;

	dev->flags = IFF_NOARP;

	dev->features = NETIF_F_HW_CSUM;
}
#endif /* USES_ALLOC_CANDEV */

static void pcan_check_ifname(char *name)
{
	/* check wanted assigned 'name' against existing device names */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	if (__dev_get_by_name(name)) {
#else
	if (__dev_get_by_name(&init_net, name)) {
#endif
		pr_info(DEVICE_NAME ": assigned netdevice %s already exists\n",
			name);

		*name = 0; /* mark for auto assignment */
	}
}

/* AF_CAN netdevice: try to reassign netdev name according to user needs */
static void pcan_netdev_create_name(char *name, struct pcandev *pdev)
{
	int minor = pdev->nMinor;
	char *pa = assign;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME
		": %s(): minor=%d major=%d (usb major=%d) assign=\"%s\"\n",
		__func__, minor, pdev->nMajor, USB_MAJOR, assign);
#endif
	if (!assign) /* auto assignment */
		return;

	if (!strncmp(pa, "devid", 5)) {

		/* if device defines an alternate number, use it instead of
		 * its minor
		 */
		if (pdev->flags & PCAN_DEV_USES_ALT_NUM) {
			snprintf(name, IFNAMSIZ-1, CAN_NETDEV_NAME,
					(int )pdev->device_alt_num);
			pcan_check_ifname(name);
			if (*name)
				return;
		} else {
			pr_warn(DEVICE_NAME ": pcan%u: "
				"can't assign flashed device id to can name\n",
				minor);
		}

		pa += 5;
		if (*pa++ != ',')
			return;
	}

	if (!strncmp(pa, "peak", 4)) {

		/* assign=peak
		 * easy: /dev/pcanXX -> canXX
		 */
		snprintf(name, IFNAMSIZ-1, CAN_NETDEV_NAME, minor);

	} else {

		/* e.g. assign=pcan32:can1,pcan41:can2 */
		int peaknum, netnum;
		char *ptr = pa;

		while (ptr < (pa + strlen(pa))) {
			/* search first 'p' from pcanXX */
			ptr = strchr(ptr, 'p');
			if (!ptr)
				return; /* no match => quit */

			if (sscanf(ptr, DEVICE_NAME "%d:can%d", &peaknum,
								&netnum) != 2) {
				pr_info(DEVICE_NAME
					": bad parameter format in netdevice "
					"assignment.\n");
				return; /* bad parameter format => quit */
			}

			if (peaknum == minor) {
				snprintf(name, IFNAMSIZ-1, CAN_NETDEV_NAME,
									netnum);
				break; /* done */
			}
			ptr++; /* search for next 'p' */
		}
	}

	if (*name)
		pcan_check_ifname(name);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
static struct can_bittiming_const *
	pcan_netdev_convert_bt_caps(struct can_bittiming_const *pconst,
				    const struct pcanfd_bittiming_range *pcaps)
{
	if (!pcaps)
		return NULL;

	memset(pconst, '\0', sizeof(*pconst));

	strncpy(pconst->name, DEVICE_NAME, sizeof(pconst->name));
	pconst->tseg1_min = pcaps->tseg1_min;
	pconst->tseg1_max = pcaps->tseg1_max;
	pconst->tseg2_min = pcaps->tseg2_min;
	pconst->tseg2_max = pcaps->tseg2_max;
	pconst->sjw_max = pcaps->sjw_max;
	pconst->brp_min = pcaps->brp_min;
	pconst->brp_max = pcaps->brp_max;
	pconst->brp_inc = pcaps->brp_inc;

	return pconst;
}
#endif

static void pcan_netdev_do_restart(struct pcandev *pdev)
{
	pcan_set_tx_engine(pdev, TX_ENGINE_STOPPED);

	/* re-open the device itself */
	pcanfd_dev_open(pdev, &pdev->init_settings);
}

#if defined(USES_PCAN_RESTART) || defined(LINUX_CAN_RESTART_TIMER)
/*
 * SHOULD do what it is done by linux-can can_restart() function
 */
static void pcan_netdev_restart_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);

#ifdef LINUX_CAN_RESTART_TIMER
	struct pcandev *pdev = container_of(dwork, struct pcandev,
						restart_work);
	pcan_netdev_do_restart(pdev);
#else
	struct pcan_priv *priv = container_of(dwork, struct pcan_priv,
						can.restart_work);
	struct pcandev *pdev = priv->dev;
	struct sk_buff *skb;
	struct can_frame *cf;
	int err;

	/* copied from can_restart(): we have  no choice, can_restart()
	 * is not public.
	 */
	/* Since 6.5.12, BUG_ON() is replaced by a simple error */
	//BUG_ON(netif_carrier_ok(pdev->netdev));
	if (netif_carrier_ok(pdev->netdev))
		netdev_err(pdev->netdev, "Attempt to restart for bus-off recovery, but carrier is OK?\n");

	/* can_flush_echo_skb(dev.c) is static. Since our echo_skb_max is 0,
	 * this call is useless... */
	/* send restart message upstream */
	skb = alloc_can_err_skb(pdev->netdev, &cf);
	if (skb == NULL) {
		err = -ENOMEM;
		goto restart;
	}
	cf->can_id |= CAN_ERR_RESTARTED;

	netif_rx(skb);

	/* error packets are no more counted as rx packets in modern linux-can*/
restart:
	priv->can.can_stats.restarts++;

	/* Now restart the device */
	err = priv->can.do_set_mode(pdev->netdev, CAN_MODE_START);

#ifdef FIX_CAN_RESTART_CARRIER_ON
	/* Donot put carrier on here because do_set_mode() may not be atomic
	 * and BUS_OFF state can be reached (and so, can_bus_off() can be
	 * called) before going back here... The below netif_carrier_on() is
	 * the cause of restart mechanism malfunction when the delay is short
	 * (~ 10 ms)
	 */
#else
	netif_carrier_on(pdev->netdev);
#endif

	if (err)
		netdev_err(pdev->netdev, "Error %d during restart", err);
	else
		netdev_info(pdev->netdev, "restarted\n");
#endif
}
#endif

static int pcan_netdev_set_mode(struct net_device *ndev, enum can_mode mode)
{
	struct pcan_priv *priv = netdev_priv(ndev);
	struct pcandev *pdev = pcan_netdev_get_dev(priv);

	if (!pdev)
		return -ENODEV;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s CAN%u mode=%d)\n",
		__func__, pdev->adapter->name, pcan_idx(pdev)+1, mode);
#endif
	switch (mode) {

	case CAN_MODE_START:

#ifdef LINUX_CAN_RESTART_TIMER
		/* do restart in a safe context */
		schedule_delayed_work(&pdev->restart_work, 0);
#else
		/* we're running in a safe context */
		pcan_netdev_do_restart(pdev);
#endif

		break;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int pcan_netdev_get_berr_counter(const struct net_device *ndev,
					struct can_berr_counter *bec)
{
	struct pcan_priv *priv = netdev_priv(ndev);
	struct pcandev *pdev;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(ndev=%p priv=%p)\n",
		__func__, ndev, priv);
#endif

	if (!priv)
		return -ENODEV;

	pdev = pcan_netdev_get_dev(priv);
	if (!pdev)
		return -ENODEV;

	bec->txerr = pdev->tx_error_counter;
	bec->rxerr = pdev->rx_error_counter;

	return 0;
}

/* AF_CAN netdevice: register network device
 *
 * Note that this function might be called from interrupt context.
 */
int pcan_netdev_register(struct pcandev *pdev)
{
	struct net_device *ndev;
	struct pcan_priv *priv;
	char name[IFNAMSIZ] = {0};
	char *can_type = "CAN";

	pcan_netdev_create_name(name, pdev);

	if (!name[0]) {
		/* use the default: autoassignment */
		strncpy(name, CAN_NETDEV_NAME, IFNAMSIZ-1);
	}

#ifdef LINUX_26

#ifdef USES_ALLOC_CANDEV
	ndev = alloc_candev(sizeof(*priv), 0);
	if (!ndev) {
		pr_err(DEVICE_NAME ": out of memory\n");
		return 1;
	}

	strncpy(ndev->name, name, sizeof(ndev->name));

	priv = netdev_priv(ndev);

#else

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
	ndev = alloc_netdev(sizeof(*priv), name, pcan_netdev_init);
#else
	ndev = alloc_netdev(sizeof(*priv), name, NET_NAME_UNKNOWN,
			pcan_netdev_init);
#endif

	if (!ndev) {
		pr_err(DEVICE_NAME ": out of memory\n");
		return 1;
	}

	priv = netdev_priv(ndev);

	/* copied from alloc_candev() */
	priv->can.echo_skb_max = 0;
	priv->can.state = CAN_STATE_STOPPED;

#endif /* USES_ALLOC_CANDEV */

#ifdef LINUX_CAN_RESTART_TIMER
	init_timer(&priv->can.restart_timer);

#elif defined(USES_PCAN_RESTART)
	/* Since 4.8, can_bus_off(dev.c) schedules delayed work to run in a
	 * while. So the delayed work struct MUST be initialized here.
	 * Unfortunately, can_restart_work(dev.c) is not public, so we have to
	 * set our own delayed work callback.
	 *
	 * Unfortunately (again), can_restart_work(dev.c) calls
	 * can_restart(dev.c) which is not public too.
	 *
	 * So, our pcan_netdev_restart_work() will have to do the job as
	 * can_restart() does.
	 */
	INIT_DELAYED_WORK(&priv->can.restart_work, pcan_netdev_restart_work);
#endif

	priv->can.do_set_mode = pcan_netdev_set_mode;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 28)
	ndev->netdev_ops  = &pcan_netdev_ops;
#else
	ndev->open = pcan_netdev_open;
	ndev->stop = pcan_netdev_close;
	ndev->hard_start_xmit = pcan_netdev_tx;
	ndev->get_stats = pcan_netdev_get_stats;
#endif

	/* while our implementation doesn't put back sent frames into rx path,
	 * then we can't say we do any ECHO!
	 */
	if (defclk) {
		const struct pcanfd_available_clocks *pc = pdev->clocks_list;
		char *sep = NULL;

		/* if this pcan device is not part of the defclk
		 * string, use its default clock
		 */
		u32 clk = pdev->sysclock_Hz, tmp32;

		/* first, search if there is no global definition for the
		 * new default clock for all the CAN devices:
		 * "defclk=x"
		 */
		int l = strtounit(defclk, &tmp32, "kM" ), i;

#ifdef DEBUG_DEFCLK
		pr_info(DEVICE_NAME
			": does \"defclk=%s\" define a global value? "
			"l=%d tmp32=%u\n", defclk, l, tmp32);
#endif
		if (l < 0) {
			char pcan_name[16], *f;

			/* there is no global definition. 
			 * look now for a specific one:
			 * "defclk=pcanx:x,"
			 */
			int ln = snprintf(pcan_name, sizeof(name),
					  DEVICE_NAME "%u:", pdev->nMinor);

			f = strstr(defclk, pcan_name);
#ifdef DEBUG_DEFCLK
			pr_info(DEVICE_NAME
				": does \"defclk=%s\" contain \"%s\"? "
				"f=%p\n", defclk, pcan_name, f);
#endif
			if (f) {

				/* found it! */
				f += ln;

				/* replace any ',' by tmp EOL */
				sep = strchr(f, ',');
				if (sep)
					*sep = '\0';

				l = strtounit(f, &tmp32, "kM" );
#ifdef DEBUG_DEFCLK
				pr_info(DEVICE_NAME
					": does \"defclk=%s\" define a new clk "
					"value for pcan%u? l=%d tmp32=%u\n",
					defclk, pdev->nMinor, l, tmp32);
#endif
			}
		}

		/* if a clk value is defined for this dev and this new value
		 * is different from 0, it could be used
		 */
		if ((l > 0) && (tmp32))

			/* defclk=MHz value: check if exists */
			clk = tmp32;

		/* restore cmdline as it was */
		if (sep)
			*sep = ',';

		/* check now if clk is known by the current device */
		for (i = 0; i < pc->count; i++)
			if (clk == pc->list[i].clock_Hz)
				break;

		/* if yes and if it is not the default one, then rebuild
		 * bittiming settings
		 */
		if ((i < pc->count) && (clk != pdev->sysclock_Hz)) {
#ifdef DEBUG_DEFCLK
			pr_info(DEVICE_NAME ": " DEVICE_NAME "%u: "
				"default clock set to %uHz\n",
				pdev->nMinor, tmp32);
#endif

			/* Yep! Use it */
			pdev->sysclock_Hz = clk;

			/* convert "bitrate" into new bittiming according to
			 * new clk
			 */
			pcan_bitrate_to_bittiming( &pdev->init_settings.nominal,
				pdev->bittiming_caps,
				pdev->sysclock_Hz);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
			/* convert "dbitrate" into new bittiming according to
			 * new clk
			 */
			if (pdev->features & PCAN_DEV_FD_RDY) {

				pcan_bitrate_to_bittiming(
					&pdev->init_settings.data,
					pdev->dbittiming_caps,
					pdev->sysclock_Hz);
			}
#endif
		}
	}

	priv->can.clock.freq = pdev->sysclock_Hz;

	/* mainline drivers don't set any default bitrate */
	/* default supported ctrlmode for all PCAN interfaces */
	priv->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES |
#ifdef CAN_CTRLMODE_CC_LEN8_DLC
				       /* All PEAK-System devices support it */
				       CAN_CTRLMODE_CC_LEN8_DLC |
#endif
				       CAN_CTRLMODE_LISTENONLY;

	/* if the device does support it, then export that LOOPBACK is also
	 * supported
	 */
	if (pdev->features & PCAN_DEV_SLF_RDY)
		priv->can.ctrlmode_supported |= CAN_CTRLMODE_LOOPBACK;

	/* if the device does support it, then export that ONE_SHOT is also
	 * supported
	 */
	if (pdev->features & PCAN_DEV_SNG_RDY)
		priv->can.ctrlmode_supported |= CAN_CTRLMODE_ONE_SHOT;

	/* if the device does support it, then export that BERR_REPORTING is
	 * also supported
	 */
	if (pdev->features & PCAN_DEV_ERRCNT_RDY) {
		priv->can.ctrlmode_supported |= CAN_CTRLMODE_BERR_REPORTING;
		priv->can.do_get_berr_counter = pcan_netdev_get_berr_counter;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
	/* registering via register_candv() enables to play with bitrates too */
	priv->can.bittiming_const = pcan_netdev_convert_bt_caps(&priv->bt_const,
							pdev->bittiming_caps);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
	/* if an open_fd entry point is defined, then the device is CAN-FD */
	if (pdev->features & PCAN_DEV_FD_RDY) {

		priv->can.ctrlmode_supported |= CAN_CTRLMODE_FD;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 5)
		priv->can.ctrlmode_supported |= CAN_CTRLMODE_FD_NON_ISO;
#endif
		priv->can.data_bittiming_const =
			pcan_netdev_convert_bt_caps(&priv->dbt_const,
						    pdev->dbittiming_caps);
		/* mainline drivers don't set any default bitrate */
		can_type = "CAN-FD";
	}
#endif

	/* 3.6.0+: need to register as candev for CAN-FD support */
	if (register_candev(ndev)) {
#else
	if (register_netdev(ndev)) {
#endif
		pr_info(DEVICE_NAME ": Failed registering netdevice\n");
		free_netdev(ndev);
		return 1;
	}

#else /* LINUX_26 */

	ndev = (struct net_device*)pcan_malloc(sizeof(struct net_device),
					   GFP_KERNEL);
	if (!ndev) {
		pr_err(DEVICE_NAME ": out of memory\n");
		return 1;
	}

	memset(ndev, 0, sizeof(struct net_device));

	priv = pcan_malloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		pr_err(DEVICE_NAME ": out of memory\n");
		pcan_free(ndev);
		return 1;
	}

	memset(priv, 0, sizeof(struct pcan_priv));
	ndev->priv = priv;

	/* fill net_device structure */
	pcan_netdev_init(ndev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 28)
	ndev->netdev_ops  = &pcan_netdev_ops;
#else
	ndev->open = pcan_netdev_open;
	ndev->stop = pcan_netdev_close;
	ndev->hard_start_xmit = pcan_netdev_tx;
	ndev->get_stats = pcan_netdev_get_stats;
#endif

	strncpy(ndev->name, name, IFNAMSIZ-1); /* name the device */
	SET_MODULE_OWNER(ndev);

	if (register_netdev(ndev)) {
		pr_info(DEVICE_NAME ": Failed registering netdevice\n");
		pcan_free(priv);
		pcan_free(ndev);
		return 1;
	}

#endif /* LINUX_26 */

	/* Make references between pcan device and netdevice */
	priv->dev = pdev;
	pdev->netdev = ndev;

#ifdef LINUX_CAN_RESTART_TIMER
	/* init delayed work struct that handles restart out of any
	 * interrupt context
	 */
	INIT_DELAYED_WORK(&pdev->restart_work, pcan_netdev_restart_work);
#endif
	pr_info(DEVICE_NAME ": registered %s netdevice %s for %s hw (%d,%d)\n",
	       can_type, ndev->name, pdev->type, pdev->nMajor, pdev->nMinor);

	return 0;
}

/* AF_CAN netdevice: unregister network device */
int pcan_netdev_unregister(struct pcandev *pdev)
{
	struct net_device *ndev = pdev->netdev;

	if (!ndev)
		return 1;

	/* mark as unregistered to be sure not to loop here again */
	pdev->netdev = NULL;

#if defined(DEBUG_TRACE) || defined(DEBUG_OPEN)
	pr_info(DEVICE_NAME ": %s(%s)\n", __func__, ndev->name);
#endif
#ifdef LINUX_CAN_RESTART_TIMER
	cancel_delayed_work_sync(&pdev->restart_work);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
	unregister_netdev(ndev);
#else
	unregister_candev(ndev);
#endif

#ifndef LINUX_26
	{
		struct pcan_priv *priv = netdev_priv(ndev);
		if (priv)
			pcan_free(priv);
	}
#endif

	return 0;
}
