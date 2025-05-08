/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_main.h - global defines to include in all files this module is made of
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
#ifndef __PCAN_MAIN_H__
#define __PCAN_MAIN_H__

/* INCLUDES */
#include "src/pcan_common.h"

#include <linux/types.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/time.h>

#ifdef LINUX_26
#include <linux/device.h>
#endif

#ifdef DEBUG
#define DEBUG_TX_ENG
#endif

#ifdef PCI_SUPPORT
#include <linux/pci.h>

#define PCAN_PCI_MINOR_BASE	0	/* the base of all pci device minors */
#endif

#ifdef OLD_DEVNUM_SCHEME
/* pcan <= v8.5.1:
 * 0	7	PCI/PCIe
 * 8	15	ISA/PC104
 * 16	23	DNG SP
 * 24	31	DNG EPP
 * 32	39	USB
 * 40	47	PC-CARD
 */
#ifdef ISA_SUPPORT
#define ISA_MINOR_BASE		8
#endif

#ifdef DONGLE_SUPPORT
#define PCAN_DNG_SP_MINOR_BASE	16	/* SP devs minors starting point */
#define PCAN_DNG_EPP_MINOR_BASE	24	/* EPP devs minors starting point */
#endif

#else
/* now:
 * 0	31	PCI/PCIe
 * 32	63	USB
 * 64	71	PC-CARD
 * 72	79	ISA/PC104
 * 80	87	DNG SP
 * 88	95	DNG EPP
 */ 
#ifdef ISA_SUPPORT
#define ISA_MINOR_BASE		72
#endif

#ifdef DONGLE_SUPPORT
#define PCAN_DNG_SP_MINOR_BASE	80	/* SP devs minors starting point */
#define PCAN_DNG_EPP_MINOR_BASE	88	/* EPP devs minors starting point */
#endif
#endif

#include <asm/atomic.h>

#ifdef PARPORT_SUBSYSTEM
#include <linux/parport.h>
#endif

#ifdef PCIEC_SUPPORT
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#endif

#ifdef USB_SUPPORT
#include <linux/usb.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 4, 19)
typedef struct urb urb_t, *purb_t;
#endif

#define PCAN_USB_MINOR_BASE	32	/* USB dev minors starting point */

#endif

#ifdef PCCARD_SUPPORT

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
#include <pcmcia/cs_types.h>
#endif
#include <pcmcia/cs.h>
#endif

#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

#ifdef OLD_DEVNUM_SCHEME
#define PCCARD_MINOR_BASE	40
#else
#define PCCARD_MINOR_BASE	64
#endif

#define PCAN_USB_MINOR_END	(PCCARD_MINOR_BASE-1)

#endif	/* PCCARD_SUPPORT */

/* compute index of last USB entry according to selected supports */
#ifndef PCAN_USB_MINOR_END

#ifdef OLD_DEVNUM_SCHEME
#define PCAN_USB_MINOR_END	-1
#else

#ifdef ISA_SUPPORT
#define PCAN_USB_MINOR_END	(ISA_MINOR_BASE-1)

#elif defined(DONGLE_SUPPORT)
#define PCAN_USB_MINOR_END	(PCAN_DNG_SP_MINOR_BASE-1)
#else
#define PCAN_USB_MINOR_END	-1
#endif
#endif	/* OLD_DEVNUM_SCHEME */
#endif	/* PCAN_USB_MINOR_END */

#ifdef NETDEV_SUPPORT
#include <linux/netdevice.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31)
#include <linux/can/dev.h>
#endif
#endif

struct pcanusr;

/* PF_CAN is part of the Linux Mainline Kernel since v2.6.25
 * For older Kernels the PCAN driver includes the needed
 * defines from private files src/can.h and src/error.h
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#else /* before 2.6.25 pcan netdev contains private includes */
#include <src/can.h>
#include <src/error.h>

#define ARPHRD_CAN	280	/* to be moved to include/linux/if_arp.h */
#define ETH_P_CAN	0x000C	/* to be moved to include/linux/if_ether.h */
#endif

/* fix overlap in namespace between socketcan can/error.h and pcan.h */
#define CAN_ERR_BUSOFF_NETDEV	CAN_ERR_BUSOFF
#undef CAN_ERR_BUSOFF

#include <pcan.h>
#include <pcanfd.h>

#include "src/pcan_fifo.h"
#include "src/pcan_timing.h"

/* DEFINES */
#define CHANNEL_SINGLE	0	/* this is a single channel device */
#define CHANNEL_MASTER	1	/* multi channel device, master device */
#define CHANNEL_SLAVE	2	/* multi channel device, this is slave */

#define READBUFFER_SIZE		360	/* buffers used in readr/write call */
#define WRITEBUFFER_SIZE	512

#define PCAN_MAJOR	0	/* use dynamic major alloc, 91 otherwise */

/* parameter wBTR0BTR1:
 * bitrate codes of BTR0/BTR1 registers
 */
#define CAN_BAUD_1M	0x0014	/*   1 MBit/s */
#define CAN_BAUD_500K	0x001C	/* 500 kBit/s */
#define CAN_BAUD_250K	0x011C	/* 250 kBit/s */
#define CAN_BAUD_125K	0x031C	/* 125 kBit/s */
#define CAN_BAUD_100K	0x432F	/* 100 kBit/s */
#define CAN_BAUD_50K	0x472F	/*  50 kBit/s */
#define CAN_BAUD_20K	0x532F	/*  20 kBit/s */
#define CAN_BAUD_10K	0x672F	/*  10 kBit/s */
#define CAN_BAUD_5K	0x7F7F	/*   5 kBit/s */

