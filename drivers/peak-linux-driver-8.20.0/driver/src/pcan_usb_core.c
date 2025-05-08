/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_usb_core.c - the outer usb parts for all pcan usb interfaces support.
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
 *               Philipp Baer <philipp.baer@informatik.uni-ulm.de>
 *               Tom Heinrich
 *               John Privitera <JohnPrivitera@dciautomation.com>
 */
/* #define DEBUG */
/* #undef DEBUG */

#include "src/pcan_common.h"     /* must always be the 1st include */

#ifdef USB_SUPPORT

#include <linux/stddef.h>        /* NULL */
#include <linux/errno.h>
#include <linux/slab.h>          /* pcan_malloc() */

#include <linux/usb.h>
#include <linux/net.h>

#include "src/pcan_main.h"
#include "src/pcan_fops.h"
#include "src/pcan_usb_core.h"
#include "src/pcan_usb.h"
#include "src/pcan_usbpro.h"
#include "src/pcanfd_usb.h"
#include "src/pcanfd_core.h"
#include "src/pcan_filter.h"

#ifdef NETDEV_SUPPORT
#include "src/pcan_netdev.h"     /* for hotplug pcan_netdev_(un)register() */
#endif

#ifdef DEBUG_USB_LITE
#define DEBUG_TRACE
#define DEBUG_WRITE
#define DEBUG_READ
#define DEBUG_URB_ALLOC
#ifdef DEBUG_USB
#define DEBUG_DECODE
#endif
#endif

/* if defined, then USB devices acces is not locked at all */
//#define PCAN_USB_DONT_USE_LOCK

#ifndef PCAN_USB_DONT_USE_LOCK
/* if defined, then locking in read/write callback makes usage of _bh() lock
 * function instead of raw spinlocks
 */
//#define PCAN_USB_USES_LOCK_BH
#endif

/* - if defined, then writer is woken up each time a packet is sent to the USB
 *   device.
 * - if not defined, then writer is woken up only when no DATA have been read
 *   from Tx fifo (<= 8.7 behaviour).
 *
 * - when undefined, it's *very* hard to INTR a task looping on select(w) +
 *   pcanfd_send_msg() and tx fifo is never full
 * - when defined, it's immediate and tx fifo is always full when looping on
 *   CAN_Write()
 */
#define PCAN_USB_SIGNAL_ON_EACH_WRITE

/* if defined, the controller is put in bus off state (whatever its state is)
 * before going to bus on. This is normally useless and time consuming.
 */
//#define PCAN_USB_OPEN_FORCES_BUS_OFF

#define PCAN_USB_VENDOR_ID		0x0c72
#define PCAN_USB_PRODUCT_ID		0x000c
#define PCAN_USBPRO_PRODUCT_ID		0x000d

/* Defines the size of one USB message that can be received from the device
 * Driver allocates one buffer of n x read_buffer_size to optimize
 * consecutive reads
 */
#define PCAN_USB_READ_BUFFER_SIZE_OLD	64   /* used len for PCAN-USB rev < 6*/
#define PCAN_USB_READ_BUFFER_SIZE	1024 /* buffer for read URB data (IN) */
#define PCAN_USB_READ_PACKET_SIZE	64   /* fixed-length packets */
#define PCAN_USBPRO_READ_BUFFER_SIZE	2048 /* 4096 says Win driver */
#define PCAN_USBFD_READ_BUFFER_SIZE	4096

#define PCAN_USB_WRITE_BUFFER_SIZE_OLD	64   /* length for PCAN-USB rev < 6 */
#define PCAN_USB_WRITE_BUFFER_SIZE	256  /* says Win driver */
#define PCAN_USB_WRITE_PACKET_SIZE	64   /* always 64 (USB1 device) */
#define PCAN_USBPRO_WRITE_BUFFER_SIZE	512
#define PCAN_USBFD_WRITE_BUFFER_SIZE	512

/* (same but with Full-Speed USB controlers) */
#define PCAN_USB_WRITE_BUFFER_SIZE_FS		PCAN_USB_WRITE_BUFFER_SIZE_OLD
#define PCAN_USBPRO_WRITE_BUFFER_SIZE_FS	64
#define PCAN_USBFD_WRITE_BUFFER_SIZE_FS		256

#define MAX_CYCLES_TO_WAIT_FOR_RELEASE	100   /* max schedules before release */

/* wait this time in seconds at startup to get first messages */
#define STARTUP_WAIT_TIME		0.01

static struct usb_device_id pcan_usb_ids[] = {
	{ USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USB_PRODUCT_ID) },
	{ USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBPRO_PRODUCT_ID) },
	{ USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBFD_PRODUCT_ID) },
	{ USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBPROFD_PRODUCT_ID) },
	{ USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBCHIP_PRODUCT_ID) },
	{ USB_DEVICE(PCAN_USB_VENDOR_ID, PCAN_USBX6_PRODUCT_ID) },
	{ }	/* Terminating entry */
};

/* static database of last known fw versions */
#define PCAN_USB1_FW_LAST_VER		VER_NUM(5, 3, 0)
#define PCAN_USB2_FW_LAST_VER		VER_NUM(8, 6, 1)
#define PCAN_USBFD_FW_LAST_VER		VER_NUM(3, 4, 2)

MODULE_DEVICE_TABLE(usb, pcan_usb_ids);

static int usb_devices = 0;		/* the number of accepted usb_devices */

/* forward declaration for chardev pcan_usb_write_notitfy() */
static int pcan_usb_write(struct pcandev *dev, struct pcanusr *ctx);

#ifdef NETDEV_SUPPORT
static void pcan_usb_plugout_netdev(struct pcandev *dev)
{
	struct net_device *ndev = dev->netdev;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u): ndev=%p\n",
		__func__, pcan_idx(dev)+1, ndev);
#endif
	if (ndev) {
		netif_stop_queue(ndev);
		pcan_netdev_unregister(dev);
	}
}
#endif

#ifdef PCAN_USB_DONT_USE_LOCK
/* Test only! */
static void pcan_usb_lock_irqsave(struct pcandev *dev,
				  pcan_lock_irqsave_ctxt *pflags)
{
}

static void pcan_usb_unlock_irqrestore(struct pcandev *dev,
				       pcan_lock_irqsave_ctxt *pflags)
{
}

#elif defined(PCAN_USB_USES_LOCK_BH)
/*
 * default locking mechanism to protect user task access to resources shared
 * with ISR.
 * USB core use bottom half...
 */
static void pcan_usb_lock_irqsave(struct pcandev *dev,
				  pcan_lock_irqsave_ctxt *pflags)
{
	spin_lock_bh(&dev->isr_lock);
}

static void pcan_usb_unlock_irqrestore(struct pcandev *dev,
				       pcan_lock_irqsave_ctxt *pflags)
{
	spin_unlock_bh(&dev->isr_lock);
}
#endif

#ifndef LINUX_26
static void pcan_usb_write_notify(struct urb *urb, struct pt_regs *pregs)
#else
static void pcan_usb_write_notify(struct urb *urb)
#endif
{
	struct pcandev *dev = urb->context;
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);
	pcan_lock_irqsave_ctxt lck_ctx;
	int err = urb->status;

#if defined(DEBUG_TRACE) || defined(DEBUG_WRITE)
	pr_info(DEVICE_NAME ": %s() status=%d actual_length=%d\n",
		__func__, err, urb->actual_length);
