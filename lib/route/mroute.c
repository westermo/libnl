/*
 * lib/route/mroute.c	Multicast Routes
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2018 Volodymyr Bendiuga <volodymyr.bendiuga@westermo.se>
 */

/**
 * @ingroup rtnl
 * @defgroup mroute Multicast Routing
 * @brief
 * @{
 */

#include <netlink-private/netlink.h>
#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/utils.h>
#include <netlink/data.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/route.h>
#include <netlink/route/mroute.h>
#include <netlink/route/link.h>


static struct nl_cache_ops rtnl_mroute_ops;
struct nl_object_ops mroute_obj_ops;

static struct nla_policy mroute_policy[RTA_MAX+1] = {
	[RTA_IIF]	 = { .type = NLA_U32 },
	[RTA_TABLE]	 = { .type = NLA_U32 },
	[RTA_DST]	 = { .maxlen = INET_ADDRSTRLEN },
	[RTA_SRC]	 = { .maxlen = INET_ADDRSTRLEN },
	[RTA_MULTIPATH]	 = { .type = NLA_NESTED },
	/* nexthop attributes */
	[RTA_FLOW]	 = { .type = NLA_U32 },
	[RTA_GATEWAY]	 = { .maxlen = INET_ADDRSTRLEN },
	[RTA_NEWDST]	 = { .maxlen = INET_ADDRSTRLEN },
	[RTA_VIA]	 = { .maxlen = INET_ADDRSTRLEN },
	[RTA_ENCAP]	 = { .type = NLA_NESTED },
	[RTA_ENCAP_TYPE] = { .type = NLA_U16 },
};

/**
 * @name Cache Management
 * @{
 */

/**
 * Build a multicast route cache holding all routes configured in kernel
 * @arg sk		Netlink socket.
 * @arg family		Address family
 * @arg result		Result pointer
 *
 * Allocates a new cache, initializes it and updates it to
 * contain all routes configured in kernel.
 *
 * @note The caller is responsible for destroying and freeing the
 *       cache after using it.
 * @return 0 on success or a negative error code.
 */
int rtnl_mroute_alloc_cache(struct nl_sock *sk, int family,
			    struct nl_cache **result)
{
	struct nl_cache *cache;
	int err;

	err = nl_cache_alloc_and_fill(&rtnl_mroute_ops, sk, &cache);
	if (err)
		return err;

	*result = cache;
	return 0;
}

/**
 * Add multicast route to cache
 * @arg cache		Multicast route cache
 * @arg mr              Multicast route
 *
 * @return 0 on success or negative error code
 */
int rtnl_mroute_add_cache(struct nl_cache *cache, struct rtnl_route *mr)
{
	return nl_cache_add(cache, OBJ_CAST(mr));
}

/**
 * Delete multicast route from cache it belongs to
 * @arg mr              Multicast route
 *
 * @return 0 on success or negative error code
 */
void rtnl_mroute_delete_cache(struct rtnl_route *mr)
{
	nl_cache_remove(OBJ_CAST(mr));
}

/**
 * Get multicast route from cache by destination address (group id)
 * @arg cache		Multicast route cache
 * @arg addr            Destination address
 *
 * @return 0 on success or negative error code
 */
struct rtnl_route *rtnl_mroute_get_by_dst(struct nl_cache *cache,
					  struct nl_addr *addr)
{
	struct rtnl_route *mr;

	if (cache->c_ops != &rtnl_mroute_ops)
		return NULL;

	nl_list_for_each_entry(mr, &cache->c_items, ce_list) {
		if (0 == nl_addr_cmp(mr->rt_dst, addr)) {
			nl_object_get((struct nl_object *) mr);
			return mr;
		}
	}

	return NULL;
}

/** @} */

static int build_mroute_msg(struct rtnl_route *route, int cmd, int flags,
			    struct nl_msg **result)
{
	struct nl_msg *msg;
	struct rtmsg rtmsg = {
		.rtm_family = route->rt_family,
		.rtm_tos = route->rt_tos,
		.rtm_table = route->rt_table,
		.rtm_scope = route->rt_scope,
		.rtm_type = route->rt_type,
		.rtm_flags = route->rt_flags,
	};
	int err = -NLE_MSGSIZE;

	if (!(msg = nlmsg_alloc_simple(cmd, flags)))
		return -NLE_NOMEM;

	if (route->rt_dst == NULL) {
		err = -NLE_MISSING_ATTR;
		goto nla_put_failure;
	}