/* Activity states */
#define ACTIVITY_NONE		0	/* LED off           - set when the channel is created or deleted */
#define ACTIVITY_INITIALIZED	1	/* LED on            - set when the channel is initialized */
#define ACTIVITY_IDLE		2	/* LED slow blinking - set when the channel is ready to receive or transmit */
#define ACTIVITY_XMIT		3	/* LED fast blinking - set when the channel has received or transmitted */

/* this is how pcan saves version major+minor+subminor into a 32-bit value */
#define PCAN_MAKE_VERSION(x, y, z)	VER_NUM(x, y, z)

#define PCAN_SF_DEVDATA		(PCAN_SF_MAX-1)
#define SF_GET_DEVDATA		PCAN_SF_GET(PCAN_SF_DEVDATA)
#define SF_SET_DEVDATA		PCAN_SF_SET(PCAN_SF_DEVDATA)

/* this structure holds various channel properties */
typedef struct chn_props {
	u8 ucExternalClock : 1;	/* supplied with a external clock */
	u8 ucMasterDevice  : 2; /* clock master, slave, single */
} CHN_PROPS;

/* helper for fast conversion between SJA1000 and host data ordering */
typedef union {
	u8  uc[4];
	u32 ul;
} ULCONV;

typedef union {
	u8  uc[2];
	u16 uw;
} UWCONV;

/* uCAN device programming interface */
struct ucan_engine;
struct ucan_msg;
struct pcandev;
struct ucan_ops {

	int (*set_clk_domain)(struct pcandev *, struct pcanfd_init *);

	/* Tx path: commands and msgs sending interface */
	int (*send_cmd)(struct pcandev *);
	int (*send_msg)(struct pcandev *);

	/* Rx path: all rx messages handlers are stored into a table */
	int (**handle_msg_table)(struct ucan_engine *,
				 struct ucan_msg *, void *);
	int handle_msg_size;

	/* handler of msgs which ID is outside handle_msg_table[] */
	int (*handle_private_msg)(struct ucan_engine *,
				  struct ucan_msg *, void *);
};

struct ucan_engine {
	struct ucan_ops *	ops;		/* uCAN cmd/msg ops */
	struct pcandev **	devs;		/* uCAN channels */
	int			devs_count;	/* count of uCAN channels */

	void *	cmd_head;	/* buffer used to save uCAN cmds */
	int	cmd_size;	/* size of this buffer in bytes */
	int	cmd_len;	/* length of the cmd (in bytes) */

	u8	frag_rec[128];	/* ALIGN(sizeof(struct ucan_rx_msg)+64,4)=92 */
	int	frag_size;
};

#define ucan_dev(u, i)		((u)->devs[i])

#ifdef PCIEC_SUPPORT
#define PCIEC_CHANNELS	2	/* maximum PCAN-PCIExpressCard channel number */

typedef struct {
	void __iomem *gpoutenable;	/* vaddr for bit-banging interface */
	void __iomem *gpin;
	void __iomem *gpout;
	struct	pcandev *dev[PCIEC_CHANNELS];	/* associated channels */

	struct i2c_adapter		adapter;	/* i2c adapter */
	struct i2c_algo_bit_data	algo_data;	/* bit banging if */

	u8	VCCenable;		/* reflection of VCCEN */
	u8	PCA9553_LS0Shadow;      /* Shadow reg holding LEDs state */
	int	run_activity_timer_cyclic;	/* sync flag stop conditions */

	struct delayed_work	activity_timer;	/* scan for activity timer */
} PCAN_PCIEC_CARD;
#endif

struct pcan_msi {
	int	msi_requested;
	int	msi_assigned;
};

struct ucan_pci_page {
	void *		vbase;
	dma_addr_t	lbase;
	u32		offset;
	u32		size;
};

/* timestamp sync in µs */
struct pcan_time_sync {

	/* SYNC variables */
	struct timeval	tv;		/* host time at sync time */
	u64		tv_ns;		/* (used for PCIe FD) */
	u64		ts_us;		/* last sync'ed device timestamp */

	u64		ttv_us;		/* count of host_us */
	u64		tts_us;		/* count of device_us */
	long		clock_drift;

	/* EVT variables (cm_inactivity_count MUST be the 1st one) */
	u32		cm_inactivity_count;

	struct timeval	evt_base_tv;
	u64		evt_base_us;
	u32		evt_time_lag;

	u32		ts_fixed;
};

/* 32-bit area describing the content of the beginning of Rx DMA area of the
 * PCI boards running uCAN
 */
struct ucan_pci_irq_status {
#ifdef __LITTLE_ENDIAN
	uint	irq_tag:4;
	uint	rx_cnt:7;
	uint	:5;
	uint	lnk:1;
	uint	:15;
#else
	uint	:15;
	uint	lnk:1;
	uint	:5;
	uint	rx_cnt:7;
	uint	irq_tag:4;
#endif
};

typedef struct {
	u32	dwConfigPort;		/* the configuration port, PCI only */
	void __iomem *can_port_addr;    /* virtual address of port */
	void __iomem *bar0_cfg_addr;	/* vaddr of the config port */
	struct pci_dev *pciDev;		/* remember the hosting PCI card */
#ifdef PCIEC_SUPPORT
	PCAN_PCIEC_CARD *card;		/* point to a card structure */
#endif
	/* PCIe uCAN specific */
	u64	ucan_cmd;

	u32	irq_tag;
	u32	irq_not_for_me;
	struct ucan_pci_irq_status irq_status;

	dma_addr_t rx_dma_laddr;
	void *	rx_dma_vaddr;

	dma_addr_t tx_dma_laddr;
	void *	tx_dma_vaddr;

	u16	tx_pages_free;
	u16	tx_page_index;

	struct ucan_pci_page *tx_pages;


} PCI_PORT;

