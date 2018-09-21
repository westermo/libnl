/*
 * lib/route/cls/flower.c		Flow based traffic control filter
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2018 Volodymyr Bendiuga <volodymyr.bendiuga@gmail.com>
 */

#include <netlink-private/netlink.h>
#include <netlink-private/tc.h>
#include <netlink/netlink.h>
#include <netlink/attr.h>
#include <netlink/utils.h>
#include <netlink-private/route/tc-api.h>
#include <netlink/route/classifier.h>
#include <netlink/route/action.h>
#include <netlink/route/cls/flower.h>


/** @cond SKIP */
#define FLOWER_ATTR_FLAGS           1 << 0x0
#define FLOWER_ATTR_ACTION          1 << 0x1
#define FLOWER_ATTR_POLICE          1 << 0x2
#define FLOWER_ATTR_VLAN_ID         1 << 0x3
#define FLOWER_ATTR_VLAN_PRIO       1 << 0x4
#define FLOWER_ATTR_VLAN_ETH_TYPE   1 << 0x5
#define FLOWER_ATTR_DST_MAC         1 << 0x6
#define FLOWER_ATTR_DST_MAC_MASK    1 << 0x7
#define FLOWER_ATTR_SRC_MAC         1 << 0x8
#define FLOWER_ATTR_SRC_MAC_MASK    1 << 0x9
#define FLOWER_ATTR_IP_DSCP         1 << 0xa
#define FLOWER_ATTR_IP_DSCP_MASK    1 << 0xb
#define FLOWER_ATTR_PROTO           1 << 0xc
/** @endcond */

#define FLOWER_DSCP_MAX             0xe0
#define FLOWER_DSCP_MASK_MAX        0xe0
#define FLOWER_VID_MAX              4095
#define FLOWER_VLAN_PRIO_MAX        7

static struct nla_policy flower_policy[TCA_FLOWER_MAX + 1] = {
        [TCA_FLOWER_KEY_ETH_TYPE]	= { .type = NLA_U16 },
        [TCA_FLOWER_KEY_ETH_DST]	= { .maxlen = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_DST_MASK]	= { .maxlen = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_SRC]	= { .maxlen = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_SRC_MASK]	= { .maxlen = ETH_ALEN },
        [TCA_FLOWER_KEY_VLAN_ID]	= { .type = NLA_U16 },
	[TCA_FLOWER_KEY_VLAN_PRIO]	= { .type = NLA_U8 },
        [TCA_FLOWER_KEY_IP_TOS]		= { .type = NLA_U8 },
	[TCA_FLOWER_KEY_IP_TOS_MASK]	= { .type = NLA_U8 },
};

