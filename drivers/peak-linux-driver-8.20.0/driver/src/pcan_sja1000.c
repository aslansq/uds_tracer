/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_sja1000.c - all about sja1000 init and data handling
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
 *               Arnaud Westenberg <arnaud@wanadoo.nl>
 *               Matt Waters <Matt.Waters@dynetics.com>
 *               Benjamin Kolb <Benjamin.Kolb@bigfoot.de>
 */
/* #define DEBUG */
/* #undef DEBUG */

#include "src/pcan_common.h"

#include <linux/sched.h>
#include <asm/errno.h>
#include <asm/byteorder.h>
#include <linux/delay.h>

#include "src/pcan_main.h"
#include "src/pcan_fifo.h"
#include "src/pcan_sja1000.h"
#include "src/pcanfd_core.h"

#ifdef NETDEV_SUPPORT
#include "src/pcan_netdev.h"
#endif

/* - if defined, then writer is woken up each time a CAN frame has been written
 *   by the device.
 * - if not defined, then writer is woken up only when no DATA have been read
 *   from Tx fifo (<= 8.7 behaviour).
 *
 * - when undefined, it's *very* hard to INTR a task looping on select(w) +
 *   pcanfd_send_msg() and tx fifo is never full
 * - when defined, it's immediate and tx fifo is always full when looping on
 *   CAN_Write()
 */
#define PCAN_SJA1000_SIGNAL_ON_EACH_WRITE

/* sja1000 registers, only PELICAN mode - TUX like it */
#define SJA1000_MOD		0	/* mode register */
#define SJA1000_CMR		1
#define SJA1000_SR		2
#define SJA1000_IR		3
#define SJA1000_IER		4	/* acceptance code */
#define SJA1000_BTR0		6	/* bus timing 0 */
#define SJA1000_BTR1		7	/* bus timing 1 */
#define SJA1000_OCR		8	/* output control */
#define SJA1000_TR		9

#define ARBIT_LOST_CAPTURE    11      /* transmit buffer: Identifier */
#define ERROR_CODE_CAPTURE    12      /* RTR bit und data length code */
#define ERROR_WARNING_LIMIT   13      /* start byte of data field */
#define RX_ERROR_COUNTER      14
#define TX_ERROR_COUNTER      15

#define ACCEPTANCE_CODE_BASE  16
#define RECEIVE_FRAME_BASE    16
#define TRANSMIT_FRAME_BASE   16

#define ACCEPTANCE_MASK_BASE  20

#define RECEIVE_MSG_COUNTER   29
#define RECEIVE_START_ADDRESS 30

#define CLKDIVIDER            31      /* set bit rate and pelican mode */

/* important sja1000 register contents, SJA1000_MOD register */
#define SLEEP_MODE             0x10
#define ACCEPT_FILTER_MODE     0x08
#define SELF_TEST_MODE         0x04
#define LISTEN_ONLY_MODE       0x02
#define RESET_MODE             0x01
#define NORMAL_MODE            0x00

/* SJA1000_CMR register */
#define TRANSMISSION_REQUEST   0x01
#define ABORT_TRANSMISSION     0x02
#define SINGLE_SHOT_REQUEST    0x03
#define RELEASE_RECEIVE_BUFFER 0x04
#define CLEAR_DATA_OVERRUN     0x08
#define SELF_RX_REQUEST        0x10
#define SELF_RX_SS_REQUEST     0x12

/* SJA1000_SR register */
#define SJA1000_SR_BS		0x80	/* bus status bit */
#define SJA1000_SR_ES		0x40	/* error status bit */
#define SJA1000_SR_TS		0x20	/* transmitting status bit */
#define SJA1000_SR_RS		0x10	/* receiving status bit */
#define SJA1000_SR_TCS		0x08	/* transmission complete status bit */
#define SJA1000_SR_TBS		0x04	/* transmit buffer status bit */
#define SJA1000_SR_DOS		0x02	/* data overrun status bit */
#define SJA1000_SR_RBS		0x01	/* receive buffer status bit */

/* INTERRUPT STATUS register */
#define BUS_ERROR_INTERRUPT    0x80
#define ARBIT_LOST_INTERRUPT   0x40
#define ERROR_PASSIV_INTERRUPT 0x20
#define WAKE_UP_INTERRUPT      0x10
#define DATA_OVERRUN_INTERRUPT 0x08
#define ERROR_WARN_INTERRUPT   0x04
#define TRANSMIT_INTERRUPT     0x02
#define RECEIVE_INTERRUPT      0x01

