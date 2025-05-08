/* SPDX-License-Identifier: GPL-2.0 */
/*
 * all parts to handle the interface specific parts of pcan-pci
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
 *               Philipp Baer <philipp.baer@informatik.uni-ulm.de>
 *               Armin Bauer <armin.bauer@desscon.com>
 */
/* #define DEBUG */
/* #undef DEBUG */

#include "src/pcan_common.h"	/* must always be the 1st include */

#include <linux/ioport.h>
#include <linux/pci.h>		/* all about pci */
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include "src/pcan_pci.h"
#include "src/pcan_sja1000.h"
#include "src/pcan_filter.h"
#include "src/pcan_pci_spi.h"

#ifdef PCIEC_SUPPORT
#include "src/pcan_pciec.h"
#endif

/* Since PCAN_PCI_ENABLE_MSI is defined in pcan_pci.h, undefining
 * PCAN_PCI_ENABLE_MSI here enables to exclusively work with INTA
 * for the boards handled by this driver
 */
//#undef PCAN_PCI_ENABLE_MSI

#ifdef DEBUG_PCI_LITE
#define DEBUG_TRACE
#define DEBUG_MSI
#endif

/* important PITA registers */
#define PITA_ICR         0x00        /* interrupt control register */
#define PITA_GPIOICR     0x18        /* GPIO interface control register */
#define PITA_MISC        0x1C        /* miscellaneous register */

#define PEAK_PCI_VENDOR_ID	0x001C	/* the PCI device and vendor IDs */

#define PCAN_PCI_ID		0x0001  /* PCI / PCIe Slot cards */
#define PCAN_EXPRESSCARD_ID	0x0002	/* PCAN-ExpressCard */
#define PCAN_PCIE_ID		0x0003	/* PCIe Slot cards */
#define PCAN_CPCI_ID		0x0004	/* cPCI */
#define PCAN_MINIPCI_ID		0x0005	/* miniPCI */
#define PCAN_PC104PLUSQUAD_ID	0x0006	/* new PC-104 Plus Quad */
#define PCAN_PCI104E_ID		0x0007	/* PCI-104 Express */
#define PCAN_MINIPCIE_ID	0x0008	/* miniPCIe Slot cards */
#define PCAN_CHIPPCIE_ID	0x0009	/* PCAN-Chip PCIe */
					/* (former PCAN-PCI Express OEM) */
#define PCAN_EXPRESSCARD34_ID	0x000a	/* PCAN-Express Card 34 */

/* CAN-FD devices id. range start */
#define PEAK_PCICANFD_ID	0x0010

#define PCAN_PCIEFD10_ID	(PEAK_PCICANFD_ID + 0)
#define PCAN_PCIEFD_ID		(PEAK_PCICANFD_ID + PCAN_PCIE_ID)
#define PCAN_CPCIEFD_ID		(PEAK_PCICANFD_ID + PCAN_CPCI_ID)
#define PCAN_PC104PFD_ID	(PEAK_PCICANFD_ID + PCAN_PC104PLUSQUAD_ID)
#define PCAN_PCI104EFD_ID	(PEAK_PCICANFD_ID + PCAN_PCI104E_ID)
#define PCAN_MINIPCIEFD_ID	(PEAK_PCICANFD_ID + PCAN_MINIPCIE_ID)
#define PCAN_CHIPPCIEFD_ID	(PEAK_PCICANFD_ID + PCAN_CHIPPCIE_ID)
#define PCAN_M2_ID		(PEAK_PCICANFD_ID + PCAN_EXPRESSCARD34_ID)

#define PCI_CONFIG_PORT_SIZE	0x1000  /* size of the config io-memory */
#define PCI_PORT_SIZE		0x0400  /* size of a channel io-memory */

#ifdef LINUX_26
#define pci_find_device(v, d, x) pci_get_device(v, d, x)
#endif

#define VERSION_REG1		0x40
#define VERSION_REG2		0x44
#define VERSION_REG2_MASK	0xfff
#define VERSION_REG2_MSI	0x110

/* Available area in flash where PCAN device info are/might be stored:
 * the base address depends on the device type (4x CAN channels or not) */
#define PCI_FLASH_DEVID_OFFSET		0x130000
#define PCIx4_FLASH_DEVID_OFFSET	0x230000

/*
 * GLOBALS
 */

