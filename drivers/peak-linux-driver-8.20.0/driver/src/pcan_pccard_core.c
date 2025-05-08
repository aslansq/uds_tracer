/* SPDX-License-Identifier: GPL-2.0 */
/*
 * all parts to handle the interface specific parts of pcan-pccard
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
/* #define DEBUG */

#include "src/pcan_common.h"	/* must always be the 1st include */

/* SGr on 19-07-2011: PCCARD_SUPPORT may be undefined in pcan_common.h if
 * kernel support not configured, even if PCCARD_SUPPORT is defined on command
 * line.
 */
#ifdef PCCARD_SUPPORT

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/delay.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
#include <asm/io.h>
#else
#include <linux/io.h>
#endif
#include <linux/types.h>
#include <linux/timer.h>	/* to support activity scan */
#include <linux/slab.h>

#include <src/pcan_main.h>
#include <src/pcan_pccard.h>
#include <src/pcan_sja1000.h>
#include <src/pcan_fifo.h>

#ifdef NETDEV_SUPPORT
#include <src/pcan_netdev.h>	/* for hotplug pcan_netdev_register() */
#endif

#include <src/pcan_pccard_core.h>
#include <src/pcan_filter.h>

#define PCCARD_PORT_SIZE   0x20         /* range of a channels port */
#define PCCARD_COMMON_SIZE 0x40         /* range of channels common ports */
#define PCCARD_CHANNEL_OFF 0x20         /* port offset of sja1000 channel #2 */
#define PCCARD_COMMON      0x40         /* port offset of common area */

/* offsets from base of common area */
#define CCR                0x00         /* CAN control register */
#define CSR                0x02         /* CAN status register */
#define CPR                0x04         /* CAN power register */
#define SPIDATI            0x06         /* SPI data in */
#define SPIDATO            0x08         /* SPI data out */
#define SPIADR             0x0A         /* SPI address register */
#define SPIINS             0x0C         /* SPI instruction register */
#define FW_MAJOR           0x10         /* firmware major number (local) */
#define FW_MINOR           0x12         /* firmware minor number (local) */

#define CCR_CLK_MASK       0x03         /* mask for clock code */
#define CCR_CLK16          0x00         /* clk code */
#define CCR_CLK10          0x01
#define CCR_CLK21          0x02
#define CCR_CLK8           0x03
#define CCR_CLK_DEFAULT    CCR_CLK16    /* set default clock */

#define CCR_RESET_MASK     0x01
#define CCR_RESET          0x01         /* put channel into reset */
#define CCR_RESET0_SHIFT      2         /* shift for channel 0 */
#define CCR_RESET1_SHIFT      3         /* shift for channel 1 */

#define CCR_LED_MASK       0x03         /* mask for LED channel 0 code */
#define CCR_LED_ON         0x00         /* switch LED permanently on */
#define CCR_LED_FAST       0x01         /* switch LED to 4 Hz flash */
#define CCR_LED_SLOW       0x02         /* switch LED to 1 Hz flash */
#define CCR_LED_OFF        0x03         /* switch LED off */
#define CCR_LED0_SHIFT        4         /* shift for channel 1 */
#define CCR_LED1_SHIFT        6         /* shift for channel 1 */

#define CCR_DEFAULT		((((u8)CCR_LED_OFF) << CCR_LED0_SHIFT) | \
				(((u8)CCR_LED_OFF) << CCR_LED1_SHIFT) |  \
								CCR_CLK_DEFAULT)

#define CSR_SPIBUSY		0x04	/* set when SPI transaction is busy */

#define SPI_MAX_WAIT_CYCLES	100
#define MAX_LOOP_CYCLES		1000	/* cycles to wait if card is plugged out or damaged */

static u16 pccard_devices = 0;

/* it seems that even when called after 'pccardctl eject' the card is seen as
 * already plugged out. So this function allways return 0.
 * BTW this does not matter since PCCARDs power is switched off during
 * plug out or 'pccardctl eject'. Therefore no de-initialisation of hardware
 * components are necessary.
 */
static inline int pccard_plugged(PCAN_PCCARD *card)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	return (pcmcia_dev_present(card->pcc_dev) != NULL);
#elif LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
	return DEV_OK(card->link.handle);
#else
	return DEV_OK((&card->link));
#endif
}

