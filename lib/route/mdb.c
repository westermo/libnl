/*
 * lib/route/mdb.c	MDB
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
#define MDB_ATTR_RPORT		  0x04

static struct nl_cache_ops rtnl_mdb_ops;
static struct nl_object_ops mdb_obj_ops;

/* Initalize mdb info
 */
static void mdb_constructor(struct nl_object *c)
{
	struct rtnl_mdb *mdb = (struct rtnl_mdb *) c;

	nl_init_list_head(&mdb->m_rport);
	nl_init_list_head(&mdb->m_grps);
}

/* Free multicast database info
 */
static void mdb_free_data(struct nl_object *c)
{
	struct rtnl_mdb *mdb = nl_object_priv(c);
	struct rtnl_mrport *mr, *tmp_mr;
	struct rtnl_mgrp *mgp, *tmp_mgp;
	struct rtnl_mgport *mgport, *tmp_mgport;

	if (!mdb)
		return;

	nl_list_for_each_entry_safe(mr, tmp_mr, &mdb->m_rport, rtmr_list) {
		rtnl_mdb_remove_mrport(mdb, mr);
		rtnl_mdb_mr_free(mr);
	}

	nl_list_for_each_entry_safe(mgp, tmp_mgp, &mdb->m_grps, grp_list) {
		nl_list_for_each_entry_safe(mgport, tmp_mgport,
							&mgp->m_gport, gport_list) {
			rtnl_mdb_remove_mgport(mgp, mgport);
			rtnl_mdb_mgport_free(mgport);
		}
		rtnl_mdb_remove_mgrp(mdb, mgp);
		rtnl_mdb_mgrp_free(mgp);
	}
}

/* Clone multicast database info
 */
static int mdb_clone(struct nl_object *_dst, struct nl_object *_src)
{
	struct rtnl_mdb *new_mdb = (struct rtnl_mdb *) _dst;
	struct rtnl_mdb *old_mdb = (struct rtnl_mdb *) _src;
	struct rtnl_mrport *rport, *new_rport;
	struct rtnl_mgrp *grp, *new_grp;
	struct rtnl_mgport *gport, *new_gport;

	new_mdb->m_family = old_mdb->m_family;
	new_mdb->m_brifindex = old_mdb->m_brifindex;
	new_mdb->ce_mask  = old_mdb->ce_mask;

	/* Clone router port list
	 */
	nl_init_list_head(&new_mdb->m_rport);
	nl_list_for_each_entry(rport, &old_mdb->m_rport, rtmr_list) {
		new_rport = rtnl_mdb_mrport_clone(rport);
		if (!new_rport)
			return -NLE_NOMEM;
		rtnl_mdb_add_mrport(new_mdb, new_rport);
	}

	/* Clone groups list
	 */
	nl_init_list_head(&new_mdb->m_grps);
	nl_list_for_each_entry(grp, &old_mdb->m_grps, grp_list) {
		new_grp = rtnl_mdb_mgrp_clone(grp);
		if (!new_grp)
			return -NLE_NOMEM;
		rtnl_mdb_add_mgrp(new_mdb, new_grp);
		/* Clone group port list
		 */
		nl_init_list_head(&new_grp->m_gport);
		nl_list_for_each_entry(gport, &grp->m_gport, gport_list) {
			new_gport = rtnl_mdb_mgport_clone(gport);
			if (!new_gport)
				return -NLE_NOMEM;
			rtnl_mdb_add_mgport(grp, new_gport);

		}
	}

	return 0;
}

/* Compare multicast database objects for the attributes requested
 * and return 1 if the attributes differ, else return 0
 */