/* internal PCI/PCIe devices database */
static const struct pcan_pci_adapter_desc {
	char *name;
	struct {
		u16 subsysid;
		char *text;
	} partnum[4];
} pcan_pci_adapter_desc[] = {
	[PCAN_PCI_ID] = {				/* 0x0001 */
		.name = "PCAN-PCI",
		.partnum = {
			{ 0x0001, "IPEH-002064" },
			{ 0x0004, "IPEH-002065" },
			{ 0x0002, "IPEH-002066" },
			{ 0x0005, "IPEH-002067" },
		},
	},

	[PCAN_PCIE_ID] = {				/* 0x0003 */
		.name = "PCAN-PCI Express",
		.partnum = {
			{ 0x0002, "IPEH-003026" },
			{ 0x0005, "IPEH-003027" },
			{ 0x0014, "IPEH-003040" },
		},
	},

	[PCAN_CPCI_ID] = {				/* 0x0004 */
		.name = "PCAN-cPCI",
		.partnum = {
			{ 0x0000, "Not used yet" },
		},
	},

	[PCAN_MINIPCI_ID] = {				/* 0x0005 */
		.name = "PCAN-miniPCI",
		.partnum = {
			{ 0x0000, "Not used yet" },
		},
	},

	[PCAN_PC104PLUSQUAD_ID] = {			/* 0x0006 */
		.name = "PCAN-PC/104-Plus Quad",
		.partnum = {
			{ 0x0014, "IPEH-002099" },
		},
	},

	[PCAN_PCI104E_ID] = {				/* 0x0007 */
		.name = "PCAN-PCI/104-Express",
		.partnum = {
			{ 0x0001, "IPEH-003054" },
			{ 0x0004, "IPEH-003055" },
			{ 0x0002, "IPEH-003056" },
			{ 0x0005, "IPEH-003057" },
		},
	},

	[PCAN_MINIPCIE_ID] = {				/* 0x0008 */
		.name = "PCAN-miniPCIe",
		.partnum = {
			{ 0x0002, "IPEH-003048" },
			{ 0x0005, "IPEH-003049" },
		},
	},

	[PCAN_CHIPPCIE_ID] = {				/* 0x0009 */
		.name = "PCAN-Chip PCIe",
		.partnum = {
			{ 0x0000, "IPES-002092" },
			{ 0x0000, "IPES-002092-QUAD" },
		},
	},

#ifdef PCIEC_SUPPORT
	[PCAN_EXPRESSCARD_ID] = {			/* 0x0002 */
		.name = "PCAN-ExpressCard",
		.partnum = {
			{ 0x0001, "IPEH-003000" },
			{ 0x0004, "IPEH-003001" },
			{ 0x0002, "IPEH-003002" },
			{ 0x0005, "IPEH-003003" },
		},
	},

	[PCAN_EXPRESSCARD34_ID] = {			/* 0x000a */
		.name = "PCAN-ExpressCard 34",
		.partnum = {
			{ 0x0002, "IPEH-003004" },
		},
	},
#endif

	/* CAN-FD devices */
	[PCAN_PCIEFD10_ID] = {				/* 0x0010 */
		.name = "PCAN-PCI Express FD",
		.partnum = {
			{ 0x0000, "Prototype" },
		},
	},

	[PCAN_PCIEFD_ID] = {				/* 0x0013 */
		.name = "PCAN-PCI Express FD",
		.partnum = {
			{ 0x0002, "IPEH-004026" },
			{ 0x0005, "IPEH-004027" },
			{ 0x0014, "IPEH-004040" },
		},
	},

	[PCAN_CPCIEFD_ID] = {				/* 0x0014 */
		.name = "PCAN-cPCIe FD",
		.partnum = {
			{ 0x0002, "IPEH-004041" },
			{ 0x0005, "IPEH-004042" },
			{ 0x0014, "IPEH-004043" },
		},
	},

	[PCAN_PC104PFD_ID] = {				/* 0x0016 */
		.name = "PCAN-PC/104-Plus FD",
		.partnum = {
			{ 0x0002, "IPEH-004096" },
			{ 0x0005, "IPEH-004097" },
			{ 0x0014, "IPEH-004098" },
		},
	},

	[PCAN_PCI104EFD_ID] = {				/* 0x0017 */
		.name = "PCAN-PCI/104-Express FD",
		.partnum = {
			{ 0x0002, "IPEH-004080" },
			{ 0x0005, "IPEH-004081" },
			{ 0x0014, "IPEH-004082" },
		},
	},

	[PCAN_MINIPCIEFD_ID] = {			/* 0x0018 */
		.name = "PCAN-miniPCIe FD",
		.partnum = {
			{ 0x0002, "IPEH-004045" },
			{ 0x0005, "IPEH-004046" },
			{ 0x0014, "IPEH-004047" },
		},
	},

	[PCAN_CHIPPCIEFD_ID] = {			/* 0x0019 */
		.name = "PCAN-Chip PCIe FD",
		.partnum = {
			{ 0x0000, "IPES-004092" },
			{ 0x0000, "IPES-004093" },
		},
	},

	[PCAN_M2_ID] = {				/* 0x001a */
		.name = "PCAN-M.2",
		.partnum = {
			{ 0x0002, "IPEH-004083" },
			{ 0x0005, "IPEH-004084" },
			{ 0x0014, "IPEH-004085" },
		},
	},
};

