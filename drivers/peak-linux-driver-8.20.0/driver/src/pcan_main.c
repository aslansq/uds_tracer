/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_main.c - the starting point of the driver,
 *               init and cleanup and proc interface
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
 *               Marcel Offermans <marcel.offermans@luminis.nl>
 *               Philipp Baer <philipp.baer@informatik.uni-ulm.de>
 *               Garth Zeglin <garthz@ri.cmu.edu>
 *               Harald Koenig <H.Koenig@science-computing.de>
 */
/* #define DEBUG */
/* #undef DEBUG */

#include "src/pcan_common.h"	/* must always be the 1st include */

/* #define KBUILD_MODNAME pcan */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/capability.h>
#include <linux/param.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
#include <asm/system.h>
#endif
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
/* if defined, create_proc_entry() is not used to create /proc/pcan */
#define CREATE_PROC_ENTRY_DEPRECATED
#endif

#ifdef DEBUG
#define DEBUG_PATCH
#define DEBUG_BUS_STATE
#define DEBUG_BUS_LOAD
#define DEBUG_RX_QUEUE
#define DEBUG_TS
#define DEBUG_ALLOC_DEV
#define DEBUG_OLD_API
#define DEBUG_INVALID_TS
#define DEBUG_CLEANUP
#define DEBUG_SYSFS
#define DEBUG_TRACE_DEFERRED
#define DEBUG_INVALID_BUS_STATE
#define DEBUG_TRACE
#else
//#define DEBUG_PATCH
//#define DEBUG_RX_QUEUE
//#define DEBUG_TS
//#define DEBUG_TS_HWTYPE	HW_USB_PRO_FD
//#define DEBUG_TS_HWTYPE	HW_USB_FD
//#define DEBUG_TS_HWTYPE	HW_PCIE_FD
//#define DEBUG_TS_HWTYPE	HW_USB
//#define DEBUG_TS_HWTYPE		HW_USB_PRO
//#define DEBUG_BUS_STATE
//#define DEBUG_BUS_LOAD
//#define DEBUG_ALLOC_DEV
//#define DEBUG_OLD_API
//#define DEBUG_INVALID_TS
//#define DEBUG_CLEANUP
//#define DEBUG_SYSFS
//#define DEBUG_TRACE_DEFERRED
//#define DEBUG_INVALID_BUS_STATE
//#define DEBUG_TRACE
#endif

#ifdef PCI_SUPPORT
#include "src/pcan_pci.h"	/* get support for PCAN-PCI */
#endif
#ifdef ISA_SUPPORT
#include "src/pcan_isa.h"	/* get support for PCAN-ISA and PCAN-104 */
#endif
#ifdef DONGLE_SUPPORT
#include "src/pcan_dongle.h"	/* get support for PCAN-Dongle */
#endif
#ifdef USB_SUPPORT
#include "src/pcan_usb_core.h"	/* get support for PCAN-USB */
#endif
#ifdef PCCARD_SUPPORT
#include "src/pcan_pccard.h"
#endif
#ifdef NETDEV_SUPPORT
#include "src/pcan_netdev.h"
#endif

#include "src/pcanfd_core.h"
#include "src/pcan_fifo.h"
#include "src/pcan_filter.h"
#include "src/pcan_sja1000.h"
#include "src/pcanfd_ucan.h"

/* if defined, create /sys/class/pcan tree */
#define SYSFS_SUPPORT

#ifndef NETDEV_SUPPORT

/* if defined, then sysfs attr "ts_mode" is writable: user can select timestamp
 * mode for a specific device via sysfs.
 * if not defined, ts_mode cannot be changed via sysfs. ts_mode can only be
 * set with deftsmode or the corresponding option.
 */
#define PCAN_SYSFS_TSMODE_RW

/* if defined, hw timestamps given to application are checked whether they
 * are valid, that is:
 * 1/ tv_usec must belong to [0..999999]
 * 2/ whole timestamp must not come from the future.
 */
#define PCAN_FIX_INVALID_TS
#endif

#ifdef PCAN_HANDLE_CLOCK_DRIFT
/* note: using 10^6 scale generates useless divisions: cooked count of us does 
 * not change in fine, while 10^4 creates a few dozens of us. difference.
 * note: when using _SHIFT value, then the driver scales the clock drift with
 * 2^_SHIFT so that it saves the multiplication.
 * note: the _SHIFT value should not be too large: having a too large _SCALE
 * value will generate lots of divisions in pcan_sync_decode() for a very amount
 * of µs of clock shift.
 */
#define PCAN_CLOCK_DRIFT_SCALE_SHIFT	17
#define PCAN_CLOCK_DRIFT_SCALE		(1<<(PCAN_CLOCK_DRIFT_SCALE_SHIFT))

#endif /* PCAN_HANDLE_CLOCK_DRIFT */

/* if defined, then time sync system is reset every PCAN_RESET_TIME_SYNC_PERIOD
 * calibration messages when no activity has been detected on the channel.
 * This allows to fix time_lag drift.
 */
#define PCAN_RESET_TIME_SYNC_PERIOD	10

#define DEFAULT_BTR0BTR1	CAN_BAUD_500K	/* defaults to 500 kbit/sec */
#define DEFAULT_DBITRATE	2000000		/* default data bitrate = 2M */

/* filled by module initialisation */
char *type[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
u32  io[8]    = {0, 0, 0, 0, 0, 0, 0, 0};
u8   irq[8]   = {0, 0, 0, 0, 0, 0, 0, 0};
u16  btr0btr1  = DEFAULT_BTR0BTR1;
char *bitrate = NULL;
char *dbitrate = NULL;

/* the global driver object, create it */
struct pcan_driver pcan_drv = {};

static u32 pcan_def_bitrate = 0;
static u32 pcan_def_dbitrate = DEFAULT_DBITRATE;

#ifdef SYSFS_SUPPORT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
/* some stuff to support SysFS coming with kernel 2.6 */
#include <linux/device.h>
#endif
#endif

/* build current driver config string for output in kernel log and procfs */
const char current_config[] = " "
#ifdef DEBUG
"[dbg] "
#endif
#ifdef MODVERSIONS
"[mod] "
#endif
#ifdef ISA_SUPPORT
"[isa] "
#endif
#ifdef PCI_SUPPORT
"[pci] "
#endif
#ifdef PCIEC_SUPPORT
"[pec] "
#endif
#ifdef DONGLE_SUPPORT
"[dng] "
#endif
#ifdef PARPORT_SUBSYSTEM
"[par] "
#endif
#ifdef USB_SUPPORT
"[usb] "
#endif
#ifdef PCCARD_SUPPORT
"[pcc] "
#endif
#ifdef NETDEV_SUPPORT
"[net] "
#endif
#ifndef NO_RT
"[rt] "
#endif
;

#ifndef NETDEV_SUPPORT

#define PCAN_DEV_RXQSIZE_MIN	2

/* 4194304 / 152 ~ 27594 with x86 */
#define PCAN_DEV_RXQSIZE_MAX	(KMALLOC_MAX_SIZE / sizeof(struct pcanfd_rxmsg))

extern ushort rxqsize;
extern unsigned char rxqprealloc;

#define PCANFD_OPT_HWTIMESTAMP_DEF	PCANFD_OPT_HWTIMESTAMP_MAX
#define PCANFD_OPT_HWTIMESTAMP_LOW	PCANFD_OPT_HWTIMESTAMP_OFF
#define PCANFD_OPT_HWTIMESTAMP_HIGH	PCANFD_OPT_HWTIMESTAMP_MAX-1

static ushort deftsmode = PCANFD_OPT_HWTIMESTAMP_DEF;	/* use def ts_mode */
module_param(deftsmode, ushort, 0644);
MODULE_PARM_DESC(deftsmode, " default ts mode");

static int _pcan_set_ts_mode(struct pcandev *dev, u32 ts_mode, bool fallback);

#define PCANFD_OPT_BUSLOADINDFREQ_DEF	500

static ushort defblperiod = PCANFD_OPT_BUSLOADINDFREQ_DEF;
module_param(defblperiod, ushort, 0644);
MODULE_PARM_DESC(defblperiod, " default bus load msg period (def="
			__stringify(PCANFD_OPT_BUSLOADINDFREQ_DEF) " ms.)");
#endif

#define PCANFD_DRIVER_CLOCK_DEF		CLOCK_REALTIME

ushort drvclkref = PCANFD_DRIVER_CLOCK_DEF;
module_param(drvclkref, ushort, 0644);
MODULE_PARM_DESC(drvclkref, " driver clock reference (def="
			__stringify(PCANFD_DRIVER_CLOCK_DEF) ")");

#define PCAN_DEV_TXQSIZE_MIN	2

/* 4194304 / 136 ~ 30840 with x86 */
#define PCAN_DEV_TXQSIZE_MAX	(KMALLOC_MAX_SIZE / sizeof(struct pcanfd_txmsg))

extern ushort txqsize;
extern unsigned char txqprealloc;

#define PCAN_DEV_DMA_MASK_DEF	64
#define PCAN_DEV_DMA_MASK_LOW	24
#define PCAN_DEV_DMA_MASK_HIGH	64

ushort dmamask = PCAN_DEV_DMA_MASK_DEF;

module_param(dmamask, ushort, 0644);
MODULE_PARM_DESC(dmamask, " ["
			__stringify(PCAN_DEV_DMA_MASK_LOW) ".."
			__stringify(PCAN_DEV_DMA_MASK_HIGH) "] (def="
			__stringify(PCAN_DEV_DMA_MASK_DEF) ")");

#define PCANFD_TX_HIWAT_MIN		5000
#define PCANFD_TX_HIWAT_MAX		10000
#ifdef NETDEV_SUPPORT
#define PCANFD_TX_HIWAT_DEF		8000
#else
#define PCANFD_TX_HIWAT_DEF		PCANFD_TX_HIWAT_MAX
#endif
ushort txqhiwat = PCANFD_TX_HIWAT_DEF;
module_param(txqhiwat, ushort, 0644);
MODULE_PARM_DESC(txqhiwat,
		 " Tx fifo hi-water ratio ["
		 __stringify(PCANFD_TX_HIWAT_MIN) ".."
		 __stringify(PCANFD_TX_HIWAT_MAX) "] def="
		 __stringify(PCANFD_TX_HIWAT_DEF) ")");

/* for procfs output the current_config is copied into this centered string */
char config[] = "*----------------------------------------------------------------------------";

#ifdef SYSFS_SUPPORT

/* linux < 2.6.27 use device_create_drvdata() */
#ifndef device_create_drvdata
#define	device_create_drvdata	device_create
#endif

static ssize_t show_u32(char *buf, u32 v)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", v);
}

static ssize_t show_int(char *buf, int v)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", v);
}

static ssize_t show_str(char *buf, char *str)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t show_pcan_devid(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->device_alt_num);
}

static ssize_t store_pcan_devid(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct pcandev *pdev = to_pcandev(dev);

	if (pdev->device_params) {
		char *endptr;
		int err;
		TPEXTRAPARAMS exp = {
			.nSubFunction = SF_SET_HCDEVICENO,
		};

		exp.func.dwSerialNumber = simple_strtoul(buf, &endptr, 0);
		if (*endptr != '\n')
			return -EINVAL;

		err = pdev->device_params(pdev, &exp);
		if (err)
			return err;
	}

	return count;
}

static ssize_t show_pcan_sn(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	TPEXTRAPARAMS exp = {
		.nSubFunction = SF_GET_SERIALNUMBER,
	};
	int err;

	if (!pdev->device_params)
		return -EOPNOTSUPP;

	err = pdev->device_params(pdev, &exp);
	if (err)
		return err;

	return show_u32(buf, exp.func.dwSerialNumber);
}

static ssize_t show_device_guid(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	int l = 0;

	if (pdev && pdev->adapter) {
		int i;

		/* RFC-4122 format: 8-4-4-4-12 */
		for (i = 0; i < sizeof(pdev->adapter->guid); i++) {
			switch (i) {
			case 4:
			case 6:
			case 8:
			case 10:
				if (l < PAGE_SIZE)
					buf[l++] = '-';
				fallthrough;
			default:
				l += snprintf(buf+l, PAGE_SIZE, "%02x",
					      pdev->adapter->guid[i]);
			}
		}
	}

	if (l < PAGE_SIZE)
		buf[l++] = '\n';

	return l;
}

static ssize_t show_pcan_mass_storage_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	u32 v;
	struct pcanfd_option opt = {
		.size = sizeof(v),
		.name = PCANFD_OPT_MAX,	/* trick to avoid copy_to_user() */
		.value = &v,
	};
	int err;

	if (!pdev->option[PCANFD_OPT_MASS_STORAGE_MODE].get) {
		pr_warn(DEVICE_NAME ": %s(L=%u): ABNORMAL NULL pointer\n",
			__func__, __LINE__);
		return -EOPNOTSUPP;
	}

	err = pdev->option[PCANFD_OPT_MASS_STORAGE_MODE].get(pdev, &opt, NULL);
	if (err)
		return err;

	return show_u32(buf, v);
}

static ssize_t store_pcan_mass_storage_mode(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct pcandev *pdev = to_pcandev(dev);
	u32 v;
	struct pcanfd_option opt = {
		.size = sizeof(v),
		.name = PCANFD_OPT_MAX,	/* trick to avoid copy_from_user() */
		.value = &v,
	};
	char *endptr;
	int err;

	if (!pdev->option[PCANFD_OPT_MASS_STORAGE_MODE].set) {
		pr_warn(DEVICE_NAME ": %s(L=%u): ABNORMAL NULL pointer\n",
			__func__, __LINE__);
		return -EOPNOTSUPP;
	}

	v = simple_strtoul(buf, &endptr, 0);
	if (*endptr != '\n')
		return -EINVAL;

	err = pdev->option[PCANFD_OPT_MASS_STORAGE_MODE].set(pdev, &opt, NULL);
	if (err)
		return err;

	return count;
}

static ssize_t show_pcan_identify(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	u32 v;
	struct pcanfd_option opt = {
		.size = sizeof(v),
		.name = PCANFD_OPT_MAX,	/* trick to avoid copy_to_user() */
		.value = &v,
	};
	int err;

	if (!pdev->option[PCANFD_OPT_FLASH_LED].get) {
		pr_warn(DEVICE_NAME ": %s(L=%u): ABNORMAL NULL pointer\n",
			__func__, __LINE__);
		return -EOPNOTSUPP;
	}

	err = pdev->option[PCANFD_OPT_FLASH_LED].get(pdev, &opt, NULL);
	if (err)
		return err;

	return show_u32(buf, v);
}

static ssize_t store_pcan_identify(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct pcandev *pdev = to_pcandev(dev);
	u32 v;
	struct pcanfd_option opt = {
		.size = sizeof(v),
		.name = PCANFD_OPT_MAX,	/* trick to avoid copy_from_user() */
		.value = &v,
	};
	char *endptr;
	int err;

	if (!pdev->option[PCANFD_OPT_FLASH_LED].set) {
		pr_warn(DEVICE_NAME ": %s(L=%u): ABNORMAL NULL pointer\n",
			__func__, __LINE__);
		return -EOPNOTSUPP;
	}

	v = simple_strtoul(buf, &endptr, 0);
	if (*endptr != '\n')
		return -EINVAL;

	err = pdev->option[PCANFD_OPT_FLASH_LED].set(pdev, &opt, NULL);
	if (err)
		return err;

	return count;
}

static ssize_t show_pcan_hwtype(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->wType);
}

static ssize_t show_pcan_minor(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return show_int(buf, to_pcandev(dev)->nMinor);
}

static ssize_t show_pcan_ts_mode(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	u32 tmp = pdev->ts_mode;

	if (pdev->flags & PCAN_DEV_TS_SOF)
		tmp |= PCANFD_OPT_HWTIMESTAMP_SOF;

	return show_u32(buf, tmp);
}

#ifdef PCAN_SYSFS_TSMODE_RW
static ssize_t store_pcan_ts_mode(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct pcandev *pdev = to_pcandev(dev);
	char *endptr;
	u32 v;
	int err;

	v = simple_strtoul(buf, &endptr, 0);
	if (*endptr != '\n')
		return -EINVAL;

	err = _pcan_set_ts_mode(pdev, v, false);
	if (err)
		return err;

	return count;
}
#endif

static ssize_t show_pcan_dev_name(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);

#ifdef NO_RT
	return snprintf(buf, PAGE_SIZE, "/dev/pcan%s%u\n",
			pdev->type, pdev->nMinor);
#else
	return snprintf(buf, PAGE_SIZE, "pcan%u\n",
			pdev->nMinor);
#endif
}

static ssize_t show_pcan_ctrlr_number(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);

	return show_int(buf, pcan_idx(pdev));
}

static ssize_t show_pcan_bitrate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.nominal.bitrate);
}

static ssize_t show_pcan_nsp(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf,
			to_pcandev(dev)->init_settings.nominal.sample_point);
}

static ssize_t show_pcan_nom_tq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.nominal.tq);
}

static ssize_t show_pcan_nom_brp(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.nominal.brp);
}

static ssize_t show_pcan_nom_tseg1(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.nominal.tseg1);
}

static ssize_t show_pcan_nom_tseg2(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.nominal.tseg2);
}

static ssize_t show_pcan_nom_sjw(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.nominal.sjw);
}

static ssize_t show_pcan_init_flags(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n",
					to_pcandev(dev)->init_settings.flags);
}

static ssize_t show_pcan_clock(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.clock_Hz);
}

static ssize_t show_pcan_bus_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->bus_state);
}

static ssize_t show_pcan_rx_err_cnt(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->rx_error_counter);
}

static ssize_t show_pcan_tx_err_cnt(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->tx_error_counter);
}

static ssize_t show_pcan_bus_load(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	return snprintf(buf, PAGE_SIZE, "%u.%02u\n",
			pdev->bus_load / 100, pdev->bus_load % 100);
}

/* only when dev->adapter is not NULL! */
static ssize_t show_pcan_adapter_number(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	return show_int(buf, pdev && pdev->adapter ? pdev->adapter->index : -1);
}

static ssize_t show_pcan_adapter_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	int l = 0;

	if (pdev && pdev->adapter)
		l += snprintf(buf+l, PAGE_SIZE, "%s", pdev->adapter->name);
	else
		l += snprintf(buf+l, PAGE_SIZE, "pdev=%p adapter=%p",
			pdev, (pdev) ? pdev->adapter : NULL);

	buf[l++] = '\n';

	return l;
}

static ssize_t show_pcan_adapter_partnum(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	int l = 0;

	if (pdev->adapter && pdev->adapter->part_num)
		l += snprintf(buf+l, PAGE_SIZE, "%s", pdev->adapter->part_num);

	buf[l++] = '\n';

	return l;
}

static ssize_t show_pcan_adapter_version(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	int l = 0;

	if (pdev->hw_ver && pdev->hw_ver->major >= 0) {
		l += snprintf(buf+l, PAGE_SIZE, "%u", pdev->hw_ver->major);
		if (pdev->hw_ver->minor >= 0) {
			l += snprintf(buf+l, PAGE_SIZE, ".%u",
					pdev->hw_ver->minor);

			if (pdev->hw_ver->subminor >= 0)
				l += snprintf(buf+l, PAGE_SIZE, ".%u",
						pdev->hw_ver->subminor);
		}

		if (l >= PAGE_SIZE)
			l = PAGE_SIZE - 2;
	}

	buf[l++] = '\n';

	return l;
}

/* /proc/pcan redundant */
static ssize_t show_pcan_type(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return show_str(buf, to_pcandev(dev)->type);
}

