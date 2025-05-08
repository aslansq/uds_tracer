/* SPDX-License-Identifier: GPL-2.0 */
/*
 * all parts to handle the SPI flash of the PCIe devices.
 *
 * Copyright (C) 2020 PEAK System-Technik GmbH <www.peak-system.com>
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
#ifndef __PCAN_PCI_SPI_H__
#define __PCAN_PCI_SPI_H__

#define PCAN_PCI_FLASHED_DEVID		16

/* If defined, the driver handles virtual device numbers for each channel:
 * canX device number = device_number[0] + X
 * If not defined, then:
 * canX device number = device_number[X]
 *
 * PCAN_PCI_SINGLE_DEVID was defined for Beta____20200122_n-8.10.0 only.
 */
//#define PCAN_PCI_SINGLE_DEVID

/* this could be used as a signature value */
#define PCAN_PCI_FLASH_SIZEOF		0x254

struct __packed pcan_pci_flash_data {
	__le32 recordsize;	/* sizeof(this) or 0xffffffff */
	__le32 serial_number;
	__le32 windows_dnr;
	__le32 device_number[PCAN_PCI_FLASHED_DEVID];
	char user_location[256];
	__le64 oem_code;
	char user_data[256];
};

struct pcan_pci_adapter;	/* need peak_pci.h */

int pcan_pci_spi_flash_read(struct pcan_pci_adapter *pcan_pci, u32 addr,
			    void *data, int l);
int pcan_pci_spi_flash_write(struct pcan_pci_adapter *pcan_pci, u32 addr,
			     void *data, int l);

#endif