static int flower_msg_parser(struct rtnl_tc *tc, void *data)
{
        struct rtnl_flower *f = data;
	struct nlattr *tb[TCA_FLOWER_MAX + 1];
	int err;

	err = tca_parse(tb, TCA_FLOWER_MAX, tc, flower_policy);
	if (err < 0)
		return err;

	if (tb[TCA_FLOWER_FLAGS]) {
		f->cf_flags = nla_get_u32(tb[TCA_FLOWER_FLAGS]);
		f->cf_mask |= FLOWER_ATTR_FLAGS;
	}

	if (tb[TCA_FLOWER_ACT]) {
		err = rtnl_act_parse(&f->cf_act, tb[TCA_FLOWER_ACT]);
		if (err)
			return err;

		f->cf_mask |= FLOWER_ATTR_ACTION;
	}

	if (tb[TCA_FLOWER_POLICE]) {
		f->cf_police = nl_data_alloc_attr(tb[TCA_FLOWER_POLICE]);
		if (!f->cf_police)
			return -NLE_NOMEM;

		f->cf_mask |= FLOWER_ATTR_POLICE;
	}

	if (tb[TCA_FLOWER_KEY_ETH_TYPE]) {
	        f->cf_proto = nla_get_u16(tb[TCA_FLOWER_KEY_ETH_TYPE]);
		f->cf_mask |= FLOWER_ATTR_PROTO;
	}

	if (tb[TCA_FLOWER_KEY_VLAN_ID]) {
	        f->cf_vlan_id = nla_get_u16(tb[TCA_FLOWER_KEY_VLAN_ID]);
		f->cf_mask |= FLOWER_ATTR_VLAN_ID;
	}

	if (tb[TCA_FLOWER_KEY_VLAN_PRIO]) {
	        f->cf_vlan_prio = nla_get_u16(tb[TCA_FLOWER_KEY_VLAN_PRIO]);
		f->cf_mask |= FLOWER_ATTR_VLAN_PRIO;
	}

	if (tb[TCA_FLOWER_KEY_VLAN_ETH_TYPE]) {
	        f->cf_vlan_ethtype = nla_get_u16(tb[TCA_FLOWER_KEY_VLAN_ETH_TYPE]);
		f->cf_mask |= FLOWER_ATTR_VLAN_ETH_TYPE;
	}

	if (tb[TCA_FLOWER_KEY_ETH_DST]) {
	        f->cf_dst_mac = nl_data_alloc_attr(tb[TCA_FLOWER_KEY_ETH_DST]);
		if (!f->cf_dst_mac)
		        return -NLE_NOMEM;

		f->cf_mask |= FLOWER_ATTR_DST_MAC;
	}

	if (tb[TCA_FLOWER_KEY_ETH_DST_MASK]) {
	        f->cf_dst_mac_mask = nl_data_alloc_attr(tb[TCA_FLOWER_KEY_ETH_DST_MASK]);
		if (!f->cf_dst_mac_mask)
		        return -NLE_NOMEM;

		f->cf_mask |= FLOWER_ATTR_DST_MAC_MASK;
	}

	if (tb[TCA_FLOWER_KEY_ETH_SRC]) {
	        f->cf_src_mac = nl_data_alloc_attr(tb[TCA_FLOWER_KEY_ETH_SRC]);
		if (!f->cf_src_mac)
		        return -NLE_NOMEM;

		f->cf_mask |= FLOWER_ATTR_SRC_MAC;
	}

	if (tb[TCA_FLOWER_KEY_ETH_SRC_MASK]) {
	        f->cf_src_mac_mask = nl_data_alloc_attr(tb[TCA_FLOWER_KEY_ETH_SRC_MASK]);
		if (!f->cf_src_mac_mask)
		        return -NLE_NOMEM;

		f->cf_mask |= FLOWER_ATTR_SRC_MAC_MASK;
	}

	if (tb[TCA_FLOWER_KEY_IP_TOS]) {
	        f->cf_ip_dscp = nla_get_u8(tb[TCA_FLOWER_KEY_IP_TOS]);
		f->cf_mask |= FLOWER_ATTR_IP_DSCP;
	}

	if (tb[TCA_FLOWER_KEY_IP_TOS_MASK]) {
	        f->cf_ip_dscp_mask = nla_get_u8(tb[TCA_FLOWER_KEY_IP_TOS_MASK]);
		f->cf_mask |= FLOWER_ATTR_IP_DSCP_MASK;
	}

        return 0;
}

static int flower_msg_fill(struct rtnl_tc *tc, void *data, struct nl_msg *msg)
{
        struct rtnl_flower *f = data;
	int err;

	if (!f)
	        return 0;

	if (f->cf_mask & FLOWER_ATTR_FLAGS)
	        NLA_PUT_U32(msg, TCA_FLOWER_FLAGS, f->cf_mask);

	if (f->cf_mask & FLOWER_ATTR_ACTION) {
	        err = rtnl_act_fill(msg, TCA_FLOWER_ACT, f->cf_act);
		if (err)
		        return err;
	}

	if (f->cf_mask & FLOWER_ATTR_POLICE)
	        NLA_PUT_DATA(msg, TCA_FLOWER_POLICE, f->cf_police);

	if (f->cf_mask & FLOWER_ATTR_PROTO)
	        NLA_PUT_U16(msg, TCA_FLOWER_KEY_ETH_TYPE, f->cf_proto);

	if (f->cf_mask & FLOWER_ATTR_VLAN_ID)
	        NLA_PUT_U16(msg, TCA_FLOWER_KEY_VLAN_ID, f->cf_vlan_id);

	if (f->cf_mask & FLOWER_ATTR_VLAN_PRIO)
	        NLA_PUT_U8(msg, TCA_FLOWER_KEY_VLAN_PRIO, f->cf_vlan_prio);

	if (f->cf_mask & FLOWER_ATTR_VLAN_ETH_TYPE)
	        NLA_PUT_U16(msg, TCA_FLOWER_KEY_VLAN_ETH_TYPE, f->cf_vlan_ethtype);

	if (f->cf_mask & FLOWER_ATTR_DST_MAC)
	        NLA_PUT_DATA(msg, TCA_FLOWER_KEY_ETH_DST, f->cf_dst_mac);

	if (f->cf_mask & FLOWER_ATTR_DST_MAC_MASK)
	        NLA_PUT_DATA(msg, TCA_FLOWER_KEY_ETH_DST_MASK, f->cf_dst_mac_mask);

	if (f->cf_mask & FLOWER_ATTR_SRC_MAC)
	        NLA_PUT_DATA(msg, TCA_FLOWER_KEY_ETH_SRC, f->cf_src_mac);

	if (f->cf_mask & FLOWER_ATTR_SRC_MAC_MASK)
	        NLA_PUT_DATA(msg, TCA_FLOWER_KEY_ETH_SRC_MASK, f->cf_src_mac_mask);

	if (f->cf_mask & FLOWER_ATTR_IP_DSCP)
	        NLA_PUT_U8(msg, TCA_FLOWER_KEY_IP_TOS, f->cf_ip_dscp);

	if (f->cf_mask & FLOWER_ATTR_IP_DSCP_MASK)
	        NLA_PUT_U8(msg, TCA_FLOWER_KEY_IP_TOS_MASK, f->cf_ip_dscp_mask);

        return 0;

 nla_put_failure:
	return -NLE_NOMEM;
}