#ifdef NETDEV_SUPPORT
static ssize_t show_pcan_ndev(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	return show_str(buf, pdev->netdev ? pdev->netdev->name : "can?");
}
#else
static ssize_t show_rx_fifo_ratio(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	u32 fifo_ratio = pcan_fifo_ratio(&pdev->rx_fifo);
	return snprintf(buf, PAGE_SIZE, "%u.%02u\n",
			fifo_ratio / 100, fifo_ratio % 100);
}
#endif

static ssize_t show_pcan_base(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", to_pcandev(dev)->dwPort);
}

static ssize_t show_tx_fifo_ratio(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	u32 fifo_ratio = pcan_fifo_ratio(&pdev->tx_fifo);
	return snprintf(buf, PAGE_SIZE, "%u.%02u\n",
			fifo_ratio / 100, fifo_ratio % 100);
}

static ssize_t show_clk_drift(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->time_sync.clock_drift);
}

#ifdef PCAN_FIX_INVALID_TS
static ssize_t show_ts_fixed(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->time_sync.ts_fixed);
}

static ssize_t show_time_lag(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->time_sync.evt_time_lag);
}
#endif

static ssize_t show_pcan_irq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->wIrq);
}

static ssize_t show_pcan_btr0btr1(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	u16 dev_btr0btr1 = (pdev->sysclock_Hz == 8*MHz) ?
		pcan_bittiming_to_btr0btr1(&pdev->init_settings.nominal) :
		sja1000_bitrate(pdev->init_settings.nominal.bitrate,
				pdev->init_settings.nominal.sample_point,
				pdev->init_settings.nominal.sjw);

	return snprintf(buf, PAGE_SIZE, "0x%04x\n", dev_btr0btr1);
}

static ssize_t show_pcan_read(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
#ifdef NETDEV_SUPPORT
	struct net_device_stats *stats = (pdev->netdev) ?
				pcan_netdev_get_stats(pdev->netdev) : NULL;
	u32 dev_read = (stats) ? stats->rx_packets : 0;

#else
	u32 dev_read = pdev->rx_fifo.dwTotal;
#endif
	return show_u32(buf, dev_read);
}

static ssize_t show_pcan_write(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
#ifdef NETDEV_SUPPORT
	struct net_device_stats *stats = (pdev->netdev) ?
				pcan_netdev_get_stats(pdev->netdev) : NULL;
	u32 dev_write = (stats) ? stats->tx_packets : 0;

#else
	u32 dev_write = pdev->tx_fifo.dwTotal;
#endif
	return show_u32(buf, dev_write);
}

static ssize_t show_pcan_rx_frames_counter(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->rx_frames_counter);
}

static ssize_t show_pcan_tx_frames_counter(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->tx_frames_counter);
}

static ssize_t show_pcan_irqs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	return show_u32(buf, pdev->rx_irq_counter +
			     pdev->tx_irq_counter);
}

static ssize_t show_pcan_rx_irqs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->rx_irq_counter);
}

static ssize_t show_pcan_tx_irqs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->tx_irq_counter);
}

static ssize_t show_pcan_errors(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->dwErrorCounter);
}

static ssize_t show_pcan_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n",
						to_pcandev(dev)->wCANStatus);
}

/* Note: 2nd arg is sysfs entry name */
static PCAN_DEVICE_ATTR_RW(devid, devid, show_pcan_devid, store_pcan_devid);
static PCAN_DEVICE_ATTR_RW(mass_storage_mode, mass_storage_mode,\
			   show_pcan_mass_storage_mode,\
			   store_pcan_mass_storage_mode);
static PCAN_DEVICE_ATTR_RW(led, led, show_pcan_identify, store_pcan_identify);
static PCAN_DEVICE_ATTR(hwtype, hwtype, show_pcan_hwtype);
#ifdef PCAN_SYSFS_TSMODE_RW
static PCAN_DEVICE_ATTR_RW(ts_mode, ts_mode, show_pcan_ts_mode, \
			   store_pcan_ts_mode);
#else
static PCAN_DEVICE_ATTR(ts_mode, ts_mode, show_pcan_ts_mode);
#endif
static PCAN_DEVICE_ATTR(serialno, serialno, show_pcan_sn);
static PCAN_DEVICE_ATTR(minor, minor, show_pcan_minor);
static PCAN_DEVICE_ATTR(dev_name, dev_name, show_pcan_dev_name);
static PCAN_DEVICE_ATTR(ctrlr_number, ctrlr_number, show_pcan_ctrlr_number);
static PCAN_DEVICE_ATTR(bitrate, nom_bitrate, show_pcan_bitrate);
static PCAN_DEVICE_ATTR(sample_point, nom_sample_point, show_pcan_nsp);
static PCAN_DEVICE_ATTR(nom_tq, nom_tq, show_pcan_nom_tq);
static PCAN_DEVICE_ATTR(nom_brp, nom_brp, show_pcan_nom_brp);
static PCAN_DEVICE_ATTR(nom_tseg1, nom_tseg1, show_pcan_nom_tseg1);
static PCAN_DEVICE_ATTR(nom_tseg2, nom_tseg2, show_pcan_nom_tseg2);
static PCAN_DEVICE_ATTR(nom_sjw, nom_sjw, show_pcan_nom_sjw);
static PCAN_DEVICE_ATTR(init_flags, init_flags, show_pcan_init_flags);
static PCAN_DEVICE_ATTR(clock, clock, show_pcan_clock);
static PCAN_DEVICE_ATTR(bus_state, bus_state, show_pcan_bus_state);
/* /proc/pcan redundant */
static PCAN_DEVICE_ATTR(type, type, show_pcan_type);
#ifdef NETDEV_SUPPORT
static PCAN_DEVICE_ATTR(ndev, ndev, show_pcan_ndev);
#else
static PCAN_DEVICE_ATTR(rx_fifo_ratio, rx_fifo_ratio, show_rx_fifo_ratio);
#endif
static PCAN_DEVICE_ATTR(base, base, show_pcan_base);
static PCAN_DEVICE_ATTR(irq, irq, show_pcan_irq);
static PCAN_DEVICE_ATTR(btr0btr1, btr0btr1, show_pcan_btr0btr1);
static PCAN_DEVICE_ATTR(read, read, show_pcan_read);
static PCAN_DEVICE_ATTR(write, write, show_pcan_write);
static PCAN_DEVICE_ATTR(errors, errors, show_pcan_errors);
static PCAN_DEVICE_ATTR(rx_frames_counter, rx_frames_counter, show_pcan_rx_frames_counter);
static PCAN_DEVICE_ATTR(tx_frames_counter, tx_frames_counter, show_pcan_tx_frames_counter);
static PCAN_DEVICE_ATTR(irqs, irqs, show_pcan_irqs);
static PCAN_DEVICE_ATTR(rx_irqs, rx_irqs, show_pcan_rx_irqs);
static PCAN_DEVICE_ATTR(tx_irqs, tx_irqs, show_pcan_tx_irqs);
static PCAN_DEVICE_ATTR(status, status, show_pcan_status);
static PCAN_DEVICE_ATTR(tx_fifo_ratio, tx_fifo_ratio, show_tx_fifo_ratio);
static PCAN_DEVICE_ATTR(clk_drift, clk_drift, show_clk_drift);
#ifdef PCAN_FIX_INVALID_TS
static PCAN_DEVICE_ATTR(ts_fixed, ts_fixed, show_ts_fixed);
static PCAN_DEVICE_ATTR(time_lag, time_lag, show_time_lag);
#endif
static struct attribute *pcan_dev_sysfs_attrs[] = {
	&pcan_dev_attr_hwtype.attr,
	&pcan_dev_attr_ts_mode.attr,
	&pcan_dev_attr_serialno.attr,
	&pcan_dev_attr_minor.attr,
	&pcan_dev_attr_dev_name.attr,
	&pcan_dev_attr_ctrlr_number.attr,
	&pcan_dev_attr_bitrate.attr,
	&pcan_dev_attr_sample_point.attr,
	&pcan_dev_attr_nom_tq.attr,
	&pcan_dev_attr_nom_brp.attr,
	&pcan_dev_attr_nom_tseg1.attr,
	&pcan_dev_attr_nom_tseg2.attr,
	&pcan_dev_attr_nom_sjw.attr,
	&pcan_dev_attr_init_flags.attr,
	&pcan_dev_attr_clock.attr,
	&pcan_dev_attr_bus_state.attr,
	/* /proc/pcan redundant */
	&pcan_dev_attr_type.attr,
#ifdef NETDEV_SUPPORT
	&pcan_dev_attr_ndev.attr,
#else
	&pcan_dev_attr_rx_fifo_ratio.attr,
#endif
	&pcan_dev_attr_base.attr,
	&pcan_dev_attr_irq.attr,
	&pcan_dev_attr_btr0btr1.attr,
	&pcan_dev_attr_read.attr,
	&pcan_dev_attr_write.attr,
	&pcan_dev_attr_rx_frames_counter.attr,
	&pcan_dev_attr_tx_frames_counter.attr,
	&pcan_dev_attr_errors.attr,
	&pcan_dev_attr_irqs.attr,
	&pcan_dev_attr_rx_irqs.attr,
	&pcan_dev_attr_tx_irqs.attr,
	&pcan_dev_attr_status.attr,
	&pcan_dev_attr_tx_fifo_ratio.attr,
	&pcan_dev_attr_clk_drift.attr,
#ifdef PCAN_FIX_INVALID_TS
	&pcan_dev_attr_ts_fixed.attr,
	&pcan_dev_attr_time_lag.attr,
#endif
	NULL
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
static struct attribute_group pcan_dev_attrs_group = {
	/* NULL ".name" => attrs will be created under pcanxxx node 
	 * .name = "pcan-dev", 
	 */
	.attrs = pcan_dev_sysfs_attrs,
};
static const struct attribute_group *pcan_dev_attrs_groups[] = {
	&pcan_dev_attrs_group,
	NULL,
};
#endif

static ssize_t show_pcan_dbitrate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.data.bitrate);
}

static ssize_t show_pcan_dsp(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.data.sample_point);
}

static ssize_t show_pcan_data_tq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.data.tq);
}

static ssize_t show_pcan_data_brp(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.data.brp);
}

static ssize_t show_pcan_data_tseg1(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.data.tseg1);
}

static ssize_t show_pcan_data_tseg2(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.data.tseg2);
}

static ssize_t show_pcan_data_sjw(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_u32(buf, to_pcandev(dev)->init_settings.data.sjw);
}

static PCAN_DEVICE_ATTR(guid, device_guid, show_device_guid);

static PCAN_DEVICE_ATTR(dbitrate, data_bitrate, show_pcan_dbitrate);
static PCAN_DEVICE_ATTR(dsample_point, data_sample_point, show_pcan_dsp);
static PCAN_DEVICE_ATTR(data_tq, data_tq, show_pcan_data_tq);
static PCAN_DEVICE_ATTR(data_brp, data_brp, show_pcan_data_brp);
static PCAN_DEVICE_ATTR(data_tseg1, data_tseg1, show_pcan_data_tseg1);
static PCAN_DEVICE_ATTR(data_tseg2, data_tseg2, show_pcan_data_tseg2);
static PCAN_DEVICE_ATTR(data_sjw, data_sjw, show_pcan_data_sjw);

static struct attribute *pcan_dev_sysfs_fd_attrs[] = {
	&pcan_dev_attr_dbitrate.attr,
	&pcan_dev_attr_dsample_point.attr,
	&pcan_dev_attr_data_tq.attr,
	&pcan_dev_attr_data_brp.attr,
	&pcan_dev_attr_data_tseg1.attr,
	&pcan_dev_attr_data_tseg2.attr,
	&pcan_dev_attr_data_sjw.attr,

	NULL
};

static PCAN_DEVICE_ATTR(bus_load, bus_load, show_pcan_bus_load);
static PCAN_DEVICE_ATTR(rx_err_cnt, rx_error_counter, show_pcan_rx_err_cnt);
static PCAN_DEVICE_ATTR(tx_err_cnt, tx_error_counter, show_pcan_tx_err_cnt);

static struct attribute *pcan_dev_sysfs_err_cnt_attrs[] = {
	&pcan_dev_attr_rx_err_cnt.attr,
	&pcan_dev_attr_tx_err_cnt.attr,

	NULL
};

static PCAN_DEVICE_ATTR(adapter_number, adapter_number,\
			show_pcan_adapter_number);
static PCAN_DEVICE_ATTR(adapter_name, adapter_name, show_pcan_adapter_name);
static PCAN_DEVICE_ATTR(adapter_partnum, adapter_partnum, show_pcan_adapter_partnum);
static PCAN_DEVICE_ATTR(adapter_version, adapter_version,\
			show_pcan_adapter_version);

static struct attribute *pcan_dev_sysfs_adapter_attrs[] = {
	&pcan_dev_attr_adapter_number.attr,
	&pcan_dev_attr_adapter_name.attr,
	&pcan_dev_attr_adapter_partnum.attr,
	&pcan_dev_attr_adapter_version.attr,

	NULL,
};

/* Create sysfs entries that need the whole devices list of an adapter to be
 * probed/created first.
 */
static void pcan_sysfs_dev_node_add_adapter(struct pcan_adapter *adapter)
{
	struct list_head *ptr;

#ifdef HANDLE_HOTPLUG
	unsigned long flags;

	pcan_lock_get_irqsave(&pcan_drv.devices_lock, flags);
#endif

	list_for_each(ptr, &pcan_drv.devices) {
		struct pcandev *dev = list_entry(ptr, struct pcandev, list_dev);

		if (adapter != dev->adapter)
			continue;

		if (!dev->sysfs_dev)
			continue;

		/* PCAN-USB X6: CAN5+CAN6 can run FW 3.5.1 while the 2 other
		 * USB interfaces run an older one! This means that GUID might
		 * be flashed only in the last interface. But it should be
		 * visible for the 3 ones!
		 */
		if (dev->adapter->flags & PCAN_ADAPTER_GUID)
			pcan_sysfs_add_attr(dev->sysfs_dev,
					    &pcan_dev_attr_guid.attr);
	}

#ifdef HANDLE_HOTPLUG
	pcan_lock_put_irqrestore(&pcan_drv.devices_lock, flags);
#endif
}

/* create a device node to export devices properties into /sys/class/pcan tree.
 */
void pcan_sysfs_dev_node_create_ex(struct pcandev *dev, struct device *parent)
{
	char tmp[32];

	/* tinker my device node name, eg. "pcanpci%d" */
	snprintf(tmp, sizeof(tmp), DEVICE_NAME "%s%s", dev->type, "%u");

#if defined(DEBUG_TRACE) || defined(DEBUG_SYSFS)
	pr_info(DEVICE_NAME ": %s(%p=%s, %d, %d)\n",
		__func__, dev, tmp, dev->nMajor, dev->nMinor);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
	/* Should use WAIT_FOR key in Udev rules... */
	dev->sysfs_dev = device_create_drvdata(pcan_drv.class, parent,
				MKDEV(dev->nMajor, dev->nMinor),
				dev, tmp, dev->nMinor);

	if (!IS_ERR(dev->sysfs_dev)) {
		pcan_sysfs_add_attrs(dev->sysfs_dev, pcan_dev_sysfs_attrs);
#else
	/* since 3.11, it is possible to add attrs when creating the device
	 * node, that is *BEFORE* the UEVENT is being sent to userspace!
	 * Doing this, Udev rules does not need of WAIT_FOR key anymore!
	 */
	dev->sysfs_dev = device_create_with_groups(pcan_drv.class, parent,
				MKDEV(dev->nMajor, dev->nMinor),
				dev, pcan_dev_attrs_groups, tmp, dev->nMinor);

	if (!IS_ERR(dev->sysfs_dev)) {
#endif /* KERNEL_VERSION(3, 11, 0) */

#ifdef DEBUG_SYSFS
		pr_info(DEVICE_NAME ": %s(%p=\"%s\")\n", __func__, dev,
			dev->sysfs_dev->kobj.name);
#endif

		if (dev->features & PCAN_DEV_FD_RDY)
			pcan_sysfs_add_attrs(dev->sysfs_dev,
						pcan_dev_sysfs_fd_attrs);

		if (dev->features & PCAN_DEV_BUSLOAD_RDY)
			pcan_sysfs_add_attr(dev->sysfs_dev,
						&pcan_dev_attr_bus_load.attr);

		if (dev->option[PCANFD_OPT_DEVICE_ID].get)
			pcan_sysfs_add_attr(dev->sysfs_dev,
						&pcan_dev_attr_devid.attr);

		if (dev->features & PCAN_DEV_ERRCNT_RDY)
			pcan_sysfs_add_attrs(dev->sysfs_dev,
						pcan_dev_sysfs_err_cnt_attrs);

		if (dev->features & PCAN_DEV_MSD_RDY)
			pcan_sysfs_add_attr(dev->sysfs_dev,
					&pcan_dev_attr_mass_storage_mode.attr);

		if (dev->device_identify)
			pcan_sysfs_add_attr(dev->sysfs_dev,
					&pcan_dev_attr_led.attr);

		if (dev->sysfs_attrs)
			pcan_sysfs_add_attrs(dev->sysfs_dev, dev->sysfs_attrs);

		/* these attrs are are not used with Udev rules... */
		pcan_sysfs_add_attrs(dev->sysfs_dev,
				     pcan_dev_sysfs_adapter_attrs);

		/* create/update siblings with adapter specific entries if
		 * we are creating the last device of the adapter
		 */
		if (pcan_idx(dev) == (dev->adapter->can_count - 1))
			pcan_sysfs_dev_node_add_adapter(dev->adapter);
	} else {
#ifdef DEBUG_SYSFS
		pr_info(DEVICE_NAME ": %s(): device_create() failure err=%ld\n",
			__func__, (long )IS_ERR(dev->sysfs_dev));
#endif
		 dev->sysfs_dev = NULL;
	}
}

/* destroy a UDEV allocated device node */
void pcan_sysfs_dev_node_destroy(struct pcandev *dev)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_SYSFS)
	pr_info(DEVICE_NAME ": %s(%p=\"%s\")\n", __func__, dev,
		(dev->sysfs_dev) ? dev->sysfs_dev->kobj.name : "NULL");
#endif
	if (!dev->sysfs_dev)
		return;

	if (dev->sysfs_attrs)
		pcan_sysfs_del_attrs(dev->sysfs_dev, dev->sysfs_attrs);

	if (dev->device_identify)
		pcan_sysfs_del_attr(dev->sysfs_dev, &pcan_dev_attr_led.attr);

	if (dev->features & PCAN_DEV_MSD_RDY)
		pcan_sysfs_del_attr(dev->sysfs_dev,
					&pcan_dev_attr_mass_storage_mode.attr);

	if (dev->features & PCAN_DEV_ERRCNT_RDY)
		pcan_sysfs_del_attrs(dev->sysfs_dev,
					pcan_dev_sysfs_err_cnt_attrs);

	if (dev->option[PCANFD_OPT_DEVICE_ID].get)
		pcan_sysfs_del_attr(dev->sysfs_dev,
					&pcan_dev_attr_devid.attr);

	if (dev->features & PCAN_DEV_BUSLOAD_RDY)
		pcan_sysfs_del_attr(dev->sysfs_dev,
					&pcan_dev_attr_bus_load.attr);

	if (dev->features & PCAN_DEV_FD_RDY)
		pcan_sysfs_del_attrs(dev->sysfs_dev,
					pcan_dev_sysfs_fd_attrs);

	if (dev->adapter->flags & PCAN_ADAPTER_GUID)
		pcan_sysfs_del_attr(dev->sysfs_dev,
					&pcan_dev_attr_guid.attr);

	pcan_sysfs_del_attrs(dev->sysfs_dev, pcan_dev_sysfs_adapter_attrs);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
	pcan_sysfs_del_attrs(dev->sysfs_dev, pcan_dev_sysfs_attrs);
#endif
	device_destroy(pcan_drv.class, MKDEV(dev->nMajor, dev->nMinor));

	dev->sysfs_dev = NULL;
}