static const struct pci_device_id pcan_pci_tbl[] = {
	{PEAK_PCI_VENDOR_ID, PCAN_PCI_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
	{PEAK_PCI_VENDOR_ID, PCAN_PCIE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
	{PEAK_PCI_VENDOR_ID, PCAN_CPCI_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
	{PEAK_PCI_VENDOR_ID, PCAN_MINIPCI_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
	{PEAK_PCI_VENDOR_ID, PCAN_PC104PLUSQUAD_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
	{PEAK_PCI_VENDOR_ID, PCAN_PCI104E_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
	{PEAK_PCI_VENDOR_ID, PCAN_MINIPCIE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
	{PEAK_PCI_VENDOR_ID, PCAN_CHIPPCIE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
#ifdef PCIEC_SUPPORT
	{PEAK_PCI_VENDOR_ID, PCAN_EXPRESSCARD_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
	{PEAK_PCI_VENDOR_ID, PCAN_EXPRESSCARD34_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0},
#endif

	/* CAN-FD devices */
	{ PCI_DEVICE(PEAK_PCI_VENDOR_ID, PCAN_PCIEFD10_ID) },
	{ PCI_DEVICE(PEAK_PCI_VENDOR_ID, PCAN_PCIEFD_ID) },
	{ PCI_DEVICE(PEAK_PCI_VENDOR_ID, PCAN_CPCIEFD_ID) },
	{ PCI_DEVICE(PEAK_PCI_VENDOR_ID, PCAN_PC104PFD_ID) },
	{ PCI_DEVICE(PEAK_PCI_VENDOR_ID, PCAN_PCI104EFD_ID) },
	{ PCI_DEVICE(PEAK_PCI_VENDOR_ID, PCAN_MINIPCIEFD_ID) },
	{ PCI_DEVICE(PEAK_PCI_VENDOR_ID, PCAN_CHIPPCIEFD_ID) },
	{ PCI_DEVICE(PEAK_PCI_VENDOR_ID, PCAN_M2_ID) },

	{0, }
};

MODULE_DEVICE_TABLE(pci, pcan_pci_tbl);

#ifdef PCAN_PCI_ENABLE_MSI
static unsigned char usemsi = PCAN_PCI_USEMSI_DEFAULT;
module_param(usemsi, byte, 0644);
MODULE_PARM_DESC(usemsi, " 0=INTA; 1=MSI (not shared); 2=MSI (shared) (def="
			__stringify(PCAN_PCI_USEMSI_DEFAULT) ")");
#endif

/* ugly (but historical) global count of ALL the pci channel devices */
int _pci_devices = 0;

/* count of SJA1000 PCI devices */
static int pcan_pci_sja1000_adapters = 0;

/* count of SJA1000 channels devices */
static int pcan_pci_sja1000_devices = 0;

static void pcan_pci_unregister_driver(struct pci_driver *p_pci_drv);

/* read a register */
static u8 pcan_pci_readreg(struct pcandev *dev, u8 port)
{
	u32 lPort = (u32 )port << 2;
	return readb(dev->port.pci.can_port_addr + lPort);
}

/* write a register */
static void pcan_pci_writereg(struct pcandev *dev, u8 port, u8 data)
{
	u32 lPort = (u32 )port << 2;
	writeb(data, dev->port.pci.can_port_addr + lPort);
}

static const u16 pita_icr_masks[] = { 0x0002, 0x0001, 0x0040, 0x0080 };

/* enable interrupt in PITA */
static void pcan_pci_enable_pita_interrupt(struct pcandev *dev)
{
	u16 pita_icr_high = readw(dev->port.pci.bar0_cfg_addr + PITA_ICR + 2);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u): PITA ICR=%04Xh\n",
		__func__, dev->can_idx+1, pita_icr_high);
#endif
	pita_icr_high |= pita_icr_masks[dev->can_idx];
	writew(pita_icr_high, dev->port.pci.bar0_cfg_addr + PITA_ICR + 2);
}

/* disable interrupt in PITA */
static void pcan_pci_disable_pita_interrupt(struct pcandev *dev)
{
	u16 pita_icr_high = readw(dev->port.pci.bar0_cfg_addr + PITA_ICR + 2);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u): PITA ICR=%04Xh\n",
		__func__, dev->can_idx+1, pita_icr_high);
#endif

	pita_icr_high &= ~pita_icr_masks[dev->can_idx];
	writew(pita_icr_high, dev->port.pci.bar0_cfg_addr + PITA_ICR + 2);

	/* read it again, to wait for write command to complete */
	readw(dev->port.pci.bar0_cfg_addr + PITA_ICR + 2);
}

/* interface depended open and close */
static int pcan_pci_open(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info("%s: %s()\n", DEVICE_NAME, __func__);
#endif
	dev->ucActivityState = ACTIVITY_IDLE;
	return 0;
}

/*
 *  Second callback called by close().
 */
static int pcan_pci_release(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info("%s: %s()\n", DEVICE_NAME, __func__);
#endif

	dev->ucActivityState = ACTIVITY_INITIALIZED;
	return 0;
}

/*
 * Third (and last) callback called by close().
 */
static void pcan_pci_free_irq(struct pcandev *dev, struct pcanusr *dev_priv)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	/* disable interrupt in PITA */
	pcan_pci_disable_pita_interrupt(dev);

#ifdef PCAN_PCI_MSI_WORKAROUND
	if (usemsi == PCAN_PCI_USEMSI_INTA)
#endif
		pcan_free_irq(dev);

	dev->wInitStep = 5;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#define pcan_pci_enable_msi_range(a, b, c)	\
	pci_alloc_irq_vectors_affinity(a, b, c, PCI_IRQ_MSI, NULL)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
#define pcan_pci_enable_msi_range(a, b, c)	\
	pci_alloc_irq_vectors(a, b, c, PCI_IRQ_MSI)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#define pcan_pci_enable_msi_range(a, b, c)	pci_enable_msi_range(a, b, c)

#else
/**
 * pci_enable_msi_range - configure device's MSI capability structure
 * @dev: device to configure
 * @minvec: minimal number of interrupts to configure
 * @maxvec: maximum number of interrupts to configure
 *
 * This function tries to allocate a maximum possible number of interrupts in a
 * range between @minvec and @maxvec. It returns a negative errno if an error
 * occurs. If it succeeds, it returns the actual number of interrupts allocated
 * and updates the @dev's irq member to the lowest new interrupt number;
 * the other interrupt numbers allocated to this device are consecutive.
 **/
static int pcan_pci_enable_msi_range(struct pci_dev *dev,
					int minvec, int maxvec)
{
	int nvec = maxvec;
	int rc;

	if (maxvec < minvec)
		return -ERANGE;

	do {
		rc = pci_enable_msi_block(dev, nvec);
		if (rc < 0) {
			return rc;
		} else if (rc > 0) {
			if (rc < minvec)
				return -ENOSPC;
			nvec = rc;
		}
	} while (rc);

	return nvec;
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0) */

void pcan_pci_disable_msi(struct pci_dev *pciDev)
{
#ifdef PCAN_PCI_ENABLE_MSI
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
	pci_free_irq_vectors(pciDev);
#else
	pci_disable_msi(pciDev);
#endif
#endif
}

int pcan_pci_enable_msi(struct pcan_pci_adapter *pa, int can_count, int irq_max)
{

	/* No need to do that since pcan_pci_enable_msi() is called according to
	 * PCI/PCIe version which tells if MSI is supported or not.
	 */
	if (irq_max < 0)
		irq_max = can_count;

	/* in some PCIe arch and/or Kernel version:
	 * looks like with x86 APIC, Linux Kernel won't give us anything else
	 * than "1" (see "native_setup_msi_irqs()" in
	 * arch/x86/kernel/apic/io_apic.c
	 */
	pa->msi_count = pcan_pci_enable_msi_range(pa->dev, 1, irq_max);

#ifdef DEBUG_MSI
	pr_info(DEVICE_NAME ": enabling [1..%u] MSI status: %d\n",
		irq_max, pa->msi_count);
#endif
	if (pa->msi_count <= 0) {

		pr_err(DEVICE_NAME ": enabling MSI failed (err %d)\n",
			pa->msi_count);

		pr_info(DEVICE_NAME
			": fallback into INTA mode IRQ%u\n", pa->dev->irq);

		pa->msi_count = 0;

		/* return -1 to be sure to set usemsi = PCAN_PCI_USEMSI_INTA*/
		return -1;

#ifdef DEBUG_MSI
	} else {
	
		pr_info(DEVICE_NAME ": %u MSI enabled from INT%u\n",
			pa->msi_count, pa->dev->irq);
#endif
	}

	return 0;
}

#ifndef NO_RT
/* RT version of the IRQ handler */
static int pcan_pci_irqhandler(rtdm_irq_t *irq_context)
{
	struct pcandev *dev = rtdm_irq_get_arg(irq_context, struct pcandev);

#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t pcan_pci_irqhandler(int irq, void *arg, struct pt_regs *regs)
{
	struct pcandev *dev = (struct pcandev *)arg;
#else
static irqreturn_t pcan_pci_irqhandler(int irq, void *arg)
{
	struct pcandev *dev = (struct pcandev *)arg;
#endif
	const u16 pita_icr_mask = pita_icr_masks[dev->can_idx];
	irqreturn_t err = PCAN_IRQ_NONE;

	/* check whether this INT is for the given 'dev' */
	u16 pita_icr = readw(dev->port.pci.bar0_cfg_addr + PITA_ICR);
	while (pita_icr & pita_icr_mask) {

		/* whatever it was a SJA1000 INT or not, this PITA INT has been
		 * handled (this avoids INTA "irq nobody care" issue) */
		err = PCAN_IRQ_HANDLED;

		/* handle INT into the SJA1000 */
		if (pcan_sja1000_irqhandler(dev) == PCAN_IRQ_NONE) {

			/* clear corresponding INTerrupt in PITA */
			writew(pita_icr_mask,
					dev->port.pci.bar0_cfg_addr + PITA_ICR);

			/* and exit (this) ISR */
			break;
		}

		pita_icr = readw(dev->port.pci.bar0_cfg_addr + PITA_ICR);
	}

	return err;
}

static int __pcan_pci_req_irq(struct pcandev *dev)
{
	int err, irq_flags = PCAN_IRQF_SHARED;

#ifdef PCAN_PCI_ENABLE_MSI
	/* if driver got the requested count of MSI, IRQ is not shared */
	if (pci_dev_msi_enabled(dev->port.pci.pciDev))
		if (!(dev->flags & PCAN_DEV_MSI_SHARED))
			irq_flags &= ~PCAN_IRQF_SHARED;
#endif /* PCAN_PCI_ENABLE_MSI */

#ifndef NO_RT
	/* RT irq requesting */
	err = rtdm_irq_request(&dev->irq_handle,
			dev->wIrq,
			pcan_pci_irqhandler,
			irq_flags | RTDM_IRQTYPE_EDGE,
#else
	/* using legacy interrupt mechanism */
	err = request_irq(dev->wIrq,
			pcan_pci_irqhandler,
			irq_flags,
#endif
			DEVICE_NAME,
			dev);

	if (err)
		pr_err(DEVICE_NAME ": %s(CAN%u): failed to request irq %d "
			"flags=%0xh (err %d)\n",
			dev->adapter->name, dev->can_idx+1,
			dev->wIrq, irq_flags, err);

	return err;
}

static int pcan_pci_req_irq(struct pcandev *dev, struct pcanusr *dev_priv)
{
	int err = 0;

#ifdef PCAN_PCI_MSI_WORKAROUND
	if (usemsi == PCAN_PCI_USEMSI_INTA)
#endif
		if ((err = __pcan_pci_req_irq(dev)))
			return err;

	pcan_pci_enable_pita_interrupt(dev);

	dev->wInitStep = 6;

	return err;
}

static int pcan_pci_cleanup(struct pcandev *dev)
{
	if (pcan_pci_sja1000_devices <= 0) {
		pr_info(DEVICE_NAME
			": %s(): ABNORMAL pcan_pci_sja1000_devices=%d! "
			"Nothing done next (pcan_pci_sja1000_adapters=%d)\n",
			__func__, pcan_pci_sja1000_devices,
			pcan_pci_sja1000_adapters);
		return 0;
	}

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s CAN%u)"
		" _pci_devices=%d pcan_pci_sja1000_adapters=%d"
		" sja100 devices=%d wInitStep=%u\n",
		__func__, dev->adapter->name, dev->can_idx+1,
		_pci_devices, pcan_pci_sja1000_adapters,
		pcan_pci_sja1000_devices, dev->wInitStep);
#endif

	switch (dev->wInitStep) {
	case 6:
		pcan_pci_free_irq(dev, NULL);

		/* fall through */
		fallthrough;
	case 5:
#ifdef PCAN_PCI_MSI_WORKAROUND
		if (usemsi != PCAN_PCI_USEMSI_INTA)
			pcan_free_irq(dev);
#endif
#ifdef PCIEC_SUPPORT
		pcan_pciec_delete_card(dev);
#endif
		/* fall through */
		fallthrough;
	case 4:
		iounmap(dev->port.pci.can_port_addr);

		/* fall through */
		fallthrough;
	case 3:
		release_mem_region(dev->dwPort, PCI_PORT_SIZE);

		/* fall through */
		fallthrough;
	case 2:
		/* SGr note: DON'T call  pcan_remove_dev_from_list(dev) here
		 * because it uses the same mutex than
		 * pcan_pci_sja1000_remove()
		 */
		if (!dev->can_idx)
			iounmap(dev->port.pci.bar0_cfg_addr);

		/* fall through */
		fallthrough;
	case 1:
		if (!dev->can_idx)
			release_mem_region(dev->port.pci.dwConfigPort,
							PCI_CONFIG_PORT_SIZE);
		/* fall through */
		fallthrough;
	case 0:
		dev->filter = pcan_delete_filter_chain(dev->filter);

		_pci_devices--;
		pcan_pci_sja1000_devices--;

#ifdef UDEV_SUPPORT
#ifndef PCAN_PCI_EVENT_DRIVEN
		if (!pcan_pci_sja1000_devices)
			pcan_pci_unregister_driver(&pcan_drv.pci_drv);
#endif
#endif
	}

	return 0;
}

static int pci_device_params(struct pcandev *dev, TPEXTRAPARAMS *params)
{
	struct pcan_pci_adapter *pcan_pci = to_pcan_pci_adapter(dev->adapter);
	__le32 flash_devid_cache;
	int err = 0;
#ifdef PCAN_PCI_SINGLE_DEVID
	const int dev_idx = 0;
#else
	const int dev_idx = dev->can_idx;
#endif

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u, f=%d, p=%08xh)\n",
		__func__, dev->can_idx+1, params->nSubFunction,
		params->func.dwSerialNumber);
#endif

	switch (params->nSubFunction) {
	case SF_GET_SERIALNUMBER:
		params->func.dwSerialNumber =
			le32_to_cpu(pcan_pci->flash_data_cache.serial_number);
		break;

	case SF_GET_HCDEVICENO:
		params->func.dwSerialNumber = dev->device_alt_num;
		break;

	case SF_SET_HCDEVICENO:

#ifdef PCAN_PCI_SINGLE_DEVID
		/* 0xffffffff value is allowed to disable the usage of device
		 * number for the device */
		if (params->func.dwSerialNumber != 0xffffffff) {

			/* flashed devid is always 1st port devid: */
			u32 can0_devid = params->func.dwSerialNumber -
								dev->can_idx;

			/* protect from negative numbers */
			if (can0_devid > params->func.dwSerialNumber) {
				err = -EINVAL;
				break;
			}

			params->func.dwSerialNumber = can0_devid;
		}
#endif

		/* save current flash cached value */
		flash_devid_cache =
			pcan_pci->flash_data_cache.device_number[dev_idx];

		/* update recordsize *ONLY* if it is unknown */
		if (pcan_pci->flash_data_cache.recordsize == 0xffffffff)
			pcan_pci->flash_data_cache.recordsize =
					cpu_to_le32(PCAN_PCI_FLASH_SIZEOF);

		pcan_pci->flash_data_cache.device_number[dev_idx] =
				cpu_to_le32(params->func.dwSerialNumber);

		err = pcan_pci_spi_flash_write(pcan_pci,
					    pcan_pci->flash_data_addr,
					    &pcan_pci->flash_data_cache,
					    sizeof(struct pcan_pci_flash_data));

		/* Update caches according to result: */
		if (err) {

			/* in case of failure: restore old (current) value
			 * in flash cache */
			pcan_pci->flash_data_cache.device_number[dev_idx] =
				flash_devid_cache;
		} else {
			/* in case of success: update device_alt_num cache
			 * value */
			dev->device_alt_num = params->func.dwSerialNumber;

			if (dev->device_alt_num != 0xffffffff) {
#ifdef PCAN_PCI_SINGLE_DEVID
				/* in that case, dwSerialNumber contains
				 * can0_devid. CAN idx must be added:
				 */
				dev->device_alt_num += dev->can_idx;
#endif
				dev->flags |= PCAN_DEV_USES_ALT_NUM;
			} else {
				dev->flags &= ~PCAN_DEV_USES_ALT_NUM;
			}
		}
		break;

	default:
		pr_warn(DEVICE_NAME ": %s CAN%u: Unknown sub-function %d!\n",
			dev->adapter->name, dev->can_idx+1,
			params->nSubFunction);

		return -EINVAL;
	}

	return err;
}

static int pci_get_devid(struct pcandev *dev,
				struct pcanfd_option *opt, void *c)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s CAN%u):\n",
		__func__, dev->adapter->name, dev->can_idx+1);
#endif

	opt->size = sizeof(dev->device_alt_num);
	if (pcan_copy_to_user(opt->value, &dev->device_alt_num, opt->size, c)) {
		pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

static int pci_set_devid(struct pcandev *dev,
				struct pcanfd_option *opt, void *c)
{
	TPEXTRAPARAMS ext_param = { .nSubFunction = SF_SET_HCDEVICENO, };
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s CAN%u)\n",
		__func__, dev->adapter->name, dev->can_idx+1);
#endif

	err = pcan_copy_from_user(&ext_param.func.dwSerialNumber,
				  opt->value, sizeof(u32), c);
	if (err) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		return -EFAULT;
	}

	return pci_device_params(dev, &ext_param);
}

