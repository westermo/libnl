/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (c) 2023 Dragos Galalae <kdraga@gwestermo.com>
 */

/**
 * @ingroup link
 * @defgroup INTERLINK/PRP/HSR
 *
 * @details
 * \b Link Type Name: "interlink"
 *
 * @{
 */

#include <netlink-private/netlink.h>
#include <netlink/netlink.h>
#include <netlink/attr.h>
#include <netlink/utils.h>
#include <netlink/object.h>
#include <netlink/route/rtnl.h>
#include <netlink-private/route/link/api.h>
#include <netlink/route/link/interlink.h>

#include <linux/if_ether.h>
#include <linux/if_link.h>

#define INTERLINK_ATTR_DEV           	(1 << 0)
#define INTERLINK_ATTR_SUPERVISION      (1 << 1)
#define INTERLINK_ATTR_VERSION          (1 << 2)
#define INTERLINK_ATTR_SUPERVISION_ADDR (1 << 3)
#define INTERLINK_ATTR_SEQ_NR           (1 << 4)
#define INTERLINK_ATTR_PROTOCOL         (1 << 5)
#define INTERLINK_ATTR_LAN_ID           (1 << 6)
#define INTERLINK_ATTR_NET_ID           (1 << 7)

struct interlink_info {
	uint32_t hi_dev;
	uint8_t  hi_supervision;
	uint8_t  hi_version;
	uint8_t  hi_protocol;
	unsigned char *hi_sv_addr;
	uint16_t hi_seq_nr;
	uint32_t hi_mask;
	uint8_t hi_lan_id;
	uint8_t hi_net_id;
};

static struct nla_policy interlink_policy[IFLA_INTERLINK_MAX+1] = {
	[IFLA_INTERLINK_DEV]		= { .type = NLA_U32 },
	[IFLA_INTERLINK_MULTICAST_SPEC]	= { .type = NLA_U8 },
	[IFLA_INTERLINK_VERSION]	        = { .type = NLA_U8 },
	[IFLA_INTERLINK_SUPERVISION_ADDR]	= { .minlen = ETH_ALEN },
	[IFLA_INTERLINK_SEQ_NR]		= { .type = NLA_U16 },
	[IFLA_INTERLINK_PROTOCOL]		= { .type = NLA_U8 },
	[IFLA_INTERLINK_LAN_ID]		= { .type = NLA_U8 },
	[IFLA_INTERLINK_NET_ID]		= { .type = NLA_U8 },
};


static int interlink_alloc(struct rtnl_link *link)
{
	if (!link->l_info) {
		link->l_info = malloc(sizeof(struct interlink_info));
		if (!link->l_info)
			return -NLE_NOMEM;
	}

	memset(link->l_info, 0, sizeof(struct interlink_info));

	return 0;
}

static void interlink_free(struct rtnl_link *link)
{
	free(link->l_info);
	link->l_info = NULL;
}

static int interlink_clone(struct rtnl_link *dst, struct rtnl_link *src)
{
	struct interlink_info *copy, *info = src->l_info;
	int err;

	dst->l_info = NULL;
	if ((err = rtnl_link_set_type(dst, "interlink")) < 0)
		return err;

	copy = dst->l_info;
	if (!info || !copy)
		return -NLE_NOMEM;

	memcpy(copy, info, sizeof(struct interlink_info));

	return 0;
}

static int interlink_parse(struct rtnl_link *link, struct nlattr *data,
		     struct nlattr *xstats)
{
	struct nlattr *tb[IFLA_INTERLINK_MAX+1];
	struct interlink_info *info;
	int err;

	NL_DBG(3, "Parsing interlink info\n");

	if ((err = nla_parse_nested(tb, IFLA_INTERLINK_MAX, data, interlink_policy)) < 0)
		goto out;

	if ((err = interlink_alloc(link)) < 0)
		goto out;

	info = link->l_info;

	if (tb[IFLA_INTERLINK_DEV]) {
		info->hi_dev = nla_get_u32(tb[IFLA_INTERLINK_DEV]);
		info->hi_mask |= INTERLINK_ATTR_DEV;
	}

	if (tb[IFLA_INTERLINK_MULTICAST_SPEC]) {
		info->hi_supervision = nla_get_u8(tb[IFLA_INTERLINK_MULTICAST_SPEC]);
		info->hi_mask |= INTERLINK_ATTR_SUPERVISION;
	}

	if (tb[IFLA_INTERLINK_VERSION]) {
		info->hi_version = nla_get_u8(tb[IFLA_INTERLINK_VERSION]);
		info->hi_mask |= INTERLINK_ATTR_VERSION;
	}

	if (tb[IFLA_INTERLINK_PROTOCOL]) {
		info->hi_protocol = nla_get_u8(tb[IFLA_INTERLINK_PROTOCOL]);
		info->hi_mask |= INTERLINK_ATTR_PROTOCOL;
	}

	if (tb[IFLA_INTERLINK_SUPERVISION_ADDR]) {
		info->hi_sv_addr = nla_data(tb[IFLA_INTERLINK_SUPERVISION_ADDR]);
		info->hi_mask |= INTERLINK_ATTR_SUPERVISION_ADDR;
	}