#else /* SYSFS_SUPPORT */

void pcan_sysfs_dev_node_create_ex(struct pcandev *dev, struct device *parent)
{
	dev->sysfs_dev = NULL;
}

void pcan_sysfs_dev_node_destroy(struct pcandev *dev)
{
}

#endif /* SYSFS_SUPPORT */

#ifdef NO_RT
#include "pcan_main_linux.c"
#else
#include "pcan_main_rt.c"
#endif

#define DUMP_WIDTH	16
#ifdef DEBUG
#define DUMP_OUT	KERN_DEBUG
#else
#define DUMP_OUT	KERN_INFO
#endif

/*
 * void dump_mem(const char *prompt, const void *p, int l)
 */
void dump_mem(const char *prompt, const void *p, int l)
{
	printk(DUMP_OUT "%s: dumping %s (%d bytes):\n",
		DEVICE_NAME, prompt ? prompt : "memory", l);
	print_hex_dump(DUMP_OUT, DEVICE_NAME ": ", DUMP_PREFIX_NONE,
		       DUMP_WIDTH, 1, p, l, false);
}

#ifndef NETDEV_SUPPORT

void pcan_sync_init(struct pcandev *dev)
{
	memset(&dev->time_sync, '\0', sizeof(dev->time_sync));
}

/*
 * fill the pqm->msg.timestamp field from pqm->hwtv to give to user.
 *
 * This function is called from userland. Therefore, it can change the saved
 * timestamsp according to CURRENT dev->ts_mode (in case it has been changed
 * between the moment the event has been saved and the moment it is read).
 */
struct pcanfd_msg *pcan_sync_timestamps(struct pcandev *dev,
					struct pcanfd_rxmsg *pqm)
{
	struct timeval now;
	int fix_ts = 0;
	struct pcanfd_msg *pm = &pqm->msg;
	int s = 1;
	u64 dus;
	signed long dtv;

	/* fix hw timestamp */
	pm->flags |= PCANFD_TIMESTAMP|PCANFD_HWTIMESTAMP;

	/* 		pqm->ts_mode	dev->ts_mode
	 * -----------------------------------------
	 * _OFF		(2)		x
	 * _ON		x		x
	 * _COOKED	x		x
	 * _RAW		(2)		(1)
	 */
	switch (dev->ts_mode) {
	case PCANFD_OPT_HWTIMESTAMP_RAW:

		/* (1): if dev->ts_mode is RAW, we can give the raw hw
		 * timestamps, whatever pqm->hwtv.ts_mode was at push time:
		 */
		pm->timestamp.tv_usec = do_div(pqm->hwtv.ts_us, USEC_PER_SEC);
		pm->timestamp.tv_sec = (__kernel_time_t )pqm->hwtv.ts_us;

#ifdef DEBUG_TS
#ifdef DEBUG_TS_HWTYPE
		if (dev->wType == DEBUG_TS_HWTYPE)
#endif
			pr_info(DEVICE_NAME
				": RAW CAN%u %llu %ld.%06ld\n",
				pcan_idx(dev)+1, pqm->hwtv.ts_us,
				pm->timestamp.tv_sec, pm->timestamp.tv_usec);
#endif
		return pm;

	default:
		break;
	}

	/* Init timestamp with last host (sync) time */
	pm->timestamp = pqm->hwtv.tv;

	switch (pqm->hwtv.ts_mode) {
	case PCANFD_OPT_HWTIMESTAMP_RAW:
	case PCANFD_OPT_HWTIMESTAMP_OFF:

		/* (2): we only have event host time =>
		 * don't set PCANFD_HWTIMESTAMP so user will know that it's
		 * host time only.
		 */
		pm->flags &= ~PCANFD_HWTIMESTAMP;

#ifdef DEBUG_TS
#ifdef DEBUG_TS_HWTYPE
		if (dev->wType == DEBUG_TS_HWTYPE)
#endif
			pr_info(DEVICE_NAME
				": OFF CAN%u %llu %ld.%06ld\n",
				pcan_idx(dev)+1, pqm->hwtv.ts_us,
				pm->timestamp.tv_sec, pm->timestamp.tv_usec);
#endif

		return pm;

	default:
		break;
	}

	/* PCANFD_HWTIMESTAMP is always set now: whever dev->ts_mode is, user
	 * will now that read timestamp is made of hw timestamps.
	 *
	 * ts_us = hw timestamp of the event
	 * tv_us = hw timestamp of last sync
	 * => dus = count of hw µs since last sync
	 *
	 * note: PCIe card sync ts is always >= event ts while
	 * USB CAN sync ts is always <= event ts
	 *
	 * note: because the sync might contain a greater timestamp than
	 * then event timestamp, should check the values before diff:
	 */
	if (unlikely(pqm->hwtv.ts_us < pqm->hwtv.tv_us)) {
		dus = pqm->hwtv.tv_us - pqm->hwtv.ts_us;
		s = -1;
	} else {
		dus = pqm->hwtv.ts_us - pqm->hwtv.tv_us;
	}

	/* Note: dus == 0 means that this event is the first resync'ed */
	if (!dus) {
		dev->time_sync.evt_time_lag = 0;

#ifdef PCAN_HANDLE_CLOCK_DRIFT
	//} else if (pqm->hwtv.ts_mode == PCANFD_OPT_HWTIMESTAMP_COOKED &&
	} else if (dev->ts_mode == PCANFD_OPT_HWTIMESTAMP_COOKED &&
	    pqm->hwtv.clock_drift &&
	    pqm->hwtv.clock_drift != PCAN_CLOCK_DRIFT_SCALE) {

		dus <<= PCAN_CLOCK_DRIFT_SCALE_SHIFT;

		/* - The smaller clock_drift is, the larger dus goes:
		 *   For example, a 60 s hardware dus becomes:
		 *
		 *   clock_drift	dus (host time)
		 *   131069		60.001.373 us
		 *   131070		60.000.915 us
		 *   131071		60.000.457 us
		 *
		 *   This means that roughly 1/2 ms per mn can be lost (or win)
		 *   if the clock drift is not accurate...
		 */
		do_div(dus, pqm->hwtv.clock_drift);
#endif
	}

	/* add number of us since last sync as well as measured time lag
	 * (the larger evt_time_lag is, the further back in time we go)
	 */
	dtv = s * dus - dev->time_sync.evt_time_lag;

	/* timeval_add_us() returns valid tv_sec and tv_usec */
	timeval_add_us(&pm->timestamp, dtv);

#ifdef DEBUG_TS
#ifdef DEBUG_TS_HWTYPE
	if (dev->wType == DEBUG_TS_HWTYPE)
#endif
		pr_info(DEVICE_NAME
			": COOK CAN%u %llu %ld.%06ld\n",
			pcan_idx(dev)+1, pqm->hwtv.ts_us,
			pm->timestamp.tv_sec, pm->timestamp.tv_usec);
#endif

	pcan_gettimeofday(&now);
	dtv = timeval_diff(&pm->timestamp, &now);
	if (dtv > 0) {

		fix_ts++;

		/* If the timestamp is more than 100 µs from the future, then
		 * it is fixed to now and the PCANFD_HWTIMESTAMP flag is
		 * removed. Otherwise, it is not.
		 */
		if (dtv >= 100) {
#ifdef DEBUG_INVALID_TS
			pr_info(DEVICE_NAME ": WARNING: ts too far from now "
				"(%ld us)\n", dtv);
#endif
			fix_ts++;
		}

		dev->time_sync.evt_time_lag += dtv;

	/* not safe to keep a too large time_lag. This version decrements it
	 * each time a timestamp is ok.
	 */
	} else if (dev->time_sync.evt_time_lag >= 10) {
		dev->time_sync.evt_time_lag -= 10;
	}

	if (fix_ts) {

		if (fix_ts > 1) {
#ifdef DEBUG_INVALID_TS
			pr_err(DEVICE_NAME ": WARNING: %s ts fixed(%d) from: "
			       "%ld.%06ld to: %ld.%06ld (now)\n",
				dev->adapter->name, fix_ts,
				pm->timestamp.tv_sec,
				pm->timestamp.tv_usec,
				now.tv_sec, now.tv_usec);

			pr_info(DEVICE_NAME ": ts[base=%llu off=%llu cd=%lu]\n",
				pqm->hwtv.tv_us, pqm->hwtv.ts_us,
				pqm->hwtv.clock_drift);
#endif
			pm->flags &= ~PCANFD_HWTIMESTAMP;
			dev->time_sync.ts_fixed++;
		}

		pm->timestamp = now;

#ifdef DEBUG_TS
#ifdef DEBUG_TS_HWTYPE
		if (dev->wType == DEBUG_TS_HWTYPE)
#endif
			pr_info(DEVICE_NAME ": FIX CAN%u %llu %ld.%06ld %u\n",
				pcan_idx(dev)+1, pqm->hwtv.ts_us,
				pm->timestamp.tv_sec,
				pm->timestamp.tv_usec,
				fix_ts);
#endif /* DEBUG_TS */
	}

	return pm;
}

int pcan_sync_decode(struct pcandev *dev, u32 ts_low, u32 ts_high,
						struct pcan_timeval *hwtv)
{
	hwtv->ts_mode = dev->ts_mode;
	hwtv->ts_us = ((u64 )ts_high << 32) | ts_low;

#ifdef DEBUG_TS
#ifdef DEBUG_TS_HWTYPE
	if (dev->wType == DEBUG_TS_HWTYPE)
#endif
		pr_info(DEVICE_NAME
			": DEC CAN%u %llu %llu %u\n",
			pcan_idx(dev)+1,
			hwtv->ts_us, dev->time_sync.evt_base_us,
			hwtv->ts_mode);
#endif

	switch (hwtv->ts_mode) {
	default:
#ifdef PCAN_RESET_TIME_SYNC_PERIOD
		/* this is a proof of activity */
		dev->time_sync.cm_inactivity_count = 0;
#endif
		hwtv->tv_us = dev->time_sync.evt_base_us;

		/* sync is not started yet: use host time instead */
		if (!hwtv->tv_us) {

			hwtv->tv_us = dev->time_sync.evt_base_us = hwtv->ts_us;

			pcan_gettimeofday(&dev->time_sync.evt_base_tv);
		}

		/* otherwise, save all the necessary stuff for user to compute
		 * host (cooked) timestamp later...
		 */
		hwtv->tv = dev->time_sync.evt_base_tv;
		hwtv->clock_drift = dev->time_sync.clock_drift;

		return 1;

	case PCANFD_OPT_HWTIMESTAMP_RAW:
		/* keep a trace of the (host) time for further use in case
		 * dev->ts_mode changed meanwhile...
		 */
		pcan_gettimeofday(&hwtv->tv);
		return 1;

	case PCANFD_OPT_HWTIMESTAMP_OFF:
		pcan_gettimeofday(&hwtv->tv);
		break;
	}

	hwtv->ts_mode = PCANFD_OPT_HWTIMESTAMP_OFF;

	return 0;
}

/* called to synchronize host time and hardware time
 * Note:
 * - USB-FD devices sync every 1s.
 * - PCIe-FD devices sync every 1,2 ms
 */
int pcan_sync_times(struct pcandev *dev, u32 ts_low, u32 ts_high, int tv_off)
{
	int err = 0;
	long dts_us = 0, dtv_us = 0;
	struct pcan_time_sync now = {
		.ts_us = ((u64 )ts_high << 32) + ts_low,
		.clock_drift = dev->time_sync.clock_drift,

		.evt_base_us = dev->time_sync.evt_base_us,
		.evt_base_tv = dev->time_sync.evt_base_tv,
		.evt_time_lag = dev->time_sync.evt_time_lag,
		.cm_inactivity_count = dev->time_sync.cm_inactivity_count + 1,
		.ts_fixed = dev->time_sync.ts_fixed,
	};

	pcan_gettimeofday_ex(&now.tv, &now.tv_ns);

	if (!dev->time_sync.ts_us) {

		/* get host time between substract any host time offset */
		timeval_add_us(&now.tv, tv_off);

		now.clock_drift = PCAN_CLOCK_DRIFT_SCALE;
		goto lbl_reset;
	}

	/* WARNING: CLOCK_REALTIME may generate negative dtv_us!!! */
	dtv_us = timeval_to_us(&now.tv) - timeval_to_us(&dev->time_sync.tv);
	if (dtv_us <= 0) {

		/* in that case, do ignore that sync tick so that next one
		 * will hopefully get a greater now.tv value...
		 */
		err = 1;
		goto lbl_exit;
	}

	/* do clock_drift computation as soon as possible */
	/* dts_us = count of µs in hardware time between 2x calls */
	/* be sure of this (and protect from several calls with
	 * same ts_high.ts_low that would change value of dev->time_sync.tv)
	 */
	dts_us = now.ts_us - dev->time_sync.ts_us;
	if (dts_us < 0) {
#ifdef DEBUG
		/* silently discard that call when calibration messages are
		 * ignored (<=> now.ts_us = 0). In this case, sync is done on
		 * events timestamps only. clock_drift is always computed.
		 */
		if (now.ts_us)
			pr_warn(DEVICE_NAME ": %s CAN%u: abnormal sync times: "
				"ts=%llu < ts=%llu\n",
				dev->adapter->name, pcan_idx(dev)+1,
				now.ts_us, dev->time_sync.ts_us);
#endif
		err = 2;
		goto lbl_exit;
	}

	/* get host time between each sync and substract any host time offset */
	if (unlikely(tv_off < 0)) {
		timeval_add_us(&now.tv, tv_off);
		dtv_us += tv_off;
	}

	if (dtv_us <= 0) {

		if (dtv_us < 0)
			pr_warn(DEVICE_NAME
				": %s CAN%u: abnormal sync times: "
				"tv=%ld.%06ld < tv=%ld.%06ld dtv_us=%ld "
				"tv_off=%d. Did host time change?\n",
				dev->adapter->name, pcan_idx(dev)+1,
				now.tv.tv_sec, now.tv.tv_usec,
				dev->time_sync.tv.tv_sec,
				dev->time_sync.tv.tv_usec,
				dtv_us, tv_off);

		err = 3;
		goto lbl_exit;
	}

#ifdef PCAN_HANDLE_CLOCK_DRIFT
	/* clock_drift computation is critial:
	 *
	 * A 60 s hardware difference time can change up to ~1 ms if the clock
	 * drift differs from 131072 to 131070:
	 *
	 *   clock_drift	dus (host time)
	 *   131069		60.001.373 us
	 *   131070		60.000.915 us
	 *   131071		60.000.457 us
	 *   131072		60.000.000 us
	 *
	 * Therefore, should reset counters used to compute clock drift
	 * periodically:
	 */
	if (dev->time_sync.ttv_us > 120*USEC_PER_SEC) {
		dev->time_sync.ttv_us = 0;
		dev->time_sync.tts_us = 0;
	}

	/* Now, calculate clock drift by comparing count of µs in both
	 * times:
	 */

	/* ttv_us = count of host µs since start of sync */
	now.ttv_us = dev->time_sync.ttv_us + dtv_us;

	/* tts_us = count of hw µs since start of sync */
	now.tts_us = dev->time_sync.tts_us + dts_us;

	/* prevent from div by 0 */
	if (now.ttv_us) {

		/* this test is useless if ttv_us and tts_us are periodically
		 * reset to 0
		 */
		u64 d = now.tts_us << PCAN_CLOCK_DRIFT_SCALE_SHIFT;
		int diff;

		/* DIV_ROUND_DOWN_ULL gives 131069 => time_lag grows up
		 * DIV_ROUND_UP_SECTOR_T gives 131070 => time_lag does
		 * not change but may be too large...
		 */
		now.clock_drift = DIV_ROUND_UP_SECTOR_T(d, now.ttv_us);
		//now.clock_drift = DIV_ROUND_DOWN_ULL(d, now.ttv_us);

		/* protect from crazy values: accept clock drift changes
		 * in the range [-8 ; +8] only.
		 */
		diff = abs(now.clock_drift - dev->time_sync.clock_drift);
		if (diff > 16) {
#ifdef DEBUG_TS
			pr_info(DEVICE_NAME
				": %s CAN%u: ABNORMAL clock drift value %ld "
				"regarding current one %ld\n",
				dev->adapter->name, pcan_idx(dev)+1,
				now.clock_drift,
				dev->time_sync.clock_drift);
#endif
			diff = 16;
		}

		/* otherwise, smoothly change to the new value */
		if (diff > 4) {
			now.clock_drift += dev->time_sync.clock_drift;
			now.clock_drift /= 2;
		}
	}

#endif /* PCAN_HANDLE_CLOCK_DRIFT */

lbl_reset:
	/* if clock drift changed, then reset event time sync to compute
	 * new time_lag
	 */
	if (now.clock_drift != dev->time_sync.clock_drift) {
		now.evt_base_us = 0;
	}

	dev->time_sync = now;

#ifdef PCAN_RESET_TIME_SYNC_PERIOD
	if (dev->time_sync.cm_inactivity_count > PCAN_RESET_TIME_SYNC_PERIOD) {
		dev->time_sync.cm_inactivity_count = 0;

		/* inactivity period: reset event time sync */
		dev->time_sync.evt_base_us = 0;
	}
#endif

lbl_exit:
#ifdef DEBUG_TS
#ifdef DEBUG_TS_HWTYPE
	if (dev->wType == DEBUG_TS_HWTYPE)
#endif
	{
		if (!err)
			pr_info(DEVICE_NAME
				": SYNC CAN%u %llu %ld.%06ld %lu %lu %ld\n",
				pcan_idx(dev)+1,
				(unsigned long long)dev->time_sync.ts_us,
				dev->time_sync.tv.tv_sec,
				dev->time_sync.tv.tv_usec,
				dtv_us, dts_us,
				dev->time_sync.clock_drift);
		else
			pr_info(DEVICE_NAME
				": SYNC CAN%u %d\n",
				pcan_idx(dev)+1,  err);
	}
#endif

	return !err;
}

/* Add rxerr/txerr to posted msg *ONLY* if they are consistant with current
 * bus state
 */