	rtmsg.rtm_dst_len = nl_addr_get_prefixlen(route->rt_dst);
	if (route->rt_src)
		rtmsg.rtm_src_len = nl_addr_get_prefixlen(route->rt_src);

	if (!(route->ce_mask & ROUTE_ATTR_SCOPE))
		rtmsg.rtm_scope = rtnl_route_guess_scope(route);

	if (rtnl_route_get_nnexthops(route) == 1) {
		struct rtnl_nexthop *nh;
		nh = rtnl_route_nexthop_n(route, 0);
		rtmsg.rtm_flags |= nh->rtnh_flags;
	}

	if (nlmsg_append(msg, &rtmsg, sizeof(rtmsg), NLMSG_ALIGNTO) < 0)
		goto nla_put_failure;

	/* Additional table attribute replacing the 8bit in the header, was
	 * required to allow more than 256 tables. MPLS does not allow the
	 * table attribute to be set
	 */
	if (route->rt_family != AF_MPLS)
		NLA_PUT_U32(msg, RTA_TABLE, route->rt_table);

	if (nl_addr_get_len(route->rt_dst))
		NLA_PUT_ADDR(msg, RTA_DST, route->rt_dst);

	if (route->ce_mask & ROUTE_ATTR_SRC)
		NLA_PUT_ADDR(msg, RTA_SRC, route->rt_src);

	if (route->ce_mask & ROUTE_ATTR_IIF)
		NLA_PUT_U32(msg, RTA_IIF, route->rt_iif);

	if (rtnl_route_get_nnexthops(route) == 1) {
		struct rtnl_nexthop *nh;

		nh = rtnl_route_nexthop_n(route, 0);
		if (nh->rtnh_gateway)
			NLA_PUT_ADDR(msg, RTA_GATEWAY, nh->rtnh_gateway);
		if (nh->rtnh_ifindex)
			NLA_PUT_U32(msg, RTA_OIF, nh->rtnh_ifindex);
		if (nh->rtnh_realms)
			NLA_PUT_U32(msg, RTA_FLOW, nh->rtnh_realms);
		if (nh->rtnh_newdst)
			NLA_PUT_ADDR(msg, RTA_NEWDST, nh->rtnh_newdst);
		if (nh->rtnh_via && rtnl_route_put_via(msg, nh->rtnh_via) < 0)
			goto nla_put_failure;
		if (nh->rtnh_encap &&
		    nh_encap_build_msg(msg, nh->rtnh_encap) < 0)
			goto nla_put_failure;
	} else if (rtnl_route_get_nnexthops(route) > 1) {
		struct nlattr *multipath;
		struct rtnl_nexthop *nh;

		if (!(multipath = nla_nest_start(msg, RTA_MULTIPATH)))
			goto nla_put_failure;

		nl_list_for_each_entry(nh, &route->rt_nexthops, rtnh_list) {
			struct rtnexthop *rtnh;

			rtnh = nlmsg_reserve(msg, sizeof(*rtnh), NLMSG_ALIGNTO);
			if (!rtnh)
				goto nla_put_failure;

			rtnh->rtnh_flags = nh->rtnh_flags;
			rtnh->rtnh_hops = nh->rtnh_weight;
			rtnh->rtnh_ifindex = nh->rtnh_ifindex;

			if (nh->rtnh_gateway)
				NLA_PUT_ADDR(msg, RTA_GATEWAY,
					     nh->rtnh_gateway);

			if (nh->rtnh_newdst)
				NLA_PUT_ADDR(msg, RTA_NEWDST, nh->rtnh_newdst);

			if (nh->rtnh_via &&
			    rtnl_route_put_via(msg, nh->rtnh_via) < 0)
				goto nla_put_failure;

			if (nh->rtnh_realms)
				NLA_PUT_U32(msg, RTA_FLOW, nh->rtnh_realms);

			if (nh->rtnh_encap &&
			    nh_encap_build_msg(msg, nh->rtnh_encap) < 0)
				goto nla_put_failure;

			rtnh->rtnh_len = nlmsg_tail(msg->nm_nlh) -
						(void *) rtnh;
		}

		nla_nest_end(msg, multipath);
	}

	*result = msg;
	return 0;

nla_put_failure:
	nlmsg_free(msg);
	return err;
}

static int rtnl_mroute_build_add_request(struct rtnl_route *tmpl, int flags,
					 struct nl_msg **result)
{
	return build_mroute_msg(tmpl, RTM_NEWROUTE, NLM_F_CREATE | flags,
				result);
}

