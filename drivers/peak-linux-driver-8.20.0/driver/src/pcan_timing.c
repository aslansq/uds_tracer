/* SPDX-License-Identifier: GPL-2.0 */
/*
 * pcan_timing.c - timing abstraction facilities
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
 * Author:       Stephane Grosjean <s.grosjean@peak-system.com>
 * Credits:
 *
 * "pcan_bitrate_to_bittiming()" and "pcan_update_spt()" was derived from
 * "can_calc_bittiming()" and "can_update_spt()" from
 * linux-4.1/drivers/net/can/dev.c:
 *
 *	Copyright (C) 2005 Marc Kleine-Budde, Pengutronix
 *	Copyright (C) 2006 Andrey Volkov, Varma Electronics
 *	Copyright (C) 2008-2009 Wolfgang Grandegger <wg@grandegger.com>
 */
/* #define DEBUG */
/* #undef DEBUG */

#include "src/pcan_common.h"

#include <linux/string.h>
#include <asm/div64.h>

#include "src/pcan_timing.h"

#define PENALTY_INVALID                0x7fffffff

/* Info about an analysed baud rate */
struct pcan_timing_parameters {
	u32	sysclock;
	u32	tBit_ns;	/* requested duration of a CAN-Bits in ns. */

	u16	tseg1, tseg2;	/* in tSCL_ps */
	u16	sjw;
	u16	prescaler;

	u32	tSCL_ps;	/* Duration of a Time Quantums in picosecs */

	u32	baudrate;
	u32	tseg1_ns, tseg2_ns, sjw_ns;	/* Segments in ns. */

	u32	penalty;		/* "penalty points" for this baud rate,
					 * if it does not hit the optimum */
};

/* Public timing capabilites */
extern const struct pcanfd_bittiming_range sja1000_capabilities;

#define DIV_ROUND(a,b)			(((a)+(b)/2)/(b))

#ifdef CONFIG_64BIT
typedef u64 big_t;
#define pcan_timing_div_64(a,b)		(a/b)
#define DIV_ROUND64(a,b)		DIV_ROUND(a,b)
#else

/* Running 32-bits arch forces using do_div() *BUT* this enables only to
 * divide u64 by u32 and, unfortunately, some division operations
 * below need u64 / u64...
 * But, some dividend and/or divisor could be (very) smaller if we were using
 * quartz frequencies in MHz value instead of Hz values
 * Then, for the moment, PCAN_TIMING_DIV64_WORKAROUND *MUST* be defined
 * The only limitation is that these quartz frequencies *MUST* be multiple of
 * 1xMHz...
 */
#define PCAN_TIMING_DIV64_WORKAROUND

/*
 * u32 pcan_timing_div_64(u64 a, u32 b)
 */
static inline u32 pcan_timing_div_64(u64 a, u32 b)
{
	do_div(a, b);
	return a;
}

/*
 * inline u32 DIV_ROUND64(u64 a, u32 b)
 *
 * Use this *ONLY* when divisor is certain to be a 32-bits value...
 */
static inline u32 DIV_ROUND64(u64 a, u32 b)
{
	u64 x = a + (b >> 1);
	return pcan_timing_div_64(x, b);
}

#ifndef PCAN_TIMING_DIV64_WORKAROUND
typedef u64 big_t;
#else
typedef u32 big_t;
#endif

#endif

#ifdef DEBUG
static void debug_pcan_timing_parameters(char *prompt,
				const struct pcan_timing_parameters *ptp)
{
	pr_info(DEVICE_NAME
		"%s: sysclock=%u tBit_ns=%u tseg1=%u tseg2=%u sjw=%u "
		"prescaler=%u tSCL_ps=%u baudrate=%u tseg1_ns=%u "
		"tseg2_ns=%u sjw_ns=%u penalty=%u\n",
		prompt, ptp->sysclock, ptp->tBit_ns, ptp->tseg1,
		ptp->tseg2, ptp->sjw, ptp->prescaler, ptp->tSCL_ps,
		ptp->baudrate, ptp->tseg1_ns, ptp->tseg2_ns, ptp->sjw_ns,
		ptp->penalty);
}

static void debug_pcan_bittiming_abs(char *prompt,
				const struct pcan_bittiming_abs *pta)
{
	pr_info(DEVICE_NAME
		"%s: prescaler=%u sync_seg_ns=%u tseg1_ns=%u "
		"tseg2_ns=%u sjw_ns=%u bitrate=%u "
		"delta_bitrate=%u sample3=%u\n",
		prompt, pta->prescaler, pta->sync_seg_ns, pta->tseg1_ns,
		pta->tseg2_ns, pta->sjw_ns, pta->bitrate,
		pta->delta_bitrate, pta->sample3);
}

