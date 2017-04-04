/*
 * lib/route/link/shdsl.c	SHDSL Link Info
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2017 Volodymyr Bendiuga <volodymyr.bendiuga@gmail.com>
 */

/**
 * @ingroup link
 * @defgroup shdsl SHDSL
 * Single-pair high-speed digital subscriber line link module
 *
 * @details
 * \b Link Type Name: "shdsl"
 *
 * @route_doc{link_shdsl, SHDSL Documentation}
 *
 * @{
 */

#include <netlink-private/netlink.h>
#include <netlink/netlink.h>
#include <netlink/attr.h>
#include <netlink/utils.h>
#include <netlink/object.h>
#include <netlink/route/rtnl.h>
#include <netlink-private/route/link/api.h>
#include <netlink/route/link/shdsl.h>

#include <linux/if_link.h>

/** @cond SKIP */
#define SHDSL_HAS_ROLE_CO           (1<<0)
#define SHDSL_HAS_ROLE_CPE          (1<<1)
#define SHDSL_HAS_LFF               (1<<2)
#define SHDSL_HAS_GHS_THR           (1<<3)
#define SHDSL_HAS_ENABLED           (1<<4)
#define SHDSL_HAS_RATE              (1<<5)
#define SHDSL_HAS_NOISE_MARGIN      (1<<6)
#define SHDSL_HAS_NONSTRICT         (1<<7)
#define SHDSL_HAS_FLOW_CTRL         (1<<8)
#define SHDSL_HAS_PRIORITY          (1<<9)
#define SHDSL_HAS_PRIO_MODE         (1<<10)
#define SHDSL_HAS_DEFAULT_VID       (1<<11)
#define SHDSL_HAS_RATE_LIMIT        (1<<12)
#define SHDSL_HAS_SHAPING           (1<<13)
#define SHDSL_HAS_LOW_JITTER        (1<<14)
#define SHDSL_HAS_EMF               (1<<15)
#define SHDSL_HAS_PAF               (1<<16)

#define GHS_THR_MAX 32767
#define SHDSL_RATE_LIMIT_MIN 70
#define SHDSL_RATE_LIMIT_MAX 256000
#define SHDSL_RATE_MAX 15304

struct shdsl_info {
	uint8_t  si_enabled;
	uint8_t  si_role;
	uint8_t  si_lff;
	uint32_t si_ghs_thr;
	uint32_t si_rate;
	uint8_t  si_noise_margin;
	uint8_t  si_nonstrict;
	uint8_t  si_flow_ctrl;
	uint8_t  si_priority;
	uint8_t  si_prio_mode;
	uint32_t si_default_vid;
	uint32_t si_rate_limit;
	uint32_t si_shaping;
	uint8_t  si_low_jitter;
	uint8_t  si_emf;
	uint8_t  si_paf;
	uint32_t si_mask;
};

/** @endcond */

static struct nla_policy shdsl_policy[IFLA_SHDSL_MAX + 1] = {
	[IFLA_SHDSL_ENABLED]      = { .type = NLA_U8 },
	[IFLA_SHDSL_ROLE]         = { .type = NLA_U8 },
	[IFLA_SHDSL_LFF]          = { .type = NLA_U8 },
	[IFLA_SHDSL_GHS_THR]      = { .type = NLA_U32 },
	[IFLA_SHDSL_RATE]         = { .type = NLA_U32 },
	[IFLA_SHDSL_NOISE_MARGIN] = { .type = NLA_U8 },
	[IFLA_SHDSL_NONSTRICT]    = { .type = NLA_U8 },
	[IFLA_SHDSL_FLOW_CTRL]    = { .type = NLA_U8 },
	[IFLA_SHDSL_PRIORITY]     = { .type = NLA_U8 },
	[IFLA_SHDSL_PRIO_MODE]    = { .type = NLA_U8 },
	[IFLA_SHDSL_DEFAULT_VID]  = { .type = NLA_U32 },
	[IFLA_SHDSL_RATE_LIMIT]   = { .type = NLA_U32 },
	[IFLA_SHDSL_SHAPING]      = { .type = NLA_U32 },
	[IFLA_SHDSL_LOW_JITTER]   = { .type = NLA_U8 },
	[IFLA_SHDSL_EMF]          = { .type = NLA_U8 },
	[IFLA_SHDSL_PAF]          = { .type = NLA_U8 },
};