/* INTERRUPT ENABLE register */
#define BUS_ERROR_SJA1000_IER    0x80
#define ARBIT_LOST_SJA1000_IER   0x40
#define ERROR_PASSIV_SJA1000_IER 0x20
#define WAKE_UP_SJA1000_IER      0x10
#define DATA_OVERRUN_SJA1000_IER 0x08
#define ERROR_WARN_SJA1000_IER   0x04
#define TRANSMIT_SJA1000_IER     0x02
#define RECEIVE_SJA1000_IER      0x01

/* OUTPUT CONTROL register */
#define SJA1000_OCR_TRANSISTOR_P1  0x80
#define SJA1000_OCR_TRANSISTOR_N1  0x40
#define SJA1000_OCR_POLARITY_1     0x20
#define SJA1000_OCR_TRANSISTOR_P0  0x10
#define SJA1000_OCR_TRANSISTOR_N0  0x08
#define SJA1000_OCR_POLARITY_0     0x04
#define SJA1000_OCR_MODE_1         0x02
#define SJA1000_OCR_MODE_0         0x01

/* TRANSMIT or RECEIVE BUFFER */
#define BUFFER_EFF                    0x80 /* set for 29 bit identifier */
#define BUFFER_RTR                    0x40 /* set for RTR request */
#define BUFFER_DLC_MASK               0x0f

/* CLKDIVIDER register */
#define CAN_MODE                      0x80
#define CAN_BYPASS                    0x40
#define RXINT_OUTPUT_ENABLE           0x20
#define CLOCK_OFF                     0x08
#define CLOCK_DIVIDER_MASK            0x07

/* additional informations */
#define CLOCK_HZ			(8*MHz)	/* sja100 frequency */

/* time for mode register to change mode */
#define MODE_REGISTER_SWITCH_TIME	100 /* msec */

/* some CLKDIVIDER register contents, hardware architecture dependend */
#define PELICAN_SINGLE	(CAN_MODE | CAN_BYPASS | 0x07 | CLOCK_OFF)
#define PELICAN_MASTER	(CAN_MODE | CAN_BYPASS | 0x07            )
#define PELICAN_DEFAULT	(CAN_MODE                                )
#define CHIP_RESET	PELICAN_SINGLE

/* hardware depended setup for SJA1000_OCR register */
#define SJA1000_OCR_SETUP	(SJA1000_OCR_TRANSISTOR_P0 | \
				 SJA1000_OCR_TRANSISTOR_N0 | \
				 SJA1000_OCR_MODE_1)

/* the interrupt enables */
#define SJA1000_IER_SETUP	(RECEIVE_SJA1000_IER | \
				 TRANSMIT_SJA1000_IER | \
				 DATA_OVERRUN_SJA1000_IER | \
				 BUS_ERROR_SJA1000_IER | \
				 ERROR_PASSIV_SJA1000_IER | \
				 ERROR_WARN_SJA1000_IER)

/* the maximum number of handled messages in one Rx interrupt */
#define MAX_MESSAGES_PER_INTERRUPT_DEF	8
#define MAX_MESSAGES_PER_INTERRUPT_MAX	16

/* the maximum number of handled sja1000 interrupts in 1 handler entry */
#define MAX_INTERRUPTS_PER_ENTRY_DEF	6	/* max seen is 3 */
#define MAX_INTERRUPTS_PER_ENTRY_MAX	8

/* constants from Arnaud Westenberg email:arnaud@wanadoo.nl */
#define MAX_TSEG1	15
#define MAX_TSEG2	7
#define BTR1_SAM	(1<<1)

static uint irqmaxloop = MAX_INTERRUPTS_PER_ENTRY_DEF;
module_param(irqmaxloop, uint, 0644);
MODULE_PARM_DESC(irqmaxloop, " max loops in ISR per CAN (0=default def="
				__stringify(MAX_INTERRUPTS_PER_ENTRY_DEF)")");

static uint irqmaxrmsg = MAX_MESSAGES_PER_INTERRUPT_DEF;
module_param(irqmaxrmsg, uint, 0644);
MODULE_PARM_DESC(irqmaxrmsg, " max msgs read per Rx IRQ (0=default def="
				__stringify(MAX_MESSAGES_PER_INTERRUPT_DEF)")");

/* Public timing capabilites
 * .sysclock_Hz = 16*MHz =>  Clock Rate = 8 MHz
 */
const struct pcanfd_bittiming_range sja1000_capabilities = {

	.brp_min = 1,
	.brp_max = 64,
	.brp_inc = 1,

	.tseg1_min = 1,		/* constant for v <= 7.13 */
	.tseg1_max = MAX_TSEG1+1,
	.tseg2_min = 1,		/* constant for v <= 7.13 */
	.tseg2_max = MAX_TSEG2+1,

	.sjw_min = 1,
	.sjw_max = 4,
};