#define __debug_pcan_timing_parameters(a) debug_pcan_timing_parameters(#a,&a)
#define __debug_pcan_bittiming_abs(a)     debug_pcan_bittiming_abs(#a,&a)
#else
#define __debug_pcan_timing_parameters(a)
#define __debug_pcan_bittiming_abs(a)
#endif

/*
 * void pcan_bittiming_to_abstract(struct pcan_bittiming_abs *abt)
 *                                 struct pcan_bittiming *pbt,
 *				   const struct pcanfd_bittiming_range *cap,
 *                                 u32 sysclock_Hz)
 *
 * Convert a SJA1000 baud rate to abstract baudrate
 */
static void pcan_bittiming_to_abstract(struct pcan_bittiming_abs *abt,
				struct pcan_bittiming *pbt,
				const struct pcanfd_bittiming_range *cap,
				u32 sysclock_Hz)
{
	const u32 sysclock_MHz = sysclock_Hz / MHz;
	u32 prescaler_Hz;

	abt->prescaler = pbt->brp; // * cap->intern_prescaler;
	prescaler_Hz = abt->prescaler * kHz;

	abt->sync_seg_ns = DIV_ROUND(prescaler_Hz, sysclock_MHz);

	abt->tseg1_ns = DIV_ROUND(prescaler_Hz * pbt->tseg1, sysclock_MHz);

	abt->tseg2_ns = DIV_ROUND(prescaler_Hz * pbt->tseg2, sysclock_MHz);

	abt->sjw_ns = DIV_ROUND(prescaler_Hz * pbt->sjw, sysclock_MHz);

	abt->bitrate = DIV_ROUND(sysclock_Hz, abt->prescaler *
						(1 + pbt->tseg1 + pbt->tseg2));

	abt->delta_bitrate = abs(abt->bitrate -
		DIV_ROUND(sysclock_Hz, abt->prescaler *
				(1 + pbt->tseg1 + pbt->tseg2 + pbt->sjw)));

	abt->sample3 = pbt->tsam;

	__debug_pcan_bittiming_abs(*abt);
}

/*
 * int pcan_convert_abstract()
 *
 *  Calculates from requested baudrate, base clock and sjw
 *  values for prescaler, sjw, tseg1 and tseg2.
 *  Position of sampling point is given in percent (%),
 *  0% = earliest possible point.
 *  100% = latestest possible point in time.
 *  Return:
 *      1 = good values for prescale, tseg1, tseg2 could be calculated
 *      0 = requested baudrate not possible
 *
 *  Ref: "Applicatiuon Note AN97046: Determination of Bit Timing Parameters
 *       for the CAN Controller SJA1000", Philips Semiconductors.
 */