static void flower_free_data(struct rtnl_tc *tc, void *data)
{
	struct rtnl_flower *f = data;

	if (f->cf_act)
		rtnl_act_put_all(&f->cf_act);

	if (f->cf_police)
		nl_data_free(f->cf_police);

	if (f->cf_dst_mac)
	        nl_data_free(f->cf_dst_mac);

	if (f->cf_dst_mac_mask)
	        nl_data_free(f->cf_dst_mac_mask);

	if (f->cf_src_mac)
	        nl_data_free(f->cf_src_mac);

	if (f->cf_src_mac_mask)
	        nl_data_free(f->cf_src_mac_mask);
}

static int flower_clone(void *_dst, void *_src)
{
        struct rtnl_flower *dst = _dst, *src = _src;

	if (src->cf_dst_mac && !(dst->cf_dst_mac = nl_data_clone(src->cf_dst_mac)))
	        return -NLE_NOMEM;

	if (src->cf_dst_mac_mask &&
	    !(dst->cf_dst_mac_mask = nl_data_clone(src->cf_dst_mac_mask)))
	        return -NLE_NOMEM;

	if (src->cf_src_mac && !(dst->cf_src_mac = nl_data_clone(src->cf_src_mac)))
	        return -NLE_NOMEM;

	if (src->cf_src_mac_mask &&
	    !(dst->cf_src_mac_mask = nl_data_clone(src->cf_src_mac_mask)))
	        return -NLE_NOMEM;

	if (src->cf_act) {
		if (!(dst->cf_act = rtnl_act_alloc()))
			return -NLE_NOMEM;

		memcpy(dst->cf_act, src->cf_act, sizeof(struct rtnl_act));

		/* action nl list next and prev pointers must be updated */
		nl_init_list_head(&dst->cf_act->ce_list);

		if (src->cf_act->c_opts &&
		    !(dst->cf_act->c_opts = nl_data_clone(src->cf_act->c_opts)))
			return -NLE_NOMEM;

		if (src->cf_act->c_xstats &&
		    !(dst->cf_act->c_xstats = nl_data_clone(src->cf_act->c_xstats)))
			return -NLE_NOMEM;

		if (src->cf_act->c_subdata &&
		    !(dst->cf_act->c_subdata = nl_data_clone(src->cf_act->c_subdata)))
			return -NLE_NOMEM;

		if (dst->cf_act->c_link) {
			nl_object_get(OBJ_CAST(dst->cf_act->c_link));
		}

		dst->cf_act->a_next = NULL;   /* Only clone first in chain */
	}

	if (src->cf_police &&
	    !(dst->cf_police = nl_data_clone(src->cf_police)))
		        return -NLE_NOMEM;

        return 0;
}

static void flower_dump_details(struct rtnl_tc *tc, void *data,
				struct nl_dump_params *p)
{
	struct rtnl_flower *f = data;

	if (!f)
	        return;

	if (f->cf_mask & FLOWER_ATTR_FLAGS)
		nl_dump(p, " flags %u", f->cf_flags);

	if (f->cf_mask & FLOWER_ATTR_PROTO)
	        nl_dump(p, " protocol %u", f->cf_proto);

	if (f->cf_mask & FLOWER_ATTR_VLAN_ID)
	        nl_dump(p, " vlan_id %u", f->cf_vlan_id);