typedef struct {
#ifdef PARPORT_SUBSYSTEM
	struct pardevice *pardev;	/* associated parallel port */
#endif
	u16	wEcr;			/* ECR register in case of EPP */
	u8	ucOldDataContent;	/* overwritten contents of port regs */
	u8	ucOldControlContent;
	u8	ucOldECRContent;

	pcan_lock_t	lock;	/* shared access to chip registers */
} DONGLE_PORT;

#ifdef PCAN_HANDLE_IRQ_SHARING
typedef struct {
	struct list_head	item;	/* link for items with same irq level */
	struct pcandev *	dev;	/* device with the same irq level */
} SAME_IRQ_ITEM;

typedef struct {
	struct list_head	same_irq_items;	/* list of SAME_IRQ_ITEM's */

	u16		same_irq_count;		/* count of devices */
	u16		same_irq_active;	/* count of active irqs */
} SAME_IRQ_LIST;
#endif

typedef struct {
#ifdef PCAN_HANDLE_IRQ_SHARING
	SAME_IRQ_ITEM	same;	/* each ISA_PORT belongs to one SAME_IRQ_LIST */
	SAME_IRQ_LIST	anchor;	/* the anchor for one irq level */

	SAME_IRQ_LIST *	my_anchor;	/* list of items for the same irq */
#endif
} ISA_PORT;

#ifdef PCCARD_SUPPORT
struct pcan_pccard;
typedef struct {
	struct pcan_pccard *card;	/* points to the associated pccard */
} PCCARD_PORT;
#endif

#ifdef USB_SUPPORT
struct pcan_usb_time {
	u16	ticks16;

	u16	sync_ticks_init;
	u32	sync_ticks_low;
	u32	sync_ticks_high;
};

typedef struct {
	u8	ucNumber;		/* number (or address) of endpoint */
	u16	wDataSz;		/* supported max data transfer length */
} PCAN_ENDPOINT;

struct pcan_usb_interface;
typedef struct pcan_usb_port {
	struct pcan_usb_interface *usb_if;

	u8	ucHardcodedDevNr;

	u32	dwTelegramCount;	/* counter for telegrams */

	struct pcan_usb_time	time;

	struct urb *	write_data;	/* pointer to write data urb */

	u8 *	write_buffer_addr;	/* buffer for to write data */

	PCAN_ENDPOINT pipe_write;

	u32	state;

	struct urb *	urb_cmd_async;		/* async. cmd URB */
	struct urb *	urb_cmd_sync;		/* sync cmd URB */
	atomic_t	cmd_async_complete;	/* flag set when async cmd  */
	atomic_t	cmd_sync_complete;	/* flag set when sync cmd  */
						/* finished */
	u8 *		cout_baddr;		/* command buffer address */
	int		cout_bsize;		/* command buffer size */

} USB_PORT;
#endif /* USB_SUPPORT */

struct pcan_version {
	int		major;
	int		minor;
	int		subminor;
	int		extra;
};

#define PCAN_GUID_LEN		16		/* 128-bit Unique Identifier */

#define PCAN_ADAPTER_GUID	0x00000001	/* adapter handles a GUID */

struct pcan_adapter {
	u32		flags;
	const char *	name;
	const char *	part_num;
	int		index;
	int		can_count;
	int		opened_count;
	struct pcan_version hw_ver;
	u8		guid[PCAN_GUID_LEN];
};

typedef struct __array_of_struct(pcanfd_available_clock, 1)
	pcanfd_mono_clock_device;

struct pcanfd_options {
	int req_size;
	int (*get)(struct pcandev *dev, struct pcanfd_option *, void *arg);
	int (*set)(struct pcandev *dev, struct pcanfd_option *, void *arg);
};

/* these structs enable to apply clock drift outside ISR, but when the message
 * is read by user.
 */
struct pcan_timeval {
	struct timeval tv;		/* host base time */
	u64 ts_us;			/* event hw time */
	u64 tv_us;			/* hw base time */
	u32 ts_mode;			/* cooking mode */
	long clock_drift;		/* clock drift */
};

struct pcanfd_rxmsg {
	struct pcanfd_msg msg;
	struct pcan_timeval hwtv;
};

struct __flex_array_of_struct(pcanfd_rxmsg);

#define pcanfd_rxmsgs	pcanfd_rxmsgs_0

struct pcanfd_txmsg {
	struct pcanfd_msg msg;
	struct list_head link;
	struct timeval delay;
};

struct __flex_array_of_struct(pcanfd_txmsg);

#define pcanfd_txmsgs	pcanfd_txmsgs_0

/* flags */
#define PCAN_DEV_USES_ALT_NUM	0x00000002
#define PCAN_DEV_IGNORE_RX	0x00000004
#define PCAN_DEV_LINKED		0x00000008
#define PCAN_DEV_CLEANED	0x00000010
#define PCAN_DEV_STATIC		0x00000020
#define PCAN_DEV_OPENED		0x00000040
#define PCAN_DEV_BUS_ON		0x00000080
#define PCAN_DEV_FREE_IRQ0	0x00000100
#define PCAN_DEV_MSI_SHARED	0x00000200
#define PCAN_DEV_CLEANING	0x00000400
#define PCAN_DEV_CLOSING	0x00000800
#define PCAN_DEV_CTRLR_FATAL	0x00001000
#define PCAN_DEV_TS_SOF		0x02000000
#define PCAN_DEV_SELF_ACK	0x04000000
#define PCAN_DEV_BRS_IGN	0x08000000

