/*
 * lib/route/shdsl.c	SHDSL configuration
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2017 Volodymyr Bendiuga <volodymyr.bendiuga@gmail.com>
 */

#include <netlink-private/netlink.h>
#include <netlink/netlink.h>
#include <netlink/attr.h>
#include <netlink/utils.h>
#include <netlink/object.h>
#include <netlink/hashtable.h>
#include <netlink/data.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/link.h>
#include <netlink/route/shdsl.h>

#define AF_SHDSL        44      /* SHDSL                        */
#define PF_SHDSL        AF_SHDSL

#define SHDSL_ATTR_FAMILY       0x01
#define SHDSL_ATTR_IFINDEX      0x02
#define SHDSL_ATTR_CHANNO       0x03
#define SHDSL_ATTR_ROLE         0x04
#define SHDSL_ATTR_GHS_THR      0x05
#define SHDSL_ATTR_RATE         0x06
#define SHDSL_ATTR_NOISE_MARGIN 0x07
#define SHDSL_ATTR_NONSTRICT    0x08
#define SHDSL_ATTR_LOW_JITTER   0x09
#define SHDSL_ATTR_LFF          0xa
#define SHDSL_ATTR_AVERAGE_BPS  0xb
#define SHDSL_ATTR_PEAK_BPS     0xc
#define SHDSL_ATTR_EMF          0xd
#define SHDSL_ATTR_LINK_STATE   0xe
#define SHDSL_ATTR_LINK_STATUS  0xf
#define SHDSL_ATTR_LINK_UPTIME  0x11
#define SHDSL_ATTR_NO_OF_NEGS   0x12
#define SHDSL_ATTR_IFNAME       0x13


#define SHDSL_ROLE_DISABLED 0
#define SHDSL_ROLE_CO       1
#define SHDSL_ROLE_CPE      2

#define DOWN_NOT_READY  0
#define INITIALIZING    1
#define UP_DATA_MODE    3
#define DOWN_READY      4
#define STOP_DOWN_READY 0x14


static struct nl_cache_ops rtnl_shdsl_ops;
static struct nl_object_ops shdsl_obj_ops;

static struct nla_policy shdsl_policy[SHDA_MAX + 1] = {
	[SHDA_ROLE]         = { .type = NLA_U8 },
	[SHDA_LFF]          = { .type = NLA_U8 },
	[SHDA_GHS_THR]      = { .type = NLA_U32 },
	[SHDA_RATE]         = { .type = NLA_U32 },
	[SHDA_AVERAGE_BPS]  = { .type = NLA_U32 },
	[SHDA_PEAK_BPS]     = { .type = NLA_U32 },
	[SHDA_LINK_STATE]   = { .type = NLA_U8 },
	[SHDA_LINK_STATUS]  = { .type = NLA_U8 },
	[SHDA_LINK_UPTIME]  = { .type = NLA_U32 },
	[SHDA_NO_OF_NEGS]   = { .type = NLA_U32 },
	[SHDA_NOISE_MARGIN] = { .type = NLA_U8 },
	[SHDA_NONSTRICT]    = { .type = NLA_U8 },
	[SHDA_LOW_JITTER]   = { .type = NLA_U8 },
	[SHDA_EMF]          = { .type = NLA_U8 },
	[SHDA_IFINDEX]      = { .type = NLA_U32 },
};

/**
 * @name SHDSL Object Allocation/Freeage
 * @{
 */

struct rtnl_shdsl *rtnl_shdsl_alloc(void)
{
	return (struct rtnl_shdsl *) nl_object_alloc(&shdsl_obj_ops);
}

void rtnl_shdsl_put(struct rtnl_shdsl *shdsl)
{
	nl_object_put((struct nl_object *) shdsl);
}

/** @} */

/**
 * @name SHDSL Cache Managament
 * @{
 */

/**
 * Build a shdsl cache including all shdsl channel configurations
 * currently configured in the kernel.
 * @arg sock		Netlink socket.
 * @arg result		Pointer to store resulting cache.
 *
 * Allocates a new shdsl cache, initializes it properly and updates it
 * to include all shdsl channel configs currently configured in the kernel.
 *
 * @return 0 on success or a negative error code.
 */
int rtnl_shdsl_alloc_cache(struct nl_sock *sock, struct nl_cache **result)
{
	return nl_cache_alloc_and_fill(&rtnl_shdsl_ops, sock, result);
}