static int rtnl_mroute_build_del_request(struct rtnl_route *tmpl, int flags,
					 struct nl_msg **result)
{
	return build_mroute_msg(tmpl, RTM_DELROUTE, flags, result);
}

/**
 * @name Multicast Route Add/Delete
 * @{
 */

int rtnl_mroute_add(struct nl_sock *sk, struct rtnl_route *mr, int flags)
{
	struct nl_msg *msg;
	int err;

	if ((err = rtnl_mroute_build_add_request(mr, flags, &msg)) < 0)
		return err;

	err = nl_send_auto_complete(sk, msg);
	nlmsg_free(msg);
	if (err < 0)
		return err;

	return wait_for_ack(sk);
}

int rtnl_mroute_delete(struct nl_sock *sk, struct rtnl_route *mr, int flags)
{
	struct nl_msg *msg;
	int err;

	if ((err = rtnl_mroute_build_del_request(mr, flags, &msg)) < 0)
		return err;

	err = nl_send_auto_complete(sk, msg);
	nlmsg_free(msg);
	if (err < 0)
		return err;

	return wait_for_ack(sk);
}

/** @} */

struct rtnl_route *rtnl_mroute_alloc(void)
{
	return (struct rtnl_route *) nl_object_alloc(&mroute_obj_ops);
}

static void mroute_constructor(struct nl_object *c)
{
	struct rtnl_route *r = (struct rtnl_route *) c;

	r->rt_family = RTNL_FAMILY_IPMR;
	r->rt_scope = RT_SCOPE_UNIVERSE;
	r->rt_table = RT_TABLE_DEFAULT;
	r->rt_protocol = RTPROT_STATIC;
	r->rt_type = RTN_MULTICAST;

	nl_init_list_head(&r->rt_nexthops);
}

static void mroute_free_data(struct nl_object *c)
{
	struct rtnl_route *r = (struct rtnl_route *) c;
	struct rtnl_nexthop *nh, *tmp;

	if (r == NULL)
		return;

	nl_addr_put(r->rt_dst);
	nl_addr_put(r->rt_src);

	nl_list_for_each_entry_safe(nh, tmp, &r->rt_nexthops, rtnh_list) {
		rtnl_route_remove_nexthop(r, nh);
		rtnl_route_nh_free(nh);
	}
}

static int mroute_clone(struct nl_object *_dst, struct nl_object *_src)
{
	struct rtnl_route *dst = (struct rtnl_route *) _dst;
	struct rtnl_route *src = (struct rtnl_route *) _src;
	struct rtnl_nexthop *nh, *new;

	if (src->rt_dst)
		if (!(dst->rt_dst = nl_addr_clone(src->rt_dst)))
			return -NLE_NOMEM;

	if (src->rt_src)
		if (!(dst->rt_src = nl_addr_clone(src->rt_src)))
			return -NLE_NOMEM;

	/* Will be inc'ed again while adding the nexthops of the source */
	dst->rt_nr_nh = 0;

	nl_init_list_head(&dst->rt_nexthops);
	nl_list_for_each_entry(nh, &src->rt_nexthops, rtnh_list) {
		new = rtnl_route_nh_clone(nh);
		if (!new)
			return -NLE_NOMEM;

		rtnl_route_add_nexthop(dst, new);
	}

	return 0;
}