#endif

	/* un-register outstanding urb */
	atomic_dec(&usb_if->w_active_urbs);

	/* don't count interrupts - count packets */
	dev->tx_irq_counter++;

	dev->lock_irq(dev, &lck_ctx);

	switch (err) {

	case 0:
		/* urb has been succesfully sent */

#ifdef PCAN_USB_SIGNAL_ON_EACH_WRITE
		/* urb has been succesfully sent: can notify writer to go on
		 * only if we aren't closing. In that case, signaling out_event
		 * should be done only whe tx fifo is empty
		 */
		if (!(dev->flags & PCAN_DEV_CLOSING)) {

			/* stop signalling when Tx fifo is almost full */
			if (pcan_fifo_ratio(&dev->tx_fifo) < txqhiwat)

				pcan_event_signal(&dev->out_event);
#ifdef NETDEV_SUPPORT
			netif_wake_queue(dev->netdev);
#endif
		}
#endif
		/* continue flushing tx fifo */
		err = pcan_usb_write(dev, NULL);
		if (!err) {
			break;
		}

		/* Note: signaling out_event in case of -ENODATA must be done
		 * to unlock a task releasing the channel that is waiting for
		 * tx fifo to empty.
		 * Since v8.8, -ENODATA means that no data have been sent to
		 * the USB device at all.
		 */
		if (err == -ENODATA) {

#ifdef PCAN_USB_SIGNAL_ON_EACH_WRITE
			if (!(dev->flags & PCAN_DEV_CLOSING))
				break;
#endif
			/* signal I'm ready to write again */
			pcan_event_signal(&dev->out_event);

#ifdef NETDEV_SUPPORT
			netif_wake_queue(dev->netdev);
#endif
			break;
		}

		/* otherwise, consider USB device fifo full:
		 * build the error frame and put it into Rx FIFO
		 */
		if (!(dev->wCANStatus & CAN_ERR_QXMTFULL)) {
			struct pcanfd_rxmsg ef;

			pcan_handle_error_ctrl(dev, &ef, PCANFD_TX_OVERFLOW);
			if (pcan_xxxdev_rx(dev, &ef) > 0) {
#ifndef NETDEV_SUPPORT
				pcan_event_signal(&dev->in_event);
#endif
			}
		}
		break;

	/* URB status when cable is disconnected */

	case -EPROTO:
	case -ECONNRESET:
	case -ESHUTDOWN:

		if (!usb_if->removing_driver)
			pr_err(DEVICE_NAME ": err %d when writing: "
				"is %s usb cable disconnected?\n",
				err, dev->adapter->name);

		dev->is_plugged = 0;

		/* unlock any waiting tasks */
		if (dev->nOpenPaths > 0) {

			pcan_event_signal(&dev->out_event);
#ifndef NETDEV_SUPPORT
			pcan_event_signal(&dev->in_event);
#endif
		}
		break;

	default:
		pr_err(DEVICE_NAME ": %s(%u): USB abnormal err %d\n",
				__func__, __LINE__, err);

		/* fall through */
		fallthrough;
	case -ENOENT:	/* urb killed */

		/* engine stopped */
		pcan_set_tx_engine(dev, TX_ENGINE_STOPPED);
		break;
	}

	dev->unlock_irq(dev, &lck_ctx);
}

#ifndef LINUX_26
static void pcan_usb_read_notify(struct urb *urb, struct pt_regs *pregs)
#else
static void pcan_usb_read_notify(struct urb *urb)
#endif
{
	struct pcan_usb_interface *usb_if = urb->context;
	const int read_buffer_len = urb->actual_length;
	u8 *read_buffer_addr;
	int read_buffer_size;
	struct pcandev *dev;
	int err, d;

#ifdef DEBUG_USB
	if (!urb->transfer_buffer) {
		pr_err(DEVICE_NAME
			": WTF?: got urb with NULL transfer_buffer!\n");
		return;
	}
#endif
	/* un-register outstanding urb */
	atomic_dec(&usb_if->r_active_urbs);

	/* do interleaving read, stop with first error */
	switch (urb->status) {

	case -EPROTO:
		/* EPROTO can be silently ignored when reading */
#ifdef DEBUG_USB
		pr_warn(DEVICE_NAME ": err %d silently ignored when reading\n",
			urb->status);
#endif
		/* fall through */
		fallthrough;
	case 0:
		break;

	case -ECONNRESET:	/* usb_unlink_urb() called */
	case -ENOENT:		/* urb killed */
	case -EPIPE:
#ifdef DEBUG_USB
		pr_info(DEVICE_NAME ": read data stream turned off (err %d)\n",
			urb->status);
#endif
		/* silently ignored, USB device disconnected */
		goto lbl_unlock;

	default:
		pr_err(DEVICE_NAME
			": unhandled read data stream turned off (err %d)\n",
			urb->status);

		/* fall through */
		fallthrough;

	/* error codes when USB device is hot unplugged */
	case -ESHUTDOWN:	/* the ep is being disabled */
	case -EILSEQ:

		/* USB device unplugged */
		goto lbl_unplug;
	}

	/* buffer interleave to increase speed */
	read_buffer_addr = urb->transfer_buffer;
	if (read_buffer_addr == usb_if->read_buffer_addr[0]) {
		FILL_BULK_URB(urb, usb_if->usb_dev,
				usb_rcvbulkpipe(usb_if->usb_dev,
						usb_if->pipe_read.ucNumber),
				usb_if->read_buffer_addr[1],
				usb_if->read_buffer_size,
				pcan_usb_read_notify, usb_if);
	} else {
		FILL_BULK_URB(urb, usb_if->usb_dev,
				usb_rcvbulkpipe(usb_if->usb_dev,
						usb_if->pipe_read.ucNumber),
				usb_if->read_buffer_addr[0],
				usb_if->read_buffer_size,
				pcan_usb_read_notify, usb_if);
	}

	/* start next urb */
	err = __usb_submit_urb(urb);
	switch (err) {
	case 0:
		atomic_inc(&usb_if->r_active_urbs);
		break;

	/* sometimes, unplugging X6 is seen here */
	case -ENODEV:
		goto lbl_unplug;

	default:
		pr_err(DEVICE_NAME ": %s() URB submit failure %d\n",
		       __func__, err);
	}

#ifdef DEBUG_READ
	if (read_buffer_len > 0)
		//if (usb_if_dev(usb_if, 0)->wType == HW_USB)
			pr_info(DEVICE_NAME ": %s > %d/%d bytes\n",
				usb_if->adapter->name, read_buffer_len,
				usb_if->read_buffer_size);
#endif

	/* decoding the received one, packet per packet */
	for (read_buffer_size = 0; read_buffer_size < read_buffer_len; ) {

		int l = usb_if->read_packet_size;
		if (l > (read_buffer_len - read_buffer_size))
			l = read_buffer_len - read_buffer_size;

		err = usb_if->device_msg_decode(usb_if, read_buffer_addr, l);
		if (err < 0)
			break;

		/* move to next "packet" (or to the end of the buffer) */
		read_buffer_addr += usb_if->read_packet_size;
		read_buffer_size += usb_if->read_packet_size;
	}

lbl_unlock:
	return;

lbl_unplug:
	if (!usb_if->removing_driver)
		pr_err(DEVICE_NAME ": err %d when reading: "
			"is %s usb cable disconnected?\n",
			urb->status, usb_if->adapter->name);

	/* "unplug" all devices of the same USB adapter */
	for (d = 0; d < usb_if->can_count; d++) {
		pcan_lock_irqsave_ctxt dev_lck_ctx;

		dev = usb_if_dev(usb_if, d);

		dev->lock_irq(dev, &dev_lck_ctx);

		/* seems that this is the most reasonable thing to do
		 * most of the times...
		 */
		dev->is_plugged = 0;

		/* unlock any waiting tasks */
		if (dev->nOpenPaths > 0) {

			pcan_event_signal(&dev->out_event);
#ifndef NETDEV_SUPPORT
			pcan_event_signal(&dev->in_event);
#endif
		}

		dev->unlock_irq(dev, &dev_lck_ctx);
	}

	return;
}