/* features */
#define PCAN_DEV_BUSLOAD_RDY	0x00010000
#define PCAN_DEV_ERRCNT_RDY	0x00020000
#define PCAN_DEV_MSD_RDY	0x00040000
#define PCAN_DEV_TXPAUSE_RDY	0x00080000
#define PCAN_DEV_HWTS_RDY	0x00100000
#define PCAN_DEV_HWTSC_RDY	0x00200000
#define PCAN_DEV_SLF_RDY	0x00400000
#define PCAN_DEV_ECHO_RDY	0x00800000
#define PCAN_DEV_FD_RDY		0x01000000
#define PCAN_DEV_TS_SOF_RDY	0x02000000
#define PCAN_DEV_SELF_ACK_RDY	0x04000000
#define PCAN_DEV_BRS_IGN_RDY	0x08000000
#define PCAN_DEV_SNG_RDY	0x10000000
#define PCAN_DEV_DEVDATA	0x20000000
#define PCAN_DEV_NEW_FW_AV	0x40000000

#define PCAN_DEV_SJA1000_RDY	(PCAN_DEV_ERRCNT_RDY|\
				 PCAN_DEV_SLF_RDY|\
				 PCAN_DEV_SNG_RDY)

#define TX_ENGINE_CLOSED	0
#define TX_ENGINE_IDLE		1
#define TX_ENGINE_STARTED	2
#define TX_ENGINE_STOPPED	3
#define TX_ENGINE_BUSY		4

#define MAX_WAIT_UNTIL_CLOSE	1000

/* this bit indicates that hw timestamps should be measured at the moment of SOF
 * rather than at the moment of EOF (default)
 */
#define PCANFD_OPT_HWTIMESTAMP_SOF	PCANFD_OPT_HWTIMESTAMP_RESERVED_4
#define PCANFD_OPT_HWTIMESTAMP_MASK	(PCANFD_OPT_HWTIMESTAMP_SOF - 1)

struct pcandev {
	struct list_head	list_dev; /* link anchor for list of devices */

	int	nOpenPaths;	/* number of open paths linked to the device */
	int	nMajor;		/* device major (USB devices have their own) */
	int	nMinor;		/* the associated minor */
	char *	type;		/* the literal type of the device, info only */

	u16	wType;		/* (number type) to distinguish sp and epp */
	u16	wInitStep;	/* device specific init state */

	int	can_idx;	/* in case of multi-CAN board/adapter */
	int	opened_index;	/* open sequence order */

	struct pcan_adapter *adapter;	/* link to the real device */
	struct pcan_version *hw_ver;

	u32	dwPort;		/* the port of the transport layer */

	u32	ts_mode;

	u32	flags;
	u32	features;
	u32	tx_iframe_delay_us;

	unsigned long bus_load_ind_period;

	struct timer_list bus_load_timer;
	u32 bus_load_count;
	u32 bus_load_total;

	u32	allowed_msgs;
	u32	sysclock_Hz;
	const struct pcanfd_available_clocks *clocks_list;
	const struct pcanfd_bittiming_range *bittiming_caps;
	const struct pcanfd_bittiming_range *dbittiming_caps;

	const struct pcanfd_options *	option;
	struct pcan_time_sync	time_sync;	/* used to sync clocks */

	int	linger_opt_value;
	int	linger_cur_value;

	struct device *		sysfs_dev;
	struct attribute **	sysfs_attrs;

	u32	device_alt_num;
	union {
		struct {
#ifdef __LITTLE_ENDIAN
			u32	mask;
			u32	code;
#else
			u32	code;
			u32	mask;
#endif
		};
		u64	value64;
	} acc_11b, acc_29b;

#ifdef NETDEV_SUPPORT
	struct net_device *netdev;	/* reference to net device for AF_CAN */
	struct delayed_work restart_work;
#endif

	struct ucan_engine ucan;	/* ref to the uCAN engine */

	union {
		DONGLE_PORT	dng;	/* private data of the various ports */
		ISA_PORT	isa;
		PCI_PORT	pci;
#ifdef PCCARD_SUPPORT
		PCCARD_PORT	pccard;
#endif
#ifdef USB_SUPPORT
		USB_PORT	usb;
#endif
	} port;

	struct chn_props	props;	/* various channel properties */

	/* read a register */
	u8   (*readreg)(struct pcandev *dev, u8 port);

	/* write a register */
	void (*writereg)(struct pcandev *dev, u8 port, u8 data);

	/* cleanup the interface */
	int  (*cleanup)(struct pcandev *dev);

	/* called at open of a path (open()) */
	int  (*open)(struct pcandev *dev);

	/* called at release of a path (close()) */
	int  (*release)(struct pcandev *dev);

	/* install the interrupt handler */
	int  (*req_irq)(struct pcandev *dev, struct pcanusr *ctx);

	void (*lock_irq)(struct pcandev *dev, pcan_lock_irqsave_ctxt *pflags);
	void (*unlock_irq)(struct pcandev *dev, pcan_lock_irqsave_ctxt *pflags);

	/* release the interrupt */
	void (*free_irq)(struct pcandev *dev, struct pcanusr *ctx);

	/* open the device itself */
	int  (*device_open)(struct pcandev *dev, u16 btr0btr1,
						u8 bExtended, u8 bListenOnly);
	int  (*device_open_fd)(struct pcandev *dev, struct pcanfd_init *pfdi);

	/* reset the controller only */
	int  (*device_reset)(struct pcandev *dev);

	/* release the device itself */
	void (*device_release)(struct pcandev *dev);

	/* identify device */
	int (*device_identify)(struct pcandev *dev, u32 delay_ms);

	/* write the device */
	int  (*device_write)(struct pcandev *dev, struct pcanusr *ctx);

