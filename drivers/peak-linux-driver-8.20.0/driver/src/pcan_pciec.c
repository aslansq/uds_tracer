/* SPDX-License-Identifier: GPL-2.0 */
/*
 * all parts to handle the interface specific parts of PCAN-PCIExpressCard
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
#include <src/pcan_common.h>

#ifdef PCIEC_SUPPORT
#include <src/pcan_pciec.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/io.h>

#define PITA_GPOUTENABLE_OFFSET 0x1a
#define PITA_GPIN_OFFSET        0x19
#define PITA_GPOUT_OFFSET       0x18

#define PCA9553_1_SLAVEADDR  (0xC4 >> 1)
#define PCA9553_LED0_OFFSET  0
#define PCA9553_LED1_OFFSET  2
#define PCA9553_DCEN_OFFSET  4
#define PCA9553_VCCEN_OFFSET 6

#define PCA9553_LOW          0
#define PCA9553_TRISTATE     1
#define PCA9553_SLOW_BLINK   2
#define PCA9553_FAST_BLINK   3

#define PCA9553_SET_MASK        0x03 /* A mask to set, shifted with .._OFFSET */
#define PCA9553_LS0_INITIALIZED 0x00 /* initial value */
#define PCA9553_LS0_NONE        0x45 /* off value */

static inline void pita_set_scl_highz(PCAN_PCIEC_CARD *card)
{
	writeb(readb(card->gpoutenable) & ~0x01, card->gpoutenable);
}

static void pita_setscl(void *data, int state)
{
	PCAN_PCIEC_CARD *card = (PCAN_PCIEC_CARD *)data;

	/* set output scl always to 0 */
	writeb(readb(card->gpout) & ~0x01, card->gpout);

	/* control output scl with gpoutenable */
	if (state)
		writeb(readb(card->gpoutenable) & ~0x01, card->gpoutenable);
	else
		writeb(readb(card->gpoutenable) |  0x01, card->gpoutenable);
}

static int pita_getscl(void *data)
{
	PCAN_PCIEC_CARD *card = (PCAN_PCIEC_CARD *)data;

	/* set tristate */
	pita_set_scl_highz(card);

	return readb(card->gpin) & 0x01;
}

static inline void pita_set_sda_highz(PCAN_PCIEC_CARD *card)
{
	writeb(readb(card->gpoutenable) & ~0x04, card->gpoutenable);
}

static void pita_setsda(void *data, int state)
{
	PCAN_PCIEC_CARD *card = (PCAN_PCIEC_CARD *)data;

	/* set output sda always to 0 */
	writeb(readb(card->gpout) & ~0x04, card->gpout);

	/* control output sda with gpoutenable */
	if (state)
		writeb(readb(card->gpoutenable) & ~0x04, card->gpoutenable);
	else
		writeb(readb(card->gpoutenable) |  0x04, card->gpoutenable);
}

static int pita_getsda(void *data)
{
	PCAN_PCIEC_CARD *card = (PCAN_PCIEC_CARD *)data;

	/* set tristate */
	pita_set_sda_highz(card);

	return (readb(card->gpin) & 0x04) ? 1 : 0;
}

static int writeBytePCA9553(PCAN_PCIEC_CARD *card, u8 offset, u8 data)
{
	u8 buffer[2] = {offset, data};
	struct i2c_msg msg[] =  {{ PCA9553_1_SLAVEADDR, 0, 2, buffer }};
	int ret;

	ret = i2c_transfer(&card->adapter, msg, 1);

	return (ret >= 0) ? 0 : ret;
}

static int initPCA9553(PCAN_PCIEC_CARD *card)
{
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* prescaler for frequency 0: "SLOW" = 1 Hz = "44" */
	err = writeBytePCA9553(card, 1, 44 / 1);
	if (err)
		goto fail;

	/* duty cycle 0: 50% */
	err = writeBytePCA9553(card, 2, 0x80);
	if (err)
		goto fail;

	/* prescaler for frequency 1: "FAST" = 5 Hz */
	err = writeBytePCA9553(card, 3, 44 / 5);
	if (err)
		goto fail;

	/* duty cycle 1: 50% */
	err = writeBytePCA9553(card, 4, 0x80);
	if (err)
		goto fail;

	card->PCA9553_LS0Shadow = PCA9553_LS0_INITIALIZED;

	/* switch LEDs and ... to initial state */
	err = writeBytePCA9553(card, 5, card->PCA9553_LS0Shadow);
	if (err)
		goto fail;

fail:
	return err;
}

static void deinitPCA9553(PCAN_PCIEC_CARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	card->PCA9553_LS0Shadow = PCA9553_LS0_NONE;

	/* switch LEDs and ... to off */
	writeBytePCA9553(card, 5, card->PCA9553_LS0Shadow);
}

