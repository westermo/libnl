/*
 * lib/route/mdb.c	Multicast Forwarding DB
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 */

#include <netlink-private/netlink.h>
#include <netlink/netlink.h>
#include <netlink/utils.h>
#include <netlink/hashtable.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/mdb.h>
#include <netlink/route/link.h>
#include <netlink/hashtable.h>

#include <linux/if_bridge.h>

#define MDB_ATTR_IFINDEX      0x01
#define MDB_ATTR_ADDR         0x02
#define MDB_ATTR_RPORT	      0x04

static struct nl_cache_ops rtnl_mdb_ops;
static struct nl_object_ops mdb_obj_ops;


static int build_mdb_msg(struct rtnl_mdb *mdb, struct rtnl_mgrp *grp, int ifindex,
			 int cmd, int flags, struct nl_msg **result)
{
	struct br_port_msg bpm;
	struct br_mdb_entry entry;
	struct nl_msg *msg;

	memset(&bpm, 0, sizeof(bpm));
	memset(&entry, 0, sizeof(entry));

	bpm.family = AF_BRIDGE;
	bpm.ifindex = rtnl_mdb_get_brifindex(mdb);

	entry.ifindex = ifindex;
	entry.addr.proto = htons(grp->proto);
	if (grp->proto == ETH_P_IP)
		entry.addr.u.ip4 = *((int*) nl_addr_get_binary_addr(grp->addr));
	else if (grp->proto == ETH_P_IPV6)
		entry.addr.u.ip6 = *((struct in6_addr*) nl_addr_get_binary_addr(grp->addr));
	else if (grp->proto == ETH_P_ALL)
		memcpy(entry.addr.u.mac, nl_addr_get_binary_addr(grp->addr), ETH_ALEN);
	if (flags & MDB_STATE_MGMT)
		entry.state |= MDB_STATE_MGMT;
        entry.state |= MDB_PERMANENT;
	if (grp->vid)
		entry.vid = grp->vid;

	msg = nlmsg_alloc_simple(cmd, flags);
	if (!msg)
		return -NLE_NOMEM;

	if (nlmsg_append(msg, &bpm, sizeof(bpm), NLMSG_ALIGNTO) < 0)
		goto nla_put_failure;

	nla_put(msg, MDBA_SET_ENTRY, sizeof(entry), &entry);

	*result = msg;
	return 0;

nla_put_failure:
	nlmsg_free(msg);
	return -NLE_MSGSIZE;
}

static int rtnl_mdb_build_request(struct rtnl_mdb *mdb, int cmd, struct rtnl_mgrp *grp,
				  int ifindex, int flags, struct nl_msg **result)
{
	return build_mdb_msg(mdb, grp, ifindex, cmd, flags, result);
}

static void mdb_constructor(struct nl_object *c)
{
	struct rtnl_mdb *mdb = (struct rtnl_mdb *) c;

	nl_init_list_head(&mdb->mrport_list);
	nl_init_list_head(&mdb->mgrp_list);
}

static void mdb_free_mrports(struct rtnl_mdb *mdb)
{
        struct rtnl_mrport *mrp, *next;

        if (mdb->ce_mask & MDB_ATTR_RPORT) {
	        nl_list_for_each_entry_safe(mrp, next, &mdb->mrport_list, mrport_entry) {
		        nl_list_del(&mrp->mrport_entry);
			rtnl_mrport_free(mrp);
			mdb->num_mrport--;
		}
	}
}

static void mdb_free_mgrps(struct rtnl_mdb *mdb)
{
	struct rtnl_mgrp *mgrp, *next;

	nl_list_for_each_entry_safe(mgrp, next, &mdb->mgrp_list, mgrp_entry) {
		rtnl_mdb_del_mgrp(mdb, mgrp);
		rtnl_mgrp_free(mgrp);
	}
}

static void mdb_free_data(struct nl_object *c)
{
	struct rtnl_mdb *mdb = nl_object_priv(c);

	if (!mdb)
		return;

	mdb_free_mrports(mdb);
	mdb_free_mgrps(mdb);
}

static struct rtnl_mrport *mrport_clone(struct rtnl_mrport *src)
{
	struct rtnl_mrport *mr;

	mr = rtnl_mrport_alloc();
	if (!mr)
		return NULL;
	mr->mrport_ifi = src->mrport_ifi;

	return mr;
}

static struct rtnl_mgport *mgport_clone(struct rtnl_mgport *src)
{
	struct rtnl_mgport *mgport;

	mgport = rtnl_mgport_alloc();
	if (!mgport)
		return NULL;
	mgport->mgport_ifi = src->mgport_ifi;

	return mgport;
}

static struct rtnl_mgrp *mgrp_clone(struct rtnl_mgrp *src)
{
	struct rtnl_mgrp *mgrp;
	struct rtnl_mgport *mgport, *new;

	mgrp = rtnl_mgrp_alloc();
	if (!mgrp)
		return NULL;

	mgrp->proto = src->proto;
	mgrp->vid = src->vid;
	mgrp->num_mgport = src->num_mgport;
	mgrp->addr = nl_addr_clone(src->addr);
	if (!mgrp->addr) {
	        free(mgrp);
		return NULL;
	}

