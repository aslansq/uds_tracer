/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_filter.c - all about CAN message filtering
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
 */
/* #define DEBUG */

#include "src/pcan_common.h"
#include "src/pcan_filter.h"

#include <linux/types.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/slab.h>


#define PCAN_MAX_FILTER_PER_CHAIN	8

struct filter_element {
	struct list_head list;
	u32 FromID;		/* all msgs lower than FromID are rejected */
	u32 ToID;		/* all msgs higher than ToID are rejected */
	u32 flags;		/* STANDARD flags excludes EXTENDED flag while
				   RTR flags excludes all-non RTR msgs */
};

struct filter_chain {
	struct list_head anchor;
	int count;		/* counts the number of filters in this chain */
	pcan_lock_t lock;	/* mutex lock for this filter chain */
};

/* create the base for a list of filters - returns a handle */
void *pcan_create_filter_chain(void)
{
	struct filter_chain *chain;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	/* alloc a new filter_element */
	chain = (struct filter_chain *)pcan_malloc(sizeof(struct filter_chain),
						GFP_KERNEL);
	if (!chain) {
		pr_err(DEVICE_NAME
			": not enough memory to create filter chain\n");
	} else {
		INIT_LIST_HEAD(&chain->anchor);

		/* initial no blocking of messages to provide compatibilty */
		chain->count = -1;
		pcan_lock_init(&chain->lock);
	}

	return (void *)chain;
}

static int __pcan_add_filter(struct filter_chain *chain,
				u32 FromID, u32 ToID, u32 _flags)
{
	struct list_head *ptr, *tmp;
	struct filter_element *pfilter;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	/* test for doubly set entries */
	list_for_each_safe(ptr, tmp, &chain->anchor) {
		pfilter = list_entry(ptr, struct filter_element, list);
		if ((pfilter->FromID == FromID) &&
		    (pfilter->ToID == ToID) &&
		    (pfilter->flags == _flags))
			return 0;
	}

	/* limit count of filters since filters are user allocated */
	if (chain->count >= PCAN_MAX_FILTER_PER_CHAIN)
		return -ENOMEM;

	/* alloc a new filter_element */
	pfilter = (struct filter_element *)pcan_malloc(sizeof(*pfilter),
							GFP_KERNEL);
	if (!pfilter) {
		pr_err(DEVICE_NAME
			": not enough memory to create filter element!\n");
		return -ENOMEM;
	}

	/* init filter element */
	pfilter->FromID = FromID;
	pfilter->ToID  = ToID;
	pfilter->flags = _flags;

	/* get first start for compatibility mode */
	if (chain->count < 0)
		chain->count = 1;
	else
		chain->count++;

	list_add_tail(&pfilter->list, &chain->anchor);

	return 0;
}

/* add a filter element to the filter chain pointed by handle
 * return 0 if it is OK, else return error
 */
int pcan_add_filter(void *handle, u32 FromID, u32 ToID, u32 _flags)
{
	struct filter_chain *chain = (struct filter_chain *)handle;
	pcan_lock_irqsave_ctxt lck_ctx;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(0x%p, 0x%08x, 0x%08x, 0x%08x)\n",
		__func__, handle, FromID, ToID, _flags);
#endif

	/* if chain isn't set ignore it */
	if (!chain)
		return 0;

	/* add this entry to chain */
	pcan_lock_get_irqsave(&chain->lock, lck_ctx);

	err = __pcan_add_filter(chain, FromID, ToID, _flags);

	pcan_lock_put_irqrestore(&chain->lock, lck_ctx);

	return err;
}

/* delete all filter elements in the filter chain pointed by handle */
void pcan_delete_filter_all(void *handle)
{
	struct filter_chain *chain = (struct filter_chain *)handle;
	struct filter_element *pfilter;
	struct list_head *ptr, *tmp;
	pcan_lock_irqsave_ctxt lck_ctx;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(0x%p)\n", __func__, handle);
#endif
	if (!chain)
		return;

	pcan_lock_get_irqsave(&chain->lock, lck_ctx);
	list_for_each_safe(ptr, tmp, &chain->anchor) {
		pfilter = list_entry(ptr, struct filter_element, list);
		list_del(ptr);
		pcan_free(pfilter);
	}

	chain->count = 0;

	pcan_lock_put_irqrestore(&chain->lock, lck_ctx);
}

#define RTR_FILTER	(pfilter->flags & PCANFD_MSG_RTR)
#define EXT_FILTER	(pfilter->flags & PCANFD_MSG_EXT)
#define RTR_IN		(rtr_message)
#define EXT_IN		(ext_message)

/* do the filtering with all filter elements pointed by handle
 * returns 0 when the message should be passed
 */