static int pcan_fix_msg_errcnt(struct pcandev *dev, struct pcanfd_rxmsg *rx)
{
	/* default is: no counters given to userspace */
	rx->msg.flags &= ~PCANFD_ERRCNT;

	if (!(dev->features & PCAN_DEV_ERRCNT_RDY))
		return -EACCES;

	/* don't post if rx/tx counters don't match current bus state.
	 * The uCAN STATUS event should arrive next...
	 */
	switch (dev->bus_state) {
	case PCANFD_ERROR_ACTIVE:
		if ((dev->rx_error_counter >= 96) ||
					(dev->tx_error_counter >= 96))
			return -EINVAL;

		break;
	case PCANFD_ERROR_WARNING:
		if ((dev->rx_error_counter >= 128) ||
					(dev->tx_error_counter >= 128))
			return -EINVAL;

		if ((dev->rx_error_counter < 96) &&
					(dev->tx_error_counter < 96))
			return -EINVAL;

		break;
	case PCANFD_ERROR_PASSIVE:
		if ((dev->rx_error_counter < 128) &&
					(dev->tx_error_counter < 128))
			return -EINVAL;
	default:
		break;
	}

	rx->msg.ctrlr_data[PCANFD_RXERRCNT] = dev->rx_error_counter;
	rx->msg.ctrlr_data[PCANFD_TXERRCNT] = dev->tx_error_counter;

	rx->msg.flags |= PCANFD_ERRCNT;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void pcan_push_bus_load_ind(unsigned long arg)
{
	struct pcandev *dev = (struct pcandev *)arg;
#else
static void pcan_push_bus_load_ind(struct timer_list *t)
{
	struct pcandev *dev = from_timer(dev, t, bus_load_timer);
#endif
	int bl, bl_diff;

#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_LOAD)
	pr_info(DEVICE_NAME
		": %s CAN%u: bus_state=%d bl_count=%d bl_total=%d "
		"bl=%d prev_bl=%d\n",
		dev->adapter->name, pcan_idx(dev)+1,
		dev->bus_state, dev->bus_load_count,
		dev->bus_load_total, dev->bus_load, dev->posted.bus_load);
#endif
	/* in case the bus state is unknown or init settings had changed,
	 * don't post nor rearm the timer.
	 */
	if ((dev->bus_state == PCANFD_UNKNOWN) ||
	    !(dev->init_settings.flags & PCANFD_INIT_BUS_LOAD_INFO))
		return;

	bl = (dev->bus_load_count) ? 
			dev->bus_load_total / dev->bus_load_count : 
			dev->bus_load;

	bl_diff = bl - dev->posted.bus_load;

	/* post bus load change if it's greater than 0.5%
	 * or if bus load reaches 0 and previous one wasn't
	 */
	if ((bl_diff >= 50) || (bl_diff <= -50) ||
					(!bl && dev->posted.bus_load)) {

		struct pcanfd_rxmsg rx = {
			.msg = {
				.type = PCANFD_TYPE_STATUS,
				.id = PCANFD_BUS_LOAD,
				.flags = PCANFD_BUSLOAD,
			},
			.hwtv = {
				.ts_mode = PCANFD_OPT_HWTIMESTAMP_OFF,
			}
		};

		pcan_gettimeofday(&rx.hwtv.tv);

		dev->bus_load_total = 0;
		dev->bus_load_count = 0;

		rx.msg.ctrlr_data[PCANFD_BUSLOAD_UNIT] = bl / 100;
		rx.msg.ctrlr_data[PCANFD_BUSLOAD_DEC] = bl % 100;

		pcan_fix_msg_errcnt(dev, &rx);

		/* do update posted.bus_load only if we're certain that the
		 * BUSLOAD msg is really copied in the Rx fifo. If Rx fifo was
		 * full, then next timer will (try to) put it again.
		 */
		if (pcan_fifo_put(&dev->rx_fifo, &rx) > 0) {
			dev->posted.bus_load = bl;
			pcan_event_signal(&dev->in_event);
		}
	}

	mod_timer(&dev->bus_load_timer, jiffies + dev->bus_load_ind_period);
}

/* x = (x >= y) ? x - y : 0; */
static int subtract_timeval(struct timeval *x, struct timeval *y)
{
	if (x->tv_usec >= y->tv_usec)
		x->tv_usec -= y->tv_usec;
	else {
		if (x->tv_sec) {
			x->tv_sec--;
			x->tv_usec += (USEC_PER_SEC - y->tv_usec);
		} else
			goto fail;
	}

	if (x->tv_sec >= y->tv_sec) {
		x->tv_sec -= y->tv_sec;
		return 1;
	}

fail:
	return 0;
}

/* get relative time to start of driver */
static void to_drv_rel_time(struct timeval *tv)
{
	if (!subtract_timeval(tv, &pcan_drv.sInitTime)) {

#ifdef DEBUG
		pr_info(DEVICE_NAME
			": WARNING: \"%u.%06u s.\" < drv \"%u.%06u s.\"\n",
			(u32 )tv->tv_sec, (u32 )tv->tv_usec,
			(u32 )pcan_drv.sInitTime.tv_sec,
			(u32 )pcan_drv.sInitTime.tv_usec);
#endif
		tv->tv_sec = tv->tv_usec = 0;
	}
}

/* get relative time to start of device */
static void to_dev_rel_time(struct pcandev *dev, struct timeval *tv)
{
	if (!subtract_timeval(tv, &dev->init_timestamp)) {

#ifdef DEBUG
		pr_info(DEVICE_NAME
			": WARNING: \"%u.%06u s.\" < dev \"%u.%06u s.\"\n",
			(u32 )tv->tv_sec, (u32 )tv->tv_usec,
			(u32 )dev->init_timestamp.tv_sec,
			(u32 )dev->init_timestamp.tv_usec);
#endif
		tv->tv_sec = tv->tv_usec = 0;
	}
}

/* this function is used to prevent from flooding rx fifo with STATUS msgs 
 * given by the hardware, especially when this hw is able to give rx and tx
 * error counters: when bus goes into warning or passive states, there can be 
 * a lot a STATUS msgs pushed in the rx fifo. If no task is reading this fifo
 * fast enough, it can be quickly full, preventing from CAN frames to be pushed.
 */
static int pcan_do_patch_rxmsg(void *item, void *arg)
{
	struct pcanfd_rxmsg *fifo = (struct pcanfd_rxmsg *)item;
	struct pcanfd_rxmsg *rx = (struct pcanfd_rxmsg *)arg;

	/* if last msg is also the same status msg, excepting counters value,
	 * it can be overwritten
	 */
	if ((fifo->msg.type == rx->msg.type) && /* same STATUS msg */
	    (fifo->msg.id == rx->msg.id) &&		 /* same id. */
	    (fifo->msg.flags == rx->msg.flags) && /* same flags */
	    (fifo->msg.data_len == rx->msg.data_len) /* same content len */
	   ) {
		/* patch fifo item with the new message, that is:
		 * - new timestamp
		 * - new data bytes
		 */
		*fifo = *rx;

#ifdef DEBUG_PATCH
		pr_info(DEVICE_NAME ": %s(): STATUS[%u] patched\n",
				__func__, fifo->msg.id);
#endif
		return -EEXIST;
	}

	/* MUST return != 0 to only process last item */
	return -ENOENT;
}

static int pcan_status_rx(struct pcandev *dev, struct pcanfd_rxmsg *rx)
{
	int rxerr = dev->posted.rxerr;
	int txerr = dev->posted.txerr;

#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_STATE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u, rx[id=%u]) "
			"bus=%u rx=%u/%u tx=%u/%u\n",
			__func__, dev->adapter->name, pcan_idx(dev)+1,
			rx->msg.id, dev->posted.bus_state,
			dev->rx_error_counter, dev->posted.rxerr,
			dev->tx_error_counter, dev->posted.txerr);
#endif

#ifdef PCAN_RUN_AS_OLD_API_DID
	/* consider notifying ACTIVE only if ioctl(SET_INIT) has been called */
	if ((rx->msg.id == PCANFD_ERROR_ACTIVE) &&
		(!(dev->init_settings.flags & PCANFD_INIT_USER))) {
#ifdef DEBUG_BUS_STATE
		pr_info(DEVICE_NAME ": %s CAN%u: "
			"!PCANFD_INIT_USER => STATUS[ACTIVE] discarded\n",
			dev->adapter->name, pcan_idx(dev)+1);
#endif
		return -EEXIST;
	}
#endif

	/* donot post any STATUS msg if user already knows it */
	if (rx->msg.id != dev->posted.bus_state) {
#ifdef DEBUG_BUS_STATE
		pr_info(DEVICE_NAME
			": %s CAN%u: msg.id %d != %d: post-it!\n",
			dev->adapter->name, pcan_idx(dev)+1,
			rx->msg.id, dev->posted.bus_state);
#endif
		goto lbl_post_it;
	}

	if (rx->msg.flags & PCANFD_ERRCNT) {
		rxerr = rx->msg.ctrlr_data[PCANFD_RXERRCNT];
		txerr = rx->msg.ctrlr_data[PCANFD_TXERRCNT];

		if (rxerr != dev->posted.rxerr) {
#ifdef DEBUG_BUS_STATE
			pr_info(DEVICE_NAME
				": %s CAN%u: rxerr %d != %d: post-it!\n",
				dev->adapter->name, pcan_idx(dev)+1,
				rxerr, dev->posted.rxerr);
#endif
			goto lbl_post_it;
		}

		if (txerr != dev->posted.txerr) {
#ifdef DEBUG_BUS_STATE
			pr_info(DEVICE_NAME
				": %s CAN%u: txerr %d != %d: post-it!\n",
				dev->adapter->name, pcan_idx(dev)+1,
				txerr, dev->posted.txerr);
#endif
			goto lbl_post_it;
		}
	}

	return -EEXIST;

lbl_post_it:

	/* keep a copy of last STATUS posted */
	dev->posted.bus_state = rx->msg.id;
	dev->posted.rxerr = rxerr;
	dev->posted.txerr = txerr;

	/* complete STATUS message before posting it */
	memset(rx->msg.data, '\0', sizeof(rx->msg.data));
	rx->msg.data_len = 0;

	return 0;
}

static int pcan_error_rx(struct pcandev *dev, struct pcanfd_rxmsg *rx)
{
	int rxerr = dev->posted.rxerr;
	int txerr = dev->posted.txerr;

	/* donot post any ERROR msg twice */
	if (rx->msg.id != dev->posted.bus_error)
		goto lbl_post_it;

	if (rx->msg.flags & PCANFD_ERRCNT) {
		rxerr = rx->msg.ctrlr_data[PCANFD_RXERRCNT];
		txerr = rx->msg.ctrlr_data[PCANFD_TXERRCNT];

		if (rxerr != dev->posted.rxerr)
			goto lbl_post_it;

		if (txerr != dev->posted.txerr)
			goto lbl_post_it;
	}

	return -EEXIST;

lbl_post_it:

	/* keep a copy of last ERROR posted */
	dev->posted.bus_error = rx->msg.id;
	dev->posted.rxerr = rxerr;
	dev->posted.txerr = txerr;

	return 0;
}

static void pcan_status_normalize_rx(struct pcandev *dev,
				     struct pcanfd_rxmsg *rx)
{
	u8 rxcnt, txcnt;

	if (!(rx->msg.flags & PCANFD_ERRCNT))
		return;

	rxcnt = rx->msg.ctrlr_data[PCANFD_RXERRCNT];
	txcnt = rx->msg.ctrlr_data[PCANFD_TXERRCNT];

	switch (rx->msg.id) {

	case PCANFD_ERROR_ACTIVE:
		if ((rxcnt < 96) && (txcnt < 96))
			break;

		rx->msg.id = PCANFD_ERROR_WARNING;

		/* fall through */
		fallthrough;
	case PCANFD_ERROR_WARNING:
		if ((rxcnt < 128) && (txcnt < 128))
			break;

		rx->msg.id = PCANFD_ERROR_PASSIVE;

		/* fall through */
		fallthrough;
	case PCANFD_ERROR_PASSIVE:
	default:
		break;
	}
}

#ifdef FIFO_PRE_ROUTINE
/*
 * int msgcmp(struct pcanfd_msg *rx1, struct pcanfd_msg *rx2)
 *
 * Compare two messages.
 *
 * @RETURN:
 *
 *	0	message are equal
 */
static int msgcmp(struct pcanfd_msg *msg1, struct pcanfd_msg *msg2)
{
	if ((msg1->type != msg2->type) ||
	    (msg1->id != msg2->id) ||
	    (msg1->flags != msg2->flags))
		return 1;

	if (msg1->flags & PCANFD_ERRCNT) {

		if (msg1->ctrlr_data[PCANFD_RXERRCNT] !=
		    msg2->ctrlr_data[PCANFD_RXERRCNT])
			return 2;

		if (msg1->ctrlr_data[PCANFD_TXERRCNT] !=
		    msg2->ctrlr_data[PCANFD_TXERRCNT])
			return 2;
	}

	if (msg1->flags & PCANFD_BUSLOAD) {

		if (msg1->ctrlr_data[PCANFD_BUSLOAD_UNIT] !=
		    msg2->ctrlr_data[PCANFD_BUSLOAD_UNIT])
			return 3;

		if (msg1->ctrlr_data[PCANFD_BUSLOAD_DEC] !=
		    msg2->ctrlr_data[PCANFD_BUSLOAD_DEC])
			return 3;
	}

	if (msg1->flags & PCANFD_OVRCNT) {

		if (msg1->ctrlr_data[PCANFD_RXERRCNT] !=
		    msg2->ctrlr_data[PCANFD_RXERRCNT])
			return 4;
	}

	return 0;
}
#endif

/*
 * This function patches the last msg pushed into the fifo with
 * STATUS[RX_OVERFLOW] (and increase the count of lost msgs in), in case msg
 * item cannot be put into the device rx fifo.
 *
 * WARNING:
 * - this function is called while the fifo is locked.
 * - its return value is returned by pcan_fifo_put_ex().
 * - if err == 0, the function is called BEFORE pushing the item. In that case
 *   any return value != 0 prevents from the item to be pushed.
 */
static int pcan_rxfifo_put_completion(FIFO_MANAGER *anchor,
				      void *item,
				      void *arg,
				      int err)
{
	struct pcanfd_rxmsg *rx = (struct pcanfd_rxmsg *)item;
	struct pcandev *dev = (struct pcandev *)arg;
	struct pcanfd_rxmsg *fifo;

#ifdef FIFO_PRE_ROUTINE
	/* pre routine: return != 0 to not push the item */
	if (!err) {

		/* check whether pushed msg is the same than the last read */
		switch (rx->msg.type) {

		case PCANFD_TYPE_NOP:
			/* don't post NOP */
			err = sizeof(*rx);
			break;

		case PCANFD_TYPE_STATUS:
			err = !msgcmp(&rx->msg, &dev->posted.status.msg);
			break;

		case PCANFD_TYPE_ERROR_MSG:
			err = !msgcmp(&rx->msg, &dev->posted.error.msg);
			break;
		}

		return err;
	}
#endif
#ifdef DEBUG_BUS_STATE
	if (rx->msg.type == PCANFD_TYPE_STATUS) {
		pr_info(DEVICE_NAME ": %s CAN%u: STATUS[id=%d] pushed\n",
			dev->adapter->name, pcan_idx(dev)+1, rx->msg.id);
	}
#endif

	/* simply ignore calls to that callback if fifo is not full */
	if (err != -ENOSPC)
		return err;

	/* To avoid filing kernel log buffer, display a msg *ONLY* if
	 * CAN_ERR_OVERRUN is not set
	 */
	if (!(dev->wCANStatus & CAN_ERR_OVERRUN)) {
		pr_err(DEVICE_NAME ": %s CAN%u: msg[type=%d id=%xh l=%u] lost: "
		       "err %d rxqsize=%u\n",
		       dev->adapter->name, pcan_idx(dev)+1,
		       rx->msg.type, rx->msg.id, rx->msg.data_len,
		       err, rxqsize);
	}

	/* rewind to last msg in fifo */
	fifo = (anchor->w == anchor->bufferBegin)
			? anchor->bufferEnd
			: anchor->w - anchor->wStepSize;

	/* if last msg is not STATUS[RX_OVERFLOW] then patch it and initialize
	 * counter of lost msg to 0 (or 1)
	 */
	if ((fifo->msg.type != PCANFD_TYPE_STATUS) ||
	    (fifo->msg.id != PCANFD_RX_OVERFLOW) ||
	    !(fifo->msg.flags & PCANFD_ERROR_INTERNAL)) {

		__u16 fifo_type = fifo->msg.type;

		/* Note: this set device status OVERRUN bit while in a critical
		 * section
		 */
		pcan_handle_error_internal(dev, fifo, PCANFD_RX_OVERFLOW);

		fifo->msg.flags |= PCANFD_OVRCNT;
		fifo->msg.ctrlr_data[PCANFD_RXERRCNT] = 0;

		fifo->hwtv.ts_mode = PCANFD_OPT_HWTIMESTAMP_OFF;
		pcan_gettimeofday(&fifo->hwtv.tv);

		/* if msg being patched was a CAN frame, then this msg is lost
		 * too
		 */
		switch (fifo_type) {
		case PCANFD_TYPE_CAN20_MSG:
		case PCANFD_TYPE_CANFD_MSG:
			fifo->msg.ctrlr_data[PCANFD_RXERRCNT]++;
			break;
		}
	}

	/* if message lost was a CAN frame, then incr count of lost CAN msg */
	switch (rx->msg.type) {
	case PCANFD_TYPE_CAN20_MSG:
	case PCANFD_TYPE_CANFD_MSG:
		fifo->msg.ctrlr_data[PCANFD_RXERRCNT]++;
		break;
	}

	/* task won't need to be signaled next because it has already been when
	 * (last) fifo msg was pushed
	 */
	return 0;
}

/*
 * Fix timestamp in msg before pushing it into Rx fifo.
 */
static void pcan_fix_msg_timestamp(struct pcandev *dev, struct pcanfd_rxmsg *rx)
{
	/* if no timestamp in this message, put current time in it */
	if (!(rx->msg.flags & PCANFD_TIMESTAMP)) {
		/* this hw does not provide any hw timestamp: use time of day */
		rx->hwtv.ts_mode = PCANFD_OPT_HWTIMESTAMP_OFF;
		pcan_gettimeofday(&rx->hwtv.tv);
	}

	/* default pcan gives timestamp relative to
	 * the time the driver has been loaded.
	 * Note: raw timestamsp cannot be changed according to any time base
	 */
	if (rx->hwtv.ts_mode != PCANFD_OPT_HWTIMESTAMP_RAW) {

		switch (dev->init_settings.flags & PCANFD_INIT_TS_FMT_MASK) {

		case PCANFD_INIT_TS_DRV_REL:
			to_drv_rel_time(&rx->hwtv.tv);
			break;

		case PCANFD_INIT_TS_DEV_REL: 
			to_dev_rel_time(dev, &rx->hwtv.tv);
			break;

		case PCANFD_INIT_TS_HOST_REL:
			break;
		}
	}

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s(): tv=%u.%06u s\n", __func__,
		(u32 )rx->hwtv.tv.tv_sec,
		(u32 )rx->hwtv.tv.tv_usec);
#endif
}

/*
 * Fix ctrlr_data[] field in msg given to application.
 */
static void pcan_fix_msg_ctrlr_data(struct pcandev *dev,
				    struct pcanfd_rxmsg *rx)
{
	rx->msg.flags &= ~(PCANFD_BUSLOAD|PCANFD_ERRCNT);

	{
		/* since PCANFD_ECHOID == PCANFD_RXERRCNT, then don't
		 * provide any error counter in CAN msg when PCANFD_MSG_ECHO
		 * is set.
		 * Up to now, this client id. is one byte length but,
		 * thinking to the future, do consider that all 4 bytes of
		 * ctrlr_data[] will be able to be used to save a 32 bit client
		 * id. Therefore, don't provide current bus load in that case
		 * too.
		 */
		switch (rx->msg.type) {
		case PCANFD_TYPE_CAN20_MSG:
		case PCANFD_TYPE_CANFD_MSG:
			if (rx->msg.flags & PCANFD_MSG_ECHO)
				break;

			/* fall through */
			fallthrough;
		default:
			if (dev->features & PCAN_DEV_BUSLOAD_RDY) {
				rx->msg.ctrlr_data[PCANFD_BUSLOAD_UNIT] =
					dev->bus_load / 100;
				rx->msg.ctrlr_data[PCANFD_BUSLOAD_DEC] =
					dev->bus_load % 100;
				rx->msg.flags |= PCANFD_BUSLOAD;
			}

			pcan_fix_msg_errcnt(dev, rx);
			break;
		}
	}
}

/*
 * Initializes a message to be ready to be pushed into Rx fifo.
 */
int pcan_init_rxmsg(struct pcandev *dev, struct pcanfd_rxmsg *rx,
		    __u16 type, __u32 id, __u32 flags)
{
	memset(rx, '\0', sizeof(*rx));

	rx->msg.type = type;
	rx->msg.id = id;
	rx->msg.flags = flags;

	pcan_fix_msg_timestamp(dev, rx);
	pcan_fix_msg_ctrlr_data(dev, rx);

	return sizeof(*rx);
}

/*
 * put received CAN frame into chardev receive FIFO
 * maybe this goes to a new file pcan_chardev.c some day.
 *
 * WARNING: this function returns >0 when frame HAS been enqueued,
 *                                0  when frame HAS NOT been enqueued,
 *                                <0 in case of error
 */
