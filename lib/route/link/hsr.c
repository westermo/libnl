/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (c) 2023 Volodymyr Bendiuga <volodymyr.bendiuga@gwestermo.com>
 */

/**
 * @ingroup link
 * @defgroup HSR/PRP
 *
 * @details
 * \b Link Type Name: "hsr"
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
#include <netlink/route/link/hsr.h>

#include <linux/if_ether.h>
#include <linux/if_link.h>


#define HSR_ATTR_SLAVE1           (1 << 0)
#define HSR_ATTR_SLAVE2           (1 << 1)
#define HSR_ATTR_SUPERVISION      (1 << 2)
#define HSR_ATTR_VERSION          (1 << 3)
#define HSR_ATTR_PROTOCOL         (1 << 4)
#define HSR_ATTR_SUPERVISION_ADDR (1 << 5)
#define HSR_ATTR_SEQ_NR           (1 << 6)
#define HSR_ATTR_EFT              (1 << 7)

struct hsr_info {
	uint32_t hi_slave1;
	uint32_t hi_slave2;
	uint8_t  hi_supervision;
	uint8_t  hi_version;
	uint8_t  hi_protocol;
	unsigned char *hi_sv_addr;
	uint16_t hi_seq_nr;
	uint32_t hi_eft;
	uint32_t hi_mask;
};

static struct nla_policy hsr_policy[IFLA_HSR_MAX+1] = {
	[IFLA_HSR_SLAVE1]		= { .type = NLA_U32 },
	[IFLA_HSR_SLAVE2]		= { .type = NLA_U32 },
	[IFLA_HSR_MULTICAST_SPEC]	= { .type = NLA_U8 },
	[IFLA_HSR_VERSION]	        = { .type = NLA_U8 },
	[IFLA_HSR_SUPERVISION_ADDR]	= { .minlen = ETH_ALEN },
	[IFLA_HSR_SEQ_NR]		= { .type = NLA_U16 },
	[IFLA_HSR_PROTOCOL]		= { .type = NLA_U8 },
	[IFLA_HSR_EFT]		        = { .type = NLA_U32 },
};


static int hsr_alloc(struct rtnl_link *link)
{
	if (!link->l_info) {
		link->l_info = malloc(sizeof(struct hsr_info));
		if (!link->l_info)
			return -NLE_NOMEM;
	}

	memset(link->l_info, 0, sizeof(struct hsr_info));

	return 0;
}

static void hsr_free(struct rtnl_link *link)
{
	free(link->l_info);
	link->l_info = NULL;
}

static int hsr_clone(struct rtnl_link *dst, struct rtnl_link *src)
{
	struct hsr_info *copy, *info = src->l_info;
	int err;

	dst->l_info = NULL;
	if ((err = rtnl_link_set_type(dst, "hsr")) < 0)
		return err;

	copy = dst->l_info;
	if (!info || !copy)
		return -NLE_NOMEM;

	memcpy(copy, info, sizeof(struct hsr_info));

	return 0;
}

static int hsr_parse(struct rtnl_link *link, struct nlattr *data,
		     struct nlattr *xstats)
{
	struct nlattr *tb[IFLA_HSR_MAX+1];
	struct hsr_info *info;
	int err;

	NL_DBG(3, "Parsing hsr info\n");

	if ((err = nla_parse_nested(tb, IFLA_HSR_MAX, data, hsr_policy)) < 0)
		goto out;

	if ((err = hsr_alloc(link)) < 0)
		goto out;

	info = link->l_info;

	if (tb[IFLA_HSR_SLAVE1]) {
		info->hi_slave1 = nla_get_u32(tb[IFLA_HSR_SLAVE1]);
		info->hi_mask |= HSR_ATTR_SLAVE1;
	}

	if (tb[IFLA_HSR_SLAVE2]) {
		info->hi_slave2 = nla_get_u32(tb[IFLA_HSR_SLAVE2]);
		info->hi_mask |= HSR_ATTR_SLAVE2;
	}

	if (tb[IFLA_HSR_MULTICAST_SPEC]) {
		info->hi_supervision = nla_get_u8(tb[IFLA_HSR_MULTICAST_SPEC]);
		info->hi_mask |= HSR_ATTR_SUPERVISION;
	}

	if (tb[IFLA_HSR_VERSION]) {
		info->hi_version = nla_get_u8(tb[IFLA_HSR_VERSION]);
		info->hi_mask |= HSR_ATTR_VERSION;
	}

	if (tb[IFLA_HSR_PROTOCOL]) {
		info->hi_protocol = nla_get_u8(tb[IFLA_HSR_PROTOCOL]);
		info->hi_mask |= HSR_ATTR_PROTOCOL;
	}

	if (tb[IFLA_HSR_SUPERVISION_ADDR]) {
		info->hi_sv_addr = nla_data(tb[IFLA_HSR_SUPERVISION_ADDR]);
		info->hi_mask |= HSR_ATTR_SUPERVISION_ADDR;
	}

	if (tb[IFLA_HSR_SEQ_NR]) {
		info->hi_seq_nr = nla_get_u16(tb[IFLA_HSR_SEQ_NR]);
		info->hi_mask |= HSR_ATTR_SEQ_NR;
	}

	if (tb[IFLA_HSR_EFT]) {
		info->hi_eft = nla_get_u32(tb[IFLA_HSR_EFT]);
		info->hi_mask |= HSR_ATTR_EFT;
	}

 out:
	return err;
}

static int hsr_put_attrs(struct nl_msg *msg, struct rtnl_link *link)
{
	struct hsr_info *info = link->l_info;
	struct nlattr *data;

	if (!(data = nla_nest_start(msg, IFLA_INFO_DATA)))
		return -NLE_MSGSIZE;

	if (info->hi_mask & HSR_ATTR_SLAVE1)
		NLA_PUT_U32(msg, IFLA_HSR_SLAVE1, info->hi_slave1);

	if (info->hi_mask & HSR_ATTR_SLAVE2)
		NLA_PUT_U32(msg, IFLA_HSR_SLAVE2, info->hi_slave2);

	if (info->hi_mask & HSR_ATTR_SUPERVISION)
		NLA_PUT_U8(msg, IFLA_HSR_MULTICAST_SPEC, info->hi_supervision);

	if (info->hi_mask & HSR_ATTR_VERSION)
		NLA_PUT_U8(msg, IFLA_HSR_VERSION, info->hi_version);

	if (info->hi_mask & HSR_ATTR_PROTOCOL)
		NLA_PUT_U8(msg, IFLA_HSR_PROTOCOL, info->hi_protocol);

	if (info->hi_mask & HSR_ATTR_EFT)
		NLA_PUT_U32(msg, IFLA_HSR_EFT, info->hi_eft);

	nla_nest_end(msg, data);

	return 0;

 nla_put_failure:
	return -NLE_MSGSIZE;
}

static void hsr_dump_line(struct rtnl_link *link, struct nl_dump_params *p)
{
	nl_dump(p, "HSR/PRP : %s", link->l_name);
}

static void hsr_dump_details(struct rtnl_link *link, struct nl_dump_params *p)
{
	struct hsr_info *info = link->l_info;

	if (info->hi_mask & HSR_ATTR_SLAVE1) {
		nl_dump(p, "      slave1-ifi ");
		nl_dump_line(p, "%d\n", info->hi_slave1);
	}

	if (info->hi_mask & HSR_ATTR_SLAVE2) {
		nl_dump(p, "      slave2-ifi ");
		nl_dump_line(p, "%d\n", info->hi_slave2);
	}

	if (info->hi_mask & HSR_ATTR_SUPERVISION) {
		nl_dump(p, "      supervision ");
		nl_dump_line(p, "%d\n", info->hi_supervision);
	}

	if (info->hi_mask & HSR_ATTR_VERSION) {
		nl_dump(p, "      protocol version ");
		nl_dump_line(p, "%s\n", info->hi_version == 0 ? "2010" : "2012");
	}

	if (info->hi_mask & HSR_ATTR_PROTOCOL) {
		nl_dump(p, "      protocol ");
		nl_dump_line(p, "%s\n", info->hi_protocol == 0 ? "HSR" : "PRP");
	}
}

static struct rtnl_link_info_ops hsr_info_ops = {
	.io_name		= "hsr",
	.io_alloc		= hsr_alloc,
	.io_free		= hsr_free,
	.io_clone		= hsr_clone,
	.io_parse		= hsr_parse,
	.io_put_attrs		= hsr_put_attrs,
	.io_dump = {
	    [NL_DUMP_LINE]	= hsr_dump_line,
	    [NL_DUMP_DETAILS]	= hsr_dump_details,
	},
};


#define IS_HSR_LINK_ASSERT(link) \
	if (!rtnl_link_is_hsr(link)) { \
		APPBUG("expecting a link object of type HSR."); \
		return -NLE_OPNOTSUPP; \
	}


/**
 * @name Attribute Modifications
 * @{
 */