/* PCIe device specific options */
static struct pcanfd_options pcan_pci_options[PCANFD_OPT_MAX] =
{
	[PCANFD_OPT_DEVICE_ID] = {
		.req_size = sizeof(u32),
		.get = pci_get_devid,
		.set = pci_set_devid,
	},
};

int pcan_pci_set_dev_adapter(struct pcandev *dev,
			     struct pcan_pci_adapter *pcan_pci)
{
	pcan_set_dev_adapter(dev, &pcan_pci->adapter);

	if (pcan_pci->flash_data_addr) {
		__le32 *pdn = pcan_pci->flash_data_cache.device_number;

#ifndef PCAN_PCI_SINGLE_DEVID
		pdn += dev->can_idx;
#endif
		/* cache device id from flash (cache) value */
		dev->device_alt_num = le32_to_cpu(*pdn);

		if (dev->device_alt_num != 0xffffffff) {
			dev->flags |= PCAN_DEV_USES_ALT_NUM;

#ifdef PCAN_PCI_SINGLE_DEVID
			/* in case a device id is defined, then generate a
			 * unique one for the channel device */
			dev->device_alt_num += dev->can_idx;
#endif
		}

		/* old API callback */
		dev->device_params = pci_device_params;

		/* set the pci specifc options to this device */
		dev->option = pcan_pci_options;
	}

