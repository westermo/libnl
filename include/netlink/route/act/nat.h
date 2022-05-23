/*
 * netlink/route/act/nat.h	NAT action
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2016 Magnus Öberg <magnus.oberg@westermo.se>
 */

#ifndef NETLINK_NAT_H_
#define NETLINK_NAT_H_

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/action.h>
#include <linux/tc_act/tc_nat.h>

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int rtnl_nat_set_old_addr(struct rtnl_act *act, in_addr_t addr);
extern int rtnl_nat_get_old_addr(struct rtnl_act *act, in_addr_t *addr);
extern int rtnl_nat_set_new_addr(struct rtnl_act *act, in_addr_t addr);
extern int rtnl_nat_get_new_addr(struct rtnl_act *act, in_addr_t *addr);
extern int rtnl_nat_set_mask(struct rtnl_act *act, in_addr_t bitmask);
extern int rtnl_nat_get_mask(struct rtnl_act *act, in_addr_t *bitmask);
extern int rtnl_nat_set_flags(struct rtnl_act *act, uint32_t flags);
extern int rtnl_nat_get_flags(struct rtnl_act *act, uint32_t *flags);
extern int rtnl_nat_set_action(struct rtnl_act *act, int action);
extern int rtnl_nat_get_action(struct rtnl_act *act, int *action);

#ifdef __cplusplus
}
#endif

#endif	/* NETLINK_NAT_H */
