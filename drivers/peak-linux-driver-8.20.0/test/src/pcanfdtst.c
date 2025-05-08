/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * pcanfdtst.c - a small program to test CAN[FD] transfer to/from PCAN channels.
 *
 * Copyright (C) 2015-2020  PEAK System-Technik GmbH <www.peak-system.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contact: <linux@peak-system.com>
 * Author:  Stephane Grosjean <s.grosjean@peak-system.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/time.h>
#include <sys/stat.h>		/* fstat() */
#include <arpa/inet.h>		/* htonl() */

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <endian.h>

/* if defined, one task is created for each channel given on cmd line.
 * if not defined, select() is used on all devices.
 */
#define ONE_TASK_PER_DEVICE

#ifndef NO_RT
#include <sys/mman.h>		/* mlockall() */

#define RT
#endif

#ifdef ONE_TASK_PER_DEVICE
/* POSIX */
#include <pthread.h>
#include <semaphore.h>
#endif

#ifdef XENOMAI
#include <alchemy/task.h>

#define __printf		rt_printf
#define __fprintf		rt_fprintf
#define __vfprintf		rt_vfprintf

#elif defined(RTAI)

#ifdef ONE_TASK_PER_DEVICE
#include <rtai_posix.h>
#else
#include <rtdm/rtdm.h>

/* overload the select() system call with the RTDM one, while it isn't by 
 * RTAI 5.1. Note that this example doesn't care about the timeout
 */
static inline int rtai_select(int nfds, fd_set *readfds, fd_set *writefds,
			      fd_set *exceptfds, struct timeval *timeout)
{
	nanosecs_rel_t ns = (nanosecs_rel_t )(timeout->tv_sec * 1000000 + timeout->tv_usec);

	return rt_dev_select(nfds, readfds, writefds, exceptfds, ns * 1000);
}

#define __select(n, r, w, e, t)	rtai_select(n, r, w, e, t)
#endif

#define __printf		print_to_screen
#endif

/* if defined BEFORE including libpcanfd.h, tests can be made with using
 * old-CAN API.
 */
/* #define PCANFD_OLD_STYLE_API */

/* if defined, channel is opened with open() + pcanfd_set_init() instead of
 * pcanfd_open()
 */
//#define PCANFD_USES_OPEN_SET_INIT

/* if defined, pcanfd_send_msgs_list() and pcanfd_recv_msgs_list() are used
 * instead of pcanfd_send_msgs() and pcanfd_recv_msgs()
 */
/* #define USES_MSGS_LIST */

/* this defines the maximum count of us. we admit with -T between host time and
 * received message time. If difference is greater, then rx test stops.
 */
#define PCANFD_TS_MAX_DELTA	1000000

#include <libpcanfd.h>

#ifndef __printf
#define __printf		printf
#endif
#ifndef __fprintf
#define __fprintf		fprintf
#endif
#ifndef __vfprintf
#define __vfprintf		vfprintf
#endif
#ifndef __select
#define __select		select
#endif

/* change this to display on larger (or smaller) screens */
#define SCREEN_WIDTH		80

/* number max of /dev/pcan interfaces tested at the same time */
#define TST_DEV_PCAN_MAX	8

/* default pause in us. between two writes/reads.
 * Note that the CAN controller can't write frames quicker than every
 * N/bitrate s., with N = [(128+3)..(128+16+3)]
 * 128 = count of bits for an extended frame format with 8 data bytes
 * 108 = count of bits for an standard frame format with 8 data bytes
 * 16 = maximum of bit stuffing (128 bits frames)
 * 3 = interframe spacing
 *
 * Standard frame format ([(108+3)..(108+15+3)] bits per frame):
 *
 * bitrate	fps		data bps	period	pause_us (measured)
 * 500k		4504/3968	288256/253952	222/252	175/200
 */
#define TST_DEFAULT_PAUSE_US	1000

enum tst_status { NOK, OK, END };
enum tst_seq_mode { FIXD, RAND, INCR, CLK };

static enum {
	TST_MODE_UNKNOWN,
	TST_MODE_TX,
	TST_MODE_RX,
	TST_MODE_GETOPT,
	TST_MODE_SETOPT,
	TST_MODE_REC,
	TST_MODE_NONE
} tst_mode = TST_MODE_UNKNOWN;

static enum log_level {
	QUIET,
	NORMAL,
	VERBOSE,
	DEBUG,
	ALWAYS,
	WARN,
	ERR
} tst_verbose = NORMAL;

static const __u8 tst_dlc2len[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64
};

static const __u8 tst_len2dlc[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,	/* 0 - 8 */
	9, 9, 9, 9,			/* 9 - 12 */
	10, 10, 10, 10,			/* 13 - 16 */
	11, 11, 11, 11,			/* 17 - 20 */
	12, 12, 12, 12,			/* 21 - 24 */
	13, 13, 13, 13, 13, 13, 13, 13,	/* 25 - 32 */
	14, 14, 14, 14, 14, 14, 14, 14,	/* 33 - 40 */
	14, 14, 14, 14, 14, 14, 14, 14,	/* 41 - 48 */
	15, 15, 15, 15, 15, 15, 15, 15,	/* 49 - 56 */
	15, 15, 15, 15, 15, 15, 15, 15	/* 57 - 64 */
};

static const char *tst_txt_status[PCANFD_STATUS_COUNT] = {
	"UNKNOWN",
	"ACTIVE",
	"WARNING",
	"PASSIVE",
	"BUSOFF",
	"RX_EMPTY",
	"RX_OVRFL",
	"TX_EMPTY",
	"TX_OVRFL",
	"BUS_ERR",
	"BUS_LOAD",
};

static int tst_puts_timestamps = 0;
static int tst_check_timestamps = 0;

static FILE *tst_stdlog = NULL;
static char *tst_stdlog_filename = NULL;
//static char *tst_stdlog_filename = "pcanfdtst.log";

static __u32 tst_flags = PCANFD_INIT_TS_HOST_REL;
//static __u32 tst_flags = PCANFD_INIT_TS_DRV_REL;
//static __u32 tst_flags = PCANFD_INIT_TS_DEV_REL;
static int tst_can_id = -1;
static enum tst_seq_mode tst_can_id_seq_mode = RAND;
static int tst_data_length = -1;
static enum tst_seq_mode tst_filler_seq_mode = FIXD;
static int tst_filler = -1;
static enum tst_seq_mode tst_data_length_seq_mode = RAND;
static __u32 tst_pause_us = TST_DEFAULT_PAUSE_US;
static __u32 tst_tx_pause_us = 0;
static __u32 tst_max_loop = 0;
static __u32 tst_max_msgs = 0;
static __u32 tst_bitrate = 0, tst_dbitrate = 0, tst_clock_Hz;
static __u32 tst_sample_pt = 0, tst_dsample_pt = 0;
static __u32 tst_incr_bytes = 0;
static __u32 tst_msgs_count = 1;
static int tst_fdmax = -1;
static int tst_sig_caught= 0;
static int pcan_device_count = 0;
static int pcan_device_opened = 0;
static __u32 tst_tx_packets = 0;
static __u32 tst_tx_bytes = 0;
static __u32 tst_rx_packets = 0;
static __u32 tst_rx_bytes = 0;
static __u32 tst_ts_mode = PCANFD_OPT_HWTIMESTAMP_MAX;
static __u32 tst_ts_base = PCANFD_INIT_TS_HOST_REL;
#ifndef ONE_TASK_PER_DEVICE
static __u32 tst_select_to_ms = 1000;
#endif
static struct timeval tst_start, tst_end;
static __u32 tst_msg_flags = PCANFD_MSG_STD;
static __u32 tst_ids[2];
static int tst_ids_set = 0;
static int tst_max_duration = 0;
static int tst_stop_on_error = 1;
static int tst_stop_on_bus = 0;
static struct pcanfd_option tst_opt = {
	.size = -1,
	.name = PCANFD_OPT_MAX,
	.value = NULL
};
static char *tst_output_fmt = "%t %n %d %i %f %l - %D";

static char *tst_play_file = NULL;
static int tst_play_fd = -1;
static int tst_play_loops = 1;

static char *tst_file_name = NULL;
static int tst_file_fd = -1;

static unsigned int drv_clk_ref = CLOCK_REALTIME;

static int exit_status = 0;

#ifdef ONE_TASK_PER_DEVICE

/* if defined sem_wait() is used instead of sem_trywait() */
//#define RDV_USES_SEM_WAIT

static sem_t rendez_vous;
#endif

static struct pcan_device {
	char *	name;

#ifdef PCANFD_OLD_STYLE_API
	HANDLE	handle;
#endif

#ifdef ONE_TASK_PER_DEVICE
	pthread_t dev_thread;
#endif
	int	fd;
	__u32	flags;
	__u32	clock_Hz;
	__u32	bitrate;
	__u32	sample_pt;
	__u32	dbitrate;
	__u32	dsample_pt;
	int	can_id;
	enum tst_seq_mode	can_id_seq_mode;
	int	filler;
	enum tst_seq_mode	filler_seq_mode;
	int	data_length;
	enum tst_seq_mode	data_length_seq_mode;
	__u32	pause_us;
	__u32	tx_pause_us;
	__u32	incr_bytes;
	__u32	msgs_count;
	__u32	msg_flags;
	__u32	ts_mode;
	__u32	ts_base;
	__u32	features;

	struct pcanfd_option opt;
	struct timeval init_time;

	struct pcanfd_msgs *can_tx_msgs;
	struct pcanfd_msgs *can_rx_msgs;

	struct pcanfd_msg_filters *msg_filters;

	union {
		__u64	seq_counter;
		__u8	seq_data[PCANFD_MAXDATALEN];
	};

	__u32	send_calls;
	__u32	recv_calls;
	__u32	should_resend;

	__u32	tx_packets;
	__u32	tx_bytes;
	__u32	tx_eagain;
	__u32	tx_drv_ovr;
	__u32	tx_ctr_ovr;
	__u8	tx_echo;

	__u32	rx_packets;
	__u32	rx_bytes;
	__u32	rx_seq_chk_error;
	__u32	rx_drv_ovr;
	__u32	rx_ctr_ovr;
	__u8	rx_echo;

	char	adapter_name[PCAN_SF_DATA_MAXLEN];

	int	ids_count;
	__u32	ids[2];

} pcan_device[TST_DEV_PCAN_MAX];

static void signal_handler(int s);
static void usage(char *errmsg);

static void __gettimeofday(struct timeval *tv)
{
	struct timespec tp;

#ifdef RTAI
	/* looks like clock_gettime() POSIX version freezes the system... */
	long long ns = rt_get_real_time_ns();
	//long long ns = rt_get_time_ns();

	tp.tv_sec = ns / 1000000000ULL;
	tp.tv_nsec = ns - (tp.tv_sec * 1000000000ULL);
#else
	clock_gettime(drv_clk_ref, &tp);
#endif
	tv->tv_sec = tp.tv_sec;
	tv->tv_usec = tp.tv_nsec / 1000;
}

static int setup_sig_handler(int signum, void (*f)(int))
{
	struct sigaction act;

	memset(&act, 0, sizeof act);
	sigemptyset(&act.sa_mask);
	act.sa_handler = f;

	/* siagaction() is thread -safe */
	return sigaction(signum, &act, NULL);
}

/*
 * Writes verbose/debug/normal information to an ouput stream file
 * (default is stdout)
 */
static void lprintf(enum log_level lvl, char *fmt, ...)
{
	va_list ap;
	FILE *pfout = tst_stdlog ? tst_stdlog : stdout;

	va_start(ap, fmt);

	if (lvl < ALWAYS)
		switch (tst_verbose) {
		case NORMAL:
			if (lvl == VERBOSE)
				goto lbl_exit;
		case VERBOSE:
			if (lvl == DEBUG)
				goto lbl_exit;
		case DEBUG:
		default:
			break;
		case QUIET:
			goto lbl_exit;
		}
	else
		switch (lvl) {
		case ERR:
			__fprintf(pfout, "*** error: ");
			break;
		case WARN:
			__fprintf(pfout, "*** warning: ");
			break;
		default:
			break;
		}

	if (tst_puts_timestamps) {
		struct timeval tv;
		__gettimeofday(&tv);
		__fprintf(pfout, "%010u.%06u: ",
			(uint )tv.tv_sec, (uint )tv.tv_usec);
	}

	__vfprintf(pfout, fmt, ap);

lbl_exit:
	va_end(ap);
}

static void init_logs(void)
{
	if (tst_stdlog_filename) {
		tst_stdlog = fopen(tst_stdlog_filename, "w");
		lprintf(VERBOSE, "--- start logging ---\n");
		if (tst_stdlog)
			fflush(tst_stdlog);
	}
}

static void exit_logs(void)
{
	lprintf(VERBOSE, "--- stop logging ---\n");

	if (tst_stdlog) {
		fflush(tst_stdlog);
		fclose(tst_stdlog);
		tst_stdlog = NULL;
	}
}

#define DRV_CLK_REF	"/sys/class/pcan/clk_ref"

static void init_clock_ref(void)
{
	FILE *f = fopen(DRV_CLK_REF, "r");
	if (f) {
		fscanf(f, "%u", &drv_clk_ref);
		fclose(f);
	}

	lprintf(VERBOSE, "Driver clock reference is %u\n", drv_clk_ref);
}

/*
 * Initialize all what it should be for the application.
 * This function should taken into account that it can be called several times.
 */
