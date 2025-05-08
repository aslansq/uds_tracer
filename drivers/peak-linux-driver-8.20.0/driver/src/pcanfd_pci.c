/* SPDX-License-Identifier: GPL-2.0 */
/*
 * PEAK-System uCAN CAN-FD PCI adapters driver.
 *
 * Copyright (C) 2015-2020 PEAK System-Technik GmbH <www.peak-system.com>
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
/*#undef DEBUG*/

#include "src/pcan_common.h"
#include "src/pcan_main.h"
#include "src/pcan_pci.h"
#include "src/pcan_pci_spi.h"

#include "src/pcanfd_ucan.h"
#include "src/pcan_filter.h"

#ifdef NETDEV_SUPPORT
#include "src/pcan_netdev.h"
#endif

#ifdef DEBUG_PCIFD_LITE
#define DEBUG_IRQ
#define DEBUG_CMD
#define DEBUG_DMA
#define DEBUG_FREE_DMA
#ifdef DEBUG_PCIFD
#define DEBUG_WRITE
#define DEBUG_RXDMA		/* dumping Rx DMA area generates loss of IRQ */
#endif
#endif

#ifdef DEBUG_IRQ
#define DEBUG_IRQ_TX
#define DEBUG_IRQ_RX
#define DEBUG_IRQ_SPURIOUS
#define DEBUG_IRQ_LOST
#define DEBUG_CRITICAL
#endif

/* if defined, spurious TX IRQ (LNK=1) are caught: */
#define PCIEFD_CATCH_SPURIOUS_TX_IRQ

/* if defined, raw spinlock is used to ensure atomicity of the written cmd */
//#define PCAN_CMD64_USES_RAW_SPIN_LOCK

/* define the period in ms of a debug timer */
/* #define DEBUG_TIMER_PERIOD_MS		(1000) */

#define DRV_NAME			"pcanfd-pci"

#define PCIEFD_FLASH_DEVID_OFFSET	0x280000

#define PCIEFD_BAR0_SIZE		(64*1024)
#define PCIEFD_RX_DMA_SIZE		(4*1024)
#define PCIEFD_TX_DMA_SIZE		(4*1024)

#define PCIEFD_TX_PAGE_SIZE		(2*1024)

/* System Control Registers */
#define PCIEFD_REG_SYS_CTL_SET		0x0000	/* set bits */
#define PCIEFD_REG_SYS_CTL_CLR		0x0004	/* clear bits */

/* System Control Registers Bits */
#define PCIEFD_SYS_CTL_TS_RST		0x00000001
#define PCIEFD_SYS_CTL_CLK_EN		0x00000002

/* Version info registers */
#define PCIEFD_REG_VER1			0x0040
#define PCIEFD_REG_VER2			0x0044

/* uCAN core addresses */
#define PCIEFD_CANx_ADDR(i)		(((i)+1) * 0x1000)

/* uCAN core registers */
#define PCIEFD_REG_MISC			0x0000	/* Misc. control */
#define PCIEFD_REG_CLK_SEL		0x0008	/* Clock selector */
#define PCIEFD_REG_CMD_PORT_L		0x0010	/* 64-bits command port */
#define PCIEFD_REG_CMD_PORT_H		0x0014
#define PCIEFD_REG_TX_REQ_ACC		0x0020	/* Tx request accumulator */
#define PCIEFD_REG_TX_CTL_SET		0x0030	/* Tx control set register */
#define PCIEFD_REG_TX_CTL_CLR		0x0038	/* Tx control clear register */
#define PCIEFD_REG_TX_DMA_ADDR_L	0x0040	/* 64-bits addr for Tx DMA */
#define PCIEFD_REG_TX_DMA_ADDR_H	0x0044
#define PCIEFD_REG_RX_CTL_SET		0x0050	/* Rx control set register */
#define PCIEFD_REG_RX_CTL_CLR		0x0058	/* Rx control clear register */
#define PCIEFD_REG_RX_CTL_WRT		0x0060	/* Rx control write register */
#define PCIEFD_REG_RX_CTL_ACK		0x0068	/* Rx control ACK register */
#define PCIEFD_REG_RX_DMA_ADDR_L	0x0070	/* 64-bits addr for Rx DMA */
#define PCIEFD_REG_RX_DMA_ADDR_H	0x0074

/* uCAN misc register bits */
#define UCAN_MISC_TS_RST		0x00000001	/* timestamp cnt reset*/

/* uCAN core Clock SELector Source & DIVider */
#define PCIEFD_CLK_SEL_DIV_MASK		0x00000007
#define PCIEFD_CLK_SEL_DIV_60MHZ	0x00000000	/* SRC=240MHz only */
#define PCIEFD_CLK_SEL_DIV_40MHZ	0x00000001	/* SRC=240MHz only */
#define PCIEFD_CLK_SEL_DIV_30MHZ	0x00000002	/* SRC=240MHz only */
#define PCIEFD_CLK_SEL_DIV_24MHZ	0x00000003	/* SRC=240MHz only */
#define PCIEFD_CLK_SEL_DIV_20MHZ	0x00000004	/* SRC=240MHz only */

#define PCIEFD_CLK_SEL_SRC_MASK		0x00000008	/* 0=80MHz, 1=240MHz */
#define PCIEFD_CLK_SEL_SRC_240MHZ	0x00000008
#define PCIEFD_CLK_SEL_SRC_80MHZ	(~PCIEFD_CLK_SEL_SRC_240MHZ & \
					  PCIEFD_CLK_SEL_SRC_MASK)

#define PCIEFD_CLK_SEL_20MHZ		(PCIEFD_CLK_SEL_SRC_240MHZ|\
					 PCIEFD_CLK_SEL_DIV_20MHZ)
#define PCIEFD_CLK_SEL_24MHZ		(PCIEFD_CLK_SEL_SRC_240MHZ|\
					 PCIEFD_CLK_SEL_DIV_24MHZ)
#define PCIEFD_CLK_SEL_30MHZ		(PCIEFD_CLK_SEL_SRC_240MHZ|\
					 PCIEFD_CLK_SEL_DIV_30MHZ)
#define PCIEFD_CLK_SEL_40MHZ		(PCIEFD_CLK_SEL_SRC_240MHZ|\
					 PCIEFD_CLK_SEL_DIV_40MHZ)
#define PCIEFD_CLK_SEL_60MHZ		(PCIEFD_CLK_SEL_SRC_240MHZ|\
					 PCIEFD_CLK_SEL_DIV_60MHZ)
#define PCIEFD_CLK_SEL_80MHZ		(PCIEFD_CLK_SEL_SRC_80MHZ)

/* uCAN RX/Tx control register bits */
#define PCIEFD_CTL_UNC_BIT		0x00010000	/* Uncached DMA mem */
#define PCIEFD_CTL_RST_BIT		0x00020000	/* reset DMA action */
#define PCIEFD_CTL_IEN_BIT		0x00040000	/* IRQ enable */

/* default values for Count and Time Limit in Rx IRQ:
 * Count Limit is the maximum number of msgs saved in Rx DMA per IRQ, while
 * Time Limit is the delay (in 100 x µs) after which an IRQ is raised, even if
 * CL is not reached.
 *
 * CL  TL  Notes on 1xframe/ms CAN bus load (500k) on a single uCAN
 *  x   1  1 Rx msg
 *  5  10  2 (sometimes 3) Rx msgs BUT MSI sharing looses IRQ
 * 16  10  Windows driver values lost 1 irq at the beginning
 * 16  16  looses some irq on high bus load with shared MSIs
 */
#define PCIEFD_CTL_IRQ_CL_DEF	16	/* Rx msg max nb per IRQ in Rx DMA */
#define PCIEFD_CTL_IRQ_CL_MIN	1
#define PCIEFD_CTL_IRQ_CL_MAX	127	/* 7 bits */

#define PCIEFD_CTL_IRQ_TL_DEF	10	/* Time before IRQ if < CL (x 100 µs) */
#define PCIEFD_CTL_IRQ_TL_MIN	1
#define PCIEFD_CTL_IRQ_TL_MAX	15	/* 4 bits */

/* Tx anticipation window (link logical address should be aligned on 2K
 * boundary) */
#define PCIEFD_LNK_COUNT	(PCIEFD_TX_DMA_SIZE / PCIEFD_TX_PAGE_SIZE)

#define PCIEFD_MSG_LNK_TX	0x1001	/* Tx msgs link */

struct pcifd_tx_link {
	__le16		size;
	__le16		type;
	__le32		laddr_lo;
	__le32		laddr_hi;
} __attribute__((packed, aligned(4)));

struct pcifd_rx_dma {
	struct ucan_pci_irq_status irq_status;	/* 32-bits */
	__le32	sys_time_low;
	__le32	sys_time_high;
#ifdef CONFIG_UBSAN_BOUNDS_STRICT
	struct ucan_rx_msg msg[];
#else
	struct ucan_rx_msg msg[0];
#endif
} __attribute__((packed, aligned(4)));

extern int _pci_devices;

/* the PEAK uCAN PCI adapter object */
struct pcifd_adapter {
	struct pcan_pci_adapter pci;	/* always first element */
#ifdef PCAN_CMD64_USES_RAW_SPIN_LOCK
	raw_spinlock_t ucan_lock;
#else
	pcan_lock_t ucan_lock;
#endif
#ifdef PCAN_PCI_ENABLE_MSIX
	struct msix_entry msix_entries[4];
#endif

#ifdef DEBUG_IRQ_LOST
	unsigned int irq_pass;
	unsigned int irq_none;
	unsigned int lnk_set[4];
	unsigned int lnk_irq[4];
#endif

#ifdef CONFIG_UBSAN_BOUNDS_STRICT
	struct pcandev *devs[];
#else
	struct pcandev *devs[0];
#endif
};

#define pcifd_dev(u, i)		((u)->devs[i])

#ifdef PCAN_PCI_ENABLE_MSI
/* new v8.3: sharing MSI does not work in some circumstances */
static unsigned char fdusemsi = PCAN_PCI_USEMSI_DEFAULT;

module_param(fdusemsi, byte, 0644);
MODULE_PARM_DESC(fdusemsi, " 0=INTA; 1=MSI (not shared); 2=MSI (shared) (def="
			__stringify(PCAN_PCI_USEMSI_DEFAULT) ")");