int pcan_chardev_rx(struct pcandev *dev, struct pcanfd_rxmsg *rx)
{
	int err;

#ifdef DEBUG_RX_QUEUE
	//if (dev->wType == HW_USB_PRO_FD)
		switch (rx->msg.type) {
		case PCANFD_TYPE_NOP:
			break;

		case PCANFD_TYPE_STATUS:
			if (rx->msg.id == PCANFD_BUS_LOAD)
				break;

			/* fall through */
			fallthrough;
		default:
			pr_info(DEVICE_NAME
				": %s(%s CAN%u > [type=%u id=%u flg=%xh len=%u "
				"%02x %02x %02x %02x])\n",
				__func__, dev->adapter->name,
				pcan_idx(dev)+1,
				rx->msg.type, rx->msg.id, rx->msg.flags,
				rx->msg.data_len, rx->msg.data[0],
				rx->msg.data[1], rx->msg.data[2],
				rx->msg.data[3]);
			break;
		}
#endif

	switch (rx->msg.type) {
	case PCANFD_TYPE_NOP:
		return 0;

	case PCANFD_TYPE_STATUS:

		/* process this case immediately to save cpu */
		if (rx->msg.id == PCANFD_BUS_LOAD) {

			dev->bus_load_total += dev->bus_load;
			dev->bus_load_count++;

			return 0;
		}
		break;

	case PCANFD_TYPE_CAN20_MSG:
	case PCANFD_TYPE_CANFD_MSG:

		/* inc rx frame counter, even if it is not posted! */
		dev->rx_frames_counter++;

		if (!(dev->allowed_msgs & PCANFD_ALLOWED_MSG_CAN)) {
#ifdef DEBUG_RX_QUEUE
			pr_info(DEVICE_NAME
				": %s(): rx [type=%d id=%d] "
				"discarded (not allowed)\n",
				__func__, rx->msg.type, rx->msg.id);
#endif

			return 0;
		}

		if (rx->msg.flags & MSGTYPE_EXTENDED) {
			if (!(dev->allowed_msgs & PCANFD_ALLOWED_MSG_EXT)) {
#ifdef DEBUG_RX_QUEUE
				pr_info(DEVICE_NAME
					": %s(): rx [type=%d id=%d] "
					"discarded (EXT fmt)\n",
					__func__, rx->msg.type, rx->msg.id);
#endif
				return 0;
			}

			/* then, check acceptance code/mask:
			 * mask:
			 * bit=1 = don't care;
			 * bit=0 = cmp this bit with coresponding bit in code
			 */
			if ((rx->msg.id & ~dev->acc_29b.mask) !=
							dev->acc_29b.code) {
#ifdef DEBUG_RX_QUEUE
				pr_info(DEVICE_NAME
					": %s(): rx [type=%d id=%0xh] "
					"discarded "
					"(acc_29b[code=%xh mask=%xh])\n",
					__func__, rx->msg.type, rx->msg.id,
					dev->acc_29b.code, dev->acc_29b.mask);
#endif
				return 0;
			}

		} else {

			/* first, check acceptance code/mask */
			if ((rx->msg.id & ~dev->acc_11b.mask) !=
							dev->acc_11b.code) {
#ifdef DEBUG_RX_QUEUE
				pr_info(DEVICE_NAME
					": %s(): rx [type=%d id=%0xh] "
					"discarded "
					"(acc_11b[code=%xh mask=%xh])\n",
					__func__, rx->msg.type, rx->msg.id,
					dev->acc_11b.code, dev->acc_11b.mask);
#endif
				return 0;
			}
		}

		if (rx->msg.flags & PCANFD_MSG_RTR)
			if (!(dev->allowed_msgs & PCANFD_ALLOWED_MSG_RTR)) {
#ifdef DEBUG_RX_QUEUE
			pr_info(DEVICE_NAME ": %s(): rx [type=%d id=%d] "
				"discarded (RTR frame)\n",
				__func__, rx->msg.type, rx->msg.id);
#endif
				return 0;
			}

		if (pcan_do_filter(dev->filter, rx)) {
#ifdef DEBUG_RX_QUEUE
			pr_debug(DEVICE_NAME ": %s(): rx [type=%d id=%d] "
				"discarded (filtered)\n",
				__func__, rx->msg.type, rx->msg.id);
#endif
			return 0;
		}
		break;
	}

	/* MUST check here if any path has been opened before posting any
	 * messages
	 */
	if (dev->nOpenPaths <= 0) {
#ifdef DEBUG_RX_QUEUE
		pr_debug(DEVICE_NAME ": %s(): rx [type=%d id=%d] "
			"discarded (nOpenPath=%d)\n",
			__func__, rx->msg.type, rx->msg.id, dev->nOpenPaths);
#endif
		return 0;
	}

	/* fix msg timestamp */
	pcan_fix_msg_timestamp(dev, rx);

	/* fix msg ctrlr_data[] */
	pcan_fix_msg_ctrlr_data(dev, rx);

	switch (rx->msg.type) {

	case PCANFD_TYPE_STATUS:
		/* other cases: message must be pushed into fifo: */

		/* donot post any STATUS message if it isn't allowed
		 * Note: this can be done here, because dev->bus_state
		 *       is already updated.
		 */
		if (!(dev->allowed_msgs & PCANFD_ALLOWED_MSG_STATUS))
			return 0;

		/* pre-process these msgs before posting them to user, to
		 * prevent to flood its rx queue with lost of same msgs
		 */
		switch (rx->msg.id) {

		case PCANFD_ERROR_ACTIVE:
		case PCANFD_ERROR_WARNING:
		case PCANFD_ERROR_PASSIVE:
			pcan_status_normalize_rx(dev, rx);

			/* fall through */
			fallthrough;
		case PCANFD_ERROR_BUSOFF:

			err = pcan_status_rx(dev, rx);
			if (err) {

				/* err == -EEXIST:
				 *
				 * no need to push this STATUS because the same
				 * status has already been posted before.
				 * Return 0 to inform that nothing has been
				 * posted.
				 */
#ifdef DEBUG_INVALID_BUS_STATE
				if (err == -EINVAL)
					pr_info(DEVICE_NAME ": %s CAN%u: "
						"bus_state=%d incompatibe with "
						"err counters rx=%u tx=%u\n",
						dev->adapter->name,
						pcan_idx(dev)+1,
						dev->bus_state,
						dev->rx_error_counter,
						dev->tx_error_counter);
#endif
				return 0;
			}
			break;

		case PCANFD_RX_EMPTY:
		case PCANFD_RX_OVERFLOW:
		case PCANFD_TX_EMPTY:
		case PCANFD_TX_OVERFLOW:
			break;

		default:
			break;
		}

		/* try to patch last posted msg if it was the same msg */
		err = pcan_fifo_foreach_back(&dev->rx_fifo,
						pcan_do_patch_rxmsg, rx);
		if (err == -EEXIST) {
			/* msg patched. don't post the msg */
			return 0;
		}

		break;

	case PCANFD_TYPE_ERROR_MSG:

		/* donot post any ERROR message if it isn't allowed */
		if (!(dev->allowed_msgs & PCANFD_ALLOWED_MSG_ERROR))
			return 0;

		/* pcan_error() should be called only when this message is
		 * allowed, to avoid to update driver rxerr/txerr cached
		 * values.
		 */
		err = pcan_error_rx(dev, rx);
		if (err == -EEXIST)
			/* no need to push this ERROR because the same
			 * error has already been posted before.
			 * Return 0 to inform that nothing has been
			 * posted.
			 */
			return 0;

		break;
	}

	/* push (or patch) frame into Rx fifo.
	 * don't return any error to not stop frames processing when they're
	 * received by packets.
	 */
	return pcan_fifo_put_ex(&dev->rx_fifo, rx,
				pcan_rxfifo_put_completion, dev);
}

/*
 * int pcan_post_bus_state(struct pcandev *dev)
 *
 *	Unconditionnaly post a STATUS[dev->bus_state] msg.
 *
 * @RETURN:
 *
 *	-EINVAL		bus state is not valid
 *	0		if msg has been pushed
 *	< 0		other errors
 */
int pcan_post_bus_state(struct pcandev *dev)
{
	int err = -EINVAL;

	if (dev->bus_state != PCANFD_UNKNOWN) {
		struct pcanfd_rxmsg rx = {
			.msg.type = PCANFD_TYPE_STATUS,
			.msg.id = dev->bus_state,
			.msg.flags = PCANFD_ERROR_BUS,
		};

		err = pcan_chardev_rx(dev, &rx);
		if (err > 0)
			pcan_event_signal(&dev->in_event);
#ifdef DEBUG_BUS_STATE
	} else {
		pr_err(DEVICE_NAME
		       ": %s(%s CAN%u): posting UNKNOWN bus_state\n",
		       __func__, dev->adapter->name, pcan_idx(dev)+1);
#endif
	}

	return err >= 0 ? 0 : err;
}
#endif /* NETDEV_SUPPORT */

/*
 * get CAN frame from the device Tx fifo.
 */
int pcan_txfifo_get(struct pcandev *dev, struct pcanfd_txmsg *tx)
{
	return pcan_fifo_get_ex(&dev->tx_fifo, tx, NULL, NULL);
}

static void pcan_init_session_counters(struct pcandev *dev)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_STATE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u): bus=%u rx=%u/tx=%u\n",
		__func__, (dev->adapter) ? dev->adapter->name : "NULL",
		pcan_idx(dev)+1, dev->bus_state,
		dev->rx_error_counter, dev->tx_error_counter);
#endif

#ifdef PCAN_USES_O_ACCMODE_HACK
	/* init posted with values that are never given to user so that first
	 * message wont't be filtered
	 */
	dev->posted.bus_state = PCANFD_UNKNOWN;
	dev->posted.bus_error = PCANFD_ERRMSG_COUNT;
	dev->posted.bus_load = 0;
	dev->posted.rxerr = 0;
	dev->posted.txerr = 0;
#endif

	/* don't post useless 1st STATUS[BUS_LOAD=0%] */
	dev->bus_load = 0;
	dev->bus_error = PCANFD_ERRMSG_COUNT;
	dev->rx_error_counter = 0;
	dev->tx_error_counter = 0;

	dev->wCANStatus &= ~(CAN_ERR_BUSOFF|CAN_ERR_BUSHEAVY|\
							CAN_ERR_BUSLIGHT);

	/* controller errors should also be cleared */
	dev->wCANStatus &= ~(CAN_ERR_QOVERRUN|CAN_ERR_QRCVEMPTY|\
							CAN_ERR_QXMTFULL);
}

int pcan_set_bus_state(struct pcandev *dev, enum pcanfd_status bus_state)
{
	if (bus_state == dev->bus_state)
		return 0;

	switch (bus_state) {
	case PCANFD_ERROR_ACTIVE:
#ifdef DEBUG_BUS_STATE
		pr_info(DEVICE_NAME ": pcan%u enters ERROR_ACTIVE\n",
			dev->nMinor);
#endif
#ifndef NETDEV_SUPPORT
		/* start STATUS[PCANFD_BUS_LOAD] timer */
		if (dev->init_settings.flags & PCANFD_INIT_BUS_LOAD_INFO) {
			mod_timer(&dev->bus_load_timer,
					jiffies + dev->bus_load_ind_period);
		}
#endif
		break;
	case PCANFD_ERROR_WARNING:
#ifdef DEBUG_BUS_STATE
		pr_info(DEVICE_NAME ": pcan%u enters ERROR_WARNING\n",
			dev->nMinor);
#endif
		pcan_set_status_bit(dev, CAN_ERR_BUSLIGHT|CAN_ERR_BUSHEAVY);
		break;
	case PCANFD_ERROR_PASSIVE:
#ifdef DEBUG_BUS_STATE
		pr_info(DEVICE_NAME ": pcan%u enters ERROR_PASSIVE\n",
			dev->nMinor);
#endif
		break;
	case PCANFD_ERROR_BUSOFF:
#ifdef DEBUG_BUS_STATE
		pr_info(DEVICE_NAME ": pcan%u enters BUSOFF\n",
			dev->nMinor);
#endif
		if (pcan_set_status_bit(dev, CAN_ERR_BUSOFF))
			dev->dwErrorCounter++;
		break;
	case PCANFD_UNKNOWN:
#ifdef DEBUG_BUS_STATE
		pr_info(DEVICE_NAME ": pcan%u back to UNKNOWN\n",
			dev->nMinor);
#endif
		pcan_init_session_counters(dev);
		break;
	default:
		pr_err(DEVICE_NAME
			": trying to set unknown bus state %d to pcan%u\n",
			bus_state, dev->nMinor);
		return 0;
	}

	dev->bus_state = bus_state;

	return 1;
}

int pcan_handle_busoff(struct pcandev *dev, struct pcanfd_rxmsg *rx)
{
	if (dev->bus_state == PCANFD_ERROR_BUSOFF) {
		rx->msg.type = PCANFD_TYPE_NOP;
		return 0;
	}

	pr_info(DEVICE_NAME ": %s CAN%u (pcan%u): BUS OFF\n",
		dev->adapter->name, pcan_idx(dev)+1, dev->nMinor);

	rx->msg.type = PCANFD_TYPE_STATUS;
	rx->msg.flags = PCANFD_ERROR_BUS;
	rx->msg.id = PCANFD_ERROR_BUSOFF;

	pcan_set_bus_state(dev, rx->msg.id);

	/* In BUSOFF state only, wake up any task waiting for room in the
	 * device tx queue, because there is no chance for the BUS to go back to
	 * any active state without re-initializing it.
	 * Note that this "rx" message is to be put into the device rx queue,
	 * thus any task waiting for incoming messages will also be
	 * woken up.
	 */
	pcan_event_signal(&dev->out_event);

	return 1;
}

void pcan_handle_error_active(struct pcandev *dev, struct pcanfd_rxmsg *rx)
{
	const enum pcanfd_status bus_state = dev->bus_state;

#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_STATE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u): bus=%u rx=%u/tx=%u (@rx=%p)\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1, bus_state,
		dev->rx_error_counter, dev->tx_error_counter, rx);
#endif

	if (rx) {

		if (bus_state != PCANFD_ERROR_ACTIVE) {

			/* inform only if controller was is in bad state */
			if (bus_state == PCANFD_ERROR_BUSOFF)
				pr_info(DEVICE_NAME ": %s CAN%u: ACTIVE\n",
					dev->adapter->name, pcan_idx(dev)+1);

			/* prepare a STATUS message to put into Rx fifo */
			rx->msg.type = PCANFD_TYPE_STATUS;
			rx->msg.id = PCANFD_ERROR_ACTIVE;
			rx->msg.flags = PCANFD_ERROR_BUS;

			/* data is reset by pcan_status_rx() next */

		/* be sure to post nothing if already in ERROR_ACTIVE */
		} else {
			rx->msg.type = PCANFD_TYPE_NOP;
		}
	}

	pcan_set_bus_state(dev, PCANFD_ERROR_ACTIVE);

	/* (re)start writing AFTER having changed the bus state
	 * see also __pcan_dev_start_writing() in src/pcanfd_core.c
	 */
	if (bus_state == PCANFD_UNKNOWN) {

		 __pcan_dev_start_writing(dev, NULL);
	}
}

void pcan_soft_error_active(struct pcandev *dev)
{
	struct pcanfd_rxmsg rx;

#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_STATE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u)\n", __func__,
		dev->adapter->name, pcan_idx(dev)+1);
#endif

	pcan_handle_error_active(dev, &rx);

#ifdef NETDEV_SUPPORT
	pcan_netdev_rx(dev, &rx);
#else
	if (pcan_chardev_rx(dev, &rx) > 0)
		pcan_event_signal(&dev->in_event);
#endif
}

/*
 * Return 0 if bus should go to ERROR_ACTIVE next
 * (see pcan_handle_error_active()).
 *
 * Return 1 if not.
 */
int pcan_handle_error_status(struct pcandev *dev, struct pcanfd_rxmsg *rx,
			     int err_warning, int err_passive)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_BUS_STATE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u, EW=%u, EP=%u): "
		"bus=%u rxerr=%u txerr=%u\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1,
		err_warning, err_passive, dev->bus_state,
		dev->rx_error_counter, dev->tx_error_counter);
#endif

	rx->msg.type = PCANFD_TYPE_STATUS;
	rx->msg.flags = PCANFD_ERROR_BUS;

	if (err_passive) {
		rx->msg.id = PCANFD_ERROR_PASSIVE;

	} else if (err_warning) {
		rx->msg.id = PCANFD_ERROR_WARNING;

		/* fix error counters (if they aren't) */
		if (dev->rx_error_counter >= 128)
			dev->rx_error_counter = 127;
		if (dev->tx_error_counter >= 128)
			dev->tx_error_counter = 127;
	} else {

		/* fix error counters (if they aren't) */
		if (dev->rx_error_counter >= 96)
			dev->rx_error_counter = 95;
		if (dev->tx_error_counter >= 96)
			dev->tx_error_counter = 95;

		/* in fact, no error bit set: back to ERROR_ACTIVE */
		if (dev->bus_state != PCANFD_ERROR_ACTIVE)
			return 0;

		/* just to jump to next 'else' part (rx->msg.id=NOP) */
		rx->msg.id = PCANFD_ERROR_ACTIVE;
	}

	/* if bus state really changed, count as error */
	if (pcan_set_bus_state(dev, rx->msg.id))
		dev->dwErrorCounter++;

	/* otherwise, donot post the same bus state */
	else
		rx->msg.type = PCANFD_TYPE_NOP;

	/* say that error state has been handled (not in ACTIVE state). */
	return 1;
}

int pcan_handle_error_ctrl(struct pcandev *dev, struct pcanfd_rxmsg *rx,
				int err_ctrlr)
{
#if defined(DEBUG_TRACE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u, err_ctrlr=%d)\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1, err_ctrlr);
#endif
	dev->dwErrorCounter++;

	if (rx)
		rx->msg.type = PCANFD_TYPE_NOP;

	switch (err_ctrlr) {
	case PCANFD_RX_OVERFLOW:
		if (!pcan_set_status_bit(dev, CAN_ERR_QOVERRUN))
			return 0;

		if (net_ratelimit())
			pr_err(DEVICE_NAME
			       ": %s CAN%u: controller internal Rx queue "
			       "overflow: possible loss of msgs(s)\n",
			       dev->adapter->name, pcan_idx(dev)+1);

		break;
	case PCANFD_RX_EMPTY:
		if (!pcan_set_status_bit(dev, CAN_ERR_QRCVEMPTY))
			return 0;

		break;
	case PCANFD_TX_OVERFLOW:
		if (!pcan_set_status_bit(dev, CAN_ERR_QXMTFULL))
			return 0;

		pr_err(DEVICE_NAME ": %s CAN%u: controller internal Tx queue "
		       "overflow: possible loss of msgs(s)\n",
		       dev->adapter->name, pcan_idx(dev)+1);

		break;
	default:
		pcan_set_status_bit(dev, CAN_ERR_RESOURCE);
		pr_err(DEVICE_NAME ": %s CAN%u: controller internal error %d\n",
		       dev->adapter->name, pcan_idx(dev)+1, err_ctrlr);
		break;
	}

	if (rx) {
		rx->msg.type = PCANFD_TYPE_STATUS;
		rx->msg.flags = PCANFD_ERROR_CTRLR;
		rx->msg.id = err_ctrlr;
		rx->msg.data_len = 0;
	}

	return 1;
}

