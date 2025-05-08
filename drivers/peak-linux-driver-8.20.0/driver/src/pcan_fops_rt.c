/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_fops_rt.c - all file operation functions, exports only struct fops
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
 *               Arno <a.vdlaan@hccnet.nl>
 *               John Privitera <JohnPrivitera@dciautomation.com>
 */
#if RTDM_API_VER < 6
#define IOCTL_REQUEST_TYPE int
#else
#define IOCTL_REQUEST_TYPE unsigned int
#endif

#ifdef XENOMAI3
#define RTDM_SUBCLASS_PCAN	0

#ifndef CONFIG_XENO_OPT_RTDM_SELECT

/* if defined, the "select()" call is implemented by the driver
 * (simulate RTAI 5.1 option) */
#define CONFIG_XENO_OPT_RTDM_SELECT
#endif

#elif defined(RTAI)

/* Note: in that case, CONFIG_XENO_OPT_RTDM_SELECT should be defined in
 *       /usr/src/rtai/rtai_config.h */
#define rtdm_event_select(e, t, s, i)	rtdm_event_select_bind(e, t, s, i)
#endif

/* CAN-FD new API */
static int copy_from_user_rt(rtdm_user_info_t *user_info,
				void *to, const void __user *from, size_t size)
{
	if (user_info) {
		if (!rtdm_read_user_ok(user_info, from, size) ||
			rtdm_copy_from_user(user_info, to, from, size))
			return -EFAULT;
	} else {
		memcpy(to, from, size);
	}

	return 0;
}

static int copy_to_user_rt(rtdm_user_info_t *user_info,
				void __user *to, const void *from, size_t size)
{
	if (user_info) {
		if (!rtdm_rw_user_ok(user_info, to, size) ||
			rtdm_copy_to_user(user_info, to, from, size))
			return -EFAULT;
	} else {
		memcpy(to, from, size);
	}

	return 0;
}

/*
 * called when the path is opened
 */
#ifdef XENOMAI3
static int pcan_open_nrt(struct rtdm_fd *fd, int oflags)
{
	int _major = 0;
	int _minor = rtdm_fd_minor(fd);
	struct pcanusr *ctx = (struct pcanusr *)rtdm_fd_to_private(fd);
#else
static int pcan_open_nrt(struct rtdm_dev_context *context,
			 rtdm_user_info_t *user_info, int oflags)
{
	int _major = MAJOR(context->device->device_id);
	int _minor = MINOR(context->device->device_id);
	struct pcanusr *ctx = (struct pcanusr *)context->dev_private;
#endif
	struct pcandev *dev;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(major=%d minor=%d, oflags=%08xh)\n",
		__func__, _major, _minor, oflags);
#endif

	/* TODO: get the device major number from xenomai structure... */
	dev = pcan_search_dev(_major, _minor);
	if (!dev)
		return -ENODEV;

	ctx->dev = dev;
	ctx->open_flags = oflags;
#ifndef XENOMAI3
	ctx->context = context;
#endif
	ctx->nReadRest = 0;
	ctx->nTotalReadCount = 0;
	ctx->pcReadPointer = ctx->pcReadBuffer;
	ctx->nWriteCount = 0;
	ctx->pcWritePointer = ctx->pcWriteBuffer;

	/* why? */
	err = pcan_open_path(dev, ctx);

	return pcan_put_dev(dev, err);
}

/*
 * called when the path is closed.
 *
 * Note: (RTDM-and-Application.pdf 2.2 p3)
 *
 * "Closing a device instance is sensitive to the correct
 *  context. If the instance has been created in nonreal-time
 *  context, it cannot be closed within a realtime
 *  task"
 */
#ifdef XENOMAI3
static void pcan_close_nrt(struct rtdm_fd *fd)
{
	struct pcanusr *ctx = (struct pcanusr *)rtdm_fd_to_private(fd);

#else
static int pcan_close_nrt(struct rtdm_dev_context *context,
				rtdm_user_info_t *user_info)
{
	struct pcanusr *ctx = (struct pcanusr *)context->dev_private;
#endif
	struct pcandev *dev;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(): RT task=%d\n",
		__func__, rtdm_in_rt_context());
#endif

	dev = pcan_get_dev(ctx);
	if (dev) {
		pcan_release_path(dev, ctx);
		ctx->dev = NULL;
#ifndef XENOMAI3
		ctx->context = NULL;
#endif
		pcan_put_dev(dev, 0);
	}

#ifndef XENOMAI3
	return 0;
#endif
}