	/* interface to set or get special parameters from the device */
	int  (*device_params)(struct pcandev *dev, TPEXTRAPARAMS *params);

#ifndef NETDEV_SUPPORT
	pcan_event_t	in_event;

	FIFO_MANAGER	rx_fifo;	/* manages the read fifo */
	struct pcanfd_rxmsg *rx_msgs;
#endif
	pcan_event_t	out_event;

	FIFO_MANAGER	tx_fifo;	/* manages the write fifo */
	struct pcanfd_txmsg *tx_msgs;

	unsigned	locked_tx_engine_state;

	pcan_lock_t	wlock;	/* mutual exclusion lock for write invocation */
	pcan_lock_t	isr_lock;	/* in isr */

	pcan_mutex_t	mutex;

#ifndef NO_RT
	struct rtdm_device *	rtdm_dev;
	rtdm_irq_t	irq_handle;	/* mandatory parameter in for Xenomai */
#endif

	int	bus_load;

	int	nLastError;	/* last error written */
	enum pcanfd_status	bus_state;
	enum pcanfd_error	bus_error;
	u32	dwErrorCounter;		/* counts all fatal errors */
	u32	rx_irq_counter;
	u32	tx_irq_counter;
	u32	rx_frames_counter;	/* Rx frames read on device */
	u32	tx_frames_counter;	/* Tx frames written on device */
	u32	rx_bytes_counter;
	u32	tx_bytes_counter;

	struct pcanfd_init	def_init_settings;
	struct timeval		init_timestamp;
	struct pcanfd_init	init_settings;

	void *	filter;		/* ID filter - currently associated to device */

	u16	wCANStatus;	/* status of CAN chip */
	u8	is_plugged;	/* the device is PhysicallyInstalled */
	u8	ucActivityState;	/* state of a channel activity */

	u8	rx_error_counter;	/* Rx errors counter */
	u8	tx_error_counter;	/* Tx errors counter */

	struct {
#ifdef FIFO_PRE_ROUTINE
		struct pcanfd_rxmsg status;
		struct pcanfd_rxmsg error;
#endif
		int	bus_state;
		int	bus_load;
		int	bus_error;
		int	rxerr;
		int	txerr;
	} posted;

	u16	wIrq;		/* the associated irq */
};

#ifdef USB_SUPPORT
struct pcan_usb_interface {
	struct pcan_adapter *adapter;
	struct usb_device *usb_dev;	/* Kernel USB device */
	struct usb_interface *usb_intf;

	int	can_count;
	int	index;
	int	opened_count;

	u32	state;
	int	removing_driver;
	int	cm_ignore_count;	/* nb of CM to ignore before handling */

	u8	ucHardcodedDevNr;
	u32	dwSerialNumber;		/* Serial number of device */
	u8	ucRevision;		/* the revision number of  */
	u16	mcu;

	struct pcan_version	hw_ver;

	atomic_t	r_active_urbs;	/* note all active urbs for reading */
	atomic_t	w_active_urbs;	/* note all active urbs for writing */

	struct urb *	urb_cmd_sync;		/* sync cmd URB */
	atomic_t	cmd_sync_complete;	/* flag set when sync cmd  */
						/* finished */

	int		read_packet_size;	/* packet read buffer size */
	int		write_packet_size;	/* packet write buffer size */

	int		read_buffer_size;
	int		write_buffer_size;

	struct urb *	read_data;		/* pointer to read data urb */
	u8 *		read_buffer_addr[2];	/* read data transfer buffers */

	/* USB pipes to/from CAN controller(s) */
	PCAN_ENDPOINT pipe_cmd_in;
	PCAN_ENDPOINT pipe_cmd_out;
	PCAN_ENDPOINT pipe_read;

	int  (*device_init)(struct pcan_usb_interface *);

	int  (*device_get_snr)(struct pcan_usb_interface *, u32 *);
	int  (*device_msg_decode)(struct pcan_usb_interface *, u8 *, int );
	void (*device_free)(struct pcan_usb_interface *);
	int  (*device_set_mass_storage_mode)(struct pcan_usb_interface *);

	int  (*device_ctrl_init)(struct pcandev *dev);
	void (*device_ctrl_cleanup)(struct pcandev *dev);
	int  (*device_ctrl_open)(struct pcandev *dev, u16, u8, u8 );
	int  (*device_ctrl_open_fd)(struct pcandev *dev,
					struct pcanfd_init *pfdi);
	int  (*device_ctrl_close)(struct pcandev *dev);
	int  (*device_ctrl_set_bus_on)(struct pcandev *dev);
	int  (*device_ctrl_set_bus_off)(struct pcandev *dev);
	int  (*device_ctrl_set_dnr)(struct pcandev *dev, u32);
	int  (*device_ctrl_get_dnr)(struct pcandev *dev, u32 *);
	int  (*device_ctrl_msg_encode)(struct pcandev *dev, u8 *, int *);

#ifdef PCAN_SF_DEVDATA
	int  (*device_set_devdata)(struct pcandev *dev, int, const u8 *);
	int  (*device_get_devdata)(struct pcandev *dev, int, u8 *);
#endif

	int 	dev_frame_index;
	int	bus_frame_index;

	u64	uptime_us;

	u32	cm_ts_low;

	int	frag_rec_offset;

#ifdef CONFIG_UBSAN_BOUNDS_STRICT
	struct pcandev *devs[];		/* a device for each CAN controller */
#else
	struct pcandev *devs[0];	/* a device for each CAN controller */
#endif
};

#define usb_if_dev(u, i)		((u)->devs[i])

#endif

