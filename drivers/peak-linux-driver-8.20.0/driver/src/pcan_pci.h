/* SPDX-License-Identifier: GPL-2.0 */
/*
 * all parts to handle device interface specific for pcan-pci
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
#ifndef __PCAN_PCI_H__
#define __PCAN_PCI_H__

#include "src/pcan_main.h"
#include "src/pcan_pci_spi.h"

/* History: in previous versions of pcan, supporting or not PCIEC was leading
 * to two ways of registering PCI devices...
 * From v8.0, PCI CAN devices creation is always event driven...
 */
/* v7.x RT: PCIEC_SUPPORT was NOT supported, thus PCAN_PCI_EVENT_DRIVEN was'nt
 * defined...
 */
#define PCAN_PCI_EVENT_DRIVEN

#ifdef CONFIG_PCI_MSI
/* If defined, driver will first try to enable MSI mode with the device. On
 * any error, it will fall back into normal INTx mode.
 * If not defined, normal INTx mode will be used, as usual.
 */
#define PCAN_PCI_ENABLE_MSI
//#define PCAN_PCI_ENABLE_MSIX

#ifdef PCAN_PCI_ENABLE_MSI
/* if defined, MSI can be shared. */
#define PCAN_PCI_SHARE_MSI
//#define DEBUG_MSI

/* workaround for using MSI with some archs and to prevent from not having any
 * interrupt any more from the board, when can0 is closed before the other canX:
 * IRQ is requested at _probe() time and is released at _exit
 */
#define PCAN_PCI_MSI_WORKAROUND	

#define PCAN_PCI_USEMSI_INTA		0
#define PCAN_PCI_USEMSI_NOTSHARED	1	/* normal MSI mode */
#define PCAN_PCI_USEMSI_SHARED		2	/* dangerous, might loss INT */
#define PCAN_PCI_USEMSI_DEFAULT		PCAN_PCI_USEMSI_INTA

#endif
#endif /* CONFIG_PCI_MSI */

struct pcan_pci_adapter {
	struct pcan_adapter		adapter;
	struct pci_dev *		dev;
	u32				flash_data_addr;
	struct pcan_pci_flash_data	flash_data_cache;
	void __iomem *			bar0_addr;
	int				msi_count;
	int				msi_step;
#ifdef CONFIG_UBSAN_BOUNDS_STRICT
	struct pcandev *		pci_devs[];
#else
	struct pcandev *		pci_devs[0];
#endif
};

#define to_pcan_pci_adapter(a)	(struct pcan_pci_adapter*)(a)

#define pci_adapter_dev(p, i)	((p)->pci_devs[i])

int pcan_pci_enable_msi(struct pcan_pci_adapter *pa, int can_count,
				int irq_min);

void pcan_pci_disable_msi(struct pci_dev *pciDev);

#ifdef PCAN_PCI_EVENT_DRIVEN
int pcan_pci_init(void);
void pcan_pci_deinit(void);
#else
int pcan_search_and_create_pci_devices(void);
#endif

int pcan_pci_flash_init(struct pcan_pci_adapter *pcan_pci, u32 addr);
int pcan_pci_set_dev_adapter(struct pcandev *dev,
			     struct pcan_pci_adapter *pcan_pci);

/* PCAN-PCi FD support */
extern int pcan_pcifd_probe(struct pci_dev *dev, u16 sub_system_id,
			    const char *adapter_name, const char *part_num,
			    int can_count);
extern void pcan_pcifd_remove(struct pci_dev *dev);

#endif
