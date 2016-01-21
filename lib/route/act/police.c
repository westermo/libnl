/*
 * lib/route/act/police.c	police action
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2016 Volodymyr Bendiuga <volodymyr.bendiuga@westermo.se>
 */

/**
 * @ingroup act
 * @defgroup act_police Policing
 *
 * @{
 */

#include <netlink-private/netlink.h>
#include <netlink-private/tc.h>
#include <netlink/netlink.h>
#include <netlink/attr.h>
#include <netlink/utils.h>
#include <netlink-private/route/tc-api.h>
#include <netlink/route/act/police.h>

#include <linux/atm.h>
#include <string.h>

#define TIME_UNITS_PER_SEC        1000000

static struct nla_policy police_policy[TCA_POLICE_MAX + 1] = {
	[TCA_POLICE_TBF]        = { .minlen = sizeof(struct tc_police) },
	[TCA_POLICE_RATE]       = { .minlen = TC_RTAB_SIZE },
};

/**
 * private helper functions
 */

static int get_size(int *sz, char *str)
{
	double size;
	if (strcasecmp(str, "k") == 0 || strcasecmp(str, "kb") == 0)
		size = 1024;
	else if (strcasecmp(str, "m") == 0 || strcasecmp(str, "mb") == 0)
		size = 1024 * 1024;
	else if (strcasecmp(str, "g") == 0 || strcasecmp(str, "gb") == 0)
		size = 1024 * 1024 * 1024;
	else if (strcasecmp(str, "kbit") == 0)
		size = 1000/8;
	else if (strcasecmp(str, "mbit") == 0)
		size = 1000 * 1000 / 8;
	else if (strcasecmp(str, "gbit") == 0)
		size = 1000 * 1000 * 1000 / 8;
	else
		return -1;

	*sz = size;
	return 0;
}

/* borrowed from iprout2/tc --> tc_core.c */
static int get_tick_in_usec(double *t)
{
	FILE *fp;
	double tick_in_usec;
	double clock_factor;
	__u32 clock_res;
	__u32 t2us;
	__u32 us2t;

	fp = fopen("/proc/net/psched", "r");
	if (NULL == fp)
		return -1;

	if (fscanf(fp, "%08x%08x%08x", &t2us, &us2t, &clock_res) != 3) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	/* compatibility hack: for old iproute binaries (ignoring
	 * the kernel clock resolution) the kernel advertises a
	 * tick multiplier of 1000 in case of nano-second resolution,
	 * which really is 1. */
	if (clock_res == 1000000000)
		t2us = us2t;

	clock_factor  = (double)clock_res / TIME_UNITS_PER_SEC;
	tick_in_usec = (double)t2us / us2t * clock_factor;
	*t = tick_in_usec;

	return 0;
}

/* borrowed from iprout2/tc --> tc_core.c */
static unsigned align_to_atm(unsigned size)
{
	int linksize, cells;
	cells = size / ATM_CELL_PAYLOAD;
	if ((size % ATM_CELL_PAYLOAD) > 0)
		cells++;

	linksize = cells * ATM_CELL_SIZE;
	return linksize;
}

/* borrowed from iprout2/tc --> tc_core.c */
static unsigned adjust_size(unsigned sz, unsigned mpu, int linklayer)
{
	if (sz < mpu)
		sz = mpu;

	switch (linklayer) {
	case TC_LINKLAYER_ATM:
		return align_to_atm(sz);
	case TC_LINKLAYER_ETHERNET:
	default:
		// No size adjustments on Ethernet
		return sz;
	}
}

/* borrowed from iprout2/tc --> tc_core.c */
static unsigned calc_xmittime(__u64 rate, unsigned size)
{
	double utick;
	int err;

	if ((err = get_tick_in_usec(&utick)) < 0)
		return err;

	return utick * (TIME_UNITS_PER_SEC * ((double)size / (double)rate));
	
}

/* borrowed from iprout2/tc --> tc_core.c */
static int calc_rate_table(struct tc_ratespec *r, __u32 *rtab,
			   int cell_log, unsigned mtu, int linklayer)
{
	int i;
	unsigned sz;
	unsigned bps = r->rate;
	unsigned mpu = r->mpu;

	if (mtu == 0)
		mtu = 2047;

	if (cell_log < 0) {
		cell_log = 0;
		while ((mtu >> cell_log) > 255)
			cell_log++;
	}

	for (i = 0; i < 256; i++) {
		sz = adjust_size((i + 1) << cell_log, mpu, linklayer);
		rtab[i] = calc_xmittime(bps, sz);
	}

	r->cell_align = -1;
	r->cell_log = cell_log;
	r->linklayer = (linklayer & TC_LINKLAYER_MASK);
	return cell_log;
}