int pcan_do_filter(void *handle, struct pcanfd_rxmsg *pe)
{
	struct filter_chain *chain = (struct filter_chain *)handle;
	struct filter_element *pfilter;
	struct list_head *ptr, *tmp;
	u32 rtr_message, ext_message;
	pcan_lock_irqsave_ctxt lck_ctx;
	int throw;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(0x%p, 0x%08x)\n",
		__func__, handle, pe->msg.type);
#endif
	if (!chain)
		return 0;

	switch (pe->msg.type) {

	case PCANFD_TYPE_CAN20_MSG:
	case PCANFD_TYPE_CANFD_MSG:
		break;

	case PCANFD_TYPE_STATUS:
		/* status (i.e. non CAN[-FD] message) is always passed
		 * TODO: is this conform to MS-Windows driver behaviour?
		 */
		return 0;

	case PCANFD_TYPE_NOP:
	default:
		/* invalid message are not passed! */
		return 1;
	}

	pcan_lock_get_irqsave(&chain->lock, lck_ctx);

	/* pass always when no filter reset has been done before */
	if (chain->count <= 0)
		goto lbl_accept;

	RTR_IN = pe->msg.flags & PCANFD_MSG_RTR;
	EXT_IN = pe->msg.flags & PCANFD_MSG_EXT;

	list_for_each_safe(ptr, tmp, &chain->anchor) {
		pfilter = list_entry(ptr, struct filter_element, list);

		/* truth table for throw
		 *
		 *             RTR_FILTER | /RTR_FILTER
		 *            ------------|------------
		 *  EXT_FILTER|  1  |  0  |  0  |  0  |/EXT_IN
		 *            |-----------|--------------
		 *            |  1  |  0  |  0  |  0  |
		 *         ---------------------------| EXT_IN
		 *            |  1  |  1  |  1  |  1  |
		 *            |-----------|--------------
		 * /EXT_FILTER|  1  |  0  |  0  |  0  |/EXT_IN
		 *            |-----|-----------|------
		 *           /RTR_IN|  RTR_IN   |/RTR_IN
		 */
		throw = ((RTR_FILTER && !RTR_IN) ||
					(!EXT_FILTER && EXT_IN));

		if ((!throw) && (pe->msg.id >= pfilter->FromID) &&
				(pe->msg.id <= pfilter->ToID))
			goto lbl_accept;
	}

	pcan_lock_put_irqrestore(&chain->lock, lck_ctx);

	/* no pass criteria was found */
	return 1;

lbl_accept:
	pcan_lock_put_irqrestore(&chain->lock, lck_ctx);

	return 0;
}

/* remove the whole filter chain (and potential filter elements) pointed by
 * handle
 */
void *pcan_delete_filter_chain(void *handle)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(0x%p)\n", __func__, handle);
#endif
	if (handle) {
		pcan_delete_filter_all(handle);
		pcan_free(handle);
	}

	return NULL;
}

/* return the number of recorded filters */
int pcan_get_filters_count(void *handle)
{
	struct filter_chain *chain = (struct filter_chain *)handle;
	return (chain->count < 0) ? 0 : chain->count;
}

int pcan_add_filters(void *handle, struct pcanfd_msg_filter *pf, int count)
{
	struct filter_chain *chain = (struct filter_chain *)handle;
	pcan_lock_irqsave_ctxt lck_ctx;
	int i, err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(0x%p, count=%d)\n", __func__, handle, count);
#endif
	if (!chain)
		return 0;

	pcan_lock_get_irqsave(&chain->lock, lck_ctx);

	for (i = err = 0; i < count; i++, pf++) {
		int err = __pcan_add_filter(chain,
					pf->id_from, pf->id_to, pf->msg_flags);
		if (err)
			break;
	}

	pcan_lock_put_irqrestore(&chain->lock, lck_ctx);

	return err;
}

int pcan_get_filters(void *handle, struct pcanfd_msg_filter *pf, int count)
{
	struct filter_chain *chain = (struct filter_chain *)handle;
	struct filter_element *pfilter;
	struct list_head *ptr, *tmp;
	pcan_lock_irqsave_ctxt lck_ctx;
	int i = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(0x%p, count=%d)\n", __func__, handle, count);
#endif
	if (!chain)
		return 0;

	pcan_lock_get_irqsave(&chain->lock, lck_ctx);

	list_for_each_safe(ptr, tmp, &chain->anchor) {
		pfilter = list_entry(ptr, struct filter_element, list);

		if (pf) {
			pf->id_from = pfilter->FromID;
			pf->id_to = pfilter->ToID;
			pf->msg_flags = pfilter->flags;
			pf++;
		}

		if (++i >= count)
			break;
	}

	pcan_lock_put_irqrestore(&chain->lock, lck_ctx);

	return i;
}
