
#ifndef NETLINK_LINK_TUN_H_
#define NETLINK_LINK_TUN_H_

#include <netlink/netlink.h>
#include <netlink/route/link.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
struct rtnl_link *rtnl_link_tun_alloc(void);
int rtnl_link_tun_set_type(struct rtnl_link *link, uint16_t type);
uint16_t rtnl_link_tun_get_type(struct rtnl_link *link);
#endif