int pcan_convert_abstract(struct pcan_bittiming_abs *abt_result,
				const struct pcan_bittiming_abs *abt_org,
				const struct pcanfd_bittiming_range *cap,
				u32 sysclock)
{
	/* Calculate baud rate values. baudrate = bits per second */
	u16 tq_nb;        /* Count of timequanta = tSCL per tBit */
	struct pcan_timing_parameters cur_baudrate, best_baudrate;

	memset(&best_baudrate, '\0', sizeof(best_baudrate));

	best_baudrate.penalty = PENALTY_INVALID;	/* mark as invalid */
	cur_baudrate.sysclock = sysclock;		/* nominal clock */

#ifdef PCAN_TIMING_DIV64_WORKAROUND
	/* to avoid div64 arithmetic: do_div() only does u64/u32 */
	sysclock /= MHz;
#endif

	__debug_pcan_bittiming_abs(*abt_org);

	/* = 2000 ns for 500kBaud , 80000 ns for 125 kBaud 
	 * note: 1xGHz < 2^32 < 5xGHz
	 */
	cur_baudrate.tBit_ns = DIV_ROUND(1*GHz, abt_org->bitrate);

	/* Iterate over all combination of "time quanta" and "sjw".
	 * Find optimal combination of new.tseg1, new.tseg2 and new.sjw
	 * with minimal difference of tseg1, tseg2 and sjw to nominal value.
	 */
	for (tq_nb=(cap->tseg1_max+cap->tseg2_max+1); tq_nb >= 4; tq_nb--) {
#ifdef PCAN_TIMING_DIV64_WORKAROUND
		cur_baudrate.prescaler =
			DIV_ROUND(cur_baudrate.tBit_ns * sysclock, tq_nb*kHz);
#else
		cur_baudrate.prescaler =
			DIV_ROUND((big_t)cur_baudrate.tBit_ns * sysclock,
				  (big_t)tq_nb*GHz);
#endif

		/* prescale==0: baud rate can not be found! */
		if (  (cur_baudrate.prescaler < cap->brp_min)
		   || (cur_baudrate.prescaler > cap->brp_max)) {
			__debug_pcan_timing_parameters(cur_baudrate);
			continue;
		}

		/* now base clock is defined, calculate time quantum for this
		 * baudrate
		 */
#ifdef PCAN_TIMING_DIV64_WORKAROUND
		cur_baudrate.tSCL_ps =
			DIV_ROUND(cur_baudrate.prescaler*MHz, sysclock);
#else
		cur_baudrate.tSCL_ps =
			DIV_ROUND((big_t )cur_baudrate.prescaler*1000*GHz,
				  sysclock);
#endif

		if (!cur_baudrate.tSCL_ps) {

			/* prescaler to small => tq_nb too big */
			__debug_pcan_timing_parameters(cur_baudrate);
			continue;
		}

		cur_baudrate.tseg1 = DIV_ROUND(1000*abt_org->tseg1_ns,
		                               cur_baudrate.tSCL_ps);
		cur_baudrate.tseg2 = DIV_ROUND(1000*abt_org->tseg2_ns,
		                               cur_baudrate.tSCL_ps);

		if ((cur_baudrate.tseg1 + cur_baudrate.tseg2 + 1) != tq_nb) {
			/*  is rounding error of tSCL_ns too big? */
			__debug_pcan_timing_parameters(cur_baudrate);
			continue;
		}

		/* if (  (cur_baudrate.tseg1 < 2) */
		if (  (cur_baudrate.tseg1 < cap->tseg1_min)
		   || (cur_baudrate.tseg1 > cap->tseg1_max)) {
			__debug_pcan_timing_parameters(cur_baudrate);
			continue;
		}

		/* if (  (cur_baudrate.tseg2 < 2) */
		if (  (cur_baudrate.tseg2 < cap->tseg2_min)
		   || (cur_baudrate.tseg2 > cap->tseg2_max)) {
			__debug_pcan_timing_parameters(cur_baudrate);
			continue;
		}

		cur_baudrate.tseg1_ns = (big_t )cur_baudrate.tseg1
		                              * cur_baudrate.tSCL_ps / 1000;
		cur_baudrate.tseg2_ns = (big_t )cur_baudrate.tseg2
		                              * cur_baudrate.tSCL_ps / 1000;

		for (cur_baudrate.sjw = cap->sjw_min;
				cur_baudrate.sjw <= cap->sjw_max;
							cur_baudrate.sjw++) {
			/* try all sjw */
			cur_baudrate.sjw_ns = (big_t )cur_baudrate.sjw *
						cur_baudrate.tSCL_ps / 1000;

#ifndef CONFIG_64BIT
			/* this only division needs 64-bits do_div().
			 * the divisor normally won't be > 2^32. Anyway, this
			 * ".baudrate" member seems not be used next and seems
			 * only an info field.
			 */
#endif
			cur_baudrate.baudrate =
				DIV_ROUND64(1000ULL*GHz,
					(  cur_baudrate.tSCL_ps
				         + cur_baudrate.tseg1_ns*1000
				         + cur_baudrate.tseg2_ns*1000));

			/* Calculate deviation of timing to original */

			/* Model for "goodness":
			 * baud rate and sjw are equally important */
			cur_baudrate.penalty =
				abs( ( cur_baudrate.tseg1_ns \
				     + cur_baudrate.tseg2_ns \
				     + cur_baudrate.tSCL_ps / 1000) \
				     - ( abt_org->tseg1_ns \
				       + abt_org->tseg2_ns \
				       + abt_org->sync_seg_ns) )
				     + abs(cur_baudrate.sjw_ns
				     - abt_org->sjw_ns);

			if (cur_baudrate.penalty < best_baudrate.penalty)
				best_baudrate = cur_baudrate;
		}
	}

	/* no baud rate found at all */
	if (best_baudrate.penalty == PENALTY_INVALID) {
		pr_err(DEVICE_NAME ": %s(): no baudrate found\n", __func__);
		return 0;
	}

	abt_result->prescaler = best_baudrate.prescaler;
	abt_result->tseg1_ns = best_baudrate.tseg1_ns;
	abt_result->tseg2_ns = best_baudrate.tseg2_ns;
	abt_result->sjw_ns = best_baudrate.sjw_ns;
	abt_result->bitrate = best_baudrate.baudrate;
	abt_result->sample3 = abt_org->sample3;

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s() best_baudrate[penalty=%u] abt_result["
		"prescaler=%u tseg1_ns=%u tseg2_ns=%u sjw_ns=%u "
		"bitrate=%u delta_bitrate=%u sample3=%u]\n",
		__func__,
		best_baudrate.penalty,
		abt_result->prescaler, abt_result->tseg1_ns,
		abt_result->tseg2_ns,
		abt_result->sjw_ns, abt_result->bitrate,
		abt_result->delta_bitrate, abt_result->sample3);
#endif
	return 1;
}