	nl_list_for_each_entry(mgport, &src->mgport_list, mgport_entry) {
		new = mgport_clone(mgport);
		if (!new) {
		        rtnl_mgrp_free(mgrp);
			return NULL;
		}
		rtnl_mgrp_add_mgport(mgrp, new);
	}

	return mgrp;
}

static int mdb_clone(struct nl_object *_dst, struct nl_object *_src)
{
	struct rtnl_mdb *dst_mdb = (struct rtnl_mdb *) _dst;
	struct rtnl_mdb *src_mdb = (struct rtnl_mdb *) _src;
	struct rtnl_mrport *dst_rport, *src_rport;
	struct rtnl_mgrp *dst_grp, *src_grp;

	nl_init_list_head(&dst_mdb->mgrp_list);
	nl_init_list_head(&dst_mdb->mrport_list);

	dst_mdb->m_family = src_mdb->m_family;
	dst_mdb->m_brifindex = src_mdb->m_brifindex;
	dst_mdb->ce_mask  = src_mdb->ce_mask;

	nl_list_for_each_entry(src_rport, &src_mdb->mrport_list, mrport_entry) {
		dst_rport = mrport_clone(src_rport);
		if (!dst_rport)
			return -NLE_NOMEM;
		rtnl_mdb_add_mrport(dst_mdb, dst_rport);
	}

	nl_list_for_each_entry(src_grp, &src_mdb->mgrp_list, mgrp_entry) {
		dst_grp = mgrp_clone(src_grp);
		if (!dst_grp)
			return -NLE_NOMEM;
		rtnl_mdb_add_mgrp(dst_mdb, dst_grp);
	}

	return 0;
}

static uint64_t mdb_compare(struct nl_object *_a, struct nl_object *_b,
			uint64_t attrs, int flags)
{
	struct rtnl_mdb *a = (struct rtnl_mdb *) _a;
	struct rtnl_mdb *b = (struct rtnl_mdb *) _b;
	struct rtnl_mrport *mr_a, *mr_b;
	int diff = 0, found;

#define MDB_DIFF(ATTR, EXPR) ATTR_DIFF(attrs, MDB_ATTR_##ATTR, a, b, EXPR)

	diff |= MDB_DIFF(IFINDEX,	a->m_brifindex != b->m_brifindex);
	/* Optimization for the most frequent case - If only ifindex attr needs to be
	 * checked, return immediately, instead of traversing router ports and group ports
	 */
	if (attrs == MDB_ATTR_IFINDEX) {
		return diff;
	}

	diff |= MDB_DIFF(RPORT,	a->num_mrport != b->num_mrport);
	/* search for a dup in each rport of a */
	nl_list_for_each_entry(mr_a, &a->mrport_list, mrport_entry) {
		found = 0;
		nl_list_for_each_entry(mr_b, &b->mrport_list, mrport_entry) {
			if (mr_a->mrport_ifi != mr_b->mrport_ifi) {
				found = 1;
				break;
			}
		}
		if (!found) {
			diff |= MDB_DIFF(RPORT,	1);
			return diff;
		}
	}
	diff |= MDB_DIFF(ADDR,	a->num_mgrp != b->num_mgrp);

	return diff;

#undef MDB_DIFF
}

static int mgport_update(struct rtnl_mgrp *old_mgrp,
			 struct rtnl_mgrp *new_mgrp, int action)
{
        struct rtnl_mgport *new_mgport, *old_mgport;
	int found_old_mgport = 0;

	/* find the first multicast group port */
	new_mgport = rtnl_mgrp_mgport_n(new_mgrp, 0);
	if (!new_mgport)
		return -NLE_OPNOTSUPP;

	/* Find if the group port exists in old mgport_list */
	nl_list_for_each_entry(old_mgport, &old_mgrp->mgport_list, mgport_entry) {
		if (old_mgport->mgport_ifi == new_mgport->mgport_ifi) {
			found_old_mgport = 1;
			break;
		}
	}

	switch(action) {
	case RTM_NEWMDB : {
		struct rtnl_mgport *cl_mgport;
		/* If mgport is already present, ignore new update */
		if (found_old_mgport)
			return NLE_SUCCESS;

		cl_mgport = mgport_clone(new_mgport);
		if (!cl_mgport)
			return -NLE_NOMEM;

		/* Add the grp port to old grp */
		rtnl_mgrp_add_mgport(old_mgrp, cl_mgport);
		NL_DBG(2, "mgrp obj %p updated. Added "
			"grp port %p\n", old_mgrp, cl_mgport);
	}
		break;
	case RTM_DELMDB : {
		/* delete the old_grp port */
		if (found_old_mgport) {
			rtnl_mgrp_del_mgport(old_mgrp, old_mgport);
			NL_DBG(2, "mdb group %p updated. Removed "
				"group port %p\n", old_mgrp, old_mgport);
			rtnl_mgport_free(old_mgport);
		}
	}
		break;
	default:
		NL_DBG(2, "Unknown action %d for %p group update\n", action, new_mgrp);
		return -NLE_OPNOTSUPP;
	}

	return NLE_SUCCESS;
}

static int mgrp_update(struct rtnl_mdb *old_mdb,
		       struct rtnl_mdb *new_mdb, int action)
{
	struct rtnl_mgrp *new_mgrp;
	struct rtnl_mgrp *old_mgrp = NULL;
	int found_old_grp = 0;