/**
 * Get ifindex of the first of the two ring ports
 * @arg link		HSR link
 * @arg slave1		ifindex of the first of the two ring ports
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_hsr_get_slave1(struct rtnl_link *link,  uint32_t *slave1)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	*slave1 = info->hi_slave1;

	return 0;
}

/**
 * Set slave1 for an HSR link
 * @arg link        HSR link
 * @arg slave1      ifindex of the first of the two ring ports
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_hsr_set_slave1(struct rtnl_link *link,  uint32_t slave1)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	info->hi_slave1 = slave1;
	info->hi_mask |= HSR_ATTR_SLAVE1;

	return 0;
}

/**
 * Get ifindex of the second of the two ring ports
 * @arg link		HSR link
 * @arg slave2		ifindex of the second of the two ring ports
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_hsr_get_slave2(struct rtnl_link *link,  uint32_t *slave2)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	*slave2 = info->hi_slave2;

	return 0;
}

/**
 * Set slave2 for an HSR link
 * @arg link        HSR link
 * @arg slave2      ifindex of the second of the two ring ports
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_hsr_set_slave2(struct rtnl_link *link,  uint32_t slave2)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	info->hi_slave2 = slave2;
	info->hi_mask |= HSR_ATTR_SLAVE2;

	return 0;
}

/**
 * Get the last byte of the mcast address used for supervision
 * @arg link		HSR link
 * @arg sv		supervision byte
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_hsr_get_supervision(struct rtnl_link *link,  uint8_t *sv)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	*sv = info->hi_supervision;

	return 0;
}

/**
 * Set supervision for an HSR link
 * @arg link    HSR link
 * @arg sv      last byte of the mcast address (0 - 255)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_hsr_set_supervision(struct rtnl_link *link,  uint8_t sv)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	info->hi_supervision = sv;
	info->hi_mask |= HSR_ATTR_SUPERVISION;

	return 0;
}

/**
 * Get protocol version
 * @arg link		HSR link
 * @arg ver		protocol version
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_hsr_get_version(struct rtnl_link *link,  uint8_t *ver)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	*ver = info->hi_version;

	return 0;
}

/**
 * Set protocol version for an HSR link
 * @arg link     HSR link
 * @arg ver      protocol version (0 - 2010 version; 1 - 2012 version)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_hsr_set_version(struct rtnl_link *link,  uint8_t ver)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	info->hi_version = ver;
	info->hi_mask |= HSR_ATTR_VERSION;

	return 0;
}

/**
 * Get protocol
 * @arg link		HSR link
 * @arg proto		protocol
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_hsr_get_proto(struct rtnl_link *link, uint8_t *proto)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	*proto = info->hi_protocol;

	return 0;
}

/**
 * Set protocol for an HSR link
 * @arg link        HSR link
 * @arg proto       HSR protocol (0 - HSR; 1 - PRP)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_hsr_set_proto(struct rtnl_link *link, uint8_t proto)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	info->hi_protocol = proto;
	info->hi_mask |= HSR_ATTR_PROTOCOL;

	return 0;
}

/**
 * Get Entry Forget Time
 * @arg link		HSR link
 * @arg eft		entry forget time
 *
 * @return 0 on success or a negative error code otherwise.
 */