static int shdsl_alloc(struct rtnl_link *link)
{
	struct shdsl_info *si;

	if (link->l_info)
		memset(link->l_info, 0, sizeof(*si));
	else {
		if ((si = calloc(1, sizeof(*si))) == NULL)
			return -NLE_NOMEM;

		link->l_info = si;
	}

	return 0;
}

static void shdsl_free(struct rtnl_link *link)
{
	struct shdsl_infi *si = link->l_info;

	free(si);
	link->l_info = NULL;
}

static void shdsl_dump_details(struct rtnl_link *link, struct nl_dump_params *p)
{
	struct shdsl_info *si = link->l_info;
	char *name;

	name = rtnl_link_get_name(link);
	if (name)
		nl_dump_line(p, "      SHDSL port %s", name);

	if (si->si_mask & SHDSL_HAS_ENABLED) {
		nl_dump(p, "      status ");
		if (si->si_enabled)
			nl_dump_line(p, "enabled");
		else
			nl_dump_line(p, "disabled");
	}

	nl_dump(p, "      role ");
	if (si->si_role == 0)
		nl_dump_line(p, "CO");
	else
		nl_dump_line(p, "CPE");

	if (si->si_mask & SHDSL_HAS_LFF) {
		nl_dump(p, "      LFF ");
		if (si->si_lff)
			nl_dump_line(p, "enabled");
		else
			nl_dump_line(p, "disabled");
	}

	if (si->si_mask & SHDSL_HAS_GHS_THR)
		nl_dump_line(p, "      G.HS Threshold %d", si->si_ghs_thr);

	if (si->si_mask & SHDSL_HAS_RATE)
		nl_dump_line(p, "      speed %d kbps", si->si_rate);

	if (si->si_mask & SHDSL_HAS_NOISE_MARGIN)
		nl_dump_line(p, "      noise margin %d", si->si_noise_margin);

	if (si->si_mask & SHDSL_HAS_NONSTRICT) {
		nl_dump(p, "      nonstrict ");
		if (si->si_nonstrict)
			nl_dump_line(p, "enabled");
		else
			nl_dump_line(p, "disabled");
	}

	if (si->si_mask & SHDSL_HAS_FLOW_CTRL) {
		nl_dump(p, "      flow control ");
		if (si->si_flow_ctrl)
			nl_dump_line(p, "enabled");
		else
			nl_dump_line(p, "disabled");
	}

	if (si->si_mask & SHDSL_HAS_PRIORITY)
		nl_dump_line(p, "      priority %d", si->si_priority);

	if (si->si_mask & SHDSL_HAS_PRIO_MODE)
		nl_dump_line(p, "      priority-mode %d", si->si_prio_mode);

	if (si->si_mask & SHDSL_HAS_DEFAULT_VID)
		nl_dump_line(p, "      vid %d", si->si_default_vid);

	if (si->si_mask & SHDSL_HAS_RATE_LIMIT) {
		nl_dump(p, "      rate limit  ");
		if (si->si_rate_limit)
			nl_dump_line(p, "%d kbps", si->si_rate_limit);
		else
			nl_dump_line(p, "disabled");
	}

	if (si->si_mask & SHDSL_HAS_SHAPING) {
		nl_dump(p, "      traffic shaping  ");
		if (si->si_shaping)
			nl_dump_line(p, "%d kbps", si->si_shaping);
		else
			nl_dump_line(p, "disabled");
	}

	if (si->si_mask & SHDSL_HAS_LOW_JITTER) {
		nl_dump(p, "      low jitter ");
		if (si->si_low_jitter)
			nl_dump_line(p, "enabled");
		else
			nl_dump_line(p, "disabled");
	}

	if (si->si_mask & SHDSL_HAS_EMF) {
		nl_dump(p, "      EMF ");
		if (si->si_emf)
			nl_dump_line(p, "enabled");
		else
			nl_dump_line(p, "disabled");
	}

	if (si->si_mask & SHDSL_HAS_PAF) {
		nl_dump(p, "      PAF ");
		if (si->si_paf)
			nl_dump_line(p, "enabled");
		else
			nl_dump_line(p, "disabled");
	}
}