	/* Find the first group in the mdb */
	new_mgrp = rtnl_mdb_mgrp_n(new_mdb, 0);
	if (!new_mgrp)
		return -NLE_OPNOTSUPP;

	/* Find the new group in the old multicast group list */
	nl_list_for_each_entry(old_mgrp, &old_mdb->mgrp_list, mgrp_entry) {
		if (!nl_addr_cmp(old_mgrp->addr, new_mgrp->addr) &&
		    (old_mgrp->vid == new_mgrp->vid)) {
		        found_old_grp = 1;
			break;
		}
	}

	/* If group is present, action is newmdb - do router port update
	 * If group is present, action is delmdb, old mgport > 1 - rport update
	 *  else, continue group update
	 */
	if (found_old_grp && ((action == RTM_NEWMDB) ||
			      ((action == RTM_DELMDB) &&
			       (old_mgrp->num_mgport > 1)))) {
		return mgport_update(old_mgrp, new_mgrp, action);
	}

	switch(action) {
	case RTM_NEWMDB : {
		struct rtnl_mgrp *cl_mgrp;

		cl_mgrp = mgrp_clone(new_mgrp);
		if (!cl_mgrp)
			return -NLE_NOMEM;

		rtnl_mdb_add_mgrp(old_mdb, cl_mgrp);
		NL_DBG(2, "mdb obj %p updated. Added grp %p\n", old_mdb, cl_mgrp);
	}
		break;
	case RTM_DELMDB : {
		if (found_old_grp) {
			rtnl_mdb_del_mgrp(old_mdb, old_mgrp);
			NL_DBG(2, "mdb obj %p updated, Removed grp %p\n", old_mdb, old_mgrp);
			rtnl_mgrp_free(old_mgrp);
		}
		/* Check if old mdb has to be deleted, return error, if so
		 */
		if (!old_mdb->num_mgrp && !old_mdb->num_mrport) {
			NL_DBG(2, "deleting mdb %p\n", old_mdb);
			return -NLE_OPNOTSUPP;
		}
	}
		break;
	default:
		NL_DBG(2, "Unknown action %d for %p group update\n", action, new_mdb);
		return -NLE_OPNOTSUPP;
	}

	return NLE_SUCCESS;
}

static int mdb_update(struct nl_object *old_obj, struct nl_object *new_obj)
{
	struct rtnl_mdb *new_mdb = (struct rtnl_mdb *) new_obj;
	struct rtnl_mdb *old_mdb = (struct rtnl_mdb *) old_obj;
	struct rtnl_mrport *new_mrport;
	int action = new_obj->ce_msgtype;
	int found_old_mrport = 0;
	struct rtnl_mrport *old_mrport;
	struct rtnl_mrport *cl_mrport;

	/* If group attr is present, do the group update, else only
	 * router port update
	 */
	if (new_mdb->ce_mask & MDB_ATTR_ADDR)
		return mgrp_update(old_mdb, new_mdb, action);

	/* Get the first router port from the new mdb */
	new_mrport = rtnl_mdb_mrport_n(new_mdb, 0);
	if (!new_mrport)
		return -NLE_OPNOTSUPP;

	/* Find if the mport exists in old mrport_list */
	nl_list_for_each_entry(old_mrport, &old_mdb->mrport_list, mrport_entry) {
		if (old_mrport->mrport_ifi == new_mrport->mrport_ifi) {
			found_old_mrport = 1;
			break;
		}
	}

	switch(action) {
	case RTM_NEWMDB:
		/* if mrport is already present, ignore new update */
		if (found_old_mrport)
			return NLE_SUCCESS;

		cl_mrport = mrport_clone(new_mrport);
		if (!cl_mrport)
			return -NLE_NOMEM;

		rtnl_mdb_add_mrport(old_mdb, cl_mrport);
		NL_DBG(2, "mdb obj %p updated. Added "
			"router port %p\n", old_mdb, cl_mrport);

		break;
	case RTM_DELMDB:
		 /* Find the mrport in old mdb and delete it */
		if (found_old_mrport) {
			rtnl_mdb_del_mrport(old_mdb, old_mrport);
			NL_DBG(2, "mdb obj %p updated. Removed "
			       "router port %p\n", old_mdb, old_mrport);
			rtnl_mrport_free(old_mrport);
		}

		/* Check if old mdb has to be deleted, return error, if so */
		if (!old_mdb->num_mrport && !old_mdb->num_mgrp) {
			NL_DBG(2, "deleting mdb %p\n", old_mdb);
			return -NLE_OPNOTSUPP;
		}

		break;
	default:
		NL_DBG(2, "Unknown action %d for %p mdb update\n", action, new_obj);
		return -NLE_OPNOTSUPP;
	}

	return NLE_SUCCESS;
}

static const struct trans_tbl mdb_attrs[] = {
	__ADD(MDB_ATTR_ADDR, addr),
	__ADD(MDB_ATTR_IFINDEX, ifindex),
	__ADD(MDB_ATTR_RPORT, rport),
};

static char *mdb_attrs2str(int attrs, char *buf, size_t len)
{
	return __flags2str(attrs, buf, len, mdb_attrs,
			   ARRAY_SIZE(mdb_attrs));
}

