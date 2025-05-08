/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_filter.h - all about CAN message filtering - interface
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
#ifndef __PCAN_FILTER_H__
#define __PCAN_FILTER_H__

#include "src/pcan_common.h"
#include "src/pcan_main.h"

void *pcan_create_filter_chain(void);
int pcan_add_filter(void *handle, u32 FromID, u32 ToID, u32 flags);
void pcan_delete_filter_all(void *handle);
int pcan_do_filter(void *handle, struct pcanfd_rxmsg *pe);
void *pcan_delete_filter_chain(void *handle);

int pcan_get_filters_count(void *handle);
int pcan_add_filters(void *handle, struct pcanfd_msg_filter *pf, int count);
int pcan_get_filters(void *handle, struct pcanfd_msg_filter *pf, int count);

#endif