#endif

static uint fdirqcl = PCIEFD_CTL_IRQ_CL_DEF;
module_param(fdirqcl, uint, 0644);
MODULE_PARM_DESC(fdirqcl, " PCIe FD IRQ Count Limit (default="
			__stringify(PCIEFD_CTL_IRQ_CL_DEF) ")");

static uint fdirqtl = PCIEFD_CTL_IRQ_TL_DEF;
module_param(fdirqtl, uint, 0644);
MODULE_PARM_DESC(fdirqtl, " PCIe FD IRQ Time Limit (default="
			__stringify(PCIEFD_CTL_IRQ_TL_DEF) ")");

static int pcifd_devices = 0;
static int pcifd_adapters = 0;

static void pcifd_hw_remove(struct pci_dev *dev);

#define ucan_dma_alloc(a, b, c, d)	dma_alloc_coherent(a, b, c, d)
#define ucan_dma_free(a, b, c, d)	dma_free_coherent(a, b, c, d)

static ssize_t show_pcan_rx_dma_vaddr(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	return snprintf(buf, PAGE_SIZE, "%p\n", pdev->port.pci.rx_dma_vaddr);
}

static ssize_t show_pcan_rx_dma_laddr(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	return snprintf(buf, PAGE_SIZE,
#ifdef CONFIG_64BIT
			"0x%llx\n",
#else
			"0x%lx\n", (long unsigned int)
#endif
			pdev->port.pci.rx_dma_laddr);
}

static ssize_t show_pcan_tx_dma_vaddr(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	return snprintf(buf, PAGE_SIZE, "%p\n", pdev->port.pci.tx_dma_vaddr);
}

static ssize_t show_pcan_tx_dma_laddr(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcandev *pdev = to_pcandev(dev);
	return snprintf(buf, PAGE_SIZE,
#ifdef CONFIG_64BIT
			"0x%llx\n",
#else
			"0x%lx\n", (long unsigned int)
#endif
			pdev->port.pci.tx_dma_laddr);
}

static PCAN_DEVICE_ATTR(rx_dma_vaddr, rx_dma_vaddr, show_pcan_rx_dma_vaddr);
static PCAN_DEVICE_ATTR(rx_dma_laddr, rx_dma_laddr, show_pcan_rx_dma_laddr);
static PCAN_DEVICE_ATTR(tx_dma_vaddr, tx_dma_vaddr, show_pcan_tx_dma_vaddr);
static PCAN_DEVICE_ATTR(tx_dma_laddr, tx_dma_laddr, show_pcan_tx_dma_laddr);

static struct attribute *pcan_dev_sysfs_pciefd_attrs[] = {
	&pcan_dev_attr_rx_dma_vaddr.attr,
	&pcan_dev_attr_rx_dma_laddr.attr,
	&pcan_dev_attr_tx_dma_vaddr.attr,
	&pcan_dev_attr_tx_dma_laddr.attr,
	NULL
};

/* Note: these functions have been imported from the Kernel in which they have
 * been added. Since they're used only here, they are defined as static.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
#define pcifd_dma_set_coherent_mask(d, m)	dma_set_coherent_mask(d, m)
#else
static inline int pcifd_dma_set_coherent_mask(struct device *dev, u64 mask)
{
	if (!dma_supported(dev, mask))
		return -EIO;
	dev->coherent_dma_mask = mask;
	return 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
#define pcifd_dma_set_mask_and_coherent(d, m)	dma_set_mask_and_coherent(d, m)
#else
static inline int pcifd_dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	int rc = dma_set_mask(dev, mask);
	if (!rc)
		pcifd_dma_set_coherent_mask(dev, mask);
	return rc;
}
#endif /* KERNEL_VERSION(3,13,0) */

#ifdef DEBUG_PCIFD
static void pcifd_sys_dump(struct pcifd_adapter *ucan_pci,
				const char *prompt, int offset, int len)
{
	char tmp[80];
	int i, l = 0;

	if (len <= 0)
		len = 0x4c;
	pr_info(DEVICE_NAME ": %s SYS block [%u..%u] @%p:\n",
		prompt ? prompt : "", offset, offset+len,
		ucan_pci->pci.bar0_addr);
	for (i = 0; i <= len; ) {
		if (!(i % 16))
			l = snprintf(tmp, sizeof(tmp), DEVICE_NAME ": %03x: ",
					offset + i);

		l += snprintf(tmp+l, sizeof(tmp)-l, "%02x ",
				readb(ucan_pci->pci.bar0_addr + offset + i));

		if (!(++i % 16))
			pr_info(DEVICE_NAME "%s\n", tmp);
	}
	if (i % 16)
		pr_info(DEVICE_NAME "%s\n", tmp);
}

static void pcifd_dev_dump(struct pcandev *dev, const char *prompt,
				int offset, int len)
{
	char tmp[80];
	int i, l = 0;

	if (len <= 0)
		len = 0x7c;
	pr_info(DEVICE_NAME ": %s uCAN%u block [%u..%u] @%p:\n",
		prompt ? prompt : "",
		dev->can_idx+1, offset, offset+len,
		dev->port.pci.can_port_addr);
	for (i = 0; i <= len; ) {
		if (!(i % 16))
			l = snprintf(tmp, sizeof(tmp), DEVICE_NAME ": %03x: ",
					offset + i);

		l += snprintf(tmp+l, sizeof(tmp)-l, "%02x ",
			readb(dev->port.pci.can_port_addr + offset + i));

		if (!(++i % 16))
			pr_info(DEVICE_NAME "%s\n", tmp);
	}
	if (i % 16)
		pr_info(DEVICE_NAME "%s\n", tmp);
}
#endif

#if defined(DEBUG_RXDMA) || defined(DEBUG_IRQ_RX)
static void pcifd_rx_dma_dump(struct pcandev *dev, const char *prompt,
				 int offset, int len)
{
	char tmp[80];
	int i, l = 0;

	if (!len)
		return;
	if (len < 0)
		len = 48; /* enough to get the channel */
	pr_info(DEVICE_NAME ": %s uCAN%u Rx DMA [%u..%u[ @%p:\n",
		prompt ? prompt : "",
		dev->can_idx+1, offset, offset+len,
		dev->port.pci.rx_dma_vaddr);
	for (i = l = 0; i < len; ) {
		if (!(i % 16))
			l = snprintf(tmp, sizeof(tmp), ": %03x - ", offset + i);

		l += snprintf(tmp+l, sizeof(tmp)-l, "%02x ",
			*(u8 *)(dev->port.pci.rx_dma_vaddr + offset + i));

		if (!(++i % 16))
			pr_info(DEVICE_NAME "%s\n", tmp);
	}
	if (i % 16)
		pr_info(DEVICE_NAME "%s\n", tmp);
}
#endif

#ifdef DEBUG_WRITE
static void pcifd_tx_dma_dump(struct pcandev *dev, const char *prompt,
				 int offset, int len)
{
	struct ucan_pci_page *page =
			dev->port.pci.tx_pages + dev->port.pci.tx_page_index;
	char tmp[80];
	int i, l = 0;

	if (!dev->port.pci.tx_pages) {
		pr_info(DEVICE_NAME ": %s(CAN%u): ABNORMAL NULL tx_pages!\n",
			__func__, dev->can_idx+1);
		return;
	}

	if (!len)
		return;
	if (len < 0)
		len = 128;
	pr_info(DEVICE_NAME ": %s uCAN%u Tx DMA page #%u (%p) [%u..%u[:\n",
		prompt ? prompt : "",
		dev->can_idx+1, dev->port.pci.tx_page_index, page->vbase,
		offset, offset+len);
	for (i = 0; i < len; ) {
		if (!(i % 16))
			l = snprintf(tmp, sizeof(tmp), ": %03x - ",
					offset + i);

		l += snprintf(tmp+l, sizeof(tmp)-l, "%02x ",
				*(u8 *)(page->vbase + offset + i));
		if (!(++i % 16))
			pr_info(DEVICE_NAME "%s\n", tmp);
	}
	if (i % 16)
		pr_info(DEVICE_NAME "%s\n", tmp);
}
#endif

/* read a 32 bits value from SYS block register */
static inline u32 ucan_sys_readl(const struct pcifd_adapter *ucan_pci,
				 u16 addr)
{
	return readl(ucan_pci->pci.bar0_addr + addr);
}

/* write a 32 bits value into a SYS block register */
static inline void ucan_sys_writel(const struct pcifd_adapter *ucan_pci,
				   u32 v, u16 addr)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(): %08x -> REG[%03x]\n", __func__, v, addr);
#endif
	writel(v, ucan_pci->pci.bar0_addr + addr);
}

/* read a 32 bits value from uCANx block register */
static inline u32 ucan_x_readl(const struct pcandev *dev, u16 addr)
{
	return readl(dev->port.pci.can_port_addr + addr);
}

/* write a 32 bits value into a uCANx block register */
static inline void ucan_x_writel(const struct pcandev *dev, u32 v, u16 addr)
{
#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(): %08x -> CAN%u_REG[%03x]\n",
		__func__, v, dev->can_idx+1, addr);
#endif
	writel(v, dev->port.pci.can_port_addr + addr);
}

/*
 * static int pcifd_send_cmd(struct pcandev *dev)
 */
static int pcifd_send_cmd(struct pcandev *dev)
{
	struct pcifd_adapter *ucan = pci_get_drvdata(dev->port.pci.pciDev);
	pcan_lock_irqsave_ctxt flags;

	/* to be sure that the 64-bits command is atomic */
#ifdef PCAN_CMD64_USES_RAW_SPIN_LOCK
	raw_spin_lock_irqsave(&ucan->ucan_lock, flags);
#else
	pcan_lock_get_irqsave(&ucan->ucan_lock, flags);
#endif

#ifdef DEBUG_CMD
	pr_info(DEVICE_NAME ": %s(CAN%u): %08x:%08x\n",
		__func__, dev->can_idx+1,
		le32_to_cpu(*(u32 *)dev->ucan.cmd_head),
		le32_to_cpu(*(u32 *)(dev->ucan.cmd_head + 4)));
#endif

	writel(*(u32 *)dev->ucan.cmd_head,
			dev->port.pci.can_port_addr + PCIEFD_REG_CMD_PORT_L);
	writel(*(u32 *)(dev->ucan.cmd_head + 4),
			dev->port.pci.can_port_addr + PCIEFD_REG_CMD_PORT_H);

#ifdef PCAN_CMD64_USES_RAW_SPIN_LOCK
	raw_spin_unlock_irqrestore(&ucan->ucan_lock, flags);
#else
	pcan_lock_put_irqrestore(&ucan->ucan_lock, flags);
#endif

	return 0;
}