/*
 * void pcan_btr0btr1_to_abstract(struct pcan_bittiming_abs *ab, u16 btr0btr1)
 *
 * Helper fonction that converts BTR0BTR1 valeu for SJA1000@16MHz into
 * an abstract time.
 */
void pcan_btr0btr1_to_abstract(struct pcan_bittiming_abs *abt, u16 btr0btr1)
{
	const u8 btr0 = btr0btr1 >> 8;
	const u8 btr1 = btr0btr1 & 0xFF;
	struct pcan_bittiming sja1000bt;

	/*  1)  decode original timing. CANAPI uses BTR0BTR1 for SJA1000@16MHz.
	 *      Postcondition: sja100bt contains timing values @16MHz
	 */
	sja1000bt.sjw = ((btr0 >> 6) & 0x03) + 1;
	sja1000bt.tseg1 = (btr1 & 0x0f) + 1;
	sja1000bt.tseg2 = ((btr1 >> 4) & 0x07) + 1;
	sja1000bt.brp = (btr0 & 0x3f) + 1;
	sja1000bt.tsam = btr1 >> 7;

	/*  2) calculate original abstract baud rate
	 *     Postcondition: "abt" valid
	 */
	pcan_bittiming_to_abstract(abt, &sja1000bt,
					&sja1000_capabilities, 8*MHz);
}

/*
 * int pcan_abstract_to_bittiming(struct pcan_bittiming *pbt,
 *				const struct pcan_bittiming_abs *abt,
 *				const struct pcanfd_bittiming_range *cap,
 *				u32 sysclock)
 */
int pcan_abstract_to_bittiming(struct pcan_bittiming *pbt,
				const struct pcan_bittiming_abs *abt,
				const struct pcanfd_bittiming_range *cap,
				u32 sysclock_Hz)
{
	const u32 sysclock_MHz = sysclock_Hz / MHz;
	u32 tSCL_ns = DIV_ROUND(abt->prescaler*kHz, sysclock_MHz);

	if (tSCL_ns) {
		pbt->bitrate = abt->bitrate;
		pbt->sjw = DIV_ROUND(abt->sjw_ns, tSCL_ns);
		pbt->tseg1 = DIV_ROUND(abt->tseg1_ns, tSCL_ns);
		pbt->tseg2 = DIV_ROUND(abt->tseg2_ns, tSCL_ns);
		pbt->brp = abt->prescaler;
		pbt->tq = tSCL_ns;
		pbt->tsam = abt->sample3;
		pbt->sample_point = (PCAN_SAMPT_SCALE * (1 + pbt->tseg1)) /
						(1 + pbt->tseg1 + pbt->tseg2);

#ifdef DEBUG
		pr_info(DEVICE_NAME ": %s(sysclock=%u): tSCL_ns=%u "
			"pbt[sjw=%u tseg1=%u tseg2=%u brp=%u tsam=%u]\n",
			__func__, sysclock_Hz, tSCL_ns,
			pbt->sjw, pbt->tseg1, pbt->tseg2,
			pbt->brp, pbt->tsam);
#endif
		return 1;
	}

	pr_err(DEVICE_NAME ": %s() tSCL_ns=%uGHz/%u=0!!!\n",
		__func__, abt->prescaler, sysclock_Hz);

	return 0;
}

#define PCAN_CALC_MAX_ERROR	50 /* in one-tenth of a percent */
#define PCAN_CALC_SYNC_SEG	1