static int mdb_msg_parser(struct nl_cache_ops *ops, struct sockaddr_nl *who,
			  struct nlmsghdr *n, struct nl_parser_param *pp)
{
	struct rtnl_mdb *mdb;
	struct nlattr *tb[MDBA_MAX + 1];
	struct br_mdb_entry *bm;
	int err;
	struct br_port_msg *br_p;

	mdb = rtnl_mdb_alloc();
	if (!mdb)
		return -NLE_NOMEM;

	mdb->ce_msgtype = n->nlmsg_type;
	br_p = nlmsg_data(n);

	err = nlmsg_parse(n, sizeof(*br_p), tb, MDBA_MAX, NULL);
	if (err < 0)
		goto put_mdb;
	mdb->m_brifindex = br_p->ifindex;
	mdb->m_family = br_p->family;

	/* Sanity check - one of thse attributes should be present,
	 * otherwise we bail out nicely.
	 */
	if (!tb[MDBA_MDB] && !tb[MDBA_ROUTER]) {
		NL_DBG(2, "mdb, rport attr not present 0x%x\n", mdb->m_brifindex);
		goto put_mdb;
	}

	if (tb[MDBA_MDB]) {
		struct nlattr *me;
		char addr[INET6_ADDRSTRLEN+5];
		int rem_mdb_len;

		nla_for_each_nested(me, tb[MDBA_MDB], rem_mdb_len) {
			struct nlattr *mi;
			struct rtnl_mgrp *mgrp;
			struct rtnl_mgport *mgprt;
			struct rtnl_mgrp *old_mgrp = NULL;
			int rem_mdb_entry_len;
			int found_old_grp = 0;

			nla_for_each_nested(mi, me, rem_mdb_entry_len) {
				int family;

				bm = nla_data(mi);
				mgrp = rtnl_mgrp_alloc();
				if (!mgrp) {
				        err = -NLE_NOMEM;
					goto put_mdb;
				}

				/*
				 * Save the family info in nl_addr
				 * format and figure out what kind of
				 * multicast entry we're looking at here
				 * ...
				 */
				mgrp->proto = ntohs(bm->addr.proto);
				switch (mgrp->proto) {
				case ETH_P_IP:
					family = AF_INET;
					mgrp->addr = nl_addr_build(family,
								   (unsigned char *)&bm->addr.u,
								   sizeof(__be32));
					break;

				case ETH_P_IPV6:
					family = AF_INET6;
					mgrp->addr = nl_addr_build(family,
								   (unsigned char *)&bm->addr.u,
								   sizeof(struct in6_addr));
					break;

				case ETH_P_ALL:
					family = AF_LLC;
					mgrp->addr = nl_addr_build(family,
								   (unsigned char *)&bm->addr.u,
								   ETH_ALEN);
					break;

				default:
					continue;
				}

				mgrp->vid = bm->vid;
				mgprt = rtnl_mgport_alloc();
				if (!mgprt) {
					rtnl_mgrp_free(mgrp);
					err = -NLE_NOMEM;
					goto put_mdb;
				}
				mgprt->mgport_ifi = bm->ifindex;
				/* Find the new group in the old multicast group list
				 * Add multicast group if not present
				 */
				nl_list_for_each_entry(old_mgrp, &mdb->mgrp_list, mgrp_entry) {
					if (!nl_addr_cmp(old_mgrp->addr, mgrp->addr) &&
					    (old_mgrp->vid == mgrp->vid)) {
						found_old_grp = 1;
						rtnl_mgrp_free(mgrp);
						mgrp = old_mgrp;
						break;
					}
				}
				if (!found_old_grp)
					rtnl_mdb_add_mgrp(mdb, mgrp);
				/* Add multicast group port */
				rtnl_mgrp_add_mgport(mgrp, mgprt);
				NL_DBG(2, "%d: %s proto: %d\n", bm->ifindex,
						nl_addr2str(mgrp->addr, addr,
							sizeof(addr)), family);
				mdb->ce_mask |= MDB_ATTR_ADDR;
			}
		}
	}

	/* Parse router port netlink attribute
	 */
	if (tb[MDBA_ROUTER]) {
		int rem;
		uint32_t ifindex;
		struct rtnl_mrport *mr;
		struct nlattr *attr;

		nla_for_each_nested(attr, tb[MDBA_ROUTER], rem) {
			if (nla_type(attr) != MDBA_ROUTER_PORT)
				continue;

			ifindex = nla_get_u32(attr);
			mr = rtnl_mrport_alloc();
			if (!mr) {
				err = -NLE_NOMEM;
				goto put_mdb;
			}

			mr->mrport_ifi = ifindex;
			NL_DBG(2, "%s rp %d\n", __FUNCTION__, mr->mrport_ifi);
			rtnl_mdb_add_mrport(mdb, mr);
			mdb->ce_mask |= MDB_ATTR_RPORT;
		}
	}

	/* Sanity check */
	if (mdb->ce_mask == 0) {
		NL_DBG(2, "rport, addr attr not parsed 0x%x\n", mdb->m_brifindex);
		goto put_mdb;
	}
	mdb->ce_mask |= MDB_ATTR_IFINDEX;

	err = pp->pp_cb((struct nl_object *) mdb, pp);

 put_mdb:
	rtnl_mdb_put(mdb);
	return err;
}

/* Request kernel for current multicast database information
 */