/*
 * static void pcifd_wait_for_eot(struct pcandev *dev)
 */
static void pcifd_wait_for_eot(struct pcandev *dev)
{
	int i;
	struct pcifd_adapter *ucan_pci =
				pci_get_drvdata(dev->port.pci.pciDev);

	/* make several dummy reads */
	for (i = 0; i < 3; i++)
		ucan_sys_readl(ucan_pci, PCIEFD_REG_VER1);
}

/*
 * static void pcifd_dma_free(struct pcandev *dev)
 */
static void pcifd_dma_free_rx(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif
	if (dev->port.pci.rx_dma_vaddr) {

#ifdef DEBUG_FREE_DMA
		pr_info(DEVICE_NAME ": CAN%u Rx DMA=%p (0x%llx) free\n",
			dev->can_idx+1,
			dev->port.pci.rx_dma_vaddr,
			dev->port.pci.rx_dma_laddr);
#endif
		ucan_dma_free(&dev->port.pci.pciDev->dev,
				  PCIEFD_RX_DMA_SIZE,
				  dev->port.pci.rx_dma_vaddr,
				  dev->port.pci.rx_dma_laddr);

		dev->port.pci.rx_dma_vaddr = NULL;
	}
}

static void pcifd_dma_free_tx(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif
	/* just to be sure, not confident in pcan core... */
	if (dev->port.pci.tx_pages) {
		pcan_free(dev->port.pci.tx_pages);
		dev->port.pci.tx_pages = NULL;
	}

	if (dev->port.pci.tx_dma_vaddr) {

#ifdef DEBUG_FREE_DMA
		pr_info(DEVICE_NAME ": CAN%u Tx DMA=%p (0x%llx) free\n",
			dev->can_idx+1,
			dev->port.pci.tx_dma_vaddr,
			dev->port.pci.tx_dma_laddr);
#endif
		ucan_dma_free(&dev->port.pci.pciDev->dev,
				  PCIEFD_TX_DMA_SIZE,
				  dev->port.pci.tx_dma_vaddr,
				  dev->port.pci.tx_dma_laddr);

		dev->port.pci.tx_dma_vaddr = NULL;
	}
}

static void pcifd_dma_free(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif
	pcifd_dma_free_tx(dev);
	pcifd_dma_free_rx(dev);
}

/*
 * static int pcifd_dma_alloc(struct pcandev *dev)
 */
static int pcifd_dma_alloc_rx(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif
	/* allocate non-cacheable DMA'able 4KB memory area for Rx */
	dev->port.pci.rx_dma_vaddr =
		ucan_dma_alloc(&dev->port.pci.pciDev->dev,
				   PCIEFD_RX_DMA_SIZE,
				   &dev->port.pci.rx_dma_laddr,
				   GFP_KERNEL);
	if (!dev->port.pci.rx_dma_vaddr) {
		dev_err(&dev->port.pci.pciDev->dev,
				"%s: Rx dma_alloc_coherent(%u) failure\n",
				DEVICE_NAME, PCIEFD_RX_DMA_SIZE);
		goto failed;
	}

#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": CAN%u Rx DMA=%p (%llx)\n",
		dev->can_idx+1, dev->port.pci.rx_dma_vaddr,
		dev->port.pci.rx_dma_laddr);
#endif
	return 0;

failed:
	return -ENOMEM;
}

static int pcifd_dma_alloc_tx(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif
	/*
	 * TODO: is it useful to alloc DMA Tx descriptors if LISTEN_ONLY mode?
	 */

	/* allocate non-cacheable DMA'able 4KB memory area for Tx */
	dev->port.pci.tx_dma_vaddr =
		ucan_dma_alloc(&dev->port.pci.pciDev->dev,
				   PCIEFD_TX_DMA_SIZE,
				   &dev->port.pci.tx_dma_laddr,
				   GFP_KERNEL);
	if (!dev->port.pci.tx_dma_vaddr) {
		dev_err(&dev->port.pci.pciDev->dev,
				"%s: Tx dma_alloc_coherent(%u) failure\n",
				DEVICE_NAME, PCIEFD_TX_DMA_SIZE);

		dev->port.pci.tx_pages = NULL;
		goto failed;
	}

	/* alloc Tx pages descriptors list */
	dev->port.pci.tx_pages = pcan_malloc(sizeof(struct ucan_pci_page) *
						PCIEFD_LNK_COUNT, GFP_KERNEL);
	if (!dev->port.pci.tx_pages) {
		dev_err(&dev->port.pci.pciDev->dev,
				"%s: failed to alloc %u pages table\n",
				DEVICE_NAME, PCIEFD_LNK_COUNT);
		goto free_tx_dma;
	}

#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": CAN%u Tx DMA=%p (%llx)\n",
		dev->can_idx+1, dev->port.pci.tx_dma_vaddr,
		dev->port.pci.tx_dma_laddr);
#endif

	return 0;

free_tx_dma:
	ucan_dma_free(&dev->port.pci.pciDev->dev,
				  PCIEFD_TX_DMA_SIZE,
				  dev->port.pci.tx_dma_vaddr,
				  dev->port.pci.tx_dma_laddr);
	dev->port.pci.tx_dma_vaddr = NULL;

failed:
	return -ENOMEM;
}

static void pcifd_dma_init_tx(struct pcandev *dev)
{
	int i;

#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	/* protect from abusive calls */
	if (!dev->port.pci.tx_pages) {
		pr_info(DEVICE_NAME ": %s(CAN%u): ABNORMAL NULL tx_pages!\n",
			__func__, dev->can_idx+1);
		return;
	}

	dev->wCANStatus &= ~CAN_ERR_QXMTFULL;
	dev->port.pci.tx_pages_free = PCIEFD_LNK_COUNT - 1;
	dev->port.pci.tx_page_index = 0;

	dev->port.pci.tx_pages[0].vbase = dev->port.pci.tx_dma_vaddr;
	dev->port.pci.tx_pages[0].lbase = dev->port.pci.tx_dma_laddr;

	for (i = 0; i < PCIEFD_LNK_COUNT; i++) {
		dev->port.pci.tx_pages[i].offset = 0;
		dev->port.pci.tx_pages[i].size = PCIEFD_TX_PAGE_SIZE -
					sizeof(struct pcifd_tx_link);
		if (i) {
			dev->port.pci.tx_pages[i].vbase =
				dev->port.pci.tx_pages[i-1].vbase +
							PCIEFD_TX_PAGE_SIZE;
			dev->port.pci.tx_pages[i].lbase =
				dev->port.pci.tx_pages[i-1].lbase +
							PCIEFD_TX_PAGE_SIZE;
		}
	}
}

static int pcifd_dma_alloc(struct pcandev *dev)
{
	int err;

#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif
	err = pcifd_dma_alloc_rx(dev);
	if (err)
		return err;

	err = pcifd_dma_alloc_tx(dev);
	if (err) {
		pcifd_dma_free_rx(dev);
		goto failed;
	}

failed:
	return err;
}

static void pcifd_dma_clr_rx(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	/* Be sure that DMA is reset for Rx */
	ucan_x_writel(dev, PCIEFD_CTL_RST_BIT, PCIEFD_REG_RX_CTL_SET);

	/* reset the logical address to the DMA engine for this uCAN */
	ucan_x_writel(dev, 0, PCIEFD_REG_RX_DMA_ADDR_L);
	ucan_x_writel(dev, 0, PCIEFD_REG_RX_DMA_ADDR_H);
}

static void pcifd_dma_clr_tx(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	/* stop (TX_RST=1) DMA Tx engine */
	ucan_x_writel(dev, PCIEFD_CTL_RST_BIT, PCIEFD_REG_TX_CTL_SET);

	/* reset the logical address to the DMA engine for this uCAN */
	ucan_x_writel(dev, 0, PCIEFD_REG_TX_DMA_ADDR_L);
	ucan_x_writel(dev, 0, PCIEFD_REG_TX_DMA_ADDR_H);
}

static void pcifd_dma_set_rx(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	/* Be sure that DMA is reset for Rx */
	ucan_x_writel(dev, PCIEFD_CTL_RST_BIT, PCIEFD_REG_RX_CTL_SET);

	/* write the logical address to the DMA engine for this uCAN */
	ucan_x_writel(dev, (u32 )dev->port.pci.rx_dma_laddr,
						PCIEFD_REG_RX_DMA_ADDR_L);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	ucan_x_writel(dev, (u32 )(dev->port.pci.rx_dma_laddr >> 32),
						PCIEFD_REG_RX_DMA_ADDR_H);
#else
	ucan_x_writel(dev, 0, PCIEFD_REG_RX_DMA_ADDR_H);
#endif

	/* the DMA memory is cacheable */
	ucan_x_writel(dev, PCIEFD_CTL_UNC_BIT, PCIEFD_REG_RX_CTL_CLR);
}

static void pcifd_dma_set_tx(struct pcandev *dev)
{
#ifdef DEBUG_DMA
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	/* stop (TX_RST=1) DMA Tx engine */
	ucan_x_writel(dev, PCIEFD_CTL_RST_BIT, PCIEFD_REG_TX_CTL_SET);

	/* write the logical address to the DMA engine for this uCAN */
	ucan_x_writel(dev, (u32 )dev->port.pci.tx_dma_laddr,
						PCIEFD_REG_TX_DMA_ADDR_L);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	ucan_x_writel(dev, (u32 )(dev->port.pci.tx_dma_laddr >> 32),
						PCIEFD_REG_TX_DMA_ADDR_H);
#else
	ucan_x_writel(dev, 0, PCIEFD_REG_TX_DMA_ADDR_H);
#endif

	/* the DMA memory is cacheable */
	ucan_x_writel(dev, PCIEFD_CTL_UNC_BIT, PCIEFD_REG_TX_CTL_CLR);
}