/* read channel individual register */
static u8 pccard_readreg(struct pcandev *dev, u8 port)
{
	return inb(dev->dwPort + port);
}

/* write channel individual register */
static void pccard_writereg(struct pcandev *dev, u8 port, u8 data)
{
	outb(data, dev->dwPort + port) ;
}

/* read common register to both channels of a card */
static inline u8 pccard_readreg_common(PCAN_PCCARD *card, u8 port)
{
	return inb(card->commonPort + port);
}

/* write common register to both channels of a card */
static inline void pccard_writereg_common(PCAN_PCCARD *card, u8 port, u8 data)
{
	outb(data, card->commonPort + port);
	if (port == CCR)
		card->cached_ccr = data;
}

/* SPI engine */
static int wait_while_spi_engine_busy(PCAN_PCCARD *card)
{
	int counter = SPI_MAX_WAIT_CYCLES;
	do
		schedule();
	while ((pccard_readreg_common(card, CSR) & CSR_SPIBUSY) && counter--);

	if (counter <= 0)
		return -EIO;

	return 0;
}

static int pccard_write_eeprom(PCAN_PCCARD *card, u16 addr, u8 val)
{
	u16 status;
	int err;
	int counter;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(addr=0x%04x, val=0x%02x)\n",
		__func__, addr, val);
#endif
	/* Instruction WRITE */
	/* WREN */
	pccard_writereg_common(card, SPIINS, 0x06);
	if ((err = wait_while_spi_engine_busy(card)) < 0)
		goto fail;

	/* warte, bis WEL gesetzt ist */
	counter = MAX_LOOP_CYCLES;
	do {
		/* RDSR == Read status */
		pccard_writereg_common(card, SPIINS, 0x05);
		if ((err = wait_while_spi_engine_busy(card)) < 0)
			goto fail;

		status = pccard_readreg_common(card, SPIDATI);

	/* warte, bis WEL (write enable) 1. */
	} while ((!(status & 0x02)) && counter--);

	if (counter <= 0) {
		err = -EIO;
		goto fail;
	}

	/* Adresse und Data setzen */
	pccard_writereg_common(card, SPIADR, addr & 0xff);
	pccard_writereg_common(card, SPIDATO, val);

	/* Instruction WRITE */
	/* WRITE mit bit3 = Addr8 */
	pccard_writereg_common(card, SPIINS, ((addr & 0x100) ? 8 : 0) | 0x02);

	if ((err = wait_while_spi_engine_busy(card)) < 0)
		goto fail;

	/* warte, bis Schreiben abgeschlossen */
	counter = MAX_LOOP_CYCLES;
	do {
		/* RDSR == Read status */
		pccard_writereg_common(card, SPIINS, 0x05);
		if ((err = wait_while_spi_engine_busy(card)) < 0)
			goto fail;

		status = pccard_readreg_common(card, SPIDATI);

	/* wait until WIP (Write In Progress) 0 */
	} while ((status & 0x01) && counter--);

	if (counter <= 0)
		err = -EIO;

fail:
	if (err)
		pr_err(DEVICE_NAME ": %s(0x%04x, 0x%02x) failed (err %d)\n",
			__func__, addr, val, err);

	return err;
}

/* helper functions */

/* get firmware number of PCC */
static inline void pccard_show_firmware_version(PCAN_PCCARD *card)
{
	u8 fw_major = pccard_readreg_common(card, FW_MAJOR);
	u8 fw_minor = pccard_readreg_common(card, FW_MINOR);

	pr_info(DEVICE_NAME ": pccard firmware %d.%d\n",
		fw_major, fw_minor);
}

/* init CCR settings */
static inline void pccard_initreg_common(PCAN_PCCARD *card)
{
	/* initializing CCR cached value with the opposite of the value we
	 * want to write implies that the CCR register WILL be really written
	 * with CCR_DEFAULT
	 */
	card->cached_ccr = (u8 )~CCR_DEFAULT;
	pccard_writereg_common(card, CCR, CCR_DEFAULT);
}