static void init_application(void)
{
	struct pcan_device *pdev;
	__u32 non_blocking_mode_flag = 0;
#ifdef PCANFD_OLD_STYLE_API
	DWORD err;
#else
	int err;
#endif
	int i;

	if (tst_mode == TST_MODE_UNKNOWN)
		usage("No test mode specified on command line");

	if (pcan_device_count <= 0)
		usage("No CAN interface specified on command line");

	/* get driver clock reference first */
	init_clock_ref();

	__gettimeofday(&tst_start);
	lprintf(DEBUG, "Time base: %u.%06u s.\n",
			(__u32 )tst_start.tv_sec, (__u32 )tst_start.tv_usec);

	init_logs();

	/* if a record file is specifed, then open it. Its content defines
	 * the count of messages to tx/rx
	 */
	if (tst_play_file) {

		tst_play_fd = open(tst_play_file, O_RDONLY);
		if (tst_play_fd < 0) {
			perror("Failed to open file of recorded frames");
			usage(NULL);
		}

		if (tst_play_loops > 0) {
			struct stat tst_play_statbuf;
			if (!fstat(tst_play_fd, &tst_play_statbuf)) {
				unsigned long nr = tst_play_statbuf.st_size /
						sizeof(struct pcanfd_msg);
				if (!tst_max_msgs || tst_max_msgs > nr)
					tst_max_msgs = nr;
			}
		}
	} else if (tst_file_name) {
		int flags = (tst_mode == TST_MODE_TX) ? O_RDONLY :
							O_WRONLY|O_CREAT;

		tst_file_fd = open(tst_file_name, flags, 0644);
		if (tst_file_fd < 0) {
			perror("Failed to open file for transfer");
			usage(NULL);
		}
	}

	lprintf(VERBOSE, "start opening %d devices:\n", pcan_device_count);

#ifdef ONE_TASK_PER_DEVICE
	lprintf(DEBUG, "(running threaded version)\n");
#else
	/* open devices in non-blocking mode */
	non_blocking_mode_flag |= OFD_NONBLOCKING;
#endif

	tst_fdmax = -1;
	pcan_device_opened = 0;
	for (pdev = &pcan_device[i = 0]; i < pcan_device_count; i++, pdev++) {
		struct pcan_extra_params extra_params;
		struct pcanfd_msg *pcan_msg;
		int tmp;

		pdev->fd = -1;
		pdev->features = 0;
		*pdev->adapter_name = '\0';

		/* init the struct msgs for the device */
		pdev->can_tx_msgs = malloc(sizeof(struct pcanfd_msgs) +
			pdev->msgs_count * sizeof(struct pcanfd_msg));

		if (!pdev->can_tx_msgs) {
			lprintf(ERR, "memory allocation failure!");
			exit(1);
		}

		pdev->can_rx_msgs = malloc(sizeof(struct pcanfd_msgs) +
			pdev->msgs_count * sizeof(struct pcanfd_msg));

		if (!pdev->can_rx_msgs) {
			lprintf(ERR, "memory allocation failure!");
			exit(1);
		}

		pdev->msg_filters = malloc(sizeof(struct pcanfd_msg_filters) +
			1 * sizeof(struct pcanfd_msg_filter));

		if (!pdev->msg_filters) {
			lprintf(ERR, "memory allocation failure!");
			exit(1);
		}
		pdev->msg_filters->count = 1;

		pdev->flags |= non_blocking_mode_flag;

		switch (tst_mode) {
		case TST_MODE_REC:
			pdev->fd = open(pdev->name, O_WRONLY|O_CREAT, 00666);
			break;
		default:

			/* sanitize */
			if (!pdev->dsample_pt)
				pdev->dsample_pt = pdev->sample_pt;

			/* check some args */
#ifdef PCANFD_OLD_STYLE_API
			pdev->handle = LINUX_CAN_Open(pdev->name, O_RDWR);
			if (pdev->handle) {

				err = CAN_Init(pdev->handle,
						pdev->bitrate,	/* BTR0BTR1 format only! */
						(pdev->flags & PCANFD_MSG_EXT) ?
							CAN_INIT_TYPE_EX :
							CAN_INIT_TYPE_ST);
				if (err) {
					lprintf(ERR, "failed to init \"%s\" "
						"to BTR0BTR1=%04xh (err %d)\n",
						pdev->name, pdev->bitrate, err);
					continue;
				}

				pdev->fd = LINUX_CAN_FileHandle(pdev->handle);
			}

#elif defined(PCANFD_USES_OPEN_SET_INIT)
			pdev->fd = open(pdev->name, O_RDWR |
					((pdev->flags & OFD_NONBLOCKING) ? \
					O_NONBLOCK : 0));

			if (pdev->fd >= 0 && pdev->bitrate) {
				struct pcanfd_init is;

				memset(&is, '\0', sizeof(is));
				is.clock_Hz = pdev->clock_Hz;
				is.nominal.bitrate = pdev->bitrate;
				is.nominal.sample_point = pdev->sample_pt;

				if (pdev->dbitrate) {
					is.flags |= PCANFD_INIT_FD;
					is.data.bitrate = pdev->bitrate;
					is.data.sample_point = pdev->dsample_pt;
				}

				pcanfd_set_init(pdev->fd, &is);
			}
#elif 1
			/* sanitize */
			if (pdev->flags & OFD_CLOCKHZ)
				pdev->flags |= OFD_DBITRATE;

			if (pdev->flags & OFD_DBITRATE)
				pdev->flags |= OFD_SAMPLEPT;

			if (pdev->flags & OFD_SAMPLEPT)
				pdev->flags |= OFD_BITRATE;

			lprintf(VERBOSE,
				"opening \"%s\" with flags=%08xh "
				"bitrate=%u bps sample_pt=%d "
				"dbitrate=%u bps dsample_pt=%d "
				"clock=%u Hz\n",
				pdev->name, pdev->flags,
				pdev->bitrate, pdev->sample_pt,
				pdev->dbitrate, pdev->dsample_pt,
				pdev->clock_Hz);

			pdev->fd = pcanfd_open(pdev->name, pdev->flags,
						pdev->bitrate,
						pdev->sample_pt,
						pdev->dbitrate,
						pdev->dsample_pt,
						pdev->clock_Hz);
#else
			/* simple example:
			 * Nominal BRP=5 TSEG1=1 TSEG2=5 SJW=2
			 * Data BRP=2 TSGE1=1 TSEG2=3 SJW=1
			 * Clock = 20MHz
			 */
			pdev->flags &= ~(OFD_BTR0BTR1|OFD_SAMPLEPT);
			pdev->flags |= OFD_BITRATE|OFD_DBITRATE|OFD_BRPTSEGSJW;
			pdev->flags |= OFD_CLOCKHZ;

			pdev->fd = pcanfd_open(pdev->name, pdev->flags,
						8, 29, 10, 1,
						5, 11, 4, 1,
						80000000);
#endif
			break;
		}

		if (pdev->fd < 0) {
			lprintf(ERR, "failed to open \"%s\" (err %d)\n",
				pdev->name, pdev->fd);
			continue;
		}

		if (pdev->ts_mode != PCANFD_OPT_HWTIMESTAMP_MAX) {

			/* (pdev->features & PCANFD_FEATURE_HWTIMESTAMP) */
			err = pcanfd_set_option(pdev->fd,
						PCANFD_OPT_HWTIMESTAMP_MODE,
						&pdev->ts_mode,
						sizeof(pdev->ts_mode));
			if (err)
				lprintf(ERR,
				    "error %d while setting TS mode to %u\n",
				    err, pdev->ts_mode);
		}

		__gettimeofday(&pdev->init_time);

		pcan_device_opened++;

		if (pdev->fd > tst_fdmax)
			tst_fdmax = pdev->fd;

		lprintf(VERBOSE, "\"%s\" opened (fd=%d)\n",
						pdev->name, pdev->fd);

		/* init tx messages area (first one indeed) */
		pcan_msg = pdev->can_tx_msgs->list;
		if (pdev->can_id_seq_mode == FIXD)
			pcan_msg->id = (__u32 )pdev->can_id;

		pdev->can_tx_msgs->count = pdev->msgs_count;

		/* only for tx tests */
		pcan_msg->type = PCANFD_TYPE_CAN20_MSG;
		pcan_msg->flags = pdev->msg_flags;

		if (pdev->flags & PCANFD_INIT_FD)
			pcan_msg->type = PCANFD_TYPE_CANFD_MSG;

		/* init rx msgs area */
		pdev->can_rx_msgs->count = pdev->msgs_count;

		/* run next code according to the running test */
		switch (tst_mode) {
		case TST_MODE_REC:
			continue;

		default:
			break;
		}

		/* setup filter mask */
		if (pdev->ids_count > 0) {
			struct pcanfd_msg_filter fm = {
				.id_from = pdev->ids[0],
				.id_to = pdev->ids[1],
				.msg_flags = pdev->msg_flags,
			};

			if (pdev->ids_count == 1)
				fm.id_to = fm.id_from;

			lprintf(VERBOSE, "adding filter [0x%x..0x%x] flg=%xh "
					"to \"%s\"\n",
				fm.id_from, fm.id_to, fm.msg_flags, pdev->name);

			pdev->msg_filters->list[0] = fm;
			err = pcanfd_add_filters(pdev->fd, pdev->msg_filters);
			if (err)
				lprintf(ERR,
					"error %d while adding filter "
					"[%xh..%xh]\n",
					err, fm.id_from, fm.id_to);
		} else {
			/* useles when opening, for tests only */
			err = pcanfd_del_filters(pdev->fd);
			if (err)
				lprintf(ERR,
					"error %d while deleting filters\n",
					err);
		}

		/* get channel features */
		err = pcanfd_get_option(pdev->fd,
					PCANFD_OPT_CHANNEL_FEATURES,
					&pdev->features,
					sizeof(pdev->features));
		if (err < 0) {
			lprintf(ERR,
			    	"error %d while reading channel features\n",
				err);

		} else {
			lprintf(DEBUG, "channel features: %08xh\n",
				pdev->features);
		}

		/* don't request any ECHO if hw can't */
		if (!(pdev->features & PCANFD_FEATURE_ECHO))
			pcan_msg->flags &= ~PCANFD_MSG_ECHO;

		extra_params.nSubFunction = SF_GET_ADAPTERNAME;
		err = pcan_set_extra_params(pdev->fd, &extra_params);
		if (err) {
			strncpy(pdev->adapter_name, "PCAN interface",
				sizeof(extra_params.func.ucDevData));
			lprintf(ERR,
				"error %d while reading device name\n",
				err);
		} else {
			memcpy(pdev->adapter_name, extra_params.func.ucDevData,
			       sizeof(extra_params.func.ucDevData));
		}

		/* if command line defines any tx pause value, set the
		 * corresponding option (if supported)
		 */
		if (pdev->tx_pause_us) {

			err = pcanfd_set_option(pdev->fd,
						PCANFD_OPT_IFRAME_DELAYUS,
						&pdev->tx_pause_us,
						sizeof(pdev->tx_pause_us));
			if (err)
				lprintf(ERR,
				    "error %d while setting TX_PAUSE[%u µs]\n",
				    err, pdev->tx_pause_us);
		}

		/* Since pcanfdtst is used for test, set LINGER to AUTO so that
		 * close() will wait until Tx fifo is empty before closing the
		 * device. This is useful when txqsize is large, for example.
		 */
		tmp = PCANFD_OPT_LINGER_AUTO;
		pcanfd_set_option(pdev->fd, PCANFD_OPT_LINGER,
				  &tmp, sizeof(int));
	}

	if (!pcan_device_opened)
		usage("No pcan device is opened. Exiting application");

	lprintf(DEBUG, "tst_fdmax=%d\n", tst_fdmax);

	tst_tx_packets = 0;
	tst_tx_bytes = 0;
	tst_rx_packets = 0;
	tst_rx_bytes = 0;

	tst_sig_caught = 0;
	setup_sig_handler(SIGUSR1, signal_handler);
	setup_sig_handler(SIGUSR2, signal_handler);
	setup_sig_handler(SIGHUP, signal_handler);
	setup_sig_handler(SIGTERM, signal_handler);
	setup_sig_handler(SIGINT, signal_handler);

	if (tst_max_duration > 0) {
		setup_sig_handler(SIGALRM, signal_handler);
		alarm(tst_max_duration);
	}

	srand(time(NULL));
}

/*
 * Counter-part of init_application().
 * This function has to close/free/release everything that has been
 * opened/allocated/taken during the application life cycle.
 * Moreover, it has to taken into account that it might be called several times.
 */
static void close_application(void)
{
	struct pcan_device *pdev;
	int i, j;

	__gettimeofday(&tst_end);

#ifdef XENOMAI
	rt_print_flush_buffers();
#endif

	for (pdev = &pcan_device[i = 0]; i < pcan_device_count; i++, pdev++) {

		if (pdev->fd >= 0) {

			switch (tst_mode) {
			case TST_MODE_REC:
				close(pdev->fd);
				pdev->fd = -1;
				break;
			default:
#ifdef PCANFD_OLD_STYLE_API
				lprintf(DEBUG, "\"%s\" closed (handle=%p)\n",
					pdev->name, pdev->handle);
				CAN_Close(pdev->handle);
				pdev->handle = NULL;
				pdev->fd = -1;
#else
				lprintf(DEBUG, "\"%s\" closed (fd=%d)\n",
					pdev->name, pdev->fd);
				pdev->fd = pcanfd_close(pdev->fd);
#endif
				break;
			}

			switch (tst_mode) {

			case TST_MODE_TX:
			case TST_MODE_REC:
				lprintf(ALWAYS,
					"%s < [packets=%u calls=%u bytes=%u "
					"eagain=%u drv_ovr=%u ctr_ovr=%u]\n",
					pdev->name,
					pdev->tx_packets, pdev->send_calls,
					pdev->tx_bytes,
					pdev->tx_eagain,
					pdev->tx_drv_ovr,
					pdev->tx_ctr_ovr);
				break;
			case TST_MODE_RX:
				lprintf(ALWAYS,
					"%s > [packets=%u calls=%u bytes=%u "
					"seq_err=%u drv_ovr=%u ctr_ovr=%u]\n",
					pdev->name,
					pdev->rx_packets, pdev->recv_calls,
					pdev->rx_bytes,
					pdev->rx_seq_chk_error,
					pdev->rx_drv_ovr,
					pdev->rx_ctr_ovr);
				break;
			case TST_MODE_GETOPT:
			case TST_MODE_SETOPT:
				if (pdev->opt.size >= 0) {
					lprintf(ALWAYS, "%s > [option=%u "
						"size=%d value=[",
						pdev->name,
						pdev->opt.name,
						pdev->opt.size);
					for (j = 0; j < pdev->opt.size; j++)
						lprintf(ALWAYS, "%02x ",
							*(__u8 *)(pdev->opt.value + j));
					lprintf(ALWAYS, "]\n");
				}
				free(pdev->opt.value);
				pdev->opt.value = NULL;
				break;
			default:
				break;
			}
		}
		if (pdev->can_tx_msgs) {
			free(pdev->can_tx_msgs);
			pdev->can_tx_msgs = NULL;
		}
		if (pdev->can_rx_msgs) {
			free(pdev->can_rx_msgs);
			pdev->can_rx_msgs = NULL;
		}
		if (pdev->msg_filters) {
			free(pdev->msg_filters);
			pdev->msg_filters = NULL;
		}
	}

	lprintf(VERBOSE, "all %d devices closed\n", pcan_device_opened);

#ifdef XENOMAI
	rt_print_flush_buffers();
#endif
	if (tst_play_fd >= 0) {
		close(tst_play_fd);
		tst_play_fd = -1;
	}

	if (tst_file_fd >= 0) {
		close(tst_file_fd);
		tst_file_fd = -1;
	}

	exit_logs();
}