/* USB write functions */
static int pcan_usb_write(struct pcandev *dev, struct pcanusr *ctx)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);
	USB_PORT *u = &dev->port.usb;
	struct urb *urb = u->write_data;
	u8 *write_buffer_addr = urb->transfer_buffer;
	int write_buffer_size;
	int err = 0;

	/* don't do anything with non-existent hardware */
	if (!dev->is_plugged)
		return -ENODEV;

	/* encoding frames in the output buffer, packet per packet */
	for (write_buffer_size = 0;
		write_buffer_size < usb_if->write_buffer_size; ) {

		int l = usb_if->write_packet_size;
		if (l > (usb_if->write_buffer_size - write_buffer_size))
			l = usb_if->write_buffer_size - write_buffer_size;

		err = usb_if->device_ctrl_msg_encode(dev, write_buffer_addr,
						     &l);

		/* in case of error, add anything that might have been stored
		 * before the error and break.
		 */
		if (err < 0) {
			write_buffer_size += l;
			break;
		}

		/* go on next packet boundary (or at the end of the buffer) */
		write_buffer_size += usb_if->write_packet_size;
		write_buffer_addr += usb_if->write_packet_size;
	}

	if (write_buffer_size > 0) {

#ifdef DEBUG_WRITE
		//if (usb_if_dev(usb_if, 0)->wType == HW_USB)
			pr_info(DEVICE_NAME
				": %s CAN%u < %d/%d bytes (err %d)\n",
				dev->adapter->name, pcan_idx(dev)+1,
				write_buffer_size, usb_if->write_buffer_size,
				err);
#endif

		urb->transfer_buffer_length = write_buffer_size;

		/* remember the USB device is BUSY */
		pcan_set_tx_engine(dev, TX_ENGINE_STARTED);

		err = usb_submit_urb(urb, GFP_KERNEL);
		if (!err) {
			pcan_clear_status_bit(dev, CAN_ERR_QXMTFULL);

			atomic_inc(&usb_if->w_active_urbs);

			return 0;
		}

		pr_err(DEVICE_NAME
		       ": failed to submit %d bytes packet to %s CAN%u "
		       "(err %d)\n", write_buffer_size,
		       dev->adapter->name, pcan_idx(dev)+1, err);
	}

	/* we're here since nothing has been sent to the USB device. So,
	 * we won't never be notified again: indicate that the engine must
	 * be restarted.
	 */
	pcan_set_tx_engine(dev, TX_ENGINE_STOPPED);

	return err;
}

static int pcan_usb_init_urb(struct urb **urb)
{
	*urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!*urb) {
		pr_err(DEVICE_NAME ": failed to alloc urb\n");
		return -ENOMEM;
	}

	return 0;
}

/* Note: urb may be NULL */
static int pcan_usb_kill_urb(struct urb *urb)
{
	int err = 0;

#if defined(DEBUG_TRACE) || defined(DEBUG_URB_ALLOC)
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, urb);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
	/* note: urb may be NULL */
	usb_kill_urb(urb);
#else
	if (urb && (urb->status == -EINPROGRESS))
		err = usb_unlink_urb(urb);
#endif

	return err;
}

static int _pcan_usb_free_urb(struct urb **urb)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_URB_ALLOC)
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, urb);
#endif
	pcan_usb_kill_urb(*urb);

	/* note: *urb may be NULL */
	usb_free_urb(*urb);

	*urb = NULL;

	return 0;
}

#if defined(DEBUG_TRACE) || defined(DEBUG_URB_ALLOC)
static int __pcan_usb_free_urb(int l, struct urb **urb)
{
	pr_info(DEVICE_NAME ": %s line %d: %s(%p): *urb=%p\n",
		__FILE__, l, __func__, urb, (urb) ? *urb : NULL);

	return _pcan_usb_free_urb(urb);
}

#define pcan_usb_free_urb(p)	__pcan_usb_free_urb(__LINE__, p)
#else
#define pcan_usb_free_urb(p)	_pcan_usb_free_urb(p)
#endif

/* usb resource allocation
 * 
 * Note: DON'T use usb_if->dapter since it is NULL
 */
static int pcan_usb_alloc_resources(struct pcan_usb_interface *usb_if)
{
	const u16 devid = le16_to_cpu(usb_if->usb_dev->descriptor.idProduct);
	struct pcandev *dev;
	USB_PORT *u;
	int err = 0, c;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(devid=%d, can_count=%d)\n",
		__func__, devid, usb_if->can_count);
#endif

	/* make param URB */
	err = pcan_usb_init_urb(&usb_if->urb_cmd_sync);
	if (err)
		goto fail;

	/* default is: data are stored in a row in the usb buffer, that is,
	 * there is no packet segmentation in these buffers.
	 */
	usb_if->read_packet_size = 0;
	usb_if->write_packet_size = 0;

	/* allocate write buffer
	 * Check revision according to device id.
	 */
	switch (devid) {

	case PCAN_USBFD_PRODUCT_ID:
	case PCAN_USBCHIP_PRODUCT_ID:
	case PCAN_USBPROFD_PRODUCT_ID:
	case PCAN_USBX6_PRODUCT_ID:
		usb_if->read_buffer_size = PCAN_USBFD_READ_BUFFER_SIZE;
		usb_if->write_buffer_size =
			(usb_if->usb_dev->speed >= USB_SPEED_HIGH) ?
				PCAN_USBFD_WRITE_BUFFER_SIZE :
				PCAN_USBFD_WRITE_BUFFER_SIZE_FS;
		break;

	case PCAN_USBPRO_PRODUCT_ID:
		/* Rev 0x00 */
		usb_if->read_buffer_size = PCAN_USBPRO_READ_BUFFER_SIZE;

		/* Copied from Win32 Driver:
		 * DeviceContext->IsDeviceHighSpeed ? 512 : 64
		 */
		usb_if->write_buffer_size =
			(usb_if->usb_dev->speed >= USB_SPEED_HIGH) ?
				PCAN_USBPRO_WRITE_BUFFER_SIZE :
				PCAN_USBPRO_WRITE_BUFFER_SIZE_FS;
		break;

	case PCAN_USB_PRODUCT_ID:
		/* the PCAN-USB stores data into 64 bytes packets in the
		 * receive buffer
		 */
		usb_if->read_packet_size = PCAN_USB_READ_PACKET_SIZE;
		usb_if->write_packet_size = PCAN_USB_WRITE_PACKET_SIZE;

		if (usb_if->ucRevision >= 7) {
			usb_if->read_buffer_size = PCAN_USB_READ_BUFFER_SIZE;
			usb_if->write_buffer_size = PCAN_USB_WRITE_BUFFER_SIZE;
			break;
		}

		/* fall through */
		fallthrough;
	default:
		usb_if->read_buffer_size = PCAN_USB_READ_BUFFER_SIZE_OLD;
		usb_if->write_buffer_size = PCAN_USB_WRITE_BUFFER_SIZE_OLD;
		break;
	}

	/* if no "packet" then read the incoming buffer in a row */
	if (!usb_if->read_packet_size)
		usb_if->read_packet_size = usb_if->read_buffer_size;
	if (!usb_if->write_packet_size)
		usb_if->write_packet_size = usb_if->write_buffer_size;

	for (c = 0; c < usb_if->can_count; c++) {
		dev = usb_if_dev(usb_if, c);
		u = &dev->port.usb;

		//u->cout_bsize = usb_if->pipe_cmd_out.wDataSz;
		u->cout_bsize = 1024;

		/* make param URB */
		err = pcan_usb_init_urb(&u->urb_cmd_async);
		if (err)
			goto fail;
		err = pcan_usb_init_urb(&u->urb_cmd_sync);
		if (err)
			goto fail;

		u->write_buffer_addr = pcan_malloc(usb_if->write_buffer_size,
						   GFP_KERNEL);
		if (!u->write_buffer_addr) {
			err = -ENOMEM;
			goto fail;
		}

#ifdef DEBUG_USB
		pr_info(DEVICE_NAME
			": USB[devid=%u] CAN%u: %d bytes buffer allocated\n",
		        devid, c+1, usb_if->write_buffer_size);
#endif
		/* make write urb */
		err = pcan_usb_init_urb(&u->write_data);
		if (err)
			goto fail;

		FILL_BULK_URB(u->write_data, usb_if->usb_dev,
		              usb_sndbulkpipe(usb_if->usb_dev,
				              u->pipe_write.ucNumber),
		              u->write_buffer_addr, usb_if->write_buffer_size,
		              pcan_usb_write_notify, dev);

		/* Since Kernel 4.13, transfer data must be dma capable */
		if (u->cout_bsize) {
			u->cout_baddr = pcan_malloc(u->cout_bsize,
						    GFP_KERNEL|GFP_DMA);
			if (!u->cout_baddr) {
				err = -ENOMEM;
				goto fail;
			}
		} else {
			u->cout_baddr = NULL;
		}
	}

	/* allocate two read buffers for URB */
	usb_if->read_buffer_addr[0] = pcan_malloc(usb_if->read_buffer_size * 2,
						  GFP_KERNEL);
	if (!usb_if->read_buffer_addr[0]) {
		err = -ENOMEM;
		goto fail;
	}