/*
 * Bit-timing calculation derived from:
 *
 * Code based on LinCAN sources and H8S2638 project
 * Copyright 2004-2006 Pavel Pisa - DCE FELK CVUT cz
 * Copyright 2005      Stanislav Marek
 * email: pisa@cmp.felk.cvut.cz
 *
 * Calculates proper bit-timing parameters for a specified bit-rate
 * and sample-point, which can then be used to set the bit-timing
 * registers of the CAN controller. You can find more information
 * in the header file linux/can/netlink.h.
 */
static int pcan_update_sample_point(const struct pcanfd_bittiming_range *btc,
			   unsigned int sample_point_nominal, unsigned int tseg,
			   unsigned int *tseg1_ptr, unsigned int *tseg2_ptr,
			   unsigned int *sample_point_error_ptr)
{
	unsigned int sample_point_error, best_sample_point_error = UINT_MAX;
	unsigned int sample_point, best_sample_point = 0;
	unsigned int tseg1, tseg2;
	int i;

#ifdef DEBUG_TRACE
	pr_info(DEVICE_NAME
		": %s(): sample_point_nominal=%u tseg=%u bsp_err=%u\n",
		 __func__, sample_point_nominal, tseg, best_sample_point_error);
#endif

	for (i = 0; i <= 1; i++) {
		tseg2 = tseg + PCAN_CALC_SYNC_SEG -
			(sample_point_nominal * (tseg + PCAN_CALC_SYNC_SEG)) /
				PCAN_SAMPT_SCALE - i;

		tseg2 = clamp(tseg2, btc->tseg2_min, btc->tseg2_max);
		tseg1 = tseg - tseg2;
		if (tseg1 > btc->tseg1_max) {
			tseg1 = btc->tseg1_max;
			tseg2 = tseg - tseg1;
		}

		sample_point = PCAN_SAMPT_SCALE *
			(tseg + PCAN_CALC_SYNC_SEG - tseg2) /
				(tseg + PCAN_CALC_SYNC_SEG);

		sample_point_error = abs(sample_point_nominal - sample_point);

#ifdef DEBUG
		pr_info(DEVICE_NAME
			": %s(%u): tseg=%u tseg1=%u tseg2=%u sample_point=%u "
			"sample_point_error=%u (best=%u/%u)\n",
			__func__, i, tseg, tseg1, tseg2,
			sample_point, sample_point_error,
			best_sample_point, best_sample_point_error);
#endif

		if ((sample_point <= sample_point_nominal) &&
			(sample_point_error < best_sample_point_error)) {
			best_sample_point = sample_point;
			best_sample_point_error = sample_point_error;
			*tseg1_ptr = tseg1;
			*tseg2_ptr = tseg2;
		}
	}

	if (sample_point_error_ptr)
		*sample_point_error_ptr = best_sample_point_error;

	return best_sample_point;
}

/*
 * Greatly inspired from "can_calc_bittiming()" function 
 * (see drivers/net/can/dev.c)
 */
