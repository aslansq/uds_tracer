/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_parse.c - read input parser and write output formatter
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
 */
/* #define DEBUG */

#include "src/pcan_main.h"

#include <linux/errno.h>
#include <linux/kernel.h>

#include "src/pcan_parse.h"
#include "src/pcanfd_core.h"

/* helper for use in read..., makes a line of formatted output */
int pcan_make_output(char *buffer, struct pcanfd_msg *pf)
{
	char *ptr = buffer;
	char r_or_m = '?', s_or_e = '?';
	int i;

	switch (pf->type) {
	case PCANFD_TYPE_STATUS:

		/* any status frames are x-ed */
		r_or_m = 'x';

		/* new: s_or_e indicate the kind of error: */
		if (pf->flags & PCANFD_ERROR_BUS)
			s_or_e = 'b';
		else if (pf->flags & PCANFD_ERROR_CTRLR)
			s_or_e = 'c';
		else if (pf->flags & PCANFD_ERROR_INTERNAL)
			s_or_e = 'i';
		else
			s_or_e = 'x';
		break;

	case PCANFD_TYPE_CAN20_MSG:
	case PCANFD_TYPE_CANFD_MSG:

		if (pf->flags & MSGTYPE_RTR)
			r_or_m = 'r';
		else if (pf->flags & PCANFD_MSG_BRS)
			r_or_m = 'b';
		else
			r_or_m = 'm';

		if (pf->flags & PCANFD_MSG_SLF)
			r_or_m -= 0x20;

		s_or_e = (pf->flags & MSGTYPE_EXTENDED) ? 'e' : 's';
		break;
	}

	/* print RTR, 11 or 29, CAN-Id and datalength */
	ptr += sprintf(ptr, "%c %c 0x%08x %2d  ", r_or_m, s_or_e,
						pf->id, pf->data_len);

	/* "Prudence est mere de surete"... sauf pour CAN-FD ;-) */

	/* don't print any data if it is a RTR message */
	if (!(pf->flags & MSGTYPE_RTR)) {
		for (i = 0; i < pf->data_len; i++) {
			ptr += sprintf(ptr, "0x%02x ", pf->data[i]);
		}
	}

	if (pf->flags & PCANFD_TIMESTAMP) {
		u32 ms, us;

		ms = pf->timestamp.tv_usec / 1000;
		us = pf->timestamp.tv_usec - (ms * 1000);
		ms += pf->timestamp.tv_sec * 1000;

		/* print timestamp */
		ptr += sprintf(ptr, " %11u %03u", ms, us);
	}

	*ptr++ = '\n';
	return (int)(ptr - buffer);
}

static inline int is_blank(char c)
{
	switch (c) {
	case ' ':
	case '\t':
		return c;
	}

	return 0;
}

static inline int is_eol(char c)
{
	switch (c) {
	case '\0':
	case '\n':
	case '\r':
		return '\n';
	}

	return 0;
}

static inline int is_eow(char c)
{
	return is_blank(c) || is_eol(c);
}

/* skip blanks and tabs */
static inline void skip_blanks(char **ptr)
{
	/* remove blanks or tabs */
	while (is_blank(**ptr))
		(*ptr)++;
}

/* skip blanks, return 0 if the 1st non-blank char is not '\n' */
static int skip_blanks_and_test_for_CR(char **ptr)
{
	/* remove blanks or tabs */
	skip_blanks(ptr);

	return is_eol(**ptr);
}

/* extract a number, either hex or decimal from a string */
static int scan_unsigned_number(char **ptr, u32 *dwResult)
{
	char *p = *ptr;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(\"%s\")\n", __func__, p);
#endif
	*dwResult = simple_strtoul(p, ptr, 0);

	return (p != *ptr) ? 0 : -ERANGE;
}

/* extract a char from a string */
static inline char scan_char(char **ptr)
{
	return *(*ptr)++;
}

/* lengthy helper for use in write..., reject empty and comment lines */
int pcan_parse_input_idle(char *buffer)
{
	char *ptr = buffer;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	/* remove leading blanks */
	skip_blanks(&ptr);

	switch (scan_char(&ptr)) {

	case '#':	/* comment */
	case '\n':
		return 0;

	default:
		return -EINVAL;
	}
}