const pcanfd_mono_clock_device sja1000_clocks = {
	.count = 1,
	.list = {
		[0] = { .clock_Hz = 8*MHz, .clock_src = 16*MHz, },
	}
};

static inline u8 __sja1000_write_cmd(struct pcandev *dev, u8 data)
{
	dev->writereg(dev, SJA1000_CMR, data);

	/* draw a breath after writing the command register */
	return dev->readreg(dev, SJA1000_SR);
}

/* guards writing sja1000's command register in multicore environments */
static inline u8 sja1000_write_cmd(struct pcandev *dev, u8 data)
{
	u8 sr;
	pcan_lock_irqsave_ctxt lck_ctx;

	pcan_lock_get_irqsave(&dev->wlock, lck_ctx);

	sr = __sja1000_write_cmd(dev, data);

	pcan_lock_put_irqrestore(&dev->wlock, lck_ctx);

	return sr;
}

/* define the maximum loops polling on a SR bit to go to 1.
 * when no udelay() between each readreg(), then this should be > 210.
 * when udelay(10) is put between each readreg(), should be > 21
 */
#define SJA1000_SR_MAX_POLL	50

static inline int pcan_pcie_wait_for_status(struct pcandev *dev, u8 mask)
{
	int i;

	for (i = SJA1000_SR_MAX_POLL; i; i--) {
		if (dev->readreg(dev, SJA1000_SR) & mask)
			break;
		udelay(10);
	}

	return i;
}

/* switches the chip into reset mode */
static int set_reset_mode(struct pcandev *dev)
{
	u32 dwStart = get_mtime();
	u8 tmp;

	tmp = dev->readreg(dev, SJA1000_MOD);
	while (!(tmp & RESET_MODE) &&
			((get_mtime() - dwStart) < MODE_REGISTER_SWITCH_TIME)) {
		/* force into reset mode */
		dev->writereg(dev, SJA1000_MOD, RESET_MODE);

		udelay(1);
		tmp = dev->readreg(dev, SJA1000_MOD);
	}

	if (!(tmp & RESET_MODE))
		return -EIO;

	return 0;
}

/* switches the chip back from reset mode */
static int set_normal_mode(struct pcandev *dev, u8 ucModifier)
{
	u32 dwStart = get_mtime();
	u8  tmp;

	tmp = dev->readreg(dev, SJA1000_MOD);
	while ((tmp != ucModifier) &&
			((get_mtime() - dwStart) < MODE_REGISTER_SWITCH_TIME)) {
		/* force into normal mode */
		dev->writereg(dev, SJA1000_MOD, ucModifier);

		udelay(1);
		tmp = dev->readreg(dev, SJA1000_MOD);
	}

	if (tmp != ucModifier)
		return -EIO;

	return 0;
}

/* interrupt enable and disable */
static inline void sja1000_irq_enable_mask(struct pcandev *dev, u8 mask)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u, mask=%02xh)\n",
		__func__, dev->can_idx+1, mask);
#endif
	dev->writereg(dev, SJA1000_IER, mask);
}

static inline void sja1000_irq_disable_mask(struct pcandev *dev, u8 mask)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u, mask=%02xh)\n",
		__func__, dev->can_idx+1, mask);
#endif
	dev->writereg(dev, SJA1000_IER, ~mask);
}

static inline void sja1000_irq_enable(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	sja1000_irq_enable_mask(dev, SJA1000_IER_SETUP);
}

static inline void sja1000_irq_disable(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	sja1000_irq_disable_mask(dev, 0xff);
}

/* find the proper clock divider */
static inline u8 clkdivider(struct pcandev *dev)
{
	/* crystal based */
	if (!dev->props.ucExternalClock)
		return PELICAN_DEFAULT;

	/* configure clock divider register, switch into pelican mode,
	 * depended of of type
	 */
	switch (dev->props.ucMasterDevice) {
	case CHANNEL_SLAVE:
	case CHANNEL_SINGLE:
		/* neither a slave nor a single device distribute the clock */
		return PELICAN_SINGLE;

	default:
		/* ...but a master does */
		return PELICAN_MASTER;
	}
}

/* init CAN-chip */
int sja1000_open(struct pcandev *dev, u16 btr0btr1, u8 bExtended,
		 u8 bListenOnly)
{
	int err;
	u8 _clkdivider = clkdivider(dev);
	u8 ucModifier = (bListenOnly) ? LISTEN_ONLY_MODE : NORMAL_MODE;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u, btr0btr1=%04xh, ext=%u, lonly=%u)\n",
		__func__, dev->nMinor, btr0btr1, bExtended, bListenOnly);