	if (tb[IFLA_INTERLINK_SEQ_NR]) {
		info->hi_seq_nr = nla_get_u16(tb[IFLA_INTERLINK_SEQ_NR]);
		info->hi_mask |= INTERLINK_ATTR_SEQ_NR;
	}

	if (tb[IFLA_INTERLINK_LAN_ID]) {
		info->hi_lan_id = nla_get_u8(tb[IFLA_INTERLINK_LAN_ID]);
		info->hi_mask |= INTERLINK_ATTR_LAN_ID;
	}

	if (tb[IFLA_INTERLINK_NET_ID]) {
		info->hi_net_id = nla_get_u8(tb[IFLA_INTERLINK_NET_ID]);
		info->hi_mask |= INTERLINK_ATTR_NET_ID;
	}

 out:
	return err;
}

static int interlink_put_attrs(struct nl_msg *msg, struct rtnl_link *link)
{
	struct interlink_info *info = link->l_info;
	struct nlattr *data;

	if (!(data = nla_nest_start(msg, IFLA_INFO_DATA)))
		return -NLE_MSGSIZE;

	if (info->hi_mask & INTERLINK_ATTR_DEV)
		NLA_PUT_U32(msg, IFLA_INTERLINK_DEV, info->hi_dev);

	if (info->hi_mask & INTERLINK_ATTR_SUPERVISION)
		NLA_PUT_U8(msg, IFLA_INTERLINK_MULTICAST_SPEC, info->hi_supervision);

	if (info->hi_mask & INTERLINK_ATTR_VERSION)
		NLA_PUT_U8(msg, IFLA_INTERLINK_VERSION, info->hi_version);

	if (info->hi_mask & INTERLINK_ATTR_PROTOCOL)
		NLA_PUT_U8(msg, IFLA_INTERLINK_PROTOCOL, info->hi_protocol);

	if (info->hi_mask & INTERLINK_ATTR_LAN_ID)
		NLA_PUT_U8(msg, IFLA_INTERLINK_LAN_ID, info->hi_lan_id);

	if (info->hi_mask & INTERLINK_ATTR_NET_ID)
		NLA_PUT_U8(msg, IFLA_INTERLINK_NET_ID, info->hi_net_id);

	nla_nest_end(msg, data);

	return 0;

 nla_put_failure:
	return -NLE_MSGSIZE;
}

static void interlink_dump_line(struct rtnl_link *link, struct nl_dump_params *p)
{
	nl_dump(p, "INTERLINK for HSR/PRP : %s", link->l_name);
}

static void interlink_dump_details(struct rtnl_link *link, struct nl_dump_params *p)
{
	struct interlink_info *info = link->l_info;

	if (info->hi_mask & INTERLINK_ATTR_DEV) {
		nl_dump(p, "      port ");
		nl_dump_line(p, "%d\n", info->hi_dev);
	}

	if (info->hi_mask & INTERLINK_ATTR_SUPERVISION) {
		nl_dump(p, "      supervision ");
		nl_dump_line(p, "%d\n", info->hi_supervision);
	}

	if (info->hi_mask & INTERLINK_ATTR_VERSION) {
		nl_dump(p, "      protocol version ");
		nl_dump_line(p, "%s\n", info->hi_version == 0 ? "2010" : "2012");
	}

	if (info->hi_mask & INTERLINK_ATTR_PROTOCOL) {
		nl_dump(p, "      protocol ");
		nl_dump_line(p, "%s\n", info->hi_protocol == 0 ? "HSR" : "PRP");
	}

	if (info->hi_mask & INTERLINK_ATTR_LAN_ID) {
		nl_dump(p, "      lan_id ");
		nl_dump_line(p, "%s\n", info->hi_lan_id == PRP_LAN_ID_A ? "A" : "B");
	}

	if (info->hi_mask & INTERLINK_ATTR_NET_ID) {
		nl_dump(p, "      net_id ");
		nl_dump_line(p, "%s\n", info->hi_net_id);
	}
}

static struct rtnl_link_info_ops interlink_info_ops = {
	.io_name		= "interlink",
	.io_alloc		= interlink_alloc,
	.io_free		= interlink_free,
	.io_clone		= interlink_clone,
	.io_parse		= interlink_parse,
	.io_put_attrs		= interlink_put_attrs,
	.io_dump = {
	    [NL_DUMP_LINE]	= interlink_dump_line,
	    [NL_DUMP_DETAILS]	= interlink_dump_details,
	},
};


#define IS_INTERLINK_LINK_ASSERT(link) \
	if (!rtnl_link_is_interlink(link)) { \
		APPBUG("expecting a link object of type INTERLINK."); \
		return -NLE_OPNOTSUPP; \
	}


/**
 * @name Attribute Modifications
 * @{
 */