/**
 * police operations
 */

static int police_msg_parser(struct rtnl_tc *tc, void *data)
{
	struct tc_police *police = data;
	struct nlattr *tb[TCA_POLICE_MAX + 1];
	int err;

	err = tca_parse(tb, TCA_POLICE_MAX, tc, police_policy);
	if (err < 0)
		return err;

	if (!tb[TCA_POLICE_TBF])
		return -NLE_MISSING_ATTR;

	nla_memcpy(police, tb[TCA_POLICE_TBF], sizeof(*police));

	return NLE_SUCCESS;
}

static void police_free_data(struct rtnl_tc *tc, void *data)
{
}

static int police_clone(void *_dst, void *_src)
{
	struct tc_police *dst = _dst, *src = _src;

	memcpy(dst, src, sizeof(*src));

	return NLE_SUCCESS;
}

static int police_msg_fill(struct rtnl_tc *tc, void *data, struct nl_msg *msg)
{
	struct tc_police *police = data;
	__u32 rtab[256];

	if (!police)
		return -NLE_OBJ_NOTFOUND;

	if (calc_rate_table(&police->rate, rtab, -1, police->mtu, police->rate.linklayer) < 0)
		return -NLE_FAILURE;

	NLA_PUT(msg, TCA_POLICE_RATE, 1024, rtab);
	NLA_PUT(msg, TCA_POLICE_TBF, sizeof(*police), police);

	return NLE_SUCCESS;

nla_put_failure:
	return -NLE_NOMEM;
}

static void police_dump_line(struct rtnl_tc *tc, void *data,
			     struct nl_dump_params *p)
{
	struct tc_police *police = data;

	if (!police)
		return;

	nl_dump(p, " rate %dkbit", police->rate.rate / 1000 * 8);
	nl_dump(p, " burst %dk", police->burst);
	nl_dump(p, " mtu %d", police->mtu);
	nl_dump(p, " mpu %d", police->rate.mpu);

	switch (police->rate.linklayer) {
	case TC_LINKLAYER_ETHERNET:
		nl_dump(p, " linklayer ethernet");
		break;
	case TC_LINKLAYER_ATM:
		nl_dump(p, " linklayer ATM");
		break;
	case TC_LINKLAYER_UNAWARE:
		nl_dump(p, " linklayer unaware");
		break;
	}

	switch (police->action) {
	case TC_POLICE_SHOT:
		nl_dump(p, " drop/shot");
		break;
	default:
		nl_dump(p, " act not supported");
	}

	nl_dump(p, " overhead %d", police->rate.overhead);
}

/**
 * @name Attribute Modifications
 * @{
 */

/**
 * Set action for netlink action object
 * @arg act        Action object
 * @arg action     Action to be set for object
 * 
 * The @action argument can be one of:
 *         TC_POLICE_UNSPEC
 *         TC_POLICE_OK
 *         TC_POLICE_RECLASSIFY
 *         TC_POLICE_SHOT
 *         TC_POLICE_PIPE
 *
 * The @action defines what will happen to network frames that
 * satisfy classifier conditions.
 *
 * @return 0 on success or negative error code in case of an error.
 */
int rtnl_police_set_action(struct rtnl_act *act, int action)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	if (action < TC_POLICE_UNSPEC || action > TC_POLICE_PIPE)
		return -NLE_INVAL;

	police->action = action;

	return NLE_SUCCESS;
}

int rtnl_police_get_action(struct rtnl_act *act)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return police->action;
}

/**
 * Set bucket size (burst) for netlink action object
 * @arg act        Action object
 * @arg burst      Bucket size
 * @arg sz         Size of bucket in data units
 *
 * The @sz can be one of:
 *         k or kb --> kilobytes
 *         m or mb --> megabytes
 *         g or gb --> gigabytes
 *
 * The bigger the bucket (burst size) the longer it takes for overflow
 * to happen. In case of overflow rate is dropped to whatever is set
 * by rtnl_police_set_rate(), and action set by rntl_police_set_act() is
 * applied to overflowed packets.
 *
 * @return 0 on success or negative error code in case of an error. 
 */
int rtnl_police_set_burst(struct rtnl_act *act, int burst, char *sz)
{
	struct tc_police *police;
	int size;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	if (burst <= 0 || get_size(&size, sz))
		return -NLE_INVAL;

	police->burst = burst * size;

	return NLE_SUCCESS;
}

int rtnl_police_get_burst(struct rtnl_act *act)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return police->burst;
}

/**
 * Set Maximum Transfer Unit for netlink action object
 * @arg act        Action object
 * @arg mtu        MTU
 * @arg sz         Size of MTU in data units
 *
 * The @sz can be one of:
 *         k or kb --> kilobytes
 *         m or mb --> megabytes
 *         g or gb --> gigabytes
 *
 * If 0 is passed for @mtu, default mtu will be set to 2047
 *
 * @return 0 on success or negative error code in case of an error. 
 */