/* lengthy helper for use in write..., parses a message command */
int pcan_parse_input_message(char *buffer, struct pcanfd_msg *pf)
{
	char *ptr = buffer;
	u32 dwLen;
	u32 dwDat;
	int i, err = -EINVAL;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(\"%s\")\n", __func__, buffer);
#endif

	/* remove leading blanks */
	skip_blanks(&ptr);

	/* search for 'b', 'm' or 'r' to distinguish between message or init
	 * strings
	 */
	pf->type = PCANFD_TYPE_CAN20_MSG;
	pf->flags = 0;
	switch (scan_char(&ptr)) {
	case 'B':
		pf->flags |= PCANFD_MSG_SLF;
		/* fall through */
		fallthrough;
	case 'b':
		pf->flags |= PCANFD_MSG_BRS;
		pf->type = PCANFD_TYPE_CANFD_MSG;
		break;
	case 'M':
		pf->flags |= PCANFD_MSG_SLF;
		/* fall through */
		fallthrough;
	case 'm':
		break; /* normal message */
	case 'R':
		pf->flags |= PCANFD_MSG_SLF;
		/* fall through */
		fallthrough;
	case 'r':
		pf->flags |= MSGTYPE_RTR; /* rtr message */
		break;

	default:
		goto reject;
	}

	if (skip_blanks_and_test_for_CR(&ptr)) /* no CR allowed here */
		goto reject;

	/* read message type */
	switch (scan_char(&ptr)) {
	case 'e':
		pf->flags |= MSGTYPE_EXTENDED;
	case 's':
		break;

	default:  goto reject;
	}

	if (skip_blanks_and_test_for_CR(&ptr))
		goto reject;

	/* read CAN-ID */
	if ((err = scan_unsigned_number(&ptr, &pf->id)))
		goto reject;

	if (pf->flags & MSGTYPE_EXTENDED) {
		if (pf->id > 0x3fffffff)
			goto reject;
	} else {
		if (pf->id > 2047)
			goto reject;
	}

	if (skip_blanks_and_test_for_CR(&ptr))
		goto reject;

	/* read datalength */
	if ((err = scan_unsigned_number(&ptr, &dwLen)))
		goto reject;

	if (dwLen > sizeof(pf->data))
		goto reject;

	/* new since 8.11: if data length > 8, then convert message to CANFD_MSG
	 * type. Next, pcanfd_send_msg() checks if the device is opened to
	 * handle those type of messages. If not, it rejects it and prints an
	 * error message in the kernel logs, just as we would do it now.
	 */
	if (dwLen > PCANFD_CAN20_MAXDATALEN)
		pf->type = PCANFD_TYPE_CANFD_MSG;

	pf->data_len = (u8)dwLen;

	/* read data elements up to message len */
	i = 0;
	while (i < dwLen) {
		if (skip_blanks_and_test_for_CR(&ptr))
			goto reject;

		if ((err = scan_unsigned_number(&ptr, &dwDat)))
			goto reject;
		if (dwDat > 255)
			goto reject;
		pf->data[i] = (u8 )dwDat;

		i++;
	}

	return 0;

reject:
	return err;
}

/*
 * strtounit(argv, "kM");
 * strtouint(argv, "ms");
 */
int strtounit(char *str, u32 *pv, char *units)
{
	char *endptr = str;
	u32 m = 1;

	u32 v = simple_strtoul(str, &endptr, 0);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(\"%s\") = %u (endptr=\"%s\")\n",
		__func__, str, v, endptr);
#endif
	if (*endptr) {
		if (units) {
			char *pu;

			/* might not be invalid if found char is a unit */
			for (pu = units; *pu; pu++) {
				m *= 1000;
				if (*endptr == *pu) {
					endptr++;
					goto lbl_ok;
				}
			}
		}

		return -(*endptr);
	}

lbl_ok:
	if (pv)
		*pv = v * m;

	return endptr - str;
}

/*
 * CANFD new API: initialization string parsing
 *
 * SYNTAX: same than in Windows PCANBasic.h, that is (for example):
 *
 *	"f_clock=80000000,nom_brp=0,nom_tseg1=13,nom_tseg2=0,nom_sjw=0,
 *	 data_brp=0,data_tseg1=13,data_tseg2=0,data_sjw=0"
 *
 * LINUX only:
 *
 *	"nom_bitrate="
 *	"data_bitrate="
 */