/* is called at user ioctl() with cmd = PCAN_INIT */
static int pcan_ioctl_init_rt(rtdm_user_info_t *user_info,
			struct pcanusr *ctx, TPCANInit __user *pi)
{
	struct pcandev *dev = ctx->dev;
	struct pcanfd_init fdi = {};
	TPCANInit init;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	err = copy_from_user_rt(user_info, &init, pi, sizeof(init));
	if (err)
		return  -EFAULT;

	/* convert wBTR0BTR1 (8MHz) value into bitrate value. pfdi->clock_Hz
	 * is let 0 so that pcanfd_dev_open() will fill it with device default
	 * clock.
	 */
	return pcanfd_ioctl_set_init(dev, pcan_init_to_fd(dev, &fdi, &init));
}

/* is called at user ioctl() with cmd = PCAN_WRITE_MSG */
static int pcan_ioctl_write_rt(rtdm_user_info_t *user_info,
			struct pcanusr *ctx, TPCANMsg __user *usr)
{
	struct pcandev *dev = ctx->dev;
	struct pcanfd_txmsg tx;
	TPCANMsg msg;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif

	/* get from user space */
	if (copy_from_user_rt(user_info, &msg, usr, sizeof(msg))) {
		err = -EFAULT;
		goto fail;
	}

	/* do some minimal (but mandatory!) check */
	if (msg.LEN > 8) {
		pr_err(DEVICE_NAME
			": trying to send msg %xh  with invalid data len %d\n",
			msg.ID, msg.LEN);
		err = -EINVAL;
		goto fail;
	}

	/* convert old-style TPCANMsg into new-style struct pcanfd_msg */
	pcan_msg_to_fd(&tx.msg, &msg);
	err = pcanfd_ioctl_send_msg(dev, &tx, ctx);
	if (err)
		goto fail;

	return 0;

fail:
#ifdef DEBUG
        pr_err(DEVICE_NAME ": failed to write CAN frame (err %d)\n", err);
#endif
	return err;
}

/* is called at user ioctl() with cmd = PCAN_READ_MSG */
static int pcan_ioctl_read_rt(rtdm_user_info_t *user_info,
			struct pcanusr *ctx, TPCANRdMsg __user *usr)
{
	struct pcandev *dev = ctx->dev;
	struct pcanfd_rxmsg msgfd;
	TPCANRdMsg msg;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif

	do {
		err = pcanfd_ioctl_recv_msg(dev, &msgfd, ctx);
		if (err)
			return err;

		if (pcan_is_fd(&msgfd.msg)) {
			pr_info(DEVICE_NAME ": CAN-FD frame discarded "
				"(CAN 2.0 application)\n");
			err = -EINVAL;
		}
	} while (err);

	if (copy_to_user_rt(user_info, usr,
				pcan_fd_to_msg(&msg, &msgfd.msg), sizeof(*usr)))
		err = -EFAULT;

	return err;
}

/* is called at user ioctl() with cmd = PCAN_GET_STATUS */
static int pcan_ioctl_status_rt(rtdm_user_info_t *user_info,
			struct pcanusr *ctx, TPSTATUS __user *status)
{
	struct pcandev *dev = ctx->dev;
	TPSTATUS local;
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif

	pcan_ioctl_status_common(dev, &local);

	if (copy_to_user_rt(user_info, status, &local, sizeof(local))) {
		err = -EFAULT;
		goto fail;
	}

	dev->wCANStatus = 0;
	dev->nLastError = 0;

fail:
	return err;
}

/* is called at user ioctl() with cmd = PCAN_GET_EXT_STATUS */
static int pcan_ioctl_extended_status_rt(rtdm_user_info_t *user_info,
			struct pcanusr *ctx, TPEXTENDEDSTATUS __user *status)
{
	struct pcandev *dev = ctx->dev;
	TPEXTENDEDSTATUS local;
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif

	pcan_ioctl_extended_status_common(dev, &local);