/**
 * Look up an shdsl config by channel number
 * @arg cache		shdsl cache
 * @arg channo		channel number of shdsl interface
 *
 * @return shdsl handle or NULL if no match was found.
 */
struct rtnl_shdsl *rtnl_shdsl_get(struct nl_cache *cache, int channo)
{
	struct rtnl_shdsl *shdsl;

	nl_list_for_each_entry(shdsl, &cache->c_items, ce_list) {
		if (shdsl->s_channo == channo) {
			nl_object_get((struct nl_object *) shdsl);
			return shdsl;
		}
	}

	return NULL;
}

/**
 * Look up an shdsl config by ifindex
 * @arg cache		shdsl cache
 * @arg ifindex		interface index
 *
 * @return shdsl handle or NULL if no match was found.
 */
struct rtnl_shdsl *rtnl_shdsl_get_by_ifindex(struct nl_cache *cache, int ifindex)
{
	struct rtnl_shdsl *shdsl;

	nl_list_for_each_entry(shdsl, &cache->c_items, ce_list) {
		if (shdsl->s_ifindex == ifindex) {
			nl_object_get((struct nl_object *) shdsl);
			return shdsl;
		}
	}

	return NULL;
}

/** @} */

/**
 * @name SHDSL Addition
 * @{
 */

static int build_shdsl_msg(struct rtnl_shdsl *tmpl, int cmd, int flags,
			   struct nl_msg **result)
{
	struct nl_msg *msg;
	struct shdsl_msg shdm = {
		.shdm_index   = tmpl->s_ifindex,
		.shdm_family  = AF_SHDSL,
		.shdm_chan    = tmpl->s_channo,
		.shdm_enabled = tmpl->s_enabled,
	};

	msg = nlmsg_alloc_simple(cmd, flags);
	if (!msg)
		return -NLE_NOMEM;

	if (nlmsg_append(msg, &shdm, sizeof(shdm), NLMSG_ALIGNTO) < 0)
		goto nla_put_failure;

	if (tmpl->ce_mask & SHDSL_ATTR_IFNAME)
		NLA_PUT_STRING(msg, SHDA_IFNAME, tmpl->s_ifname);

	if (tmpl->ce_mask & SHDSL_ATTR_ROLE)
		NLA_PUT_U8(msg, SHDA_ROLE, tmpl->s_role);

	if (tmpl->ce_mask & SHDSL_ATTR_LFF)
		NLA_PUT_U8(msg, SHDA_LFF, tmpl->s_lff);

	if (tmpl->ce_mask & SHDSL_ATTR_GHS_THR)
		NLA_PUT_U32(msg, SHDA_GHS_THR, tmpl->s_ghs_thr);

	if (tmpl->ce_mask & SHDSL_ATTR_RATE)
		NLA_PUT_U32(msg, SHDA_RATE, tmpl->s_rate);

	if (tmpl->ce_mask & SHDSL_ATTR_NOISE_MARGIN)
		NLA_PUT_U8(msg, SHDA_NOISE_MARGIN, tmpl->s_noise_margin);

	if (tmpl->ce_mask & SHDSL_ATTR_NONSTRICT)
		NLA_PUT_U8(msg, SHDA_NONSTRICT, tmpl->s_nonstrict);

	if (tmpl->ce_mask & SHDSL_ATTR_LOW_JITTER)
		NLA_PUT_U8(msg, SHDA_LOW_JITTER, tmpl->s_low_jitter);

	if (tmpl->ce_mask & SHDSL_ATTR_EMF)
		NLA_PUT_U8(msg, SHDA_EMF, tmpl->s_emf);

	*result = msg;
	return 0;

 nla_put_failure:
	nlmsg_free(msg);
	return -NLE_MSGSIZE;
}

/**
 * Build netlink request message to add a new shdsl configuration
 * @arg tmpl		template with data of new shdsl configuration
 * @arg flags		additional netlink message flags
 * @arg result		Pointer to store resulting message.
 *
 * Builds a new netlink message requesting a addition of a new
 * shdsl config. The netlink message header isn't fully equipped with
 * all relevant fields and must thus be sent out via nl_send_auto_complete()
 * or supplemented as needed. \a tmpl must contain the attributes of the new
 * shdsl config set via \c rtnl_shdsl_set_* functions.
 *
 * @return 0 on success or a negative error code.
 */