void pcan_handle_error_msg(struct pcandev *dev, struct pcanfd_rxmsg *rx,
				int err_type, u8 err_code,
				int err_rx, int err_gen)
{
#if defined(DEBUG_TRACE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u, err_type=%d)\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1, err_type);
#endif

	if (rx) {
		rx->msg.type = PCANFD_TYPE_ERROR_MSG;
		rx->msg.id = err_type;
		if (err_rx)
			rx->msg.flags |= PCANFD_ERRMSG_RX;
		if (err_gen)
			rx->msg.flags |= PCANFD_ERRMSG_GEN;

		rx->msg.data_len = 1;
		rx->msg.data[0] = err_code;
	}

	dev->dwErrorCounter++;
}

void pcan_handle_error_internal(struct pcandev *dev,
				struct pcanfd_rxmsg *rx,
				int err_internal)
{
#if defined(DEBUG_TRACE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u, err_internal=%d)\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1, err_internal);
#endif

	dev->dwErrorCounter++;

	/* Note: don't test those bits since they are set/unset from task too */
	switch (err_internal) {
	case PCANFD_RX_OVERFLOW:
		pcan_set_status_bit(dev, CAN_ERR_OVERRUN);
		break;
	case PCANFD_TX_OVERFLOW:
		pcan_set_status_bit(dev, CAN_ERR_XMTFULL);
		break;
	default:
		pr_err(DEVICE_NAME ": pcan%u: internal error %d\n",
				dev->nMinor, err_internal);
		pcan_set_status_bit(dev, CAN_ERR_RESOURCE);
		break;
	}

	if (rx) {
		rx->msg.type = PCANFD_TYPE_STATUS;
		rx->msg.flags = PCANFD_ERROR_INTERNAL;
		rx->msg.id = err_internal;
		rx->msg.data_len = 0;
	}
}

void pcan_handle_error_protocol(struct pcandev *dev,
				struct pcanfd_rxmsg *rx,
				int err_protocol)
{
#if defined(DEBUG_TRACE)
	pr_info(DEVICE_NAME ": %s(%s CAN%u, err_protocol=%d)\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1, err_protocol);
#endif

	dev->dwErrorCounter++;

	if (rx)
		rx->msg.type = PCANFD_TYPE_NOP;

	switch (err_protocol) {
	case PCANFD_RX_OVERFLOW:
		/* as old driver did */
		if (!pcan_set_status_bit(dev, CAN_ERR_QOVERRUN))
			return;

		break;
	case PCANFD_TX_OVERFLOW:
		if (!pcan_set_status_bit(dev, CAN_ERR_QXMTFULL))
			return;

		break;
	default:
		pr_err(DEVICE_NAME ": pcan%u: protocol error %d\n",
			dev->nMinor, err_protocol);
		pcan_set_status_bit(dev, CAN_ERR_RESOURCE);
		break;
	}

	if (rx) {
		rx->msg.type = PCANFD_TYPE_STATUS;
		rx->msg.flags = PCANFD_ERROR_PROTOCOL;
		rx->msg.id = err_protocol;
		rx->msg.data_len = 0;
	}
}

/*
 * Set a status bit only if it isn't yet. Return 0 if it was.
 */
int pcan_set_status_bit(struct pcandev *dev, u16 bits)
{
	if (dev->wCANStatus & bits)
		return 0;

	dev->wCANStatus |= bits;
	return bits;
}

/*
 * Clear any wCANStatus bit.
 *
 * @return:
 *
 * 0	if no bit were set
 * 1	if any was set.
 */
int pcan_clear_status_bit(struct pcandev *dev, u16 bits)
{
	if (!(dev->wCANStatus & bits))
		return 0;

	/* clear corresponding bits and generate a STATUS message to update
	 * the state in the application context
	 */
	dev->wCANStatus &= ~bits;

	return 1;
}

/* safety call the device "dev->cleanup()" specific callback and manage
 * to be called only once. The "dev->cleanup()" callback should not be
 * directlty called.
 */
void pcan_cleanup_dev(struct pcandev *dev)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_CLEANUP)
	pr_info(DEVICE_NAME ": %s(dev=%p flags=%08Xh): cleaning %s CAN%d\n",
		 __func__, dev,
		 (dev) ? dev->flags : 0,
		 (dev && dev->adapter) ? dev->adapter->name : "Unknown adapter",
		 (dev) ? pcan_idx(dev)+1 : -1);
#endif
	if (dev) {
		if (!(dev->flags & (PCAN_DEV_CLEANED|PCAN_DEV_CLEANING)))
			if (dev->cleanup) {
				/* protect from reentrance */
				dev->flags |= PCAN_DEV_CLEANING;
				dev->cleanup(dev);
			}

		dev->flags &= ~(PCAN_DEV_OPENED|PCAN_DEV_CLEANING);
		dev->flags |= PCAN_DEV_CLEANED;
	}
}

/* request time in msec, fast */
u32 get_mtime(void)
{
	/* return (jiffies / HZ) * 1000; */
	return jiffies_to_msecs(jiffies);
}

/*
 * Safe add a new device to the driver registered devices list
 */
void pcan_add_dev_in_list_ex(struct pcandev *dev, u32 dev_flags)
{
#ifdef HANDLE_HOTPLUG
	unsigned long flags;

	pcan_lock_get_irqsave(&pcan_drv.devices_lock, flags);
#endif
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, dev);
#endif

	list_add_tail(&dev->list_dev, &pcan_drv.devices);
	dev->flags |= PCAN_DEV_LINKED | dev_flags;
	pcan_drv.wDeviceCount++;

#ifdef HANDLE_HOTPLUG
	pcan_lock_put_irqrestore(&pcan_drv.devices_lock, flags);
#endif
}

/*
 * Safe remove a device from the driver registered devices list
 */
struct pcandev *pcan_remove_dev_from_list(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, dev);
#endif

	if (dev->flags & PCAN_DEV_LINKED) {

#ifdef HANDLE_HOTPLUG
		unsigned long flags;

		/* pcandev is being destroyed by remove_dev_list(), so let
		 * remove_dev_list() unlink the pcandev...
		 * if (!pcan_mutex_trylock(&pcan_drv.devices_lock)) {
		 */
		if (!pcan_lock_try_irqsave(&pcan_drv.devices_lock, flags)) {
			pr_warn(DEVICE_NAME
				": can't unlink dev %p (global lock in use)\n",
				dev);
			return NULL;
		}
#endif
		list_del(&dev->list_dev);
		dev->flags &= ~PCAN_DEV_LINKED;

		if (pcan_drv.wDeviceCount)
			pcan_drv.wDeviceCount--;

#ifdef HANDLE_HOTPLUG
		pcan_lock_put_irqrestore(&pcan_drv.devices_lock, flags);
#endif
		return dev;
	}

	return NULL;
}

/*
 * Safe check whether a device is linked in the pcan driver devices list.
 */
int pcan_is_device_in_list(struct pcandev *dev)
{
	struct list_head *ptr;
	int found = 0;

#ifdef HANDLE_HOTPLUG
	unsigned long flags;

	pcan_lock_get_irqsave(&pcan_drv.devices_lock, flags);
#endif

	list_for_each(ptr, &pcan_drv.devices) {
		struct pcandev *d = list_entry(ptr, struct pcandev, list_dev);
		if (dev == d) {
			found = 1;
			break;
		}
	}

#ifdef HANDLE_HOTPLUG
	pcan_lock_put_irqrestore(&pcan_drv.devices_lock, flags);
#endif
	return found;
}

#ifdef HANDLE_HOTPLUG
static struct pcandev *pcan_is_minor_used(struct pcandev *pdev, int minor)
{
	struct list_head *ptr;

	list_for_each(ptr, &pcan_drv.devices) {
		struct pcandev *dev = list_entry(ptr, struct pcandev, list_dev);

		/* Note: if not NULL, pdev is a linked device we don't want
		 * to test its nMinor.
		 */
		if (dev != pdev)
			if (dev->nMinor == minor)
				return dev;
	}

	return NULL;
}

/*
 * Look for enough minors numbers in a given range.
 * Each call returns 1st device number of a range that is enough to store all
 * the minors an adapter is able to handle.
 * Caller MUST update the return value according to the real channel.
 * Called by hotplug devices only.
 */
int pcan_find_free_minors(struct pcandev *pdev, int from, int until)
{
	int minor = (from >= 0) ? from : 0;
	unsigned long flags;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(from=%d, until=%d)\n", __func__, from, until);
#endif

	pcan_lock_get_irqsave(&pcan_drv.devices_lock, flags);

	while (1) {
		struct pcandev *dev = NULL;
		int i;

		/* check whether there are enough free minors for the adapter */
		for (i = 0; i < pdev->adapter->can_count; i++) {
			dev = pcan_is_minor_used(pdev, minor + i);
			if (dev) {
				/* a minor of the range is used:
				 * - if it is used by a channel of the same
				 *   adapter: range is already reserved,
				 *   nothing to do.
				 * - if that channel is of another
				 *   adapter then skip all its
				 *   channels and restart.
				 */
				break;
			}
		}

		/* dev == NULL: range [minor..] found
		 * dev adapters are equal: range [minor..] found too
		 */
		if (!dev || (dev->adapter == pdev->adapter))
			break;

		/* minor defines 1st channel of some other adapter.
		 * Skip all the others and restart
		 */
		minor += dev->adapter->can_count;

		/* if searching is limited, check whether minor is always
		 * into these limits...
		 */
		if (until > from)
			if (minor > until) {
				minor = -ENODEV;
				break;
			}
	}

	pcan_lock_put_irqrestore(&pcan_drv.devices_lock, flags);

	return minor;
}
#endif /* HANDLE_HOTPLUG */

/* called when 'cat /proc/pcan' invoked */
#ifdef CREATE_PROC_ENTRY_DEPRECATED
static int pcan_read_procmem(struct seq_file *m, void *v)
{
#else
static int pcan_read_procmem(char *page, char **start, off_t offset, int count,
							int *eof, void *data)
{
	int len = 0;
#endif
	struct pcandev *dev;
	struct list_head *ptr;
#ifdef HANDLE_HOTPLUG
	unsigned long flags;
#endif

#ifdef CREATE_PROC_ENTRY_DEPRECATED
	seq_printf(m, "\n");
	seq_printf(m,
		"*------------- PEAK-System CAN interfaces (www.peak-system.com) -------------\n");
	seq_printf(m,
		"*------------- %s (%s) %s %s --------------\n",
		pcan_drv.szVersionString, CURRENT_VERSIONSTRING,
		__DATE__, __TIME__);
	seq_printf(m, "%s\n", config);
	seq_printf(m, "*--------------------- %d interfaces @ major %03d found -----------------------\n",
		pcan_drv.wDeviceCount, pcan_drv.nMajor);
	seq_printf(m,
		"*n -type- -ndev- --base-- irq --btr- --read-- --write- --irqs-- -errors- status\n");
#else
	len += sprintf(page + len, "\n");
	len += sprintf(page + len,
		"*------------- PEAK-System CAN interfaces (www.peak-system.com) -------------\n");
	len += sprintf(page + len,
		"*------------- %s (%s) %s %s --------------\n",
		pcan_drv.szVersionString, CURRENT_VERSIONSTRING,
		__DATE__, __TIME__);
	len += sprintf(page + len, "%s\n", config);
	len += sprintf(page + len,
		"*--------------------- %d interfaces @ major %03d found -----------------------\n",
		pcan_drv.wDeviceCount, pcan_drv.nMajor);
	len += sprintf(page + len,
		"*n -type- -ndev- --base-- irq --btr- --read-- --write- --irqs-- -errors- status\n");
#endif
#ifdef HANDLE_HOTPLUG
	/* enter critical section (get mutex) */
	pcan_lock_get_irqsave(&pcan_drv.devices_lock, flags);
#endif
	/* loop trough my devices */
	for (ptr = pcan_drv.devices.next; ptr != &pcan_drv.devices;
							ptr = ptr->next) {
		u32 dwPort = 0;
		u16 wIrq = 0, dev_btr0btr1;
		int minor;
#ifdef NETDEV_SUPPORT
		struct net_device_stats *stats; /* rx/tx stats */
#endif

		dev = (struct pcandev *)ptr;
		minor = dev->nMinor;

		dev_btr0btr1 = (dev->sysclock_Hz == 8*MHz) ?
			pcan_bittiming_to_btr0btr1(&dev->init_settings.nominal):
			sja1000_bitrate(dev->init_settings.nominal.bitrate,
				dev->init_settings.nominal.sample_point,
				dev->init_settings.nominal.sjw);

		switch (dev->wType) {
		case HW_USB:
		case HW_USB_FD:
		case HW_USB_PRO:
		case HW_USB_PRO_FD:
		case HW_USB_X6:
#ifdef USB_SUPPORT
			/* get serial number of device */
			if (dev->is_plugged) {
				dwPort = pcan_usb_get_if(dev)->dwSerialNumber;
				wIrq = dev->port.usb.ucHardcodedDevNr;
			} else {
				dwPort = 0x00dead00;  /* it is dead */
				wIrq = 0;
			}
#endif
			break;
		default:
			dwPort = dev->dwPort;
			wIrq = dev->wIrq;
			break;
		}

#ifdef NETDEV_SUPPORT
		stats = (dev->netdev) ?
				pcan_netdev_get_stats(dev->netdev) : NULL;
#endif
#ifdef CREATE_PROC_ENTRY_DEPRECATED
		seq_printf(m,
#else
		len += sprintf(page + len,
#endif
		"%2d %6s %6s %8x %03d 0x%04x %08lx %08lx %08x %08x 0x%04x\n",
			minor,
			dev->type,
#ifdef NETDEV_SUPPORT
			(dev->netdev) ? (dev->netdev->name) : "can?",
#else
			"-NA-",
#endif
			dwPort,
			wIrq,
			dev_btr0btr1,
#ifdef NETDEV_SUPPORT
			(stats) ? stats->rx_packets : 0,
			dev->tx_fifo.dwTotal +
					((stats) ? stats->tx_packets : 0),
#else
			(unsigned long)dev->rx_fifo.dwTotal,
			(unsigned long)dev->tx_fifo.dwTotal,
#endif
			dev->rx_irq_counter + dev->tx_irq_counter,
			dev->dwErrorCounter,
			dev->wCANStatus);
	}

#ifdef HANDLE_HOTPLUG
	/* release mutex */
	pcan_lock_put_irqrestore(&pcan_drv.devices_lock, flags);
#endif
#ifdef CREATE_PROC_ENTRY_DEPRECATED
	return 0;
#else
	len += sprintf(page + len, "\n");

	*eof = 1;
	return len;
#endif
}

#ifdef CONFIG_SYSFS
int pcan_sysfs_add_attr(struct device *dev, struct attribute *attr)
{
	return sysfs_add_file_to_group(&dev->kobj, attr, NULL);
}

int pcan_sysfs_add_attrs(struct device *dev, struct attribute **attrs)
{
	int err = 0;
	struct attribute **ppa;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p=\"%s\")\n", __func__, dev, dev->kobj.name);
#endif
	for (ppa = attrs; *ppa; ppa++) {
		err = pcan_sysfs_add_attr(dev, *ppa);
		if (err) {
			pr_err(DEVICE_NAME
				": failed to add \"%s\" to \"%s\" (err %d)\n",
				(*ppa)->name, dev->kobj.name, err);
			break;
		}
	}

	return err;
}

void pcan_sysfs_del_attr(struct device *dev, struct attribute *attr)
{
	sysfs_remove_file_from_group(&dev->kobj, attr, NULL);
}

void pcan_sysfs_del_attrs(struct device *dev, struct attribute **attrs)
{
	struct attribute **ppa;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p=\"%s\")\n", __func__, dev, dev->kobj.name);
#endif
	for (ppa = attrs; *ppa; ppa++)
		pcan_sysfs_del_attr(dev, *ppa);
}
#else
int pcan_sysfs_add_attr(struct device *dev, struct attribute *attr)
{
	return 0;
}

int pcan_sysfs_add_attrs(struct device *dev, struct attribute **attrs)
{
	return 0;
}

void pcan_sysfs_del_attr(struct device *dev, struct attribute *attr) {}
void pcan_sysfs_del_attrs(struct device *dev, struct attribute **attrs) {}
#endif

#ifdef CREATE_PROC_ENTRY_DEPRECATED
static int open_callback(struct inode *inode, struct file *file)
{
	return single_open(file, pcan_read_procmem, NULL);
}

static struct proc_dir_entry *proc_file_entry;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
	.proc_open  = open_callback,
	.proc_read  = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations proc_file_fops = {
	.owner = THIS_MODULE,
	.open  = open_callback,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#else
#define proc_file_entry		NULL
#endif

void remove_dev_list(void)
{
	struct list_head *pos, *n;
#ifdef DEBUG_ALLOC_DEV
	int n_free = 0, n_del = 0;
#endif
#ifdef HANDLE_HOTPLUG
	unsigned long flags;

	pcan_lock_get_irqsave(&pcan_drv.devices_lock, flags);
#endif

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	list_for_each_prev_safe(pos, n, &pcan_drv.devices) {
		struct pcandev *dev = list_entry(pos, struct pcandev, list_dev);

		pcan_destroy_dev(dev);

		list_del(pos);

		/* free device object allocated memory */
#ifdef DEBUG_ALLOC_DEV
		if (!pcan_free_dev(dev))
			n_free++;
		n_del++;
#else
		pcan_free_dev(dev);
#endif
	}

	pcan_drv.wDeviceCount = 0;

#ifdef HANDLE_HOTPLUG
	pcan_lock_put_irqrestore(&pcan_drv.devices_lock, flags);
#endif

#ifdef DEBUG_ALLOC_DEV
	pr_info(DEVICE_NAME ": %d device(s) removed (%u freed)\n",
		n_del, n_free);
#endif
}

#ifdef SYSFS_SUPPORT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
#define PCAN_ATTR_RO_CONST	const
#else
#define PCAN_ATTR_RO_CONST
#endif

static ssize_t version_show(PCAN_ATTR_RO_CONST struct class *cls,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
			    PCAN_ATTR_RO_CONST struct class_attribute *attr,
#endif
			    char *buf)
{
	return sprintf(buf, "%s\n", CURRENT_VERSIONSTRING);
}

static ssize_t clk_ref_show(PCAN_ATTR_RO_CONST struct class *cls,
			    PCAN_ATTR_RO_CONST struct class_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%u\n", drvclkref);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
static CLASS_ATTR_RO(version);
static CLASS_ATTR_RO(clk_ref);

static struct attribute *pcan_class_attrs[] = {
	&class_attr_version.attr,
	&class_attr_clk_ref.attr,
	NULL,
};

ATTRIBUTE_GROUPS(pcan_class);
#else
static struct class_attribute pcan_class_attrs[] = {
	__ATTR_RO(version),
	__ATTR_RO(clk_ref),
	__ATTR_NULL
};
#endif

static struct class pcan_class = {
	.name = DEVICE_NAME,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	.class_groups = pcan_class_groups,
#else
	.class_attrs = pcan_class_attrs,
#endif
};
#endif

/* called when the device is removed 'rmmod pcan' */
static void pcan_cleanup_module(void)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(init_step=%u)\n",
					__func__, pcan_drv.wInitStep);
#endif
	switch (pcan_drv.wInitStep) {

	case 4:
		remove_proc_entry(DEVICE_NAME, NULL);

		/* fall through */
		fallthrough;
	case 3:
		DEV_UNREGISTER();

		/* fall through */
		fallthrough;
	case 2:
#ifdef USB_SUPPORT
		pcan_usb_deinit();
#endif

#ifdef PCCARD_SUPPORT
		pcan_pccard_deinit();
#endif

#ifdef PCAN_PCI_EVENT_DRIVEN
		pcan_pci_deinit();
#endif
		/* fall through */
		fallthrough;
	case 1:
#ifdef SYSFS_SUPPORT
		class_unregister(&pcan_class);
#endif

#ifdef NO_RT
		unregister_chrdev(pcan_drv.nMajor, DEVICE_NAME);
#endif

		REMOVE_DEV_LIST();

#ifdef HANDLE_HOTPLUG
		/* destroy mutex used to access pcan devices list */
		pcan_lock_destroy(&pcan_drv.devices_lock);
#endif
		/* fall through */
		fallthrough;
	case 0:
		pcan_drv.wInitStep = 0;
	}

	pr_info(DEVICE_NAME ": removed.\n");
}

