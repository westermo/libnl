/*
 * lib/route/act/nat.c	NAT action
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2016 Magnus Öberg <magnus.oberg@westermo.se>
 */

/**
 * @ingroup act
 * @defgroup act_nat NAT
 *
 * @{
 */

#include <netlink-private/netlink.h>
#include <netlink-private/tc.h>
#include <netlink/netlink.h>
#include <netlink/attr.h>
#include <netlink/utils.h>
#include <netlink-private/route/tc-api.h>
#include <netlink/route/act/nat.h>
#include <netlink/route/tc.h>

static struct nla_policy nat_policy[TCA_NAT_MAX + 1] = {
	[TCA_NAT_PARMS] = { .minlen = sizeof(struct tc_nat) },
};

/**
 * nat operations
 */

static int nat_msg_parser(struct rtnl_tc *tc, void *data)
{
	struct tc_nat *nat = data;
	struct nlattr *tb[TCA_NAT_MAX + 1];
	int err;

	err = tca_parse(tb, TCA_NAT_MAX, tc, nat_policy);
	if (err < 0)
		return err;

	if (!tb[TCA_NAT_PARMS])
		return -NLE_MISSING_ATTR;

	nla_memcpy(nat, tb[TCA_NAT_PARMS], sizeof(*nat));
	rtnl_tc_set_act_index(tc, nat->index);

	return NLE_SUCCESS;
}

static void nat_free_data(struct rtnl_tc *tc, void *data)
{
}

static int nat_clone(void *_dst, void *_src)
{
	struct tc_nat *dst = _dst, *src = _src;

	memcpy(dst, src, sizeof(*src));

	return NLE_SUCCESS;
}

static int nat_msg_fill(struct rtnl_tc *tc, void *data, struct nl_msg *msg)
{
	struct tc_nat *nat = data;

	if (!nat)
		return -NLE_OBJ_NOTFOUND;

	NLA_PUT(msg, TCA_NAT_PARMS, sizeof(*nat), nat);

	return NLE_SUCCESS;

nla_put_failure:
	return -NLE_NOMEM;
}

static void nat_dump_line(struct rtnl_tc *tc, void *data,
                          struct nl_dump_params *p)
{
	struct tc_nat *nat = data;
	char buf[32];
	uint32_t mask;
	int pfx = 0;

	if (!nat)
		return;

	if (nat->flags & TCA_NAT_FLAG_EGRESS)
		nl_dump(p, " egress");
	else
		nl_dump(p, " ingress");

	mask = ntohl(nat->mask);
	while (mask > 0) {
		mask = mask >> 1;
		pfx++;
	}

	inet_ntop(AF_INET, &nat->old_addr, buf, sizeof(buf));
	nl_dump(p, " %s", buf);
	if (pfx < 32)
		nl_dump(p, "/%d", pfx);

	inet_ntop(AF_INET, &nat->new_addr, buf, sizeof(buf));
	nl_dump(p, " %s", buf);
	if (pfx < 32)
		nl_dump(p, "/%d", pfx);
}

/**
 * @name Attribute Modifications
 * @{
 */

/**
 * Set old IPv4 address on a netlink NAT action object
 * @arg act        Action object
 * @arg addr       Binary IPv4 address in network byte order
 *
 * @return 0 on success or negative error code in case of an error.
 */
int rtnl_nat_set_old_addr(struct rtnl_act *act, uint32_t addr)
{
	struct tc_nat *nat;

	if (!(nat = (struct tc_nat *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	nat->old_addr = addr;

	return NLE_SUCCESS;
}

uint32_t rtnl_nat_get_old_addr(struct rtnl_act *act)
{
	struct tc_nat *nat;

	if (!(nat = (struct tc_nat *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return nat->old_addr;
}

/**
 * Set new IPv4 address on a netlink NAT action object
 * @arg act        Action object
 * @arg addr       Binary IPv4 address in network byte order
 *
 * @return 0 on success or negative error code in case of an error.
 */
int rtnl_nat_set_new_addr(struct rtnl_act *act, uint32_t addr)
{
	struct tc_nat *nat;

	if (!(nat = (struct tc_nat *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	nat->new_addr = addr;

	return NLE_SUCCESS;
}

uint32_t rtnl_nat_get_new_addr(struct rtnl_act *act)
{
	struct tc_nat *nat;

	if (!(nat = (struct tc_nat *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return nat->new_addr;
}

/**
 * Set IPv4 address mask on a netlink NAT action object
 * @arg act        Action object
 * @arg mask       Binary IPv4 address mask in network byte order
 *
 * @return 0 on success or negative error code in case of an error.
 */
int rtnl_nat_set_mask(struct rtnl_act *act, uint32_t mask)
{
	struct tc_nat *nat;

	if (!(nat = (struct tc_nat *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	nat->mask = mask;

	return NLE_SUCCESS;
}

uint32_t rtnl_nat_get_mask(struct rtnl_act *act)
{
	struct tc_nat *nat;

	if (!(nat = (struct tc_nat *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return nat->mask;
}

/**
 * Set flags for a netlink NAT action object
 * @arg act        Action object
 * @arg flags      TCA_NAT_FLAG_* flags.
 *
 * Currently only TCA_NAT_FLAG_EGRESS is defined. Selects NAT on
 * egress/IP src if set, ingress/IP dst otherwise.
 *
 * @return 0 on success or negative error code in case of an error.
 */
int rtnl_nat_set_flags(struct rtnl_act *act, uint32_t flags)
{
	struct tc_nat *nat;

	if (!(nat = (struct tc_nat *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	nat->flags = flags;

	return NLE_SUCCESS;
}

uint32_t rtnl_nat_get_flags(struct rtnl_act *act)
{
	struct tc_nat *nat;

	if (!(nat = (struct tc_nat *) rtnl_tc_data(TC_CAST(act))))
		return -NLE_NOMEM;

	return nat->flags;
}

/**
 * @}
 */

static struct rtnl_tc_ops nat_ops = {
	.to_kind                = "nat",
	.to_type                = RTNL_TC_TYPE_ACT,
	.to_size                = sizeof(struct tc_nat),
	.to_msg_parser          = nat_msg_parser,
	.to_free_data           = nat_free_data,
	.to_clone               = nat_clone,
	.to_msg_fill            = nat_msg_fill,
	.to_dump = {
		[NL_DUMP_LINE]  = nat_dump_line,
	},
};

static void __init nat_init(void)
{
	rtnl_tc_register(&nat_ops);
}

static void __exit nat_exit(void)
{
	rtnl_tc_unregister(&nat_ops);
}

/**
 * @}
 */