int rtnl_shdsl_build_add_request(struct rtnl_shdsl *tmpl, int flags,
				 struct nl_msg **result)
{
	return build_shdsl_msg(tmpl, RTM_NEWCONF, flags, result);
}

/**
 * Add a new shdsl configuration
 * @arg sk		Netlink socket.
 * @arg tmpl		template with requested changes
 * @arg flags		additional netlink message flags
 *
 * Builds a netlink message by calling rtnl_shdsl_build_add_request(),
 * sends the request to the kernel and waits for the next ACK to be
 * received and thus blocks until the request has been fullfilled.
 *
 * @return 0 on sucess or a negative error if an error occured.
 */
int rtnl_shdsl_add(struct nl_sock *sk, struct rtnl_shdsl *tmpl, int flags)
{
	int err;
	struct nl_msg *msg;

	if ((err = rtnl_shdsl_build_add_request(tmpl, flags, &msg)) < 0)
		return err;

	err = nl_send_auto_complete(sk, msg);
	nlmsg_free(msg);
	if (err < 0)
		return err;

	return wait_for_ack(sk);
}

/** @} */

/**
 * @name Attributes
 * @{
 */

void rtnl_shdsl_set_ifindex(struct rtnl_shdsl *shdsl, int ifindex)
{
	shdsl->s_ifindex = (uint8_t)ifindex;
	shdsl->ce_mask |= SHDSL_ATTR_IFINDEX;
}

int rtnl_shdsl_get_ifindex(struct rtnl_shdsl *shdsl)
{
	return (int)shdsl->s_ifindex;
}

void rtnl_shdsl_set_ifname(struct rtnl_shdsl *shdsl, char *ifname)
{
	strncpy(shdsl->s_ifname, ifname, sizeof(shdsl->s_ifname));
	shdsl->ce_mask |= SHDSL_ATTR_IFNAME;
}

char *rtnl_shdsl_get_ifname(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_ifname;
}

void rtnl_shdsl_set_enabled(struct rtnl_shdsl *shdsl, int enabled)
{
	shdsl->s_enabled = enabled;
}

int rtnl_shdsl_get_enabled(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_enabled;
}

void rtnl_shdsl_set_channo(struct rtnl_shdsl *shdsl, int channo)
{
	shdsl->s_channo = channo;
	shdsl->ce_mask |= SHDSL_ATTR_CHANNO;
}

int rtnl_shdsl_get_channo(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_channo;
}

void rtnl_shdsl_set_role(struct rtnl_shdsl *shdsl, int role)
{
	shdsl->s_role = role;
	shdsl->ce_mask |= SHDSL_ATTR_ROLE;
}

int rtnl_shdsl_get_role(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_role;
}

void rtnl_shdsl_set_lff(struct rtnl_shdsl *shdsl, int lff)
{
	shdsl->s_lff = lff;
	shdsl->ce_mask |= SHDSL_ATTR_LFF;
}

int rtnl_shdsl_get_lff(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_lff;
}

void rtnl_shdsl_set_ghs_thr(struct rtnl_shdsl *shdsl, int ghs_thr)
{
	shdsl->s_ghs_thr = ghs_thr;
	shdsl->ce_mask |= SHDSL_ATTR_GHS_THR;
}

int rtnl_shdsl_get_ghs_thr(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_ghs_thr;
}

void rtnl_shdsl_set_rate(struct rtnl_shdsl *shdsl, int rate)
{
	shdsl->s_rate = rate;
	shdsl->ce_mask |= SHDSL_ATTR_RATE;
}

int rtnl_shdsl_get_rate(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_rate;
}

int rtnl_shdsl_get_average_bps(struct rtnl_shdsl *shdsl)
{
	if (shdsl->ce_mask & SHDSL_ATTR_AVERAGE_BPS)
		return shdsl->s_average_bps;
	else
		return -1;
}

int rtnl_shdsl_get_peak_bps(struct rtnl_shdsl *shdsl)
{
	if (shdsl->ce_mask & SHDSL_ATTR_PEAK_BPS)
		return shdsl->s_peak_bps;
	else
		return -1;
}

void rtnl_shdsl_set_noise_margin(struct rtnl_shdsl *shdsl, int nm)
{
	shdsl->s_noise_margin = nm;
	shdsl->ce_mask |= SHDSL_ATTR_NOISE_MARGIN;
}