#ifdef DEBUG_USB
	pr_info(DEVICE_NAME
		": %s() allocate %d buffers of %d bytes for reading\n",
	        __func__, 2, usb_if->read_buffer_size);
#endif

	usb_if->read_buffer_addr[1] = usb_if->read_buffer_addr[0]
	                            + usb_if->read_buffer_size;

	/* make read urb */
	err = pcan_usb_init_urb(&usb_if->read_data);
	if (!err)
		return 0;

fail:
	pr_err(DEVICE_NAME ": USB[devid=%u]: "
		"resource alloc failure (err %d)\n", devid, err);

	return err;
}

static void pcan_usb_free_dev_resources(struct pcandev *dev)
{
	USB_PORT *u = &dev->port.usb;

#ifdef DEBUG_TRACE
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);

	pr_info(DEVICE_NAME ": %s(%s CAN%u usb_if->index=%d)\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1,
		usb_if->index);
#endif

	pcan_usb_free_urb(&u->write_data);

	u->cout_baddr = pcan_free(u->cout_baddr);

	u->write_buffer_addr = pcan_free(u->write_buffer_addr);

	pcan_usb_free_urb(&u->urb_cmd_sync);
	pcan_usb_free_urb(&u->urb_cmd_async);
}

/* free resources allocated with pcan_usb_alloc_resources()
 * things are done so that NULL pointers (except usb_if) are allowed.
 */
static void pcan_usb_free_resources(struct pcan_usb_interface *usb_if)
{
	int c;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(usb_if->index=%d)\n",
		__func__, usb_if->index);
#endif

	pcan_usb_free_urb(&usb_if->read_data);

	usb_if->read_buffer_addr[0] = pcan_free(usb_if->read_buffer_addr[0]);

	for (c = 0; c < usb_if->can_count; c++) {
		struct pcandev *dev = usb_if_dev(usb_if, c);

		if (dev)
			pcan_usb_free_dev_resources(dev);
	}

	pcan_usb_free_urb(&usb_if->urb_cmd_sync);
}

static int pcan_usb_stop(struct pcandev *dev)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);
	USB_PORT *u = &dev->port.usb;
	int i, err = 0;

#if defined(DEBUG_TRACE) || defined(DEBUG_WRITE)
	pr_info(DEVICE_NAME ": %s(CAN%u), minor=%d: flags=%08xh\n",
		__func__, pcan_idx(dev)+1, dev->nMinor, dev->flags);
#endif
	if (dev->flags & PCAN_DEV_CLEANED)
		return 0;

	if (!(dev->flags & PCAN_DEV_OPENED))
		return 0;

	if (usb_if->device_ctrl_close)
		err = usb_if->device_ctrl_close(dev);

	if (usb_if->opened_count > 0)
		usb_if->opened_count--;

#ifdef DEBUG_WRITE
	pr_info(DEVICE_NAME ": have still %d+%d active URBs on interface\n",
		atomic_read(&usb_if->r_active_urbs),
		atomic_read(&usb_if->w_active_urbs));
#endif

	/* be sure that the outstanding write urbs have been acked: the PCAN-USB
	 * doesn't like to go to bus off when write URBs are killed before
	 * being ACKed...
	 */
	for (i = 0; i < 10; i++)
		if  (atomic_read(&usb_if->w_active_urbs)) {
#ifdef DEBUG_WRITE
			pr_info(DEVICE_NAME ": still %u write urbs\n",
				atomic_read(&usb_if->w_active_urbs));
#endif
			//pcan_msleep_interruptible(10);
			mdelay(10);
		} else
			break;

	/* unlink URBs for device/controller */
	pcan_usb_kill_urb(u->write_data);

	return usb_if->device_ctrl_set_bus_off(dev);
}

/* remove device resources 
 * Note: this version doesn't use wInitStep
 */
static int pcan_usb_cleanup(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u): wInitStep=%d\n",
		__func__, dev->nMinor, dev->wInitStep);
#endif

#ifdef NETDEV_SUPPORT
	pcan_usb_plugout_netdev(dev);
#endif
	pcan_remove_dev_from_list(dev);

	pcan_destroy_dev(dev);

	pcan_usb_free_dev_resources(dev);

	if (usb_devices)
		usb_devices--;

	return 0;
}

static void pcan_usb_free_irq(struct pcandev *dev, struct pcanusr *dev_priv)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u)\n", __func__, dev->nMinor);
#endif

	/* mis-used here for another purpose
	 * pcan_usb_free_irq() calls when the last path to device just closing
	 * and the device itself is already plugged out
	 */
	if (!dev->is_plugged)
		pcan_cleanup_dev(dev);
}

/* new API open path. */
static int pcan_usb_device_open_fd(struct pcandev *dev,
				   struct pcanfd_init *pfdi)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u): nOpenPaths=%d\n",
		__func__, dev->nMinor, dev->nOpenPaths);
#endif
#ifdef PCAN_USB_OPEN_FORCES_BUS_OFF
	/* otherwise, first action: turn CAN off */
	err = usb_if->device_ctrl_set_bus_off(dev);
	if (err)
		goto fail;
#endif
	/* init hardware specific parts */
	err = usb_if->device_ctrl_open_fd(dev, pfdi);
	if (err)
		goto fail;

	usb_if->opened_count++;

	/* last action: turn CAN on */
	err = usb_if->device_ctrl_set_bus_on(dev);
	if (err)
		goto fail;

#ifdef STARTUP_WAIT_TIME
	/* delay to get first messages read */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout((int)(STARTUP_WAIT_TIME * HZ + 0.9));
#endif

fail:
	return err;
}

/* CAN 2.0 a/b only open path.
 * btr0btr1 is the SJA1000 representation of the bitrate, whatever the device
 * clock is. It is device responsibility to convert it.
 */
static int pcan_usb_device_open(struct pcandev *dev, uint16_t btr0btr1,
                                u8 bExtended, u8 bListenOnly)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u): nOpenPaths=%d\n",
		__func__, dev->nMinor, dev->nOpenPaths);
#endif
#ifdef PCAN_USB_OPEN_FORCES_BUS_OFF
	/* otherwise, first action: turn CAN off */
	err = usb_if->device_ctrl_set_bus_off(dev);
	if (err)
		goto fail;
#endif
	/* init hardware specific parts */
	err = usb_if->device_ctrl_open(dev, btr0btr1, bExtended, bListenOnly);
	if (err)
		goto fail;

	usb_if->opened_count++;

	/* last action: turn CAN on */
	err = usb_if->device_ctrl_set_bus_on(dev);
	if (err)
		goto fail;

#ifdef STARTUP_WAIT_TIME
	/* delay to get first messages read */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout((int)(STARTUP_WAIT_TIME * HZ + 0.9));
#endif

fail:
	return err;
}

static void pcan_usb_device_release(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u): nOpenPaths=%d\n",
		__func__, dev->nMinor, dev->nOpenPaths);
#endif

	/* theoretically, waiting is useless:
	 * ifdef PCAN_USB_SIGNAL_ON_EACH_WRITE, out_event is signaled on
	 * -ENODATA only when the device is being closed
	 * ifndef PCAN_USB_SIGNAL_ON_EACH_WRITE, out_event is signaled on
	 * -ENODATA only.
	 * Thus, when we arrive here, there is no write urb on the flight
	 * anymore.
	 */
	pcan_usb_stop(dev);
}

/* get or set special device related parameters */
static int pcan_usb_device_params(struct pcandev *dev, TPEXTRAPARAMS *params)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);
	USB_PORT *u = &dev->port.usb;
	int err = -ENOTSUPP;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u): sub_fct=%d\n",
		__func__, dev->nMinor, params->nSubFunction);