void pcan_init_version(struct pcan_version *ver)
{
	ver->major = -1;
	ver->minor = -1;
	ver->subminor = -1;
}

void pcan_init_adapter(struct pcan_adapter *pa, const char *name,
		       const char *part_num, int index, int can_count)
{
	pa->flags = 0;
	pa->name = name;
	pa->part_num = part_num;
	pa->index = index;
	pa->can_count = can_count;
	pa->opened_count = 0;
	pcan_init_version(&pa->hw_ver);
	memset(pa->guid, 0xff, sizeof(pa->guid));
}

void *__pcan_alloc_adapter_ex(const char *name, const char *part_num, int index,
			      int can_count, int extra_size)
{
	struct pcan_adapter *pa;
	int l = sizeof(*pa);

	if (l < extra_size)
		l = extra_size;

	l += can_count * sizeof(struct pcandev *);

	pa = pcan_malloc(l, GFP_KERNEL);
	if (pa) {
		memset(pa, '\0', l);
		pcan_init_adapter(pa, name, part_num, index, can_count);
	}

	return pa;
}

struct pcan_adapter *__pcan_free_adapter(struct pcan_adapter *pa)
{
	pcan_free(pa);
	return NULL;
}

/* This function can be called next to pcan_alloc_dev().  */
void pcan_set_dev_adapter(struct pcandev *dev, struct pcan_adapter *pa)
{
	dev->adapter = pa;

	/* DON't do any doubly linkage here like:
	 *
	 * pa->devs[pcan_idx(dev)] = dev;
	 *
	 * because "struct pcan_adapter" is used as a strict base type and the
	 * "devs[]" array is in fact really defined at the end of the derived
	 * struct types (see "struct pcan_pci_adapter" and
	 * "struct pcifd_adapter" for ex)
	 */

	/* device hw version is the one read from the adapter */
	dev->hw_ver = &pa->hw_ver;
}

struct pcandev *__pcan_alloc_dev(char *type_str, u16 type, int index)
{
	const int sizeof_dev = sizeof(struct pcandev);
	struct pcandev *dev = pcan_malloc(sizeof_dev, GFP_KERNEL);

	if (!dev) {
		pr_err(DEVICE_NAME ": FATAL: failed to alloc dev %s CAN%u\n",
		       type_str, index+1);
		return NULL;
	}

	memset(dev, '\0', sizeof_dev);

	dev->type = type_str;
	dev->wType = type;
	dev->can_idx = index;

	return dev;
}

struct pcandev *__pcan_free_dev(struct pcandev *dev)
{
#ifndef NETDEV_SUPPORT
	if (rxqprealloc)
		dev->rx_msgs = pcan_free(dev->rx_msgs);
#endif

	if (txqprealloc)
		dev->tx_msgs = pcan_free(dev->tx_msgs);

	if (dev->flags & PCAN_DEV_STATIC)
		return dev;

	pcan_free(dev);
	return NULL;
}

/* called just before pcan_free_dev(), that is the device can't be used next.
 * the goal of this is to keep the "dev" pointer alive only.
 */
void __pcan_destroy_dev(struct pcandev *dev)
{
#ifdef NETDEV_SUPPORT
	pcan_netdev_unregister(dev);
#endif

	pcan_sysfs_dev_node_destroy(dev);

	/* Thank you Brad Johnson and his team from Astronics! */
#ifndef NETDEV_SUPPORT
	/* delete STATUS[PCANFD_BUS_LOAD] timer */
	if (dev->features & PCAN_DEV_BUSLOAD_RDY)
		del_timer_sync(&dev->bus_load_timer);
#endif

	/* prevent from calling more than once the cleanup proc */
	pcan_cleanup_dev(dev);

	dev->wInitStep = 0xff;
	dev->filter = pcan_delete_filter_chain(dev->filter);

	if (dev->nOpenPaths >= 0) {
		dev->nOpenPaths = 0;
		pcan_release_path(dev, NULL);
	}

	pcan_mutex_destroy(&dev->mutex);
}

static int pcan_get_opt_u32(struct pcandev *dev,
			struct pcanfd_option *opt, void *c, u32 tmp)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(tmp));
#endif

	opt->size = sizeof(tmp);
	if (pcan_copy_to_user(opt->value, &tmp, opt->size, c)) {
		pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

int pcan_get_dev_features(struct pcandev *dev, u32 *features)
{
	*features = 0;

	if (dev->features & PCAN_DEV_FD_RDY)
		*features |= PCANFD_FEATURE_FD;
	if (dev->features & PCAN_DEV_TXPAUSE_RDY)
		*features |= PCANFD_FEATURE_IFRAME_DELAYUS;
	if (dev->features & PCAN_DEV_BUSLOAD_RDY)
		*features |= PCANFD_FEATURE_BUSLOAD;
	if (dev->features & PCAN_DEV_HWTS_RDY)
		*features |= PCANFD_FEATURE_HWTIMESTAMP;
	if (dev->features & PCAN_DEV_SLF_RDY)
		*features |= PCANFD_FEATURE_SELFRECEIVE;
	if (dev->features & PCAN_DEV_ECHO_RDY)
		*features |= PCANFD_FEATURE_ECHO;
	if (dev->features & PCAN_DEV_MSD_RDY)
		*features |= PCANFD_FEATURE_MSD;
	if (dev->features & PCAN_DEV_TS_SOF_RDY)
		*features |= PCANFD_FEATURE_TS_SOF;
	if (dev->features & PCAN_DEV_SELF_ACK_RDY)
		*features |= PCANFD_FEATURE_SELF_ACK;
	if (dev->features & PCAN_DEV_BRS_IGN_RDY)
		*features |= PCANFD_FEATURE_BRS_IGN;

	if (dev->option[PCANFD_OPT_DEVICE_ID].get)
		*features |= PCANFD_FEATURE_DEVICEID;

	if (dev->features & PCAN_DEV_DEVDATA)
		*features |= PCANFD_FEATURE_DEVDATA;
	if (dev->features & PCAN_DEV_NEW_FW_AV)
		*features |= PCANFD_FEATURE_NEW_FW_AV;

	return 0;
}

static int pcan_get_channel_features(struct pcandev *dev,
				     struct pcanfd_option *opt, void *c)
{
	u32 tmp32;

	pcan_get_dev_features(dev, &tmp32);
	return pcan_get_opt_u32(dev, opt, c, tmp32);
}

static int pcan_get_avclocks(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	int lk;
	u32 ck;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size,
		(uint )sizeof(struct pcanfd_available_clocks));
#endif

	/* Copy only the nb of clock corresponding to the user buffer */
	ck = dev->clocks_list->count;
	lk = sizeof(struct pcanfd_available_clocks_0) +
			ck * sizeof(struct pcanfd_available_clock);

	if (opt->size >= lk)
		opt->size = lk;
	else {
		pr_err(DEVICE_NAME
			": %d bytes buffer needed to copy %s clocks\n",
			lk, dev->adapter->name);
		opt->size = lk;
		return -ENOSPC;
	}

	if (pcan_copy_to_user(opt->value, dev->clocks_list, opt->size, c)) {
		pr_err(DEVICE_NAME ": %s(1): copy_to_user() failure\n",
			__func__);
		return -EFAULT;
	}

	/* update count of clocks given to user (if needed) */
	if (ck < dev->clocks_list->count)
		if (pcan_copy_to_user(opt->value, &ck, sizeof(ck), c)) {
			pr_err(DEVICE_NAME ": %s(2): copy_to_user() failure\n",
				__func__);
			return -EFAULT;
		}

	return 0;
}

static int pcan_get_bittiming_range(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(*dev->bittiming_caps));
#endif

	opt->size = sizeof(*dev->bittiming_caps);
	if (pcan_copy_to_user(opt->value, dev->bittiming_caps, opt->size, c)) {
		pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int pcan_get_dbittiming_range(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(*dev->dbittiming_caps));
#endif

	if (!dev->dbittiming_caps)
		return -EOPNOTSUPP;

	opt->size = sizeof(*dev->dbittiming_caps);
	if (pcan_copy_to_user(opt->value, dev->dbittiming_caps, opt->size, c)) {
		pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int pcan_get_allowed_msgs(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->allowed_msgs));
#endif
	return pcan_get_opt_u32(dev, opt, c, dev->allowed_msgs);
}

static int pcan_set_allowed_msgs(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->allowed_msgs));
#endif

	if (pcan_copy_from_user(&dev->allowed_msgs, opt->value,
					sizeof(dev->allowed_msgs), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int pcan_get_acc_filter_29b(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->acc_29b.value64));
#endif

	opt->size = sizeof(dev->acc_29b.value64);
	if (pcan_copy_to_user(opt->value, &dev->acc_29b.value64,
				opt->size, c)) {
		pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int pcan_set_acc_filter_29b(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->acc_29b.value64));
#endif

	if (pcan_copy_from_user(&dev->acc_29b.value64, opt->value,
					sizeof(dev->acc_29b.value64), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	dev->acc_29b.mask &= CAN_MAX_EXTENDED_ID;
	dev->acc_29b.code &= ~dev->acc_29b.mask;

	return 0;
}

static int pcan_get_acc_filter_11b(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->acc_11b.value64));
#endif

	opt->size = sizeof(dev->acc_11b.value64);
	if (pcan_copy_to_user(opt->value, &dev->acc_11b.value64,
					opt->size, c)) {
		pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int pcan_set_acc_filter_11b(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->acc_11b.value64));
#endif

	if (pcan_copy_from_user(&dev->acc_11b.value64, opt->value,
					sizeof(dev->acc_11b.value64), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	dev->acc_11b.mask &= CAN_MAX_STANDARD_ID;
	dev->acc_11b.code &= ~dev->acc_11b.mask;

	return 0;
}

static int pcan_get_ifrm_delay_us(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	return (dev->features & PCAN_DEV_TXPAUSE_RDY) ?
		pcan_get_opt_u32(dev, opt, c, dev->tx_iframe_delay_us) :
		-EOPNOTSUPP;
}

static int pcan_set_ifrm_delay_us(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	if (!(dev->features & PCAN_DEV_TXPAUSE_RDY))
		return -EOPNOTSUPP;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u): delay=%u\n",
		__func__, opt->size, (uint )sizeof(dev->tx_iframe_delay_us),
		dev->tx_iframe_delay_us);
#endif

	if (pcan_copy_from_user(&dev->tx_iframe_delay_us, opt->value,
					sizeof(dev->tx_iframe_delay_us), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int pcan_get_self_ack(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	if (!(dev->features & PCAN_DEV_SELF_ACK_RDY))
		return -EOPNOTSUPP;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d): slf_ack=%u\n",
		__func__, opt->size, !!(dev->flags & PCAN_DEV_SELF_ACK));
#endif

	return pcan_get_opt_u32(dev, opt, c,
				!!(dev->flags & PCAN_DEV_SELF_ACK));
}

static int pcan_set_self_ack(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	u32 tmp;
	int err;

#ifdef DEBUG_TRACE
	if (pcan_copy_from_user(&tmp, opt->value, sizeof(tmp), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	pr_info(DEVICE_NAME ": %s(tmp=%u)\n", __func__, tmp);
#endif

	if (!(dev->features & PCAN_DEV_SELF_ACK_RDY))
		return -EOPNOTSUPP;

#ifndef DEBUG_TRACE
	if (pcan_copy_from_user(&tmp, opt->value, sizeof(tmp), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}
#endif
	if (tmp) {
		dev->flags &= ~PCAN_DEV_SELF_ACK;
		err = ucan_set_options(dev, UCAN_OPTION_SELF_ACK, 0);
		if (!err)
			dev->flags |= PCAN_DEV_SELF_ACK;
		else
			pr_err(DEVICE_NAME
				": %s CAN%u: failed to set slf-ack (err %d)\n",
				dev->adapter->name, pcan_idx(dev)+1, err);
	} else {
		dev->flags |= PCAN_DEV_SELF_ACK;
		err = ucan_clr_options(dev, UCAN_OPTION_SELF_ACK, 0);
		if (!err)
			dev->flags &= ~PCAN_DEV_SELF_ACK;
		else
			pr_err(DEVICE_NAME
			       ": %s CAN%u: failed to clear slf-ack (err %d)\n",
			       dev->adapter->name, pcan_idx(dev)+1, err);
	}

	return err;
}

static int pcan_get_brs_ign(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	if (!(dev->features & PCAN_DEV_BRS_IGN_RDY))
		return -EOPNOTSUPP;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d): brs_ign=%u\n",
		__func__, opt->size, !!(dev->flags & PCAN_DEV_BRS_IGN));
#endif
	return pcan_get_opt_u32(dev, opt, c,
				!!(dev->flags & PCAN_DEV_BRS_IGN));
}

static int pcan_set_brs_ign(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	u32 tmp;
	int err;

#ifdef DEBUG_TRACE
	if (pcan_copy_from_user(&tmp, opt->value, sizeof(tmp), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	pr_info(DEVICE_NAME ": %s(tmp=%u)\n", __func__, tmp);
#endif

	if (!(dev->features & PCAN_DEV_BRS_IGN_RDY))
		return -EOPNOTSUPP;

#ifndef DEBUG_TRACE
	if (pcan_copy_from_user(&tmp, opt->value, sizeof(tmp), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}
#endif

	if (tmp) {
		dev->flags &= ~PCAN_DEV_BRS_IGN;
		err = ucan_set_options(dev, UCAN_OPTION_BRS_IGN, 0);
		if (!err)
			dev->flags |= PCAN_DEV_BRS_IGN;
		else
			pr_err(DEVICE_NAME
				": %s CAN%u: failed to set brs-ign (err %d)\n",
				dev->adapter->name, pcan_idx(dev)+1, err);
	} else {
		dev->flags |= PCAN_DEV_BRS_IGN;
		err = ucan_clr_options(dev, UCAN_OPTION_BRS_IGN, 0);
		if (!err)
			dev->flags &= ~PCAN_DEV_BRS_IGN;
		else
			pr_err(DEVICE_NAME
			       ": %s CAN%u: failed to clear brs-ign (err %d)\n",
			       dev->adapter->name, pcan_idx(dev)+1, err);
	}

	return err;
}

#ifndef NETDEV_SUPPORT
static int pcan_get_ts_mode(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	u32 tmp = dev->ts_mode;

	if (dev->flags & PCAN_DEV_TS_SOF)
		tmp |= PCANFD_OPT_HWTIMESTAMP_SOF;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u): ts_mode=%u\n",
		__func__, opt->size, (uint )sizeof(dev->ts_mode), tmp);
#endif

	return pcan_get_opt_u32(dev, opt, c, tmp);
}

static int _pcan_set_ts_mode(struct pcandev *dev, u32 ts_mode, bool fallback)
{
	u32 sof_flag = (ts_mode & PCANFD_OPT_HWTIMESTAMP_SOF);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(ts_mode=%u, fallback=%d)\n",
		__func__, ts_mode, fallback);
#endif

	ts_mode &= ~PCANFD_OPT_HWTIMESTAMP_SOF;

	switch (ts_mode) {
	case PCANFD_OPT_HWTIMESTAMP_DEF:

		ts_mode = PCANFD_OPT_HWTIMESTAMP_COOKED;

		/* fall through */
		fallthrough;
	case PCANFD_OPT_HWTIMESTAMP_ON:
	case PCANFD_OPT_HWTIMESTAMP_COOKED:
		if (dev->features & PCAN_DEV_HWTSC_RDY)
			break;

		if (!fallback)
			return -ENOTSUPP;

		ts_mode = PCANFD_OPT_HWTIMESTAMP_RAW;

		/* fall through */
		fallthrough;
	case PCANFD_OPT_HWTIMESTAMP_RAW:
		if (dev->features & PCAN_DEV_HWTS_RDY)
			break;

		if (!fallback)
			return -ENOTSUPP;

		ts_mode = PCANFD_OPT_HWTIMESTAMP_OFF;

		/* fall through */
		fallthrough;
	case PCANFD_OPT_HWTIMESTAMP_OFF:
		break;

		/* fall through */
		fallthrough;
	default:
		return -EINVAL;
	}

	/* set or clear the option according to SOF bit.
	 * Note: up to now, the TS_SOF feature is avalable only on uCAN FW
	 */
	if (dev->features & PCAN_DEV_TS_SOF_RDY) {
		if (sof_flag) {
			if (!ucan_set_options(dev, UCAN_OPTION_TS_SOF, 0))
				dev->flags |= PCAN_DEV_TS_SOF;

		} else {
			if (!ucan_clr_options(dev, UCAN_OPTION_TS_SOF, 0))
				dev->flags &= ~PCAN_DEV_TS_SOF;
		}
	} else if (sof_flag && !fallback) {
		return -ENOTSUPP;
	}

	dev->ts_mode = ts_mode;

	return 0;
}

static int pcan_set_ts_mode(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	u32 tmp;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->ts_mode));
#endif

	if (pcan_copy_from_user(&tmp, opt->value, sizeof(tmp), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return _pcan_set_ts_mode(dev, tmp, false);
}
#endif

static int pcan_get_drv_version(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	const u32 tmp32 = PCAN_MAKE_VERSION(PCAN_VERSION_MAJOR,
						PCAN_VERSION_MINOR,
						PCAN_VERSION_SUBMINOR);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d)\n", __func__, opt->size);
#endif

	return pcan_get_opt_u32(dev, opt, c, tmp32);
}

static int pcan_get_drv_clk_ref(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d)\n", __func__, opt->size);
#endif

	return pcan_get_opt_u32(dev, opt, c, drvclkref);
}

static int pcan_get_linger(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->linger_opt_value));
#endif

	/* int are always 4 bytes long */
	return pcan_get_opt_u32(dev, opt, c, (u32 )dev->linger_opt_value);
}

static int pcan_set_linger(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	int tmp;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(dev->linger_opt_value));
#endif

	if (pcan_copy_from_user(&tmp, opt->value, sizeof(tmp), c)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	dev->linger_opt_value = tmp;

	return 0;
}

static int pcan_get_fw_version(struct pcandev *dev,
					struct pcanfd_option *opt, void *c)
{
	const u32 tmp32 = PCAN_MAKE_VERSION(dev->hw_ver->major,
					    dev->hw_ver->minor,
					    dev->hw_ver->subminor);
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(u_size=%d k_size=%u)\n",
		__func__, opt->size, (uint )sizeof(tmp32));
#endif

	return pcan_get_opt_u32(dev, opt, c, tmp32);
}