int rtnl_shdsl_get_noise_margin(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_noise_margin;
}

void rtnl_shdsl_set_nonstrict(struct rtnl_shdsl *shdsl, int nonstrict)
{
	shdsl->s_nonstrict = nonstrict;
	shdsl->ce_mask |= SHDSL_ATTR_NONSTRICT;
}

int rtnl_shdsl_get_nonstrict(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_nonstrict;
}

void rtnl_shdsl_set_low_jitter(struct rtnl_shdsl *shdsl, int lj)
{
	shdsl->s_low_jitter = lj;
	shdsl->ce_mask |= SHDSL_ATTR_LOW_JITTER;
}

int rtnl_shdsl_get_low_jitter(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_low_jitter;
}

void rtnl_shdsl_set_emf(struct rtnl_shdsl *shdsl, int emf)
{
	shdsl->s_emf = emf;
	shdsl->ce_mask |= SHDSL_ATTR_EMF;
}

int rtnl_shdsl_get_emf(struct rtnl_shdsl *shdsl)
{
	return shdsl->s_emf;
}

int rtnl_shdsl_get_link_state(struct rtnl_shdsl *shdsl)
{
	if (shdsl->ce_mask & SHDSL_ATTR_LINK_STATE)
		return shdsl->s_state;
	else
		return -1;
}

int rtnl_shdsl_get_link_status(struct rtnl_shdsl *shdsl)
{
	if (shdsl->ce_mask & SHDSL_ATTR_LINK_STATUS)
		return shdsl->s_status;
	else
		return -1;
}

int rtnl_shdsl_get_link_uptime(struct rtnl_shdsl *shdsl)
{
	if (shdsl->ce_mask & SHDSL_ATTR_LINK_UPTIME)
		return shdsl->s_uptime;
	else
		return -1;
}

int rtnl_shdsl_get_no_of_negs(struct rtnl_shdsl *shdsl)
{
	if (shdsl->ce_mask & SHDSL_ATTR_NO_OF_NEGS)
		return shdsl->s_no_of_negs;
	else
		return -1;
}

/** @} */

static uint64_t shdsl_compare(struct nl_object *_a, struct nl_object *_b,
			 uint64_t attrs, int flags)
{
	struct rtnl_shdsl *a = (struct rtnl_shdsl *) _a;
	struct rtnl_shdsl *b = (struct rtnl_shdsl *) _b;
	int diff = 0;

#define SHDSL_DIFF(ATTR, EXPR) ATTR_DIFF(attrs, SHDSL_ATTR_##ATTR, a, b, EXPR)

	diff |= SHDSL_DIFF(FAMILY, a->s_family != b->s_family);
	diff |= SHDSL_DIFF(CHANNO, a->s_channo != b->s_channo);
	diff |= SHDSL_DIFF(ROLE, a->s_role != b->s_role);
	diff |= SHDSL_DIFF(GHS_THR, a->s_ghs_thr != b->s_ghs_thr);
	diff |= SHDSL_DIFF(RATE, a->s_rate != b->s_rate);
	diff |= SHDSL_DIFF(NOISE_MARGIN, a->s_noise_margin != b->s_noise_margin);
	diff |= SHDSL_DIFF(NONSTRICT, a->s_nonstrict != b->s_nonstrict);
	diff |= SHDSL_DIFF(LOW_JITTER, a->s_low_jitter != b->s_low_jitter);

#undef SHDSL_DIFF

	return diff;
}

char *rtnl_shdsl_role2str(uint8_t role)
{
	char *rolestr;

	switch (role) {
	case SHDSL_ROLE_DISABLED:
		rolestr = "disabled";
		break;
	case SHDSL_ROLE_CO:
		rolestr = "CO";
		break;
	case SHDSL_ROLE_CPE:
		rolestr = "CPE";
		break;
	default:
		rolestr = "no role";
		break;
	}

	return rolestr;
}

char *rtnl_shdsl_state2str(uint8_t state)
{
	char *str;

	switch (state) {
	case DOWN_NOT_READY:
		str = "DOWN_NOT_READY";
		break;
	case INITIALIZING:
		str = "INITIALIZING";
		break;
	case UP_DATA_MODE:
		str = "UP_DATA_MODE";
		break;
	case DOWN_READY:
		str = "DOWN_READY";
		break;
	case STOP_DOWN_READY:
		str = "STOP_DOWN_READY";
		break;
	default:
		str = "UNKNOWN";
		break;
	}

	return str;
}