	return 0;
}

static int pcan_pci_channel_init(struct pcan_pci_adapter *pcan_pci,
				 struct pcandev *dev)
{
	struct pci_dev *pciDev = pcan_pci->dev; /* shortcut */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,0,0)
	int err;
#endif

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u) _pci_devices=%d\n",
		__func__, dev->can_idx+1, _pci_devices);
#endif

	/* obsolete - will be removed soon */
	if (dev->can_idx)
		dev->props.ucMasterDevice = CHANNEL_SLAVE;
	else
		dev->props.ucMasterDevice = CHANNEL_MASTER;

	/* set this before any instructions */
	dev->wInitStep = 2;
	dev->readreg = pcan_pci_readreg;
	dev->writereg = pcan_pci_writereg;
	dev->cleanup = pcan_pci_cleanup;
	dev->req_irq = pcan_pci_req_irq;
	dev->free_irq = pcan_pci_free_irq;
	dev->open = pcan_pci_open;
	dev->release = pcan_pci_release;
	dev->nMajor = pcan_drv.nMajor;
	dev->nMinor = PCAN_PCI_MINOR_BASE + _pci_devices;
	dev->filter = pcan_create_filter_chain();

	dev->port.pci.dwConfigPort = (u32 )pciDev->resource[0].start;
	dev->dwPort = (u32 )pciDev->resource[1].start + dev->can_idx *
								PCI_PORT_SIZE;

	dev->port.pci.bar0_cfg_addr = pcan_pci->bar0_addr;
	dev->wIrq = (u16 )pciDev->irq;

	/* adjust MSI/INTA irq from adapter device IRQ value */
	dev->flags &= ~PCAN_DEV_MSI_SHARED;