static int mdb_request_update(struct nl_cache *c, struct nl_sock *h)
{
	return nl_rtgen_request(h, RTM_GETMDB, PF_BRIDGE, NLM_F_DUMP);
}

/* Dump multicast database info
 */
static void mdb_dump_line(struct nl_object *a, struct nl_dump_params *p)
{
	struct rtnl_mdb *m = (struct rtnl_mdb *) a;
	struct nl_cache *link_cache;
	char state[128];
	char addr[INET6_ADDRSTRLEN+5];

	link_cache = nl_cache_mngt_require("route/link");
	if (link_cache)
		nl_dump(p, "bridge %s \n",
			rtnl_link_i2name(link_cache, m->m_brifindex,
					 state, sizeof(state)));
	else
		nl_dump(p, "bridge %d \n", m->m_brifindex);

	if (m->ce_mask & MDB_ATTR_ADDR) {
		struct rtnl_mgrp *mgrp;
		struct rtnl_mgport *prt;

		if (m->num_mgrp)
			nl_dump(p, "Num of Groups %d \n", m->num_mgrp);
		nl_list_for_each_entry(mgrp, &m->mgrp_list, mgrp_entry) {
			nl_dump(p, "grp %s\n", nl_addr2str(mgrp->addr, addr, sizeof(addr)));
			nl_list_for_each_entry(prt, &mgrp->mgport_list, mgport_entry) {
				if (link_cache)
					nl_dump(p, "dev %s \n",
						rtnl_link_i2name(link_cache,
								 prt->mgport_ifi,
								 state,
								 sizeof(state)));
				else
				        nl_dump(p, "dev %d \n", prt->mgport_ifi);
			}
		}
	}

	if (m->ce_mask & MDB_ATTR_RPORT) {
		struct rtnl_mrport *mr;

		if (m->num_mrport)
			nl_dump(p, "Num of Router ports %d \n", m->num_mrport);
		nl_list_for_each_entry(mr, &m->mrport_list, mrport_entry) {
			if (link_cache)
				nl_dump(p, "rport %s \n",
					rtnl_link_i2name(link_cache, mr->mrport_ifi,
							 state, sizeof(state)));
			else
				nl_dump(p, "rport %d \n", mr->mrport_ifi);
		}
	}
	nl_dump(p, "\n");
}

/* Dump details for multicast database
 */
static void mdb_dump_details(struct nl_object *a, struct nl_dump_params *p)
{
	mdb_dump_line(a, p);
}

/* Dump stats for multicast database
 */
static void mdb_dump_stats(struct nl_object *a, struct nl_dump_params *p)
{
	mdb_dump_details(a, p);
}

/* ---------------------------------------------------------------------------------------- */

/**
 * @name Cache Management
 * @{
 */

/**
 * Allocate MDB cache
 * @arg sock		initialized netlink socket
 * @arg result		residence of allocated mdb cache
 *
 * Cache must be released with nl_cache_free() after usage.
 *
 * @return 0 on success or negative error code.
 */
int rtnl_mdb_alloc_cache(struct nl_sock *sock, struct nl_cache **result)
{
	return nl_cache_alloc_and_fill(&rtnl_mdb_ops, sock, result);
}

/**
 * Search for mdb in cache based on ifindex
 * @arg cache		mdb cache
 * @arg ifi		Bridge ifindex
 *
 * Searches mdb cache previously allocated with rtnl_mdb_alloc_cache()
 * for an mdb entry with a matching ifindex.
 *
 * The reference counter is incremented before returning the mdb object, therefore
 * the reference must be given back with rtnl_mdb_put() after usage.
 *
 * @return mdb object or NULL if no match was found.
 */
struct rtnl_mdb *rtnl_mdb_get_by_ifi(struct nl_cache *cache, int ifi)
{
        struct rtnl_mdb *mdb;

	if (cache->c_ops != &rtnl_mdb_ops)
	        return NULL;

	nl_list_for_each_entry(mdb, &cache->c_items, ce_list) {
	        if (rtnl_mdb_get_brifindex(mdb) == ifi) {
	                nl_object_get((struct nl_object *) mdb);
			return mdb;
		}
		else
		        continue;
	}

	return NULL;
}

/** @} */

/**
 * @name Multicast Group Port
 * @{
 */

/**
 * Allocate multicast group port
 *
 * @return Multicast group port on success or NULL on error.
 */
struct rtnl_mgport *rtnl_mgport_alloc(void)
{
	struct rtnl_mgport *mgprt;

	mgprt = calloc(1, sizeof(*mgprt));
	if (!mgprt)
		return NULL;

	nl_init_list_head(&mgprt->mgport_entry);

	return mgprt;
}

/**
 * Free multicast group port
 * @arg mgport		multicast group port
 * @return Multicast group port on success or NULL on error.
 */
void rtnl_mgport_free(struct rtnl_mgport *mgport)
{
	free(mgport);
}

/**
 * Get multicast group port's ifindex
 * @arg mgp		multicast group port
 * @return Multicast group port's ifindex.
 */
unsigned int rtnl_mgport_get_ifi(struct rtnl_mgport *mgp)
{
	return mgp->mgport_ifi;
}

/**
 * Set multicast group port's ifindex
 * @arg mgp		multicast group port
 * @arg ifindex		ifindex
 */