static int shdsl_clone(struct rtnl_link *dst, struct rtnl_link *src)
{
	struct shdsl_info *sdst, *ssrc = src->l_info;
	int err;

	dst->l_info = NULL;
	if ((err = rtnl_link_set_type(dst, "shdsl")) < 0)
		return err;
	sdst = dst->l_info;

	if (!sdst || !ssrc)
		return -NLE_NOMEM;

	memcpy(sdst, ssrc, sizeof(struct shdsl_info));

	return 0;
}

static int shdsl_parse(struct rtnl_link *link, struct nlattr *data,
		       struct nlattr *xstats)
{
	struct nlattr *tb[IFLA_SHDSL_MAX + 1];
	struct shdsl_info *si;
	int err;

	NL_DBG(3, "Parsing SHDSL link info\n");

	if ((err = nla_parse_nested(tb, IFLA_SHDSL_MAX, data, shdsl_policy)) < 0)
		goto errout;

	if ((err = shdsl_alloc(link)) < 0)
		goto errout;

	si = link->l_info;

	if (tb[IFLA_SHDSL_ENABLED]) {
		si->si_enabled = nla_get_u8(tb[IFLA_SHDSL_ENABLED]);
		si->si_mask |= SHDSL_HAS_ENABLED;
	}

	if (tb[IFLA_SHDSL_ROLE]) {
		si->si_role = nla_get_u8(tb[IFLA_SHDSL_ROLE]);
		if (si->si_role & SHDSL_HAS_ROLE_CO)
			si->si_mask |= SHDSL_HAS_ROLE_CO;
		else if (si->si_role & SHDSL_HAS_ROLE_CPE)
			si->si_mask |= SHDSL_HAS_ROLE_CPE;
	}

	if (tb[IFLA_SHDSL_LFF]) {
		si->si_lff = nla_get_u8(tb[IFLA_SHDSL_LFF]);
		si->si_mask |= SHDSL_HAS_LFF;
	}

	if (tb[IFLA_SHDSL_GHS_THR]) {
		si->si_ghs_thr = nla_get_u32(tb[IFLA_SHDSL_GHS_THR]);
		si->si_mask |= SHDSL_HAS_GHS_THR;
	}

	if (tb[IFLA_SHDSL_RATE]) {
		si->si_rate = nla_get_u32(tb[IFLA_SHDSL_RATE]);
		si->si_mask |= SHDSL_HAS_RATE;
	}

	if (tb[IFLA_SHDSL_NOISE_MARGIN]) {
		si->si_noise_margin = nla_get_u8(tb[IFLA_SHDSL_NOISE_MARGIN]);
		si->si_mask |= SHDSL_HAS_NOISE_MARGIN;
	}

	if (tb[IFLA_SHDSL_NONSTRICT]) {
		si->si_nonstrict = nla_get_u8(tb[IFLA_SHDSL_NONSTRICT]);
		si->si_mask |= SHDSL_HAS_NONSTRICT;
	}

	if (tb[IFLA_SHDSL_FLOW_CTRL]) {
		si->si_flow_ctrl = nla_get_u8(tb[IFLA_SHDSL_FLOW_CTRL]);
		si->si_mask |= SHDSL_HAS_FLOW_CTRL;
	}

	if (tb[IFLA_SHDSL_PRIORITY]) {
		si->si_priority = nla_get_u8(tb[IFLA_SHDSL_PRIORITY]);
		si->si_mask |= SHDSL_HAS_PRIORITY;
	}

	if (tb[IFLA_SHDSL_PRIO_MODE]) {
		si->si_prio_mode = nla_get_u8(tb[IFLA_SHDSL_PRIO_MODE]);
		si->si_mask |= SHDSL_HAS_PRIO_MODE;
	}

	if (tb[IFLA_SHDSL_DEFAULT_VID]) {
		si->si_default_vid = nla_get_u32(tb[IFLA_SHDSL_DEFAULT_VID]);
		si->si_mask |= SHDSL_HAS_DEFAULT_VID;
	}