#endif
	sja1000_irq_disable(dev);

	sja1000_write_cmd(dev, ABORT_TRANSMISSION);

	/* switch to reset */
	err = set_reset_mode(dev);
	if (err) {
		pr_err(DEVICE_NAME ": set_reset_mode(CAN%u) failed (err %d)\n",
			dev->can_idx+1, err);
		goto fail;
	}

	/* configure clock divider register, switch into pelican mode,
	 * depended of of type
	 */
	dev->writereg(dev, CLKDIVIDER, _clkdivider);

	/* configure acceptance code registers */
	dev->writereg(dev, ACCEPTANCE_CODE_BASE,   0xff);
	dev->writereg(dev, ACCEPTANCE_CODE_BASE+1, 0xff);
	dev->writereg(dev, ACCEPTANCE_CODE_BASE+2, 0xff);
	dev->writereg(dev, ACCEPTANCE_CODE_BASE+3, 0xff);

	/* configure all acceptance mask registers to don't care */
	dev->writereg(dev, ACCEPTANCE_MASK_BASE,   0xff);
	dev->writereg(dev, ACCEPTANCE_MASK_BASE+1, 0xff);
	dev->writereg(dev, ACCEPTANCE_MASK_BASE+2, 0xff);
	dev->writereg(dev, ACCEPTANCE_MASK_BASE+3, 0xff);

	/* configure bus timing registers */
	dev->writereg(dev, SJA1000_BTR0, (u8)((btr0btr1 >> 8) & 0xff));
	dev->writereg(dev, SJA1000_BTR1, (u8)((btr0btr1     ) & 0xff));

	/* configure output control registers */
	dev->writereg(dev, SJA1000_OCR, SJA1000_OCR_SETUP);

	/* clear error counters */
	dev->writereg(dev, RX_ERROR_COUNTER, 0x00);
	dev->writereg(dev, TX_ERROR_COUNTER, 0x00);

	/* clear any pending interrupt */
	dev->readreg(dev, SJA1000_IR);

	/* enter normal operating mode */
	err = set_normal_mode(dev, ucModifier);
	if (err) {
		pr_err("%s: set_normal_mode(CAN%u) failed (err %d)\n",
				DEVICE_NAME, dev->can_idx+1, err);
		goto fail;
	}

	/* enable CAN interrupts */
	sja1000_irq_enable(dev);

	/* setup (and notify) the initial state to ERROR_ACTIVE */
	pcan_soft_error_active(dev);
fail:
	return err;
}

/* release CAN-chip.
 *
 * This callabck is called by:
 *
 * 1 - ioctl(SET_INIT) if the device was opened before.
 * 2 - close() as the 1st callback called.
 */
void sja1000_release(struct pcandev *dev)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u)\n", __func__, dev->nMinor);
#endif

	/* disable CAN interrupts and set chip in reset mode
	 * Note: According to SJA1000 DS, an ABORT cmd generates an INT, so
	 * disable SJA1000 INT before ABORTing
	 */
	sja1000_irq_disable(dev);

	/* PCIe: be sure that the transmission has completed */
	if (!pcan_pcie_wait_for_status(dev, SJA1000_SR_TBS)) {

		/* abort pending transmissions */
		sja1000_write_cmd(dev, ABORT_TRANSMISSION);
	}

	set_reset_mode(dev);
}

/* reset the CAN controller (bus=off then bus=on) */
int sja1000_reset(struct pcandev *dev)
{
	int err;

	err = set_reset_mode(dev);
	if (err)
		return err;

	/* clear error counters */
	dev->writereg(dev, RX_ERROR_COUNTER, 0x00);
	dev->writereg(dev, TX_ERROR_COUNTER, 0x00);

	/* enter normal operating mode */
	err = set_normal_mode(dev,
			dev->init_settings.flags & PCANFD_INIT_LISTEN_ONLY ?
				LISTEN_ONLY_MODE : NORMAL_MODE);

	if (!err)
		pcan_soft_error_active(dev);

	return err;
}

/* read CAN-data from chip, supposed a message is available */
static int __sja1000_read(struct pcandev *dev)
{
	int msgs = irqmaxrmsg;
	u8 fi, dlc;
	ULCONV localID;
	struct pcanfd_rxmsg f;
	int result = 0;
	int i;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u)\n", __func__, dev->nMinor);