	if (copy_to_user_rt(user_info, status, &local, sizeof(local))) {
		err = -EFAULT;
		goto fail;
	}

	dev->wCANStatus = 0;
	dev->nLastError = 0;

fail:
	return err;
}

/* is called at user ioctl() with cmd = PCAN_DIAG */
static int pcan_ioctl_diag_rt(rtdm_user_info_t *user_info,
			struct pcanusr *ctx, TPDIAG __user *diag)
{
	struct pcandev *dev = ctx->dev;
	TPDIAG local;
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif

	pcan_ioctl_diag_common(dev, &local);

	if (copy_to_user_rt(user_info, diag, &local, sizeof(local)))
		err = -EFAULT;

	return err;
}

/* get BTR0BTR1 init values */
static int pcan_ioctl_BTR0BTR1_rt(rtdm_user_info_t *user_info,
			struct pcanusr *ctx, TPBTR0BTR1 __user *BTR0BTR1)
{
	TPBTR0BTR1 local;
	int err = 0;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s()\n", __func__);
#endif

	if (copy_from_user_rt(user_info, &local, BTR0BTR1, sizeof(local))) {
		err = -EFAULT;
		goto fail;
	}

	/* this does not influence hardware settings, only BTR0BTR1 values are
	 * calculated
	 */
	local.wBTR0BTR1 = sja1000_bitrate(local.dwBitRate, 0, 0);
	if (!local.wBTR0BTR1) {
		err = -EFAULT;
		goto fail;
	}

	if (copy_to_user_rt(user_info, BTR0BTR1, &local, sizeof(*BTR0BTR1)))
		err = -EFAULT;

fail:
	return err;
}

/* add a message filter_element into the filter chain or delete all
 * filter_elements */
static int pcan_ioctl_msg_filter_rt(rtdm_user_info_t *user_info,
			struct pcanusr *ctx, TPMSGFILTER __user *filter)
{
	struct pcandev *dev = ctx->dev;
	TPMSGFILTER local_filter;

#ifdef DEBUG_TRACE
	pr_info("%s: %s(CAN%u)\n", DEVICE_NAME, __func__, pcan_idx(dev)+1);
#endif

	/* filter == NULL -> delete the filter_elements in the chain */
	if (!filter) {
		pcan_delete_filter_all(dev->filter);
		return 0;
	}

	if (copy_from_user_rt(user_info, &local_filter,
					filter, sizeof(local_filter)))
		return -EFAULT;

	return pcan_add_filter(dev->filter, local_filter.FromID,
				local_filter.ToID, local_filter.MSGTYPE);
}

/*
 * set or get extra parameters from the devices
 */
static int pcan_ioctl_extra_parameters_rt(rtdm_user_info_t *user_info,
					  struct pcanusr *ctx,
					  TPEXTRAPARAMS __user *params,
					  int sizeof_extra_params)
{
	struct pcandev *dev = ctx->dev;
	TPEXTRAPARAMS local;
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s CAN%u)\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1);
#endif

	if (copy_from_user_rt(user_info, &local, params, sizeof_extra_params)) {
		pr_err(DEVICE_NAME ": %s(): copy_from_user() failure\n",
			__func__);
		err = -EFAULT;
		goto fail;
	}

	err = _pcan_ioctl_extra_parameters(dev, &local, sizeof_extra_params);
	if (!err)
		if (copy_to_user_rt(user_info, params, &local, 
				    sizeof_extra_params)) {
			pr_err(DEVICE_NAME ": %s(): copy_to_user() failure\n",
			       __func__);
			err = -EFAULT;
		}

fail:
	return err;
}

