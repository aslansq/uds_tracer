/* SPDX-License-Identifier: GPL-2.0 */
/*
 * all parts to handle the SPI flash of the PCIe devices.
 *
 * Copyright (C) 2020 PEAK System-Technik GmbH <www.peak-system.com>
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
 * Author:       Stephane Grosjean <s.grosjean@peak-system.com>
 */
/* #define DEBUG */
/* #undef DEBUG */

#include "src/pcan_common.h"	/* must always be the 1st include */
#include "src/pcan_pci.h"
#include "src/pcan_pci_spi.h"

#ifdef DEBUG_PCI_LITE
#define DEBUG_TRACE_CMD

#ifdef DEBUG_PCI
#define DEBUG_TRACE_FLASH
#endif
#endif

/* delay (in ms) waiting for status bit */
#define SPI_BUSY_WAIT_DELAY	10

/* SPI registers access base in cfg addr */
#define FLASH_UPDATE_BASE	0x48

/* SPI access is mapped on the 3xBYTEs starting from FLASH_UPDATE_BASE */
#define SPI_DREG		0
#define SPI_STAT		1
#define SPI_CS			2

/* JEDEC commands
 * (see also: include/linux/mtd/spi-nor.h)
 */
#define JEDEC_BYTE_PROGRAM	0x02
#define JEDEC_READ		0x03
#define JEDEC_RDSR		0x05
#define JEDEC_WREN		0x06
#define JEDEC_BE_D8		0xd8

#define JEDEC_PAGE_SIZE		256

/* layer 1 */
static u8 pcan_pci_spi_read_reg(struct pcan_pci_adapter *pcan_pci,
				      int spi_reg)
{
	return readb(pcan_pci->bar0_addr + FLASH_UPDATE_BASE + spi_reg);
}

static void pcan_pci_spi_write_reg(struct pcan_pci_adapter *pcan_pci,
					 int spi_reg, u8 v)
{
	writeb(v, pcan_pci->bar0_addr + FLASH_UPDATE_BASE + spi_reg);
}

static int pcan_pci_spi_wait_while_busy(struct pcan_pci_adapter *pcan_pci,
					unsigned long delay_ms)
{
	u64 timeout = pcan_getnow_ns() + ((u64 )delay_ms * 1000 * 1000);

	/* loop while busy */
	do {
		u8 status = pcan_pci_spi_read_reg(pcan_pci, SPI_STAT);
		if (!(status & 1))
			return 0;

		/* relax */
		pcan_usleep(10);

	} while (pcan_getnow_ns() < timeout);

	return -EBUSY;
}

/* layer 2 */
static int pcan_pci_spi_flash_init(struct pcan_pci_adapter *pcan_pci)
{
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): enter\n",
		__func__, pcan_pci->adapter.name);
#endif

	/* initial wait for busy (10 ms max) */
	err = pcan_pci_spi_wait_while_busy(pcan_pci, SPI_BUSY_WAIT_DELAY);
	if (err) {
		pr_err(DEVICE_NAME ": %s: flash init failure (err %d)\n",
		       pcan_pci->adapter.name, err);
		return err;
	}

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): device not busy: drive CS low\n",
		__func__, pcan_pci->adapter.name);
#endif

	/* drive CS low */
	pcan_pci_spi_write_reg(pcan_pci, SPI_CS, 0);

	/* dummy read busy (only for small delay after CS low) */
	pcan_pci_spi_read_reg(pcan_pci, SPI_STAT);
	pcan_pci_spi_read_reg(pcan_pci, SPI_STAT);
	pcan_pci_spi_read_reg(pcan_pci, SPI_STAT);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): leave\n",
		__func__, pcan_pci->adapter.name);
#endif

	return 0;
}

static void pcan_pci_spi_flash_exit(struct pcan_pci_adapter *pcan_pci)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): enter\n",
		__func__, pcan_pci->adapter.name);
#endif

	/* dummy read busy (for delay) */
	pcan_pci_spi_read_reg(pcan_pci, SPI_STAT);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): drive CS high\n",
		__func__, pcan_pci->adapter.name);
#endif

	/* drive CS high */
	pcan_pci_spi_write_reg(pcan_pci, SPI_CS, 1);

	/* dummy read busy (only for small delay after CS low) */
	pcan_pci_spi_read_reg(pcan_pci, SPI_STAT);
	pcan_pci_spi_read_reg(pcan_pci, SPI_STAT);
	pcan_pci_spi_read_reg(pcan_pci, SPI_STAT);

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): leave\n",
		__func__, pcan_pci->adapter.name);
#endif
}

static int pcan_pci_spi_write_data_reg(struct pcan_pci_adapter *pcan_pci, u8 b)
{
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s, b=%02xh): enter\n",
		__func__, pcan_pci->adapter.name, b);
#endif

	/* start byte transfer by writing data register */
	pcan_pci_spi_write_reg(pcan_pci, SPI_DREG, b);

	/* wait for busy going low */
	err = pcan_pci_spi_wait_while_busy(pcan_pci, SPI_BUSY_WAIT_DELAY);
	if (err) {
		pr_err(DEVICE_NAME ": %s: flash write failure (err %d)\n",
		       pcan_pci->adapter.name, err);
		return err;
	}

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): leave\n",
		__func__, pcan_pci->adapter.name);