static void display_dev_stats(struct pcifd_adapter *ua, struct pcandev *dev)
{
	pr_info(DEVICE_NAME
		": %s CAN%u tx_eng=%u dev_tag=%u hw_tag=%u rx_cnt=%u lnk=%u "
		"tx_irq=%u rx_irq=%u tx=%u rx=%u "
#ifdef DEBUG_IRQ_LOST
		"lnk_set=%u lnk_irq=%u"
#endif
		"\n",
		dev->adapter->name, dev->can_idx+1,
		dev->locked_tx_engine_state,
		dev->port.pci.irq_tag, dev->port.pci.irq_status.irq_tag,
		dev->port.pci.irq_status.rx_cnt, dev->port.pci.irq_status.lnk,
		dev->tx_irq_counter, dev->rx_irq_counter,
		dev->tx_frames_counter, dev->rx_frames_counter
#ifdef DEBUG_IRQ_LOST
		, ua->lnk_set[dev->can_idx], ua->lnk_irq[dev->can_idx]
#endif
		);
}

static void display_stats(struct pcifd_adapter *ua)
{
	int i;

	for (i = 0; i < ua->pci.adapter.can_count; i++) {
		struct pcandev *dev = pcifd_dev(ua, i);

		if (dev->flags & PCAN_DEV_OPENED)
			display_dev_stats(ua, dev);
	}
}

/*
 * static int __pcifd_device_write(struct pcandev *dev,
 *				   struct pcanusr *ctx)
 */
static int __pcifd_device_write(struct pcandev *dev, struct pcanusr *ctx)
{
	PCI_PORT *pdpci = &dev->port.pci;
	struct ucan_pci_page *page = pdpci->tx_pages +
						dev->port.pci.tx_page_index;
	struct pcifd_tx_link *lk = NULL;
	int err;
#ifdef DEBUG_WRITE
	int len = 0, frc = 0;
	int offset = page->offset;
#endif

	if (!dev->port.pci.tx_pages) {
		pr_err(DEVICE_NAME ": %s(CAN%u): ABNORMAL NULL tx_pages!\n",
		       __func__, dev->can_idx+1);
		return -EINVAL;
	}

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u): head=%p end=%p (left=%u fifo=%u)\n",
		__func__, dev->can_idx+1,
		page->vbase + page->offset, page->vbase + page->size,
		page->size - page->offset,
		pcan_fifo_items_count(&dev->tx_fifo));
#endif

	/* write as many messages as contained in the pcandev Tx FIFO */
	while (1) {

		err = ucan_encode_msg(dev, page->vbase + page->offset,
						page->size - page->offset);
		if (err < 0) {

			/* if not enough space in the current page, change it */
			if (err != -ENOSPC)
				break;

			/* no more page free ? */
			if (!pdpci->tx_pages_free) {
				err = 0;
				break;
			}

			pdpci->tx_pages_free--;

			lk = page->vbase + page->offset;

#ifdef DEBUG_WRITE
			/* dump all that has been copied before inserting
			 * link
			 */
			pcifd_tx_dma_dump(dev, __func__, offset, len);
			offset = 0;
			len = 0;
#endif

			/* retry in some other page */
			pdpci->tx_page_index = 1 - pdpci->tx_page_index;
			page = pdpci->tx_pages + pdpci->tx_page_index;

			/* put the link at the end of old page */
			lk->size = cpu_to_le16(sizeof(struct pcifd_tx_link));
			lk->type = cpu_to_le16(PCIEFD_MSG_LNK_TX);
			lk->laddr_lo = cpu_to_le32(page->lbase);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			lk->laddr_hi = cpu_to_le32(page->lbase >> 32);
#else
			lk->laddr_hi = 0;
#endif
			/* next msgs will be put from the begininng */
			page->offset = 0;

			/* sure we'll get a Tx IRQ for this! */

#ifdef DEBUG_IRQ_LOST
			((struct pcifd_adapter *)dev->adapter)->lnk_set[dev->can_idx]++;
#endif
#ifdef DEBUG_IRQ_TX
			pr_info(DEVICE_NAME ": CAN%u TX engine: STARTED\n",
				dev->can_idx+1);
#endif
			pcan_set_tx_engine(dev, TX_ENGINE_STARTED);
			continue;
		}

		/* always true: if (err > 0) { */
		{
#ifdef DEBUG_WRITE
			frc++;
			len += err;
#endif
#ifdef DEBUG_PCIFD
			pr_info(DEVICE_NAME ": %s(CAN%u): %d bytes written\n",
				__func__, dev->can_idx+1, err);
#endif
			page->offset += err;
			dev->tx_frames_counter++;

			pcan_clear_status_bit(dev, CAN_ERR_QXMTFULL);

			/* be sure that the whole record is written before
			 * telling the device that it is
			 */
			wmb();

			/* tell the device that one message has been written */
			ucan_x_writel(dev, 1, PCIEFD_REG_TX_REQ_ACC);

#ifdef UCAN_MSG_CAN_TX_PAUSE
			/* tell the device that one more message has been
			 * written if a TX_PAUSE record has been inserted.
			 */
			if (dev->tx_iframe_delay_us)
				ucan_x_writel(dev, 1, PCIEFD_REG_TX_REQ_ACC);
#endif
		}
	}

	/* no LNK inserted => no Tx IRQ => tell user to send by himself */
	if (!lk) {
#ifdef DEBUG_IRQ_TX
		if (dev->locked_tx_engine_state != TX_ENGINE_STOPPED)
			pr_info(DEVICE_NAME ": CAN%u TX engine: STOPPED\n",
				dev->can_idx+1);
#endif
		pcan_set_tx_engine(dev, TX_ENGINE_STOPPED);
	}

#ifdef DEBUG_WRITE
	/* if at least ONE message has been written, say it! */
	if (frc > 0) {
		pr_info(DEVICE_NAME
			": %u messages written (%u total, err %d):\n",
			frc, dev->tx_frames_counter, err);
		pcifd_tx_dma_dump(dev, __func__, offset, len);
	} else {
		pr_info(DEVICE_NAME
			": no new message written (err %d)\n",
			err);
	}
#endif

	return err;
}

/*
 * ACK hardware to re-enable DMA
 */
static void pcifd_dma_ack(struct pcandev *dev)
{
	/* give a tag which value is not the value of 1st u32 in DMA area */
	dev->port.pci.irq_tag =
			le32_to_cpu(*(__le32 *)dev->port.pci.rx_dma_vaddr);

	dev->port.pci.irq_tag++;
	dev->port.pci.irq_tag &= 0xf;

	ucan_x_writel(dev, dev->port.pci.irq_tag, PCIEFD_REG_RX_CTL_ACK);
}

/*
 * static irqreturn_t pcifd_irq_handler(int irq, void *arg)
 * 
 * the hardware part of the IRQ: ack as fast as possibel what blocks hw.
 */