struct pcanusr {

#ifdef NETDEV_SUPPORT
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	struct net_device_stats		stats;	/* standard netdev statistics */

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31)
	struct can_priv			can;	/* must be the 1st one */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
	/* consider playing with bitrates only for 3.6+ and CAN-FD */
	struct can_bittiming_const	bt_const;
	struct can_bittiming_const	dbt_const;
#endif
#endif

#endif /* NETDEV_SUPPORT */

	int	open_flags;

	u8	pcReadBuffer[READBUFFER_SIZE];	/* used in read() call */
	u8 *	pcReadPointer;	/* points into current read data rest */
	int	nReadRest;	/* rest of data left to read */

	int	nTotalReadCount;        /* for test only */

	u8	pcWriteBuffer[WRITEBUFFER_SIZE];/* used in write() call */
	u8 *	pcWritePointer;	/* work pointer into buffer */
	int	nWriteCount;	/* count of written data bytes */

#ifdef NO_RT
	struct file *			filep;		/* back linkage */
#elif !defined(XENOMAI3)
	struct rtdm_dev_context *	context;	/* back linkage */
#endif

	struct pcandev *dev;	/* pointer to related device */
};

struct pcan_driver {
	int	nMajor;		/* the major number of Pcan interfaces */
	u16	wDeviceCount;	/* count of found devices */
	u16	wInitStep;	/* driver specific init state */

	struct timeval		sInitTime;	/* time when init was called */
	struct list_head	devices;	/* list of devices */
#ifdef HANDLE_HOTPLUG
	pcan_lock_t		devices_lock;	/* devices list mutex */
#endif
	u8 *	szVersionString;	/* driver version string */

#ifdef PCCARD_SUPPORT
#ifndef LINUX_24
	struct pcmcia_driver	pccarddrv;	/* pccard driver structure */
#endif
#endif

#ifdef USB_SUPPORT
	struct usb_driver	usbdrv;		/* usb driver structure */
#endif

#ifdef PCI_SUPPORT
	struct pci_driver	pci_drv;	/* pci driver structure */
#endif

#ifdef ISA_SUPPORT
	/* legacy platform driver */
	struct device_driver	legacy_driver_isa;
#endif

#ifdef DONGLE_SUPPORT
	/* legacy platform driver */
	struct device_driver	legacy_driver_dongle;
#endif

	struct class *	class;	/* the associated class of pcan devices */
};

#ifndef NO_RT
struct pcan_rtdm_dev {
	struct list_head	list_dev;
	struct rtdm_device *	device;
};
#endif

/* the global driver object */
extern struct pcan_driver pcan_drv;
extern ushort txqhiwat;

extern const struct pcanfd_bittiming_range sja1000_capabilities;

extern const pcanfd_mono_clock_device sja1000_clocks;

/* Global functions */

#ifdef USB_SUPPORT

/* this function is global for USB adapters */
static struct pcan_usb_interface *pcan_usb_get_if(const struct pcandev *dev)
{
	return dev->port.usb.usb_if;
}

/* Before calling this function, some fields needs to be set:
 * - can_idx *and* wType (which is done by pcan_alloc_dev())
 *
 * And for PCAN-USB X6 device:
 * - dev->port.usb.usb_if (which is done next to pcan_alloc_dev())
 * - can_count (which is done before entering pcan_alloc_dev() loop)
 * (see pcan_usb_plugin() in pcan_usb_core.c)
 * - usb_if->index (which is done by pcanfd_device_init())
 */
static inline int pcan_idx(const struct pcandev *dev)
{
	int c = dev->can_idx;

	if (dev->wType == HW_USB_X6) {
		struct pcan_usb_interface *usb_if = pcan_usb_get_if(dev);

		c += usb_if->index * usb_if->can_count;
	}
	return c;
}
#else
#define pcan_idx(dev)	((dev)->can_idx)
#endif

/* request time in msec, fast */
u32 get_mtime(void);

void pcan_init_version(struct pcan_version *ver);
void pcan_init_adapter(struct pcan_adapter *pa, const char *name,
		       const char *part_num, int index, int can_count);

void *__pcan_alloc_adapter_ex(const char *name, const char *part_num, int index,
			      int can_count, int extra_size);

#ifdef DEBUG
static inline void *_pcan_alloc_adapter_ex(char *f, int l, const char *name,
					   const char *part_num, int index,
					   int can_count, int extra_size)
{
	void *p = __pcan_alloc_adapter_ex(name, part_num, index, can_count,
					  extra_size);
	pr_info(DEVICE_NAME ": %s(%u): %s()=%p\n", f, l, __func__, p);
	return p;
}

#define pcan_alloc_adapter_ex(n, p, i, c, e)	\
		_pcan_alloc_adapter_ex(__FILE__, __LINE__, n, p, i, c, e)
#define pcan_alloc_adapter(n, p, i, c)		\
		_pcan_alloc_adapter_ex(__FILE__, __LINE__, n, p, i, c, 0)
#else
#define pcan_alloc_adapter_ex(n, p, i, c, e)	\
		__pcan_alloc_adapter_ex(n, p, i, c, e)

static inline struct pcan_adapter *pcan_alloc_adapter(const char *name,
						      const char *part_num,
						      int index, int can_count)
{
	return pcan_alloc_adapter_ex(name, part_num, index, can_count, 0);
}
#endif

struct pcan_adapter *__pcan_free_adapter(struct pcan_adapter *pa);

#ifdef DEBUG
static inline struct pcan_adapter *_pcan_free_adapter(char *f, int l,
						      struct pcan_adapter *p)
{
	pr_info(DEVICE_NAME ": %s(%u): %s(%p)\n", f, l, __func__, p);
	return __pcan_free_adapter(p);
}