static struct pcanfd_options pcan_def_opts[PCANFD_OPT_MAX] = 
{
	[PCANFD_OPT_CHANNEL_FEATURES] = {
		.req_size = sizeof(u32),
		.get = pcan_get_channel_features,
	},
	[PCANFD_OPT_AVAILABLE_CLOCKS] = {
		.req_size = sizeof(struct pcanfd_available_clocks_1),
		.get = pcan_get_avclocks,
	},
	[PCANFD_OPT_BITTIMING_RANGES] = {
		.req_size = sizeof(struct pcanfd_bittiming_range),
		.get = pcan_get_bittiming_range,
	},
	[PCANFD_OPT_DBITTIMING_RANGES] = {
		.req_size = sizeof(struct pcanfd_bittiming_range),
		.get = pcan_get_dbittiming_range,
	},
	[PCANFD_OPT_ALLOWED_MSGS] = {
		.req_size = sizeof(u32),
		.get = pcan_get_allowed_msgs,
		.set = pcan_set_allowed_msgs,
	},
	[PCANFD_OPT_ACC_FILTER_11B] = {
		.req_size = sizeof(u64),
		.get = pcan_get_acc_filter_11b,
		.set = pcan_set_acc_filter_11b,
	},
	[PCANFD_OPT_ACC_FILTER_29B] = {
		.req_size = sizeof(u64),
		.get = pcan_get_acc_filter_29b,
		.set = pcan_set_acc_filter_29b,
	},
	[PCANFD_OPT_IFRAME_DELAYUS] = {
		.req_size = sizeof(u32),
		.get = pcan_get_ifrm_delay_us,
		.set = pcan_set_ifrm_delay_us,
	},
#ifndef NETDEV_SUPPORT
	[PCANFD_OPT_HWTIMESTAMP_MODE] = {
		.req_size = sizeof(u32),
		.get = pcan_get_ts_mode,
		.set = pcan_set_ts_mode,
	},
#endif
	[PCANFD_OPT_DRV_VERSION] = {
		.req_size = sizeof(u32),
		.get = pcan_get_drv_version,
	},
	[PCANFD_OPT_FW_VERSION] = {
		.req_size = sizeof(u32),
		.get = pcan_get_fw_version,
	},
	[PCANFD_OPT_DRV_CLK_REF] = {
		.req_size = sizeof(u32),
		.get = pcan_get_drv_clk_ref,
	},
	[PCANFD_OPT_LINGER] = {
		.req_size = sizeof(int),	/* always 4 bytes long */
		.get = pcan_get_linger,
		.set = pcan_set_linger
	},
	[PCANFD_OPT_SELF_ACK] = {
		.req_size = sizeof(u32),
		.get = pcan_get_self_ack,
		.set = pcan_set_self_ack,
	},
	[PCANFD_OPT_BRS_IGNORE] = {
		.req_size = sizeof(u32),
		.get = pcan_get_brs_ign,
		.set = pcan_set_brs_ign,
	},
};

const struct pcanfd_options *pcan_inherit_options_from(
				struct pcanfd_options *child_opts,
				const struct pcanfd_options *parent_opts)
{
	int i;

	if (!parent_opts)
		parent_opts = pcan_def_opts;

	/* copy parent option only if child's isn't NULL */
	for (i = 0; i < PCANFD_OPT_MAX; i++) {
		if (!child_opts[i].req_size)
			child_opts[i].req_size = parent_opts[i].req_size;
		if (!child_opts[i].get)
			child_opts[i].get = parent_opts[i].get;
		if (!child_opts[i].set)
			child_opts[i].set = parent_opts[i].set;
	}

	return child_opts;
}

/* CAN 2.0 old API entry point (btr0btr1 with 8*MHz clock)
 * This function does the correct conversion of 8*MHz wBTR0BTR1 according to 
 * the default clock, then opens a CAN 2.0 channel with calling the
 * (new API) device_open_fd() entry point.
 */
static int pcan_device_open_fd_wrapper(struct pcandev *dev, u16 btr0btr1,
					u8 ext, u8 listen_only)
{
	struct pcanfd_init fdi = {};
	TPCANInit init = {
		.wBTR0BTR1 = btr0btr1,
		.ucCANMsgType = ext ? MSGTYPE_EXTENDED : MSGTYPE_STANDARD,
		.ucListenOnly = listen_only,
	};

#if defined(DEBUG_TRACE) || defined(DEBUG_OLD_API)
	pr_info(DEVICE_NAME
		": %s(CAN%u): %s(btr0btr1=%04xh, ext=%u, listen_only=%u)\n",
		dev->adapter->name, pcan_idx(dev)+1,
		__func__, btr0btr1, ext, listen_only);
#endif
	return dev->device_open_fd(dev, pcan_init_to_fd(dev, &fdi, &init));
}

/*
 * default locking mechanism to protect user task access to resources shared
 * with ISR.
 */
static void pcan_device_lock_irqsave(struct pcandev *dev,
				     pcan_lock_irqsave_ctxt *pflags)
{
	pcan_lock_get_irqsave(&dev->isr_lock, *pflags);
}

static void pcan_device_unlock_irqrestore(struct pcandev *dev,
					  pcan_lock_irqsave_ctxt *pflags)
{
	pcan_lock_put_irqrestore(&dev->isr_lock, *pflags);
}

/* init some equal parts of dev */
void pcan_soft_init_ex(struct pcandev *dev,
			const struct pcanfd_available_clocks *clocks,
			const struct pcanfd_bittiming_range *pc,
			u32 features)
{
	const u32 sysclock_Hz = clocks->list[0].clock_Hz;

	switch (dev->wType) {
	case HW_ISA:
	case HW_DONGLE_SJA:
	case HW_DONGLE_SJA_EPP:
	case HW_DONGLE_PRO:
	case HW_DONGLE_PRO_EPP:
	case HW_ISA_SJA:
	case HW_PCI:
	case HW_PCCARD:
		/* all of these old devices are SJA1000 based devices */
		features |= PCAN_DEV_ERRCNT_RDY;
		break;

	default:
		/* all of these devices have hw timestamps that can be cooked */
		features |= PCAN_DEV_HWTS_RDY|PCAN_DEV_HWTSC_RDY;
		break;
	}
	dev->features = features;
	dev->flags = 0;

#ifndef NETDEV_SUPPORT
	dev->bus_load_ind_period = msecs_to_jiffies(defblperiod);
	dev->bus_load_count = 0;
	dev->bus_load_total = 0;

	_pcan_set_ts_mode(dev, deftsmode, true);

	if (dev->features & PCAN_DEV_BUSLOAD_RDY) {
		pcan_setup_timer(&dev->bus_load_timer, pcan_push_bus_load_ind,
				 (unsigned long )dev);
	}

	pcan_sync_init(dev);
#endif

	/* fake to force pcan_set_bus_state(UNKNOWN) to work */
	dev->bus_state = PCANFD_ERROR_BUSOFF;
	pcanfd_dev_open_init(dev);

	dev->option = pcan_def_opts;

	dev->sysfs_dev = NULL;

	dev->nOpenPaths = 0;
	dev->nLastError = 0;
	dev->bus_error = PCANFD_ERRMSG_COUNT;
	dev->adapter = NULL;
	dev->wCANStatus = 0;
	dev->filter = NULL;
	dev->sysfs_attrs = NULL;

	dev->linger_opt_value = MAX_WAIT_UNTIL_CLOSE;

	dev->device_alt_num = 0xffffffff;

	dev->bittiming_caps = pc;
	dev->clocks_list = clocks;

	dev->sysclock_Hz = sysclock_Hz;

	/* default CAN device running fw version is the version of the 
	 * real hardware equipment. PCAN-USB X6 devices are quite 
	 * different since they embed 3x USB interfaces that might run 
	 * 3x different FW...
	 */
	dev->hw_ver = NULL;

	memset(&dev->def_init_settings, '\0', sizeof(dev->def_init_settings));
	dev->def_init_settings.clock_Hz = sysclock_Hz;

	if (pcan_def_bitrate) {
		dev->def_init_settings.nominal.bitrate = pcan_def_bitrate;

		/* normalize default bit-timings specs */
		pcan_bittiming_normalize(&dev->def_init_settings.nominal,
					dev->sysclock_Hz,
					dev->bittiming_caps);
	} else {

		/* first, compute nominal bitrate from BTR0BTR1 */
		pcan_btr0btr1_to_bittiming(&dev->def_init_settings.nominal,
					   btr0btr1);

		/* if default clock is not 8*MHz, rebuild BTR0BTR1
		 * accordingly...
		 */
		if (dev->sysclock_Hz != 8*MHz) {

			/* compute real bittimings with real clock value */
			dev->def_init_settings.nominal.brp = 0;

			pcan_bittiming_normalize(
					&dev->def_init_settings.nominal,
					dev->sysclock_Hz,
					dev->bittiming_caps);
		}
	}

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s() btr0btr1=%04xh => nominal bitrate=%u bps\n",
		__func__, btr0btr1, dev->def_init_settings.nominal.bitrate);
#endif

	/* do the same for dbitrate */
	dev->def_init_settings.data.bitrate = pcan_def_dbitrate;
	dev->dbittiming_caps = NULL;

	pcanfd_copy_init(&dev->init_settings, &dev->def_init_settings);

	memset(&dev->props, 0, sizeof(dev->props));

	/* set default access functions: 
	 * init device_open() callback with wrapper to new device_open_fd()
	 * callback. Old controlers will then overload it with their own. 
	 * Newer (CAN FD) will then just have to provide their own
	 * device_open_fd() only.
	 */
	dev->device_open = pcan_device_open_fd_wrapper;

	/* set default locking mechanism protecting access to resources
	 * shared by ISR
	 */
	dev->lock_irq = pcan_device_lock_irqsave;
	dev->unlock_irq = pcan_device_unlock_irqrestore;

	dev->device_open_fd = NULL;
	dev->device_release = NULL;
	dev->device_write  = NULL;
	dev->cleanup = NULL;

	dev->device_params = NULL;    /* the default */

	dev->is_plugged = 1;  /* assume the device IS installed */
	dev->ucActivityState = ACTIVITY_NONE;

	/* suppose the device ready to write frames */
	pcan_lock_init(&dev->isr_lock);
	pcan_set_tx_engine(dev, TX_ENGINE_CLOSED);

	if (txqprealloc) {
		if (pcan_alloc_tx_msgs(dev))
			pr_warn(DEVICE_NAME
				"Tx fifo buffer pre-allocation failed. Will retry when device is opened\n");
	} else {
		dev->tx_msgs = NULL;
	}

	memset(&dev->tx_fifo, '\0', sizeof(dev->tx_fifo));

#ifdef NETDEV_SUPPORT
	dev->netdev = NULL;
#else

	if (rxqprealloc) {
		if (pcan_alloc_rx_msgs(dev))
			pr_warn(DEVICE_NAME
				"Rx fifo buffer pre-allocation failed. Will retry when device is opened\n");
	} else {
		dev->rx_msgs = NULL;
	}

	memset(&dev->rx_fifo, '\0', sizeof(dev->rx_fifo));

#endif
	pcan_lock_init(&dev->wlock);
	pcan_mutex_init(&dev->mutex);
}

/* create all declared Peak legacy devices */
static int make_legacy_devices(void)
{
	int result = 0;
	int i;

#if defined(DEBUG_TRACE)
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	for (i = 0; ((i < 8) && (type[i] != NULL)); i++) {
#ifdef DEBUG
		pr_info(DEVICE_NAME ": %s(): create devices for type=\"%s\"\n",
			__func__, type[i]);
#endif
#ifdef ISA_SUPPORT
		if (!strncmp(type[i], "isa", 4))
			result = pcan_create_isa_devices(type[i],
							io[i], irq[i]);
#endif

#ifdef DONGLE_SUPPORT
		if (!strncmp(type[i], "sp", 4) ||
					!strncmp(type[i], "epp", 4))
			result = pcan_create_dongle_devices(type[i],
							io[i], irq[i]);
#endif

		if (result)
			break;
	}

#if defined(ISA_SUPPORT) && defined(PCAN_HANDLE_IRQ_SHARING)
	/* create lists of devices with the same irqs */
	pcan_create_isa_shared_irq_lists();
#endif

	return result;
}

static int parmtoul(char *parm, u32 *pv)
{
	if (parm[0] == '0' && (parm[1] == 'x' || parm[1] == 'X')) {
		char *endptr = parm;
		u32 v = simple_strtoul(parm, &endptr, 16);
		if (*endptr)
			return -EINVAL;

		if (pv)
			*pv = v;
		return 'x';
	}

	return (strtounit(parm, pv, "kM") < 0) ? -EINVAL : 'd';
}

/* called when the device is installed 'insmod pcan.o' or 'insmod pcan.ko' */
static int __init pcan_init_module(void)
{
	int result = 0;

	memset(&pcan_drv, 0, sizeof(pcan_drv));
	pcan_drv.wInitStep = 0;

	/* in this version, "bitrate" parameter (and new "dbitrate" parameter)
	 * is a string parameter.
	 * Rule (for compatibility purpose):
	 * - if the string starts with "0x" and if the value is < 0xffff,
	 *   then it is considered as a BTR0BTR1 value
	 * - otherwise, the parameter should be a numeric value, optionaly
	 *   followed by "M" (mega) or "k" (kilo), to specify a new default
	 *   value for the nominal bitrate of the CAN channels.
	 */
	if (bitrate &&
	    (parmtoul(bitrate, &pcan_def_bitrate) == 'x') &&
	    (pcan_def_bitrate <= 0xffff)) {
		btr0btr1 = (u16 )pcan_def_bitrate;
		pcan_def_bitrate = 0;
	}

	if (dbitrate)
		strtounit(dbitrate, &pcan_def_dbitrate, "kM");

#ifdef PCAN_DEV_RXQSIZE_MIN
	/* check whether Rx/Tx queue default sizes are ok */
	if (rxqsize < PCAN_DEV_RXQSIZE_MIN)
		rxqsize = (ushort )PCAN_DEV_RXQSIZE_MIN;
#endif
#ifdef PCAN_DEV_RXQSIZE_MAX
	if (rxqsize > PCAN_DEV_RXQSIZE_MAX)
		rxqsize = (ushort )PCAN_DEV_RXQSIZE_MAX;
#endif
#ifdef PCAN_DEV_TXQSIZE_MIN
	if (txqsize < PCAN_DEV_TXQSIZE_MIN)
		txqsize = (ushort )PCAN_DEV_TXQSIZE_MIN;
#endif
#ifdef PCAN_DEV_TXQSIZE_MAX
	if (txqsize > PCAN_DEV_TXQSIZE_MAX)
		txqsize = (ushort )PCAN_DEV_TXQSIZE_MAX;
#endif
#ifdef PCANFD_TX_HIWAT_MIN
	if (txqhiwat < PCANFD_TX_HIWAT_MIN)
		txqhiwat = (ushort )PCANFD_TX_HIWAT_MIN;
#endif
#ifdef PCANFD_TX_HIWAT_MAX
	if (txqhiwat > PCANFD_TX_HIWAT_MAX)
		txqhiwat = (ushort )PCANFD_TX_HIWAT_MAX;
#endif
	if ((dmamask < PCAN_DEV_DMA_MASK_LOW) ||
			(dmamask > PCAN_DEV_DMA_MASK_HIGH))
		dmamask = PCAN_DEV_DMA_MASK_DEF;

#ifndef NETDEV_SUPPORT
	if (deftsmode > PCANFD_OPT_HWTIMESTAMP_MAX)
		deftsmode = PCANFD_OPT_HWTIMESTAMP_MAX;
#endif

	/* check driver clock reference */
	switch (drvclkref) {
	case CLOCK_REALTIME:		/* 0 */
	case CLOCK_MONOTONIC:		/* 1 */
	case CLOCK_MONOTONIC_RAW:	/* 4 */
	case CLOCK_BOOTTIME:		/* 7 */
		break;

	default:
		drvclkref = PCANFD_DRIVER_CLOCK_DEF;
		break;
	}

#ifdef RTAI
	/* this should be done at least once */
	start_rt_timer(0);
#endif
	/* store time for timestamp relation, increments in usec */
	pcan_gettimeofday(&pcan_drv.sInitTime);

	/* get the release name global */
	pcan_drv.szVersionString = CURRENT_RELEASE;
	pcan_drv.nMajor = PCAN_MAJOR;

	pr_info(DEVICE_NAME ": %s (%s)\n", pcan_drv.szVersionString,
#if defined(__BIG_ENDIAN)
		"be"
#else
		"le"
#endif
		);

	pr_info(DEVICE_NAME ": driver config%s\n", current_config);
	if (!pcan_drv.sInitTime.tv_sec && !pcan_drv.sInitTime.tv_usec)
		pr_warn(DEVICE_NAME ": WARNING: got abnormal NULL time\n");

#ifdef DEBUG
	pr_info(DEVICE_NAME ": driver start time=%u.%06u s.\n",
		(u32 )pcan_drv.sInitTime.tv_sec,
		(u32 )pcan_drv.sInitTime.tv_usec);
	pr_info(DEVICE_NAME ": DEBUG is switched on\n");

	/* give a quick descr of some module parameters values */
#ifndef NETDEV_SUPPORT
	pr_info(DEVICE_NAME ": rxqsize=%u\n", rxqsize);
	pr_info(DEVICE_NAME ": rxqprealloc=%u\n", rxqprealloc);
#endif
	pr_info(DEVICE_NAME ": txqsize=%u\n", txqsize);
	pr_info(DEVICE_NAME ": txqprealloc=%u\n", txqprealloc);
	pr_info(DEVICE_NAME ": txqhiwat=%u\n", txqhiwat);
#endif

	/* Copy the centered string only once and use sizeof() for
	 * compiletime value calculation and optimisation. Also ensure
	 * to have a valid current_config and that it fits into config[]
	 */
	if ((sizeof(current_config) > 3) &&
				(sizeof(config) > sizeof(current_config)))
		memcpy(config + (sizeof(config)-sizeof(current_config))/2,
			current_config,
			strlen(current_config));

	INIT_LIST_HEAD(&pcan_drv.devices);
	pcan_drv.wDeviceCount = 0;

#ifdef HANDLE_HOTPLUG
	/* initialize mutex used to access pcan devices list */
	pcan_lock_init(&pcan_drv.devices_lock);
#endif

	/* register the driver by the OS */
#ifdef NO_RT
	result = register_chrdev(pcan_drv.nMajor, DEVICE_NAME, &pcan_fops);
	if (result < 0) {
#ifdef HANDLE_HOTPLUG
		pcan_lock_destroy(&pcan_drv.devices_lock);
#endif
		goto fail;
	}
	if (!pcan_drv.nMajor)
		pcan_drv.nMajor = result;
#else
	INIT_LIST_HEAD(&pcan_rtdm_dev_list);
#endif

#ifdef SYSFS_SUPPORT
	/* register /sys/class/pcan */
	if (class_register(&pcan_class))
		goto fail;

	pcan_drv.class = &pcan_class;
#endif

	pcan_drv.wInitStep = 1;

#ifdef PCI_SUPPORT
#ifdef PCAN_PCI_EVENT_DRIVEN
	pcan_pci_init();
#else
	pcan_search_and_create_pci_devices();
#endif
	/* search pci devices */
#endif

	/* create isa and dongle devices */
	make_legacy_devices();

#ifdef USB_SUPPORT
	/* register usb devices only */
	pcan_usb_register_devices();
#endif

#ifdef PCCARD_SUPPORT
	pcan_pccard_register_devices();
#endif

#if !defined USB_SUPPORT && !defined PCCARD_SUPPORT
	/* no device found, stop all */
	if (!pcan_drv.wDeviceCount)
		goto fail;
#endif

	pcan_drv.wInitStep = 2;

	result = DEV_REGISTER();
	if (result < 0)
		goto fail;

	if (!pcan_drv.nMajor)
		pcan_drv.nMajor = result;

	pcan_drv.wInitStep = 3;

#ifdef CREATE_PROC_ENTRY_DEPRECATED
	proc_file_entry = proc_create(DEVICE_NAME, 0, NULL, &proc_file_fops);
	if (!proc_file_entry) {
		result = -ENOMEM;
		goto fail;
	}
#else
	/* create the proc entry */
	if (create_proc_read_entry(DEVICE_NAME, 0, NULL,
					pcan_read_procmem, NULL) == NULL) {
		/* maybe wrong if there is no proc filesystem configured */
		result = -ENODEV;
		goto fail;
	}
#endif
	pcan_drv.wInitStep = 4;

	pr_info(DEVICE_NAME ": major %d.\n", pcan_drv.nMajor);

	/* succeed */
	return 0;

fail:
	pcan_cleanup_module();
	return result;
}

module_init(pcan_init_module);
module_exit(pcan_cleanup_module);