static irqreturn_t pcifd_irq_handler(int irq, void *arg)
{
	struct pcandev *dev = (struct pcandev *)arg;
	struct pcifd_rx_dma *rx_dma =
			(struct pcifd_rx_dma *)dev->port.pci.rx_dma_vaddr;
#ifdef DEBUG_IRQ_LOST
	struct pcifd_adapter *ua = pci_get_drvdata(dev->port.pci.pciDev);
#endif
	pcan_lock_irqsave_ctxt flags;
	int err, n;

	/* INTA mode only: dummy read to finalize PCIe transactions
	 * (only if MSI aren't shared?)
	 */
#ifdef PCAN_PCI_ENABLE_MSI
	if (!pci_dev_msi_enabled(dev->port.pci.pciDev))
#endif
		(void )ucan_sys_readl(pci_get_drvdata(dev->port.pci.pciDev),
					PCIEFD_REG_VER1);

	/* read the irq status from the 1st 32-bits of the Rx DMA area */
	dev->port.pci.irq_status = *(struct ucan_pci_irq_status *)rx_dma;

#ifdef DEBUG_IRQ_LOST
	ua->irq_pass++;
#endif
	/* check if IRQ is for me: if the IRQ tag read in my Rx DMA area does
	 * match the one I'm waiting for, the IRQ does concern me! 
	 * This test is mandatory in a context of this IRQ is shared among other
	 * (non-CAN) devices.
	 */
	if (dev->port.pci.irq_status.irq_tag != dev->port.pci.irq_tag) {
#ifdef DEBUG_IRQ
		if (!dev->port.pci.irq_not_for_me) {
			pr_warn(DEVICE_NAME
				": IRQ%u not for CAN%u (irq_tag=%xh while "
				"DMA is %xh)\n",
				dev->wIrq,
				dev->can_idx+1,
				dev->port.pci.irq_tag,
				dev->port.pci.irq_status.irq_tag);
		}
		dev->port.pci.irq_not_for_me++;
#endif

		goto irq_handler_exit;
	}

#if defined(DEBUG_IRQ_RX)
	if (dev->port.pci.irq_status.rx_cnt)
		pr_info(DEVICE_NAME ": %s(CAN%u): irq_tag=%02xh rx_cnt=%u\n",
			__func__, dev->can_idx+1,
			dev->port.pci.irq_status.irq_tag,
			dev->port.pci.irq_status.rx_cnt);
#endif
#if defined(DEBUG_IRQ_TX)
	if (dev->port.pci.irq_status.lnk)
		pr_info(DEVICE_NAME ": %s(CAN%u): irq_tag=%02xh lnk=%u\n",
			__func__, dev->can_idx+1,
			dev->port.pci.irq_status.irq_tag,
			dev->port.pci.irq_status.lnk);
#endif

	if (!dev->port.pci.irq_status.rx_cnt && !dev->port.pci.irq_status.lnk) {
#ifdef DEBUG_IRQ_SPURIOUS
		pr_warn(DEVICE_NAME
			": %s(CAN%u) got spurious IRQ%d (rx_cnt=%u lnk=%u)\n",
			dev->adapter->name,
			dev->can_idx+1, irq,
			dev->port.pci.irq_status.rx_cnt,
			dev->port.pci.irq_status.lnk);
#endif
		goto irq_handler_exit;
	}

#ifndef NETDEV_SUPPORT
	/* keep adapter time for further sync */
	pcan_sync_times(dev, le32_to_cpu(rx_dma->sys_time_low),
				le32_to_cpu(rx_dma->sys_time_high), 0);
#endif

#if defined(DEBUG_RXDMA) || defined(DEBUG_IRQ_RX)
	/* dump Rx DMA area before handling its content */
	pcifd_rx_dma_dump(dev, __func__, 0, -1);
#endif

	/* handle all the incoming messages (if any) */
	n = dev->port.pci.irq_status.rx_cnt;
	ucan_handle_msgs_list(&dev->ucan, rx_dma->msg, &n);

	/* if some CAN messages were pushed into Rx queue, wake up any
	 * "waiting-for-read" task
	 */
	if (n > 0) {
		dev->rx_irq_counter++;

#ifdef DEBUG_IRQ_RX
		pr_info(DEVICE_NAME
			": CAN%u rx_cnt=%u n=%d signaling reading task...\n",
			dev->can_idx+1, dev->port.pci.irq_status.rx_cnt, n);
#endif
#ifndef NETDEV_SUPPORT
		pcan_event_signal(&dev->in_event);
#endif
	}

	/* in case of Tx "page" complete IRQ, restart flushing tx fifo and
	 * wake up any "waiting-for-write" task in case of empty fifo
	 */
	if (dev->port.pci.irq_status.lnk) {

		dev->tx_irq_counter++;

		err = 0;

#ifdef DEBUG_IRQ_LOST
		((struct pcifd_adapter *)dev->adapter)->lnk_irq[dev->can_idx]++;
#endif

		pcan_lock_get_irqsave(&dev->isr_lock, flags);

		if (dev->locked_tx_engine_state == TX_ENGINE_STARTED) {
			dev->port.pci.tx_pages_free++;

			pcan_clear_status_bit(dev, CAN_ERR_QXMTFULL);

			/* restart flushing output queue */
			err = __pcifd_device_write(dev, NULL);
		}

#ifdef PCIEFD_CATCH_SPURIOUS_TX_IRQ
		/* WTF? Got a LNK=1 IRQ but TX engine is not STARTED? */
		else if (dev->locked_tx_engine_state != TX_ENGINE_CLOSED) {
			pr_warn(DEVICE_NAME
				": CAN%u Tx engine !STARTED(%u) but lnk=%u\n",
				dev->can_idx+1,
				dev->locked_tx_engine_state,
				dev->port.pci.irq_status.lnk);

			pr_warn(DEVICE_NAME
				": device=%s IRQ=%d [MSI=%c SHARED=%c] "
				"tx_pages_free=%d\n",
				dev->adapter->name, dev->wIrq,
				pci_dev_msi_enabled(dev->port.pci.pciDev) ?
								'y' : 'n',
				(dev->flags & PCAN_DEV_MSI_SHARED) ? 'y' : 'n',
				dev->port.pci.tx_pages_free);

			display_stats((struct pcifd_adapter *)dev->adapter);

			/* ... but it looks like it's not enough: when this case
			 * occurs, the driver never receives any other IRQ!
			 *
			 * Note that this situation occurs when:
			 *
			 * This is quite easy to reproduce with pcanview when
			 * running with BRP/TSEGx that are different between
			 * Windows and Linux versions, for bor both bitrates:
			 *
			 * 500k+2Mbps on each side, but:
			 * BRP/TSEG1/TSEG2	Windows		Linux
			 * 500kbps		10/12/3		2/64/15
			 * 2Mbps		4/7/2		4/7/2
			 *
			 * (end sending BRS frames from both sides, resetting
			 * when bus goes to PASSIVE...
			 */
		}
#endif /* PCIEFD_CATCH_SPURIOUS_TX_IRQ */

		pcan_lock_put_irqrestore(&dev->isr_lock, flags);

		/* err == 0 => no more free page in Tx DMA area */
		if (err) {
#ifdef DEBUG_IRQ_TX
			pr_info(DEVICE_NAME
				": CAN%u lnk=%u signaling writing task...\n",
				dev->can_idx+1, dev->port.pci.irq_status.lnk);
#endif
			pcan_event_signal(&dev->out_event);
#ifdef NETDEV_SUPPORT
			if (dev->netdev)
				netif_wake_queue(dev->netdev);
#endif
		}
	}

	/* re-enable DMA transfer for this uCAN */
	pcifd_dma_ack(dev);

	return PCAN_IRQ_HANDLED;

irq_handler_exit:
#ifdef DEBUG_IRQ_LOST
	ua->irq_none++;

#ifdef PCAN_PCI_ENABLE_MSI
	/* it makes no sense to debug shared legacy IRQ... */
	if ((fdusemsi == PCAN_PCI_USEMSI_SHARED) &&

			/* use '==' to avoid reentrance */
			(ua->irq_pass == ua->pci.adapter.opened_count)) {

		/* if IRQ has not be handled at all, display it */
		if (ua->irq_none >= ua->pci.adapter.opened_count) {
			pr_info(DEVICE_NAME
				": unhandled IRQ (%u = %uxCANs)\n",
				ua->irq_none,
				ua->pci.adapter.opened_count);

			display_stats(ua);
		}

		ua->irq_pass = 0;
		ua->irq_none = 0;
	}
#endif
#endif

	return PCAN_IRQ_NONE;
}

#ifndef NO_RT
/* RT version of the IRQ handler */
static int pcifd_irq_handler_rt(rtdm_irq_t *irq_context)
{
	struct pcandev *dev = rtdm_irq_get_arg(irq_context, struct pcandev);

	return pcifd_irq_handler(dev->wIrq, dev);
}
#endif

#ifdef DEBUG_TIMER_PERIOD_MS
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
static void pcifd_polling_timer(unsigned long arg)
{
	struct pcandev *dev = (struct pcandev *)arg;
#else
static void pcifd_polling_timer(struct timer_list *t)
{
	struct pcandev *dev = from_timer(dev, t, polling_timer);
#endif
	pcifd_dev_dump(dev, __func__, 0, -1);

	mod_timer(&dev->polling_timer,
			jiffies + msecs_to_jiffies(DEBUG_TIMER_PERIOD_MS));
}
#endif

/*
 * static void pcifd_device_release(struct pcandev *dev)
 *
 * This callabck is called by:
 * 
 * 1 - ioctl(SET_INIT) if the device was opened before.
 * 2 - close() as the 1st callback called.
 *
 * Difference between both is that, in case 2), nOpenPath = 0.
 */
static void pcifd_device_release(struct pcandev *dev)
{
#if defined(DEBUG_TRACE)
	pr_err(DEVICE_NAME
		": %s(CAN%u) dmarx: 0x%x latched: 0x%x expected: 0x%x\n",
		__func__, dev->can_idx+1,
		*(u32 *)dev->port.pci.rx_dma_vaddr,
		*(u32 *)&dev->port.pci.irq_status,
		dev->port.pci.irq_tag);
#endif
#if defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	pcifd_rx_dma_dump(dev, __func__, 0, -1);
#endif

	/* Note: DON't ucan_tx_abort() when Tx DMA NOT in RST mode! */

	/* bus off */
	ucan_set_bus_off(dev);

	/* Note: v8.16 uses ucan_device_close() to clear options set */
	ucan_device_close(dev);

	/* since INIT sets Tx path, device_release() MUST reset it: */

	/* stop (TX_RST=1) DMA Tx engine */
	ucan_x_writel(dev, PCIEFD_CTL_RST_BIT, PCIEFD_REG_TX_CTL_SET);

	/* Tx abort with Tx DMA NOT in RST mode is dangerous (reboot needed) */
	ucan_tx_abort(dev, UCAN_TX_ABORT_FLUSH);
}

/*
 * static int pcifd_driver_release(struct pcandev *dev)
 *
 * Second callback called by close().
 *
 * Note that IRQ are always ON.
 */
static int pcifd_driver_release(struct pcandev *dev)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	return 0;
}

/*
 * static void pcifd_driver_free_irq(struct pcandev *dev)
 *
 * Third (and last) callback called by close().
 */
static void pcifd_driver_free_irq(struct pcandev *dev,
					struct pcanusr *ctx)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	/* disable IRQ for this uCAN */
	ucan_x_writel(dev, PCIEFD_CTL_IEN_BIT, PCIEFD_REG_RX_CTL_CLR);

	pcifd_dma_clr_rx(dev);
	pcifd_dma_clr_tx(dev);

	pcifd_wait_for_eot(dev);

#ifdef DEBUG_IRQ_LOST
	{
		struct pcifd_adapter *ua = (struct pcifd_adapter *)dev->adapter;

		ua->lnk_irq[dev->can_idx] = 0;
		ua->lnk_set[dev->can_idx] = 0;
	}
#endif

#ifdef DEBUG_TIMER_PERIOD_MS
	del_timer_sync(&dev->polling_timer);
#endif

#ifdef PCAN_PCI_MSI_WORKAROUND
	if (fdusemsi == PCAN_PCI_USEMSI_INTA)
#endif
		pcan_free_irq(dev);
}

static int pcifd_enable_tx_path(struct pcandev *dev)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	if (dev->bus_state != PCANFD_ERROR_BUSOFF) {

		/* reinit pages settings (this must be atomic) */
		pcifd_dma_init_tx(dev);

		/* start (TX_RST=0) Tx Path */
		ucan_x_writel(dev, PCIEFD_CTL_RST_BIT, PCIEFD_REG_TX_CTL_CLR);
	}

	/* write can now occur */
	pcan_set_tx_engine(dev, TX_ENGINE_STOPPED);

	return 0;
}

/*
 * static int pcifd_driver_open(struct pcandev *dev)
 *
 * 1st callback called  by pcan_open_path(),
 * before req_irq() (pcifd_driver_req_irq()).
 */
static int pcifd_driver_open(struct pcandev *dev)
{
	int err = 0;

#if defined(DEBUG_TRACE)
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif
	/* Note: the following commands MUST be done in RESET mode */
	if (dev->flags & PCAN_DEV_BUS_ON)
		ucan_set_bus_off(dev);

#ifndef NETDEV_SUPPORT
	/* to force to init sync objects */
	pcan_sync_init(dev);
#endif

	/* reset timestamps */
	ucan_x_writel(dev, 0, PCIEFD_REG_MISC);

	pcifd_dma_set_rx(dev);
	pcifd_dma_set_tx(dev);

	return err;
}