/* hard reset only one channel to its default settings */
static inline void pccard_channel_reset(PCAN_PCCARD *card, int can_idx)
{
	u8 data;
	u8 shift = (can_idx) ? CCR_RESET1_SHIFT : CCR_RESET0_SHIFT;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(can_idx=%d)\n", __func__, can_idx);
#endif

	data = pccard_readreg_common(card, CCR);
	data &= ~(CCR_RESET_MASK << shift);
	data |=  (CCR_RESET      << shift);
	pccard_writereg_common(card, CCR, data);
	mdelay(2);

	data = pccard_readreg_common(card, CCR);
	data &= ~(CCR_RESET_MASK << shift);
	data &= ~(CCR_RESET      << shift);
	pccard_writereg_common(card, CCR, data);

	mdelay(10); /* wait until reset has settled */
}

/* set LED */
static inline void pccard_set_led(PCAN_PCCARD *card, int can_idx, u8 mode)
{
	u8 data;
	u8 shift = (can_idx) ? CCR_LED1_SHIFT : CCR_LED0_SHIFT;

	/* to avoid slow and busy read, use a checd value of the CCR */
	data = card->cached_ccr;

	mode &= CCR_LED_MASK;

	/* write only if something has changed */
	if (((data >> shift) & CCR_LED_MASK) != mode) {
		data &= ~(CCR_LED_MASK << shift);
		data |= (mode << shift);
		pccard_writereg_common(card, CCR, data);
	}
}

/* enable CAN power */
static inline void pccard_enable_CAN_power(PCAN_PCCARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	pccard_write_eeprom(card, 0, 1);
}

/* disable CAN power */
static inline void pccard_disable_CAN_power(PCAN_PCCARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	pccard_write_eeprom(card, 0, 0);
}

/* activity scanner to control LEDs */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
static void pccard_activity_scanner(unsigned long ptr)
{
	PCAN_PCCARD *card = (PCAN_PCCARD *)ptr;
#else
static void pccard_activity_scanner(struct timer_list *t)
{
	PCAN_PCCARD *card = from_timer(card, t, activity_timer);
#endif
	struct pcandev *dev;
	int i;

	for (i = 0; i < PCCARD_CHANNELS; i++) {
		dev = card->dev[i];
#ifdef DEBUG
		pr_info(DEVICE_NAME ": %s(%p): dev[%u] activity=%u\n",
			__func__, card, i,
			(dev) ? dev->ucActivityState : 255);
#endif

		if (dev) {
			u8 state = dev->ucActivityState;

			switch(state) {
			case ACTIVITY_XMIT:
				  dev->ucActivityState = ACTIVITY_IDLE;
				  pccard_set_led(card, i, CCR_LED_FAST);
				  break;
			case ACTIVITY_IDLE:
				  pccard_set_led(card, i, CCR_LED_SLOW);
				  break;
			case ACTIVITY_INITIALIZED:
				  pccard_set_led(card, i, CCR_LED_ON);
				  break;
			default:
				  pccard_set_led(card, i, CCR_LED_OFF);
				  break;
			}
		} else {
			pccard_set_led(card, i, CCR_LED_OFF);
		}
	}

	/* restart timer */
	if (card->run_activity_timer_cyclic) {
		card->activity_timer.expires  = jiffies + HZ;
		add_timer(&card->activity_timer);
	}
}

static void pccard_start_activity_scanner(PCAN_PCCARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, card);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
	init_timer(&card->activity_timer);
	card->activity_timer.function = pccard_activity_scanner;
	card->activity_timer.data = (unsigned long)card;
#else
	timer_setup(&card->activity_timer, pccard_activity_scanner, 0);
#endif
	card->activity_timer.expires = jiffies + HZ; /* one second */

	card->run_activity_timer_cyclic = 1;
	add_timer(&card->activity_timer);
}

static void pccard_stop_activity_scanner(PCAN_PCCARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%p)\n", __func__, card);
#endif
	card->run_activity_timer_cyclic = 0;
	del_timer_sync(&card->activity_timer);
}