#endif

	switch (params->nSubFunction & 0xff) {
	case SF_GET_SERIALNUMBER:
		err = usb_if->device_get_snr(usb_if,
						&params->func.dwSerialNumber);
		break;
	case SF_GET_HCDEVICENO:
		/* can cast to u32 * since "func" is an union with
		 * dwSerialNumber
		 */
		err = usb_if->device_ctrl_get_dnr(dev,
				//(u32 *)&params->func.ucHCDeviceNo);
				&params->func.dwSerialNumber);
		break;
	case SF_SET_HCDEVICENO:
		/*
		 * err = usb_if->device_ctrl_set_dnr(dev,
		 *			params->func.ucHCDeviceNo);
		 */
		err = usb_if->device_ctrl_set_dnr(dev,
						params->func.dwSerialNumber);
		/* Should update dev object cache with new value
		 * (see /dev/pcan display)
		 */
		if (!err) {
			u->ucHardcodedDevNr = params->func.ucHCDeviceNo;

			/* why not caching full 32b value in device_alt_num? */
			dev->device_alt_num = params->func.dwSerialNumber;
		}
		break;

	case SF_SET_DEVDATA:
		if (usb_if->device_set_devdata)
			err = usb_if->device_set_devdata(dev,
							params->nSubFunction,
							params->func.ucDevData);
		break;

	case SF_GET_DEVDATA:
		if (usb_if->device_get_devdata)
			err = usb_if->device_get_devdata(dev,
							params->nSubFunction,
							params->func.ucDevData);
		break;

	default:
		pr_warn(DEVICE_NAME ": %s CAN%u: Unknown sub-function %d!\n",
			dev->adapter->name, pcan_idx(dev)+1,
			params->nSubFunction);

		return -EINVAL;
	}

	return err;
}

static int pcan_usb_get_devid(struct pcandev *dev,
				struct pcanfd_option *opt, void *c)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);
	u32 dev_id;

	int err = usb_if->device_ctrl_get_dnr(dev, &dev_id);
	if (err) {
		pr_err(DEVICE_NAME
			": %s() err %d getting dev number from %s CAN%d\n",
			__func__, err, dev->adapter->name, pcan_idx(dev)+1);
		return err;
	}

	opt->size = sizeof(dev_id);
	if (pcan_copy_to_user(opt->value, &dev_id, opt->size, c)) {
		pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int pcan_usb_set_devid(struct pcandev *dev,
				struct pcanfd_option *opt, void *c)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);
	USB_PORT *u = &dev->port.usb;
	u32 dev_id;

	int err = pcan_copy_from_user(&dev_id, opt->value, sizeof(u32), c);
	if (err) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	err = usb_if->device_ctrl_set_dnr(dev, dev_id);
	if (err) {
		pr_err(DEVICE_NAME
			": %s() err %d setting dev number to %s CAN%d\n",
			__func__, err, dev->adapter->name, pcan_idx(dev)+1);
		return err;
	}

	/* Should update dev object cache with new value
	 * (see /dev/pcan display)
	 */
	u->ucHardcodedDevNr = (u8 )dev_id;
	dev->device_alt_num = dev_id;

	return 0;
}

static int pcan_usb_get_mass_storage_mode(struct pcandev *dev,
					  struct pcanfd_option *opt, void *c)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);

	/* reading mass_storage_mode option always returns 0 */
	const u32 v = 0;

	/* check whether the fw is able to set mass storage mode */
	if (!usb_if->device_set_mass_storage_mode)
		return -EOPNOTSUPP;

	if (opt->name != PCANFD_OPT_MASS_STORAGE_MODE)
		memcpy(opt->value, &v, sizeof(v));
	else {
		int err = pcan_copy_to_user(opt->value, &v, sizeof(v), c);
		if (err) {
			pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			       __func__);
			return -EFAULT;
		}
	}

	opt->size = sizeof(v);
	return 0;
};

static int pcan_usb_set_mass_storage_mode(struct pcandev *dev,
					  struct pcanfd_option *opt, void *c)
{
	struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s usb_if=%p)\n", __func__,
		usb_if->adapter->name, usb_if);
#endif

	/* check whether the fw is able to set mass storage mode */
	if (!usb_if->device_set_mass_storage_mode) {
		pr_warn(DEVICE_NAME
			": %s: mass storage mode not supported\n",
			usb_if->adapter->name);

		return -EOPNOTSUPP;
	}

	/* tip: if opt == NULL then this callback is called to check whether
	 * it's possible or not to set the mass storage mode
	 */
	if (opt && opt->value) {
		int err;
		u32 v;

		/* Trick: if opt->name != PCANFD_OPT_MASS_STORAGE_MODE then
		 * this is an internal call: value is not a userspace
		 * address.
		 */
		if (opt->name != PCANFD_OPT_MASS_STORAGE_MODE)
			memcpy(&v, opt->value, sizeof(u32));
		else {
			err = pcan_copy_from_user(&v, opt->value,
						  sizeof(u32), c);
			if (err) {
				pr_err(DEVICE_NAME
				       ": %s(): copy_from_user() failure\n",
				       __func__);
				return -EFAULT;
			}
		}

		if (v) {

#ifdef GLOBAL_ROOT_UID
			/* Note: GLOBAL_ROOT_UID defined since kernel 3.5:
			 * check whether current task is allowed to do that
			 */
			if (!uid_eq(current_euid(), GLOBAL_ROOT_UID)) {
				pr_warn(DEVICE_NAME
					": %s: switching in MSD requires root "
					"privileges\n",
					usb_if->adapter->name);

				return -EPERM;
			}
#endif
			/* set it */
			err = usb_if->device_set_mass_storage_mode(usb_if);
			if (err) {
				pr_err(DEVICE_NAME
					": %s: setting mass storage mode "
					"failed (err %d)\n",
					usb_if->adapter->name, err);
				return err;
			}
		}
	}

	return 0;
}

static int pcan_usb_get_flash_led(struct pcandev *dev,
				  struct pcanfd_option *opt, void *c)
{
	/* reading led option always returns 0 */
	const u32 v = 0;

	/* check whether the device can be identified */
	if (!dev->device_identify) {
		pr_warn(DEVICE_NAME ": %s can't be identified\n",
			dev->adapter->name);

		return -EOPNOTSUPP;
	}

	if (opt->name != PCANFD_OPT_MASS_STORAGE_MODE)
		memcpy(opt->value, &v, sizeof(v));
	else {
		int err = pcan_copy_to_user(opt->value, &v, sizeof(v), c);
		if (err) {
			pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			       __func__);
			return -EFAULT;
		}
	}

	opt->size = sizeof(v);
	return 0;
};

static int pcan_usb_set_flash_led(struct pcandev *dev,
				  struct pcanfd_option *opt, void *c)
{
	int err;
	u32 v;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s #%u)\n", __func__,
		dev->adapter->name, pcan_idx(dev)+1);
#endif

	/* check whether the device can be identified */
	if (!dev->device_identify) {
		pr_warn(DEVICE_NAME ": %s can't be identified\n",
			dev->adapter->name);

		return -EOPNOTSUPP;
	}

	/* Trick: if opt->name != PCANFD_OPT_FLASH_LED then
	 * this is an internal call: value is not a userspace address.
	 */
	if (opt->name != PCANFD_OPT_FLASH_LED)
		memcpy(&v, opt->value, sizeof(u32));
	else {
		err = pcan_copy_from_user(&v, opt->value,
					  sizeof(u32), c);
		if (err) {
			pr_err(DEVICE_NAME
			       ": %s(): copy_from_user() failure\n",
			       __func__);
			return -EFAULT;
		}
	}

	return dev->device_identify(dev, v);
}

/* USB device specific options */
static struct pcanfd_options pcan_usb_opts[PCANFD_OPT_MAX] =
{
	[PCANFD_OPT_DEVICE_ID] = {
		.req_size = sizeof(u32),
		.get = pcan_usb_get_devid,
		.set = pcan_usb_set_devid,
	},
	[PCANFD_OPT_MASS_STORAGE_MODE] = {
		.req_size = sizeof(u32),
		.get = pcan_usb_get_mass_storage_mode,
		.set = pcan_usb_set_mass_storage_mode,
	},
	[PCANFD_OPT_FLASH_LED] = {
		.req_size = sizeof(u32),
		.get = pcan_usb_get_flash_led,
		.set = pcan_usb_set_flash_led,
	},
};