static int scan_bitrate_string(char **buffer, struct pcanfd_init *pfdi)
{
	char *pc;

	/* loop parsing the string */
	for (pc = *buffer; !is_eow(*pc); ) {
		char *ps, *pe;
		u32 *pv = NULL;

#ifdef DEBUG
		pr_info(DEVICE_NAME
			": %s(): parsing from char #%u '%c' (%02xh)\n",
			__func__, (uint )(pc - *buffer), *pc, *pc);
#endif

		/* search for ',' or ' ' */
		for (pe = ps = pc; *ps != ',' && !is_eow(*ps); ps++)
			switch (*ps) {
			case '=':
				pe = ps;
				break;
			}

		/* end of record found in 'ps',
		 * *pe should == '='
		 */
		if (pe == pc)
			return -EINVAL;

		*pe = '\0';
#ifdef DEBUG
		pr_info(DEVICE_NAME ": %s(): parsing keyword \"%s\"\n",
			__func__, pc);
#endif

		if (!strcmp(pc, "f_clock"))
			pv = &pfdi->clock_Hz;
		else if (!strcmp(pc, "nom_bitrate"))
			pv = &pfdi->nominal.bitrate;
		else if (!strcmp(pc, "nom_brp"))
			pv = &pfdi->nominal.brp;
		else if (!strcmp(pc, "nom_tseg1"))
			pv = &pfdi->nominal.tseg1;
		else if (!strcmp(pc, "nom_tseg2"))
			pv = &pfdi->nominal.tseg2;
		else if (!strcmp(pc, "nom_sjw"))
			pv = &pfdi->nominal.sjw;
		else if (!strcmp(pc, "data_bitrate"))
			pv = &pfdi->data.bitrate;
		else if (!strcmp(pc, "data_brp"))
			pv = &pfdi->data.brp;
		else if (!strcmp(pc, "data_tseg1"))
			pv = &pfdi->data.tseg1;
		else if (!strcmp(pc, "data_tseg2"))
			pv = &pfdi->data.tseg2;
		else if (!strcmp(pc, "data_sjw"))
			pv = &pfdi->data.sjw;

		/* restore things as we found them */
		*pe = '=';

		/* unknown keyword are ignored */
		if (pv) {
			char tmp = *ps;
			int err;

			*ps = '\0';
			err = strtounit(pe+1, pv, "kM");
			*ps = tmp;

			if (err < 0) {
				pr_info(DEVICE_NAME ": invalid char '%c' "
					"in numeric value\n", -err);
				return -EINVAL;
			}
#ifdef DEBUG
			pr_info(DEVICE_NAME ": %s(): \"%s\" = %u\n",
				__func__, pc, *pv);
		} else {
			pr_info(DEVICE_NAME
				": %s(): \"%s\" = unknown (ignored)\n",
				__func__, pc);
#endif
		}

		if (is_eow(*ps))
			break;

		pc = ps + 1;
	}

	return 0;
}

/*
 * lengthy helper for use in write..., parses a init command
 */
int pcan_parse_input_init(char *buffer, struct pcanfd_init *pfdi)
{
	char *ptr = buffer;
	u32 btr0btr1;
	int i, err = -EINVAL;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(\"%s\")\n", __func__, buffer);
#endif

	/* remove leading blanks */
	skip_blanks(&ptr);

	/* is it really a init string */
	if (scan_char(&ptr) != 'i')
		goto reject;

	/* parse init string, a CR is not allowed here */
	if (skip_blanks_and_test_for_CR(&ptr))
		goto reject;

	memset(pfdi, '\0', sizeof(*pfdi));

	/* get BTR0BTR1 */
	if (!scan_unsigned_number(&ptr, &btr0btr1)) {
		if (btr0btr1 > 0xFFFF)
			goto reject;

		/* for compatibility with old API, BTR0BTR1 is SJA1000
		 * representation of the bitrate, that is: 0x001c equals
		 * 500 kbps with 8*MHz clock.
		 */
		pfdi->clock_Hz = 8*MHz;

		pcan_btr0btr1_to_bittiming(&pfdi->nominal, btr0btr1);

	} else if (scan_bitrate_string(&ptr, pfdi))
		goto reject;

	/* optional rest, only 2 switches are possible */
	for (i = 0; i < 2; i++) {
		if (skip_blanks_and_test_for_CR(&ptr))
			break;

		switch (scan_char(&ptr)) {
		case 's':
			pfdi->flags |= PCANFD_INIT_STD_MSG_ONLY;
		case 'e':
			break;
		case 'l':
			pfdi->flags |= PCANFD_INIT_LISTEN_ONLY;
			break;
		default:
			break;
		}
	}

	if (pfdi->data.bitrate || pfdi->data.brp || pfdi->data.tseg1 ||
			pfdi->data.tseg2 || pfdi->data.sjw)
		pfdi->flags |= PCANFD_INIT_FD;

#ifdef DEBUG
	pr_info(DEVICE_NAME
		": %s(): init[flags=%08xh bitrate=%u dbitrate=%u]\n",
		__func__, pfdi->flags,
		pfdi->nominal.bitrate, pfdi->data.bitrate);
#endif
	return 0;

reject:
	return err;
}