int rtnl_hsr_get_eft(struct rtnl_link *link, uint32_t *eft)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	*eft = info->hi_eft;

	return 0;
}

/**
 * Set Entry Forget Time for an HSR link
 * @arg link        HSR link
 * @arg eft         Entry Forget Time (0 - 7)
 *
 * @return 0 on success or negative error code in case of an error
 */
int rtnl_hsr_set_eft(struct rtnl_link *link, uint32_t eft)
{
	struct hsr_info *info = link->l_info;

	IS_HSR_LINK_ASSERT(link);

	info->hi_eft = eft;
	info->hi_mask |= HSR_ATTR_EFT;

	return 0;
}


/**
 * Allocate link object of type HSR
 * @arg name		(optional) name of the HSR link
 *
 * @return Allocated link object or NULL.
 */
struct rtnl_link *rtnl_link_hsr_alloc(char *name)
{
	struct rtnl_link *link;
	int err;

	if (!(link = rtnl_link_alloc()))
		return NULL;

	if (name)
		rtnl_link_set_name(link, name);

	if ((err = rtnl_link_set_type(link, "hsr")) < 0) {
		rtnl_link_put(link);
		return NULL;
	}

	return link;
}

/**
 * Check if the link is of type HSR
 * @arg link		Link object
 *
 * @return True if link is an HSR link, otherwise false.
 */
int rtnl_link_is_hsr(struct rtnl_link *link)
{
	return link->l_info_ops && !strcmp(link->l_info_ops->io_name, "hsr");
}

/**
 * Create a new kernel HSR device
 * @arg sock		netlink socket
 * @arg link		HSR link to be added to kernel
 *
 * Creates a new hsr device in the kernel
 *
 * @return 0 on success or a negative error code
 */
int rtnl_link_hsr_add(struct nl_sock *sock, struct rtnl_link *link)
{
	return rtnl_link_add(sock, link, NLM_F_CREATE | NLM_F_EXCL);
}

/**
 * @}
 */


static void __init hsr_init(void)
{
	rtnl_link_register_info(&hsr_info_ops);
}

static void __exit hsr_exit(void)
{
	rtnl_link_unregister_info(&hsr_info_ops);
}

/**
 * @}
 */