#endif

	for (msgs = 0; msgs < irqmaxrmsg; msgs++) {

		u8 dreg = dev->readreg(dev, SJA1000_SR);
		if (!(dreg & SJA1000_SR_RBS)) {
			break;
		}

		fi = dev->readreg(dev, RECEIVE_FRAME_BASE);

		dlc = fi & BUFFER_DLC_MASK;

		f.msg.data_len = dlc;
		if (f.msg.data_len > PCANFD_CAN20_MAXDATALEN)
			f.msg.data_len = PCANFD_CAN20_MAXDATALEN;

		if (fi & BUFFER_EFF) {
			/* extended frame format (EFF) */
			f.msg.flags = MSGTYPE_EXTENDED;
			dreg = RECEIVE_FRAME_BASE + 5;

#if defined(__LITTLE_ENDIAN)
			localID.uc[3] = dev->readreg(dev, RECEIVE_FRAME_BASE+1);
			localID.uc[2] = dev->readreg(dev, RECEIVE_FRAME_BASE+2);
			localID.uc[1] = dev->readreg(dev, RECEIVE_FRAME_BASE+3);
			localID.uc[0] = dev->readreg(dev, RECEIVE_FRAME_BASE+4);
#else
			localID.uc[0] = dev->readreg(dev, RECEIVE_FRAME_BASE+1);
			localID.uc[1] = dev->readreg(dev, RECEIVE_FRAME_BASE+2);
			localID.uc[2] = dev->readreg(dev, RECEIVE_FRAME_BASE+3);
			localID.uc[3] = dev->readreg(dev, RECEIVE_FRAME_BASE+4);
#endif
			f.msg.id = localID.ul >> 3;

		} else {
			/* standard frame format (SFF) */
			f.msg.flags = MSGTYPE_STANDARD;
			dreg = RECEIVE_FRAME_BASE + 3;

			localID.ul = 0;
#if defined(__LITTLE_ENDIAN)
			localID.uc[3] = dev->readreg(dev, RECEIVE_FRAME_BASE+1);
			localID.uc[2] = dev->readreg(dev, RECEIVE_FRAME_BASE+2);
#else
			localID.uc[0] = dev->readreg(dev, RECEIVE_FRAME_BASE+1);
			localID.uc[1] = dev->readreg(dev, RECEIVE_FRAME_BASE+2);
#endif
			f.msg.id = localID.ul >> 21;
		}

		pcanfd_msg_store_dlc(&f.msg, dlc);

		/* clear aligned data section */
		*(__u64 *)&f.msg.data[0] = (__u64)0;

		for (i = 0; i < f.msg.data_len; i++)
			f.msg.data[i] = dev->readreg(dev, dreg++);

		/* release the receive buffer asap */

		/* Note: [AN97076 p 35]: "(in PeliCAN mode the Receive
		 * Interrupt (RI) is cleared first, when giving the Release
		 * Buffer command)
		 */
		dreg = sja1000_write_cmd(dev, RELEASE_RECEIVE_BUFFER);

		/* complete now frame to give to application: */
		f.msg.type = PCANFD_TYPE_CAN20_MSG;
		if (fi & BUFFER_RTR)
			f.msg.flags |= MSGTYPE_RTR;


		/* put into specific data sink and save the last result */
		if (pcan_xxxdev_rx(dev, &f) > 0)
			result++;
	}

	return result;
}

/* write CAN-data to chip */
static int sja1000_write_msg(struct pcandev *dev, struct pcanfd_msg *pf)
{
	ULCONV localID;
	u8 fi, dreg, cmd;
	int i;

	if (!(dev->readreg(dev, SJA1000_SR) & SJA1000_SR_TBS))
		pr_warn(DEVICE_NAME ": CAN%u TBS==0!\n", dev->can_idx+1);

	localID.ul = pf->id;

	fi = pf->data_len;
	if (fi == PCANFD_CAN20_MAXDATALEN) {
		u8 tmp_dlc = pcanfd_msg_read_dlc(pf);
		if (tmp_dlc > fi)
			fi = tmp_dlc;
	}

	if (pf->flags & PCANFD_MSG_RTR)
		fi |= BUFFER_RTR;

	if (pf->flags & PCANFD_MSG_EXT) {
		dreg = TRANSMIT_FRAME_BASE + 5;
		fi |= BUFFER_EFF;

		localID.ul <<= 3;

		dev->writereg(dev, TRANSMIT_FRAME_BASE, fi);

#if defined(__LITTLE_ENDIAN)
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 1, localID.uc[3]);
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 2, localID.uc[2]);
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 3, localID.uc[1]);
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 4, localID.uc[0]);
#else
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 1, localID.uc[0]);
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 2, localID.uc[1]);
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 3, localID.uc[2]);
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 4, localID.uc[3]);
#endif
	} else {
		dreg = TRANSMIT_FRAME_BASE + 3;

		localID.ul <<= 21;

		dev->writereg(dev, TRANSMIT_FRAME_BASE, fi);

#if defined(__LITTLE_ENDIAN)
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 1, localID.uc[3]);
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 2, localID.uc[2]);
#else
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 1, localID.uc[0]);
		dev->writereg(dev, TRANSMIT_FRAME_BASE + 2, localID.uc[1]);