/* all about interrupt handling */
#ifndef NO_RT
static int pcan_pccard_irqhandler(rtdm_irq_t *irq_context)
{
#error PCAN-PCCARD devices are not supported in RT environments
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
/* make a special irqhandler since PCCARD irqs are ISA like */
static irqreturn_t pcan_pccard_irqhandler(int irq, void *dev_id,
							struct pt_regs *regs)
#else
static irqreturn_t pcan_pccard_irqhandler(int irq, void *dev_id)
#endif
{
	struct pcandev *dev = (struct pcandev *)dev_id;
	PCAN_PCCARD *card = dev->port.pccard.card;
	int ret = 0;
	u16 stop_count = 100;
	u16 loop_count;
	u16 index;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(dev_id=%p)\n", __func__, dev_id);
#endif
	for (index = loop_count = 0; loop_count < PCCARD_CHANNELS; index++) {
		/* consider single channel cards, too */
		if ((dev = card->dev[index % PCCARD_CHANNELS])) {
			int tmpret = pcan_sja1000_irqhandler(dev);
			if (!tmpret)
				loop_count++;
			else {
				ret = 1;

				/* restart, since all channels must respond in
				 * one pass with no interrupt pending
				 */
				loop_count = 0;
			}

			if (!stop_count--) {
				pr_warn(DEVICE_NAME
					": Too much PCCARD interrupt "
					"load, processing halted!\n");
				break;
			}
		} else {
			loop_count++;
		}

		if (loop_count == PCCARD_CHANNELS)
			break;
	}

	return PCAN_IRQ_RETVAL(ret);
}

static int pccard_req_irq(struct pcandev *dev, struct pcanusr *dev_priv)
{
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	err = request_irq(dev->wIrq, pcan_pccard_irqhandler,
				IRQF_SHARED, DEVICE_NAME, dev);
	if (err)
		return err;

	dev->wInitStep = 5;

	return 0;
}

static void pccard_free_irq(struct pcandev *dev, struct pcanusr *dev_priv)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	free_irq(dev->wIrq, dev);
	dev->wInitStep = 4;
}

/* free allocated resources */
static int pccard_cleanup(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	switch (dev->wInitStep) {
	case 5:
		pccard_free_irq(dev, NULL);

		/* fall through */
		fallthrough;
	case 4:
		dev->ucActivityState = ACTIVITY_NONE; /* temporary */
		pcan_sysfs_dev_node_destroy(dev);
#ifdef NETDEV_SUPPORT
		pcan_netdev_unregister(dev);
#endif

		/* fall through */
		fallthrough;
	case 3:
		if (pccard_devices)
			pccard_devices--;

		/* fall through */
		fallthrough;
	case 2:
		if (pcan_remove_dev_from_list(dev))
			pcan_destroy_dev(dev);

		/* fall through */
		fallthrough;
	case 1:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)
		release_region(dev->dwPort, PCCARD_PORT_SIZE);
#endif

		/* fall through */
		fallthrough;
	case 0:
		dev->filter = pcan_delete_filter_chain(dev->filter);
	}

	return 0;
}

/* interface depended open and close */
static int pccard_open(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	dev->ucActivityState = ACTIVITY_IDLE;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	dev->port.pccard.card->pcc_dev->open++;
#else
	dev->port.pccard.card->link.open++;
#endif
	return 0;
}

static int pccard_release(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif
	dev->ucActivityState = ACTIVITY_INITIALIZED;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	if (dev->port.pccard.card->pcc_dev->open > 0)
		dev->port.pccard.card->pcc_dev->open--;
#else
	if (dev->port.pccard.card->link.open > 0)
		dev->port.pccard.card->link.open--;
#endif
	return 0;
}

/* create and init a single pccard pcan device structure */
static struct pcandev *pccard_create_single_device(PCAN_PCCARD *card,
					int can_idx, u32 dwPort, u16 wIrq)
{
	struct pcandev *dev;
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%d, 0x%04x, %d)\n",
		__func__, can_idx, dwPort, wIrq);
#endif

	/* allocate memory for my device */
	dev = pcan_alloc_dev("pccard", HW_PCCARD, can_idx);
	if (!dev) {
		err = -ENOMEM;
		goto fail;
	}

	/* init structure elements to defaults */
	pcan_soft_init(dev);

	dev->adapter = &card->adapter;

	/* device handling standards for sja1000 driven parts */
	dev->device_open      = sja1000_open;
	dev->device_release   = sja1000_release;
	dev->device_write     = sja1000_write;

	/* set this before any instructions, fill struct pcandev, part 1 */
	dev->readreg     = pccard_readreg;
	dev->writereg    = pccard_writereg;
	dev->cleanup     = pccard_cleanup;
	dev->req_irq     = pccard_req_irq;
	dev->free_irq    = pccard_free_irq;
	dev->open        = pccard_open;
	dev->release     = pccard_release;
	dev->nMajor      = pcan_drv.nMajor;
	dev->nMinor      = PCCARD_MINOR_BASE + pccard_devices;
	dev->filter      = pcan_create_filter_chain();

	dev->props.ucExternalClock = 1;
	dev->props.ucMasterDevice = (can_idx) ? CHANNEL_SLAVE : CHANNEL_MASTER;

	/* reject illegal combination */
	if (!dwPort || !wIrq) {
		err = -EINVAL;
		goto fail_cleanup;
	}

	/* fill struct pcandev, hardware specfic parts */
	dev->dwPort = dwPort;
	dev->wIrq = wIrq;
	dev->port.pccard.card = card;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)
	/* allocating io resources */
	err = ___request_region(dev->dwPort, PCCARD_PORT_SIZE, DEVICE_NAME);
	if (err)
		goto fail_cleanup;