/* is called at user ioctl() call */
#ifdef XENOMAI3
static int pcan_ioctl_rt(struct rtdm_fd *user_info,
			 IOCTL_REQUEST_TYPE cmd, void *arg)
{
	struct pcanusr *ctx = (struct pcanusr *)rtdm_fd_to_private(user_info);

#else
static int pcan_ioctl_rt(struct rtdm_dev_context *context,
			 rtdm_user_info_t *user_info,
			 IOCTL_REQUEST_TYPE cmd, void *arg)
{
	struct pcanusr *ctx = (struct pcanusr *)context->dev_private;
#endif
	void __user *up = (void __user *)arg;
	struct pcanfd_init fdi;
	struct pcanfd_state fds;
	struct pcanfd_rxmsg rx;
	struct pcanfd_txmsg tx;
	int l, err;

	struct pcandev *dev = pcan_get_dev(ctx);
	if (!dev)
		return -ENODEV;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(CAN%u)\n", __func__, pcan_idx(dev)+1);
#endif
	switch (cmd) {
	case PCAN_INIT:
		err = pcan_ioctl_init_rt(user_info, ctx,
						(TPCANInit __user *)arg);
		break;
	case PCAN_READ_MSG:
		/* support blocking and nonblocking IO */
		err = pcan_ioctl_read_rt(user_info, ctx,
						(TPCANRdMsg __user *)arg);
		break;
	case PCAN_WRITE_MSG:
		/* support blocking and nonblocking IO */
		err = pcan_ioctl_write_rt(user_info, ctx,
						(TPCANMsg __user *)arg);
		break;
	case PCAN_GET_STATUS:
		err = pcan_ioctl_status_rt(user_info, ctx,
						(TPSTATUS __user *)arg);
		break;
	case PCAN_GET_EXT_STATUS:
		err = pcan_ioctl_extended_status_rt(user_info, ctx,
						(TPEXTENDEDSTATUS __user *)arg);
		break;
	case PCAN_DIAG:
		err = pcan_ioctl_diag_rt(user_info, ctx,
						(TPDIAG __user *)arg);
		break;
	case PCAN_BTR0BTR1:
		err = pcan_ioctl_BTR0BTR1_rt(user_info, ctx,
						(TPBTR0BTR1 __user *)arg);
		break;
	case PCAN_MSG_FILTER:
		err = pcan_ioctl_msg_filter_rt(user_info, ctx,
						(TPMSGFILTER *)arg);
		break;
	case PCAN_EXTRA_PARAMS_8_13:
	case PCAN_EXTRA_PARAMS:
		err = pcan_ioctl_extra_parameters_rt(user_info, ctx,
						(TPEXTRAPARAMS __user *)arg,
						_IOC_SIZE(cmd));
		break;

	/* CAN-FD new API */
	case PCANFD_SET_INIT:
		err = copy_from_user_rt(user_info, &fdi, up, sizeof(fdi));
		if (err)
			return pcan_put_dev(dev, -EFAULT);

		err = pcanfd_ioctl_set_init(dev, &fdi);
		break;

	case PCANFD_GET_INIT:
		err = pcanfd_ioctl_get_init(dev, &fdi);
		if (err)
			break;

		err = copy_to_user_rt(user_info, up, &fdi, sizeof(fdi));
		if (err)
			return pcan_put_dev(dev, -EFAULT);
		break;

	case PCANFD_RESET:
		err = pcanfd_ioctl_reset(dev, (unsigned long )up);
		break;

	case PCANFD_GET_STATE:
		err = pcanfd_ioctl_get_state(dev, &fds);
		if (err)
			break;

		err = copy_to_user_rt(user_info, up, &fds, sizeof(fds));
		if (err)
			return pcan_put_dev(dev, -EFAULT);

		break;

#ifdef PCANFD_ADD_FILTER
	case PCANFD_ADD_FILTER:
		if (arg) {
			struct pcanfd_msg_filter mf;

			err = copy_from_user_rt(user_info, &mf, up, sizeof(mf));
			if (err)
				return pcan_put_dev(dev, -EFAULT);

			err = pcanfd_ioctl_add_filter(dev, &mf);
		} else {
			err = pcanfd_ioctl_add_filter(dev, NULL);
		}
		break;
#endif
	case PCANFD_ADD_FILTERS:
		if (arg) {
			struct pcanfd_msg_filters mfl, *pfl;

			l = sizeof(struct pcanfd_msg_filters_0);
			err = copy_from_user_rt(user_info, &mfl, up, l);
			if (err)
				return pcan_put_dev(dev, -EFAULT);

			if (!mfl.count)
				return pcan_put_dev(dev, 0);

			l += mfl.count * sizeof(struct pcanfd_msg_filter);
			pfl = pcan_malloc(l, GFP_KERNEL);
			if (!pfl) {
				pr_err(DEVICE_NAME
					": failed to alloc filter list\n");
				return pcan_put_dev(dev, -ENOMEM);
			}

			if (copy_from_user_rt(user_info, pfl, up, l)) {
				pcan_free(pfl);
				return pcan_put_dev(dev, -EFAULT);
			}

			/* re-sync with kernel space content */
			pfl->count = mfl.count;
			err = pcanfd_ioctl_add_filters(dev, pfl);

			pcan_free(pfl);
		} else {
			err = pcanfd_ioctl_add_filters(dev, NULL);
		}
		break;

	case PCANFD_GET_FILTERS:
		if (arg) {
			struct pcanfd_msg_filters mfl, *pfl;

			l = sizeof(struct pcanfd_msg_filters_0);
			err = copy_from_user_rt(user_info, &mfl, up, l);
			if (err)
				return pcan_put_dev(dev, -EFAULT);

			if (!mfl.count)
				return pcan_put_dev(dev, 0);

			l += mfl.count * sizeof(struct pcanfd_msg_filter);
			pfl = pcan_malloc(l, GFP_KERNEL);
			if (!pfl) {
				pr_err(DEVICE_NAME
					": failed to alloc filter list\n");
				return pcan_put_dev(dev, -ENOMEM);
			}

			pfl->count = mfl.count;
			err = pcanfd_ioctl_get_filters(dev, pfl);

			/* copy the count and the filter received */
			l = sizeof(struct pcanfd_msg_filters_0) +
				pfl->count * sizeof(struct pcanfd_msg_filter);

			if (copy_to_user_rt(user_info, up, pfl, l)) {
				pr_err("%s: %s(): copy_to_user_rt() failure\n",
						DEVICE_NAME, __func__);
				err = -EFAULT;
			}

			pcan_free(pfl);
		} else {
			err = pcanfd_ioctl_get_filters(dev, NULL);
		}
		break;

	case PCANFD_SEND_MSG:
		err = copy_from_user_rt(user_info, &tx.msg, up, sizeof(tx.msg));
		if (err)
			return pcan_put_dev(dev, -EFAULT);

		err = pcanfd_ioctl_send_msg(dev, &tx, ctx);
		break;

	case PCANFD_RECV_MSG:
		err = pcanfd_ioctl_recv_msg(dev, &rx, ctx);
		if (err)
			break;

		err = copy_to_user_rt(user_info, up, &rx.msg, sizeof(rx.msg));
		if (err)
			return pcan_put_dev(dev, -EFAULT);
		break;

	case PCANFD_SEND_MSGS:
		err = handle_pcanfd_send_msgs(dev, up, ctx, user_info);
		break;

	case PCANFD_RECV_MSGS:
		err = handle_pcanfd_recv_msgs(dev, up, ctx, user_info);
		break;

	case PCANFD_GET_AVAILABLE_CLOCKS:
		err = handle_pcanfd_get_av_clocks(dev, up, ctx, user_info);
		break;

	case PCANFD_GET_BITTIMING_RANGES:
		err = handle_pcanfd_get_bittiming_ranges(dev, up, ctx,
								user_info);
		break;

	case PCANFD_GET_OPTION:
		err = handle_pcanfd_get_option(dev, up, ctx, user_info);
		break;

	case PCANFD_SET_OPTION:
		err = handle_pcanfd_set_option(dev, up, ctx, user_info);
		break;

	default:
		pr_err(DEVICE_NAME ": %s(cmd=%u): unsupported cmd "
			"(dir=%u type=%u nr=%u size=%u)\n",
			__func__, cmd,
			_IOC_DIR(cmd), _IOC_TYPE(cmd),
			_IOC_NR(cmd), _IOC_SIZE(cmd));
		err = -ENOTTY;
		break;
	}

	return pcan_put_dev(dev, err);
}

#ifdef CONFIG_XENO_OPT_RTDM_SELECT
#ifdef XENOMAI3
static int pcan_select_rt(struct rtdm_fd *fd,
			  struct xnselector *selector,
			  unsigned int type,
			  unsigned int index)
{
	struct pcanusr *ctx = (struct pcanusr *)rtdm_fd_to_private(fd);
#else
static int pcan_select_rt(struct rtdm_dev_context *context,
			  rtdm_selector_t *selector,
			  enum rtdm_selecttype type,
			  unsigned int index)
{
	struct pcanusr *ctx = (struct pcanusr *)context->dev_private;
#endif
	struct pcandev *dev = pcan_get_dev(ctx);
	int err;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(type=%d, index=%d)\n",
		__func__, type, index);
#endif

	if (!dev)
		return -ENODEV;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME ": %s(%s CAN%u)\n",
		__func__, dev->adapter->name, pcan_idx(dev)+1);
#endif

	switch (type) {

	case RTDM_SELECTTYPE_READ:
		err = rtdm_event_select(&dev->in_event, selector,
					RTDM_SELECTTYPE_READ, index);
		break;

	case RTDM_SELECTTYPE_WRITE:
		err = rtdm_event_select(&dev->out_event, selector,
					RTDM_SELECTTYPE_WRITE, index);
		break;

	default:
		err = -EINVAL;
	}

	return pcan_put_dev(dev, err);
}
#endif /* CONFIG_XENO_OPT_RTDM_SELECT */