/*
 * Do what must be done before exiting the application.
 */
static int exit_application(int err)
{
	close_application();

	if (pcan_device_opened > 0) {
		struct timeval tst_duration;
		unsigned long long total_us, total_bits;

		timersub(&tst_end, &tst_start, &tst_duration);
		total_us = tst_duration.tv_sec * 1000000ULL +
						tst_duration.tv_usec;

		lprintf(ALWAYS, "Test duration: %u.%06u s. ",
			tst_duration.tv_sec, tst_duration.tv_usec);

		switch (tst_mode) {

		case TST_MODE_TX:
			if (tst_tx_bytes > 0) {
				total_bits = tst_tx_bytes * 8 * 1000000ULL;
				lprintf(ALWAYS, "sent frames: %u (%lu bps)",
					tst_tx_packets, total_bits/total_us);
			}
			break;
		case TST_MODE_REC:
			lprintf(ALWAYS, "recorded frames: %u",
				tst_tx_packets);
			break;
		case TST_MODE_RX:
			if (tst_rx_bytes > 0) {
				total_bits = tst_rx_bytes * 8 * 1000000ULL;
				lprintf(ALWAYS, "received frames: %u (%lu bps)",
					tst_rx_packets, total_bits/total_us);
			}
			break;
		default:
			break;
		}

		lprintf(ALWAYS, "\n");

#ifdef XENOMAI
		rt_print_flush_buffers();
#endif
	}

	exit(err);

	/* just to avoid warnings... */
	return err;
}

static void usage(char *errmsg)
{
	if (errmsg)
		fprintf(stderr, "\n%s\n\n", errmsg);

	fprintf(stderr, "Setup CAN[FD] tests between CAN channels over the pcan driver (>= v8.x)\n");
#ifdef ONE_TASK_PER_DEVICE
	fprintf(stderr, "Multi-threaded POSIX version\n");
#else
	fprintf(stderr, "Monotask version using select() system call\n");
#endif
	fprintf(stderr, "\nWARNING\n");
	fprintf(stderr, "\tThis application comes with ABSOLUTELY NO WARRANTY. This is free\n\tsoftware and you are welcome to redistribute it under certain\n\tconditions. For details, see attached COPYING file.\n");
	fprintf(stderr, "\nUSAGE\n");
	fprintf(stderr, "\t$ pcanfdtst MODE [OPTIONS] FILE [FILE...]\n");
	fprintf(stderr, "\nMODE\n");
	fprintf(stderr, "\ttx      generate CAN traffic on the specified CAN interfaces\n");
	fprintf(stderr, "\trx      check CAN traffic received on the specified CAN interfaces\n");
	fprintf(stderr, "\tgetopt  get a specific option value from the given CAN interface(s)\n");
	fprintf(stderr, "\tsetopt  set an option value to the given CAN interface(s)\n");
	fprintf(stderr, "\trec     same as 'tx' but frames are recorded into the given file\n");
	fprintf(stderr, "\nFILE\n");
	fprintf(stderr, "\tFor all modes except 'rec' mode:\n\n");
#ifdef RT
	fprintf(stderr, "\tpcanx         indicate which RT CAN interface is used in the test.\n");
#else
	fprintf(stderr, "\t/dev/pcanx    indicate which CAN interface is used in the test.\n");
#endif
	fprintf(stderr, "\t              Several CAN interfaces can be specified. In that case,\n");
	fprintf(stderr, "\t              each one is opened in non-blocking mode.\n");
	fprintf(stderr, "\n\t'rec' mode only:\n\n");
	fprintf(stderr, "\tfile_name     file path in which frames have to be recorded.\n");
	fprintf(stderr, "\nOPTIONS\n");
	fprintf(stderr, "\t-1 | --one-shot      select one-shot mode\n");
	fprintf(stderr, "\t-a | --accept f-t    add message filter [f...t]\n");
#ifdef PCANFD_OLD_STYLE_API
	fprintf(stderr, "\t-b | --bitrate v     set BTR0BTR1\n");
#else
	fprintf(stderr, "\t-b | --bitrate v     set [nominal] bitrate to \"v\" bps\n");
	fprintf(stderr, "\t     --btr0btr1      bitrates with BTR0BTR1 format\n");
	fprintf(stderr, "\t-B | --brs           set BRS bit in outgoing CAN FD frames\n");
	fprintf(stderr, "\t-c | --clock v       select clock frequency \"v\" Hz\n");
#endif
	fprintf(stderr, "\t-D | --debug         (maybe too) lot of display\n");
#ifndef PCANFD_OLD_STYLE_API
	fprintf(stderr, "\t-d | --dbitrate v    set data bitrate to \"v\" bps\n");
	fprintf(stderr, "\t     --dsample-pt v  define the data bitrate sample point ratio x 10000\n");
	fprintf(stderr, "\t-E | --esi           set ESI bit in outgoing CANFD msgs\n");
	fprintf(stderr, "\t     --echo          tx frame is echoed by the hw into the rx path\n");
	fprintf(stderr, "\t-f | --fd            select CAN-FD ISO mode\n");
	fprintf(stderr, "\t     --fd-non-iso    select CAN-FD non-ISO mode\n");
	fprintf(stderr, "\t-F | --filler v|r|i|c  select how data are filled:\n");
	fprintf(stderr, "\t                     fixed, randomly, incrementally or by the clock\n");
	fprintf(stderr, "\t     --file file     transmit data from/receive data to file\n");
#endif
	fprintf(stderr, "\t-h | --help          display this help\n");
	fprintf(stderr, "\t-i | --id v|r|i      set fixed CAN Id. to \"v\", randomly or incr.\n");
	fprintf(stderr, "\t-is v|r|i            set fixed standard CAN Id \"v\", randomly or incr.\n");
	fprintf(stderr, "\t-ie v|r|i            set fixed extented CAN Id \"v\", randomly or incr.\n");
	fprintf(stderr, "\t-I | --incr v        \"v\"=nb of data bytes to use for increment counter\n");
	fprintf(stderr, "\t-l | --len v|r|i     set fixed CAN dlc \"v\", randomly or incr.\n");
#ifndef PCANFD_OLD_STYLE_API
	fprintf(stderr, "\t-m | --mul v         tx/rx \"v\" msgs at once\n");
#endif
	fprintf(stderr, "\t-M | --max-duration v   define max duration the test should run in s.\n");
	fprintf(stderr, "\t-n v                 send/read \"v\" CAN msgs then stop\n");
	fprintf(stderr, "\t-o | --listen-only   set pcan device in listen-only mode\n");
	fprintf(stderr, "\t     --opt-name v    specify the option name (getopt/setopt modes)\n");
	fprintf(stderr, "\t     --opt-value v   specify the option value (getopt/setopt modes)\n");
	fprintf(stderr, "\t     --opt-size v    specify the option size (getopt/setopt modes)\n");
	fprintf(stderr, "\t-p | --pause-us v    \"v\" us. pause between sys calls (rx/tx def=0/%u)\n", tst_pause_us);
	fprintf(stderr, "\t     --play file     play recorded frames from \"file\" according to MODE\n");
	fprintf(stderr, "\t     --play-forever file same as --play but loop forever on \"file\"\n");
	fprintf(stderr, "\t-P | --tx-pause-us v force a pause of \"v\" us. between each Tx frame\n");
	fprintf(stderr, "\t                     (if hw supports it)\n");
	fprintf(stderr, "\t-q | --quiet         nothing is displayed\n");
	fprintf(stderr, "\t-r | --rtr           set the RTR flag to msgs sent\n");
	fprintf(stderr, "\t     --no-rtr        clear the RTR flag from msgs sent\n");
	fprintf(stderr, "\t-s | --stdmsg-only   don't handle extended msgs\n");
	fprintf(stderr, "\t     --sample-pt v   define the bitrate sample point ratio x 10000\n");
	fprintf(stderr, "\t-S | --stop-on-bus v stop when bus state > v (default: %u)\n", tst_stop_on_bus);
#ifndef ONE_TASK_PER_DEVICE
	fprintf(stderr, "\t-t | --timeout-ms v  wait \"v\" ms. for events (select() system call)\n");
#endif
	fprintf(stderr, "\t-T | --check-ts      check host vs. driver timestatmps, stop if wrong\n");
	fprintf(stderr, "\t     --ts-base v     set timestamp base [0..2]\n");
	fprintf(stderr, "\t     --ts-mode v     set hw timestamp mode to v (hw dependant)\n");
	fprintf(stderr, "\t-u | --bus-load      get bus load notifications from the driver\n");
	fprintf(stderr, "\t-v | --verbose       things are (very much) explained\n");
	fprintf(stderr, "\t-w | --with-ts       logs are prefixed with time of day (s.us)\n");
	fprintf(stderr, "\t+FORMAT              output line format:\n");
	fprintf(stderr, "\t                     %%t  timestamp (s.us format)\n");
	fprintf(stderr, "\t                     %%d  direction (< or >)\n");
	fprintf(stderr, "\t                     %%n  device node name\n");
	fprintf(stderr, "\t                     %%i  CAN Id. (hex format)\n");
	fprintf(stderr, "\t                     %%f  flags\n");
	fprintf(stderr, "\t                     %%l  data length\n");
	fprintf(stderr, "\t                     %%D  data bytes\n");
	fprintf(stderr, "\t                     (default format is: \"%s\")\n",
		tst_output_fmt);

	fprintf(stderr, "\n");
	exit_application(2);
}

/*
 * strtounit(argv, "kM");
 * strtounit(argv, "ms");
 */
static unsigned long strtounit(const char *str, const char *units)
{
	char *endptr;
	__u32 m = 1;

	__u32 v = strtoul(str, &endptr, 0);
	if (*endptr) {
		if (units) {
			const char *pu;

			/* might not be invalid if found char is a unit */
			for (pu = units; *pu; pu++) {
				m *= 1000;
				if (*endptr == *pu)
					break;
			}
			if (!*pu)
				usage("Unknown unit character in numeric "
				      "value on command line");

		} else {
			char tmp[512];
			snprintf(tmp, sizeof(tmp),
				"Invalid character in numeric value \"%s\" on "
				"command line", str);
			usage(tmp);
		}
	}

	lprintf(DEBUG, "\"%s\" converted into %u\n", str, v * m);
	return v * m;
}

static int strtoulist(const char *str, int n, __u32 *pl)
{
	int i;

	if (!pl)
		return 0;

	for (i = 0; *str && (i < n); i++) {

		char *endptr;

		__u32 v	= strtoul(str, &endptr, 0);
		lprintf(DEBUG, "i=%d: \"%s\" converted into %u (end='%c')\n",
				i, str, v, *endptr);
		switch (*endptr) {
		case ',':
		case ':':
		case '-':
			endptr++;
		case '\0':
			str = endptr;
			break;
		default:
			return i;
		}

		*pl++ = v;
	}

	return i;
}

/*
 * Linux signal handler.
 *
 * Note that all children do inherit from their parent's signal handler
 */
static void signal_handler(int s)
{
	lprintf(VERBOSE, "%s(signal=%d)\n", __func__, s);

	tst_sig_caught = 0;

	switch (s) {

	case SIGALRM:
		/* -M option has been used: application must exit */
		lprintf(ALWAYS, "\nTest timeout\n");

		/* it's an error if test==RX and we're waiting for N msgs.
		 * Otherwise, it's a normal exit
		 */
		if (1) { //(tst_mode == TST_MODE_RX) && (!tst_max_msgs)) {
			tst_max_loop = 1;
			tst_sig_caught = s;
		}
		break;

	case SIGHUP:
		tst_sig_caught = s;
		close_application();
		init_application();
		break;

	case SIGUSR1:
		exit_logs();
		init_logs();
		tst_sig_caught = s;
		break;

	case SIGUSR2:
		/* end of loop and normal exit code */
		tst_max_loop = 1;
		tst_sig_caught = s;
		break;

	case SIGINT:

		lprintf(VERBOSE, "Stopping tasks...\n");

		/* main task has been INTR by user: lprintf RT tasks to stop:
		 * here, we "simply" set the number max of loops to 1 so that
		 * the INTeRrupted tasks will end their test loops and exit.
		 */
		tst_max_loop = 1;
		tst_sig_caught = s;
		break;

#ifdef XENOMAI
	case SIGXCPU:
		lprintf(VERBOSE,
			"RT task has switched into secondary mode...\n");
		tst_sig_caught = s;
		break;
#endif

	default:
		break;
	}
}

/*
 * Handle a system error in the context of a device.
 */
static enum tst_status handle_errno(int _errno, struct pcan_device *dev)
{
	switch (_errno) {

	case EINTR:
		if (tst_sig_caught) {
			tst_sig_caught = 0;
			return OK;
		}
		lprintf(VERBOSE, "Interrupted!\n");
		break;

	case ENETDOWN:
		lprintf(WARN, "Tx waiting stopped because of BUS-OFF\n");
		break;

	case ETIMEDOUT:
		/* error code returned when Tx task tired to wait for room in
		 * the Tx FIFIO. This generally occurs because the CAN bus
		 * state has changed
		 */
		lprintf(VERBOSE, "WARNING: Tx FIFO ran out of space\n");

		/* return OK to continue trying to write, hoping the bus
		 * state comes back to ACTIVE
		 */
		return OK;

	case EAGAIN:
		if (dev) {
			if (dev->flags & OFD_NONBLOCKING)
				lprintf(ERR,
					"ABNORMAL errno %d when using "
					"select()!\n", _errno);
			else
				lprintf(ERR,
					"ABNORMAL errno %d: system says "
					"task is NOT ABLE to wait!\n", _errno);
			break;
		}

	default:
		lprintf(ERR, "system call failure (errno=%d)\n", _errno);
		break;
	}