#endif
	}

	for (i = 0; i < pf->data_len; i++)
		dev->writereg(dev, dreg++, pf->data[i]);

	/* request a transmission */

	if (pf->flags & MSGTYPE_SINGLESHOT) {
		if (pf->flags & MSGTYPE_SELFRECEIVE)
			cmd = SELF_RX_SS_REQUEST;
		else
			cmd = SINGLE_SHOT_REQUEST;
	} else {
		if (pf->flags & MSGTYPE_SELFRECEIVE)
			cmd = SELF_RX_REQUEST;
		else
			cmd = TRANSMISSION_REQUEST;
	}

	/* SGr Note: why using a mutex for a single command while we're already
	 * in a critical section?
	 */
	sja1000_write_cmd(dev, cmd);

	/* Note: waiting for TBS is not a good idea here because this function
	 * is called by the ISR, and the ISR doesn't need (and shouldn't)
	 * actually wait!
	 */

	/* Tx engine is started: next write will be made by ISR */
	pcan_set_tx_engine(dev, TX_ENGINE_STARTED);

	return 0;
}

static int __sja1000_write(struct pcandev *dev, struct pcanusr *ctx)
{
	struct pcanfd_txmsg tx;

	/* get a fifo element and step forward */
	int err = pcan_txfifo_get(dev, &tx);
	if (err >= 0)
		err = sja1000_write_msg(dev, &tx.msg);

	return err;
}

/* write CAN-data from FIFO to chip
 *
 * This function is generally called from user-space task, when TX_ENGINE is
 * STOPPED. This call is protected by isr_lock lock.
 */
int sja1000_write(struct pcandev *dev, struct pcanusr *ctx)
{
	int err = -EIO;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u)\n", __func__, dev->nMinor);
#endif

	/* Check (and wait for) the SJA1000 Tx buffer to be empty before
	 * writing in
	 */
	if (pcan_pcie_wait_for_status(dev, SJA1000_SR_TBS))
		err = __sja1000_write(dev, ctx);
	else
		pr_err(DEVICE_NAME
			": %s() failed: %s CAN%u Transmit Buffer not empty\n",
			__func__, dev->adapter->name, dev->can_idx+1);

	return err;
}