#endif

	/* read cycle */
	return pcan_pci_spi_read_reg(pcan_pci, SPI_DREG);
}

/* layer 3 */
static int pcan_pci_spi_flash_write_cmd(struct pcan_pci_adapter *pcan_pci,
					u8 cmd, u8 *s)
{
	int err;

#ifdef DEBUG_TRACE_CMD
	pr_info(DEVICE_NAME ": %s(%s, cmd=%02xh): enter\n",
		__func__, pcan_pci->adapter.name, cmd);
#endif

	/* drive CS low */
	err = pcan_pci_spi_flash_init(pcan_pci);
	if (!err) {

		/* write the cmd */
		err = pcan_pci_spi_write_data_reg(pcan_pci, cmd);
		if (err >= 0) {

#ifdef DEBUG_TRACE_CMD
			pr_info(DEVICE_NAME ": %s(%s, cmd=%02xh): err=%d\n",
				__func__, pcan_pci->adapter.name, cmd, err);
#endif
			err = 0;

			/* if the cmd waits for a response, do another
			 * write cycle
			 */
			if (s) {
				err = pcan_pci_spi_write_data_reg(pcan_pci, 0xff);
				if (err >= 0) {
					*s = err;
					err = 0;
				}
			}
		}

		/* drive CS high */
		pcan_pci_spi_flash_exit(pcan_pci);
	}

#ifdef DEBUG_TRACE_CMD
	pr_info(DEVICE_NAME ": %s(%s, cmd=%02xh): returns %d\n",
		__func__, pcan_pci->adapter.name, cmd, err);
#endif

	return err;
}

static inline
int pcan_pci_spi_flash_write_cmd_enable(struct pcan_pci_adapter *pcan_pci)
{
	return pcan_pci_spi_flash_write_cmd(pcan_pci, JEDEC_WREN, NULL);
}

/*
 * cmd	block size erased
 * D8h	64K (JEDEC standard)
 * D7h	4K
 * 52h	32k 
 */
static int pcan_pci_spi_flash_block_erase(struct pcan_pci_adapter *pcan_pci,
					  u32 addr)
{
	const u8 erase_cmd = JEDEC_BE_D8;
	u64 timeout;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): addr=0x%xh\n",
		__func__, pcan_pci->adapter.name, addr);
#endif

	/* Write Enable (WREN) instruction */
	pcan_pci_spi_flash_write_cmd_enable(pcan_pci);

	/* drive CS low */
	err = pcan_pci_spi_flash_init(pcan_pci);
	if (err)
		return err;

	/* write erase cmd */
	err = pcan_pci_spi_write_data_reg(pcan_pci, erase_cmd);
	if (err < 0)
		goto lbl_flash_exit;

	/* address */
	err = pcan_pci_spi_write_data_reg(pcan_pci, (u8 )((addr >> 16) & 0xff));
	if (err < 0)
		goto lbl_flash_exit;

	err = pcan_pci_spi_write_data_reg(pcan_pci, (u8 )((addr >> 8) & 0xff));
	if (err < 0)
		goto lbl_flash_exit;

	err = pcan_pci_spi_write_data_reg(pcan_pci, (u8 )((addr >> 0) & 0xff));

lbl_flash_exit:

	/* drive CS high */
	pcan_pci_spi_flash_exit(pcan_pci);
	if (err < 0)
		return err;

	/* wait for block erase (u-boot says 5s) */
	timeout = pcan_getnow_ns() + 5ULL * (1000 * 1000 * 1000);
	do {
		u8 s;

		err = pcan_pci_spi_flash_write_cmd(pcan_pci, JEDEC_RDSR, &s);
		if (err)
			return err;

		if (!(s & 1))
			return 0;

		/* relax */
		pcan_usleep(750);

	} while (pcan_getnow_ns() < timeout);

	pr_err(DEVICE_NAME ": %s: timeout waiting for block erase\n",
	       pcan_pci->adapter.name);

	return -ETIMEDOUT;
}

/*
 * program 256 bytes in flash device
 */
static int pcan_pci_spi_flash_byte_program(struct pcan_pci_adapter *pcan_pci,
					   u32 addr, void *data, int l)
{
	const u8 write_cmd = JEDEC_BYTE_PROGRAM;
	u64 timeout;
	int i, err;
	u8 *pb;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): addr=0x%xh l=%d\n",
		__func__, pcan_pci->adapter.name, addr, l);