static void initPITAGPIO(PCAN_PCIEC_CARD *card)
{
	/* initialize GPIOs to high-Z */
	pita_set_scl_highz(card);
	pita_set_sda_highz(card);
}

/**
 * Initialize the I2C subsystem for one or more CAN channels.
 * Only call for blinking PCIEC masters.
 * Please note that this function skips hardware initialization for other
 * channels if there is a master channel
 * for multiple CAN channels sharing I2C hardware.
 * @param card The associated card structure
 * @return The error code
 */
static int pcan_initI2C(struct pci_dev *pciDev, PCAN_PCIEC_CARD *card,
			void __iomem *bar0_cfg_addr)
{
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* set special card addresses */
	card->gpoutenable = bar0_cfg_addr + PITA_GPOUTENABLE_OFFSET;
	card->gpin        = bar0_cfg_addr + PITA_GPIN_OFFSET;
	card->gpout       = bar0_cfg_addr + PITA_GPOUT_OFFSET;

	/* prepare algo_bit structure since we have a bit banging interface */
	memset(&card->algo_data, 0, sizeof(card->algo_data));
	card->algo_data.data     = card;
	card->algo_data.setscl   = pita_setscl;
	card->algo_data.getscl   = pita_getscl;
	card->algo_data.setsda   = pita_setsda;
	card->algo_data.getsda   = pita_getsda;
	card->algo_data.udelay   = 10; // usec
	card->algo_data.timeout  = HZ;

	/*
	 * create the card's special i2c_adapter structure with reference to
	 * the bit banging interface
	 */
	memset(&card->adapter, 0, sizeof(card->adapter));
	card->adapter.algo_data  = &card->algo_data;
	card->adapter.algo       = NULL;
	card->adapter.owner      = THIS_MODULE;
	strncpy(card->adapter.name, "pcan_i2c", sizeof(card->adapter.name));
	card->adapter.class      = I2C_CLASS_HWMON;
	card->adapter.retries    = 1;
	card->adapter.timeout    = 2 * HZ;
	card->adapter.dev.parent = &pciDev->dev;

	initPITAGPIO(card);

	err = i2c_bit_add_bus(&card->adapter);
	if (err) {
		pr_err(DEVICE_NAME ": can't register i2c channel (err %d).\n",
			err);
		return err;
	}

	return initPCA9553(card);
}

/**
 * Set VCC to external circuit on or off. Please note, setting is done in
 * background.
 * @param card The associated PCAN-ExpressCard
 * @param On If zero, then it is switched off, else on.
 * @return 0 if the task is put successfully into background.
 */
void pcan_setVCCEN(PCAN_PCIEC_CARD *card, int On)
{
	card->VCCenable = (On) ? 1 : 0;
}

/**
 * Deinitializes and frees any associated I2C subsystem from the CAN channel.
 * only call for blinking PCIEC masters.
 * @param card The associated PCAN-ExpressCard
 */
static void pcan_deinitI2C(PCAN_PCIEC_CARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	deinitPCA9553(card);

	/* init equals deinit */
	initPITAGPIO(card);

	i2c_del_adapter(&card->adapter);
}

/**
 * This function is called cyclic from a delayed work
 */
static void pciec_activity_scanner(struct work_struct *work)
{
	PCAN_PCIEC_CARD *card =
		container_of(work, PCAN_PCIEC_CARD, activity_timer.work);
	struct pcandev *dev;
	int i;
	u8 tmp;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(card=%p)\n", __func__, card);
#endif
	if (!card)
		return;

	tmp = card->PCA9553_LS0Shadow;

	/* move VCCEN into tmp */
	tmp &= ~(PCA9553_SET_MASK << PCA9553_VCCEN_OFFSET);
	if (!card->VCCenable)
		tmp |=   PCA9553_TRISTATE << PCA9553_VCCEN_OFFSET;

	/* first check what is to do */
	for (i = 0; i < PCIEC_CHANNELS; i++) {
		dev = card->dev[i];
		if (dev) {
			u8 state = dev->ucActivityState;
			u8 offset = (dev->can_idx) ?
				PCA9553_LED1_OFFSET : PCA9553_LED0_OFFSET;

			tmp &= ~(PCA9553_SET_MASK   << offset);

			switch(state) {
			case ACTIVITY_XMIT:
				tmp |= PCA9553_FAST_BLINK << offset;
				/* automatic fallback */
				dev->ucActivityState = ACTIVITY_IDLE;
				break;

			case ACTIVITY_IDLE:
				tmp |= PCA9553_SLOW_BLINK << offset;
				break;

			case ACTIVITY_INITIALIZED:
				tmp |= PCA9553_LOW << offset;
				break;

			case ACTIVITY_NONE:
			default:
				break;
			}
		}
	}

	/* check if the LS0 settings have changed, only update i2c if so */
	if (tmp != card->PCA9553_LS0Shadow) {
		card->PCA9553_LS0Shadow = tmp;
		writeBytePCA9553(card, 5, card->PCA9553_LS0Shadow);
	}

	/* restart timer */
	if (card->run_activity_timer_cyclic)
	  schedule_delayed_work(&card->activity_timer, HZ);
}

