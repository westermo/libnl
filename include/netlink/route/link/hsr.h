/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (c) 2023 Volodymyr Bendiuga <volodymyr.bendiuga@gwestermo.com>
 */

#ifndef NETLINK_LINK_HSR_H_
#define NETLINK_LINK_HSR_H_

#include <netlink/netlink.h>
#include <netlink/route/link.h>

#ifdef __cplusplus
extern "C" {
#endif

	extern struct rtnl_link *rtnl_link_hsr_alloc(char *name);
	extern int rtnl_link_hsr_add(struct nl_sock *sock, struct rtnl_link *link);
	extern int rtnl_link_is_hsr(struct rtnl_link *link);

	extern int rtnl_hsr_get_slave1(struct rtnl_link *link,  uint32_t *slave1);
	extern int rtnl_hsr_set_slave1(struct rtnl_link *link,  uint32_t slave1);

	extern int rtnl_hsr_get_slave2(struct rtnl_link *link,  uint32_t *slave2);
	extern int rtnl_hsr_set_slave2(struct rtnl_link *link,  uint32_t slave2);

	extern int rtnl_hsr_get_supervision(struct rtnl_link *link,  uint8_t *sv);
	extern int rtnl_hsr_set_supervision(struct rtnl_link *link,  uint8_t sv);

	extern int rtnl_hsr_get_version(struct rtnl_link *link,  uint8_t *ver);
	extern int rtnl_hsr_set_version(struct rtnl_link *link,  uint8_t ver);

	extern int rtnl_hsr_get_proto(struct rtnl_link *link, uint8_t *proto);
	extern int rtnl_hsr_set_proto(struct rtnl_link *link, uint8_t proto);

#ifdef __cplusplus
}
#endif

#endif