	return NOK;
}

static int to_relative_time(struct timeval *ptv, struct timeval *ptb)
{
	if (ptv->tv_sec >= ptb->tv_sec) {
		ptv->tv_sec -= ptb->tv_sec;
		if (ptv->tv_usec >= ptb->tv_usec) {
			ptv->tv_usec -= tst_start.tv_usec;
		} else {
			ptv->tv_sec--;
			ptv->tv_usec += 1000000 - ptb->tv_usec;
		}

		return 1;
	}

	return 0;
}

static enum tst_status do_check_timestamps(struct pcanfd_msg *pcan_msg)
{
	struct timeval now, d;

	if (!(pcan_msg->flags & PCANFD_TIMESTAMP))
		return OK;

	__gettimeofday(&now);

	/* check if message timestamps is "correct", that is:
	 * - not greater than now
	 * - not *very* different than now
	 */
	timersub(&now, &pcan_msg->timestamp, &d);

	if ((d.tv_sec < 0) || (!d.tv_sec && d.tv_usec < 0)) {
		lprintf(WARN, "message timestamp from the future!\n");
		lprintf(WARN, "timestamp = %u.%06u s vs. now = %u.%06u\n",
			pcan_msg->timestamp.tv_sec, pcan_msg->timestamp.tv_usec,
			now.tv_sec, now.tv_usec);
		return NOK;
	}

	if ((d.tv_sec > 0) || (d.tv_usec > PCANFD_TS_MAX_DELTA)) {
		lprintf(WARN, "message timestamp too far from now "
			"(> 0.%u s.)\n", PCANFD_TS_MAX_DELTA);
		return NOK;
	}

	return OK;
}

/*
 * This function displays an event received from the device.
 */
static int putmsg_ts(char *txt, int l, int lmax, struct pcan_device *dev,
			struct pcanfd_msg *pcan_msg, int l1, int l2)
{
	struct timeval now, *ptv;

	__gettimeofday(&now);

	if (pcan_msg->flags & PCANFD_TIMESTAMP)
		ptv = &pcan_msg->timestamp;
	else {
		ptv = &now;

		pcan_msg->timestamp = now;
		pcan_msg->flags |= PCANFD_TIMESTAMP;
		pcan_msg->flags &= ~PCANFD_HWTIMESTAMP;
	}

	/* change to relative time */
	switch (dev->flags & PCANFD_INIT_TS_FMT_MASK) {

	/* use this ts mode to get application relative times */
	case PCANFD_INIT_TS_HOST_REL:
		//to_relative_time(ptv, &tst_start);
		break;

	case PCANFD_INIT_TS_DEV_REL:

		/* if ts is simulated (Tx test case, for ex),
		 * must translate it accoding to the chosen
		 * time base
		 */
		if (ptv == &now) {
			to_relative_time(ptv, &dev->init_time);
			break;
		}

		/* fall through */

	case PCANFD_INIT_TS_DRV_REL:

	/* other cases: display timestamp "as is" */
	default:
		break;
	}

#if 0
	l += snprintf(txt+l, lmax-l, "now=%6u.%06u: ",
			(uint )now.tv_sec, (uint )now.tv_usec);
#endif

	return l + snprintf(txt+l, lmax-l, "%*u%c%0*u",
			(l1) ? l1 : 7,
			(uint )ptv->tv_sec,
			(pcan_msg->flags & PCANFD_HWTIMESTAMP) ?
				'.' : '~',
			(l2) ? l2 : 6,
			(uint )ptv->tv_usec);
}

static int putmsg_id(char *txt, int l, int lmax, struct pcan_device *dev,
			struct pcanfd_msg *pcan_msg, int l1, int l2)
{
	switch (pcan_msg->type) {

	case PCANFD_TYPE_STATUS:

		switch (pcan_msg->id) {

		case PCANFD_ERROR_ACTIVE:
		case PCANFD_ERROR_WARNING:
		case PCANFD_ERROR_PASSIVE:
		case PCANFD_ERROR_BUSOFF:
			l += snprintf(txt+l, lmax-l , "BUS_STATE=%-8s",
					tst_txt_status[pcan_msg->id]);

			if (pcan_msg->flags & PCANFD_ERRCNT) {
				l += snprintf(txt+l, lmax-l, " [Rx:%u Tx:%u]",
					pcan_msg->ctrlr_data[PCANFD_RXERRCNT],
					pcan_msg->ctrlr_data[PCANFD_TXERRCNT]);
			}
			break;
		case PCANFD_RX_EMPTY:
		case PCANFD_RX_OVERFLOW:
		case PCANFD_TX_EMPTY:
		case PCANFD_TX_OVERFLOW:
		case PCANFD_BUS_ERROR:
			if (pcan_msg->flags & PCANFD_ERROR_CTRLR) {
				l += snprintf(txt+l, lmax-l , "CTR_ERR=%-8s",
						tst_txt_status[pcan_msg->id]);
#if 0
				switch (pcan_msg->id) {
				case PCANFD_RX_OVERFLOW:
					dev->rx_ctr_ovr++;
					break;
				case PCANFD_TX_OVERFLOW:
					dev->tx_ctr_ovr++;
					break;
				}
#endif

			} else if (pcan_msg->flags & PCANFD_ERROR_INTERNAL) {
				l += sprintf(txt+l, "DRV_ERR=%-8s",
						tst_txt_status[pcan_msg->id]);
#if 0
				switch (pcan_msg->id) {
				case PCANFD_RX_OVERFLOW:
					dev->rx_drv_ovr++;
					break;
				case PCANFD_TX_OVERFLOW:
					dev->tx_drv_ovr++;
					break;
				}
#endif
			}
			break;
		case PCANFD_BUS_LOAD:
			if (pcan_msg->flags & PCANFD_BUSLOAD) {
				l += snprintf(txt+l, lmax-l, "%s=%u.%02u%%",
					tst_txt_status[pcan_msg->id],
					pcan_msg->ctrlr_data[PCANFD_BUSLOAD_UNIT],
					pcan_msg->ctrlr_data[PCANFD_BUSLOAD_DEC]);
			}
			break;
		default:
			l += snprintf(txt+l, lmax-l, "INV_STATUS=%u",
					pcan_msg->id);
			break;
		}
		break;

	case PCANFD_TYPE_CAN20_MSG:
	case PCANFD_TYPE_CANFD_MSG:

		if (!l1)
			l1 = 8;

		if (pcan_msg->flags & PCANFD_MSG_EXT)
			l += snprintf(txt+l, lmax-l, "%0*x",
					l1, pcan_msg->id);
		else {
			if (l+l1 > lmax)
				l1 = lmax - l;

			memset(txt+l, ' ', l1);

			if (!l2)
				l2 = 3;

			l += l1 - l2;
			l += snprintf(txt+l, lmax-l, "%0*x", //     %0*x",
						l2, pcan_msg->id);
		}
		break;

	default:
		l += snprintf(txt+l, lmax-l ,
				"%u (Unknown message discarded)",
				pcan_msg->type);
	}

	return l;
}

static int putmsg_flags(char *txt, int l, int lmax, struct pcan_device *dev,
			struct pcanfd_msg *pcan_msg, int l1, int l2)
{
	switch (pcan_msg->type) {

	case PCANFD_TYPE_CAN20_MSG:
	case PCANFD_TYPE_CANFD_MSG:

		l += snprintf(txt+l, lmax-l, "%c%c%c%c%c",
			(pcan_msg->flags & PCANFD_MSG_RTR) ? 'r' : '.',
			(pcan_msg->flags & PCANFD_MSG_EXT) ? 'x' : '.',
			(pcan_msg->flags & PCANFD_MSG_ECHO) ? 'e' :
				(pcan_msg->flags & PCANFD_MSG_SLF) ? 's' : '.',
			(pcan_msg->flags & PCANFD_MSG_BRS) ? 'b' : '.',
			(pcan_msg->flags & PCANFD_MSG_ESI) ? 'i' :
				(pcan_msg->flags & PCANFD_MSG_SNG) ? 's' : '.');
		break;

	default:
		break;
	}

	return l;
}

static int putmsg_len(char *txt, int l, int lmax, struct pcan_device *dev,
			struct pcanfd_msg *pcan_msg, int l1, int l2)
{
	int len = pcan_msg->data_len;

	switch (pcan_msg->type) {

	case PCANFD_TYPE_CAN20_MSG:
		if (len == PCANFD_CAN20_MAXDATALEN) {
			__u8 dlc = pcanfd_msg_read_dlc(pcan_msg);
			if (dlc > len)
				len = dlc;
		}

		/* fallthrough; */
	case PCANFD_TYPE_CANFD_MSG:
		l += snprintf(txt+l, lmax-l, "%*u", l1 ? l1 : 2, len);
		break;

	default:
		break;
	}

	return l;
}

static int putmsg_data(enum log_level ll, char *txt, int l, int lmax,
			struct pcan_device *dev,
			struct pcanfd_msg *pcan_msg, int l1, int l2)
{
	int i, li, data_bytes_per_line;

	switch (pcan_msg->type) {

	case PCANFD_TYPE_CAN20_MSG:
	case PCANFD_TYPE_CANFD_MSG:

		li = l;

		/* compute room left on line for data bytes */
		data_bytes_per_line = (lmax - li) / 3;
		for (i = 0; i < pcan_msg->data_len; ) {
			if (!(i % data_bytes_per_line))
				while (l < li)
					txt[l++] = ' ';

			l += snprintf(txt+l, lmax-l,
					"%02x ", pcan_msg->data[i]);

			if (++i >= pcan_msg->data_len)
				break;

			if (!(i % data_bytes_per_line)) {
				lprintf(ll, "%s\n", txt);
				l = 0;
			}
		}
		break;

	default:
		break;
	}

	return l;
}

static enum tst_status putmsg(struct pcan_device *dev, char dir,
					struct pcanfd_msg *pcan_msg)
{
	char txt[SCREEN_WIDTH+1];
	const int lmax = sizeof(txt); /* snprintf() 'size' arg includes EOL */
	enum log_level ll = NORMAL;
	char *pf;
	int l, l1 = 0, l2 = 0;
	enum { IDLE, IN_FMT, IN_L1, IN_L2 } state = IDLE;

	for (pf = tst_output_fmt, l = 0; *pf; ) {

		switch (state) {
		case IDLE:
			if (*pf == '%') {
				l1 = 0;
				l2 = 0;
				state = IN_FMT;
				break;
			}
			if (l < lmax)
				txt[l++] = *pf;
			txt[l] = '\0';
			break;

		case IN_FMT:
			switch (*pf) {
			case 't':
				l = putmsg_ts(txt, l, lmax,
						dev, pcan_msg, l1, l2);
				state = IDLE;
				break;
			case 'd':
				l += snprintf(txt+l, lmax-l, "%*c",
						(l1) ? l1 : 1, dir);
				state = IDLE;
				break;
			case 'n':
				if (l1)
					l += snprintf(txt+l, lmax-l,
							"%*s", l1, dev->name);
				else
					l += snprintf(txt+l, lmax-l,
							"%s", dev->name);
				state = IDLE;
				break;
			case 'i':
				l = putmsg_id(txt, l, lmax,
						dev, pcan_msg, l1, l2);
				state = IDLE;
				break;
			case 'f':
				l = putmsg_flags(txt, l, lmax,
						dev, pcan_msg, l1, l2);
				state = IDLE;
				break;
			case 'l':
				l = putmsg_len(txt, l, lmax,
						dev, pcan_msg, l1, l2);
				state = IDLE;
				break;
			case 'D':
				l = putmsg_data(ll, txt, l, lmax,
						dev, pcan_msg, l1, l2);
				state = IDLE;
				break;
			case '.':
				if (!l2) {
					state = IN_L2;
					break;
				}
			default:
				if (!l1 && (*pf >= '0' && *pf <= '9')) {
					l1 = *pf - '0';
					state = IN_L1;
					break;
				}
				if (l < lmax)
					txt[l++] = *pf;
				txt[l] = '\0';
				break;
			}
			break;

		case IN_L1:
			if (*pf >= '0' && *pf <= '9') {
				l1 = (l1 * 10) + (*pf - '0');
				break;
			}
			/* all other chars: loop in IN_FMT */
			state = IN_FMT;
			continue;

		case IN_L2:
			if (*pf >= '0' && *pf <= '9') {
				l2 = (l2 * 10) + (*pf - '0');
				break;
			}
			/* all other chars: loop in IN_FMT */
			state = IN_FMT;
			continue;
		}

		pf++;
	}

	lprintf(ll, "%s\n", txt);

	if (tst_check_timestamps)
		return do_check_timestamps(pcan_msg);

	return OK;
}

static enum tst_status init_tx_msg(struct pcan_device *dev,
				   struct pcanfd_msg *tx_msg)
{
	int i;
	__u32 v;

	__gettimeofday(&tx_msg->timestamp);

	tx_msg->flags |= PCANFD_TIMESTAMP;
	tx_msg->flags &= ~PCANFD_HWTIMESTAMP;

	if (tst_play_fd >= 0) {
		i = read(tst_play_fd, tx_msg, sizeof(*tx_msg));

		/* EOF: reloop if forever */
		if (!i && !tst_play_loops) {
			lseek(tst_play_fd, 0, SEEK_SET);
			i = read(tst_play_fd, tx_msg, sizeof(*tx_msg));
		}

		if (i != sizeof(*tx_msg)) {
			lprintf(ERR, "Failed to read next frame from record "
				     "file (errno %s)\n", errno);
			return handle_errno(-errno, dev);
		}

		return OK;
	}

	if (tst_file_fd >= 0) {
		i = read(tst_file_fd, tx_msg->data, dev->data_length);

		/* EOF ? */
		if (!i) {
			lprintf(DEBUG, "EOF!\n");
			return NOK;
		}

		if (i < 0) {
			lprintf(ERR, "Failed to read next frame from "
				     "file (errno %s)\n", errno);
			return handle_errno(-errno, dev);
		}

		tx_msg->id = dev->can_id;
		tx_msg->data_len = i;

		if (dev->msg_flags & PCANFD_MSG_EXT)
			tx_msg->id &= CAN_MAX_EXTENDED_ID;
		else
			tx_msg->id &= CAN_MAX_STANDARD_ID;

		return OK;
	}