	if (tb[IFLA_SHDSL_RATE_LIMIT]) {
		si->si_rate_limit = nla_get_u32(tb[IFLA_SHDSL_RATE_LIMIT]);
		si->si_mask |= SHDSL_HAS_RATE_LIMIT;
	}

	if (tb[IFLA_SHDSL_SHAPING]) {
		si->si_shaping = nla_get_u32(tb[IFLA_SHDSL_SHAPING]);
		si->si_mask |= SHDSL_HAS_SHAPING;
	}

	if (tb[IFLA_SHDSL_LOW_JITTER]) {
		si->si_low_jitter = nla_get_u8(tb[IFLA_SHDSL_LOW_JITTER]);
		si->si_mask |= SHDSL_HAS_LOW_JITTER;
	}

	if (tb[IFLA_SHDSL_EMF]) {
		si->si_emf = nla_get_u8(tb[IFLA_SHDSL_EMF]);
		si->si_mask |= SHDSL_HAS_EMF;
	}

	if (tb[IFLA_SHDSL_PAF]) {
		si->si_paf = nla_get_u8(tb[IFLA_SHDSL_PAF]);
		si->si_mask |= SHDSL_HAS_PAF;
	}

	err = 0;

errout:
	return err;
}

static int shdsl_put_attrs(struct nl_msg *msg, struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;
	struct nlattr *data;

	if (!(data = nla_nest_start(msg, IFLA_INFO_DATA)))
		return -NLE_MSGSIZE;

	if (si->si_mask & SHDSL_HAS_ENABLED)
	  NLA_PUT_U8(msg, IFLA_SHDSL_ENABLED, si->si_enabled);

	if ((si->si_mask & SHDSL_HAS_ROLE_CO) || (si->si_mask & SHDSL_HAS_ROLE_CPE))
	  NLA_PUT_U8(msg, IFLA_SHDSL_ROLE, si->si_role);

	if (si->si_mask & SHDSL_HAS_LFF)
	  NLA_PUT_U8(msg, IFLA_SHDSL_LFF, si->si_lff);

	if (si->si_mask & SHDSL_HAS_GHS_THR)
	  NLA_PUT_U32(msg, IFLA_SHDSL_GHS_THR, si->si_ghs_thr);

	if (si->si_mask & SHDSL_HAS_RATE)
	  NLA_PUT_U32(msg, IFLA_SHDSL_RATE, si->si_rate);

	if (si->si_mask & SHDSL_HAS_NOISE_MARGIN)
	  NLA_PUT_U8(msg, IFLA_SHDSL_NOISE_MARGIN, si->si_noise_margin);

	if (si->si_mask & SHDSL_HAS_NONSTRICT)
	  NLA_PUT_U8(msg, IFLA_SHDSL_NONSTRICT, si->si_nonstrict);

	if (si->si_mask & SHDSL_HAS_FLOW_CTRL)
	  NLA_PUT_U8(msg, IFLA_SHDSL_FLOW_CTRL, si->si_flow_ctrl);

	if (si->si_mask & SHDSL_HAS_PRIORITY)
	  NLA_PUT_U8(msg, IFLA_SHDSL_PRIORITY, si->si_priority);

	if (si->si_mask & SHDSL_HAS_PRIO_MODE)
	  NLA_PUT_U8(msg, IFLA_SHDSL_PRIO_MODE, si->si_prio_mode);

	if (si->si_mask & SHDSL_HAS_DEFAULT_VID)
	  NLA_PUT_U32(msg, IFLA_SHDSL_DEFAULT_VID, si->si_default_vid);

	if (si->si_mask & SHDSL_HAS_RATE_LIMIT)
	  NLA_PUT_U32(msg, IFLA_SHDSL_RATE_LIMIT, si->si_rate_limit);

	if (si->si_mask & SHDSL_HAS_SHAPING)
	  NLA_PUT_U32(msg, IFLA_SHDSL_SHAPING, si->si_shaping);

	if (si->si_mask & SHDSL_HAS_LOW_JITTER)
	  NLA_PUT_U8(msg, IFLA_SHDSL_LOW_JITTER, si->si_low_jitter);

	if (si->si_mask & SHDSL_HAS_EMF)
	  NLA_PUT_U8(msg, IFLA_SHDSL_EMF, si->si_emf);

	if (si->si_mask & SHDSL_HAS_PAF)
	  NLA_PUT_U8(msg, IFLA_SHDSL_PAF, si->si_paf);

	nla_nest_end(msg, data);

nla_put_failure:

	return 0;
}