/*
 * static int pcifd_driver_req_irq(struct pcandev *dev)
 *
 * 2nd calbacks called by pcan_open_path(),
 * after open() (pcifd_driver_open())
 */
static int __pcifd_driver_req_irq(struct pcandev *dev)
{
	int err, irq_flags = PCAN_IRQF_SHARED;

#ifdef PCAN_PCI_ENABLE_MSI
	/* if driver got the requested count of MSI, IRQ is not shared */
	if (pci_dev_msi_enabled(dev->port.pci.pciDev))
		if (!(dev->flags & PCAN_DEV_MSI_SHARED))
			irq_flags &= ~PCAN_IRQF_SHARED;
#endif /* PCAN_PCI_ENABLE_MSI */

#if defined(DEBUG_TRACE) || defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	pr_info(DEVICE_NAME ": %s(CAN%u) irq_flags=%0xh\n",
		__func__, dev->can_idx+1, irq_flags);
#endif

#ifndef NO_RT
	irq_flags |= RTDM_IRQTYPE_EDGE;

	/* RT irq requesting */
	err = rtdm_irq_request(&dev->irq_handle,
			dev->wIrq,
			pcifd_irq_handler_rt,
#else
	/* using legacy interrupt mechanism */
	err = request_irq(dev->wIrq,
			pcifd_irq_handler,
#endif
			irq_flags,
			DEVICE_NAME,
			dev);

	if (err)
		pr_err(DEVICE_NAME ": %s(CAN%u): failed to request irq %d "
			"flags=%0xh (err %d)\n",
			dev->adapter->name, dev->can_idx+1,
			dev->wIrq, irq_flags, err);

	return err;
}

static int pcifd_driver_req_irq(struct pcandev *dev, struct pcanusr *ctx)
{
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

#ifdef PCAN_PCI_MSI_WORKAROUND
	if (fdusemsi == PCAN_PCI_USEMSI_INTA)
#endif
		if ((err = __pcifd_driver_req_irq(dev)))
			return err;

#ifdef DEBUG_TIMER_PERIOD_MS
	pcan_setup_timer(&dev->polling_timer, pcifd_polling_timer,
			                    (unsigned long )dev);
	mod_timer(&dev->polling_timer,
			jiffies + msecs_to_jiffies(DEBUG_TIMER_PERIOD_MS));
#endif

#ifdef DEBUG_IRQ_LOST
	{
		struct pcifd_adapter *ua = (struct pcifd_adapter *)dev->adapter;

		ua->lnk_irq[dev->can_idx] = 0;
		ua->lnk_set[dev->can_idx] = 0;
	}
#endif

	/* write max count of msgs per IRQ */
	ucan_x_writel(dev, (fdirqtl) << 8 | fdirqcl, PCIEFD_REG_RX_CTL_WRT);

	/* clear DMA RST for Rx (Rx start) */
	ucan_x_writel(dev, PCIEFD_CTL_RST_BIT, PCIEFD_REG_RX_CTL_CLR);

	/* do an initial ACK (before writing RX_BARRIER, it's better)  */

	/* Note: if we use RX DMA area to init irq_tag, then we have to wait
	 * a bit for the above command to complete
	 */
	pcifd_dma_ack(dev);

	/* enable IRQ for this uCAN after having set next irq_tag */
	ucan_x_writel(dev, PCIEFD_CTL_IEN_BIT, PCIEFD_REG_RX_CTL_SET);

#ifdef DEBUG_PCIFD
	/* RX_DMA_ADDR_L should have changed (+0xc) */
	pcifd_dev_dump(dev, __func__, 0, -1);
#endif

	return err;
}

/*
 * static int pcifd_device_open_fd(struct pcandev *dev,
 *					struct pcanfd_init *pfdi)
 *
 * ioctl(init).
 *
 * Note that this function will be called twice: 
 *
 * 1/ when user calls "open()" (with default bitrates)
 * 2/ when user calls "ioctl(SET_INIT)"
 */
static int pcifd_device_open_fd(struct pcandev *dev,
				struct pcanfd_init *pfdi)
{
	int err;

#if defined(DEBUG_TRACE) || defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	pr_info(DEVICE_NAME ": %s(CAN%u, clk=%u Hz, bitrates=%u/%u bps) "
			"bus=%d\n",
		__func__, dev->can_idx+1, pfdi->clock_Hz,
		pfdi->nominal.bitrate, pfdi->data.bitrate, dev->bus_state);
#endif

	/* if coming from a BUS-OFF condition, no need to redo all of that! */
	if (dev->bus_state == PCANFD_ERROR_BUSOFF) {

		/* reset counters only */
		ucan_clr_err_counters(dev);

		pcan_set_tx_engine(dev, TX_ENGINE_STARTED);

		err = ucan_set_bus_on(dev);
		if (err) {
			pr_err(DEVICE_NAME
				": failed to restart PCIe CAN%u  "
				"(err %d)\n", dev->can_idx+1, err);
		}

		return 0;
	}

	/* put TX engine state into IDLE so that it won't go into STOPPED
	 * state next: we will do it once RB bit will be received
	 */
	pcan_set_tx_engine(dev, TX_ENGINE_IDLE);

	/* Note: the following commands MUST be done in RESET mode */
	ucan_set_bus_off(dev);

	err = ucan_device_open_fd(dev, pfdi, 0, 0);
	if (err) {
		pr_err(DEVICE_NAME
			": failed to open PCI device CAN%u (err %d)\n",
			dev->can_idx+1, err);
		goto fail;
	}

	/* v8.16: options are now set/cleared in ucan_device_open_fd() */
	err = ucan_set_bus_on(dev);
	if (err) {
		pr_err(DEVICE_NAME
			": failed to set PCI CAN%u in operational state "
			"(err %d)\n", dev->can_idx+1, err);
		goto fail;
	}

	/* it looks like sometimes, a small delay is needed between BUS=ON
	 * and RX_BARRIER commands. Without this delay, the STATUS=ACTIVE msg
	 * may not be received while RB=1 will always be.
	 */
	pcan_usleep(50);

	ucan_rx_barrier(dev);

	return 0;

fail:
	pcifd_driver_free_irq(dev, NULL);
	return err;
}

/*
 * static int pcifd_device_write(struct pcandev *dev, struct pcanusr *ctx)
 *
 * Note: calling this function with dev->isr_lock locked!
 */
static int pcifd_device_write(struct pcandev *dev, struct pcanusr *ctx)
{
	return __pcifd_device_write(dev, ctx);
}

/* decode uCAN message timestamp */
static struct pcan_timeval *pcifd_decode_timestamp(struct pcandev *dev,
						u32 ts_low, u32 ts_high,
						struct pcan_timeval *tv)
{
	pcan_sync_decode(dev, ts_low, ts_high, tv);
	return tv;
}

/* handle a UCAN_MSG_CAN_RX message read from Rx DMA */
static int pcifd_handle_canrx(struct ucan_engine *ucan,
					struct ucan_msg *rx_msg, void *arg)
{
	struct ucan_rx_msg *rm = (struct ucan_rx_msg *)rx_msg;
	struct pcandev *dev = (struct pcandev *)arg;
	struct pcan_timeval tv;

	return ucan_post_canrx_msg(dev, rm,
		pcifd_decode_timestamp(dev, le32_to_cpu(rm->ts_low),
					le32_to_cpu(rm->ts_high), &tv));
}

/* handle a UCAN_MSG_ERROR message recieved from Rx DMA area */
static int pcifd_handle_error(struct ucan_engine *ucan,
					struct ucan_msg *rx_msg, void *arg)
{
	struct ucan_error_msg *er = (struct ucan_error_msg *)rx_msg;
	struct pcandev *dev = (struct pcandev *)arg;
	struct pcan_timeval tv;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s CAN%u): rx=%u tx=%u\n", __func__,
			dev->adapter->name, dev->can_idx+1,
			er->rx_err_cnt, er->tx_err_cnt);
#endif

	return ucan_post_error_msg(dev, er,
		pcifd_decode_timestamp(dev, le32_to_cpu(er->ts_low),
					le32_to_cpu(er->ts_high), &tv));
}

/* handle a UCAN_MSG_BUSLOAD message received from Rx DMA area */
static int pcifd_handle_bus_load(struct ucan_engine *ucan,
					struct ucan_msg *rx_msg, void *arg)
{
	struct ucan_bus_load_msg *bl = (struct ucan_bus_load_msg *)rx_msg;
	struct pcandev *dev = (struct pcandev *)arg;
	struct pcan_timeval tv;

	return ucan_post_bus_load_msg(dev, bl,
			pcifd_decode_timestamp(dev, le32_to_cpu(bl->ts_low),
						le32_to_cpu(bl->ts_high), &tv));
}

/* handle a UCAN_MSG_STATUS message recieved from Rx DMA area */
static int pcifd_handle_status(struct ucan_engine *ucan,
					struct ucan_msg *rx_msg, void *arg)
{
	struct ucan_status_msg *st = (struct ucan_status_msg *)rx_msg;
	struct pcandev *dev = (struct pcandev *)arg;
	int err;

#if defined(DEBUG_WRITE) || defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	//if (dev->can_idx == 0)
	pr_info(DEVICE_NAME ": %s(CAN%u): EP=%u EW=%u BO=%u RB=%u\n",
		__func__, dev->can_idx+1, !!UCAN_STMSG_PASSIVE(st),
		!!UCAN_STMSG_WARNING(st), !!UCAN_STMSG_BUSOFF(st),
		!!UCAN_STMSG_RB(st));
#endif