/* SJA1000 interrupt handler */
irqreturn_t __pcan_sja1000_irqhandler(struct pcandev *dev)
{
	irqreturn_t ret = PCAN_IRQ_NONE;
	int j, err;
	u32 rwakeup = 0;
	u32 wwakeup = 0;
	struct pcanfd_rxmsg ef;

	memset(&ef, 0, sizeof(ef));

	for (j = 0; j < irqmaxloop; j++) {

		/* Note: [AN97076 p 35]: "all interrupt flags are cleared" */
		u8 irqstatus = dev->readreg(dev, SJA1000_IR);
		u8 chipstatus;

		if (!irqstatus)
			break;

		/* quick hack to badly workaround write stall
		 * if ((irqstatus & TRANSMIT_INTERRUPT) ||
		 *     (!atomic_read(&dev->hw_is_ready_to_send) &&
		 *      !pcan_fifo_empty(&dev->tx_fifo) &&
		 *      (dev->readreg(dev, SJA1000_SR) & SJA1000_SR_TBS)))
		 */
		if (irqstatus & TRANSMIT_INTERRUPT) {
			pcan_lock_irqsave_ctxt flags;

			pcan_lock_get_irqsave(&dev->isr_lock, flags);

			dev->tx_irq_counter++;

			/* handle transmission */
			err = __sja1000_write(dev, NULL);
			switch (err) {
			case -ENODATA:
				pcan_set_tx_engine(dev, TX_ENGINE_STOPPED);
#ifdef PCAN_SJA1000_SIGNAL_ON_EACH_WRITE
				/* if device is being closed, then wakeup 
				 * when last frame has been written only
				 */
				if (dev->flags & PCAN_DEV_CLOSING)
#endif
					wwakeup++;
				break;
			case 0:
#ifdef PCAN_SJA1000_SIGNAL_ON_EACH_WRITE
				/* if device is not being closed, wakeup each
				 * time a frame has been read from tx fifo.
				 * Otherwise, event is signaled only when tx
				 * fifo is empty.
				 */
				if (!(dev->flags & PCAN_DEV_CLOSING))
					wwakeup++;
#endif
				dev->tx_frames_counter++;
				break;
			default:
				dev->nLastError = err;
				pcan_handle_error_ctrl(dev, &ef, PCANFD_TX_OVERFLOW);
			}

			pcan_lock_put_irqrestore(&dev->isr_lock, flags);

			/* reset to ACTIVITY_IDLE by cyclic timer */
			dev->ucActivityState = ACTIVITY_XMIT;
		}

		if (irqstatus & RECEIVE_INTERRUPT) {

			dev->rx_irq_counter++;

			/* handle reception: put to input queues */
			err = __sja1000_read(dev);

			/* successfully enqueued at least ONE msg into FIFO */
			if (err > 0)
				rwakeup++;

			/* reset to ACTIVITY_IDLE by cyclic timer */
			dev->ucActivityState = ACTIVITY_XMIT;
		}

		if (irqstatus & DATA_OVERRUN_INTERRUPT) {
			sja1000_write_cmd(dev, CLEAR_DATA_OVERRUN);

#ifdef DEBUG
			pr_info(DEVICE_NAME ": %s(pcan%d), DATA_OVR\n",
				__func__, dev->nMinor);
#endif
			/* handle data overrun */
			pcan_handle_error_ctrl(dev, &ef, PCANFD_RX_OVERFLOW);

			/* reset to ACTIVITY_IDLE by cyclic time */
			dev->ucActivityState = ACTIVITY_XMIT;
		}

		/* should have a look to RX_ERROR_COUNTER and TX_ERROR_COUNTER
		 * SJA1000 registers! Reading these value could be nice...
		 */
		if (irqstatus & (ERROR_PASSIV_INTERRUPT|ERROR_WARN_INTERRUPT)) {

			chipstatus = dev->readreg(dev, SJA1000_SR);

#ifdef DEBUG
			pr_info(DEVICE_NAME ": %s(pcan%d): "
				"irqstatus=%xh chipstatus=0x%02x\n",
				__func__, dev->nMinor,
				irqstatus, chipstatus);
#endif
			switch (chipstatus & (SJA1000_SR_BS | SJA1000_SR_ES)) {
			case 0x00:
				/* error active, clear only local status */
				pcan_handle_error_active(dev, &ef);
				break;
			case SJA1000_SR_BS:
			case SJA1000_SR_BS | SJA1000_SR_ES:
				/* bus-off */
				pcan_handle_busoff(dev, &ef);
				break;
			case SJA1000_SR_ES:

				/* either enter or leave error passive status */
				if (irqstatus & ERROR_PASSIV_INTERRUPT) {
					/* enter error passive state */
					pcan_handle_error_status(dev, &ef,
									0, 1);
				} else {
					/* warning limit reached event */
					pcan_handle_error_status(dev, &ef,
									1, 0);
				}
				break;
			}

			/* (simply to enter into next condition) */
			irqstatus |= BUS_ERROR_INTERRUPT;

		} else if (irqstatus & BUS_ERROR_INTERRUPT) {

			u8 ecc = dev->readreg(dev, ERROR_CODE_CAPTURE);

			pcan_handle_error_msg(dev, &ef,
					(ecc & 0xc0) >> 6, (ecc & 0x1f),
					(ecc & 0x20), 0);

#ifdef DEBUG
			pr_info(DEVICE_NAME ": %s(pcan%d): BUS_ERROR %02xh\n",
				__func__, dev->nMinor, ecc);
#endif
		}

		if (irqstatus & BUS_ERROR_INTERRUPT) {

			dev->rx_error_counter = 
				dev->readreg(dev, RX_ERROR_COUNTER);
			dev->tx_error_counter =
				dev->readreg(dev, TX_ERROR_COUNTER);

			/* reset to ACTIVITY_IDLE by cyclic timer */
			dev->ucActivityState = ACTIVITY_XMIT;
		}

		/* if any error condition occurred, send an error frame to
		 * userspace
		 */
		if (ef.msg.type) {

			/* put into specific data sink */
			if (pcan_xxxdev_rx(dev, &ef) > 0)
				rwakeup++;

			/* clear for next loop */
			memset(&ef, 0, sizeof(ef));
		}

		ret = PCAN_IRQ_HANDLED;
	}

	if (wwakeup) {
		/* signal I'm ready to write */
		pcan_event_signal(&dev->out_event);

#ifdef NETDEV_SUPPORT
		if (dev->netdev)
			netif_wake_queue(dev->netdev);
#endif
	}

	if (rwakeup) {
#ifndef NETDEV_SUPPORT
		pcan_event_signal(&dev->in_event);
#endif
	}

	return ret;
}

