/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * common.h - the header for common parts for transmittest and receivetest
 *
 * Copyright (C) 2001-2020  PEAK System-Technik GmbH <www.peak-system.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contact:      <linux@peak-system.com>
 * Maintainer:   Stephane Grosjean <s.grosjean@peak-system.com>
 * Contribution: Klaus Hitschler <klaus.hitschler@gmx.de>
 */
#include <libpcan.h> 

extern HANDLE h;
extern const char *current_release;

#ifdef __cplusplus
extern "C" {
#endif

void disclaimer(const char *prgName);

void print_message(const char *prompt, TPCANMsg *m);
void print_message_ex(const char *prompt, TPCANRdMsg *mr);

int getTypeOfInterface(char *szTypeName);

const char *getNameOfInterface(int nType);

void print_diag(const char *prgName);

#ifdef __cplusplus
}
#endif