	/* this STATUS is the CNF of the CMD_RX_BARRIER.
	 * It also enables Tx path.
	 */
	if (UCAN_STMSG_RB(st)) {
		pcan_lock_irqsave_ctxt flags;

		err = 0;

		dev->lock_irq(dev, &flags);

		if (dev->locked_tx_engine_state == TX_ENGINE_IDLE) {
			pcifd_enable_tx_path(dev);

#ifdef NETDEV_SUPPORT
			/* restart netdev since tx_engine was not STOPPED */
			err = 1;
#else
			/* Tx path is now enabled: can start writing... */
			err = __pcifd_device_write(dev, NULL);
#endif
		}

		dev->unlock_irq(dev, &flags);

		/* err == 0 => no more free page in Tx DMA area */
		if (err) {

#if defined(DEBUG_WRITE) || defined(DEBUG_IRQ_TX)
			pr_info(DEVICE_NAME
				": %s(CAN%u): signaling writing task\n",
				__func__, dev->can_idx+1);
#endif

			pcan_event_signal(&dev->out_event);
#ifdef NETDEV_SUPPORT
			/* restart netdev in case pcan fifo was full */
			if (dev->netdev)
				netif_wake_queue(dev->netdev);
#endif
			err = 0;
		}
	} else {
		struct pcan_timeval tv;

		err = ucan_post_status_msg(dev, st,
			pcifd_decode_timestamp(dev, le32_to_cpu(st->ts_low),
					       le32_to_cpu(st->ts_high), &tv));
	}

	return err;
}