irqreturn_t pcan_sja1000_irqhandler(struct pcandev *dev)
{
	irqreturn_t err;

	err = __pcan_sja1000_irqhandler(dev);

	return err;
}

#ifndef NO_RT
int sja1000_irqhandler(rtdm_irq_t *irq_context)
{
	struct pcanusr *ctx = rtdm_irq_get_arg(irq_context,
							struct pcanusr);
	struct pcandev *dev = ctx->dev;

#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
irqreturn_t sja1000_irqhandler(int irq, void *arg, struct pt_regs *pt)
{
	struct pcandev *dev = (struct pcandev *)arg;
#else
irqreturn_t sja1000_irqhandler(int irq, void *arg)
{
	struct pcandev *dev = (struct pcandev *)arg;
#endif

	return pcan_sja1000_irqhandler(dev);
}

/* probe for a sja1000 - use it only in reset mode! */
int sja1000_probe(struct pcandev *dev)
{
	u8 tmp;
	u8 _clkdivider = clkdivider(dev);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(pcan%u)\n", __func__, dev->nMinor);
#endif
	/* do some check on module parameters */
	if (!irqmaxloop)
		irqmaxloop = MAX_INTERRUPTS_PER_ENTRY_DEF;
	else if (irqmaxloop > MAX_INTERRUPTS_PER_ENTRY_MAX)
		irqmaxloop = MAX_INTERRUPTS_PER_ENTRY_MAX;

	if (!irqmaxrmsg)
		irqmaxrmsg = MAX_MESSAGES_PER_INTERRUPT_DEF;
	else if (irqmaxrmsg > MAX_MESSAGES_PER_INTERRUPT_MAX)
		irqmaxrmsg = MAX_MESSAGES_PER_INTERRUPT_MAX;

	/* trace the clockdivider register to test for sja1000 / 82c200 */
	tmp = dev->readreg(dev, CLKDIVIDER);
	if (tmp & 0x10) {
		pr_err(DEVICE_NAME ": %s CAN%u: abnormal CLKDIVIDER %02Xh\n",
			dev->adapter->name, dev->can_idx+1, tmp);
		goto fail;
	}

	/*  until here, it's either a 82c200 or a sja1000 */
	if (set_reset_mode(dev))
		goto fail;

	/* switch to PeliCAN mode */
	dev->writereg(dev, CLKDIVIDER, _clkdivider);

	/* precautionary disable interrupts */
	sja1000_irq_disable(dev);
	//wmb();

	/* new 7.5: PELICAN mode takes sometimes longer: adding some delay
	 * solves the problem (many thanks to Hardi Stengelin)
	 */
	udelay(10);  /* Wait until the pelican mode is activ */

	tmp = dev->readreg(dev, SJA1000_SR);
	if ((tmp & 0x30) != 0x30) {
		pr_err(DEVICE_NAME ": %s CAN%u: abnormal SJA1000_SR %02xh\n",
			dev->adapter->name, dev->can_idx+1, tmp);
		goto fail;
	}

	if (tmp & SJA1000_SR_TBS)
		/* Writing is now ok */
		pcan_set_tx_engine(dev, TX_ENGINE_STOPPED);

	/* clear any pending INT */
	tmp = dev->readreg(dev, SJA1000_IR);
	if (tmp & 0xfb) {
		pr_err(DEVICE_NAME ": %s CAN%u: abnormal SJA1000_IR %02xh\n",
			dev->adapter->name, dev->can_idx+1, tmp);
		goto fail;
	}

	tmp = dev->readreg(dev, RECEIVE_MSG_COUNTER);
	if (tmp) {
		pr_err(DEVICE_NAME
			": %s CAN%u: abnormal RECEIVE_MSG_COUNTER %02xh\n",
			dev->adapter->name, dev->can_idx+1, tmp);
		goto fail;
	}

	return 0;

fail:
	/* no such device or address */
	return -ENXIO;
}

/* get BTR0BTR1 init values */
u16 sja1000_bitrate(u32 dwBitRate, u32 sample_pt, u32 sjw)
{
	struct pcan_bittiming bt;
	u16 wBTR0BTR1;

	memset(&bt, '\0', sizeof(bt));
	bt.bitrate = dwBitRate;
	bt.sjw = (sjw) ? sjw : sja1000_capabilities.sjw_min;
	bt.sample_point = sample_pt;

	pcan_bitrate_to_bittiming(&bt, &sja1000_capabilities, CLOCK_HZ);

	wBTR0BTR1 = pcan_bittiming_to_btr0btr1(&bt);

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s() %u bps = 0x%04x\n",
		__func__, dwBitRate, wBTR0BTR1);
#endif
	return wBTR0BTR1;
}
