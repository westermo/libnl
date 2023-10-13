/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (c) 2023 Dragos Galalae <kdraga@gwestermo.com>
 */

#ifndef NETLINK_LINK_INTERLINK_H_
#define NETLINK_LINK_INTERLINK_H_

#include <netlink/netlink.h>
#include <netlink/route/link.h>

#ifdef __cplusplus
extern "C" {
#endif

	extern struct rtnl_link *rtnl_link_interlink_alloc(char *name);
	extern int rtnl_link_interlink_add(struct nl_sock *sock, struct rtnl_link *link);
	extern int rtnl_link_is_interlink(struct rtnl_link *link);

	extern int rtnl_interlink_get_port(struct rtnl_link *link,  uint32_t *slave1);
	extern int rtnl_interlink_set_port(struct rtnl_link *link,  uint32_t slave1);

	extern int rtnl_interlink_get_supervision(struct rtnl_link *link,  uint8_t *sv);
	extern int rtnl_interlink_set_supervision(struct rtnl_link *link,  uint8_t sv);

	extern int rtnl_interlink_get_version(struct rtnl_link *link,  uint8_t *ver);
	extern int rtnl_interlink_set_version(struct rtnl_link *link,  uint8_t ver);

	extern int rtnl_interlink_get_proto(struct rtnl_link *link, uint8_t *proto);
	extern int rtnl_interlink_set_proto(struct rtnl_link *link, uint8_t proto);

	extern int rtnl_interlink_get_lan_id(struct rtnl_link *link, uint8_t *lan_id);
	extern int rtnl_interlink_set_lan_id(struct rtnl_link *link, uint8_t lan_id);

	extern int rtnl_interlink_get_net_id(struct rtnl_link *link, uint8_t *net_id);
	extern int rtnl_interlink_set_net_id(struct rtnl_link *link, uint8_t net_id);

#ifdef __cplusplus
}
#endif

#endif