	memset(tx_msg->data, '\0', sizeof(tx_msg->data));

	switch (dev->can_id_seq_mode) {
	case FIXD:
		tx_msg->id = dev->can_id;
		break;
	case RAND:
		tx_msg->id = rand();
		if (dev->msg_flags & PCANFD_MSG_EXT)
			tx_msg->id &= CAN_MAX_EXTENDED_ID;
		else
			tx_msg->id &= CAN_MAX_STANDARD_ID;
		break;
	case INCR:
		tx_msg->id = dev->can_id++;
		if (dev->msg_flags & PCANFD_MSG_EXT)
			dev->can_id &= CAN_MAX_EXTENDED_ID;
		else
			dev->can_id &= CAN_MAX_STANDARD_ID;
		break;
	default:
		break;
	}

	if (dev->msg_flags & PCANFD_MSG_EXT)
		tx_msg->id &= CAN_MAX_EXTENDED_ID;
	else
		tx_msg->id &= CAN_MAX_STANDARD_ID;

	switch (dev->data_length_seq_mode) {
	case RAND:
		tx_msg->data_len = rand();
		if (tx_msg->type == PCANFD_TYPE_CANFD_MSG)
			tx_msg->data_len %= PCANFD_CANFD_MAXDATALEN+1;
		else
			tx_msg->data_len %= PCANFD_CAN20_MAXDATALEN+1;
		break;
	default:
		tx_msg->data_len = dev->data_length;

		if (tx_msg->type == PCANFD_TYPE_CAN20_MSG) {
			if (tx_msg->data_len > PCANFD_CAN20_MAXDATALEN) {
				pcanfd_msg_store_dlc(tx_msg, tx_msg->data_len);
				tx_msg->data_len = PCANFD_CAN20_MAXDATALEN;
			}
		}
		break;
	}

	if (dev->incr_bytes) {
		__u64 can_counter;

		can_counter = htole64(dev->seq_counter);
		memcpy(tx_msg->data, &can_counter, dev->incr_bytes);

		dev->seq_counter++;

		if (tx_msg->data_len < dev->incr_bytes)
			tx_msg->data_len = dev->incr_bytes;
	}

	switch (dev->filler_seq_mode) {
	case FIXD:
	case INCR:
		memset(tx_msg->data + dev->incr_bytes, dev->filler,
				tx_msg->data_len - dev->incr_bytes);
		if (dev->filler_seq_mode != INCR)
			break;
		dev->filler++;
		break;
	case RAND:
		for (i = dev->incr_bytes; i < tx_msg->data_len; i++)
			tx_msg->data[i] = (__u8)rand();
		break;
	case CLK:
		v = htonl(tx_msg->timestamp.tv_sec);
		memcpy(tx_msg->data, &v, 4);
		v = htonl(tx_msg->timestamp.tv_usec);
		memcpy(tx_msg->data+4, &v, 4);
		tx_msg->data_len = 8;
		break;
	}

	if (tx_msg->flags & PCANFD_MSG_ECHO) {
		tx_msg->ctrlr_data[PCANFD_ECHOID] = dev->tx_echo;

		dev->tx_echo++;
		dev->tx_echo &= 0x7f;
	}

	if (dev->data_length_seq_mode == INCR) {
		if (tx_msg->type == PCANFD_TYPE_CANFD_MSG) {
			i = tst_len2dlc[tx_msg->data_len];
			dev->data_length = tst_dlc2len[(i + 1) % 16];
		} else {
			dev->data_length = (tx_msg->data_len + 1) %
				(PCANFD_CAN20_MAXDATALEN+1);
		}
	}

	return OK;
}

/*
 * This function handles TX test, according to arguments passed on command line
 */
static enum tst_status handle_tx_tst(struct pcan_device *dev)
{
	struct pcanfd_msg *pcan_msg = dev->can_tx_msgs->list;
	int msgs_to_send;
	int i, err = 0, m;

	if (!dev->should_resend) {

		if (init_tx_msg(dev, pcan_msg) != OK)
			return NOK;

		dev->can_tx_msgs->count = dev->msgs_count;

		/* fill the entire list with a copy of the 1st msg */
		for (i = 1; i < dev->msgs_count; i++)
			memcpy(dev->can_tx_msgs->list + i,
				pcan_msg, sizeof(struct pcanfd_msg));
	} else {

		/* should_resend contains the number of msgs to send next to
		 * a Tx fifo full event, when we tried to send more msgs at once
		 * than tx fifo was able to save. Therefore, one more loop is
		 * needed to complete the whole test.
		 */
		dev->can_tx_msgs->count = dev->should_resend;
	}

	msgs_to_send = dev->can_tx_msgs->count;

	switch (tst_mode) {

	case TST_MODE_REC:
		for (i = 0; i < dev->msgs_count; i++) {
			err = write(dev->fd,
				dev->can_tx_msgs->list + i,
				sizeof(struct pcanfd_msg));

			lprintf(DEBUG, "write(%d, "
				"msg id=%xh flags=%08xh len=%u) returns %d\n",
				dev->fd,
				dev->can_tx_msgs->list[i].id,
				dev->can_tx_msgs->list[i].flags,
				dev->can_tx_msgs->list[i].data_len,
				err);

			if (err != sizeof(pcan_msg))
				break;
		}
		break;

	default:
		if (dev->msgs_count > 1) {
#ifndef USES_MSGS_LIST

			err = pcanfd_send_msgs(dev->fd, dev->can_tx_msgs);
			lprintf(DEBUG,
				"pcanfd_send_msgs(%d, %u) returns %d "
				"(msgs count=%d)\n",
				dev->fd, msgs_to_send, err,
				dev->can_tx_msgs->count);

			if (!err) {
				dev->should_resend = msgs_to_send -
						dev->can_tx_msgs->count;
#else
			err = pcanfd_send_msgs_list(dev->fd,
					    dev->can_tx_msgs->count,
					    dev->can_tx_msgs->list);
			lprintf(DEBUG,
				"pcanfd_send_msgs_list(%d, %u) returns %d\n",
				dev->fd, dev->can_tx_msgs->count, err);

			if (err > 0) {
				dev->should_resend = msgs_to_send - err;
#endif
			}
		} else {

#ifdef PCANFD_OLD_STYLE_API
			TPCANRdMsg msgv1;

			if (!pcanfd_to_msg(&msgv1, pcan_msg)) {
				lprintf(WARN,
					"CAN-FD messages can't be sent!\n");
				return NOK;
			}

			err = -CAN_Write(dev->handle, &msgv1.Msg);
			lprintf(DEBUG,
				"CAN_Write(%p) returns %d\n", dev->handle, err);
#else
			err = pcanfd_send_msg(dev->fd, pcan_msg);
			lprintf(DEBUG, "pcanfd_send_msg(%d, "
				"msg id=%xh flags=%08xh len=%u) returns %d\n",
				dev->fd,
				pcan_msg->id, pcan_msg->flags,
				pcan_msg->data_len, err);
#endif
			if (!err)
				dev->should_resend = 0;
			else
				dev->can_tx_msgs->count = 0;
		}

		if (err == -EAGAIN) {
			/* multi-msgs mode:
			 * the file descriptor has been opened in
			 * non-blocking mode, but there is not enough
			 * room to store all the
			 * 'dev->can_tx_msgs->count' msgs.
			 * Have to wait next select(write) ok to
			 * resend it...
			 *
			 * mono-msg mode:
			 * file descriptor opened in non-blocking mode.
			 * Normally, this could not happened because of
			 * select()... *But* handle_single_device() can
			 * be called when only the readfd has been
			 * signaled, when in TX_MODE.
			 */
			dev->tx_eagain++;
			dev->should_resend = msgs_to_send;

			/* this is actually not an error */
			err = 0;
		}
		break;
	}

	if (err < 0)
		return handle_errno(-err, dev);

	/* keep the count of msgs really sent */
	m = dev->can_tx_msgs->count;

	dev->send_calls++;
	dev->tx_packets += m;
	dev->tx_bytes += m * pcan_msg->data_len;

	tst_tx_packets += m;
	tst_tx_bytes += m * pcan_msg->data_len;

	/* display the msgs really sent */
	for (i = 0; i < m; i++)
		if (putmsg(dev, '<', pcan_msg) == NOK)
			return NOK;

#ifdef XENOMAI
	/* periodically flush printf buffer */
	if (!(tst_tx_bytes % 400))
		rt_print_flush_buffers();
#endif

	/* need at least one more loop */
	if (dev->should_resend && tst_max_loop)
		tst_max_loop++;

	return OK;
}

static enum tst_status handle_rx_tst_status(struct pcan_device *dev,
						struct pcanfd_msg *can_msg)
{
	lprintf(DEBUG, "%s(%d)\n", __func__, can_msg->type);

	//if (tst_verbose >= NORMAL)
	putmsg(dev, '>', can_msg);

	switch (can_msg->type) {

	case PCANFD_TYPE_STATUS:

		switch (can_msg->id) {

		case PCANFD_ERROR_WARNING:
		case PCANFD_ERROR_PASSIVE:
		case PCANFD_ERROR_BUSOFF:
			if (can_msg->id > tst_stop_on_bus)
				return NOK;
			break;

		case PCANFD_RX_EMPTY:
		case PCANFD_RX_OVERFLOW:
		case PCANFD_TX_EMPTY:
		case PCANFD_TX_OVERFLOW:
		case PCANFD_BUS_ERROR:
			if (can_msg->flags & PCANFD_ERROR_CTRLR) {
				switch (can_msg->id) {
				case PCANFD_RX_OVERFLOW:
					dev->rx_ctr_ovr++;
					break;
				case PCANFD_TX_OVERFLOW:
					dev->tx_ctr_ovr++;
					break;
				}

			} else if (can_msg->flags & PCANFD_ERROR_INTERNAL) {
				switch (can_msg->id) {
				case PCANFD_RX_OVERFLOW:
					dev->rx_drv_ovr++;
					break;
				case PCANFD_TX_OVERFLOW:
					dev->tx_drv_ovr++;
					break;
				}
			}
			break;
		}
		break;
	}

	/* handle some errors as FATAL when file transfering */
	if (tst_file_fd >= 0 && tst_stop_on_error) {
		switch (tst_mode) {
		case TST_MODE_RX:
			if (dev->rx_ctr_ovr || dev->rx_drv_ovr)
				return NOK;
			break;
		case TST_MODE_TX:
			if (dev->tx_ctr_ovr || dev->tx_drv_ovr)
				return NOK;
			break;
		default:
			break;
		}
	}

	return OK;
}

static enum tst_status cmp_rx_msg(struct pcan_device *dev,
				  struct pcanfd_msg *rx_msg)
{
	int rx_msg_data_len;

	if (tst_play_fd >= 0) {
		struct pcanfd_msg pcan_msg;

		int l = read(tst_play_fd, &pcan_msg, sizeof(pcan_msg));

		/* EOF: reloop if forever */
		if (!l && !tst_play_loops) {
			lseek(tst_play_fd, 0, SEEK_SET);
			l = read(tst_play_fd, &pcan_msg, sizeof(pcan_msg));
		}

		if (l != sizeof(pcan_msg)) {
			lprintf(ERR, "failed to read next frame from record "
				     "file (errno %s)\n", errno);

			return handle_errno(-errno, dev);
		}

		/* do some checks on data read from the file */
		if (pcan_msg.data_len > 64) {
			lprintf(WARN, "data_len=%d... "
				"WTF does this record file come from?\n",
				pcan_msg.data_len);
			return NOK;
		}

		l = tst_dlc2len[tst_len2dlc[pcan_msg.data_len]];

		if ((rx_msg->id != pcan_msg.id) ||
		    (rx_msg->data_len != l) ||
		    (memcmp(rx_msg->data, pcan_msg.data, pcan_msg.data_len))) {

			dev->rx_seq_chk_error++;
			if (tst_stop_on_error)
				return NOK;
		}

		return OK;
	}

	/* if an ID has been specified, check it with the one received.
	 * if not match, the received msg is silently discarded.
	 */
	if (dev->can_id >= 0) {
		if (rx_msg->id != dev->can_id) {
			dev->rx_seq_chk_error++;
			if (tst_stop_on_error)
				return NOK;

			if (dev->can_id_seq_mode == INCR) {
				/* resync */
				dev->can_id = (int )rx_msg->id;
			}
			return OK;
		}

		/* update next */
		if (dev->can_id_seq_mode == INCR) {
			dev->can_id++;
			if (dev->msg_flags & PCANFD_MSG_EXT)
				dev->can_id &= CAN_MAX_EXTENDED_ID;
			else
				dev->can_id &= CAN_MAX_STANDARD_ID;
		}
	}

	/* check sequence first */
	if (dev->incr_bytes) {
		__u64 seq_counter, can_counter = 0;

		memcpy(&can_counter, rx_msg->data, dev->incr_bytes);

		seq_counter = le64toh(can_counter);
		if (seq_counter != dev->seq_counter) {
			dev->rx_seq_chk_error++;
			lprintf(ALWAYS,
				"Seq Check Error: %s > %llu (%llxh) "
				"while waiting for %llu (%llxh)\n",
				dev->name, seq_counter, seq_counter,
				dev->seq_counter, dev->seq_counter);
			if (tst_stop_on_error)
				return NOK;
		}

		/* update next sequence number to wait for, if the
		 * number of received packets is a multiple of
		 * msgs_count
		 */
		if ((dev->rx_packets > 0) &&
				!(dev->rx_packets % dev->msgs_count)) {
			__u64 mask64 = 0;
			int i;

			for (i = 0; i < dev->incr_bytes; i++) {
				mask64 <<= 8;
				mask64 |= 0xff;
			}
			dev->seq_counter = (seq_counter + 1) & mask64;
		}

		if (dev->data_length < dev->incr_bytes)
			dev->data_length = dev->incr_bytes;
	}

	rx_msg_data_len = rx_msg->data_len;