static int mdb_compare(struct nl_object *_a, struct nl_object *_b,
			uint32_t attrs, int flags)
{
	struct rtnl_mdb *a = (struct rtnl_mdb *) _a;
	struct rtnl_mdb *b = (struct rtnl_mdb *) _b;
	struct rtnl_mrport *mr_a, *mr_b;
	int diff = 0, found;

#define MDB_DIFF(ATTR, EXPR) ATTR_DIFF(attrs, MDB_ATTR_##ATTR, a, b, EXPR)

	diff |= MDB_DIFF(IFINDEX,	a->m_brifindex != b->m_brifindex);
	/* Optimization for frequest case - If only ifindex attr needs to be
	 * checked, return now, instead of traversing router port, group port list
	 */
	if (attrs == MDB_ATTR_IFINDEX) {
		return diff;
	}

	diff |= MDB_DIFF(RPORT,	a->m_nr_rport != b->m_nr_rport);
	/* search for a dup in each rport of a */
	nl_list_for_each_entry(mr_a, &a->m_rport, rtmr_list) {
		found = 0;
		nl_list_for_each_entry(mr_b, &b->m_rport, rtmr_list) {
			if (mr_a->m_rpifindex != mr_b->m_rpifindex) {
				found = 1;
				break;
			}
		}
		if (!found) {
			diff |= MDB_DIFF(RPORT,	1);
			return diff;
		}
	}
	diff |= MDB_DIFF(ADDR,	a->m_nr_grp != b->m_nr_grp);

	return diff;

#undef MDB_DIFF
}

/* Clone multicast router port
 */
struct rtnl_mrport *rtnl_mdb_mrport_clone(struct rtnl_mrport *src)
{
	struct rtnl_mrport *mr;

	mr = rtnl_mdb_mr_alloc();
	if (!mr)
		return NULL;
	mr->m_rpifindex = src->m_rpifindex;

	return mr;
}

/* Add multicast group port
 */
struct rtnl_mgport *rtnl_mdb_mgport_clone(struct rtnl_mgport *src)
{
	struct rtnl_mgport *mgport;

	mgport = rtnl_mdb_mgport_alloc();
	if (!mgport)
		return NULL;
	mgport->m_grpifindex = src->m_grpifindex;

	return mgport;
}

/* Clone multicast group port
 */
struct rtnl_mgrp *rtnl_mdb_mgrp_clone(struct rtnl_mgrp *src)
{
	struct rtnl_mgrp *mgrp;
	struct rtnl_mgport *mgport, *new;

	mgrp = rtnl_mdb_mgrp_alloc();
	if (!mgrp)
		return NULL;

	mgrp->addr = nl_addr_clone(src->addr);
	if (!mgrp->addr) {
		return NULL;
	}

	nl_list_for_each_entry(mgport, &src->m_gport, gport_list) {
		new = rtnl_mdb_mgport_clone(mgport);
		if (!new)
			return NULL;
		rtnl_mdb_add_mgport(mgrp, new);
	}

	return mgrp;
}

/* Update(Add/Delete) new multicast group port information received
 * into the old group port object present
 */
static int mdb_grp_port_update(struct rtnl_mgrp *old_mgp,
					struct rtnl_mgrp *new_mgp, int action)
{
	struct rtnl_mgport *new_mgport;
	struct rtnl_mgport *old_mgport;
	int found_old_grp_port = 0;

	// find the first multicast group port
	new_mgport = rtnl_mdb_mgport_n(new_mgp, 0);
	if (!new_mgport)
		return -NLE_OPNOTSUPP;

	/* Find if the group port exists in old mgport_list
	 */
	nl_list_for_each_entry(old_mgport, &old_mgp->m_gport, gport_list) {
		if (old_mgport->m_grpifindex == new_mgport->m_grpifindex) {
			found_old_grp_port = 1;
			break;
		}
	}

	switch(action) {
	case RTM_NEWMDB : {
		struct rtnl_mgport *cl_mgport;
		// If grp_port is already present, ignore new update
		if (found_old_grp_port)
			return NLE_SUCCESS;

		cl_mgport = rtnl_mdb_mgport_clone(new_mgport);
		if (!cl_mgport)
			return -NLE_NOMEM;

		// Add the grp port to old grp
		rtnl_mdb_add_mgport(old_mgp, cl_mgport);
		NL_DBG(2, "mgrp obj %p updated. Added "
			"grp port %p\n", old_mgp, cl_mgport);
	}
		break;
	case RTM_DELMDB : {
		/* delete the old_grp port
		 */
		if (found_old_grp_port) {
			rtnl_mdb_remove_mgport(old_mgp, old_mgport);
			NL_DBG(2, "mdb group %p updated. Removed "
				"group port %p\n", old_mgp, old_mgport);
			rtnl_mdb_mgport_free(old_mgport);
		}
	}
		break;
	default:
		NL_DBG(2, "Unknown action %d for %p group update\n", action, new_mgp);
		return -NLE_OPNOTSUPP;
	}