#endif

	if (l > JEDEC_PAGE_SIZE)
		return -ERANGE;

	/* Write Enable (WREN) instruction */
	pcan_pci_spi_flash_write_cmd_enable(pcan_pci);

	/* drive CS low */
	err = pcan_pci_spi_flash_init(pcan_pci);
	if (err)
		return err;

	/* write erase cmd */
	err = pcan_pci_spi_write_data_reg(pcan_pci, write_cmd);
	if (err < 0)
		goto lbl_flash_exit;

	/* address */
	err = pcan_pci_spi_write_data_reg(pcan_pci, (u8 )((addr >> 16) & 0xff));
	if (err < 0)
		goto lbl_flash_exit;

	err = pcan_pci_spi_write_data_reg(pcan_pci, (u8 )((addr >> 8) & 0xff));
	if (err < 0)
		goto lbl_flash_exit;

	err = pcan_pci_spi_write_data_reg(pcan_pci, (u8 )((addr >> 0) & 0xff));
	if (err < 0)
		goto lbl_flash_exit;

	/* write bytes */
	pb = (u8 *)data;
	for (i = 0; i < l; i++) {
		err = pcan_pci_spi_write_data_reg(pcan_pci, *pb++);
		if (err < 0)
			break;
	}

lbl_flash_exit:

	/* drive CS high */
	pcan_pci_spi_flash_exit(pcan_pci);
	if (err < 0)
		return err;

	/* wait for block erase (u-boot says 2 s) */
	timeout = pcan_getnow_ns() + 2ULL * (1000 * 1000 * 1000);
	do {
		u8 s;

		err = pcan_pci_spi_flash_write_cmd(pcan_pci, JEDEC_RDSR, &s);
		if (err)
			return err;

		if (!(s & 1))
			return 0;

		/* relax */
		pcan_usleep(750);

	} while (pcan_getnow_ns() < timeout);

	pr_err(DEVICE_NAME ": %s: timeout waiting for byte program\n",
	       pcan_pci->adapter.name);

	return -ETIMEDOUT;
}

int pcan_pci_spi_flash_read(struct pcan_pci_adapter *pcan_pci, u32 addr,
			    void *data, int l)
{
	const u8 read_cmd = JEDEC_READ;
	u8 *pb = (u8 *)data;
	int err = -ERANGE;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s): addr=0x%xh l=%d\n",
		__func__, pcan_pci->adapter.name, addr, l);
#endif

	while (l > 0) {
		int i, lp = (l >= JEDEC_PAGE_SIZE) ? JEDEC_PAGE_SIZE : l;

		/* drive CS low */
		err = pcan_pci_spi_flash_init(pcan_pci);
		if (err)
			break;

		/* write read cmd */
		err = pcan_pci_spi_write_data_reg(pcan_pci, read_cmd);
		if (err < 0)
			goto lbl_flash_exit;

		/* address */
		err = pcan_pci_spi_write_data_reg(pcan_pci,
						  (u8 )((addr >> 16) & 0xff));
		if (err < 0)
			goto lbl_flash_exit;

		err = pcan_pci_spi_write_data_reg(pcan_pci,
						  (u8 )((addr >> 8) & 0xff));
		if (err < 0)
			goto lbl_flash_exit;

		err = pcan_pci_spi_write_data_reg(pcan_pci,
						  (u8 )((addr >> 0) & 0xff));
		if (err < 0)
			goto lbl_flash_exit;

		/* read bytes */
		for (err = i = 0; i < lp; i++) {
			err = pcan_pci_spi_write_data_reg(pcan_pci, 0xff);
			if (err < 0)
				break;

			*pb++ = err;
			err = 0;
		}

lbl_flash_exit:
		/* drive CS high */
		pcan_pci_spi_flash_exit(pcan_pci);
		if (err < 0)
			break;

		addr += lp;
		l -= lp;
	}

	return err;
}

int pcan_pci_spi_flash_write(struct pcan_pci_adapter *pcan_pci, u32 addr,
			     void *data, int l)
{
	int err;

#ifdef DEBUG_TRACE_FLASH
	pr_info(DEVICE_NAME ": %s(%s): addr=0x%xh\n",
		__func__, pcan_pci->adapter.name, addr);
	dump_mem("data to flash", data, l);
#endif

	/* this function erases only one block */
	if (l > 64*1024*1024)
		return -ERANGE;

	/* first, erase 64K block where data must be stored */
	err = pcan_pci_spi_flash_block_erase(pcan_pci, addr);
	if (err)
		return err;

	while (l > 0) {
		u8 flash_data[JEDEC_PAGE_SIZE];
		int lp = (l >= JEDEC_PAGE_SIZE) ? JEDEC_PAGE_SIZE : l;

		/* program 256 bytes */
		err = pcan_pci_spi_flash_byte_program(pcan_pci, addr, data, lp);
		if (err)
			break;

		/* read the programmed 256 bytes */
		err = pcan_pci_spi_flash_read(pcan_pci, addr, flash_data, lp);
		if (err)
			break;

#ifdef DEBUG_TRACE_FLASH
		dump_mem("data read from flash", flash_data, lp);
#endif

		/* verify the data */
		if (memcmp(data, flash_data, lp)) {
			pr_err(DEVICE_NAME
			       ": %s: flash memory content differs\n",
			       pcan_pci->adapter.name);
			err = -EINVAL;
			break;
		}

		addr += lp;
		data += lp;

		l -= lp;
	}

	return err;
}