void rtnl_mgport_set_ifi(struct rtnl_mgport *mgp, int ifindex)
{
	mgp->mgport_ifi = ifindex;
}

/** @} */

/**
 * @name Multicast Group
 * @{
 */

/**
 * Allocate multicast group
 *
 * @return Multicast group on success or NULL on error.
 */
struct rtnl_mgrp *rtnl_mgrp_alloc(void)
{
	struct rtnl_mgrp *mg;

	mg = calloc(1, sizeof(*mg));
	if (!mg)
		return NULL;

	nl_init_list_head(&mg->mgrp_entry);
	nl_init_list_head(&mg->mgport_list);

	return mg;
}

/**
 * Free multicast group
 * @arg mgrp		multicast group
 */
void rtnl_mgrp_free(struct rtnl_mgrp *mgrp)
{
	nl_addr_put(mgrp->addr);
	free(mgrp);
}

/**
 * Add port to multicast group
 * @arg mgrp		Multicast Group
 * @arg mgport		Port
 *
 */
void rtnl_mgrp_add_mgport(struct rtnl_mgrp *mgrp, struct rtnl_mgport *mgport)
{
	nl_list_add_tail(&mgport->mgport_entry, &mgrp->mgport_list);
	mgrp->num_mgport++;
}

/**
 * Delete port from multicast group
 * @arg mgrp		Multicast Group
 * @arg mgport		Port
 *
 */
void rtnl_mgrp_del_mgport(struct rtnl_mgrp *mgrp, struct rtnl_mgport *mgport)
{
	nl_list_del(&mgport->mgport_entry);
	mgrp->num_mgport--;
}

/**
 * Free all ports in multicast group
 * @arg mgrp		Multicast Group
 */
void rtnl_mgrp_free_mgports(struct rtnl_mgrp *mgrp)
{
	struct rtnl_mgport *mgport, *next;

	nl_list_for_each_entry_safe(mgport, next, &mgrp->mgport_list, mgport_entry) {
	        rtnl_mgrp_del_mgport(mgrp, mgport);
		rtnl_mgport_free(mgport);
	}
}

/**
 * Get number of multicast group ports
 * @arg mgrp		Multicast Group.
 *
 * @return Number of multicast group ports.
 */
unsigned int rtnl_mgrp_get_num_mgport(struct rtnl_mgrp *mgrp)
{
    return mgrp->num_mgport;
}

/**
 * Traverse multicast group port list
 * @arg grp		Multicast Group.
 * @arg cb		Callback to be executed
 * @arg arg		Argument for callback
 *
 */
void rtnl_mgrp_foreach_mgport(struct rtnl_mgrp *grp,
			      void (*cb)(struct rtnl_mgport *, void *),
			      void *arg)
{
	struct rtnl_mgport *prt;

	nl_list_for_each_entry(prt, &grp->mgport_list, mgport_entry)
		cb(prt, arg);
}

/**
 * Get nth element in multicast group port list
 * @arg grp		Multicast Group
 * @arg n		Element id
 *
 * @return multicast group port or NULL if such element does not exist.
 */
struct rtnl_mgport *rtnl_mgrp_mgport_n(struct rtnl_mgrp *grp, int n)
{
	struct rtnl_mgport *prt;
	int i;

	if (grp->num_mgport > n) {
		i = 0;
		nl_list_for_each_entry(prt, &grp->mgport_list, mgport_entry) {
			if (i == n) return prt;
			i++;
		}
	}
        return NULL;
}

/**
 * Get multicast group's IP address
 * @arg grp		Multicast Group
 *
 * @return IP address
 */
struct nl_addr *rtnl_mgrp_get_ipaddr(struct rtnl_mgrp *mgrp)
{
	return mgrp->addr;
}

/**
 * Set IP address for multicast group
 * @arg grp		Multicast Group
 * @arg ip		IP address
 *
 */
void rtnl_mgrp_set_ipaddr(struct rtnl_mgrp *mgrp, int ip)
{
	mgrp->addr = nl_addr_build(AF_INET,
				   (unsigned char *)&ip,
				   sizeof(__be32));

	mgrp->proto = ETH_P_IP;
}

/**
 * Get multicast group's MAC address
 * @arg grp		Multicast Group
 *
 * @return MAC address
 */
struct nl_addr *rtnl_mgrp_get_macaddr(struct rtnl_mgrp *mgrp)
{
	return mgrp->addr;
}

/**
 * Set MAC address for multicast group
 * @arg grp		Multicast Group
 * @arg mac		mac address
 *
 */
void rtnl_mgrp_set_macaddr(struct rtnl_mgrp *mgrp, uint8_t *mac)
{
	mgrp->addr = nl_addr_build(AF_LLC, mac, ETH_ALEN);
	mgrp->proto = ETH_P_ALL;
}

/**
 * Set VLAN for multicast group entry
 * @arg grp		Multicast Group
 * @arg vid	        vlan id
 *
 */
void rtnl_mgrp_set_vid(struct rtnl_mgrp *mgrp, int vid)
{
	mgrp->vid = vid;
}

/**
 * Get VLAN for multicast group entry
 * @arg grp		Multicast Group
 *
 * @return vid of multicast group
 */
int rtnl_mgrp_get_vid(struct rtnl_mgrp *mgrp)
{
	return mgrp->vid;
}