#endif
	dev->wInitStep = 1;

	/* base init of hardware - reset channel */
	pccard_channel_reset(card, can_idx);

	/* probe hardware */
	err = sja1000_probe(dev);
	if (err)
		goto fail_cleanup;

	/* add this device to the list */
	pcan_add_dev_in_list(dev);

	dev->wInitStep = 2;

	pccard_devices++;
	dev->wInitStep = 3;

	dev->ucActivityState = ACTIVITY_INITIALIZED;
	dev->wInitStep = 4;

	pr_info(DEVICE_NAME ": - pccard device minor %d found\n", dev->nMinor);

	return dev;

fail_cleanup:
	pcan_cleanup_dev(dev);
	pcan_free_dev(dev);

fail:
	pr_err(DEVICE_NAME ": %s(CAN%d) failed (err %d)\n",
		__func__, can_idx+1, err);

	return NULL;
}

/* allocate resources and common io area and initialize hardware of devices */
int pccard_create_all_devices(PCAN_PCCARD *card)
{
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	if (card) {
		int chn = 0;
		struct pcandev *dev;
		int i;

		/* fill card structure */
		card->commonPort = card->basePort + PCCARD_COMMON;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)
		/* request commonly used io area */
		err = ___request_region(card->commonPort, PCCARD_COMMON_SIZE,
					DEVICE_NAME);
		if (err)
			goto fail;
#endif

		/* init common registers */
		pccard_initreg_common(card);

		for (i = 0; i < PCCARD_CHANNELS; i++) {
			card->dev[i] = NULL;

			/* probe devices and if successfully create device
			 * structues - channel 0
			 */
			dev = pccard_create_single_device(card, i,
					card->basePort + i * PCCARD_CHANNEL_OFF,
					card->commonIrq);
			if (dev) {
				chn++;
				card->dev[i] = dev;
#ifdef NETDEV_SUPPORT
				pcan_netdev_register(dev);
#endif
				pcan_sysfs_dev_node_create(dev);
			}
		}

		/* no usable channel found */
		if (!chn) {
			pr_err(DEVICE_NAME
				": pccard with no channel found!\n");
			err = -ENODEV;
			goto fail;
		}

		/* get version of hardware to log */
		pccard_show_firmware_version(card);

		/* enable power to connector */
		pccard_enable_CAN_power(card);

		/* start scanning card's acitvity to control LEDs */
		pccard_start_activity_scanner(card);

		return 0;
	}

fail:
	if (err)
		pr_err(DEVICE_NAME ": %s() failed (err %d)\n", __func__, err);

	return err;
}

/* deinit devices and release allocted resources of devices and common io area
 *
 * Note that the pccardd(8) daemon may get the "pcan" module, as soon as the
 * PCAN-PCCARD card is inserted into the slot. Under these circumstances, the
 * system won't remove the "pcan" module since it is in use.
 * The "pcan" module will be removed *ONLY* if the card is removed from the
 * PCMCIA slot before!
 */
void pccard_release_all_devices(PCAN_PCCARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(card=%p)\n", __func__, card);
#endif
	if (card) {
		struct pcandev *dev;
		int i;

		/* stop scanning card's acitvity to control LEDs */
		pccard_stop_activity_scanner(card);

		if (pccard_plugged(card))
			 pccard_disable_CAN_power(card);

		for (i = 0; i < PCCARD_CHANNELS; i++) {
			/* 1. release associated devices */
			dev = card->dev[i];
			pcan_cleanup_dev(dev);
			card->dev[i] = NULL;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)
		release_region(card->commonPort, PCCARD_COMMON_SIZE);
#endif
	}
}
#endif /* PCCARD_SUPPORT */
