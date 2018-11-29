/*
 * netlink/route/route.h	Routes
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2003-2012 Thomas Graf <tgraf@suug.ch>
 */

#ifndef NETLINK_ROUTE_H_
#define NETLINK_ROUTE_H_

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/addr.h>
#include <netlink/data.h>
#include <netlink/route/nexthop.h>
#include <netlink/route/rtnl.h>
#include <linux/in_route.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond SKIP */
#define ROUTE_ATTR_FAMILY    0x000001
#define ROUTE_ATTR_TOS       0x000002
#define ROUTE_ATTR_TABLE     0x000004
#define ROUTE_ATTR_PROTOCOL  0x000008
#define ROUTE_ATTR_SCOPE     0x000010
#define ROUTE_ATTR_TYPE      0x000020
#define ROUTE_ATTR_FLAGS     0x000040
#define ROUTE_ATTR_DST       0x000080
#define ROUTE_ATTR_SRC       0x000100
#define ROUTE_ATTR_IIF       0x000200
#define ROUTE_ATTR_OIF       0x000400
#define ROUTE_ATTR_GATEWAY   0x000800
#define ROUTE_ATTR_PRIO      0x001000
#define ROUTE_ATTR_PREF_SRC  0x002000
#define ROUTE_ATTR_METRICS   0x004000
#define ROUTE_ATTR_MULTIPATH 0x008000
#define ROUTE_ATTR_REALMS    0x010000
#define ROUTE_ATTR_CACHEINFO 0x020000
#define ROUTE_ATTR_TTL_PROPAGATE 0x040000
/** @endcond */

/**
 * @ingroup route
 * When passed to rtnl_route_alloc_cache() the cache will
 * correspond to the contents of the routing cache instead
 * of the actual routes.
 */
#define ROUTE_CACHE_CONTENT	1

struct rtnl_route;

struct rtnl_rtcacheinfo
{
	uint32_t	rtci_clntref;
	uint32_t	rtci_last_use;
	uint32_t	rtci_expires;
	int32_t		rtci_error;
	uint32_t	rtci_used;
	uint32_t	rtci_id;
	uint32_t	rtci_ts;
	uint32_t	rtci_tsage;
};

extern struct nl_object_ops route_obj_ops;

extern struct rtnl_route *	rtnl_route_alloc(void);
extern void	rtnl_route_put(struct rtnl_route *);
extern int	rtnl_route_alloc_cache(struct nl_sock *, int, int,
				       struct nl_cache **);

extern void	rtnl_route_get(struct rtnl_route *);

extern int	rtnl_route_parse(struct nlmsghdr *, struct rtnl_route **);
extern int	rtnl_route_build_msg(struct nl_msg *, struct rtnl_route *);

extern int	rtnl_route_lookup(struct nl_sock *sk, struct nl_addr *dst,
				  struct rtnl_route **result);

extern int	rtnl_route_build_add_request(struct rtnl_route *, int,
					     struct nl_msg **);
extern int	rtnl_route_add(struct nl_sock *, struct rtnl_route *, int);
extern int	rtnl_route_build_del_request(struct rtnl_route *, int,
					     struct nl_msg **);
extern int	rtnl_route_delete(struct nl_sock *, struct rtnl_route *, int);

extern void	rtnl_route_set_table(struct rtnl_route *, uint32_t);
extern uint32_t	rtnl_route_get_table(struct rtnl_route *);
extern void	rtnl_route_set_scope(struct rtnl_route *, uint8_t);
extern uint8_t	rtnl_route_get_scope(struct rtnl_route *);
extern void	rtnl_route_set_tos(struct rtnl_route *, uint8_t);
extern uint8_t	rtnl_route_get_tos(struct rtnl_route *);
extern void	rtnl_route_set_protocol(struct rtnl_route *, uint8_t);
extern uint8_t	rtnl_route_get_protocol(struct rtnl_route *);
extern void	rtnl_route_set_priority(struct rtnl_route *, uint32_t);
extern uint32_t	rtnl_route_get_priority(struct rtnl_route *);
extern int	rtnl_route_set_family(struct rtnl_route *, uint8_t);
extern uint8_t	rtnl_route_get_family(struct rtnl_route *);
extern int	rtnl_route_set_type(struct rtnl_route *, uint8_t);
extern uint8_t	rtnl_route_get_type(struct rtnl_route *);
extern void	rtnl_route_set_flags(struct rtnl_route *, uint32_t);
extern void	rtnl_route_unset_flags(struct rtnl_route *, uint32_t);
extern uint32_t	rtnl_route_get_flags(struct rtnl_route *);
extern int	rtnl_route_set_metric(struct rtnl_route *, int, unsigned int);
extern int	rtnl_route_unset_metric(struct rtnl_route *, int);
extern int	rtnl_route_get_metric(struct rtnl_route *, int, uint32_t *);
extern int	rtnl_route_set_dst(struct rtnl_route *, struct nl_addr *);
extern struct nl_addr *rtnl_route_get_dst(struct rtnl_route *);
extern int	rtnl_route_set_src(struct rtnl_route *, struct nl_addr *);
extern struct nl_addr *rtnl_route_get_src(struct rtnl_route *);
extern int	rtnl_route_set_pref_src(struct rtnl_route *, struct nl_addr *);
extern struct nl_addr *rtnl_route_get_pref_src(struct rtnl_route *);
extern void	rtnl_route_set_iif(struct rtnl_route *, int);
extern int	rtnl_route_get_iif(struct rtnl_route *);
extern int	rtnl_route_get_src_len(struct rtnl_route *);
extern void	rtnl_route_set_ttl_propagate(struct rtnl_route *route,
					     uint8_t ttl_prop);
extern int	rtnl_route_get_ttl_propagate(struct rtnl_route *route);

extern void	rtnl_route_add_nexthop(struct rtnl_route *,
				       struct rtnl_nexthop *);
extern void	rtnl_route_remove_nexthop(struct rtnl_route *,
					  struct rtnl_nexthop *);
extern struct nl_list_head *rtnl_route_get_nexthops(struct rtnl_route *);
extern int	rtnl_route_get_nnexthops(struct rtnl_route *);

extern void	rtnl_route_foreach_nexthop(struct rtnl_route *r,
                                 void (*cb)(struct rtnl_nexthop *, void *),
                                 void *arg);

extern struct rtnl_nexthop * rtnl_route_nexthop_n(struct rtnl_route *r, int n);

extern int	rtnl_route_guess_scope(struct rtnl_route *);

extern char *	rtnl_route_table2str(int, char *, size_t);
extern int	rtnl_route_str2table(const char *);
extern int	rtnl_route_read_table_names(const char *);

extern char *	rtnl_route_proto2str(int, char *, size_t);
extern int	rtnl_route_str2proto(const char *);
extern int	rtnl_route_read_protocol_names(const char *);

extern char *	rtnl_route_metric2str(int, char *, size_t);
extern int	rtnl_route_str2metric(const char *);
extern int      rtnl_route_parse_multipath(struct rtnl_route *route, struct nlattr *attr);
extern int      rtnl_route_put_via(struct nl_msg *msg, struct nl_addr *addr);
extern uint32_t route_id_attrs_get(struct nl_object *obj);
extern char *   route_attrs2str(int attrs, char *buf, size_t len);
extern void     route_dump_line(struct nl_object *a, struct nl_dump_params *p);
extern void     route_dump_details(struct nl_object *a, struct nl_dump_params *p);

#ifdef __cplusplus
}
#endif

#endif