	if (f->cf_mask & FLOWER_ATTR_VLAN_PRIO)
	        nl_dump(p, " vlan_prio %u", f->cf_vlan_prio);

	if (f->cf_mask & FLOWER_ATTR_VLAN_ETH_TYPE)
	        nl_dump(p, " vlan_ethtype %u", f->cf_vlan_ethtype);

	if (f->cf_mask & FLOWER_ATTR_DST_MAC)
	        nl_dump(p, " dst_mac %s", f->cf_dst_mac);

	if (f->cf_mask & FLOWER_ATTR_DST_MAC_MASK)
	        nl_dump(p, " dst_mac_mask %s", f->cf_dst_mac_mask);

	if (f->cf_mask & FLOWER_ATTR_SRC_MAC)
	        nl_dump(p, " src_mac %s", f->cf_src_mac);

	if (f->cf_mask & FLOWER_ATTR_SRC_MAC_MASK)
	        nl_dump(p, " src_mac_mask %s", f->cf_src_mac_mask);

	if (f->cf_mask & FLOWER_ATTR_IP_DSCP)
	        nl_dump(p, " dscp %u", f->cf_ip_dscp);

	if (f->cf_mask & FLOWER_ATTR_IP_DSCP_MASK)
	        nl_dump(p, " dscp_mask %u", f->cf_ip_dscp_mask);
}

/**
 * @name Attribute Modification
 * @{
 */

/**
 * Set protocol for flower classifier
 * @arg cls		Flower classifier.
 * @arg proto		protocol (ETH_P_*)
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_set_proto(struct rtnl_cls *cls, uint16_t proto)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
		return -NLE_NOMEM;

	f->cf_proto = htons(proto);
	f->cf_mask |= FLOWER_ATTR_PROTO;

	return 0;
}

/**
 * Get protocol for flower classifier
 * @arg cls		Flower classifier.
 * @arg proto		protocol
 * @return 0 on success or a negative error code.
*/
int rtnl_flower_get_proto(struct rtnl_cls *cls, uint16_t *proto)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data_peek(TC_CAST(cls))))
	        return -NLE_NOMEM;

        if (!(f->cf_mask & FLOWER_ATTR_PROTO))
	        return -NLE_MISSING_ATTR;

	*proto = ntohs(f->cf_proto);

	return 0;
}

/**
 * Set vlan id for flower classifier
 * @arg cls		Flower classifier.
 * @arg vid		vlan id
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_set_vlan_id(struct rtnl_cls *cls, uint16_t vid)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
		return -NLE_NOMEM;

	if (vid > FLOWER_VID_MAX)
	        return -NLE_RANGE;

	f->cf_vlan_id = vid;
	f->cf_mask |= FLOWER_ATTR_VLAN_ID;

	return 0;
}

/**
 * Get vlan id for flower classifier
 * @arg cls		Flower classifier.
 * @arg vid		vlan id
 * @return 0 on success or a negative error code.
*/
int rtnl_flower_get_vlan_id(struct rtnl_cls *cls, uint16_t *vid)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data_peek(TC_CAST(cls))))
	        return -NLE_NOMEM;

        if (!(f->cf_mask & FLOWER_ATTR_VLAN_ID))
	        return -NLE_MISSING_ATTR;

	*vid = f->cf_vlan_id;

	return 0;
}

/**
 * Set vlan priority for flower classifier
 * @arg cls		Flower classifier.
 * @arg prio		vlan priority
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_set_vlan_prio(struct rtnl_cls *cls, uint16_t prio)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
		return -NLE_NOMEM;

	if (prio > FLOWER_VLAN_PRIO_MAX)
	        return -NLE_RANGE;

	f->cf_vlan_prio = prio;
	f->cf_mask |= FLOWER_ATTR_VLAN_PRIO;

	return 0;
}

/**
 * Get vlan prio for flower classifier
 * @arg cls		Flower classifier.
 * @arg prio		vlan priority
 * @return 0 on success or a negative error code.
*/
int rtnl_flower_get_vlan_prio(struct rtnl_cls *cls, uint16_t *prio)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data_peek(TC_CAST(cls))))
	        return -NLE_NOMEM;

        if (!(f->cf_mask & FLOWER_ATTR_VLAN_PRIO))
	        return -NLE_MISSING_ATTR;

	*prio = f->cf_vlan_prio;

	return 0;
}