	if (rx_msg->type == PCANFD_TYPE_CAN20_MSG)
		if (rx_msg_data_len == PCANFD_CAN20_MAXDATALEN) {
			__u8 dlc = pcanfd_msg_read_dlc(rx_msg);
			if (dlc > rx_msg_data_len)
				rx_msg_data_len = dlc;
		}

	switch (dev->data_length_seq_mode) {
	case INCR:
	case FIXD:
		if (rx_msg_data_len != dev->data_length) {
			dev->rx_seq_chk_error++;
			lprintf(ALWAYS, "Seq Check Error: %s > got LEN=%u "
					"while waiting for %u\n",
				dev->name, rx_msg->data_len, dev->data_length);

			if (tst_stop_on_error)
				return NOK;

			/* resync */
			dev->data_length = rx_msg->data_len;
		}

		if (dev->data_length_seq_mode != INCR)
			break;

		/* incr next length when comparing last rx_msg of a list */
		if ((dev->rx_packets > 0) &&
				!(dev->rx_packets % dev->msgs_count)) {

			if (dev->flags & PCANFD_INIT_FD) {
				int i = tst_len2dlc[rx_msg->data_len];
				dev->data_length = tst_dlc2len[(i + 1) % 16];
			} else {
				dev->data_length = (rx_msg->data_len + 1) %
					(PCANFD_CAN20_MAXDATALEN+1);
			}
		}
		break;
	case RAND:
		/* no check */
		break;
	default:
		break;
	}

	if (!dev->incr_bytes) {
		int i;
		__u32 v;
		struct timeval tv;

		switch (dev->filler_seq_mode) {
		case FIXD:
			if (dev->filler < 0)
				break;
		case INCR:
			if (dev->filler < 0)
				dev->filler = rx_msg->data[0];

			for (i = 0; i < rx_msg->data_len; i++)
				if (rx_msg->data[i] != (__u8 )dev->filler) {
					dev->rx_seq_chk_error++;
					lprintf(ALWAYS,
						"Seq Check Error: %s > "
						"data byte #%u=%02x vs. %02x\n",
						dev->name,
						i, rx_msg->data[i],
						(__u8 )dev->filler);
					if (tst_stop_on_error)
						return NOK;
					break;
				}

			if (dev->filler_seq_mode == INCR)
				dev->filler++;
		case RAND:
			/* fall through */
			break;
		case CLK:
			if (rx_msg->data_len < 8) {
				lprintf(ALWAYS,
					"Invalid data length (should be 8 when "
					"clock filler mode is selected)\n");
				if (tst_stop_on_error)
					return NOK;
				break;
			}

			memcpy(&v, rx_msg->data, 4);
			tv.tv_sec = ntohl(v);
			memcpy(&v, rx_msg->data+4, 4);
			tv.tv_usec = ntohl(v);

			/* normally, rx_msg timestamp is greater than tv */
			if (timercmp(&rx_msg->timestamp, &tv, <)) {
				struct timeval tv_res;

				timersub(&tv, &rx_msg->timestamp, &tv_res);

				lprintf(WARN,
					"Rx time is %u.%06d s earlier than "
					"Tx time\n",
					tv_res.tv_sec, tv_res.tv_usec);

				if (tst_stop_on_error)
					return NOK;
			}
			break;
		}
	}

	return OK;
}

/*
 * This function handles RX test, according to arguments passed on command line
 *
 * Note that this function is called when tst_mode == TST_MODE_TX too.
 */
static enum tst_status handle_rx_tst(struct pcan_device *dev)
{
	struct pcanfd_msg *rx_msg = dev->can_rx_msgs->list;
	enum tst_status tst_status = OK;
	int m, err;

	/* be sure to multi read *ONLY* when in RX mode (in TX mode, a single
	 * read MUST be used because this function is called when at least ONE
	 * (and maybe only ONE) msg is present in the RX queue).
	 */
	if ((tst_mode == TST_MODE_RX) && (dev->msgs_count > 1)) {
#ifndef USES_MSGS_LIST
		dev->can_rx_msgs->count = dev->msgs_count;

		err = pcanfd_recv_msgs(dev->fd, dev->can_rx_msgs);

		lprintf(DEBUG, "pcanfd_recv_msgs(%d, %u) returns %d "
			"(msgs count=%u)\n",
			dev->fd, dev->msgs_count, err,
			dev->can_rx_msgs->count);
#else
		dev->can_rx_msgs->count = 0;

		err = pcanfd_recv_msgs_list(dev->fd,
					dev->msgs_count,
					dev->can_rx_msgs->list);

		lprintf(DEBUG, "pcanfd_recv_msgs_list(%d, %u) returns %d\n",
			dev->fd, dev->msgs_count, err);

		if (err > 0) {
			dev->can_rx_msgs->count = err;
			err = 0;
		}
#endif

	} else {
#ifdef PCANFD_OLD_STYLE_API
		TPCANRdMsg msgv1;

		err = -LINUX_CAN_Read(dev->handle, &msgv1);
		if (!err)
			pcanmsg_to_fd(rx_msg, &msgv1);

		lprintf(DEBUG, "LINUX_CAN_Read(%p) returns %d\n",
				dev->handle, err);
#else
		err = pcanfd_recv_msg(dev->fd, rx_msg);

		lprintf(DEBUG, "pcanfd_recv_msg(%d) returns %d\n",
				dev->fd, err);
#endif
		/* to simplify further processing... */
		dev->can_rx_msgs->count = 1;
	}

	if (err)
		return handle_errno(-err, dev);

	dev->recv_calls++;

	for (m = 0; m < dev->can_rx_msgs->count; m++, rx_msg++) {

		lprintf(DEBUG, "Got msg type=%u id=%xh flags=%xh len=%u "
			    "ts=%u.%06u s.\n",
				rx_msg->type, rx_msg->id, rx_msg->flags,
				rx_msg->data_len,
				(__u32 )rx_msg->timestamp.tv_sec,
				(__u32 )rx_msg->timestamp.tv_usec);

		if (rx_msg->type == PCANFD_TYPE_STATUS) {
			if (handle_rx_tst_status(dev, rx_msg) != OK)
				return NOK;

			continue;
		}

		if (rx_msg->flags & PCANFD_MSG_ECHO) {
			if (rx_msg->ctrlr_data[PCANFD_ECHOID] != dev->rx_echo) {

				/* display the echo */
				putmsg(dev, '>', rx_msg);

				lprintf(ALWAYS,
					"Seq Check Error: %s > wrong echo id "
					"%02xh instead of %02xh\n",
					dev->name,
					rx_msg->ctrlr_data[PCANFD_ECHOID],
					dev->rx_echo);

				if (tst_stop_on_error)
					return NOK;
			}

			dev->rx_echo = rx_msg->ctrlr_data[PCANFD_ECHOID] + 1;
			dev->rx_echo &= 0x7f;

			continue;
		}

		if (rx_msg->flags & PCANFD_MSG_SLF)
			continue;

		dev->rx_packets++;
		dev->rx_bytes += rx_msg->data_len;

		/* incr count of of rx CAN msgs */
		tst_rx_packets++;
		tst_rx_bytes += rx_msg->data_len;

		if (tst_file_fd >= 0)
			write(tst_file_fd, rx_msg->data, rx_msg->data_len);

#if 0//def PCANFD_OLD_STYLE_API
		printf("%10u,%03u > %s %08xh %02u ",
			msgv1.dwTime, msgv1.wUsec,
			dev->name,
			msgv1.Msg.ID, msgv1.Msg.LEN);
#else
		//if (tst_verbose >= NORMAL)
		if (putmsg(dev, '>', rx_msg) == NOK)
			return NOK;

		tst_status = cmp_rx_msg(dev, rx_msg);
		if (tst_status != OK)
			break;
#endif

		if (tst_max_msgs > 0)
			if (tst_rx_packets >= tst_max_msgs) {

				/* properly stop the test */
				tst_max_loop = 1;
				tst_status = END;
				break;
			}
	}

#ifdef XENOMAI
	/* periodically flush printf buffer */
	if (!(tst_rx_bytes % 400))
		rt_print_flush_buffers();
#endif
	return tst_status;
}

/*
 * Get a specific option value (and size).
 */
static enum tst_status handle_getopt_tst(struct pcan_device *pdev)
{
	enum tst_status tst = OK;
	int err;

	if (pdev->opt.size <= 0)
		pdev->opt.size = 64;

	pdev->opt.value = malloc(pdev->opt.size);
	if (!pdev->opt.value) {
		lprintf(ERR,
			"%s: not enough memory to get option %u value "
			"(size=%d)\n",
			pdev->name, pdev->opt.name, pdev->opt.size);

		return NOK;
	}

	err = pcanfd_get_option(pdev->fd, pdev->opt.name,
					  pdev->opt.value,
					  pdev->opt.size);
	if (err < 0) {
		lprintf(ERR, "%s: error %d getting option %u value\n",
			pdev->name, errno, pdev->opt.name);

		tst = NOK;
	} else if (err > pdev->opt.size) {
		lprintf(ERR, "%s: --opt-size should be greater than %u\n",
			pdev->name, err);

		err = -ENOSPC;
		tst = NOK;
	}

	pdev->opt.size = err;
	return tst;
}

/*
 * Set a specific option with a given value (and size).
 */
static enum tst_status handle_setopt_tst(struct pcan_device *pdev)
{
	enum tst_status tst = OK;
	int err;

	if (pdev->opt.size > 0 && pdev->opt.value == NULL) {
		lprintf(ERR,
		       "%s: option value can't be NULL while its size isn't!\n",
			pdev->name);

		return NOK;
	}

	err = pcanfd_set_option(pdev->fd, pdev->opt.name,
					  pdev->opt.value,
					  pdev->opt.size);
	if (err) {
		lprintf(ERR, "%s: error %d setting option %u "
			     "(size=%d value=0x%x (%d))\n",
			pdev->name, errno,
			pdev->opt.name, pdev->opt.size,
			*(__u32 *)pdev->opt.value, *(__u32 *)pdev->opt.value);

		return NOK;
	}

	return tst;
}

/*
 * Record frames into a file.
 */
static enum tst_status handle_rec_tst(struct pcan_device *dev)
{
	return handle_tx_tst(dev);
}

/*
 * This function handles read/write operations from one device.
 *
 * - If the device has been opened in non-blocking mode (pdev->flags &
 *   OFD_NONBLOCKING), reading/writing operations won't block (single-task AND
 *   multi-device mode)
 *
 * - If the device has been opened in blocking mode (single-task AND
 *   single-device mode OR multi-task mode), then read/write operation is able
 *   to block.
 */
static enum tst_status handle_single_device(struct pcan_device *pdev)
{
	struct pcanfd_state dev_state;
	enum tst_status tst = OK;
	int err;

	/* if file descriptor no more opened, do nothing... */
	if (pdev->fd < 0)
		return OK;

	switch (tst_mode) {

	case TST_MODE_TX:
	case TST_MODE_NONE:

		/* when writing, should also have a look to the read side,
		 * since bus errors are events posted in the device rx queue.
		 * When such an error occurs, the driver wakes up any task
		 * waiting for room in the Tx queue. That's the reason why
		 * we should also check the Rx queue next...
		 */
		err = pcanfd_get_state(pdev->fd, &dev_state);
		if (err) {
			tst = handle_errno(-err, pdev);
			break;
		}

		lprintf(DEBUG, "%s state: hw=%02Xh bus=%u "
			"pending rx=%u/%u tx=%u/%u\n",
			pdev->name, dev_state.hw_type, dev_state.bus_state,
			dev_state.rx_pending_msgs, dev_state.rx_max_msgs,
			dev_state.tx_pending_msgs, dev_state.tx_max_msgs);

		/* if nothing to read, can break here */
		if (!dev_state.rx_pending_msgs)
			break;

		/* otherwise, read the rx queue to process the pending msg */
		if (tst_mode == TST_MODE_NONE)
			break;

	case TST_MODE_RX:
		tst = handle_rx_tst(pdev);
		break;

	case TST_MODE_GETOPT:
		tst = handle_getopt_tst(pdev);
		break;

	case TST_MODE_SETOPT:
		tst = handle_setopt_tst(pdev);
		break;

	case TST_MODE_REC:
		tst = handle_rec_tst(pdev);
		break;

	default:
		return tst_mode;
	}

	/* handle TX test part here */
	if (tst_mode == TST_MODE_TX)
		if (tst == OK)
			tst = handle_tx_tst(pdev);

	if (pdev->pause_us)
		if (usleep(pdev->pause_us))
			tst = handle_errno(errno, pdev);

	return tst;
}

#ifndef ONE_TASK_PER_DEVICE
/*
 * Handles all the events coming from the opened devices at once (using select()
 * system call).
 *
 * This function is obvioulsy to be used in mono-task environment.
 */
static enum tst_status handle_several_devices(void)
{
	int i, use_select = 0, fd_count;
	struct pcan_device *pdev;
	enum tst_status tst = OK;
	fd_set fds_read, fds_write;

	struct timeval sel_to;
	struct timeval *sel_to_ptr = (tst_select_to_ms > 0) ?  &sel_to : NULL;

	FD_ZERO(&fds_read);
	FD_ZERO(&fds_write);

	for (pdev = &pcan_device[i = 0]; i < pcan_device_count; i++, pdev++) {

		/* if file descriptor no more opened, do nothing... */
		if (pdev->fd < 0)
			continue;

		/* use select() system call if device has been opened in
		 * non-blocking mode. Corresponding bit is always set,
		 * to be sure to read/write, even in blocking mode.
		 */
		switch (tst_mode) {
		case TST_MODE_TX:
			FD_SET(pdev->fd, &fds_write);

#if 1
			/* Note: when writing, should also
			 * have a look to the read side, since
			 * bus errors are events now. Thus, the device is
			 * opened in NONBLOCKING mode when running Tx test...
			 */
#else
			break;
#endif
		case TST_MODE_RX:
			FD_SET(pdev->fd, &fds_read);
			break;

		default:
			break;
		}

		if (pdev->flags & OFD_NONBLOCKING)
			use_select++;
	}