int pcan_bitrate_to_bittiming(struct pcan_bittiming *bt,
			const struct pcanfd_bittiming_range *btc,
			u32 sysclock_Hz)
{
	u32 clk_freq;
	unsigned int tseg_min;

	unsigned int bitrate;		/* current bitrate */
	unsigned int bitrate_error;	/* diff btw current & nominal values */
	unsigned int best_bitrate_error = UINT_MAX;
	unsigned int sample_point_error;
	unsigned int best_sample_point_error = UINT_MAX;
	unsigned int sample_point_nominal;
	unsigned int best_tseg = 0;	/* current best value for tseg */
	unsigned int best_brp = 0;	/* current best value for brp */
	unsigned int brp, tsegall, tseg, tseg1 = 0, tseg2 = 0;
	u64 v64;

	if (!btc) // || !btc->intern_prescaler)
		return -EINVAL;

	tseg_min = (btc->tseg1_min + btc->tseg2_min) * 2;
	clk_freq = sysclock_Hz; // / btc->intern_prescaler;

	/* Use CiA recommended sample points
	 * - CANopen networks follows CiA 301
	 * - CAN-FD CiA 601-1
	 */
	if (bt->sample_point) {

		/* sanity check */
		if (bt->sample_point > PCAN_SAMPT_SCALE)
			return -EINVAL;

		sample_point_nominal = bt->sample_point;
	} else {
		if (bt->bitrate > 800000)
			/* 75% */
			sample_point_nominal = 750 * PCAN_SAMPT_SCALE/1000;
		else if (bt->bitrate > 500000)
			/* 80% */
			sample_point_nominal = 800 * PCAN_SAMPT_SCALE/1000;
		else
			/* 87,5% */
			sample_point_nominal = 875 * PCAN_SAMPT_SCALE/1000;
	}

#ifdef DEBUG
	pr_info(DEVICE_NAME ": %s(%u): sp=%u [%u] "
			"brp=[%u..%u] brp_inc=%u "
			"tseg1=[%u..%u] "
			"tseg2=[%u..%u] "
			"sjw=[%u..%u]\n",
			__func__, bt->bitrate, bt->sample_point,
			sample_point_nominal,
			btc->brp_min, btc->brp_max, btc->brp_inc,
			btc->tseg1_min, btc->tseg1_max,
			btc->tseg2_min, btc->tseg2_max,
			btc->sjw_min, btc->sjw_max);
#endif

	/* tseg even = round down, odd = round up */
	for (tseg = (btc->tseg1_max + btc->tseg2_max) * 2 + 1;
					     tseg >= tseg_min; tseg--) {

		tsegall = PCAN_CALC_SYNC_SEG + tseg / 2;

		/* Compute all possible tseg choices (tseg=tseg1+tseg2) */
		brp = clk_freq / (tsegall * bt->bitrate) + tseg % 2;

		/* chose brp step which is possible in system */
		brp = (brp / btc->brp_inc) * btc->brp_inc;

#ifdef DEBUG
		pr_info(DEVICE_NAME
			": %s(%u): brp=%d (best=%d) tseg=%d (best=%d) "
			"tseg1=%d tseg2=%d best_spt_err=%u tq=%u\n",
			__func__, bt->bitrate,
			brp, best_brp,
			tseg, best_tseg,
			tseg1, tseg2,
			best_sample_point_error, bt->tq);
#endif

		if ((brp < btc->brp_min) || (brp > btc->brp_max))
			continue;

		bitrate = clk_freq / (brp * tsegall);
		bitrate_error = abs(bt->bitrate - bitrate);

		/* tseg brp biterror */
		if (bitrate_error > best_bitrate_error)
			continue;

		/* reset sample point error if we have a better bitrate */
		if (bitrate_error < best_bitrate_error)
			best_sample_point_error = UINT_MAX;

		pcan_update_sample_point(btc, sample_point_nominal, tseg / 2,
					&tseg1, &tseg2, &sample_point_error);
		if (sample_point_error > best_sample_point_error)
			continue;

		best_sample_point_error = sample_point_error;
		best_bitrate_error = bitrate_error;
		best_tseg = tseg / 2;
		best_brp = brp;

		if (!bitrate_error && !sample_point_error)
			break;
	}

	if (best_bitrate_error) {

		/* Error in one-tenth of a percent */
		v64 = (u64 )best_bitrate_error * PCAN_SAMPT_SCALE;
		do_div(v64, bt->bitrate);
		bitrate_error = (unsigned int )v64;
		if (bitrate_error > PCAN_CALC_MAX_ERROR) {
			pr_err(DEVICE_NAME
				": bitrate error %u.%u%% too high\n",
				bitrate_error / 10, bitrate_error % 10);
			return -EDOM;
		}

		pr_warn(DEVICE_NAME ": bitrate error %u.%u%%\n",
			bitrate_error / 10, bitrate_error % 10);
	}

	/* real sample point */
	bt->sample_point = pcan_update_sample_point(btc, sample_point_nominal,
					best_tseg, &tseg1, &tseg2, NULL);

	/* real time quantum */
	v64 = (u64)best_brp * GHz;
	do_div(v64, clk_freq);
	bt->tq = (u32)v64;

	bt->tseg1 = tseg1;
	bt->tseg2 = tseg2;
	bt->brp = best_brp;

	/* check for sjw user settings */
	if (!bt->sjw || !btc->sjw_max)
		bt->sjw = 1;
	else {
		/* bt->sjw is at least 1 -> sanitize upper bound to sjw_max */
		if (bt->sjw > btc->sjw_max)
			bt->sjw = btc->sjw_max;
		/* bt->sjw must not be higher than tseg2 */
		if (tseg2 < bt->sjw)
			bt->sjw = tseg2;
	}

#ifdef DEBUG
	pr_info(DEVICE_NAME
		": %s(%u): tq=%u brp=%u tseg1=%u tseg2=%u sp=%u sjw=%u\n",
		__func__, bt->bitrate,
		bt->tq, bt->brp, bt->tseg1, bt->tseg2,
		bt->sample_point, bt->sjw);
#endif

	return 0;
}