static struct pcanfd_options pcan_usb_chip_opts[PCANFD_OPT_MAX] =
{
	[PCANFD_IO_DIGITAL_CFG] = {
		.req_size = sizeof(u32),
		.get = pcan_usb_chip_get_dig_cfg,
		.set = pcan_usb_chip_set_dig_cfg,
	},
	[PCANFD_IO_DIGITAL_VAL] = {
		.req_size = sizeof(u32),
		.get = pcan_usb_chip_get_dig_val,
		.set = pcan_usb_chip_set_dig_val,
	},
	[PCANFD_IO_DIGITAL_SET] = {
		.req_size = sizeof(u32),
		.set = pcan_usb_chip_set_dig_bit,
	},
	[PCANFD_IO_DIGITAL_CLR] = {
		.req_size = sizeof(u32),
		.set = pcan_usb_chip_clr_dig_bit,
	},
	[PCANFD_IO_ANALOG_VAL] = {
		.req_size = sizeof(u32),
		.get = pcan_usb_chip_get_ana_val,
	},
};

static int pcan_usb_create_dev(struct pcan_usb_interface *usb_if,
                               int ctrl_index)
{
	struct pcandev *dev = usb_if_dev(usb_if, ctrl_index);
	struct usb_device *usb_dev = usb_if->usb_dev;
	const struct pcanfd_options *device_opts = pcan_usb_opts;
	USB_PORT *u = &dev->port.usb;
	int def_features = PCAN_DEV_SJA1000_RDY|PCAN_DEV_ECHO_RDY;
	int err, retry;
	const u32 hw_ver = VER_NUM(usb_if->hw_ver.major,
				   usb_if->hw_ver.minor,
				   usb_if->hw_ver.subminor);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s ctrl_index=%u]\n",
	        __func__, usb_if->adapter->name, ctrl_index);
#endif

	switch (le16_to_cpu(usb_dev->descriptor.idProduct)) {
	case PCAN_USBCHIP_PRODUCT_ID:

		/* PCAN-USB Chip specific options with FW >= 3.3.0 */
		if (hw_ver >= VER_NUM(3, 3, 0))
			device_opts = pcan_usb_chip_opts;

		/* fall through */
		fallthrough;
	case PCAN_USBFD_PRODUCT_ID:
	case PCAN_USBPROFD_PRODUCT_ID:
	case PCAN_USBX6_PRODUCT_ID:
		ucan_soft_init(dev, &usb_if->hw_ver);

		/* inform in case running fw is not the last one */
		if (hw_ver < PCAN_USBFD_FW_LAST_VER)
			dev->features |= PCAN_DEV_NEW_FW_AV;


		/* these options are available from FW >= 3.4.0 */
		if (hw_ver >= VER_NUM(3, 4, 0))
			dev->features |= PCAN_DEV_TS_SOF_RDY |
					 PCAN_DEV_SELF_ACK_RDY |
					 PCAN_DEV_BRS_IGN_RDY;
		break;

	case PCAN_USBPRO_PRODUCT_ID:

		/* init structure elements to defaults */
		pcan_soft_init_ex(dev, 
			(const struct pcanfd_available_clocks *)&sja2010_clocks,
			&sja2010_caps,
			PCAN_DEV_BUSLOAD_RDY|def_features);
		break;

	case PCAN_USB_PRODUCT_ID:
	default:
		/* init structure elements to defaults */

		/* remove SRR and SNG when using to old devices */
		if (usb_if->ucRevision < 41) {
			def_features &= ~(PCAN_DEV_SLF_RDY |
					  PCAN_DEV_ECHO_RDY |
					  PCAN_DEV_SNG_RDY);
		}

		pcan_soft_init_ex(dev,
			(const struct pcanfd_available_clocks *)&sja1000_clocks,
			&sja1000_capabilities,
			def_features);

		/* inform in case running fw is not the last one:
		 * gen2:
		 */
		if (hw_ver >= VER_NUM(8, 0, 0)) {
			if (hw_ver < PCAN_USB2_FW_LAST_VER)
				dev->features |= PCAN_DEV_NEW_FW_AV;
		/* gen 1: */
		} else if (hw_ver < PCAN_USB1_FW_LAST_VER)
			dev->features |= PCAN_DEV_NEW_FW_AV;

		/* Device Id. is a single-octet value in these old adapters,
		 * thus, the 'default' value is 0xff (instead of 0xffffffff)
		 */
		dev->device_alt_num = 0xff;
		break;
	}

#if defined(PCAN_USB_USES_LOCK_BH) || defined(PCAN_USB_DONT_USE_LOCK)
	dev->lock_irq = pcan_usb_lock_irqsave;
	dev->unlock_irq = pcan_usb_unlock_irqrestore;
#endif

	if (usb_if->device_set_devdata)
		dev->features |= PCAN_DEV_DEVDATA;

	/* device_init() has been called: can do that! */
	pcan_set_dev_adapter(dev, usb_if->adapter);

	/* since USB-X6 might run with 3x different FW, then the device version
	 * must point to the usb_if hw version instead of the adapter one
	 */
	dev->hw_ver = &usb_if->hw_ver;

	/* override with USB devices specific options callbacks */
	dev->option = device_opts;

	/* override standard device access functions:
	 * if device is CANFD capable, set the CANFD open function. Otherwise,
	 * set the deafult CAN 2.0 open function
	 */
	if (usb_if->device_ctrl_open_fd)
		dev->device_open_fd = pcan_usb_device_open_fd;
	else
		dev->device_open = pcan_usb_device_open;

	/* remember that the device can switch into Mass Storage Device mode */
	if (usb_if->device_set_mass_storage_mode)
		dev->features |= PCAN_DEV_MSD_RDY;

	dev->device_write = pcan_usb_write;
	dev->device_release = pcan_usb_device_release;

	/* set this before any instructions, fill struct pcandev, part 1 */
	dev->cleanup = pcan_usb_cleanup;
	dev->free_irq = pcan_usb_free_irq;
	dev->filter = pcan_create_filter_chain();
	dev->device_params = pcan_usb_device_params;

#ifdef DEBUG_USB
	pr_info(DEVICE_NAME ": usb hardware revision = %d\n",
		usb_if->ucRevision);
#endif

	/* add this device to the list */
	pcan_add_dev_in_list(dev);
	usb_devices++;

	/* get serial number as soon as possible */
	usb_if->device_get_snr(usb_if, &usb_if->dwSerialNumber);

	/* Get device number early too (sometimes, need to retry...) */
	for (retry = 3; retry; retry--) {
		u32 device_nr32;
		err = usb_if->device_ctrl_get_dnr(dev, &device_nr32);
		if (!err) {

			u->ucHardcodedDevNr = (u8 )device_nr32;
			dev->device_alt_num = device_nr32;
#ifdef DEBUG_USB
			pr_info(DEVICE_NAME ": %s(): CAN%u devid=%xh (%u)\n",
				__func__, ctrl_index, device_nr32, device_nr32);
#endif
			break;
		}
	}

	dev->nMajor = pcan_drv.nMajor;

	/* dev->adapter must not be NULL before calling this */
	dev->nMinor = pcan_find_free_minors(dev, PCAN_USB_MINOR_BASE,
					    PCAN_USB_MINOR_END);
	if (dev->nMinor < 0) {
		err = dev->nMinor;
		pr_err(DEVICE_NAME ": not enough minors\n");
		goto reject;
	}

	/* pcan_find_free_minors() returns 1st minor of a range large enough
	 * to store all consecutive minors for the same adapter. This enables
	 * to register devices whatever the order their interface is enumerated.
	 *
	 * Note: usb_if->index is always 0 except for the PCAN-USB X6
	 */
	dev->nMinor += usb_if->index * usb_if->can_count + ctrl_index;

	/* do register pcan dev under sysfs */
	pcan_sysfs_dev_node_create_ex(dev, &usb_if->usb_intf->dev);

	/* set device in inactive state to prevent violating the bus.
	 * This is also important for the FW to know the channel index
	 */
	usb_if->device_ctrl_set_bus_off(dev);

	/* Call hardware supplied own callback to do some private init */
	if (usb_if->device_ctrl_init) {
		err = usb_if->device_ctrl_init(dev);
		if (err) {
			pr_err(DEVICE_NAME
				": %s CAN%u initialization not complete\n",
				usb_if->adapter->name, ctrl_index+1);
			goto reject;
		}
	}

