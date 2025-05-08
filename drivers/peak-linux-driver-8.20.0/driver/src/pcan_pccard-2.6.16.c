/* SPDX-License-Identifier: GPL-2.0 */
/*
 * system dependend parts to handle pcan-pccard
 * special code for kernels less and equal than 2.6.16
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
static dev_link_t *pccard_card_list = NULL;
static dev_info_t pccard_info = DEVICE_NAME;
#endif

#ifdef LINUX_24
static void cs_error(client_handle_t handle, int func, int ret)
{
	error_info_t err = { func, ret };
	CardServices(ReportError, handle, &err);
}
#endif

/* called at CARD_REMOVAL */
static int pccard_plugout(dev_link_t *link)
{
	PCAN_PCCARD *card = link->priv;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	if (link->open) {
		int i;

		pr_info(DEVICE_NAME
			": release postponed since device is still used!\n");

		link->state |= DEV_STALE_CONFIG;

		/* mark all associated channels as plugged out */
		for (i = 0; i < PCCARD_CHANNELS; i++)
			if (card->dev[i])
				card->dev[i]->is_plugged = 0;

		return -EBUSY;
	}
#endif

	link->state &= ~DEV_PRESENT;

	if (link->state & DEV_CONFIG) {
		pccard_release_all_devices(card);

		/* release pcmcia driver part */
		link->dev  = NULL;
		pcmcia_release_configuration(link->handle);
		pcmcia_release_io(link->handle, &link->io);
		pcmcia_release_irq(link->handle, &link->irq);

		link->state &= ~DEV_CONFIG;
	}

	return 0;
}

/* called at CARD_INSERTION or startup with already inserted card */
static int pccard_plugin(dev_link_t *link)
{
	client_handle_t handle = link->handle;
	tuple_t tuple;
	cisdata_t buf[CISTPL_END + 1];
	cisparse_t parse;
	config_info_t conf;
	cistpl_vers_1_t *cv;
	int last_ret;
	int last_fn;
	PCAN_PCCARD *card = NULL;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	/* show manufacturer string in log */
	CS_PREPARE(CISTPL_VERS_1);
	CS_CHECK(GetFirstTuple, pcmcia_get_first_tuple(handle, &tuple));
	CS_CHECK(GetTupleData, pcmcia_get_tuple_data(handle, &tuple));
	CS_CHECK(ParseTuple, pcmcia_parse_tuple(handle, &tuple, &parse));
	cv = &parse.version_1;
	pr_info(DEVICE_NAME ": %s %s %s %s\n",
		&cv->str[cv->ofs[0]], &cv->str[cv->ofs[1]],
		&cv->str[cv->ofs[2]], &cv->str[cv->ofs[3]]);

	/* specify what to request */
	CS_PREPARE(CISTPL_CONFIG);
	CS_CHECK(GetFirstTuple, pcmcia_get_first_tuple(handle, &tuple));
	CS_CHECK(GetTupleData, pcmcia_get_tuple_data(handle, &tuple));
	CS_CHECK(ParseTuple, pcmcia_parse_tuple(handle, &tuple, &parse));

	link->conf.ConfigBase = parse.config.base;
	link->conf.Present = parse.config.rmask[0];

	link->state |= DEV_CONFIG;

	CS_CHECK(GetConfigurationInfo,
			pcmcia_get_configuration_info(handle, &conf));

	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
	tuple.Attributes   = 0;
	CS_CHECK(GetFirstTuple, pcmcia_get_first_tuple(handle, &tuple));
	while (1) {
		cistpl_cftable_entry_t *cfg = &parse.cftable_entry;

		if (pcmcia_get_tuple_data(handle, &tuple) ||
				pcmcia_parse_tuple(handle, &tuple, &parse))
			goto next_entry;

		card = link->priv;

		if (cfg->io.nwin > 0) {
			cistpl_io_t *io = &cfg->io;

			link->conf.Attributes = CONF_ENABLE_IRQ;
			link->conf.IntType = INT_MEMORY_AND_IO;
			link->conf.Vcc =
				cfg->vcc.param[CISTPL_POWER_VNOM] / 10000;
			link->conf.Vpp1 = 0;
			link->conf.Vpp2 = 0;
			link->conf.ConfigIndex = cfg->index;
			link->conf.Present = PRESENT_OPTION | PRESENT_STATUS;

			link->irq.Attributes = IRQ_TYPE_EXCLUSIVE;
			link->irq.IRQInfo1 = cfg->irq.IRQInfo1;
			link->irq.IRQInfo2 = cfg->irq.IRQInfo2;

			/* we use our own handler */
			link->irq.Handler = NULL;

			link->io.BasePort1 = io->win[0].base;
			link->io.NumPorts1 = io->win[0].len;

			/* only this kind of access is yet supported */
			link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
			link->io.IOAddrLines = io->flags & CISTPL_IO_LINES_MASK;
			if (!pcmcia_request_io(link->handle, &link->io))
				break;
		}

next_entry:
		CS_CHECK(GetNextTuple, pcmcia_get_next_tuple(handle, &tuple));
	}

	CS_CHECK(RequestIRQ, pcmcia_request_irq(handle, &link->irq));
	CS_CHECK(RequestConfiguration,
			pcmcia_request_configuration(handle, &link->conf));

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": pccard found: base1=0x%04x, size=%d, irq=%d\n",
		link->io.BasePort1, link->io.NumPorts1, link->irq.AssignedIRQ);
