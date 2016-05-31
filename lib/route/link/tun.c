/*
 * lib/route/link/vlan.c	VLAN Link Info
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2003-2013 Thomas Graf <tgraf@suug.ch>
 */

/**
 * @ingroup link
 * @defgroup vlan VLAN
 * Virtual LAN link module
 *
 * @details
 * \b Link Type Name: "vlan"
 *
 * @route_doc{link_vlan, VLAN Documentation}
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
#include <netlink/route/link/tun.h>

#include <linux/if_vlan.h>

/** @cond SKIP */
#define TUN_HAS_TYPE		(1<<0)

struct tun_info
{
	uint16_t ti_type;
	uint32_t		ti_mask;
};

static struct nla_policy tun_policy[IFLA_TUN_MAX+1] = {
	[IFLA_TUN_TYPE]		= { .type = NLA_U16 },
};

static int tun_alloc(struct rtnl_link *link)
{
   	struct tun_info *ti;

	if (link->l_info) {
		ti = link->l_info;
		memset(link->l_info, 0, sizeof(*ti));
	} else {
		if ((ti = calloc(1, sizeof(*ti))) == NULL)
			return -NLE_NOMEM;

		link->l_info = ti;
	}

	return 0;
}

static int tun_parse(struct rtnl_link *link, struct nlattr *data,
		     struct nlattr *xstats)
{
	struct nlattr *tb[IFLA_TUN_MAX+1];
	struct tun_info *ti;
	int err;

	fprintf (stderr, "%s: entering\n", __func__);
	if ((err = nla_parse_nested(tb, IFLA_TUN_MAX, data, tun_policy)) < 0)
		goto errout;
   	if ((err = tun_alloc(link)) < 0)
		goto errout;

        ti = link->l_info;

        if (tb[IFLA_TUN_TYPE]) {
		ti->ti_type = nla_get_u16(tb[IFLA_TUN_TYPE]);
		ti->ti_mask = TUN_HAS_TYPE;
        }
	err = 0;

errout:
	return err;
}

static void tun_free(struct rtnl_link *link)
{
	struct tun_info *ti = link->l_info;
	fprintf (stderr, "%s: entering\n", __func__);
	free(ti);
	link->l_info = NULL;
	return;
}

static void tun_dump_line(struct rtnl_link *link, struct nl_dump_params *p)
{
	struct tun_info *ti = link->l_info;
        fprintf (stderr, "%s: entering\n", __func__);
	nl_dump(p, "-type %d", ti->ti_type);
}

static void tun_dump_details(struct rtnl_link *link, struct nl_dump_params *p)
{
	fprintf (stderr, "%s: entering\n", __func__);
	return;
}

static int tun_clone(struct rtnl_link *dst, struct rtnl_link *src)
{
	int err;

	dst->l_info = NULL;
	if ((err = rtnl_link_set_type(dst, "tun")) < 0)
		return err;

	return 0;
}

static int tun_put_attrs(struct nl_msg *msg, struct rtnl_link *link)
{
	struct tun_info *ti = link->l_info;
	struct nlattr *data;
        fprintf (stderr, "%s: entering\n", __func__);
        if (!(data = nla_nest_start(msg, IFLA_INFO_DATA)))
		return -NLE_MSGSIZE;
	if (ti->ti_mask & TUN_HAS_TYPE) {
		fprintf(stderr, "PUT TYPE %d!\n", ti->ti_type);
		NLA_PUT_U16(msg, IFLA_TUN_TYPE, ti->ti_type);
        }
	nla_nest_end(msg, data);

        fprintf (stderr, "Put attrs!\n");
nla_put_failure:
	return 0;
}

static struct rtnl_link_info_ops tun_info_ops = {
	.io_name		= "tun",
	.io_alloc		= tun_alloc,
	.io_parse		= tun_parse,
	.io_dump = {
		[NL_DUMP_LINE]	= tun_dump_line,
		[NL_DUMP_DETAILS]	= tun_dump_details,
	},
	.io_clone		= tun_clone,
	.io_put_attrs		= tun_put_attrs,
	.io_free		= tun_free,
};

/** @cond SKIP */
#define IS_TUN_LINK_ASSERT(link)					\
	if ((link)->l_info_ops != &tun_info_ops) {			\
		APPBUG("Link is not a tun link. set type \"tun\" first."); \
		return -NLE_OPNOTSUPP;					\
	}
/** @endcond */

/**
 * @name VLAN Object
 * @{
 */

/**
 * Allocate link object of type VLAN
 *
 * @return Allocated link object or NULL.
 */
struct rtnl_link *rtnl_link_tun_alloc(void)
{
	struct rtnl_link *link;
        int err;

	if (!(link = rtnl_link_alloc()))
		return NULL;

	if ((err = rtnl_link_set_type(link, "tun")) < 0) {
		rtnl_link_put(link);
		return NULL;
	}

	return link;
}

int rtnl_link_tun_set_type(struct rtnl_link *link, uint16_t type)
{
   	struct tun_info *ti = link->l_info;

	IS_TUN_LINK_ASSERT(link);

	ti->ti_type = type;
	ti->ti_mask |= TUN_HAS_TYPE;

	return 0;
}

uint16_t rtnl_link_tun_get_type(struct rtnl_link *link)
{
   	struct tun_info *ti = link->l_info;

	IS_TUN_LINK_ASSERT(link);

        if (ti->ti_mask & TUN_HAS_TYPE)
           return ti->ti_type;

        return 0;
}

static void __init tun_init(void)
{
	rtnl_link_register_info(&tun_info_ops);
}

static void __exit tun_exit(void)
{
	rtnl_link_unregister_info(&tun_info_ops);
}
