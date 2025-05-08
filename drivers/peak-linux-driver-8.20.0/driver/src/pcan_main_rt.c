/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_main_rt.c - the starting point of the driver,
 *                  init and cleanup and proc interface
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
 *               Marcel Offermans <marcel.offermans@luminis.nl>
 *               Philipp Baer <philipp.baer@informatik.uni-ulm.de>
 *               Garth Zeglin <garthz@ri.cmu.edu>
 *               Harald Koenig <H.Koenig@science-computing.de>
 */
#define DEV_REGISTER		rt_dev_register
#define DEV_UNREGISTER		rt_dev_unregister
#define REMOVE_DEV_LIST		rt_remove_dev_list

/* list of the RT device objects */
static struct list_head pcan_rtdm_dev_list;

static int rt_dev_register(void)
{
	struct list_head *pos;
	struct pcandev *dev;
	struct rtdm_device *rtdmdev;
	struct pcan_rtdm_dev *rt_dev;
	int result = 0;

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* this function is called from init_module(), so in a monotask 
	 * interrupt less context: no lock needed
	 */
	list_for_each(pos, &pcan_drv.devices) {
		dev = list_entry(pos, struct pcandev, list_dev);

		rtdmdev = (struct rtdm_device *)pcan_malloc(sizeof(*rtdmdev),
							    GFP_KERNEL);
		if (!rtdmdev)
			return -ENOMEM;

		rt_dev = (struct pcan_rtdm_dev *)pcan_malloc(sizeof(*rt_dev),
							     GFP_KERNEL);
		if (!rt_dev) {
			pcan_free(rtdmdev);
			return -ENOMEM;
		}

#ifdef XENOMAI3
		/* device_id is no read with rtdm_fd_minor()
		 * proc_name has been dropped
		 */
		memset(rtdmdev, '\0', sizeof(*rtdmdev));
		rtdmdev->driver = &pcandrv_rt;
		rtdmdev->label = "pcan%d";

		rtdmdev->device_data = dev;
#else
		memcpy(rtdmdev, &pcandev_rt, sizeof(struct rtdm_device));
		rtdmdev->device_id = MKDEV(dev->nMajor,dev->nMinor);
		snprintf(rtdmdev->device_name, RTDM_MAX_DEVNAME_LEN,
							"pcan%d", dev->nMinor);
		rtdmdev->proc_name = rtdmdev->device_name;
#endif
		result = rtdm_dev_register(rtdmdev);

		if (!result) {
#ifdef DEBUG
			pr_info(DEVICE_NAME
				": %s(): RTDM device \"pcan%d\" registered\n",
				__func__, dev->nMinor);
#endif
			rt_dev->device = rtdmdev;
			list_add_tail(&rt_dev->list_dev, &pcan_rtdm_dev_list);
		} else {

			pr_err(DEVICE_NAME
				": rtdm_dev_register() failure (err %d)\n",
				result);

			pcan_free(rtdmdev);
			pcan_free(rt_dev);
			return result;
		}

		dev->rtdm_dev = rtdmdev;

		/* create sysfs device node */
		pcan_sysfs_dev_node_create(dev);
	}

	return result;
}

static void rt_dev_unregister(void)
{
	struct list_head *pos;
	struct pcandev *dev;
	struct pcan_rtdm_dev *rt_dev;

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* this function is called from exit_module(), so in a monotask 
	 * interrupt less context: no lock needed
	 */

	/* remove sysfs entries */
	list_for_each(pos, &pcan_drv.devices) {
		dev = list_entry(pos, struct pcandev, list_dev);
		pcan_sysfs_dev_node_destroy(dev);
	}

	/* unregister all registered devices */
	list_for_each(pos, &pcan_rtdm_dev_list) {

		rt_dev = list_entry(pos, struct pcan_rtdm_dev, list_dev);

#ifdef XENOMAI3
		rtdm_dev_unregister(rt_dev->device);
#else
		rtdm_dev_unregister(rt_dev->device, 1000);
#endif
	}
}

void rt_remove_dev_list(void)
{
	struct pcan_rtdm_dev *rt_dev;
	struct list_head *pos, *n;

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	/* cycle through the list of devices and remove them */
	list_for_each_safe(pos, n, &pcan_rtdm_dev_list) {
		rt_dev = list_entry(pos, struct pcan_rtdm_dev, list_dev);

		/* free all device allocted memory */
		pcan_free(rt_dev->device);
		list_del(&rt_dev->list_dev);

		pcan_free(rt_dev);
	}

	/* remove all pcan devices */
	remove_dev_list();
}