/**
 * Get ifindex of the port
 * @arg link		INTERLINK link
 * @arg port		ifindex of the first of the two ring ports
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_interlink_get_port(struct rtnl_link *link,  uint32_t *port)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	*port = info->hi_dev;

	return 0;
}

/**
 * Set port for an INTERLINK link
 * @arg link        INTERLINK link
 * @arg port      ifindex of the first of the two ring ports
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_interlink_set_port(struct rtnl_link *link,  uint32_t port)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	info->hi_dev = port;
	info->hi_mask |= INTERLINK_ATTR_DEV;

	return 0;
}

/**
 * Get the last byte of the mcast address used for supervision
 * @arg link		INTERLINK link
 * @arg sv		supervision byte
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_interlink_get_supervision(struct rtnl_link *link,  uint8_t *sv)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	*sv = info->hi_supervision;

	return 0;
}

/**
 * Set supervision for an INTERLINK link
 * @arg link    INTERLINK link
 * @arg sv      last byte of the mcast address (0 - 255)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_interlink_set_supervision(struct rtnl_link *link,  uint8_t sv)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	info->hi_supervision = sv;
	info->hi_mask |= INTERLINK_ATTR_SUPERVISION;

	return 0;
}

/**
 * Get protocol version
 * @arg link		INTERLINK link
 * @arg ver		protocol version
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_interlink_get_version(struct rtnl_link *link,  uint8_t *ver)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	*ver = info->hi_version;

	return 0;
}

/**
 * Set protocol version for an INTERLINK link
 * @arg link     INTERLINK link
 * @arg ver      protocol version (0 - 2010 version; 1 - 2012 version)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_interlink_set_version(struct rtnl_link *link,  uint8_t ver)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	info->hi_version = ver;
	info->hi_mask |= INTERLINK_ATTR_VERSION;

	return 0;
}

/**
 * Get protocol
 * @arg link		INTERLINK link
 * @arg proto		protocol
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_interlink_get_proto(struct rtnl_link *link, uint8_t *proto)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	*proto = info->hi_protocol;

	return 0;
}

/**
 * Set protocol for an INTERLINK link
 * @arg link        INTERLINK link
 * @arg proto       INTERLINK protocol (0 - HSR; 1 - PRP)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_interlink_set_proto(struct rtnl_link *link, uint8_t proto)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	info->hi_protocol = proto;
	info->hi_mask |= INTERLINK_ATTR_PROTOCOL;

	return 0;
}

/**
 * Get lan ID
 * @arg link		INTERLINK link
 * @arg lan_id		lan ID
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_interlink_get_lan_id(struct rtnl_link *link, uint8_t *lan_id)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	*lan_id = info->hi_lan_id;

	return 0;
}

/**
 * Set network ID for an INTERLINK link
 * @arg link        INTERLINK link
 * @arg lan_id      INTERLINK lan id (0 - A; 1 - B)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_interlink_set_lan_id(struct rtnl_link *link, uint8_t lan_id)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	info->hi_lan_id = lan_id;
	info->hi_mask |= INTERLINK_ATTR_LAN_ID;

	return 0;
}

/**
 * Get network ID
 * @arg link		INTERLINK link
 * @arg net_id		network ID
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_interlink_get_net_id(struct rtnl_link *link, uint8_t *net_id)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	*net_id = info->hi_net_id;

	return 0;
}

/**
 * Set network ID for an INTERLINK link
 * @arg link        INTERLINK link
 * @arg net_id      INTERLINK network id (1 to 7)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_interlink_set_net_id(struct rtnl_link *link, uint8_t net_id)
{
	struct interlink_info *info = link->l_info;

	IS_INTERLINK_LINK_ASSERT(link);

	info->hi_net_id = net_id;
	info->hi_mask |= INTERLINK_ATTR_NET_ID;

	return 0;
}

/**
 * Allocate link object of type INTERLINK
 * @arg name		(optional) name of the INTERLINK link
 *
 * @return Allocated link object or NULL.
 */
struct rtnl_link *rtnl_link_interlink_alloc(char *name)
{
	struct rtnl_link *link;
	int err;

	if (!(link = rtnl_link_alloc()))
		return NULL;

	if (name)
		rtnl_link_set_name(link, name);

	if ((err = rtnl_link_set_type(link, "interlink")) < 0) {
		rtnl_link_put(link);
		return NULL;
	}

	return link;
}

/**
 * Check if the link is of type INTERLINK
 * @arg link		Link object
 *
 * @return True if link is an INTERLINK link, otherwise false.
 */
int rtnl_link_is_interlink(struct rtnl_link *link)
{
	return link->l_info_ops && !strcmp(link->l_info_ops->io_name, "interlink");
}

/**
 * Create a new kernel INTERLINK device
 * @arg sock		netlink socket
 * @arg link		INTERLINK link to be added to kernel
 *
 * Creates a new interlink device in the kernel
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_interlink_add(struct nl_sock *sock, struct rtnl_link *link)
{
	return rtnl_link_add(sock, link, NLM_F_CREATE | NLM_F_EXCL);
}

/**
 * @}
 */


static void __init interlink_init(void)
{
	rtnl_link_register_info(&interlink_info_ops);
}

static void __exit interlink_exit(void)
{
	rtnl_link_unregister_info(&interlink_info_ops);
}

/**
 * @}
 */