#define pcan_free_adapter(p)	_pcan_free_adapter(__FILE__, __LINE__, p)
#else
#define pcan_free_adapter(p)	__pcan_free_adapter(p)
#endif

void pcan_set_dev_adapter(struct pcandev *dev, struct pcan_adapter *pa);

struct pcandev *__pcan_alloc_dev(char *type_str, u16 type, int index);

#ifdef DEBUG
static inline struct pcandev *_pcan_alloc_dev(char *f, int l, char *type_str,
					      u16 type, int index)
{
	struct pcandev *p = __pcan_alloc_dev(type_str, type, index);
	pr_info(DEVICE_NAME ": %s(%u): %s()=%p\n", f, l, __func__, p);
	return p;
}

#define pcan_alloc_dev(s, t, i)	_pcan_alloc_dev(__FILE__, __LINE__, s, t, i)
#else
#define pcan_alloc_dev(s, t, i)	__pcan_alloc_dev(s, t, i)
#endif

struct pcandev *__pcan_free_dev(struct pcandev *dev);

#ifdef DEBUG
static inline struct pcandev *_pcan_free_dev(char *f, int l, struct pcandev *p)
{
	pr_info(DEVICE_NAME ": %s(%u): %s(%p)\n", f, l, __func__, p);
	return __pcan_free_dev(p);
}

#define pcan_free_dev(p)	_pcan_free_dev(__FILE__, __LINE__, p)
#else
#define pcan_free_dev(p)	__pcan_free_dev(p)
#endif

void __pcan_destroy_dev(struct pcandev *dev);

#ifdef DEBUG
static inline void _pcan_destroy_dev(char *f, int l, struct pcandev *dev)
{
	pr_info(DEVICE_NAME ": %s(%u): %s(%p)\n", f, l, __func__, dev);
	__pcan_destroy_dev(dev);
}

#define pcan_destroy_dev(p)	_pcan_destroy_dev(__FILE__, __LINE__, p)
#else
#define pcan_destroy_dev(p)	__pcan_destroy_dev(p)
#endif

static inline void pcan_lock_dev(struct pcandev *dev)
{
#ifdef NO_RT
	pcan_mutex_lock(&dev->mutex);
#endif
}

static inline void pcan_unlock_dev(struct pcandev *dev)
{
#ifdef NO_RT
	pcan_mutex_unlock(&dev->mutex);
#endif
}

static inline int pcan_trylock_dev(struct pcandev *dev)
{
#ifdef NO_RT
	return pcan_mutex_trylock(&dev->mutex);
#else
	return 1;
#endif
}

void pcan_add_dev_in_list_ex(struct pcandev *dev, u32 flags);
static inline void pcan_add_dev_in_list(struct pcandev *dev)
{
	pcan_add_dev_in_list_ex(dev, 0);
}

struct pcandev *pcan_remove_dev_from_list(struct pcandev *dev);
int pcan_is_device_in_list(struct pcandev *dev);
int pcan_find_free_minors(struct pcandev *pdev, int from, int until);

#define PCAN_DEVICE_ATTR(_v, _name, _show) \
	struct device_attribute pcan_dev_attr_##_v = \
				__ATTR(_name, S_IRUGO, _show, NULL)

#define PCAN_DEVICE_ATTR_RW(_v, _name, _show, _store) \
	struct device_attribute pcan_dev_attr_##_v = \
				__ATTR(_name, S_IRUGO|S_IWUSR, _show, _store)

static inline struct pcandev *to_pcandev(struct device *dev)
{
	return (struct pcandev *)dev_get_drvdata(dev);
}

static inline void pcan_free_irq(struct pcandev *dev)
{
#ifdef NO_RT
	free_irq(dev->wIrq, dev);
#else
	rtdm_irq_free(&dev->irq_handle);
#endif
}

void pcan_soft_init_ex(struct pcandev *dev,
			const struct pcanfd_available_clocks *clocks,
			const struct pcanfd_bittiming_range *pc,
			u32 features);

static inline void pcan_soft_init(struct pcandev *dev)
{
	pcan_soft_init_ex(dev,
			(struct pcanfd_available_clocks *)&sja1000_clocks,
			&sja1000_capabilities, PCAN_DEV_SJA1000_RDY);
}

void dump_mem(const char *prompt, const void *p, int l);

static inline void __pcan_set_tx_engine(struct pcandev *dev, int tx_eng)
{
	dev->locked_tx_engine_state = tx_eng;
}

#ifdef DEBUG_TX_ENG
static inline void pcan_set_tx_engine_dbg(struct pcandev *dev, int tx_eng,
						const char *f, int l)
{
	if (dev->locked_tx_engine_state != tx_eng)
		pr_info(DEVICE_NAME
			": %s(l=%u): CAN%u TX engine goes to %u\n",
			f, l, pcan_idx(dev)+1, tx_eng);

	__pcan_set_tx_engine(dev, tx_eng);
}

#define pcan_set_tx_engine(d, s)	pcan_set_tx_engine_dbg(d, s, \
							__func__, __LINE__)
#else
#define pcan_set_tx_engine(d, s)	__pcan_set_tx_engine(d, s)
#endif

int __pcan_dev_start_writing(struct pcandev *dev, struct pcanusr *ctx);

#ifdef NETDEV_SUPPORT
static inline void pcan_sync_decode(struct pcandev *dev,
				    u32 ts_low, u32 ts_high,
				    struct pcan_timeval *hwtv)
{
	hwtv->ts_us = ((u64 )ts_high << 32) | ts_low;
}
static inline void pcan_sync_decode64(struct pcandev *dev, u64 ts64,
				      struct pcan_timeval *hwtv)
{
	hwtv->ts_us = ts64;
}
#else
void pcan_sync_init(struct pcandev *dev);
int pcan_sync_decode(struct pcandev *dev, u32 ts_low, u32 ts_high,
		     struct pcan_timeval *hwtv);