/* select the clock domain by writing the corresponding register */
static int pcifd_set_clk_domain(struct pcandev *dev,
				struct pcanfd_init *pfdi)
{
	/* select the 80MHz clock for the CAN with PCIEFD_REG_CLK_SEL */
	switch (pfdi->clock_Hz) {
	case 20000000:
		ucan_x_writel(dev, PCIEFD_CLK_SEL_20MHZ, PCIEFD_REG_CLK_SEL);
		break;
	case 24000000:
		ucan_x_writel(dev, PCIEFD_CLK_SEL_24MHZ, PCIEFD_REG_CLK_SEL);
		break;
	case 30000000:
		ucan_x_writel(dev, PCIEFD_CLK_SEL_30MHZ, PCIEFD_REG_CLK_SEL);
		break;
	case 40000000:
		ucan_x_writel(dev, PCIEFD_CLK_SEL_40MHZ, PCIEFD_REG_CLK_SEL);
		break;
	case 60000000:
		ucan_x_writel(dev, PCIEFD_CLK_SEL_60MHZ, PCIEFD_REG_CLK_SEL);
		break;
	case 80000000:
		ucan_x_writel(dev, PCIEFD_CLK_SEL_80MHZ, PCIEFD_REG_CLK_SEL);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* interface functions used to send commands / handle msgs to PCI/uCAN */
static int (*pcifd_handlers[])(struct ucan_engine *,
				  struct ucan_msg *,
				  void *) = {
	[UCAN_MSG_CAN_RX] = pcifd_handle_canrx,

	/* this handler pushes STATUS msgs when rx/tx errors counters change */
	[UCAN_MSG_ERROR] = pcifd_handle_error,
	[UCAN_MSG_STATUS] = pcifd_handle_status,
	[UCAN_MSG_BUSLOAD] = pcifd_handle_bus_load,
};

static struct ucan_ops pcifd_driver_ops = {
	.set_clk_domain = pcifd_set_clk_domain,
	.send_cmd = pcifd_send_cmd,
	.handle_msg_table = pcifd_handlers,
	.handle_msg_size = ARRAY_SIZE(pcifd_handlers),
	//.handle_private_msg = ucan_pcie_handle_private_msg,
};

/*
 * static int pcifd_driver_cleanup(struct pcandev *dev)
 *
 * called when device is removed from the devices list.
 */
static int pcifd_driver_cleanup(struct pcandev *dev)
{
#if defined(DEBUG_TRACE) || defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	pcifd_dma_clr_rx(dev);
	pcifd_dma_clr_tx(dev);
	pcifd_wait_for_eot(dev);

	pcifd_dma_free(dev);

#ifdef PCAN_PCI_MSI_WORKAROUND
	if (fdusemsi != PCAN_PCI_USEMSI_INTA)
		pcan_free_irq(dev);
#endif

	pcan_remove_dev_from_list(dev);
	pcan_destroy_dev(dev);

	if (_pci_devices > 0)
		_pci_devices--;

	if (pcifd_devices > 0) {
		pcifd_devices--;

		/* in case we have been called from the module cleanup()
		 * function, then if the last dev is being destroyed, then the
		 * driver must be removed too
		 */
		if (!pcifd_devices)
			pcifd_hw_remove(dev->port.pci.pciDev);
	}

	return 0;
}

/* probe one uCAN channel */
static int pcifd_dev_probe(struct pcifd_adapter *ucan_pci, int c)
{
	struct pcandev *dev = pcifd_dev(ucan_pci, c);

	dev->can_idx = c;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif

	ucan_soft_init(dev, &ucan_pci->pci.adapter.hw_ver);

	dev->open = pcifd_driver_open;
	dev->req_irq = pcifd_driver_req_irq;
	dev->free_irq = pcifd_driver_free_irq;
	dev->release = pcifd_driver_release;
	dev->filter = pcan_create_filter_chain();
	dev->cleanup = pcifd_driver_cleanup;

	/* pcan chardev interface */
	dev->device_open_fd = pcifd_device_open_fd;
	dev->device_write = pcifd_device_write;
	dev->device_release = pcifd_device_release;

	dev->port.pci.pciDev = ucan_pci->pci.dev;
	dev->port.pci.bar0_cfg_addr = ucan_pci->pci.bar0_addr;
	dev->port.pci.can_port_addr = ucan_pci->pci.bar0_addr +
							PCIEFD_CANx_ADDR(c);
	dev->dwPort = (u32 )(long )dev->port.pci.can_port_addr;
#ifdef DEBUG_PCIFD
	pcifd_dev_dump(dev, __func__, 0, -1);
#endif
	/* init uCAN engine: here, the uCAN handles one pcandev */
	dev->ucan.ops = &pcifd_driver_ops;
	dev->ucan.devs = ucan_pci->devs;
	dev->ucan.cmd_head = &dev->port.pci.ucan_cmd;
	dev->ucan.cmd_size = sizeof(u64);
	dev->ucan.cmd_len = 0;

	if (pcifd_dma_alloc(dev))
		return -ENOMEM;

	/* adjust MSI/INTA irq from adapter device IRQ value */
	dev->flags &= ~PCAN_DEV_MSI_SHARED;
	dev->wIrq = ucan_pci->pci.dev->irq;

#ifdef PCAN_PCI_ENABLE_MSI
	if (pci_dev_msi_enabled(ucan_pci->pci.dev)) {

		if (fdusemsi != PCAN_PCI_USEMSI_SHARED)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
			dev->wIrq = pci_irq_vector(ucan_pci->pci.dev, 
						   dev->can_idx);
#else
			dev->wIrq += dev->can_idx;
#endif

		else
			dev->flags |= PCAN_DEV_MSI_SHARED;
	}
#endif

#ifdef DEBUG_PCIFD
	pr_info("%s: CAN%u: assigned to irq %u (msi_shared=%u)\n",
		DEVICE_NAME, dev->can_idx+1, dev->wIrq,
		!!(dev->flags & PCAN_DEV_MSI_SHARED));
#endif
	dev->nMajor = pcan_drv.nMajor;
	dev->nMinor = PCAN_PCI_MINOR_BASE + _pci_devices++;

	pcan_pci_set_dev_adapter(dev, &ucan_pci->pci);

	if (VER_NUM(dev->hw_ver->major,
		    dev->hw_ver->minor,
		    dev->hw_ver->subminor) >= VER_NUM(3, 5, 0))

		dev->features |= PCAN_DEV_TS_SOF_RDY |
				 PCAN_DEV_SELF_ACK_RDY |
				 PCAN_DEV_BRS_IGN_RDY;

	pcan_lock_init(&dev->isr_lock);

	pcan_add_dev_in_list(dev);

	/* give our specific attributes */
	dev->sysfs_attrs = pcan_dev_sysfs_pciefd_attrs;

	pcifd_devices++;

#ifdef PCAN_PCI_MSI_WORKAROUND
	if (fdusemsi != PCAN_PCI_USEMSI_INTA)
		__pcifd_driver_req_irq(dev);
#endif

	/* Win driver does write a NOP command first... (???) */
	ucan_add_cmd_nop(dev);
	pcifd_send_cmd(dev);

	if (dev->device_alt_num != 0xffffffff)
		pr_info(DEVICE_NAME
			": - pcie fd device minor %d number %u found\n",
			dev->nMinor, dev->device_alt_num);
	else
		pr_info(DEVICE_NAME ": - pcie fd device minor %d found\n",
			dev->nMinor);

	return 0;
}

/* remove a uCAN channel */
static int pcifd_dev_remove(struct pcifd_adapter *ucan_pci, int c)
{
	struct pcandev *dev = pcifd_dev(ucan_pci, c);

	if (!dev)
		return 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, dev->can_idx+1);
#endif
	pcan_cleanup_dev(dev);

	return 0;
}

static void pcifd_free_adapter(struct pcifd_adapter *ucan_pci)
{
	int i;

	for (i = 0; i < ucan_pci->pci.adapter.can_count; i++)
		pcan_free_dev(pcifd_dev(ucan_pci, i));

	pcan_free_adapter(&ucan_pci->pci.adapter);
	pcifd_adapters--;
}

/* probe a uCAN device */
int pcan_pcifd_probe(struct pci_dev *dev, u16 sub_system_id,
		     const char *adapter_name, const char *part_num,
		     int can_count)
{
	extern ushort dmamask;

	const u64 drm = dma_get_required_mask(&dev->dev);
	struct pcifd_adapter *ucan_pci;
	int err, l, i;
	u32 v1, v2, fw_ver;

	dev_info(&dev->dev, "%s sub-system id %0xh (%u channels)\n",
		adapter_name, sub_system_id, can_count);

	err = pci_request_regions(dev, DRV_NAME);
	if (err)
		goto fail;

	/* allocate the adapter object */
	l = sizeof(*ucan_pci);
	ucan_pci = pcan_alloc_adapter_ex(adapter_name, part_num,
					 pcifd_adapters++, can_count, l);
	if (!ucan_pci)
		goto fail_release_regions;

	for (i = 0; i < can_count; i++) {
		pcifd_dev(ucan_pci, i) = pcan_alloc_dev("pcifd", HW_PCIE_FD, i);
		if (!pcifd_dev(ucan_pci, i)) {
			err = -ENOMEM;
			goto fail_free;
		}
	}

	/* map SYStem block */
	ucan_pci->pci.bar0_addr = pci_iomap(dev, 0, PCIEFD_BAR0_SIZE);
        if (!ucan_pci->pci.bar0_addr) {
		dev_err(&dev->dev, "failed to map PCI resource #0\n");
		err = -ENOMEM;
		goto fail_free;
	}

	ucan_pci->pci.dev = dev;

#ifdef DEBUG_PCIFD
	pr_info(DEVICE_NAME ": BAR0=%p\n", ucan_pci->pci.bar0_addr);
#endif

#ifdef PCAN_PCI_ENABLE_MSI
#ifdef PCAN_PCI_ENABLE_MSIX
	for (i = 0; i < 4; i++)
		ucan_pci->msix_entries[i].entry = i;

	err = pci_enable_msix_range(dev,
				ucan_pci->msix_entries,
				can_count,
				can_count);
	if (err > 0) {
		pr_info(DEVICE_NAME ": enabling %u MSI-X status: %d\n",
			can_count, err);

		pci_disable_msix(dev);
	} else {
		pr_info(DEVICE_NAME ": MSI-X failed (err %d)\n", err);
	}
#endif

	/* enable MSI for the PCI device: try one MSI per CAN but accept
	 * 1 for all
	 */
	switch (fdusemsi) {
	case PCAN_PCI_USEMSI_SHARED:
	case PCAN_PCI_USEMSI_NOTSHARED:
		/* try MSI and accept to share INTerrupt(s) */
		err = pcan_pci_enable_msi(&ucan_pci->pci, can_count, -1);
		break;
	default:
		err = -1;
		break;
	}

	/* change fdusemsi to control when IRQ will be requested/freed */
	if (err)
		fdusemsi = PCAN_PCI_USEMSI_INTA;

	else if (ucan_pci->pci.msi_count == can_count)
		fdusemsi = PCAN_PCI_USEMSI_NOTSHARED;
	else
		fdusemsi = PCAN_PCI_USEMSI_SHARED;
#endif /* PCAN_PCI_ENABLE_MSI */

	/* check module parameters; warn if out-of-range and use default one */
	if ((fdirqcl < PCIEFD_CTL_IRQ_CL_MIN) ||
		(fdirqcl > PCIEFD_CTL_IRQ_CL_MAX)) {
		pr_info(DEVICE_NAME
			": fdirqcl=%u out of range [%u..%u]: reset to %u\n",
			fdirqcl,
			PCIEFD_CTL_IRQ_CL_MIN, PCIEFD_CTL_IRQ_CL_MAX,
			PCIEFD_CTL_IRQ_CL_DEF);

		fdirqcl = PCIEFD_CTL_IRQ_CL_DEF;

	} else if (fdirqcl != PCIEFD_CTL_IRQ_CL_DEF) {
		pr_warn(DEVICE_NAME
			": running with fdirqcl=%d instead of %u\n",
			fdirqcl, PCIEFD_CTL_IRQ_CL_DEF);
	}

	if ((fdirqtl < PCIEFD_CTL_IRQ_TL_MIN) ||
		(fdirqtl > PCIEFD_CTL_IRQ_TL_MAX)) {
		pr_info(DEVICE_NAME
			": fdirqtl=%u out of range [%u..%u]: reset to %u\n",
			fdirqtl,
			PCIEFD_CTL_IRQ_TL_MIN, PCIEFD_CTL_IRQ_TL_MAX,
			PCIEFD_CTL_IRQ_TL_DEF);

		fdirqtl = PCIEFD_CTL_IRQ_TL_DEF;

	} else if (fdirqtl != PCIEFD_CTL_IRQ_TL_DEF) {
		pr_warn(DEVICE_NAME
			": running with fdirqtl=%d instead of %u\n",
			fdirqtl, PCIEFD_CTL_IRQ_TL_DEF);
	}

	v1 = ucan_sys_readl(ucan_pci, PCIEFD_REG_VER1);
	v2 = ucan_sys_readl(ucan_pci, PCIEFD_REG_VER2);

	ucan_pci->pci.adapter.hw_ver.major = (v2 & 0x0000f000) >> 12;
	ucan_pci->pci.adapter.hw_ver.minor = (v2 & 0x00000f00) >> 8;
	ucan_pci->pci.adapter.hw_ver.subminor = (v2 & 0x000000f0) >> 4;
	ucan_pci->pci.adapter.hw_ver.extra = 0;

	pcan_pci_flash_init(&ucan_pci->pci, PCIEFD_FLASH_DEVID_OFFSET);

	pr_info(DEVICE_NAME ": PCB v%xh FPGA v%u.%u.%u (mode %u)%s\n",
		(v1 & 0xf0000000) >> 28,
		ucan_pci->pci.adapter.hw_ver.major,
		ucan_pci->pci.adapter.hw_ver.minor,
		ucan_pci->pci.adapter.hw_ver.subminor,
		(v2 & 0x0000000f),
		(ucan_pci->pci.flash_data_cache.recordsize == 0xffffffff) \
			? "" : " Flash: Ok");
#ifdef DEBUG_PCIFD
	pcifd_sys_dump(ucan_pci, __func__, 0, -1);
#endif

	fw_ver = VER_NUM(ucan_pci->pci.adapter.hw_ver.major,
			 ucan_pci->pci.adapter.hw_ver.minor,
			 ucan_pci->pci.adapter.hw_ver.subminor);

	/* FW 3.2.x and 64-bit DMA controller workaround:
	 * request 32-bit DMA addresses only
	 */
	if (fw_ver < VER_NUM(3, 3, 0))
		dmamask = 32;

	/* can the DMA controller support the DMA addressing limitation ? */
	if (!pcifd_dma_set_mask_and_coherent(&dev->dev,
					     DMA_BIT_MASK(dmamask))) {
#ifdef DEBUG_DMA
		pr_info(DEVICE_NAME ": %u-bits DMA controler (%llxh)\n",
			dmamask, drm);
#endif
	} else {
		err = pcifd_dma_set_mask_and_coherent(&dev->dev,
						      DMA_BIT_MASK(32));
		if (err) {
			dev_warn(&dev->dev,
				"%s: No suitable DMA available: err %d "
				"(required mask=%llxh)\n",
				DEVICE_NAME, err, drm);
			goto fail_release_regions;
		}
#ifdef DEBUG_DMA
		pr_info(DEVICE_NAME ": 32-bits DMA controler (%llxh)\n",
			drm);
#endif
	}

#ifdef PCAN_CMD64_USES_RAW_SPIN_LOCK
	raw_spin_lock_init(&ucan_pci->ucan_lock);
#else
	pcan_lock_init(&ucan_pci->ucan_lock);
#endif

	pci_set_drvdata(dev, ucan_pci);

	/* stop system clock */
	ucan_sys_writel(ucan_pci,
			PCIEFD_SYS_CTL_CLK_EN, PCIEFD_REG_SYS_CTL_CLR);

	/* do this after iomap */
	pci_set_master(dev);

	for (i = 0; i < can_count; i++) {
		err = pcifd_dev_probe(ucan_pci, i);
		if (err)
			goto fail_all;
	}

	/* complete uCAN device initialisation */
	for (i = 0; i < can_count; i++)
		pcifd_dev(ucan_pci, i)->ucan.devs_count = can_count;

	/* reset system timestamp counter */
	ucan_sys_writel(ucan_pci,
			PCIEFD_SYS_CTL_TS_RST, PCIEFD_REG_SYS_CTL_SET);

	/* Reset uCAN clock */
	for (i = 0; i < can_count; i++)
		ucan_x_writel(pcifd_dev(ucan_pci, i),
				UCAN_MISC_TS_RST, PCIEFD_REG_MISC);

	/* wait a bit (read cycle) */
	(void )ucan_sys_readl(ucan_pci, PCIEFD_REG_VER1);

	/* free all clocks */
	ucan_sys_writel(ucan_pci,
			PCIEFD_SYS_CTL_TS_RST, PCIEFD_REG_SYS_CTL_CLR);

	for (i = 0; i < can_count; i++)
		ucan_x_writel(pcifd_dev(ucan_pci, i),
				0, PCIEFD_REG_MISC);

	/* start system clock */
	ucan_sys_writel(ucan_pci,
			PCIEFD_SYS_CTL_CLK_EN, PCIEFD_REG_SYS_CTL_SET);
	return 0;

fail_all:
	while (i-- > 0)
		pcifd_dev_remove(ucan_pci, i);

	pcan_pci_disable_msi(dev);

	pci_iounmap(dev, ucan_pci->pci.bar0_addr);

fail_free:
	pcifd_free_adapter(ucan_pci);

fail_release_regions:
	pci_release_regions(dev);

fail:
	return err;
}

/* remove the driver */
static void pcifd_hw_remove(struct pci_dev *dev)
{
	struct pcifd_adapter *ucan_pci = pci_get_drvdata(dev);

#if defined(DEBUG_TRACE) || defined(DEBUG_WRITE) || defined(DEBUG_IRQ_RX) || defined(DEBUG_IRQ_TX)
	pr_info("%s: %s()\n", DEVICE_NAME, __func__);
#endif
	if (!ucan_pci)
		return;

	/* set driver data to NULL to not go back here again... */
	pci_set_drvdata(dev, NULL);

	pcan_pci_disable_msi(dev);

	pci_iounmap(dev, ucan_pci->pci.bar0_addr);

	pcifd_free_adapter(ucan_pci);
	pci_release_regions(dev);
}

void pcan_pcifd_remove(struct pci_dev *dev)
{
	struct pcifd_adapter *ucan_pci = pci_get_drvdata(dev);
	int i;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	if (!ucan_pci)
		return;

	/* note: because we have to manage two ways of deregistering (Udev /
	 * no Udev), pcifd_dev_cleanup() (thus pcifd_hw_remove())
	 * could be reentrant. pci_set_drvdata(NULL) is used to prevent from
	 * this reentrance (see pcifd_hw_remove())
	 */
	for (i = 0; i < ucan_pci->pci.adapter.can_count; i++)
		pcifd_dev_remove(ucan_pci, i);

	pcifd_hw_remove(dev);
}