/**
 * Set vlan ethertype for flower classifier
 * @arg cls		Flower classifier.
 * @arg ethtype		vlan ethertype
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_set_vlan_ethtype(struct rtnl_cls *cls, uint16_t ethtype)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
		return -NLE_NOMEM;

	if (!(f->cf_mask & FLOWER_ATTR_PROTO))
	        return -NLE_MISSING_ATTR;

	if (f->cf_proto != htons(ETH_P_8021Q))
	        return -NLE_INVAL;

	f->cf_vlan_ethtype = htons(ethtype);
	f->cf_mask |= FLOWER_ATTR_VLAN_ETH_TYPE;

	return 0;
}

/**
 * Set destination mac address for flower classifier
 * @arg cls		Flower classifier.
 * @arg mac		destination mac address
 * @arg mask		mask for mac address
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_set_dst_mac(struct rtnl_cls *cls, unsigned char *mac,
			    unsigned char *mask)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
	        return -NLE_NOMEM;

	if (mac) {
	        f->cf_dst_mac = nl_data_alloc(NULL, ETH_ALEN * sizeof(unsigned char));
		if (!f->cf_dst_mac)
		        return -NLE_NOMEM;

		memcpy(f->cf_dst_mac->d_data, mac, ETH_ALEN);
		f->cf_mask |= FLOWER_ATTR_DST_MAC;

		if (mask) {
		        f->cf_dst_mac_mask = nl_data_alloc(NULL, ETH_ALEN * sizeof(unsigned char));
			if (!f->cf_dst_mac_mask)
			        return -NLE_NOMEM;

			memcpy(f->cf_dst_mac_mask->d_data, mask, ETH_ALEN);
			f->cf_mask |= FLOWER_ATTR_DST_MAC_MASK;
		}

		return 0;
	}

	return -NLE_FAILURE;
}

/**
 * Get destination mac address for flower classifier
 * @arg cls		Flower classifier.
 * @arg mac		destination mac address
 * @arg mask		mask for mac address
 * @return 0 on success or a negative error code.
*/
int rtnl_flower_get_dst_mac(struct rtnl_cls *cls, unsigned char *mac,
			    unsigned char *mask)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data_peek(TC_CAST(cls))))
	        return -NLE_NOMEM;

        if (!(f->cf_mask & FLOWER_ATTR_DST_MAC))
	        return -NLE_MISSING_ATTR;

	memcpy(mac, f->cf_dst_mac->d_data, ETH_ALEN);

	if (f->cf_mask & FLOWER_ATTR_DST_MAC_MASK)
	        memcpy(mask, f->cf_dst_mac_mask->d_data, ETH_ALEN);

        return 0;
}

/**
 * Set source mac address for flower classifier
 * @arg cls		Flower classifier.
 * @arg mac		source mac address
 * @arg mask		mask for mac address
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_set_src_mac(struct rtnl_cls *cls, unsigned char *mac,
			    unsigned char *mask)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
	        return -NLE_NOMEM;

	if (mac) {
	        f->cf_src_mac = nl_data_alloc(NULL, ETH_ALEN * sizeof(unsigned char));
		if (!f->cf_src_mac)
		        return -NLE_NOMEM;

		memcpy(f->cf_src_mac->d_data, mac, ETH_ALEN);
		f->cf_mask |= FLOWER_ATTR_SRC_MAC;

		if (mask) {
		        f->cf_src_mac_mask = nl_data_alloc(NULL, ETH_ALEN * sizeof(unsigned char));
			if (!f->cf_src_mac_mask)
			        return -NLE_NOMEM;

			memcpy(f->cf_src_mac_mask->d_data, mask, ETH_ALEN);
			f->cf_mask |= FLOWER_ATTR_SRC_MAC_MASK;
		}

		return 0;
	}

	return -NLE_FAILURE;
}

/**
 * Get source mac address for flower classifier
 * @arg cls		Flower classifier.
 * @arg mac		source mac address
 * @arg mask		mask for mac address
 * @return 0 on success or a negative error code.
*/
int rtnl_flower_get_src_mac(struct rtnl_cls *cls, unsigned char *mac,
			    unsigned char *mask)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data_peek(TC_CAST(cls))))
	        return -NLE_NOMEM;

        if (!(f->cf_mask & FLOWER_ATTR_SRC_MAC))
	        return -NLE_MISSING_ATTR;

	memcpy(mac, f->cf_src_mac->d_data, ETH_ALEN);

	if (f->cf_mask & FLOWER_ATTR_SRC_MAC_MASK)
	        memcpy(mask, f->cf_src_mac_mask->d_data, ETH_ALEN);

        return 0;
}

