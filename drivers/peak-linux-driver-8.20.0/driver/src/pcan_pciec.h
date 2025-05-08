/* SPDX-License-Identifier: GPL-2.0 */
/*
 * all parts to handle device interface specific for PCAN-PCIExpressCard
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
#ifndef __PCAN_PCIEC_H__
#define __PCAN_PCIEC_H__

#include <src/pcan_main.h>

PCAN_PCIEC_CARD *pcan_pciec_create_card(struct pci_dev *pciDev,
					struct pcandev *dev);
PCAN_PCIEC_CARD *pcan_pciec_locate_card(struct pci_dev *pciDev,
					struct pcandev *dev);
void pcan_pciec_delete_card(struct pcandev *dev);
void pcan_setVCCEN(PCAN_PCIEC_CARD *card, int On);

#endif /* __PCAN_PCIEC_H__ */
