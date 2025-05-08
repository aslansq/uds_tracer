/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * parser.h - header of parser which parses the input file and put the messages 
 *            into a list
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
 * Contact:    <linux@peak-system.com>
 * Maintainer: Stephane Grosjean <s.grosjean@peak-system.com>
 * Author:     Klaus Hitschler <klaus.hitschler@gmx.de>
 */
#ifndef __PARSER_H__
#define __PARSER_H__

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <ctype.h>
#include <libpcan.h>
#include <list>

class parser {

	public:

		parser(void);
		parser(const char *filename);
		~parser(void);
	
		int nGetLastError(void);
		void setFileName(const char *filename);
		std::list<TPCANMsg> *Messages(void);
	
	private:

		void skip_blanks(char **ptr);
		int  skip_blanks_and_test_for_CR(char **ptr);
		int  scan_unsigned_number(char **ptr, __u32 *dwResult);
		char scan_char(char **ptr);
		int  parse_input_message(char *buffer, TPCANMsg *Message);
		
		const char *m_szFileName;
		int m_nLastError;
		std::list<TPCANMsg> m_List;
};

#endif // __PARSER_H__