#endif
	/* init (cardmgr) devices associated with that card
	 * (is that necessary?)
	 */
	card->node.major = pcan_drv.nMajor;
	card->node.minor = PCCARD_MINOR_BASE;
	strcpy(card->node.dev_name, DEVICE_NAME);
	link->dev = &card->node;

	/* create device descriptors associated with the card -
	 * get relevant parts to get independend from dev_link_t
	 */
	card->basePort = link->io.BasePort1;
	card->commonIrq = link->irq.AssignedIRQ;
	last_ret = pccard_create_all_devices(card);
	if (last_ret)
		goto fail;

	link->state &= ~DEV_CONFIG_PENDING;
	return 0;

cs_failed:
	cs_error(link->handle, last_fn, last_ret);

fail:
	/* remove card virtually */
	pccard_plugout(link);

	link->state &= ~DEV_CONFIG_PENDING;

	return -ENODEV;
}

/* detach entry */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
static void pccard_detach(dev_link_t *link)
{
	dev_link_t **plink;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* test if the link is part of the linked list of pccard devices */
	for (plink = &pccard_card_list; *plink; plink = &(*plink)->next)
		if (*plink == link)
			break;

	if (*plink == NULL)
		return;

	/* do not finish stale config */
	if (link->state & DEV_STALE_CONFIG)
		return;

	/* un-register */
	if (link->handle) {
		int ret;

		ret = pcmcia_deregister_client(link->handle);
		if (ret != CS_SUCCESS)
			cs_error(link->handle, DeregisterClient, ret);
		else
			link->handle = NULL;
	}

	/* unlink from list */
	*plink = link->next;
	if (link->priv) {
		pcan_free(link->priv);
		link->priv = NULL;
	}
}
#else
static void pccard_detach(struct pcmcia_device *pcmcia_dev)
{
	dev_link_t *link = dev_to_instance(pcmcia_dev);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* finish pending work */
	if (link->priv) {
		pccard_plugout(link);
		pcan_free(link->priv);
		link->priv = NULL;
	}
}
#endif

/* attach entry */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
static int pccard_event(event_t event, int priority,
			event_callback_args_t *args);

static dev_link_t *pccard_attach(void)
#else
static int pccard_probe(struct pcmcia_device *pcmcia_dev)
#endif
{
	PCAN_PCCARD *card;
	dev_link_t *link = NULL;
	int ret = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	client_reg_t client_reg;
#endif

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* allocate a device structure */
	card = (PCAN_PCCARD *)pcan_malloc(sizeof(PCAN_PCCARD), GFP_KERNEL);
	if (!card) {
		pr_err(DEVICE_NAME ": can't allocate card memory\n");
		ret = -ENOMEM;
		goto fail;
	}

	memset(card, 0, sizeof(*card));

	pcan_init_adapter(&card->adapter, "PCAN-PC Card", pccard_adapters++,
							 PCCARD_CHANNELS);
	/* init pcmcia card specific parts */
	link = &card->link;
	link->priv = card;
	link->state = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	/* insert into linked list of pccard devices - current lonly element */
	link->next = pccard_card_list;
	pccard_card_list = link;

	/* register client to card services */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
	client_reg.EventMask = CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
				CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
				CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
	client_reg.event_handler = &pccard_event;
#endif
	client_reg.dev_info = &pccard_info;
	client_reg.Version = 0x0210; /* version 2.1 */
	client_reg.event_callback_args.client_data = link;
	ret = pcmcia_register_client(&link->handle, &client_reg);
	if ((ret != CS_SUCCESS) || ((card->dev[0] == NULL)
				&& (card->dev[1] == NULL))) {
		cs_error(link->handle, RegisterClient, ret);
		pccard_detach(link);
		link = NULL;
		goto fail;
	}

fail:
	return link;
#else
	link->handle = pcmcia_dev;
	pcmcia_dev->instance = link;

	pccard_plugin(link);

fail:
	return ret;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
/* called at each card state change */
static int pccard_event(event_t event, int priority,
					event_callback_args_t *args)
{
	dev_link_t *link = (dev_link_t *)args->client_data;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	switch (event) {
	case CS_EVENT_EJECTION_REQUEST:
		break;

	case CS_EVENT_CARD_INSERTION:
		pccard_plugin(link);
		break;

	case CS_EVENT_CARD_REMOVAL:
		pccard_plugout(link);
		break;

	case CS_EVENT_PM_SUSPEND:
	case CS_EVENT_PM_RESUME:
		break;

	case CS_EVENT_RESET_PHYSICAL:
		break;

	case CS_EVENT_CARD_RESET:
		break;

	default:
		break;
	}

	return 0;
}

static void pcan_unlink_pccard(void)
{
	dev_link_t *link;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* remove rest of linked (stale) devices  */
	for (link = pccard_card_list; link; ) {
		dev_link_t *tmp_link = link;

		link = link->next;

		pccard_plugout(tmp_link);
		pccard_adapters--;

		if (tmp_link->handle)
			pcmcia_deregister_client(tmp_link->handle);

		/* free and remove from list */
		if (tmp_link->priv) {
			pcan_free(tmp_link->priv);
			tmp_link->priv = NULL;
		}
	}
}

#else

static int pccard_suspend(struct pcmcia_device *pcmcia_dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	return 0;
}

static int pccard_resume(struct pcmcia_device *pcmcia_dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	return 0;
}
#endif
