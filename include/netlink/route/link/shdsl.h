/*
 * netlink/route/link/shdsl.h		SHDSL Interface
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2017 Volodymyr Bendiuga <volodymyr.bendiuga@gmail.com>
 */

#ifndef NETLINK_LINK_SHDSL_H_
#define NETLINK_LINK_SHDSL_H_

#include <netlink/netlink.h>
#include <netlink/route/link.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct rtnl_link	*rtnl_link_shdsl_alloc(void);
extern int rtnl_link_is_shdsl(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_enabled(struct rtnl_link *link, uint8_t val);
extern int rtnl_link_shdsl_get_enabled(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_role(struct rtnl_link *link, uint8_t role);
extern int rtnl_link_shdsl_get_role(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_lff(struct rtnl_link *link, uint8_t lff);
extern int rtnl_link_shdsl_get_lff(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_ghs_thr(struct rtnl_link *link, uint32_t thr);
extern int rtnl_link_shdsl_get_ghs_thr(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_rate_limit(struct rtnl_link *link, uint32_t rl);
extern int rtnl_link_shdsl_get_rate_limit(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_rate(struct rtnl_link *link, uint32_t rate);
extern int rtnl_link_shdsl_get_rate(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_noise_margin(struct rtnl_link *link, uint8_t noise);
extern int rtnl_link_shdsl_get_noise_margin(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_nonstrict(struct rtnl_link *link, uint8_t nonstrict);
extern int rtnl_link_shdsl_get_nonstrict(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_flow_control(struct rtnl_link *link, uint8_t fc);
extern int rtnl_link_shdsl_get_flow_control(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_priority(struct rtnl_link *link, uint8_t prio);
extern int rtnl_link_shdsl_get_priority(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_prio_mode(struct rtnl_link *link, uint8_t mode);
extern int rtnl_link_shdsl_get_prio_mode(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_default_vid(struct rtnl_link *link, uint32_t vid);
extern int rtnl_link_shdsl_get_default_vid(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_shaping(struct rtnl_link *link, uint32_t rate);
extern int rtnl_link_shdsl_get_shaping(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_low_jitter(struct rtnl_link *link, uint8_t val);
extern int rtnl_link_shdsl_get_low_jitter(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_emf(struct rtnl_link *link, uint8_t val);
extern int rtnl_link_shdsl_get_emf(struct rtnl_link *link);
extern int rtnl_link_shdsl_set_paf(struct rtnl_link *link, uint8_t val);
extern int rtnl_link_shdsl_get_paf(struct rtnl_link *link);
#ifdef __cplusplus
}
#endif

#endif /* NETLINK_LINK_SHDSL_H_ */
