/*
 * netlink/route/mdb.h	MDB
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2003-2008 Thomas Graf <tgraf@suug.ch>
 */

#ifndef NETLINK_MDB_H_
#define NETLINK_MDB_H_

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/addr.h>
#include <netlink/route/link.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtnl_mdb;
struct rtnl_mrport;
struct rtnl_mgport;
struct rtnl_mgrp;

/* MDB CACHE */
extern int	           rtnl_mdb_alloc_cache(struct nl_sock *sk, struct nl_cache **cache);
extern struct rtnl_mdb    *rtnl_mdb_get_by_ifi(struct nl_cache *cache, int ifi);

/* Multicast Router Port */
extern struct rtnl_mrport *rtnl_mrport_alloc(void);
extern void                rtnl_mrport_free(struct rtnl_mrport *mrp);
extern unsigned int        rtnl_mrport_get_grpifindex(struct rtnl_mrport *mrp);

/* Multicast Group Port */
extern struct rtnl_mgport *rtnl_mgport_alloc(void);
extern void                rtnl_mgport_free(struct rtnl_mgport *mgp);
extern unsigned int        rtnl_mgport_get_ifi(struct rtnl_mgport *mgp);
extern void                rtnl_mgport_set_ifi(struct rtnl_mgport *mgp, int ifi);

/* Multicast Group */
extern struct rtnl_mgrp   *rtnl_mgrp_alloc(void);
extern void                rtnl_mgrp_free(struct rtnl_mgrp *mgrp);
extern void                rtnl_mgrp_add_mgport(struct rtnl_mgrp *mgrp,
						struct rtnl_mgport *mgport);
extern void                rtnl_mgrp_del_mgport(struct rtnl_mgrp *mgrp,
						struct rtnl_mgport *mgport);
extern void                rtnl_mgrp_free_mgports(struct rtnl_mgrp *mgrp);
extern unsigned int        rtnl_mgrp_get_num_mgport(struct rtnl_mgrp *mgrp);
extern void                rtnl_mgrp_foreach_mgport(struct rtnl_mgrp *mgrp,
						    void (*cb)(struct rtnl_mgport *, void *),
						    void *arg);
extern struct rtnl_mgport *rtnl_mgrp_mgport_n(struct rtnl_mgrp *grp, int n);
extern void                rtnl_mgrp_set_ipaddr(struct rtnl_mgrp *mgrp, int ip);
extern struct nl_addr     *rtnl_mgrp_get_ipaddr(struct rtnl_mgrp *mgrp);
extern void                rtnl_mgrp_set_macaddr(struct rtnl_mgrp *mgrp, uint8_t *mac);
extern struct nl_addr     *rtnl_mgrp_get_macaddr(struct rtnl_mgrp *mgrp);
extern void                rtnl_mgrp_set_vid(struct rtnl_mgrp *mgrp, int vid);
extern int                 rtnl_mgrp_get_vid(struct rtnl_mgrp *mgrp);
extern int                 rtnl_mgrp_get_state(struct rtnl_mgrp *mgrp);

/* MDB */
extern struct rtnl_mdb    *rtnl_mdb_alloc(void);
extern void	           rtnl_mdb_put(struct rtnl_mdb *);
extern void                rtnl_mdb_set_brifindex(struct rtnl_mdb *mdb, int ifi);
extern unsigned int        rtnl_mdb_get_brifindex(struct rtnl_mdb *mdb);
extern void                rtnl_mdb_set_vid(struct rtnl_mgrp *grp, int vid);
extern int                 rtnl_mdb_get_vid(struct rtnl_mgrp *grp);
extern unsigned int        rtnl_mdb_get_family(struct rtnl_mdb *mdb);

extern void                rtnl_mdb_add_mrport(struct rtnl_mdb *mdb, struct rtnl_mrport *mr);
extern void                rtnl_mdb_del_mrport(struct rtnl_mdb *mdb, struct rtnl_mrport *mr);
extern unsigned int        rtnl_mdb_get_num_mrport(struct rtnl_mdb *mdb);
extern struct rtnl_mrport *rtnl_mdb_mrport_n(struct rtnl_mdb *mdb, int n);
extern void                rtnl_mdb_foreach_mrport(struct rtnl_mdb *mdb,
						   void (*cb)(struct rtnl_mrport *, void *),
						   void *arg);

extern void                rtnl_mdb_add_mgrp(struct rtnl_mdb *mdb, struct rtnl_mgrp *mg);
extern void                rtnl_mdb_del_mgrp(struct rtnl_mdb *mdb, struct rtnl_mgrp *mg);
extern struct rtnl_mgrp   *rtnl_mdb_mgrp_n(struct rtnl_mdb *mdb, int n);
extern unsigned int        rtnl_mdb_get_num_mgrp(struct rtnl_mdb *mdb);
extern void                rtnl_mdb_foreach_mgrp(struct rtnl_mdb *mdb,
						 void (*cb)(struct rtnl_mgrp *, void *),
						 void *arg);

extern int                 rtnl_mdb_add_group(struct nl_sock *sk, struct rtnl_mdb *mdb,
					      struct rtnl_mgrp *mgrp, int flags);
extern int                 rtnl_mdb_del_group(struct nl_sock *sk, struct rtnl_mdb *mdb,
					      struct rtnl_mgrp *mgrp, int flags);

#ifdef __cplusplus
}
#endif

#endif	/* NETLINK_MDB_H_ */