int pcan_sync_times(struct pcandev *dev, u32 ts_low, u32 ts_high, int tv_off);

static inline int pcan_sync_decode64(struct pcandev *dev, u64 ts64,
				     struct pcan_timeval *hwtv)
{
	return pcan_sync_decode(dev, ts64 & 0xffffffff, ts64 >> 32, hwtv);
}

static inline int pcan_sync_times64(struct pcandev *dev, u64 ts64, int tv_off)
{
	return pcan_sync_times(dev, ts64 & 0xffffffff, ts64 >> 32, tv_off);
}

struct pcanfd_msg *pcan_sync_timestamps(struct pcandev *dev,
					struct pcanfd_rxmsg *pqm);
#endif

int pcan_post_bus_state(struct pcandev *dev);
int pcan_set_bus_state(struct pcandev *dev, enum pcanfd_status bus_state);
void pcan_copy_err_counters(struct pcandev *dev, struct pcanfd_rxmsg *pf);
int pcan_get_dev_features(struct pcandev *dev, u32 *features);

int pcan_handle_busoff(struct pcandev *dev, struct pcanfd_rxmsg *pf);
void pcan_handle_error_active(struct pcandev *dev, struct pcanfd_rxmsg *pf);
int pcan_handle_error_status(struct pcandev *dev, struct pcanfd_rxmsg *pf,
				int err_warning, int err_passive);
int pcan_handle_error_ctrl(struct pcandev *dev, struct pcanfd_rxmsg *pf,
				int err_ctrl);
void pcan_handle_error_msg(struct pcandev *dev, struct pcanfd_rxmsg *pf,
			int err_type, u8 err_code, int err_rx, int err_gen);
void pcan_handle_error_internal(struct pcandev *dev, struct pcanfd_rxmsg *pf,
				int err_internal);
void pcan_handle_error_protocol(struct pcandev *dev, struct pcanfd_rxmsg *pf,
				int err_protocol);

void pcan_soft_error_active(struct pcandev *dev);

int pcan_set_status_bit(struct pcandev *dev, u16 bits);
int pcan_clear_status_bit(struct pcandev *dev, u16 bits);

void pcan_cleanup_dev(struct pcandev *dev);

u16 sja1000_bitrate(u32 dwBitRate, u32 sample_pt, u32 sjw);

/* get bitrate in bps */
static inline u32 pcan_get_bps(u32 clk_Hz, struct pcan_bittiming *pbt)
{
	return clk_Hz / (pbt->brp * (1 + pbt->tseg1 + pbt->tseg2));
}

int pcan_init_rxmsg(struct pcandev *dev, struct pcanfd_rxmsg *rx,
		    __u16 type, __u32 id, __u32 flags);

int pcan_chardev_rx(struct pcandev *dev, struct pcanfd_rxmsg *cf);
int pcan_txfifo_get(struct pcandev *dev, struct pcanfd_txmsg *tx);

void pcan_sysfs_dev_node_create_ex(struct pcandev *dev, struct device *parent);
void pcan_sysfs_dev_node_destroy(struct pcandev *dev);

static inline void pcan_sysfs_dev_node_create(struct pcandev *dev)
{
	pcan_sysfs_dev_node_create_ex(dev, NULL);
}

void remove_dev_list(void);

#define pcanfd_dump_bittiming(pbt, c)	\
		__pcanfd_dump_bittiming(__FILE__, __LINE__, pbt, c)

static inline void __pcanfd_dump_bittiming(const char *fn, int l,
				struct pcan_bittiming *pbt, u32 clock_Hz)
{
	u32 btr = pbt->bitrate;
	char unit = ' ';

	if (btr >= 1000000) {
		btr /= 1000000;
		unit = 'M';
	} else if (btr >= 1000) {
		btr /= 1000;
		unit = 'k';
	}
	pr_info(DEVICE_NAME ": %s(%u): clk=%uMHz btr=%u%cbps sp=%u "
		"[BRP=%u TSEGx=%u/%u SJW=%u TQ=%u]\n",
		fn, l, clock_Hz/1000000, btr, unit, pbt->sample_point,
		pbt->brp, pbt->tseg1, pbt->tseg2, pbt->sjw, pbt->tq);
}

static inline u16 pcan_bittiming_to_btr0btr1(struct pcan_bittiming *pbt)
{
	u16 btr0btr1;

	btr0btr1 = ((pbt->sjw - 1) & 0x3) << 14;
	btr0btr1 |= ((pbt->brp - 1) & 0x3f) << 8;
	btr0btr1 |= (!!pbt->tsam) << 7;
	btr0btr1 |= ((pbt->tseg2 - 1) & 0x7) << 4;
	btr0btr1 |= ((pbt->tseg1 - 1) & 0xf);

	return btr0btr1;
}

int strtounit(char *str, u32 *pv, char *units);

const struct pcanfd_options *pcan_inherit_options_from(
				struct pcanfd_options *child_opts,
				const struct pcanfd_options *parent_opts);

int pcan_sysfs_add_attr(struct device *dev, struct attribute *attrs);
int pcan_sysfs_add_attrs(struct device *dev, struct attribute **attrs);
void pcan_sysfs_del_attr(struct device *dev, struct attribute *attrs);
void pcan_sysfs_del_attrs(struct device *dev, struct attribute **attrs);

#endif /* __PCAN_MAIN_H__ */
