#ifndef NETLINK_MROUTE_H_
#define NETLINK_MROUTE_H_

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/addr.h>
#include <netlink/data.h>
#include <netlink/route/nexthop.h>
#include <netlink/route/rtnl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtnl_route;
extern struct rtnl_route *rtnl_mroute_alloc(void);
extern int	rtnl_mroute_alloc_cache(struct nl_sock *sk, int family,
					struct nl_cache ** cache);
extern int	rtnl_mroute_add_cache(struct nl_cache *cache, struct rtnl_route *mr);
extern void	rtnl_mroute_delete_cache(struct rtnl_route *mr);
extern struct rtnl_route *rtnl_mroute_get_by_dst(struct nl_cache *cache,
						 struct nl_addr *addr);

extern int	rtnl_mroute_add(struct nl_sock *sk, struct rtnl_route *mr, int flags);
extern int	rtnl_mroute_delete(struct nl_sock *sk, struct rtnl_route *mr, int flags);

#ifdef __cplusplus
}
#endif

#endif