#ifdef PCAN_PCI_ENABLE_MSI
	if (pci_dev_msi_enabled(pcan_pci->dev)) {

		if (usemsi != PCAN_PCI_USEMSI_SHARED)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
			dev->wIrq = pci_irq_vector(pciDev, dev->can_idx);
#else
			dev->wIrq += dev->can_idx;
#endif

		else
			dev->flags |= PCAN_DEV_MSI_SHARED;
	}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,0,0)
	err = check_mem_region(dev->dwPort, PCI_PORT_SIZE);
	if (err) {
		pr_info(DEVICE_NAME
			": %s(%u) check_mem_region(%d, %d) err=%d\n",
			__func__, __LINE__, dev->dwPort, PCI_PORT_SIZE, err);

		return err;
	}
#endif

	if (!request_mem_region(dev->dwPort, PCI_PORT_SIZE, DEVICE_NAME)) {
		pr_info(DEVICE_NAME
			": %s(%u) request_mem_region(%d, %d) failed\n",
			__func__, __LINE__, dev->dwPort, PCI_PORT_SIZE);
		dev->wInitStep--;
		return -EBUSY;
	}

	dev->wInitStep = 3;

	dev->port.pci.can_port_addr = ioremap(dev->dwPort, PCI_PORT_SIZE);
	if (!dev->port.pci.can_port_addr) {
		pr_info(DEVICE_NAME
			": %s(%u) ioremap(%d, %d) failure\n",
			__func__, __LINE__, dev->dwPort, PCI_PORT_SIZE);
		release_mem_region(dev->dwPort, PCI_PORT_SIZE);
		dev->wInitStep--;
		return -ENODEV;
	}

	dev->wInitStep = 4;

	_pci_devices++;
	dev->wInitStep = 5;

	/* double link the device and the adapter */
	pcan_pci_set_dev_adapter(dev, pcan_pci);
	pci_adapter_dev(pcan_pci, dev->can_idx) = dev;

	if (dev->device_alt_num != 0xffffffff)
		pr_info(DEVICE_NAME
			": - pci/pcie device minor %d number %u found\n",
			dev->nMinor, dev->device_alt_num);
	else
		pr_info(DEVICE_NAME ": - pci/pcie device minor %d found\n",
			dev->nMinor);

	return 0;
}

/*
 * create one pci based devices from peak
 * - this may be one of multiple from a card
 */
static int create_one_pci_device(struct pcan_pci_adapter *pcan_pci, int can_idx)
{
	struct pci_dev *pciDev = pcan_pci->dev; /* shortcut */
	struct pcandev *local_dev;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, can_idx+1);
#endif

	/* make the first device on board */
	local_dev = pcan_alloc_dev("pci", HW_PCI, can_idx);
	if (!local_dev) {
		err = -ENOMEM;
		goto fail;
	}

	pcan_soft_init(local_dev);

	local_dev->device_open = sja1000_open;
	local_dev->device_write = sja1000_write;
	local_dev->device_release = sja1000_release;
	local_dev->device_reset = sja1000_reset;

	local_dev->props.ucExternalClock = 1;

#ifdef PCIEC_SUPPORT
	/* card pointer must be NULL for all but PCAN-Expresscard */
	local_dev->port.pci.card = NULL;
#endif

	err = pcan_pci_channel_init(pcan_pci, local_dev);
	if (!err)
		err = sja1000_probe(local_dev);

	if (err) {
#ifndef PCIEC_SUPPORT
		/* Thanks Hardi! */
		local_dev->cleanup(local_dev);
#endif
		goto fail_free;
	}

	local_dev->port.pci.pciDev = pciDev;

#ifdef PCIEC_SUPPORT
	/* we have a card with i2c controlled blinking LED */
	if ((pciDev->device == PCAN_EXPRESSCARD_ID) ||
		(pciDev->device == PCAN_EXPRESSCARD34_ID)) {

		/* master channel */
		if (!local_dev->can_idx)
			local_dev->port.pci.card =
				pcan_pciec_create_card(pciDev, local_dev);
		else
			local_dev->port.pci.card =
				pcan_pciec_locate_card(pciDev, local_dev);
	}
#endif

	/* add this device to the list */
	pcan_add_dev_in_list(local_dev);

#ifdef PCAN_PCI_MSI_WORKAROUND
	if (usemsi != PCAN_PCI_USEMSI_INTA)
		__pcan_pci_req_irq(local_dev);
#endif

	return 0;

fail_free:
	pcan_free_dev(local_dev);
fail:
	pr_err(DEVICE_NAME ": %s(CAN%u) discarded: err %d\n",
		__func__, can_idx+1, err);

	return err;
}

int pcan_pci_flash_init(struct pcan_pci_adapter *pcan_pci, u32 addr)
{
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): flash addr=%0xh\n",
		__func__, pcan_pci->adapter.name, addr);
#endif

	/* cache the flash data */
	pcan_pci->flash_data_addr = addr;

	/* default is: device with uninitialized flash memory */
	memset(&pcan_pci->flash_data_cache, 0xff,
			sizeof(pcan_pci->flash_data_cache));

	/* read now its content */
	err = pcan_pci_spi_flash_read(pcan_pci, pcan_pci->flash_data_addr,
				      &pcan_pci->flash_data_cache,
				      sizeof(pcan_pci->flash_data_cache));
	if (err) {

		/* restore uninitialized content in case of any error */
		memset(&pcan_pci->flash_data_cache, 0xff,
			sizeof(pcan_pci->flash_data_cache));

		pr_warn(DEVICE_NAME
			": %s: failed to read flash memory (err %d)\n",
			pcan_pci->adapter.name, err);
#ifdef DEBUG_PCI_LITE
	} else {
		dump_mem("flash data", &pcan_pci->flash_data_cache,
					sizeof(pcan_pci->flash_data_cache));
#endif
	}

	return err;
}

/* move to event driven creation of devices, not for kernels 2.4.x */
static int pcan_pci_sja1000_probe(struct pci_dev *pciDev, u16 wSubSysID,
				  const char *adapter_name,
				  const char *part_num,
				  int can_count)
{
	struct pcan_pci_adapter *pcan_pci;
	int err, i;
	u32 v1;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, pciDev);