static uint64_t mroute_compare(struct nl_object *_a, struct nl_object *_b,
			       uint64_t attrs, int flags)
{
	struct rtnl_route *a = (struct rtnl_route *) _a;
	struct rtnl_route *b = (struct rtnl_route *) _b;
	struct rtnl_nexthop *nh_a, *nh_b;
	int found;
	uint64_t diff = 0;

#define ROUTE_DIFF(ATTR, EXPR) ATTR_DIFF(attrs, ROUTE_ATTR_##ATTR, a, b, EXPR)

	diff |= ROUTE_DIFF(FAMILY,	a->rt_family != b->rt_family);
	diff |= ROUTE_DIFF(TOS,		a->rt_tos != b->rt_tos);
	diff |= ROUTE_DIFF(TABLE,	a->rt_table != b->rt_table);
	diff |= ROUTE_DIFF(PROTOCOL,	a->rt_protocol != b->rt_protocol);
	diff |= ROUTE_DIFF(SCOPE,	a->rt_scope != b->rt_scope);
	diff |= ROUTE_DIFF(TYPE,	a->rt_type != b->rt_type);
	diff |= ROUTE_DIFF(DST,		nl_addr_cmp(a->rt_dst, b->rt_dst));
	diff |= ROUTE_DIFF(SRC,		nl_addr_cmp(a->rt_src, b->rt_src));
	diff |= ROUTE_DIFF(IIF,		a->rt_iif != b->rt_iif);

	if (flags & LOOSE_COMPARISON) {
		nl_list_for_each_entry(nh_b, &b->rt_nexthops, rtnh_list) {
			found = 0;
			nl_list_for_each_entry(nh_a, &a->rt_nexthops,
					       rtnh_list) {
				if (!rtnl_route_nh_compare(nh_a, nh_b,
							nh_b->ce_mask, 1)) {
					found = 1;
					break;
				}
			}

			if (!found)
				goto nh_mismatch;
		}

		diff |= ROUTE_DIFF(FLAGS,
			  (a->rt_flags ^ b->rt_flags) & b->rt_flag_mask);
	} else {
		if (a->rt_nr_nh != b->rt_nr_nh)
			goto nh_mismatch;

		/* search for a dup in each nh of a */
		nl_list_for_each_entry(nh_a, &a->rt_nexthops, rtnh_list) {
			found = 0;
			nl_list_for_each_entry(nh_b, &b->rt_nexthops,
					       rtnh_list) {
				if (!rtnl_route_nh_compare(nh_a, nh_b, ~0, 0)) {
					found = 1;
					break;
				}
			}
			if (!found)
				goto nh_mismatch;
		}

		/* search for a dup in each nh of b, covers case where a has
		 * dupes itself */
		nl_list_for_each_entry(nh_b, &b->rt_nexthops, rtnh_list) {
			found = 0;
			nl_list_for_each_entry(nh_a, &a->rt_nexthops,
					       rtnh_list) {
				if (!rtnl_route_nh_compare(nh_a, nh_b, ~0, 0)) {
					found = 1;
					break;
				}
			}
			if (!found)
				goto nh_mismatch;
		}

		diff |= ROUTE_DIFF(FLAGS, a->rt_flags != b->rt_flags);
	}

out:
	return diff;

nh_mismatch:
	diff |= ROUTE_DIFF(MULTIPATH, 1);
	goto out;

#undef ROUTE_DIFF
}

static int mroute_parse_addr(struct rtnl_route *mr, struct nlattr *attr,
			      struct rtmsg *rtm, int src)
{
	struct nl_addr *addr = NULL;
	int err = -NLE_NOMEM;

	if (attr) {
		if (!(addr = nl_addr_alloc_attr(attr, mr->rt_family)))
			goto errout;
	} else {
		if (!(addr = nl_addr_alloc(0)))
			goto errout;
		nl_addr_set_family(addr, mr->rt_family);
	}

	if (src) {
		nl_addr_set_prefixlen(addr, rtm->rtm_src_len);
		rtnl_route_set_src(mr, addr);
	} else {
		nl_addr_set_prefixlen(addr, rtm->rtm_dst_len);
		rtnl_route_set_dst(mr, addr);
	}

	nl_addr_put(addr);
	return 0;

 errout:
	return err;
}

static int rtnl_mroute_parse(struct nlmsghdr *nlh, struct rtnl_route **result)
{
	struct rtmsg *rtm;
	struct rtnl_route *mroute;
	struct nlattr *tb[RTA_MAX + 1];
	int err = 0;

	mroute = rtnl_mroute_alloc();
	if (!mroute)
		return -NLE_NOMEM;

	mroute->ce_msgtype = nlh->nlmsg_type;

	err = nlmsg_parse(nlh, sizeof(struct rtmsg), tb, RTA_MAX, mroute_policy);
	if (err < 0)
		goto errout;

	rtm = nlmsg_data(nlh);
	mroute->rt_family = rtm->rtm_family;
	mroute->rt_tos = rtm->rtm_tos;
	mroute->rt_table = rtm->rtm_table;
	mroute->rt_type = rtm->rtm_type;
	mroute->rt_scope = rtm->rtm_scope;
	mroute->rt_protocol = rtm->rtm_protocol;
	mroute->rt_flags = rtm->rtm_flags;

	mroute->ce_mask |= ROUTE_ATTR_FAMILY | ROUTE_ATTR_TOS |
		           ROUTE_ATTR_TABLE | ROUTE_ATTR_TYPE |
			   ROUTE_ATTR_SCOPE | ROUTE_ATTR_PROTOCOL |
			   ROUTE_ATTR_FLAGS;

	if (tb[RTA_TABLE])
		rtnl_route_set_table(mroute, nla_get_u32(tb[RTA_TABLE]));

	if (tb[RTA_IIF])
		rtnl_route_set_iif(mroute, nla_get_u32(tb[RTA_IIF]));

	if (tb[RTA_DST]) {
		if ((err = mroute_parse_addr(mroute, tb[RTA_DST], rtm, 0)) < 0)
			goto errout;
	} else {
		if ((err = mroute_parse_addr(mroute, NULL, rtm, 0)) < 0)
			goto errout;
	}

	if (tb[RTA_SRC]) {
		if ((err = mroute_parse_addr(mroute, tb[RTA_SRC], rtm, 1)) < 0)
			goto errout;
	} else {
		if ((err = mroute_parse_addr(mroute, NULL, rtm, 1)) < 0)
			goto errout;
	}

	if (tb[RTA_MULTIPATH])
		if ((err = rtnl_route_parse_multipath(mroute, tb[RTA_MULTIPATH])) < 0)
			goto errout;

	*result = mroute;
	return 0;

 errout:
	rtnl_route_put(mroute);
	return err;
}