#ifdef NETDEV_SUPPORT
	pcan_netdev_register(dev);
#endif

	if (dev->flags & PCAN_DEV_USES_ALT_NUM)
		pr_info(DEVICE_NAME
			": - usb%sdevice minor %d number %d found\n",
			(dev->features & PCAN_DEV_FD_RDY) ? " fd " : " ",
			dev->nMinor, dev->device_alt_num);

	else
		pr_info(DEVICE_NAME ": - usb%sdevice minor %d found\n",
			(dev->features & PCAN_DEV_FD_RDY) ? " fd " : " ",
			dev->nMinor);

	return 0;

reject:
	pcan_cleanup_dev(dev);

	pr_err(DEVICE_NAME
		": failed to register %s CAN%u as a new USB CAN channel "
		"err %d\n",
		dev->adapter->name, ctrl_index+1, err);

	return err;
}

static void pcan_usb_free_interface(struct pcan_usb_interface *usb_if)
{
	int i;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(usb_if->index=%d)\n",
		__func__, usb_if->index);
#endif

	for (i = 0; i < usb_if->can_count; i++) {
		struct pcandev *dev = usb_if_dev(usb_if, i);

		/* the device has been unplugged: it will be deleted
		 * later...
		 */
		if (!dev)
			continue;

		/* if device was in global devices list, then it has
		 * been initialized, then it can be destroyed
		 */
		if (pcan_remove_dev_from_list(dev))
			pcan_destroy_dev(dev);

		pcan_free_dev(dev);
	}

	pcan_free(usb_if);
}

static int pcan_usb_plugin(struct usb_interface *interface,
                           const struct usb_device_id *id)
{
	struct usb_device *usb_dev = interface_to_usbdev(interface);
	struct usb_host_interface *iface_desc = &interface->altsetting[0];
	struct pcan_usb_interface *usb_if;
	int (*device_init)(struct pcan_usb_interface *);
	int err, i, dev_ctrl_count, sizeof_if;
	char *dev_type_str;
	u16 dev_type;

	/* Ignore any interface != #0: this is not a CAN one */
	if (iface_desc->desc.bInterfaceNumber)
		return -ENODEV;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(0x%04x, 0x%04x, 0x%04x): bNumEndpoints=%d\n",
		__func__,
		usb_dev->descriptor.idVendor, usb_dev->descriptor.idProduct,
		usb_dev->descriptor.bcdDevice, iface_desc->desc.bNumEndpoints);
#endif

	/* only 1 interface is supported
	 * Note: HW_USB_PRO: interface#0 for CAN, #1 for LIN
	 */
	err = usb_set_interface(usb_dev, 0, 0);
	if (err < 0) {
		pr_err(DEVICE_NAME
		       ": usb_set_interface(devid=%04xh) failed! (err %d)\n",
		       usb_dev->descriptor.idProduct, err);
		return err;
	}

	/* Now, according to device id, create as many device as CAN
	 * controllers
	 */
	switch (le16_to_cpu(usb_dev->descriptor.idProduct)) {
	case PCAN_USBCHIP_PRODUCT_ID:
	case PCAN_USBFD_PRODUCT_ID:
		dev_type_str = "usbfd";
		dev_type = HW_USB_FD;
		dev_ctrl_count = 1;
		device_init = pcan_usbfd_init;
		break;
	case PCAN_USBPROFD_PRODUCT_ID:
		dev_type_str = "usbfd";
		dev_type = HW_USB_PRO_FD;
		dev_ctrl_count = 2;
		device_init = pcan_usbfd_init;
		break;
	case PCAN_USBX6_PRODUCT_ID:
		dev_type_str = "usbfd";
		dev_type = HW_USB_X6;
		dev_ctrl_count = 2;
		device_init = pcan_usbfd_init;
		break;
	case PCAN_USBPRO_PRODUCT_ID:
		dev_type_str = "usb";
		dev_type = HW_USB_PRO;
		dev_ctrl_count = 2;
		device_init = pcan_usbpro_init;
		break;
	case PCAN_USB_PRODUCT_ID:
	default:
		dev_type_str = "usb";
		dev_type = HW_USB;
		dev_ctrl_count = 1;
		device_init = pcan_usb_init;
		break;
	}

#ifdef DEBUG_USB
	pr_info(DEVICE_NAME
		": new%susb adapter with %u CAN controller(s) detected\n",
		(usb_dev->speed == USB_SPEED_HIGH) ? " high speed " : " ",
		dev_ctrl_count);
#endif

	/* create our interface object for the USB device */
	sizeof_if = sizeof(struct pcan_usb_interface) + dev_ctrl_count *
				sizeof(struct pcandev *);

	usb_if = pcan_malloc(sizeof_if, GFP_KERNEL);
	if (!usb_if) {
		pr_err(DEVICE_NAME
		       ": alloc usb interface failed!\n");
		return -ENOMEM;
	}

	memset(usb_if, '\0', sizeof_if);

	usb_if->mcu = 0xffff;

	/* set default EPs */
	usb_if->pipe_cmd_out.ucNumber = 0x01;
	usb_if->pipe_cmd_in.ucNumber = 0x01 | USB_DIR_IN;
	usb_if->pipe_read.ucNumber = 0x02 | USB_DIR_IN;

	for (i = 0; i < dev_ctrl_count; i++ ) {
		struct pcandev *dev = pcan_alloc_dev(dev_type_str, dev_type, i);
		if (!dev) {
			pcan_usb_free_interface(usb_if);

			pr_err(DEVICE_NAME ": alloc pcandev #%u failed!\n", i);
			return -ENOMEM;
		}

		/* do the doubly-linkage asap (before device_init()) */
		dev->port.usb.usb_if = usb_if;
		usb_if_dev(usb_if, i) = dev;

		/* CANx data out default EP: CAN0=0x02 CAN1=0x03 */
		dev->port.usb.pipe_write.ucNumber = 0x02 + i;

		usb_if->can_count++;
	}

	pcan_init_version(&usb_if->hw_ver);

	/* store pointer to kernel supplied usb_dev */
	usb_if->usb_dev = usb_dev;
	usb_if->usb_intf = interface;

	/* these devices may define their own EP values: get them now */
	if (device_init == pcan_usbfd_init)
		err = pcan_usbfd_init_ep(usb_if);

	/* preset active URB counter */
	atomic_set(&usb_if->r_active_urbs, 0);
	atomic_set(&usb_if->w_active_urbs, 0);

	/* get endpoint addresses (numbers) and associated max data length
	 * (only from setting 0)
	 */

/*
 * USB-Pro
 *      Function   Interface   Endpoints            DeviceId
 *      ---------  ---------   -----------------------------------------
 *      Control                0
 *      CAN        0                                "CAN-Device",
 *                                                  USB\VID_0c72&PID_000d&MI_00
 *                             1=Command,           bidi for both CAN_Ctrller
 *                             2=CAN-Controller 0,  rcv (IN) both CAN-Ctrller,
 *                                                  transmit (OUT) CAN-Ctrl#0,
 *                             3=CAN-Controller 1   transmit (OUT) CAN-Ctrl#1
 *      LIN        1                                "LIN-Device",
 *                                                  USB\VID_0c72&PID_000d&MI_01
 *                             4=Command,
 *                             5=Controller 0,
 *                             6=Controller 1
 */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		PCAN_ENDPOINT *pipe_addr = NULL;
		struct usb_endpoint_descriptor *ep =
					&iface_desc->endpoint[i].desc;

		if (ep->bEndpointAddress == usb_if->pipe_cmd_out.ucNumber)
			pipe_addr = &usb_if->pipe_cmd_out;

		else if (ep->bEndpointAddress == usb_if->pipe_cmd_in.ucNumber)
			pipe_addr = &usb_if->pipe_cmd_in;

		else if (ep->bEndpointAddress == usb_if->pipe_read.ucNumber)
			pipe_addr = &usb_if->pipe_read;

		else if (ep->bEndpointAddress ==
			usb_if_dev(usb_if, 0)->port.usb.pipe_write.ucNumber)
			pipe_addr = &usb_if_dev(usb_if, 0)->port.usb.pipe_write;

		else if ((usb_if->can_count > 1) &&
			 (ep->bEndpointAddress ==
			  usb_if_dev(usb_if, 1)->port.usb.pipe_write.ucNumber))
			pipe_addr = &usb_if_dev(usb_if, 1)->port.usb.pipe_write;

		if (pipe_addr)
			pipe_addr->wDataSz = le16_to_cpu(ep->wMaxPacketSize);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
		usb_reset_endpoint(usb_dev, ep->bEndpointAddress);