/**
 * Set dscp value for flower classifier
 * @arg cls		Flower classifier.
 * @arg dscp		dscp value
 * @arg mask		mask for dscp value
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_set_ip_dscp(struct rtnl_cls *cls, uint8_t dscp, uint8_t mask)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
	        return -NLE_NOMEM;

	if (dscp > FLOWER_DSCP_MAX)
	        return -NLE_RANGE;

	if (mask > FLOWER_DSCP_MASK_MAX)
	        return -NLE_RANGE;

	f->cf_ip_dscp = dscp;
	f->cf_mask |= FLOWER_ATTR_IP_DSCP;

	if (mask) {
	        f->cf_ip_dscp_mask = mask;
		f->cf_mask |= FLOWER_ATTR_IP_DSCP_MASK;
	}

	return 0;
}

/**
 * Get dscp value for flower classifier
 * @arg cls		Flower classifier.
 * @arg dscp		dscp value
 * @arg mask		mask for dscp value
 * @return 0 on success or a negative error code.
*/
int rtnl_flower_get_ip_dscp(struct rtnl_cls *cls, uint8_t *dscp, uint8_t *mask)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data_peek(TC_CAST(cls))))
	        return -NLE_NOMEM;

        if (!(f->cf_mask & FLOWER_ATTR_IP_DSCP))
	        return -NLE_MISSING_ATTR;

	*dscp = f->cf_ip_dscp;
	*mask = f->cf_ip_dscp_mask;

	return 0;
}

/**
 * Append action for flower classifier
 * @arg cls		Flower classifier.
 * @arg act		action to append
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_append_action(struct rtnl_cls *cls, struct rtnl_act *act)
{
        struct rtnl_flower *f;

	if (!act)
	        return 0;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
		return -NLE_NOMEM;

	f->cf_mask |= FLOWER_ATTR_ACTION;

	rtnl_act_get(act);
	return rtnl_act_append(&f->cf_act, act);
}

/**
 * Delete action from flower classifier
 * @arg cls		Flower classifier.
 * @arg act		action to delete
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_del_action(struct rtnl_cls *cls, struct rtnl_act *act)
{
        struct rtnl_flower *f;
	int ret;

	if (!act)
		return 0;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
		return -NLE_NOMEM;

	if (!(f->cf_mask & FLOWER_ATTR_ACTION))
		return -NLE_INVAL;

	ret = rtnl_act_remove(&f->cf_act, act);
	if (ret)
		return ret;

	if (!f->cf_act)
		f->cf_mask &= ~FLOWER_ATTR_ACTION;
	rtnl_act_put(act);

	return 0;
}

/**
 * Get action from flower classifier
 * @arg cls		Flower classifier.
 * @return action on success or NULL on error.
 */
struct rtnl_act* rtnl_flower_get_action(struct rtnl_cls *cls)
{
        struct rtnl_flower *f;

	if (!(f = rtnl_tc_data_peek(TC_CAST(cls))))
	        return NULL;

        if (!(f->cf_mask & FLOWER_ATTR_ACTION))
	        return NULL;

	rtnl_act_get(f->cf_act);

	return f->cf_act;
}

/**
 * Set flags for flower classifier
 * @arg cls		Flower classifier.
 * @arg flags		(TCA_CLS_FLAGS_SKIP_HW | TCA_CLS_FLAGS_SKIP_SW)
 * @return 0 on success or a negative error code.
 */
int rtnl_flower_set_flags(struct rtnl_cls *cls, int flags)
{
	struct rtnl_flower *f;

	if (!(f = rtnl_tc_data(TC_CAST(cls))))
		return -NLE_NOMEM;

	f->cf_flags = flags;
	f->cf_mask |= FLOWER_ATTR_FLAGS;

	return 0;
}

/** @} */

static struct rtnl_tc_ops flower_ops = {
	.to_kind		= "flower",
	.to_type		= RTNL_TC_TYPE_CLS,
	.to_size		= sizeof(struct rtnl_flower),
	.to_msg_parser		= flower_msg_parser,
	.to_free_data		= flower_free_data,
	.to_clone		= flower_clone,
	.to_msg_fill		= flower_msg_fill,
	.to_dump = {
	    [NL_DUMP_DETAILS]	= flower_dump_details,
	},
};

static void __init flower_init(void)
{
	rtnl_tc_register(&flower_ops);
}

static void __exit flower_exit(void)
{
	rtnl_tc_unregister(&flower_ops);
}