static struct rtnl_link_info_ops shdsl_info_ops = {
	.io_name		  = "shdsl",
	.io_alloc                 = shdsl_alloc,
	.io_parse                 = shdsl_parse,
	.io_dump = {
		[NL_DUMP_DETAILS] = shdsl_dump_details,
	},
	.io_clone                 = shdsl_clone,
	.io_put_attrs             = shdsl_put_attrs,
	.io_free                  = shdsl_free,
};

/** @cond SKIP */
#define IS_SHDSL_LINK_ASSERT(link) \
	if ((link)->l_info_ops != &shdsl_info_ops) { \
		APPBUG("Link is not a shdsl link. set type \"shdsl\" first."); \
		return -NLE_OPNOTSUPP; \
	}
/** @endcond */

/**
 * @name SHDSL Object
 * @{
 */

/**
 * Allocate link object of type SHDSL
 *
 * @return Allocated link object or NULL.
 */
struct rtnl_link *rtnl_link_shdsl_alloc(void)
{
	struct rtnl_link *link;
	int err;

	if (!(link = rtnl_link_alloc()))
		return NULL;

	if ((err = rtnl_link_set_type(link, "shdsl")) < 0) {
		rtnl_link_put(link);
		return NULL;
	}

	return link;
}

/**
 * Check if link is a SHDSL link
 * @arg link		Link object
 *
 * @return True if link is a SHDSL link, otherwise false is returned.
 */
int rtnl_link_is_shdsl(struct rtnl_link *link)
{
	return link->l_info_ops && !strcmp(link->l_info_ops->io_name, "shdsl");
}

/**
 * Enable SHDSL
 * @arg link		Link object
 * @arg val		value (1 - enable, 0 - disable)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_enabled(struct rtnl_link *link, uint8_t val)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (val < 0 || val > 1)
		return -NLE_INVAL;

	si->si_enabled = val;
	si->si_mask |= SHDSL_HAS_ENABLED;

	return 0;
}

/**
 * Get SHDSL enabled status
 * @arg link		Link object
 *
 * @return enabled status on success or a negative error code
 */
int rtnl_link_shdsl_get_enabled(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_ENABLED)
		return si->si_enabled;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL role
 * @arg link		Link object
 * @arg role		SHDSL role (1 - CO,  2 - CPE)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_role(struct rtnl_link *link, uint8_t role)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	/* toggle between CO and CPE roles */
	if (role & SHDSL_HAS_ROLE_CO) {
		si->si_role = role;
		si->si_mask &= ~0xFF;
		si->si_mask |= SHDSL_HAS_ROLE_CO;
	}
	else if (role & SHDSL_HAS_ROLE_CPE) {
		si->si_role = role;
		si->si_mask &= ~0xFF;
		si->si_mask |= SHDSL_HAS_ROLE_CPE;
	}
	else
		return -NLE_INVAL;

	return 0;
}

/**
 * Get SHDSL role
 * @arg link		Link object
 *
 * @return shdsl role on success or a negative error code
 */
int rtnl_link_shdsl_get_role(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_ROLE_CO || si->si_mask & SHDSL_HAS_ROLE_CPE)
		return si->si_role;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL link fault forwarding
 * @arg link		Link object
 * @arg lff		Link fault forwarding (1 - enable, 0 - disable)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_lff(struct rtnl_link *link, uint8_t lff)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (lff < 0 || lff > 1)
		return -NLE_INVAL;

	si->si_lff = lff;
	si->si_mask |= SHDSL_HAS_LFF;

	return 0;
}

/**
 * Get SHDSL link fault forwarding
 * @arg link		Link object
 *
 * @return lff on success or a negative error code
 */
int rtnl_link_shdsl_get_lff(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_LFF)
		return si->si_lff;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL G.HS Threshold
 * @arg link		Link object
 * @arg thr		Threshold
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_ghs_thr(struct rtnl_link *link, uint32_t thr)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (thr < 0 || thr > GHS_THR_MAX)
		return -NLE_INVAL;

	si->si_ghs_thr = thr;
	si->si_mask |= SHDSL_HAS_GHS_THR;

	return 0;
}