int rtnl_police_set_mtu(struct rtnl_act *act, int mtu, char *sz)
{
	struct tc_police *police;
	int size;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	police->mtu = 0;
	if (mtu < 0 || get_size(&size, sz))
		return -NLE_INVAL;

	police->mtu = mtu * size;

	return 0;
}

int rtnl_police_get_mtu(struct rtnl_act *act)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;
	
	return police->mtu;
}

/**
 * Set Minimum Packet Unit for netlink action object
 * @arg act        Action object
 * @arg mpu        MPU
 * @arg sz         Size of MPU in data units
 *
 * The @sz can be one of:
 *         k or kb --> kilobytes
 *         m or mb --> megabytes
 *         g or gb --> gigabytes
 *
 * @return 0 on success or negative error code in case of an error. 
 */
int rtnl_police_set_mpu(struct rtnl_act *act, int mpu, char *sz)
{
	struct tc_police *police;
	int size;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	police->rate.mpu = 0;
	if (mpu < 0 || get_size(&size, sz))
		return -NLE_INVAL;

	police->rate.mpu = mpu * size;

	return 0;
}

int rtnl_police_get_mpu(struct rtnl_act *act)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;
 
	return police->rate.mpu;
}

/**
 * Set rate for netlink action object
 * @arg act        Action object
 * @arg rate       Rate
 * @arg units      Size of Rate in data units
 *
 * The @units can be one of:
 *         kbit --> kilobits
 *         mbit --> megabits
 *         gbit --> gigabits
 *
 * It is important that this function is called after
 * rtnl_police_set_burst() has been called. Otherwise
 * behaviour is undefined.
 *
 * @return 0 on success or negative error code in case of an error. 
 */
int rtnl_police_set_rate(struct rtnl_act *act, int rate, char *units)
{
	struct tc_police *police;
	int s;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	if (rate <= 0 || get_size(&s, units))
		return -NLE_INVAL;

	police->rate.rate = rate * s;
	police->burst = calc_xmittime(police->rate.rate, police->burst);

	return NLE_SUCCESS;
}

int rtnl_police_get_rate(struct rtnl_act *act)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return police->rate.rate / 1000 * 8;
}

/**
 * Set Overhead for netlink action object
 * @arg act        Action object
 * @arg ovrhd      Overhead
 *
 * The size of @ovrhd is assumed to be in bytes.
 *
 * The overhead is a per-packet size overhead used in rate computations
 *
 * @return 0 on success or negative error code in case of an error. 
 */
int rtnl_police_set_overhead(struct rtnl_act *act, int ovrhd)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	if (ovrhd < 0) {
		police->rate.overhead = 0;
		return -NLE_INVAL;
	}

	police->rate.overhead = ovrhd;

	return NLE_SUCCESS;
}

int rtnl_police_get_overhead(struct rtnl_act *act)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return police->rate.overhead;
}

/**
 * Set Linklayer for netlink action object
 * @arg act        Action object
 * @arg ll         Link layer
 *
 * The @ll can be one of:
 *         TC_LINKLAYER_UNAWARE
 *         TC_LINKLAYER_ETHERNET
 *         TC_LINKLAYER_ATM
 *
 * For regular ethernet traffic TC_LINKLAYER_ETHERNET should be used.
 *
 * @return 0 on success or negative error code in case of an error. 
 */
int rtnl_police_set_linklayer(struct rtnl_act *act, int ll)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	if (ll < TC_LINKLAYER_UNAWARE || ll > TC_LINKLAYER_ATM)
		return -NLE_INVAL;

	police->rate.linklayer = ll;

	return NLE_SUCCESS;
}

int rtnl_police_get_linklayer(struct rtnl_act *act)
{
	struct tc_police *police;

	if (!(police = (struct tc_police *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return police->rate.linklayer;
}

/**
 * @}
 */

static struct rtnl_tc_ops police_ops = {
	.to_kind                = "police",
	.to_type                = RTNL_TC_TYPE_ACT,
	.to_size                = sizeof(struct tc_police),
	.to_msg_parser          = police_msg_parser,
	.to_free_data           = police_free_data,
	.to_clone               = police_clone,
	.to_msg_fill            = police_msg_fill,
	.to_dump = {
		[NL_DUMP_LINE]  = police_dump_line,
	},
};

static void __init police_init(void)
{
	rtnl_tc_register(&police_ops);
}

static void __exit police_exit(void)
{
	rtnl_tc_unregister(&police_ops);
}

/**
 * @}
 */