/** @} */

/**
 * @name Multicast Router Port
 * @{
 */

/**
 * Allocate multicast router port
 *
 * @return multicast router port or NULL on error
 */
struct rtnl_mrport *rtnl_mrport_alloc(void)
{
	struct rtnl_mrport *mr;

	mr = calloc(1, sizeof(*mr));
	if (!mr)
		return NULL;

	nl_init_list_head(&mr->mrport_entry);

	return mr;
}

/**
 * Free multicast router port
 * @arg mr		Multicast Router port
 *
 */
void rtnl_mrport_free(struct rtnl_mrport *mr)
{
	free(mr);
}

/**
 * Get multicast router port's ifindex
 * @arg mrport		Multicast Router port
 *
 * @return ifindex of multicast router port
 */
unsigned int rtnl_mrport_get_grpifindex(struct rtnl_mrport *mrprt)
{
	return mrprt->mrport_ifi;
}

/** @} */

/**
 * @name Multicast Database
 * @{
 */

/**
 * Allocate multicast database
 *
 * @return Multicast database on success or NULL on error.
 */
struct rtnl_mdb *rtnl_mdb_alloc(void)
{
	return (struct rtnl_mdb *) nl_object_alloc(&mdb_obj_ops);
}

/**
 * Decrease reference counter and free MDB
 * @arg mdb		Multicase database
 *
 */
void rtnl_mdb_put(struct rtnl_mdb *mdb)
{
	nl_object_put((struct nl_object *) mdb);
}

/**
 * Traverse multicast router port list
 * @arg m		MDB object
 * @arg cb		Callback
 * @arg arg		Argument for callback
 *
 */
void rtnl_mdb_foreach_mrport(struct rtnl_mdb *m,
			     void (*cb)(struct rtnl_mrport *, void *), void *arg)
{
	struct rtnl_mrport *mr;

	if (m->ce_mask & MDB_ATTR_RPORT) {
		nl_list_for_each_entry(mr, &m->mrport_list, mrport_entry) {
				cb(mr, arg);
		}
	}
}

/**
 * Get nth element of multicast router port list
 * @arg m		MDB object
 * @arg n		Element id
 *
 * @return Multicast router port on success or NULL on error.
 */
struct rtnl_mrport *rtnl_mdb_mrport_n(struct rtnl_mdb *m, int n)
{
	struct rtnl_mrport *mr;
	int i;

	if ((m->ce_mask & MDB_ATTR_RPORT) && (m->num_mrport > n)) {
		i = 0;
		nl_list_for_each_entry(mr, &m->mrport_list, mrport_entry) {
			if (i == n) return mr;
			i++;
		}
	}

	return NULL;
}

/**
 * Traverse MDB's multicast group list
 * @arg m		MDB object
 * @arg cb		Callback
 * @arg arg		Argument for callback
 *
 */
void rtnl_mdb_foreach_mgrp(struct rtnl_mdb *m,
			   void (*cb)(struct rtnl_mgrp *, void *),
			   void *arg)
{
	struct rtnl_mgrp *mgrp;

	if (m->ce_mask & MDB_ATTR_ADDR) {
		nl_list_for_each_entry(mgrp, &m->mgrp_list, mgrp_entry)
			cb(mgrp, arg);
	}
}

/**
 * Get nth element of multicast group list
 * @arg m		MDB object
 * @arg n		Element id
 *
 * @return Multicast group on success or NULL on error.
 */
struct rtnl_mgrp *rtnl_mdb_mgrp_n(struct rtnl_mdb *m, int n)
{
	struct rtnl_mgrp *mgrp;
	int i;

	if ((m->ce_mask & MDB_ATTR_ADDR) && (m->num_mgrp > n)) {
		i = 0;
		nl_list_for_each_entry(mgrp, &m->mgrp_list, mgrp_entry) {
			if (i == n) return mgrp;
			i++;
		}
	}

	return NULL;
}

/**
 * Get mdb family
 * @arg mdb		Multicast database
 *
 * @return family - currently AF_BRIDGE
 */
unsigned int rtnl_mdb_get_family(struct rtnl_mdb *mdb)
{
	return mdb->m_family;
}

/**
 * Get Bridge ifindex of MDB
 * @arg mdb		Multicast database
 *
 * @return ifindex of bridge
 */
unsigned int rtnl_mdb_get_brifindex(struct rtnl_mdb *mdb)
{
	return mdb->m_brifindex;
}

/**
 * Set Bridge ifindex of MDB
 * @arg mdb		Multicast database
 * @arg ifindex		Interface index
 *
 */
void rtnl_mdb_set_brifindex(struct rtnl_mdb *mdb, int ifindex)
{
	mdb->m_brifindex = ifindex;
}


/* Get number of multicast router ports
 */
unsigned int rtnl_mdb_get_num_mrport(struct rtnl_mdb *mdb)
{
	return mdb->num_mrport;
}

/**
 * Get number of multicast groups
 * @arg mdb		Multicast database
 *
 * @return number of multicast groups
 */
unsigned int rtnl_mdb_get_num_mgrp(struct rtnl_mdb *mdb)
{
  return mdb->num_mgrp;
}

/**
 * Add multicast router port
 * @arg mdb		Multicast database
 * @arg mr		Multicast router port
 *
 */
