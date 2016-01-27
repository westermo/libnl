/*
 * netlink/route/act/police.h	police action
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2016 Volodymyr Bendiuga <volodymyr.bendiuga@westermo.se>
 */

#ifndef NETLINK_POLICE_H_
#define NETLINK_POLICE_H_

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/action.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int rtnl_police_set_action(struct rtnl_act *act, int action);
extern int rtnl_police_get_action(struct rtnl_act *act);
extern int rtnl_police_set_bucket(struct rtnl_act *act, int bkt);
extern int rtnl_police_get_bucket(struct rtnl_act *act);
extern int rtnl_police_set_burst(struct rtnl_act *act, int burst, char *sz);
extern int rtnl_police_get_burst(struct rtnl_act *act);
extern int rtnl_police_set_mtu(struct rtnl_act *act, int mtu, char *sz);
extern int rtnl_police_get_mtu(struct rtnl_act *act);
extern int rtnl_police_set_mpu(struct rtnl_act *act, int mpu, char *sz);
extern int rtnl_police_get_mpu(struct rtnl_act *act);
extern int rtnl_police_set_rate(struct rtnl_act *act, int rate, char *units);
extern int rtnl_police_get_rate(struct rtnl_act *act);
extern int rtnl_police_set_overhead(struct rtnl_act *act, int ovrhd);
extern int rtnl_police_get_overhead(struct rtnl_act *act);
extern int rtnl_police_set_linklayer(struct rtnl_act *act, int ll);
extern int rtnl_police_get_linklayer(struct rtnl_act *act);


#ifdef __cplusplus
}
#endif

#endif	/* NETLINK_POLICE_H */