#endif
	}

	/* ucRevision needs to be defined before allocating resources
	 * (PCAN-USB) */
#if defined(__LITTLE_ENDIAN)
	usb_if->ucHardcodedDevNr =
		(u8)(usb_if->usb_dev->descriptor.bcdDevice & 0xff);
	usb_if->ucRevision = (u8)(usb_if->usb_dev->descriptor.bcdDevice >> 8);
#elif defined(__BIG_ENDIAN)
	usb_if->ucHardcodedDevNr =
		(u8)(usb_if->usb_dev->descriptor.bcdDevice >> 8);
	usb_if->ucRevision = (u8)(usb_if->usb_dev->descriptor.bcdDevice & 0xff);
#else
#error "Please fix the endianness defines in <asm/byteorder.h>"
#endif

#ifdef DEBUG_USB
	pr_info(DEVICE_NAME
		": %s(): ucHardcodedDevNr=0x%02x ucRevision=0x%02X\n",
		__func__, usb_if->ucHardcodedDevNr, usb_if->ucRevision);
#endif

	/* resources MUST be allocated before calling device_init() */
	err = pcan_usb_alloc_resources(usb_if);
	if (err) {

		/* Note: pcan_free() is able to handle NULL pointers */
		goto reject_free;
	}

	/* call initialisation callback for entire device */
	err = device_init(usb_if);
	if (err) {
		pr_err(DEVICE_NAME ": device_init() failure err %d\n", err);
		goto reject_free;
	}

	/* install the reception part for the interface */
	FILL_BULK_URB(usb_if->read_data, usb_if->usb_dev,
	              usb_rcvbulkpipe(usb_if->usb_dev,
	                              usb_if->pipe_read.ucNumber),
	              usb_if->read_buffer_addr[0],
		      usb_if->read_buffer_size,
	              pcan_usb_read_notify, usb_if);

	/* submit urb */
	err = __usb_submit_urb(usb_if->read_data);
	if (err) {
		pr_err(DEVICE_NAME ": %s() can't submit! (%d)\n",
			__func__, err);
		goto reject_free;
	}

	atomic_inc(&usb_if->r_active_urbs);

	/* should be set BEFORE pcan_usb_create_dev() */
	usb_set_intfdata(interface, usb_if);

	/* next, initialize each controller */
	for (i = 0; i < dev_ctrl_count; i++) {

		struct pcandev *dev = usb_if_dev(usb_if, i);

		/* preset finish flags */
		atomic_set(&dev->port.usb.cmd_sync_complete, 0);
		atomic_set(&dev->port.usb.cmd_async_complete, 1);

		err = pcan_usb_create_dev(usb_if, i);
		if (err)
			goto reject_free_all_dev;
	}

	return 0;

reject_free_all_dev:

	/* remove ALL previously created devs for the same USB interface */
	while (--i >= 0) {
		struct pcandev *dev = usb_if_dev(usb_if, i);
		const int m = dev->nMinor;

		pcan_cleanup_dev(dev);

		pr_info(DEVICE_NAME ": usb device minor %d removed\n", m);
	}

reject_free:
	pcan_usb_free_resources(usb_if);

	pcan_usb_free_interface(usb_if);

	return err;
}

/* called when device is plugged out AND when driver is removed, in a context
 * that it can sleep, with a locked device.
 */
static void pcan_usb_plugout(struct usb_interface *interface)
{
	struct pcan_usb_interface *usb_if = usb_get_intfdata(interface);
	int c;

	if (!usb_if)
		return;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(devid=%04xh)\n", __func__,
		le16_to_cpu(usb_if->usb_dev->descriptor.idProduct));
#endif

	/* do it now in case of reentrance... */
	usb_set_intfdata(interface, NULL);

	for (c = 0; c < usb_if->can_count; c++) {
		struct pcandev *dev = usb_if_dev(usb_if, c);

		/* when the device is plugged out, then its pointer is
		 * removed from the usb_if list
		 */
		if (!dev)
			continue;

		/* Should close all dev resources EVEN if the device is in use,
		 * otherwise application may not be noticed that the device was
		 * removed: CAN_Open(); while (1) CAN_Read(h);
		 */
		pcan_cleanup_dev(dev);
	}

	pcan_usb_free_resources(usb_if);

	if (usb_if->device_free)
		usb_if->device_free(usb_if);

	usb_reset_device(usb_if->usb_dev);

	pcan_usb_free_interface(usb_if);
}

/* small interface to rest of driver, only init and deinit */
static int pcan_usb_core_init(void)
{
	memset (&pcan_drv.usbdrv, 0, sizeof(pcan_drv.usbdrv));

	/* do inherit default options */
	pcan_inherit_options_from(pcan_usb_chip_opts,
		pcan_inherit_options_from(pcan_usb_opts, NULL));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,24) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	pcan_drv.usbdrv.owner = THIS_MODULE;
#endif

	pcan_drv.usbdrv.probe = pcan_usb_plugin;
	pcan_drv.usbdrv.disconnect = pcan_usb_plugout;
	pcan_drv.usbdrv.name = DEVICE_NAME;
	pcan_drv.usbdrv.id_table = pcan_usb_ids;

	return usb_register(&pcan_drv.usbdrv);
}

/* called when the driver is removed for each each usb_if found:
 * this is the 1st function called on each plugged pcandev.
 */
static int pcan_usb_do_cleanup(struct device *dev, void *arg)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct pcan_usb_interface *usb_if = \
			(struct pcan_usb_interface *)usb_get_intfdata(intf);
	int c;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(usb_if=%p, can_count=%d)\n",
		__func__, usb_if, usb_if->can_count);
#endif

	usb_if->removing_driver++;

	/* Browse controllers list */
	for (c = 0; c < usb_if->can_count; c++) {
		struct pcandev *pdev = usb_if_dev(usb_if, c);
#ifdef DEBUG_USB
		pr_info(DEVICE_NAME ": %s(%s CAN%u): plugged=%u\n", __func__,
			pdev->adapter->name, pcan_idx(pdev)+1,
			pdev->is_plugged);
#endif
		/* this should always be true... */
		if (pdev->is_plugged)

			/* Last chance for URB submitting */
			if (usb_if->device_ctrl_cleanup)
				usb_if->device_ctrl_cleanup(pdev);
	}

	return 0;
}

/* called when the driver is removed, that is, all USB devices found are
 * plugged
 */
void pcan_usb_deinit(void)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	if (pcan_drv.usbdrv.probe == pcan_usb_plugin) {

		/* Added this since it is the last chance for URB submitting */
		int err = driver_for_each_device(
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18) && LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
					&pcan_drv.usbdrv.drvwrap.driver,
#else
					&pcan_drv.usbdrv.driver,
#endif
					NULL, NULL, pcan_usb_do_cleanup);

		/* driver_for_each_device() is declared with "must_check"
		 * attribute so check err here, knowing that drv is not NULL
		 * (1st arg) and that pcan_usb_do_cleanup() always return 0
		 */
		if (err)
			err = 0;

		/* then it was registered:
		 * unregistering usb driver makes a plugout of devices
		 */
		usb_deregister(&pcan_drv.usbdrv);
	}
}

/* init for usb based devices from peak */
int pcan_usb_register_devices(void)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	return pcan_usb_core_init();
}
#endif /* USB_SUPPORT */