static void shdsl_dump_line(struct nl_object *a, struct nl_dump_params *p)
{
	struct nl_cache *lcache;
	struct rtnl_shdsl *shdsl = (struct rtnl_shdsl *) a;
	char buf[128];

	lcache = nl_cache_mngt_require_safe("route/link");

	if (lcache)
		nl_dump(p, "dev %s ", rtnl_link_i2name(lcache, shdsl->s_ifindex,
						       buf, sizeof(buf)));
	else
		nl_dump(p, "dev %d ", shdsl->s_ifindex);

	if (shdsl->ce_mask & SHDSL_ATTR_CHANNO)
		nl_dump(p, "channo %d ", shdsl->s_channo);

	if (shdsl->ce_mask & SHDSL_ATTR_ROLE)
		nl_dump(p, "role %s ", rtnl_shdsl_role2str(shdsl->s_role));

	nl_dump(p, "\n");

	if (lcache)
		nl_cache_put(lcache);
}

static void shdsl_dump_details(struct nl_object *a, struct nl_dump_params *p)
{
	shdsl_dump_line(a, p);
}

static void shdsl_dump_stats(struct nl_object *a, struct nl_dump_params *p)
{
	shdsl_dump_details(a, p);
}

static int shdsl_request_update(struct nl_cache *c, struct nl_sock *sk)
{
	struct shdsl_msg shdm = {
		.shdm_family = AF_SHDSL,
		.shdm_index = c->c_iarg1,
	};

	return nl_send_simple(sk, RTM_GETCONF, NLM_F_DUMP, &shdm,
			      sizeof(shdm));
}

static int rtnl_shdsl_parse(struct nlmsghdr *n, struct rtnl_shdsl **result)
{
	struct rtnl_shdsl *shdsl;
	struct nlattr *tb[SHDA_MAX + 1];
	struct shdsl_msg *shdm;
	int err = 0;

	shdsl = rtnl_shdsl_alloc();
	if (!shdsl) {
		err = -NLE_NOMEM;
		goto errout;
	}

	shdsl->ce_msgtype = n->nlmsg_type;
	shdm = nlmsg_data(n);

	err = nlmsg_parse(n, sizeof(*shdm), tb, SHDA_MAX, shdsl_policy);
	if (err < 0)
		goto errout;

	shdsl->s_family = shdm->shdm_family;
	shdsl->s_ifindex = shdm->shdm_index;
	shdsl->s_channo = shdm->shdm_chan;

	shdsl->ce_mask |= (SHDSL_ATTR_FAMILY| SHDSL_ATTR_CHANNO);

	if (tb[SHDA_IFINDEX]) {
		shdsl->s_ifindex = nla_get_u8(tb[SHDA_IFINDEX]);
		shdsl->ce_mask |= SHDSL_ATTR_IFINDEX;
	}

	if (tb[SHDA_IFNAME]) {
		nla_strlcpy(shdsl->s_ifname, tb[SHDA_IFNAME], sizeof(shdsl->s_ifname));
		shdsl->ce_mask |= SHDSL_ATTR_IFNAME;
	}

	if (tb[SHDA_ROLE]) {
		shdsl->s_role = nla_get_u8(tb[SHDA_ROLE]);
		shdsl->ce_mask |= SHDSL_ATTR_ROLE;
	}

	if (tb[SHDA_LFF]) {
		shdsl->s_lff = nla_get_u8(tb[SHDA_LFF]);
		shdsl->ce_mask |= SHDSL_ATTR_LFF;
	}

	if (tb[SHDA_GHS_THR]) {
		shdsl->s_ghs_thr = nla_get_u32(tb[SHDA_GHS_THR]);
		shdsl->ce_mask |= SHDSL_ATTR_GHS_THR;
	}

	if (tb[SHDA_RATE]) {
		shdsl->s_rate = nla_get_u32(tb[SHDA_RATE]);
		shdsl->ce_mask |= SHDSL_ATTR_RATE;
	}

	if (tb[SHDA_AVERAGE_BPS]) {
		shdsl->s_average_bps = nla_get_u32(tb[SHDA_AVERAGE_BPS]);
		shdsl->ce_mask |= SHDSL_ATTR_AVERAGE_BPS;
	}

	if (tb[SHDA_PEAK_BPS]) {
		shdsl->s_peak_bps = nla_get_u32(tb[SHDA_PEAK_BPS]);
		shdsl->ce_mask |= SHDSL_ATTR_PEAK_BPS;
	}

	if (tb[SHDA_NOISE_MARGIN]) {
		shdsl->s_noise_margin = nla_get_u8(tb[SHDA_NOISE_MARGIN]);
		shdsl->ce_mask |= SHDSL_ATTR_NOISE_MARGIN;
	}

	if (tb[SHDA_NONSTRICT]) {
		shdsl->s_nonstrict = nla_get_u8(tb[SHDA_NONSTRICT]);
		shdsl->ce_mask |= SHDSL_ATTR_NONSTRICT;
	}

	if (tb[SHDA_LOW_JITTER]) {
		shdsl->s_low_jitter = nla_get_u8(tb[SHDA_LOW_JITTER]);
		shdsl->ce_mask |= SHDSL_ATTR_LOW_JITTER;
	}

	if (tb[SHDA_EMF]) {
		shdsl->s_emf = nla_get_u8(tb[SHDA_EMF]);
		shdsl->ce_mask |= SHDSL_ATTR_EMF;
	}

	if (tb[SHDA_LINK_STATE]) {
		shdsl->s_state = nla_get_u8(tb[SHDA_LINK_STATE]);
		shdsl->ce_mask |= SHDSL_ATTR_LINK_STATE;
	}

	if (tb[SHDA_LINK_STATUS]) {
		shdsl->s_status = nla_get_u8(tb[SHDA_LINK_STATUS]);
		shdsl->ce_mask |= SHDSL_ATTR_LINK_STATUS;
	}

	if (tb[SHDA_LINK_UPTIME]) {
		shdsl->s_uptime = nla_get_u32(tb[SHDA_LINK_UPTIME]);
		shdsl->ce_mask |= SHDSL_ATTR_LINK_UPTIME;
	}

	if (tb[SHDA_NO_OF_NEGS]) {
		shdsl->s_no_of_negs = nla_get_u32(tb[SHDA_NO_OF_NEGS]);
		shdsl->ce_mask |= SHDSL_ATTR_NO_OF_NEGS;
	}

	*result = shdsl;
	return 0;

 errout:
	rtnl_shdsl_put(shdsl);
	return err;
}