/**
 * Get SHDSL G.HS Threshold
 * @arg link		Link object
 *
 * @return G.HS threshold on success or a negative error code
 */
int rtnl_link_shdsl_get_ghs_thr(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_GHS_THR)
		return  si->si_ghs_thr;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL rate limit
 * @arg link		Link object
 * @arg rate		rate limit value in kbps (0 - disable; 70 - 256000)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_rate_limit(struct rtnl_link *link, uint32_t rl)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (rl < 0 || rl < SHDSL_RATE_LIMIT_MIN || rl > SHDSL_RATE_LIMIT_MAX)
		return -NLE_INVAL;

	si->si_rate_limit = rl;
	si->si_mask |= SHDSL_HAS_RATE_LIMIT;

	return 0;
}

/**
 * Get SHDSL rate limit
 * @arg link		Link object
 *
 * @return rate limit on success or a negative error code
 */
int rtnl_link_shdsl_get_rate_limit(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_RATE_LIMIT)
		return si->si_rate_limit;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL rate in kbps
 * @arg link		Link object
 * @arg rate		rate value in kbps
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_rate(struct rtnl_link *link, uint32_t rate)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (rate < 0 || rate > SHDSL_RATE_MAX)
		return -NLE_INVAL;

	si->si_rate = rate;
	si->si_mask |= SHDSL_HAS_RATE;

	return 0;
}

/**
 * Get SHDSL rate
 * @arg link		Link object
 *
 * @return speed in kbps on success or a negative error code
 */
int rtnl_link_shdsl_get_rate(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_RATE)
		return si->si_rate;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL noise margin
 * @arg link		Link object
 * @arg noise	        noise margin value in db
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_noise_margin(struct rtnl_link *link, uint8_t noise)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (noise < 0)
		return -NLE_INVAL;

	si->si_noise_margin = noise;
	si->si_mask |= SHDSL_HAS_NOISE_MARGIN;

	return 0;
}

/**
 * Get SHDSL noise margin
 * @arg link		Link object
 *
 * @return noise value on success or a negative error code
 */
int rtnl_link_shdsl_get_noise_margin(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_NOISE_MARGIN)
		return si->si_noise_margin;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL less strict match of noise margin value
 * @arg link		Link object
 * @arg nonstrict       nonstrict match (1 - enable, 0 - disable)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_nonstrict(struct rtnl_link *link, uint8_t nonstrict)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (nonstrict < 0 || nonstrict > 1)
		return -NLE_INVAL;

	si->si_nonstrict = nonstrict;
	si->si_mask |= SHDSL_HAS_NONSTRICT;

	return 0;
}

/**
 * Get SHDSL less strict match
 * @arg link		Link object
 *
 * @return the nonstrict value on success or a negative error code
 */
int rtnl_link_shdsl_get_nonstrict(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_NONSTRICT)
		return si->si_nonstrict;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL flow control
 * @arg link		Link object
 * @arg fc              flow control value (1 - enable, 0 - disable)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_flow_control(struct rtnl_link *link, uint8_t fc)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (fc < 0 || fc > 1)
		return -NLE_INVAL;

	si->si_flow_ctrl = fc;
	si->si_mask |= SHDSL_HAS_FLOW_CTRL;

	return 0;
}

/**
 * Get SHDSL flow control value
 * @arg link		Link object
 *
 * @return the nonstrict value on success or a negative error code
 */
int rtnl_link_shdsl_get_flow_control(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_FLOW_CTRL)
		return si->si_flow_ctrl;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL port priority
 * @arg link		Link object
 * @arg prio            priority (0 - 7)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_priority(struct rtnl_link *link, uint8_t prio)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (prio < 0 || prio > 7)
		return -NLE_INVAL;

	si->si_priority = prio;
	si->si_mask |= SHDSL_HAS_PRIORITY;

	return 0;
}

/**
 * Get SHDSL port priority
 * @arg link		Link object
 *
 * @return priority value on success or a negative error code
 */