	return NLE_SUCCESS;
}

/* Update(Add/Delete) new multicast group information received
 * into the old group object present
 */
static int mdb_grp_update(struct rtnl_mdb *old_mdb, struct rtnl_mdb *new_mdb,
							int action)
{
	struct rtnl_mgrp *new_mgrp;
	struct rtnl_mgrp *old_mgrp = NULL;
	struct rtnl_mgport *old_mgport;
	int found_old_grp = 0;

	/* Find the first group in the mdb
	 */
	new_mgrp = rtnl_mdb_mgrp_n(new_mdb, 0);
	if (!new_mgrp)
		return -NLE_OPNOTSUPP;

	/* Find the new group in the old multicast group list
	 */
	nl_list_for_each_entry(old_mgrp, &old_mdb->m_grps, grp_list) {
		if (!nl_addr_cmp(old_mgrp->addr, new_mgrp->addr)) {
			found_old_grp = 1;
			break;
		}
	}

	/* If group is present, action is newmdb - do router port update
	 * If group is present, action is delmdb, old mgport > 1 - rport update
	 *  else, continue group update
	 */
	if (found_old_grp && ((action == RTM_NEWMDB) || ((action == RTM_DELMDB) &&
				(old_mgrp->m_ng_port > 1)))) {
		return mdb_grp_port_update(old_mgrp, new_mgrp, action);
	}

	switch(action) {
	case RTM_NEWMDB : {
		struct rtnl_mgrp *cl_mgrp;

		cl_mgrp = rtnl_mdb_mgrp_clone(new_mgrp);
		if (!cl_mgrp)
			return -NLE_NOMEM;
		// Add the grp to old mdb
		rtnl_mdb_add_mgrp(old_mdb, cl_mgrp);
		NL_DBG(2, "mdb obj %p updated. Added grp %p\n", old_mdb, cl_mgrp);
	}
		break;
	case RTM_DELMDB : {
		if (found_old_grp) {
			/* Remove last group port and group
			 */
			old_mgport = rtnl_mdb_mgport_n(old_mgrp, 0);
			if (old_mgport) {
				rtnl_mdb_remove_mgport(old_mgrp, old_mgport);
				NL_DBG(2, "Removed group port %p\n", old_mgport);
				rtnl_mdb_mgport_free(old_mgport);
			}
			rtnl_mdb_remove_mgrp(old_mdb, old_mgrp);
			NL_DBG(2, "mdb obj %p updated, Removed grp %p\n", old_mdb, old_mgrp);
			rtnl_mdb_mgrp_free(old_mgrp);
		}
		/* Check if old mdb has to be deleted, return error, if so
		 */
		if (!old_mdb->m_nr_grp && !old_mdb->m_nr_rport) {
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

/* Update(Add/Delete) new multicast database information received
 * into the old datase object present
 */
static int mdb_update(struct nl_object *old_obj, struct nl_object *new_obj)
{
	struct rtnl_mdb *new_mdb = (struct rtnl_mdb *) new_obj;
	struct rtnl_mdb *old_mdb = (struct rtnl_mdb *) old_obj;
	struct rtnl_mrport *new_mrport;
	int action = new_obj->ce_msgtype;
	int found_old_mrport = 0;
	struct rtnl_mrport *old_mrport;

	/* If group attr is present, do the group update, else only
	* router port update
	*/
	if (new_mdb->ce_mask & MDB_ATTR_ADDR) {
		return mdb_grp_update(old_mdb, new_mdb, action);
	}
	/* Get the first router port from the new mdb
	 */
	new_mrport = rtnl_mdb_mrport_n(new_mdb, 0);
	if (!new_mrport)
		return -NLE_OPNOTSUPP;

	/* Find if the mport exists in old mrport_list
	 */
	nl_list_for_each_entry(old_mrport, &old_mdb->m_rport, rtmr_list) {
		if (old_mrport->m_rpifindex == new_mrport->m_rpifindex) {
			found_old_mrport = 1;
			break;
		}
	}

	switch(action) {
	case RTM_NEWMDB : {
		struct rtnl_mrport *cl_mrport;

		// If mrport is already present, ignore new update
		if (found_old_mrport)
			return NLE_SUCCESS;

		cl_mrport = rtnl_mdb_mrport_clone(new_mrport);
		if (!cl_mrport)
			return -NLE_NOMEM;

		// Add the router port to old mdb
		rtnl_mdb_add_mrport(old_mdb, cl_mrport);

		NL_DBG(2, "mdb obj %p updated. Added "
			"router port %p\n", old_mdb, cl_mrport);
	}
		break;
	case RTM_DELMDB : {

		/*
		 * Find the mrport in old mdb and delete it
		 */
		if (found_old_mrport) {
			rtnl_mdb_remove_mrport(old_mdb, old_mrport);
			NL_DBG(2, "mdb obj %p updated. Removed "
				"router port %p\n", old_mdb, old_mrport);
			rtnl_mdb_mr_free(old_mrport);
		}
		/* Check if old mdb has to be deleted, return error, if so
		 */
		if (!old_mdb->m_nr_grp && !old_mdb->m_nr_rport) {
			NL_DBG(2, "deleting mdb %p\n", old_mdb);
			return -NLE_OPNOTSUPP;
		}
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

/* Parse multicast database netlink attributes in messages and allocate
 * appropriate objects
 */
static int mdb_msg_parser(struct nl_cache_ops *ops, struct sockaddr_nl *who,
			    struct nlmsghdr *n, struct nl_parser_param *pp)
{
	struct rtnl_mdb *mdb;
	struct nlattr *tb[MDBA_MAX + 1];
	struct br_mdb_entry *bm;
	int err;
	struct br_port_msg *br_p;

	mdb = rtnl_mdb_alloc();
	if (!mdb) {
		err = -NLE_NOMEM;
		goto errout;
	}

	mdb->ce_msgtype = n->nlmsg_type;
	br_p = nlmsg_data(n);

	err = nlmsg_parse(n, sizeof(*br_p), tb, MDBA_MAX, NULL);
	if (err < 0)
		goto errout;
	mdb->m_brifindex = br_p->ifindex;
	mdb->m_family = br_p->family;

	/* Sanity check - one of thse attributes should be present
	 */
	if (!tb[MDBA_MDB] && !tb[MDBA_ROUTER]) {
		NL_DBG(2, "mdb, rport attr not present 0x%x\n", mdb->m_brifindex);
		goto errout;
	}

	/* Parse MDB entry netlink attribute
	 */
	if (tb[MDBA_MDB]) {
		struct nlattr *me;
		char addr[INET6_ADDRSTRLEN+5];
		int rem_mdb_len;

		nla_for_each_nested(me, tb[MDBA_MDB], rem_mdb_len) {
			struct nlattr *mi;
			struct rtnl_mgrp *mgrp;
			struct rtnl_mgport *mgprt;
			int family;
			int rem_mdb_entry_len;
			int found_old_grp = 0;
			struct rtnl_mgrp *old_mgrp = NULL;

			nla_for_each_nested(mi, me, rem_mdb_entry_len) {
				bm = nla_data(mi);
				mgrp = rtnl_mdb_mgrp_alloc();
				if (!mgrp)
					return -NLE_NOMEM;
				/* Save the family info in nl_addr format
				 */
				if (htons(ETH_P_IP) == bm->addr.proto) {
					family = AF_INET;
				} else {
					family = AF_INET6;
				}
				mgrp->addr = nl_addr_build(family,
						(unsigned char *)&bm->addr.u,
						family==AF_INET ?
						sizeof(__be32):sizeof(struct in6_addr));
				mgprt = rtnl_mdb_mgport_alloc();
				if (!mgprt) {
					rtnl_mdb_mgrp_free(mgrp);
					return -NLE_NOMEM;
				}
				mgprt->m_grpifindex = bm->ifindex;
				/* Find the new group in the old multicast group list
				 * Add multicast group if not present
				 */
				nl_list_for_each_entry(old_mgrp, &mdb->m_grps, grp_list) {
					if (!nl_addr_cmp(old_mgrp->addr, mgrp->addr)) {
						found_old_grp = 1;
						rtnl_mdb_mgrp_free(mgrp);
						mgrp = old_mgrp;
						break;
					}
				}
				if (!found_old_grp)
					rtnl_mdb_add_mgrp(mdb, mgrp);
				/* Add multicast group port
				 */
				rtnl_mdb_add_mgport(mgrp, mgprt);
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
			mr = rtnl_mdb_mr_alloc();
			if (!mr)
				return -NLE_NOMEM;

			mr->m_rpifindex = ifindex;
			NL_DBG(2, "%s rp %d\n", __FUNCTION__, mr->m_rpifindex);
			// Add router port to mdb
			rtnl_mdb_add_mrport(mdb, mr);
			mdb->ce_mask |= MDB_ATTR_RPORT;
		}
	}

	// Sanity check
	if (mdb->ce_mask == 0) {
		NL_DBG(2, "rport, addr attr not parsed 0x%x\n", mdb->m_brifindex);
		goto errout;
	}
	mdb->ce_mask |= MDB_ATTR_IFINDEX;

	err = pp->pp_cb((struct nl_object *) mdb, pp);
errout:
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

		if (m->m_nr_grp)
			nl_dump(p, "Num of Groups %d \n", m->m_nr_grp);
		nl_list_for_each_entry(mgrp, &m->m_grps, grp_list) {
			nl_dump(p, "grp %s\n", nl_addr2str(mgrp->addr, addr, sizeof(addr)));
			nl_list_for_each_entry(prt, &mgrp->m_gport, gport_list) {
				if (link_cache)
					nl_dump(p, "dev %s \n",
							rtnl_link_i2name(link_cache, prt->m_grpifindex,
								state, sizeof(state)));
				else
					nl_dump(p, "dev %d \n", prt->m_grpifindex);
			}
		}
	}

	if (m->ce_mask & MDB_ATTR_RPORT) {
		struct rtnl_mrport *mr;

		if (m->m_nr_rport)
			nl_dump(p, "Num of Router ports %d \n", m->m_nr_rport);
		nl_list_for_each_entry(mr, &m->m_rport, rtmr_list) {
			if (link_cache)
				nl_dump(p, "rport %s \n",
						rtnl_link_i2name(link_cache, mr->m_rpifindex,
							state, sizeof(state)));
			else
				nl_dump(p, "rport %d \n", mr->m_rpifindex);
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

/* Allocate multicast database info
 */
struct rtnl_mdb *rtnl_mdb_alloc(void)
{
	return (struct rtnl_mdb *) nl_object_alloc(&mdb_obj_ops);
}

/* Release reference for multicast database info
 */
void rtnl_mdb_put(struct rtnl_mdb *mdb)
{
	nl_object_put((struct nl_object *) mdb);
}

/* Traverse multicast router port list
 */
void rtnl_mdb_foreach_mrport(struct rtnl_mdb *m,
				void (*cb)(struct rtnl_mrport *, void *), void *arg)
{
	struct rtnl_mrport *mr;

	if (m->ce_mask & MDB_ATTR_RPORT) {
		nl_list_for_each_entry(mr, &m->m_rport, rtmr_list) {
				cb(mr, arg);
		}
	}
}

/* Get nth element in multicast router port list
 */
struct rtnl_mrport *rtnl_mdb_mrport_n(struct rtnl_mdb *m, int n)
{
	struct rtnl_mrport *mr;
	int i;

	if (m->ce_mask & MDB_ATTR_RPORT && m->m_nr_rport > n) {
		i = 0;
		nl_list_for_each_entry(mr, &m->m_rport, rtmr_list) {
			if (i == n) return mr;
			i++;
		}
	}
	return NULL;
}

/* Traverse multicast group list
 */
void rtnl_mdb_foreach_mgrp(struct rtnl_mdb *m,
				void (*cb)(struct rtnl_mgrp *, void *),
				void *arg)
{
	struct rtnl_mgrp *mgrp;

	if (m->ce_mask & MDB_ATTR_ADDR) {
		nl_list_for_each_entry(mgrp, &m->m_grps, grp_list)
			cb(mgrp, arg);
	}
}

/* Get nth element in multicast group list
 */
struct rtnl_mgrp *rtnl_mdb_mgrp_n(struct rtnl_mdb *m, int n)
{
	struct rtnl_mgrp *mgrp;
	int i;

	if (m->ce_mask & MDB_ATTR_ADDR && m->m_nr_grp > n) {
		i = 0;
		nl_list_for_each_entry(mgrp, &m->m_grps, grp_list) {
			if (i == n) return mgrp;
			i++;
		}
	}
	return NULL;
}

/* Traverse multicast group port list
 */
void rtnl_mdb_foreach_mgport(struct rtnl_mgrp *grp,
				void (*cb)(struct rtnl_mgport *, void *),
				void *arg)
{
	struct rtnl_mgport *prt;

	nl_list_for_each_entry(prt, &grp->m_gport, gport_list)
		cb(prt, arg);
}

/* Get nth element in multicast group port list
 */
struct rtnl_mgport *rtnl_mdb_mgport_n(struct rtnl_mgrp *grp, int n)
{
	struct rtnl_mgport *prt;
	int i;

	if (grp->m_ng_port > n) {
		i = 0;
		nl_list_for_each_entry(prt, &grp->m_gport, gport_list) {
			if (i == n) return prt;
			i++;
		}
	}
        return NULL;
}

/* Get mdb family - currently AF_BRIDGE
 */
unsigned int rtnl_mdb_get_family(struct rtnl_mdb *mdb)
{
	return mdb->m_family;
}

/* Get Bridge ifindex
 */
unsigned int rtnl_mdb_get_brifindex(struct rtnl_mdb *mdb)
{
	return mdb->m_brifindex;
}

/* Get multicast group ifindex
 */
unsigned int rtnl_mdb_get_grpifindex(struct rtnl_mgport *mgp)
{
	return mgp->m_grpifindex;
}

/* Get multicast group IP addr
 */
struct nl_addr *rtnl_mdb_get_ipaddr(struct rtnl_mgrp *mgrp)
{
	return mgrp->addr;
}

/* Get number of multicast router ports
 */
unsigned int rtnl_mdb_get_nr_rport(struct rtnl_mdb *mdb)
{
	return mdb->m_nr_rport;
}

/* Get number of multicast groups
 */
unsigned int rtnl_mdb_get_nr_grps(struct rtnl_mdb *mdb)
{
	return mdb->m_nr_grp;
}

/* Remove multicast router port
 */
void rtnl_mdb_remove_mrport(struct rtnl_mdb *mdb, struct rtnl_mrport *mr)
{
	if (mdb->ce_mask & MDB_ATTR_RPORT) {
		mdb->m_nr_rport--;
		nl_list_del(&mr->rtmr_list);
	}
}

/* Add multicast router port
 */
void rtnl_mdb_add_mrport(struct rtnl_mdb *mdb, struct rtnl_mrport *mr)
{
	nl_list_add_tail(&mr->rtmr_list, &mdb->m_rport);
	mdb->m_nr_rport++;
	mdb->ce_mask |= MDB_ATTR_RPORT;
}

/* Free multicast router port
 */
void rtnl_mdb_mr_free(struct rtnl_mrport *mr)
{
	free(mr);
}

/* Allocate multicast router port
 */
struct rtnl_mrport *rtnl_mdb_mr_alloc(void)
{
	struct rtnl_mrport *mr;

	mr = calloc(1, sizeof(*mr));
	if (!mr)
		return NULL;

	nl_init_list_head(&mr->rtmr_list);

	return mr;
}

/* Get multicast router port ifindex
 */
unsigned int rtnl_mrport_get_rpifindex(struct rtnl_mrport *mrprt)
{
	return mrprt->m_rpifindex;
}

/* Allocate multicast group
 */
struct rtnl_mgrp *rtnl_mdb_mgrp_alloc(void)
{
	struct rtnl_mgrp *mg;

	mg = calloc(1, sizeof(*mg));
	if (!mg)
		return NULL;

	nl_init_list_head(&mg->grp_list);
	nl_init_list_head(&mg->m_gport);
	return mg;
}

/* Remove multicast group
 */
void rtnl_mdb_remove_mgrp(struct rtnl_mdb *mdb, struct rtnl_mgrp *mg)
{
	mdb->m_nr_grp--;
	nl_list_del(&mg->grp_list);
}

/* Add multicast group
 */
void rtnl_mdb_add_mgrp(struct rtnl_mdb *mdb, struct rtnl_mgrp *mg)
{
	nl_list_add_tail(&mg->grp_list, &mdb->m_grps);
	mdb->m_nr_grp++;
	mdb->ce_mask |= MDB_ATTR_ADDR;
}

/* Free multicast group
 */
void rtnl_mdb_mgrp_free(struct rtnl_mgrp *mg)
{
	nl_addr_put(mg->addr);
	free(mg);
}

/* Allocate multicast group port
 */
struct rtnl_mgport *rtnl_mdb_mgport_alloc(void)
{
	struct rtnl_mgport *mgprt;

	mgprt = calloc(1, sizeof(*mgprt));
	if (!mgprt)
		return NULL;

	nl_init_list_head(&mgprt->gport_list);

	return mgprt;
}

/* Remove multicast group port
 */
void rtnl_mdb_remove_mgport(struct rtnl_mgrp *mgrp, struct rtnl_mgport *mgprt)
{
	mgrp->m_ng_port--;
	nl_list_del(&mgprt->gport_list);
}

/* Add multicast group port
 */
void rtnl_mdb_add_mgport(struct rtnl_mgrp *mgrp, struct rtnl_mgport *mgprt)
{
	nl_list_add_tail(&mgprt->gport_list, &mgrp->m_gport);
	mgrp->m_ng_port++;
}

/* Free multicast group port
 */
void rtnl_mdb_mgport_free(struct rtnl_mgport *mgprt)
{
	free(mgprt);
}

static struct nl_object_ops mdb_obj_ops = {
	.oo_name		= "route/mdb",
	.oo_size		= sizeof(struct rtnl_mdb),
	.oo_constructor	= mdb_constructor,
	.oo_free_data	= mdb_free_data,
	.oo_clone		= mdb_clone,
	.oo_dump = {
		[NL_DUMP_LINE]	= mdb_dump_line,
		[NL_DUMP_DETAILS]	= mdb_dump_details,
		[NL_DUMP_STATS]	= mdb_dump_stats,
	},
	.oo_compare		= mdb_compare,
	.oo_update		= mdb_update,
	.oo_attrs2str	= mdb_attrs2str,
	.oo_id_attrs	= MDB_ATTR_IFINDEX
};

static struct nl_af_group mdb_groups[] = {
	{ PF_BRIDGE, RTNLGRP_MDB },
	{ END_OF_GROUP_LIST },
};

static struct nl_cache_ops rtnl_mdb_ops = {
	.co_name		= "route/mdb",
	.co_hdrsize		= sizeof(struct br_port_msg),
	.co_msgtypes	= {
					{ RTM_NEWMDB, NL_ACT_NEW, "new" },
					{ RTM_DELMDB, NL_ACT_DEL, "del" },
					{ RTM_GETMDB, NL_ACT_GET, "get" },
					END_OF_MSGTYPES_LIST,
		            },
	.co_protocol	= NETLINK_ROUTE,
	.co_groups		= mdb_groups,
	.co_request_update	= mdb_request_update,
	.co_msg_parser		= mdb_msg_parser,
	.co_obj_ops		= &mdb_obj_ops,
	.co_hash_size   = 4096,
};

static void __init mdb_init(void)
{
	nl_cache_mngt_register(&rtnl_mdb_ops);
}

static void __exit mdb_exit(void)
{
	nl_cache_mngt_unregister(&rtnl_mdb_ops);
}