static int mroute_msg_parser(struct nl_cache_ops *cops, struct sockaddr_nl *who,
			     struct nlmsghdr *nlh, struct nl_parser_param *pp)
{
	struct rtnl_route *mr = NULL;
	int err;

	if ((err = rtnl_mroute_parse(nlh, &mr)) < 0)
		return err;

	err = pp->pp_cb((struct nl_object *) mr, pp);

	rtnl_route_put(mr);
	return err;
}

static int mroute_request_update(struct nl_cache *c, struct nl_sock *h)
{
	struct rtmsg rhdr = {
		.rtm_family = c->c_iarg1,
	};

	if (c->c_iarg2 & ROUTE_CACHE_CONTENT)
		rhdr.rtm_flags |= RTM_F_CLONED;

	return nl_send_simple(h, RTM_GETROUTE, NLM_F_DUMP, &rhdr, sizeof(rhdr));
}

static struct nl_af_group mroute_groups[] = {
	{ RTNL_FAMILY_IPMR,	RTNLGRP_IPV4_MROUTE },
	{ RTNL_FAMILY_IP6MR,	RTNLGRP_IPV6_MROUTE },
	{ END_OF_GROUP_LIST },
};

/** @cond SKIP */
struct nl_object_ops mroute_obj_ops = {
	.oo_name		= "route/mroute",
	.oo_size		= sizeof(struct rtnl_route),
	.oo_constructor		= mroute_constructor,
	.oo_free_data		= mroute_free_data,
	.oo_clone		= mroute_clone,
	.oo_dump = {
	    [NL_DUMP_LINE]	= route_dump_line,
	    [NL_DUMP_DETAILS]	= route_dump_details,
	},
	.oo_compare		= mroute_compare,
	.oo_attrs2str		= route_attrs2str,
	.oo_id_attrs		= (ROUTE_ATTR_FAMILY | ROUTE_ATTR_TABLE |
				   ROUTE_ATTR_DST | ROUTE_ATTR_IIF |
				   ROUTE_ATTR_TYPE | ROUTE_ATTR_FLAGS),
	.oo_id_attrs_get	= route_id_attrs_get,
};

static struct nl_cache_ops rtnl_mroute_ops = {
	.co_name		= "route/mroute",
	.co_hdrsize		= sizeof(struct rtmsg),
	.co_flags               = NL_CACHE_AF_ITER,
	.co_msgtypes		= {
					{ RTM_NEWROUTE, NL_ACT_NEW, "new" },
					{ RTM_DELROUTE, NL_ACT_DEL, "del" },
					{ RTM_GETROUTE, NL_ACT_GET, "get" },
					END_OF_MSGTYPES_LIST,
				  },
	.co_protocol		= NETLINK_ROUTE,
	.co_groups		= mroute_groups,
	.co_request_update	= mroute_request_update,
	.co_msg_parser		= mroute_msg_parser,
	.co_obj_ops		= &mroute_obj_ops,
};

static void __init mroute_init(void)
{
	nl_cache_mngt_register(&rtnl_mroute_ops);
}

static void __exit mroute_exit(void)
{
	nl_cache_mngt_unregister(&rtnl_mroute_ops);
}

/** @endcond */
/** @} */