int rtnl_link_shdsl_get_priority(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_PRIORITY)
		return si->si_priority;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL priority mode
 * @arg link		Link object
 * @arg mode            priority mode (0 - disable, 1 - tag, 2 - ip, 3 - port)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_prio_mode(struct rtnl_link *link, uint8_t mode)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (mode < 0 || mode > 3)
		return -NLE_INVAL;

	si->si_prio_mode = mode;
	si->si_mask |= SHDSL_HAS_PRIO_MODE;

	return 0;
}

/**
 * Get SHDSL priority mode
 * @arg link		Link object
 *
 * @return priority mode value on success or a negative error code
 */
int rtnl_link_shdsl_get_prio_mode(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_PRIO_MODE)
		return si->si_prio_mode;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL default vlan id
 * @arg link		Link object
 * @arg vid             default vlan id
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_default_vid(struct rtnl_link *link, uint32_t vid)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (vid < 0 || vid > 4096)
		return -NLE_INVAL;

	si->si_default_vid = vid;
	si->si_mask |= SHDSL_HAS_DEFAULT_VID;

	return 0;
}

/**
 * Get SHDSL default vid
 * @arg link		Link object
 *
 * @return vlan id on success or a negative error code
 */
int rtnl_link_shdsl_get_default_vid(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_DEFAULT_VID)
		return si->si_default_vid;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL traffic shaping
 * @arg link		Link object
 * @arg rate            rate in kbps
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_shaping(struct rtnl_link *link, uint32_t rate)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (rate < 0 || rate < SHDSL_RATE_LIMIT_MIN || rate > SHDSL_RATE_LIMIT_MAX)
		return -NLE_INVAL;

	si->si_shaping = rate;
	si->si_mask |= SHDSL_HAS_SHAPING;

	return 0;
}

/**
 * Get SHDSL traffic shaping rate
 * @arg link		Link object
 *
 * @return traffic shaping rate on success or a negative error code
 */
int rtnl_link_shdsl_get_shaping(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_SHAPING)
		return si->si_shaping;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL low jitter
 * @arg link		Link object
 * @arg val             value (0 - disable, 1 - enable)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_low_jitter(struct rtnl_link *link, uint8_t val)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (val < 0 || val > 1)
		return -NLE_INVAL;

	si->si_low_jitter = val;
	si->si_mask |= SHDSL_HAS_LOW_JITTER;

	return 0;
}

/**
 * Get SHDSL low jitter
 * @arg link		Link object
 *
 * @return low jitter mode on success or a negative error code
 */
int rtnl_link_shdsl_get_low_jitter(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_LOW_JITTER)
		return si->si_low_jitter;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL Emergency Freeze
 * @arg link		Link object
 * @arg val             value (0 - disable, 1 - enable)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_emf(struct rtnl_link *link, uint8_t val)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (val < 0 || val > 1)
		return -NLE_INVAL;

	si->si_emf = val;
	si->si_mask |= SHDSL_HAS_EMF;

	return 0;
}

/**
 * Get SHDSL Emergency Freeze 
 * @arg link		Link object
 *
 * @return emergency freeze mode on success or a negative error code
 */
int rtnl_link_shdsl_get_emf(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_EMF)
		return si->si_emf;

	return -NLE_AGAIN;
}

/**
 * Set SHDSL PAF
 * @arg link		Link object
 * @arg val             value (0 - disable, 1 - enable)
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_shdsl_set_paf(struct rtnl_link *link, uint8_t val)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (val < 0 || val > 1)
		return -NLE_INVAL;

	si->si_paf = val;
	si->si_mask |= SHDSL_HAS_PAF;

	return 0;
}

/**
 * Get SHDSL PAF
 * @arg link		Link object
 *
 * @return paf mode on success or a negative error code
 */
int rtnl_link_shdsl_get_paf(struct rtnl_link *link)
{
	struct shdsl_info *si = link->l_info;

	IS_SHDSL_LINK_ASSERT(link);

	if (si->si_mask & SHDSL_HAS_PAF)
		return si->si_paf;

	return -NLE_AGAIN;
}

/** @} */

static void __init shdsl_init(void)
{
	rtnl_link_register_info(&shdsl_info_ops);
}

static void __exit shdsl_exit(void)
{
	rtnl_link_unregister_info(&shdsl_info_ops);
}

/** @} */