/**
 * Starts the activity scanner
 * @param card The associated card structure
 */
static void pciec_start_activity_scanner(PCAN_PCIEC_CARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(card=%p)\n", __func__, card);
#endif

	INIT_DELAYED_WORK(&card->activity_timer, pciec_activity_scanner);
	card->run_activity_timer_cyclic = 1;
	schedule_delayed_work(&card->activity_timer, HZ);
}

/**
 * Stops the activity scanner
 * @param card The associated card structure
 */
static void pciec_stop_activity_scanner(PCAN_PCIEC_CARD *card)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(card=%p)\n", __func__, card);
#endif

	card->run_activity_timer_cyclic = 0;
	cancel_delayed_work_sync(&card->activity_timer);
}

/**
 * Creates and activates a card infrastructure to handle ExpressCard's LEDs
 * @param pci_Dev The pci device structure as parent for the card's I2C device
 * @param dev The associated pcan device structure
 * @return A pointer to a card structure
 */
PCAN_PCIEC_CARD *pcan_pciec_create_card(struct pci_dev *pciDev,
					struct pcandev *dev)
{
	PCAN_PCIEC_CARD *card = NULL;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	card = (PCAN_PCIEC_CARD *)pcan_malloc(sizeof(PCAN_PCIEC_CARD),
					      GFP_KERNEL);
	if (!card) {
		pr_err(DEVICE_NAME ": %s(): memory alloc failure\n", __func__);
		return NULL;
	}

	memset(card, 0, sizeof(PCAN_PCIEC_CARD));

	dev->port.pci.card = card;
	card->dev[0] = dev;

	/* TODO error check */
	pcan_initI2C(pciDev, card, dev->port.pci.bar0_cfg_addr);

	pciec_start_activity_scanner(card);

	/* driver is loaded */
	dev->ucActivityState = ACTIVITY_INITIALIZED;

	return card;
}

/**
 * Creates and activates a card infrastructure to handle ExpressCard's LEDs
 * @param pci_Dev The pci device structure as parent for the card's I2C device
 * @param dev The associated pcan device structure
 * @return A pointer to a card structure
 */
PCAN_PCIEC_CARD *pcan_pciec_locate_card(struct pci_dev *pciDev,
					struct pcandev *dev)
{
	PCAN_PCIEC_CARD *card = NULL;
	struct list_head *ptr;
	struct pcandev *local_dev = NULL;
#ifdef HANDLE_HOTPLUG
	unsigned long flags;

	pcan_lock_get_irqsave(&pcan_drv.devices_lock, flags);
#endif

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	/*
	 * it is a bit complicated to get the master channel association since
	 * the device did not know its sibling
	 */
	for (ptr = pcan_drv.devices.prev; ptr != &pcan_drv.devices;
							ptr = ptr->prev) {
		local_dev = (struct pcandev *)ptr;

		if ((local_dev != NULL) && (local_dev != dev)  &&
		    (local_dev->can_idx == 0) &&
		    (local_dev->port.pci.dwConfigPort ==
						dev->port.pci.dwConfigPort)) {
			card = local_dev->port.pci.card;
			break;
		}
	}

#ifdef HANDLE_HOTPLUG
	pcan_lock_put_irqrestore(&pcan_drv.devices_lock, flags);
#endif
	if (card) {
		/* reverse lookup initialization */
		dev->port.pci.card = card;
		card->dev[dev->can_idx] = dev;

		/* driver is loaded */
		dev->ucActivityState = ACTIVITY_INITIALIZED;
	}

	return card;
}

/**
 * Removes the card infrastructure to handle ExpressCard's LEDs,
 * only cards will be deleted
 * @param dev The associated pcan device structure
 */
void pcan_pciec_delete_card(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	dev->ucActivityState = ACTIVITY_NONE;

	if (dev->port.pci.card) {
		if (!dev->can_idx) {
			pciec_stop_activity_scanner(dev->port.pci.card);

			pcan_deinitI2C(dev->port.pci.card);

			pcan_free(dev->port.pci.card);
		} else {
			PCAN_PCIEC_CARD *card = dev->port.pci.card;

			/* remove card's association to device */
			card->dev[dev->can_idx] = NULL;
    		}

		/* remove device's association to card */
		dev->port.pci.card = NULL;
	}
}

#endif /* PCIEC_SUPPORT */