	/* if, at least, one device is ready to work, wait for any events
	 * coming from the driver.
	 */
	if (use_select) {
		if (sel_to_ptr) {
			sel_to_ptr->tv_sec = tst_select_to_ms / 1000;
			sel_to_ptr->tv_usec =
					(tst_select_to_ms % 1000) * 1000;

			lprintf(DEBUG, "waiting for event on %u pcan "
					"devices during %u ms...\n",
					use_select, tst_select_to_ms);
		} else {
			lprintf(DEBUG, "waiting for event on %u pcan "
					"devices...\n", use_select);
		}

		fd_count = __select(tst_fdmax+1, &fds_read, &fds_write,
				    NULL, sel_to_ptr);
		if (!fd_count) {
			/* timeout */
			lprintf(DEBUG, "Timeout!\n");
			return tst;

		} else if (fd_count < 0) {
			tst = handle_errno(errno, NULL);
			return NOK;

		} else {
			lprintf(DEBUG, "got %u event(s) from the pcan "
					"devices\n", fd_count);
		}
	}

	/* now, loop on devices to check which one has something to say... */
	for (pdev = &pcan_device[i = 0]; i < pcan_device_count && tst == OK;
								i++, pdev++) {

		switch (tst_mode) {
		case TST_MODE_TX:
		case TST_MODE_RX:

			/* if something happened on the device (or if the
			 * device was opened in blocking-mode, check it now.
			 * Otherwise, do nothing.
			 */
			if (!FD_ISSET(pdev->fd, &fds_read) &&
					!FD_ISSET(pdev->fd, &fds_write))
				break;

		default:
			tst = handle_single_device(pdev);
			break;
		}
	}

	return tst;
}
#endif

/*
 * Device task main loop: handle test for a given device
 */
static void dev_main_loop(void *arg)
{
	struct pcan_device *pdev = (struct pcan_device *)arg;
	enum tst_status tst = OK;
	int loop_count;

#ifdef XENOMAI
	pthread_setmode_np(0, PTHREAD_WARNSW, NULL);
	// TODO: pthread_setname_np(pthread_self(), "can_reading_task");

#elif defined(RTAI)
	RT_TASK *rt_task;
	int task_id = getpid() * TST_DEV_PCAN_MAX + (pdev - pcan_device);

	/* RTAI: setup this task for hard real time */
	rt_task = rt_task_init_schmod(task_id, 1, 0, 0, SCHED_FIFO, 0xF);
	if (!rt_task) {
		lprintf(ERR, "%s(): rt_task_init_schmod(%u) failed\n",
			__func__, task_id);
		goto lbl_return;
	}

	rt_make_hard_real_time();
#endif
#ifdef ONE_TASK_PER_DEVICE
	/* enable pthread_cancel() to break CAN_Read() by changing its default
	 * cancelability type.
	 * note:
	 * - Xenomai errno=-512 
	 * - RTAI errno=-4
	 */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif

	/* when sending, the count of loop is equal to the number of write */
	if (tst_max_msgs > 0 &&
		(tst_mode == TST_MODE_TX || tst_mode == TST_MODE_REC))
		tst_max_loop = tst_max_msgs;

	if (!tst_max_loop)
		lprintf(VERBOSE, "start infinite loop\n");
	else
		lprintf(VERBOSE, "running %u loops\n", tst_max_loop);

	/* run test forever or until error or end of loop condition */
	for (loop_count = 1; tst == OK; loop_count++) {

		lprintf(DEBUG, "loop #%u (max=%u)\n", loop_count, tst_max_loop);

#ifdef ONE_TASK_PER_DEVICE
		tst = handle_single_device(pdev);
#else
		tst = handle_several_devices();
#endif

		if (tst_max_loop)
			if (loop_count >= tst_max_loop) {
				lprintf(VERBOSE, "stop test after %u loops\n",
						loop_count);
				break;
			}
	}

#ifdef RTAI
	rt_make_soft_real_time();
	rt_task_delete(rt_task);

lbl_return:
#endif
#ifdef ONE_TASK_PER_DEVICE
	lprintf(DEBUG, "signaling parent I'm exiting...\n");
	sem_post(&rendez_vous);
#endif
	lprintf(VERBOSE, "end of test loop (tst=%u).\n", tst);

	exit_status = !tst;
}

/*
 * Application main process
 */
static void run_application(void)
{
#ifdef ONE_TASK_PER_DEVICE
	struct pcan_device *pdev;
	pthread_attr_t thattr;
	int i, err;

	err = sem_init(&rendez_vous, 0, 0);
	if (err) {
		lprintf(ERR, "failed to create tasks semaphore: err=%d\n",
			err);
		return;
	}

	pthread_attr_init(&thattr);
	pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_JOINABLE);

	/* create one thread per device */
	for (pdev = &pcan_device[i = 0]; i < pcan_device_count; i++, pdev++) {
		err = pthread_create(&pdev->dev_thread,
				     NULL,
				     (void *(*)(void *))dev_main_loop,
				     pdev);
		if (err)
			lprintf(ERR, "failed to spawn device thread: err=%d\n",
				err);
	}