#ifdef XENOMAI3
static int pcan_ioctl_nrt(struct rtdm_fd *user_info,
			  IOCTL_REQUEST_TYPE cmd, void *arg)
#else
static int pcan_ioctl_nrt(struct rtdm_dev_context *context,
			  rtdm_user_info_t *user_info,
			  IOCTL_REQUEST_TYPE cmd, void *arg)
#endif
{
	switch (cmd) {
	case PCAN_WRITE_MSG:
	case PCANFD_SEND_MSG:
	case PCANFD_SEND_MSGS:
	case PCAN_READ_MSG:
	case PCANFD_RECV_MSG:
	case PCANFD_RECV_MSGS:
		pr_warn(DEVICE_NAME
			": WARNING[%p] ioctl(%x) called from non RT context!\n",
			rtdm_task_current(), _IOC_NR(cmd));
	default:
		break;
	}

#ifdef XENOMAI3
	return pcan_ioctl_rt(user_info, cmd, arg);
#else
	return pcan_ioctl_rt(context, user_info, cmd, arg);
#endif
}

/* this structure is used in init_module(void) */
#ifdef XENOMAI3
struct rtdm_driver pcandrv_rt = {
	.profile_info = RTDM_PROFILE_INFO(pcan,
					RTDM_CLASS_MISC,
					RTDM_SUBCLASS_PCAN,
					0),