static int shdsl_msg_parser(struct nl_cache_ops *ops, struct sockaddr_nl *who,
			    struct nlmsghdr *n, struct nl_parser_param *pp)
{
	struct rtnl_shdsl *shdsl;
	int err;

	if ((err = rtnl_shdsl_parse(n, &shdsl)) < 0)
		return err;

	err = pp->pp_cb((struct nl_object *) shdsl, pp);

	rtnl_shdsl_put(shdsl);
	return err;
}

static struct nl_af_group shdsl_groups[] = {
	{ AF_SHDSL, RTNLGRP_SHDSL },
	{ END_OF_GROUP_LIST },
};

static struct nl_object_ops shdsl_obj_ops = {
	.oo_name                  = "route/shdsl",
	.oo_size                  = sizeof(struct rtnl_shdsl),
	.oo_dump = {
		[NL_DUMP_LINE]    = shdsl_dump_line,
		[NL_DUMP_DETAILS] = shdsl_dump_details,
		[NL_DUMP_STATS]   = shdsl_dump_stats,
	},
	.oo_compare               = shdsl_compare,
};

static struct nl_cache_ops rtnl_shdsl_ops = {
	.co_name           = "route/shdsl",
	.co_hdrsize        = sizeof(struct shdsl_msg),
	.co_msgtypes       = {
		                   { RTM_NEWCONF, NL_ACT_NEW, "new" },
			           { RTM_GETCONF, NL_ACT_GET, "get" },
			           END_OF_MSGTYPES_LIST,
	                     },
	.co_protocol       = NETLINK_ROUTE,
	.co_groups         = shdsl_groups,
	.co_request_update = shdsl_request_update,
	.co_msg_parser     = shdsl_msg_parser,
	.co_obj_ops        = &shdsl_obj_ops,
};

static void __init shdsl_init(void)
{
	nl_cache_mngt_register(&rtnl_shdsl_ops);
}

static void __exit shdsl_exit(void)
{
	nl_cache_mngt_unregister(&rtnl_shdsl_ops);
}