	/* rendez-vous with all children */
	for (i = 0; i < pcan_device_count; ) {

		lprintf(DEBUG, "waiting for %u children to exit...\n",
			pcan_device_count - i);

		/* wait for a first task to stop */
		/* TODO: what about using tst_max_duration here ? */
#ifdef RDV_USES_SEM_WAIT
		err = sem_wait(&rendez_vous);
		if (err) {
#else
		err = sem_trywait(&rendez_vous);
		if (err) {
			if (errno == EAGAIN) {

				/* to be interruptible with ^C... */
				lprintf(DEBUG, "sem_trywait() EAGAIN\n");
				sleep(1);
				if (!tst_sig_caught)
					continue;

				errno = EINTR;	/* to be sure... */
			}
#endif

#ifdef XENOMAI
			/* pthread_kill() does not work. Should rely on
			 * pthread_cancel() only...
			 */
#else
			/* main task has been ^C: forward SIGINT to threads,
			 * then restart rendez-vous */
			if (errno == EINTR) {
				int j;
				for (j = 0; j < pcan_device_count; j++) {
					lprintf(DEBUG, "Signalling task #%u\n",
						j);
					pthread_kill(pcan_device[j].dev_thread,
						     SIGINT);
				}

				continue;
			}
			lprintf(ERR, "failed to wait for semaphore: errno=%d\n",
				errno);
#endif
			break;
		}

		i++;
	}

	lprintf(DEBUG, "rendez-vous with all %u children succeeded...\n",
		pcan_device_count);

	/* join each thread */
	for (pdev = &pcan_device[i = 0]; i < pcan_device_count; i++, pdev++) {

		void *retval;

		lprintf(DEBUG, "canceling thread #%u...\n", i);

		/* forward ^C to tasks */
		//pthread_kill(pdev->dev_thread, SIGINT);
		pthread_cancel(pdev->dev_thread);

		/* destroy the tasks */
		pthread_join(pdev->dev_thread, &retval);
	}

	lprintf(DEBUG, "All %u threads joined...\n", pcan_device_count);

	/* rendez-vous semaphore no more useful */
	sem_destroy(&rendez_vous);

#else /* ONE_TASK_PER_DEVICE */

	/* simply run a single-task main loop... */
	dev_main_loop(NULL);
#endif
}

int main(int argc, char *argv[])
{
	struct pcan_device *pdev = pcan_device;
	enum {
		IN_BITRATE, IN_SAMPLE_PT,
		IN_DBITRATE, IN_DSAMPLE_PT, IN_TSMODE, IN_TSBASE,
		IN_CLOCK, IN_MAXCANMSGS, IN_ID, IN_PAUSE, IN_TXPAUSE,
		IN_LENGTH, IN_INCR, IN_TIMEOUT, IN_MUL, IN_ACCEPT,
		IN_MAXDURATION, IN_PLAY, IN_PLAY_FOREVER, IN_FILLER, IN_FILE,
		IN_OPT_NAME, IN_OPT_SIZE, IN_OPT_VALUE, IN_STOPONBUS,
		IDLE
	} opt_state = IDLE;
	int i;

	for (i = 1; i < argc; i++) {

		lprintf(DEBUG, "state=%u: argv[%u]=\"%s\"\n",
				opt_state, i, argv[i]);

		if (opt_state != IDLE) {
			unsigned long tmp;
			void *p;

			switch (opt_state) {
			case IN_BITRATE:
#ifdef PCANFD_OLD_STYLE_API
				tst_bitrate = strtounit(argv[i], NULL);
				if (tst_bitrate > 0xffff)
					usage("bitrate MUST match BTR0BTR1 "
						"format");
#else
				tst_bitrate = strtounit(argv[i], "kM");
				tst_flags |= OFD_BITRATE;
#endif
				if (tst_flags & OFD_BTR0BTR1)
					lprintf(DEBUG, "--btr0btr1 0x%04x\n",
							tst_bitrate);
				else
					lprintf(DEBUG, "--bitrate %u\n",
							tst_bitrate);
				break;

			case IN_SAMPLE_PT:
				tst_sample_pt = strtounit(argv[i], NULL);
				tst_flags |= OFD_SAMPLEPT;
				lprintf(DEBUG, "--sample_pt %u\n",
					tst_sample_pt);
				break;
#ifndef PCANFD_OLD_STYLE_API
			case IN_DBITRATE:
				tst_dbitrate = strtounit(argv[i], "kM");
				tst_flags |= OFD_DBITRATE;
				if (tst_flags & OFD_BTR0BTR1)
					lprintf(DEBUG, "--dbitrate 0x%04x\n",
							tst_dbitrate);
				else
					lprintf(DEBUG, "--dbitrate %u\n",
							tst_dbitrate);
				break;

			case IN_DSAMPLE_PT:
				tst_dsample_pt = strtounit(argv[i], NULL);
				tst_flags |= OFD_SAMPLEPT;
				lprintf(DEBUG, "--dsample_pt %u\n",
					tst_dsample_pt);
				break;

			case IN_CLOCK:
				tst_clock_Hz = strtounit(argv[i], "kM");
				tst_flags |= OFD_CLOCKHZ;
				lprintf(DEBUG, "--clock %u\n", tst_clock_Hz);
				break;
#endif
			case IN_TSMODE:
				tst_ts_mode = strtounit(argv[i], NULL);
				lprintf(DEBUG, "--ts-mode %u\n", tst_ts_mode);
				break;

			case IN_TSBASE:
				tst_ts_base = (strtounit(argv[i], NULL) << 4) &
						PCANFD_INIT_TS_FMT_MASK;
				lprintf(DEBUG, "--ts-base %u\n", tst_ts_base);
				break;

			case IN_MAXCANMSGS:
				tst_max_msgs = strtounit(argv[i], "kM");
				lprintf(DEBUG, "-n %u\n", tst_max_msgs);
				break;

			case IN_MAXDURATION:
				tst_max_duration = strtounit(argv[i], NULL);
				lprintf(DEBUG, "--max-duration %u\n",
						tst_max_duration);
				break;

			case IN_ID:
				switch (argv[i][0]) {
				case 'r':
					if (!argv[i][1]) {
						tst_can_id_seq_mode = RAND;
						lprintf(DEBUG, "-i%c r\n",
						(tst_msg_flags &
							PCANFD_MSG_EXT) ?
								'e' : 's');
					} else {
						usage("wrong CAN Id specifier");
					}
					tst_can_id = -1;
					break;
				case 'i':
					if (!argv[i][1]) {
						tst_can_id_seq_mode = INCR;
						lprintf(DEBUG, "-i%c i\n",
						(tst_msg_flags &
							PCANFD_MSG_EXT) ?
								'e' : 's');
					} else {
						usage("wrong CAN Id specifier");
					}
					tst_can_id = 0;
					break;
				default:
					tst_can_id_seq_mode = FIXD;
					tst_can_id = strtounit(argv[i], NULL);
					lprintf(DEBUG, "-i%c 0x%x\n",
					(tst_msg_flags & PCANFD_MSG_EXT) ?
							'e' : 's', tst_can_id);
				}
				break;

			case IN_ACCEPT:
				if (!strcmp(argv[i], "all"))
					tst_ids_set = 0;
				else {
					tst_ids_set =
						strtoulist(argv[i], 2, tst_ids);
				}
				switch (tst_ids_set) {
				case 2:
					lprintf(DEBUG, "--accept 0x%x-0x%x\n",
							tst_ids[0], tst_ids[1]);
					break;
				case 1:
					lprintf(DEBUG, "--accept %x\n",
							tst_ids[0]);
					break;
				case 0:
					lprintf(DEBUG, "--accept all\n");
				}
				break;

			case IN_LENGTH:
				switch (argv[i][0]) {
				case 'r':
					if (!argv[i][1]) {
						tst_data_length_seq_mode = RAND;
						lprintf(DEBUG, "--len r\n");
					} else {
						usage("wrong length specifier");
					}
					tst_data_length = -1;
					break;
				case 'i':
					if (!argv[i][1]) {
						tst_data_length_seq_mode = INCR;
						lprintf(DEBUG, "--len i\n");
					} else {
						usage("wrong length specifier");
					}
					if (tst_data_length < 0)
						tst_data_length = 0;
					break;

				default:
					tst_data_length_seq_mode = FIXD;
					tst_data_length = strtounit(argv[i],
									NULL);
					lprintf(DEBUG, "--len %u\n",
							tst_data_length);
				}
				break;

			case IN_FILLER:
				switch (argv[i][0]) {
				case 'r':
					if (!argv[i][1]) {
						tst_filler_seq_mode = RAND;
						lprintf(DEBUG, "--filler r\n");
					} else {
						usage("wrong filler specifier");
					}
					break;
				case 'i':
					if (!argv[i][1]) {
						tst_filler_seq_mode = INCR;
						lprintf(DEBUG, "--filler i\n");
					} else {
						usage("wrong filler specifier");
					}
					break;

				case 'c':
					if (!argv[i][1]) {
						tst_filler_seq_mode = CLK;
						lprintf(DEBUG, "--filler c\n");
					} else {
						usage("wrong filler specifier");
					}
					break;

				default:
					tst_filler_seq_mode = FIXD;
					tst_filler = strtounit(argv[i], NULL);
					lprintf(DEBUG, "--filler %u\n",
							tst_filler);
				}
				break;

			case IN_INCR:
				tst_incr_bytes = strtounit(argv[i], NULL);
				if (tst_incr_bytes > 8)
					usage("wrong count of bytes in increment: must be <= 8");
				lprintf(DEBUG, "--incr %u\n", tst_incr_bytes);
				tst_data_length = tst_incr_bytes;
				tst_data_length_seq_mode = FIXD;
				break;

#ifndef PCANFD_OLD_STYLE_API
			case IN_MUL:
				tst_msgs_count = strtounit(argv[i], NULL);
				lprintf(DEBUG, "--mul %u\n", tst_msgs_count);
				break;
#endif
			case IN_PAUSE:
				tst_pause_us = strtounit(argv[i], "ms");
				lprintf(DEBUG, "--pause-us %u\n", tst_pause_us);
				break;

			case IN_TXPAUSE:
				tst_tx_pause_us = strtounit(argv[i], "ms");
				lprintf(DEBUG, "--tx-pause-us %u\n",
					tst_tx_pause_us);
				break;

#ifndef ONE_TASK_PER_DEVICE
			case IN_TIMEOUT:
				tst_select_to_ms = strtounit(argv[i], "s");
				lprintf(DEBUG, "--timeout-ms %u\n",
							tst_select_to_ms);
				break;
#endif
			case IN_OPT_NAME:
				tst_opt.name = strtounit(argv[i], NULL);
				lprintf(DEBUG, "--opt-name %d\n", tst_opt.name);
				break;

			case IN_OPT_SIZE:
				tst_opt.size = strtounit(argv[i], NULL);
				lprintf(DEBUG, "--opt-size %d\n", tst_opt.size);
				break;

			case IN_OPT_VALUE:
				if (argv[i][0] == '"') {
					tmp = strlen(argv[i]);
					if (argv[i][tmp-1] != '"')
						usage("mal-formed characters string on command line");

					tst_opt.size = tmp - 2;
					p = argv[i]+1;
				} else {
					tmp = strtounit(argv[i], NULL);
					if (tst_opt.size <= 0)
						tst_opt.size = 4; /* 32-bit */

					p = &tmp;
				}
				tst_opt.value = malloc(tst_opt.size);
				if (tst_opt.value) {
					memcpy(tst_opt.value, p, tst_opt.size);
					lprintf(DEBUG, "--opt-value %x\n",
							tst_opt.value);
				}
				break;

			case IN_STOPONBUS:
				tst_stop_on_bus = strtounit(argv[i], NULL);
				lprintf(DEBUG, "--stop-on-bus %d\n", tst_stop_on_bus);
				break;

			case IN_PLAY:
				tst_play_file = argv[i];
				tst_play_loops = 1;
				lprintf(DEBUG, "--play %s\n", tst_play_file);
				break;

			case IN_PLAY_FOREVER:
				tst_play_file = argv[i];
				tst_play_loops = 0;
				lprintf(DEBUG, "--play-forever %s\n",
					tst_play_file);
				break;

			case IN_FILE:
				tst_file_name = argv[i];

				/* init default (if not) to enable traffic */
				if (tst_can_id < 0)
					tst_can_id = 1;

				if (tst_data_length < 0)
					tst_data_length =
						(tst_flags & PCANFD_INIT_FD) ?
							64 : 8;

				/* don't wait between frames if default pause_us
				 * has not yet been changed */
				if (tst_pause_us == TST_DEFAULT_PAUSE_US)
					tst_pause_us = 0;

				/* don't stop on error while transfering */
				tst_stop_on_error = 0;

				lprintf(DEBUG, "--file %s\n", tst_file_name);
				break;

			default:
				break;
			}

			opt_state = IDLE;
			continue;
		}

		lprintf(DEBUG, "- check for '%c'\n", argv[i][0]);

		if (argv[i][0] == '-') {
			char opt = argv[i][1];

			if (opt == '-') {
				lprintf(DEBUG, "(long option detected)\n");
				if (
				   !strcmp(argv[i]+2, "accept")
				|| !strcmp(argv[i]+2, "bitrate")
#ifndef PCANFD_OLD_STYLE_API
				|| !strcmp(argv[i]+2, "clock")
				|| !strcmp(argv[i]+2, "dbitrate")
				|| !strcmp(argv[i]+2, "fd")
#endif
				|| !strcmp(argv[i]+2, "help")
				|| !strcmp(argv[i]+2, "id")
				|| !strcmp(argv[i]+2, "len")
#ifndef PCANFD_OLD_STYLE_API
				|| !strcmp(argv[i]+2, "mul")
#endif
				|| !strcmp(argv[i]+2, "pause-us")
				|| !strcmp(argv[i]+2, "quiet")
				|| !strcmp(argv[i]+2, "rtr")
				|| !strcmp(argv[i]+2, "stdmsg-only")
				|| !strcmp(argv[i]+2, "timeout-ms")
				|| !strcmp(argv[i]+2, "verbose")
				|| !strcmp(argv[i]+2, "with-ts")
				) {
					opt = argv[i][2];

				} else if (!strcmp(argv[i]+2, "brs")
					|| !strcmp(argv[i]+2, "debug")
					|| !strcmp(argv[i]+2, "esi")
					|| !strcmp(argv[i]+2, "filler")
					|| !strcmp(argv[i]+2, "incr")
					|| !strcmp(argv[i]+2, "max-duration")
					|| !strcmp(argv[i]+2, "stop-on-bus")
					) {
					opt = toupper(argv[i][2]);

				} else if (!strcmp(argv[i]+2, "listen-only")
					) {
					opt = argv[i][9];

				} else if (!strcmp(argv[i]+2, "bus-load")
					) {
					opt = argv[i][3];

				} else if (!strcmp(argv[i]+2, "check-ts")) {
					opt = 'T';

				} else if (!strcmp(argv[i]+2, "no-rtr")) {
					tst_msg_flags &= ~PCANFD_MSG_RTR;
					continue;
#ifndef PCANFD_OLD_STYLE_API
				} else if (!strcmp(argv[i]+2, "fd-non-iso")) {
					tst_flags |= PCANFD_INIT_FD|\
							PCANFD_INIT_FD_NON_ISO;
					continue;
#endif
				} else if (!strcmp(argv[i]+2, "btr0btr1")) {
					tst_flags |= OFD_BTR0BTR1;
					opt_state = IN_BITRATE;
					continue;
				} else if (!strcmp(argv[i]+2, "echo")) {
					tst_msg_flags |= PCANFD_MSG_ECHO;
					continue;
				} else if (!strcmp(argv[i]+2, "sample-pt")) {
					opt_state = IN_SAMPLE_PT;
					continue;
				} else if (!strcmp(argv[i]+2, "dsample-pt")) {
					opt_state = IN_DSAMPLE_PT;
					continue;
				} else if (!strcmp(argv[i]+2, "tx-pause-us")) {
					opt_state = IN_TXPAUSE;
					continue;
				} else if (!strcmp(argv[i]+2, "ts-mode")) {
					opt_state = IN_TSMODE;
					continue;
				} else if (!strcmp(argv[i]+2, "ts-base")) {
					opt_state = IN_TSBASE;
					continue;
				} else if (!strcmp(argv[i]+2, "opt-name")) {
					opt_state = IN_OPT_NAME;
					continue;
				} else if (!strcmp(argv[i]+2, "opt-size")) {
					opt_state = IN_OPT_SIZE;
					continue;
				} else if (!strcmp(argv[i]+2, "opt-value")) {
					opt_state = IN_OPT_VALUE;
					continue;
				} else if (!strcmp(argv[i]+2, "play")) {
					opt_state = IN_PLAY;
					continue;
				} else if (!strcmp(argv[i]+2, "play-forever")) {
					opt_state = IN_PLAY_FOREVER;
					continue;
				} else if (!strcmp(argv[i]+2, "file")) {
					opt_state = IN_FILE;
					continue;
				} else if (!strcmp(argv[i]+2, "one-shot")) {
					opt = '1';
				}
			}

			lprintf(DEBUG, "- check for option '%c'\n", opt);

			switch (opt) {
			case '1':
				tst_msg_flags |= PCANFD_MSG_SNG;
				break;
			case 'a':
				opt_state = IN_ACCEPT;
				break;
			case 'b':
				opt_state = IN_BITRATE;
				break;
			case 'B':
				tst_msg_flags |= PCANFD_MSG_BRS;
				break;
#ifndef PCANFD_OLD_STYLE_API
			case 'c':
				opt_state = IN_CLOCK;
				break;
			case 'd':
				opt_state = IN_DBITRATE;
				break;
#endif
			case 'D':
				tst_verbose = DEBUG;
				break;

			case 'E':
				tst_msg_flags |= PCANFD_MSG_ESI;
				break;
#ifndef PCANFD_OLD_STYLE_API
			case 'f':
				tst_flags |= PCANFD_INIT_FD;
				break;
#endif
			case 'F':
				opt_state = IN_FILLER;
				break;
			case 'h':
				usage(NULL);
				break;
			case 'i':
				opt_state = IN_ID;
				switch (argv[i][2]) {
				case 'e':
				case 'x':
					tst_msg_flags |= PCANFD_MSG_EXT;
					break;
				case 's':
					tst_msg_flags &= ~PCANFD_MSG_EXT;
					break;
				}
				break;
			case 'I':
				opt_state = IN_INCR;
				break;
			case 'l':
				opt_state = IN_LENGTH;
				break;
#ifndef PCANFD_OLD_STYLE_API
			case 'm':
				opt_state = IN_MUL;
				break;
#endif
			case 'M':
				/* this option setup SIGALRM */
				opt_state = IN_MAXDURATION;
				break;
			case 'n':
				opt_state = IN_MAXCANMSGS;
				break;
			case 'o':
				tst_flags |= PCANFD_INIT_LISTEN_ONLY;
				break;
			case 'p':
				opt_state = IN_PAUSE;
				break;
			case 'P':
				opt_state = IN_TXPAUSE;
				break;
			case 'q':
				tst_verbose = QUIET;
				break;
			case 'r':
				tst_msg_flags |= PCANFD_MSG_RTR;
				break;

			case 's':
				tst_flags |= PCANFD_INIT_STD_MSG_ONLY;
				break;
			case 'S':
				opt_state = IN_STOPONBUS;
				break;
#ifndef ONE_TASK_PER_DEVICE
			case 't':
				opt_state = IN_TIMEOUT;
				break;
#endif
			case 'T':
				tst_check_timestamps = 1;
				break;
			case 'u':
				tst_flags |= PCANFD_INIT_BUS_LOAD_INFO;
				break;
			case 'v':
				tst_verbose = VERBOSE;
				break;
			case 'w':
				tst_puts_timestamps = 1;
				break;
			default:
				usage("Unknown option on command line");
				break;
			}

			continue;

		} else if (argv[i][0] == '+' ) {

			tst_output_fmt = argv[i]+1;
			lprintf(DEBUG, "+%s\n", tst_output_fmt);
			continue;
		}

		if (!strncmp(argv[i], "tx", 2)) {
			tst_mode = TST_MODE_TX;
		} else if (!strncmp(argv[i], "rx", 2)) {
			tst_mode = TST_MODE_RX;
			tst_pause_us = 0;
		} else if (!strncmp(argv[i], "getopt", 6)) {
			tst_mode = TST_MODE_GETOPT;
			tst_max_loop = 1;
		} else if (!strncmp(argv[i], "setopt", 6)) {
			tst_mode = TST_MODE_SETOPT;
			tst_max_loop = 1;
		} else if (!strncmp(argv[i], "rec", 3)) {
			tst_mode = TST_MODE_REC;
		} else if (!strncmp(argv[i], "none", 4)) {
			tst_mode = TST_MODE_NONE;
		} else if (pcan_device_count < TST_DEV_PCAN_MAX) {
			memset(pdev, '\0', sizeof(*pdev));

			pdev->name = argv[i];
			pdev->fd = -1;
			pdev->flags = tst_flags | tst_ts_base;
			pdev->bitrate = tst_bitrate;
			pdev->sample_pt = tst_sample_pt;
			pdev->dbitrate = tst_dbitrate;
			pdev->dsample_pt = tst_dsample_pt;
			pdev->clock_Hz = tst_clock_Hz;
			pdev->can_id = tst_can_id;
			pdev->can_id_seq_mode = tst_can_id_seq_mode;
#if 1
			pdev->data_length = tst_data_length;
#else
			if (tst_data_length >= 0)
				pdev->data_length = tst_data_length;
			else if (tst_mode == TST_MODE_RX)
				pdev->data_length = -1;
			else
				pdev->data_length = 0;
#endif
			pdev->data_length_seq_mode = tst_data_length_seq_mode;
			pdev->filler_seq_mode = tst_filler_seq_mode;
			pdev->filler = tst_filler;
			pdev->pause_us = tst_pause_us;
			pdev->tx_pause_us = tst_tx_pause_us;
			pdev->incr_bytes = tst_incr_bytes;
			pdev->msgs_count = tst_msgs_count;
			pdev->msg_flags = tst_msg_flags;
			pdev->seq_counter = 0;
			pdev->ids_count = tst_ids_set;
			pdev->ts_mode = tst_ts_mode;

			pdev->opt.name = tst_opt.name;
			pdev->opt.size = tst_opt.size;

			if (pdev->opt.size <= 0)
				pdev->opt.size = 64;

			pdev->opt.value = malloc(pdev->opt.size);
			if (tst_opt.value)
				memcpy(pdev->opt.value, tst_opt.value,
					pdev->opt.size);

			memcpy(pdev->ids, tst_ids,
					pdev->ids_count*sizeof(tst_ids[0]));

			pcan_device_count++;
			pdev++;
		}
	}

#ifndef ONE_TASK_PER_DEVICE
	if (tst_max_duration > 0)
		if (tst_select_to_ms > tst_max_duration*1000) {
			usage("Wrong timeout value: "
			      "can't be greater than test maximum duration");
		}
#endif
	init_application();

#ifdef RT
	/* lock all of the calling process virtual address space into RAM */
	mlockall(MCL_CURRENT | MCL_FUTURE);

#ifdef XENOMAI
	/* be notified if task enters secondary mode */
	setup_sig_handler(SIGXCPU, signal_handler);

#elif defined(RTAI)

	rt_allow_nonroot_hrt();
#endif
#endif /* RT */

	run_application();

	return exit_application(exit_status);
}