void rtnl_mdb_add_mrport(struct rtnl_mdb *mdb, struct rtnl_mrport *mr)
{
	nl_list_add_tail(&mr->mrport_entry, &mdb->mrport_list);
	mdb->num_mrport++;
	mdb->ce_mask |= MDB_ATTR_RPORT;
}

/**
 * Delete multicast router port
 * @arg mdb		Multicast database
 * @arg mr		Multicast router port
 *
 */
void rtnl_mdb_del_mrport(struct rtnl_mdb *mdb, struct rtnl_mrport *mr)
{
        if (mdb->ce_mask & MDB_ATTR_RPORT) {
	        nl_list_del(&mr->mrport_entry);
		mdb->num_mrport--;
	}
}

/**
 * Add multicast group to list
 * @arg mdb		Multicast database
 * @arg mg		Multicast group
 *
 */
void rtnl_mdb_add_mgrp(struct rtnl_mdb *mdb, struct rtnl_mgrp *mg)
{
	nl_list_add_tail(&mg->mgrp_entry, &mdb->mgrp_list);
	mdb->num_mgrp++;
	mdb->ce_mask |= MDB_ATTR_ADDR;
}

/**
 * Delete multicast group from list
 * @arg mdb		Multicast database
 * @arg mg		Multicast group
 *
 */
void rtnl_mdb_del_mgrp(struct rtnl_mdb *mdb, struct rtnl_mgrp *mg)
{
        rtnl_mgrp_free_mgports(mg);
	nl_list_del(&mg->mgrp_entry);
	mdb->num_mgrp--;
	mdb->ce_mask |= MDB_ATTR_ADDR;
}

static int rtnl_mdb_compose(struct nl_sock *sk, struct rtnl_mdb *mdb, struct rtnl_mgrp *grp,
			    int rtm, int flags)
{
        struct nl_msg *msg;
	struct rtnl_mgport *port;
	int err = 0, ret = 0;

	nl_list_for_each_entry(port, &grp->mgport_list, mgport_entry) {
	        if (rtnl_mdb_build_request(mdb, rtm, grp,
					   rtnl_mgport_get_ifi(port),
					   flags, &msg) < 0)
			continue;

		err = nl_send_sync(sk, msg);
		if (err < 0) {
		        ret = err;
			continue;
		}
	}

	return ret;
}

/**
 * Remove multicast group from MDB
 * @arg sk		Netlink socket
 * @arg mdb		Multicast database
 * @arg grp		Multicast group
 * @arg flags		Additional flags
 *
 * @return 0 on success or negative error code.
 */
int rtnl_mdb_del_group(struct nl_sock *sk, struct rtnl_mdb *mdb, struct rtnl_mgrp *grp, int flags)
{
        return rtnl_mdb_compose(sk, mdb, grp, RTM_DELMDB, flags);
}

/**
 * Add multicast group to MDB
 * @arg sk		Netlink socket
 * @arg mdb		Multicast database
 * @arg grp		Multicast group
 * @arg flags		Additional flags
 *
 * @return 0 on success or negative error code.
 */
int rtnl_mdb_add_group(struct nl_sock *sk, struct rtnl_mdb *mdb, struct rtnl_mgrp *grp, int flags)
{
        return rtnl_mdb_compose(sk, mdb, grp, RTM_NEWMDB, flags);
}

/** @} */

static struct nl_object_ops mdb_obj_ops = {
	.oo_name		= "route/mdb",
	.oo_size		= sizeof(struct rtnl_mdb),
	.oo_constructor	        = mdb_constructor,
	.oo_free_data	        = mdb_free_data,
	.oo_clone		= mdb_clone,
	.oo_dump = {
                [NL_DUMP_LINE]	        = mdb_dump_line,
		[NL_DUMP_DETAILS]	= mdb_dump_details,
		[NL_DUMP_STATS]	        = mdb_dump_stats,
	},
	.oo_compare		= mdb_compare,
	.oo_update		= mdb_update,
	.oo_attrs2str	        = mdb_attrs2str,
	.oo_id_attrs	        = MDB_ATTR_IFINDEX
};

static struct nl_af_group mdb_groups[] = {
	{ PF_BRIDGE, RTNLGRP_MDB },
	{ END_OF_GROUP_LIST },
};

static struct nl_cache_ops rtnl_mdb_ops = {
	.co_name		= "route/mdb",
	.co_hdrsize		= sizeof(struct br_port_msg),
	.co_msgtypes	        = {
					{ RTM_NEWMDB, NL_ACT_NEW, "new" },
					{ RTM_DELMDB, NL_ACT_DEL, "del" },
					{ RTM_GETMDB, NL_ACT_GET, "get" },
					END_OF_MSGTYPES_LIST,
                                   },
	.co_protocol	        = NETLINK_ROUTE,
	.co_groups		= mdb_groups,
	.co_request_update	= mdb_request_update,
	.co_msg_parser		= mdb_msg_parser,
	.co_obj_ops		= &mdb_obj_ops,
	.co_hash_size           = 4096,
};

static void __init mdb_init(void)
{
	nl_cache_mngt_register(&rtnl_mdb_ops);
}

static void __exit mdb_exit(void)
{
	nl_cache_mngt_unregister(&rtnl_mdb_ops);
}
