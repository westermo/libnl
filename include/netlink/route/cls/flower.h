/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (c) 2018 Volodymyr Bendiuga <volodymyr.bendiuga@westermo.se>
 */

#ifndef NETLINK_FLOWER_H_
#define NETLINK_FLOWER_H_

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/classifier.h>
#include <netlink/route/action.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int rtnl_flower_set_proto(struct rtnl_cls *cls, uint16_t);
extern int rtnl_flower_get_proto(struct rtnl_cls *cls, uint16_t *);

extern int rtnl_flower_set_vlan_id(struct rtnl_cls *, uint16_t);
extern int rtnl_flower_get_vlan_id(struct rtnl_cls *, uint16_t *);

extern int rtnl_flower_set_vlan_prio(struct rtnl_cls *, uint8_t);
extern int rtnl_flower_get_vlan_prio(struct rtnl_cls *, uint8_t *);

extern int rtnl_flower_set_vlan_ethtype(struct rtnl_cls *, uint16_t);
extern int rtnl_flower_get_vlan_ethtype(struct rtnl_cls *, uint16_t *);

extern int rtnl_flower_set_dst_mac(struct rtnl_cls *, unsigned char *,
                                   unsigned char *);
extern int rtnl_flower_get_dst_mac(struct rtnl_cls *, unsigned char *,
                                   unsigned char *);

extern int rtnl_flower_set_src_mac(struct rtnl_cls *, unsigned char *,
                                   unsigned char *);
extern int rtnl_flower_get_src_mac(struct rtnl_cls *, unsigned char *,
                                   unsigned char *);

extern int rtnl_flower_set_ip_dscp(struct rtnl_cls *, uint8_t);
extern int rtnl_flower_get_ip_dscp(struct rtnl_cls *, uint8_t *);
extern int rtnl_flower_set_ip_tos(struct rtnl_cls *, uint8_t, uint8_t);
extern int rtnl_flower_get_ip_tos(struct rtnl_cls *, uint8_t *, uint8_t *);

extern int rtnl_flower_set_ip_ttl(struct rtnl_cls *, uint8_t, uint8_t);
extern int rtnl_flower_get_ip_ttl(struct rtnl_cls *, uint8_t *, uint8_t *);

extern int rtnl_flower_set_ipv4_src(struct rtnl_cls *, in_addr_t, in_addr_t);
extern int rtnl_flower_get_ipv4_src(struct rtnl_cls *, in_addr_t *,
				    in_addr_t *);
extern int rtnl_flower_set_ipv4_dst(struct rtnl_cls *, in_addr_t, in_addr_t);
extern int rtnl_flower_get_ipv4_dst(struct rtnl_cls *, in_addr_t *,
				    in_addr_t *);

extern int rtnl_flower_set_ip_proto(struct rtnl_cls *, uint8_t);
extern int rtnl_flower_get_ip_proto(struct rtnl_cls *, uint8_t *);

extern int rtnl_flower_set_icmpv6_type(struct rtnl_cls *, uint8_t, uint8_t);
extern int rtnl_flower_get_icmpv6_type(struct rtnl_cls *, uint8_t *, uint8_t *);

extern int rtnl_flower_set_flags(struct rtnl_cls *, int);
extern int rtnl_flower_get_flags(struct rtnl_cls *, int *);

extern int rtnl_flower_append_action(struct rtnl_cls *, struct rtnl_act *);
extern int rtnl_flower_del_action(struct rtnl_cls *, struct rtnl_act *);
extern struct rtnl_act* rtnl_flower_get_action(struct rtnl_cls *);

extern int rtnl_flower_set_src_port_range(struct rtnl_cls *cls, uint16_t min, uint16_t max);
extern int rtnl_flower_get_src_port_range(struct rtnl_cls *cls, uint16_t *min, uint16_t *max);
extern int rtnl_flower_set_dst_port_range(struct rtnl_cls *cls, uint16_t min, uint16_t max);
extern int rtnl_flower_get_dst_port_range(struct rtnl_cls *cls, uint16_t *min, uint16_t *max);

extern int rtnl_flower_set_tcp_src_port(struct rtnl_cls *cls, uint16_t port);
extern int rtnl_flower_set_tcp_dst_port(struct rtnl_cls *cls, uint16_t port);
extern int rtnl_flower_get_tcp_src_port(struct rtnl_cls *cls, uint16_t *port);
extern int rtnl_flower_get_tcp_dst_port(struct rtnl_cls *cls, uint16_t *port);

extern int rtnl_flower_set_udp_src_port(struct rtnl_cls *cls, uint16_t port);
extern int rtnl_flower_set_udp_dst_port(struct rtnl_cls *cls, uint16_t port);
extern int rtnl_flower_get_udp_src_port(struct rtnl_cls *cls, uint16_t *port);
extern int rtnl_flower_get_udp_dst_port(struct rtnl_cls *cls, uint16_t *port);

#ifdef __cplusplus
}
#endif

#endif