#endif
#ifdef DEBUG_PCI
	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		if (pciDev->resource[i].name)
			pr_info(DEVICE_NAME
				": resource[%d]: name=\"%s\" start=%d (%xh) "
				"end=%d (%xh) flags=%08xh\n",
				i,
				pciDev->resource[i].name,
				(int )pciDev->resource[i].start,
				(int )pciDev->resource[i].start,
				(int )pciDev->resource[i].end,
				(int )pciDev->resource[i].end,
				(int )pciDev->resource[i].flags);
	}
#endif

	dev_info(&pciDev->dev, "%s sub-system id %0xh (%u channels)\n",
		adapter_name, wSubSysID, can_count);

	/* configure the PCI chip, part 1 */
	err = pci_write_config_word(pciDev, PCI_COMMAND, 2);
	if (err)
		goto fail;

	err = pci_write_config_word(pciDev, 0x44, 0);
	if (err)
		goto fail;
	wmb();

	pcan_pci = pcan_alloc_adapter_ex(adapter_name, part_num,
					pcan_pci_sja1000_adapters,
					can_count, sizeof(*pcan_pci));
	if (!pcan_pci)
		goto fail;

	pcan_pci->dev = pciDev;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,0,0)
	err = check_mem_region((u32 )pciDev->resource[0].start,
					PCI_CONFIG_PORT_SIZE);
	if (err) {
		pr_info(DEVICE_NAME ": %s() check_mem_region(%xh, %d) err=%d\n",
			__func__, (u32)pciDev->resource[0].start,
			PCI_CONFIG_PORT_SIZE, err);
		goto fail_free;
	}
#endif
	if (!request_mem_region((u32 )pciDev->resource[0].start,
					PCI_CONFIG_PORT_SIZE, DEVICE_NAME)) {
		pr_info(DEVICE_NAME
			": %s(%u) request_mem_region(%xh, %d) failure\n",
			__func__, __LINE__, (u32)pciDev->resource[0].start,
			PCI_CONFIG_PORT_SIZE);
		err = -EBUSY;
		goto fail_free;
	}

	pcan_pci->bar0_addr = ioremap((u32 )pciDev->resource[0].start,
					PCI_CONFIG_PORT_SIZE);

	if (!pcan_pci->bar0_addr) {
		pr_info(DEVICE_NAME ": %s() ioremap(%xh, %d) failure\n",
			__func__, (u32 )pciDev->resource[0].start,
			PCI_CONFIG_PORT_SIZE);
		err = -EIO;
		goto fail_release_regions;
	}

	/* configuration of the PCI chip, part 2: */

	/* set GPIO control register */
	writew(0x0005, pcan_pci->bar0_addr + PITA_GPIOICR + 2);

	/* enable all channels */
	writeb(0x00, pcan_pci->bar0_addr + PITA_GPIOICR);

	/* toggle reset */
	writeb(0x05, pcan_pci->bar0_addr + PITA_MISC + 3);
	mdelay(5);

	/* leave parport mux mode */
	writeb(0x04, pcan_pci->bar0_addr + PITA_MISC + 3);
	wmb();

	v1 = readl(pcan_pci->bar0_addr + VERSION_REG1);
	if (v1) {

		/* this is an FPGA equipped board */
		u32 v2 = readl(pcan_pci->bar0_addr + VERSION_REG2);

		pcan_pci->adapter.hw_ver.major = (v2 & 0x0000f000) >> 12;
		pcan_pci->adapter.hw_ver.minor = (v2 & 0x00000f00) >> 8;
		pcan_pci->adapter.hw_ver.subminor = (v2 & 0x000000f0) >> 4;
		pcan_pci->adapter.hw_ver.extra = 0;

#ifdef PCAN_PCI_ENABLE_MSI
		/* read MSI ability of the board */
		if (((v2 >> 4) & VERSION_REG2_MASK) >= VERSION_REG2_MSI) {

			switch (usemsi) {
			case PCAN_PCI_USEMSI_SHARED:
			case PCAN_PCI_USEMSI_NOTSHARED:
				/* try MSI and accept to share INTerrupt(s) */
				err = pcan_pci_enable_msi(pcan_pci,
							  can_count, -1);
				break;
			default:
				err = -1;
				break;
			}

			/* change usemsi to control when IRQ will be
			 * requested/freed next
			 */
			if (err)
				usemsi = PCAN_PCI_USEMSI_INTA;

			else if (pcan_pci->msi_count == can_count)
				usemsi = PCAN_PCI_USEMSI_NOTSHARED;
			else
				usemsi = PCAN_PCI_USEMSI_SHARED;
		}
#endif

		/* cache the flash data.
		 * Note: flash_data_addr == 0 => SJA1000 card with no flash
		 */
		pcan_pci_flash_init(pcan_pci, (can_count <= 2) ? \
						PCI_FLASH_DEVID_OFFSET : \
						PCIx4_FLASH_DEVID_OFFSET);

		pr_info(DEVICE_NAME ": PCB v%xh FPGA v%u.%u.%u (mode %u)%s\n",
			(v1 & 0xf0000000) >> 28,
			pcan_pci->adapter.hw_ver.major,
			pcan_pci->adapter.hw_ver.minor,
			pcan_pci->adapter.hw_ver.subminor,
			(v2 & 0x0000000f),
			(pcan_pci->flash_data_cache.recordsize == 0xffffffff) \
				? "" : " Flash: Ok");
	}

	for (i = 0; i < can_count; i++) {
		err = create_one_pci_device(pcan_pci, i);
		if (err)
			goto fail_all;
	}

	pcan_pci_sja1000_devices += can_count;
	pcan_pci_sja1000_adapters++;

	return 0;

fail_all:
	while (i-- > 0) {
		struct pcandev *dev = pci_adapter_dev(pcan_pci, i);

		iounmap(dev->port.pci.can_port_addr);
		release_mem_region(dev->dwPort, PCI_PORT_SIZE);

		/* if device was in global devices list, then it has
		 * been initialized, then it can be destroyed
		 */
		if (pcan_remove_dev_from_list(dev))
			pcan_destroy_dev(dev);
	}

	iounmap(pcan_pci->bar0_addr);

fail_release_regions:
	release_mem_region((u32 )pciDev->resource[0].start,
				PCI_CONFIG_PORT_SIZE);
fail_free:
	for (i = 0; i < can_count; i++)
		pcan_free_dev(pci_adapter_dev(pcan_pci, i));

	pcan_free_adapter(&pcan_pci->adapter);

	pcan_pci_disable_msi(pciDev);