	.context_size = sizeof(struct pcanusr),
	.device_flags = RTDM_NAMED_DEVICE,
	.device_count = 32,
	.ops = {
		.open = pcan_open_nrt,
		.ioctl_rt = pcan_ioctl_rt,
		.ioctl_nrt = pcan_ioctl_nrt,
		.close = pcan_close_nrt,

#ifdef CONFIG_XENO_OPT_RTDM_SELECT
		.select = pcan_select_rt,
#endif
	},
};
#else
struct rtdm_device pcandev_rt = {
	.device_flags = RTDM_NAMED_DEVICE,
	.context_size = sizeof(struct pcanusr),
	.struct_version = RTDM_DEVICE_STRUCT_VER,

	.device_name = "",

	/* Named device instance creation for real-time contexts.
	 * usage is deprecated and should be NULL
	 */
	.open_rt = NULL,

	/* Named device instance creation for non-real-time contexts */
	.open_nrt = pcan_open_nrt,

	.ops = {
		/* usage is deprecated and should be NULL */
		.close_rt = NULL,

		.close_nrt = pcan_close_nrt,
		.ioctl_rt = pcan_ioctl_rt,
		.ioctl_nrt = pcan_ioctl_nrt,

#ifdef CONFIG_XENO_OPT_RTDM_SELECT
		.select_bind = pcan_select_rt,
#endif
	},

	.device_class = RTDM_CLASS_CAN,
	.driver_version = RTDM_DRIVER_VER(PCAN_VERSION_MAJOR,
					  PCAN_VERSION_MINOR,
					  PCAN_VERSION_SUBMINOR),
	.driver_name = "pcan",
	.provider_name = "PEAK-System Technik GmbH",
	.proc_name = pcandev_rt.device_name,
};
#endif