fail:
	return err;
}

static int pcan_pci_probe(struct pci_dev *dev, const struct pci_device_id *ent)
{
	const struct pcan_pci_adapter_desc * adapter_desc;
	char *adapter_partnum;
	u16 sub_system_id;
	int err, can_count, i;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(dev=%p, ent:id=%0xh)\n",
		__func__, dev, ent->device);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
	err = pci_enable_device(dev);
	if (err)
		goto fail;
#endif

	err = pci_read_config_word(dev, PCI_SUBSYSTEM_ID, &sub_system_id);
	if (err)
		goto fail_disable_pci;

	/* number of CAN channels depends on the sub-system id */
	if (sub_system_id < 0x0004)
		can_count = 1;
	else if (sub_system_id < 0x0010)
		can_count = 2;
	else if (sub_system_id < 0x0012)
		can_count = 3;
	else
		can_count = 4;

	adapter_desc = &pcan_pci_adapter_desc[dev->device];

	/* in case no subsystem id exist, use the first one as default (if
	 * any)
	 */
	adapter_partnum = adapter_desc->partnum[0].text;
	for (i = 0; i < ARRAY_SIZE(adapter_desc->partnum); i++)
		if (adapter_desc->partnum[i].subsysid == sub_system_id) {
			adapter_partnum = adapter_desc->partnum[i].text;
			break;
		}

	/* consider that devid >= 0x10 => CAN-FD devices */
	if (ent->device >= PEAK_PCICANFD_ID) {
		err = pcan_pcifd_probe(dev, sub_system_id,
				       adapter_desc->name,
				       adapter_partnum,
				       can_count);
	} else {
		err = pcan_pci_sja1000_probe(dev, sub_system_id,
					     adapter_desc->name,
					     adapter_partnum,
					     can_count);
	}

	if (err)
		goto fail_disable_pci;

	return 0;

fail_disable_pci:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
	pci_disable_device(dev);
fail:
#endif
	return err;
}

/*
 * pcan_pci_register_driver() is the common entry point for both versions
 * with or without #define PCAN_PCI_EVENT_DRIVEN. It is also the common
 * part of both SJA1000 and CANFD devices.
 */
static int pcan_pci_register_driver(struct pci_driver *pci_drv)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, pci_drv);
#endif

	/* do inherit default options */
	pcan_inherit_options_from(pcan_pci_options, NULL);

	pci_drv->name = DEVICE_NAME;
	pci_drv->id_table = pcan_pci_tbl;

	return pci_register_driver(pci_drv);
}

static void pcan_pci_unregister_driver(struct pci_driver *pci_drv)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, pci_drv);
#endif

	pci_unregister_driver(pci_drv);
}

#ifdef PCAN_PCI_EVENT_DRIVEN

static void pcan_pci_sja1000_remove(struct pci_dev *pciDev)
{
	struct pcandev *dev;
	struct pcan_adapter *adapter = NULL;
	struct list_head *pos;
	struct list_head *n;

	/* since hotplug devices have been removed before, no more need to 
	 * lock access to the devices list. This is mandatory when in PREEMPT
	 * because free_irq() might be called for PCI/PCIe devices when the
	 * MSI workaround is on
	 */
	list_for_each_prev_safe(pos, n, &pcan_drv.devices) {
		dev = list_entry(pos, struct pcandev, list_dev);
		if ((dev->wType == HW_PCI) &&
					(dev->port.pci.pciDev == pciDev)) {

			pcan_cleanup_dev(dev);
			pcan_destroy_dev(dev);

			list_del(&dev->list_dev);
			pcan_drv.wDeviceCount--;

			/* free all device allocated memory */
			if (!adapter)
				adapter = dev->adapter;

			pcan_free_dev(dev);
		}
	}

	if (adapter)
		pcan_free_adapter(adapter);

	pcan_pci_disable_msi(pciDev);
}

static void pcan_pci_remove(struct pci_dev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p:id=%0Xh)\n",
		__func__, dev, dev->device);
#endif

	if (dev->device >= PEAK_PCICANFD_ID) {
		pcan_pcifd_remove(dev);
	} else {
		pcan_pci_sja1000_remove(dev);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
	pci_disable_device(dev);
#endif
}

int pcan_pci_init(void)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	pcan_drv.pci_drv.probe = pcan_pci_probe;
	pcan_drv.pci_drv.remove = pcan_pci_remove;

	return pcan_pci_register_driver(&pcan_drv.pci_drv);
}

void pcan_pci_deinit(void)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	pcan_pci_unregister_driver(&pcan_drv.pci_drv);
}

#else /* PCAN_PCI_EVENT_DRIVEN */

#ifdef LINUX_26
static inline int pci_present(void)
{
#ifdef CONFIG_PCI
	return 1;
#else
	return 0;
#endif
}
#endif

/* search all pci based devices from peak */
int pcan_search_and_create_pci_devices(void)
{
	const int n = sizeof(pcan_pci_tbl) / sizeof(pcan_pci_tbl[0]) - 1;
	int err = 0;
	int i;

	/* search pci devices */
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	if (!pci_present())
		return 0;

	/* for each device id... */
	for (i = 0; i < n; i++) {
		struct pci_dev *from = NULL;
		struct pci_dev *pciDev;

		/* ...loop looking for all the same adapters */
		do {
			pciDev = pci_find_device(pcan_pci_tbl[i].vendor,
						 pcan_pci_tbl[i].device,
						 from);

			/* no (more) device found with that device id.:
			 * break the current device loop to search for any
			 * other PEAK devices...
			 */
			if (!pciDev)
				break;

			/* a PCI device with PCAN_PCI_VENDOR_ID and
			 * PCAN_PCI_DEVICE_ID was found
			 */
			from = pciDev;

			/* create all corresponding channel devices */
			err = pcan_pci_probe(pciDev, pcan_pci_tbl + i);

		} while (!err);
	}

#ifdef UDEV_SUPPORT
	/* register only if at least one SJA1000 channel has been found */
	if (pcan_pci_sja1000_devices > 0)
		pcan_pci_register_driver(&pcan_drv.pci_drv);
#endif

	return err;
}
#endif /* PCAN_PCI_EVENT_DRIVEN */
