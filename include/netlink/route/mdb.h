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

extern struct rtnl_mdb *rtnl_mdb_alloc(void);
extern void	rtnl_mdb_put(struct rtnl_mdb *);
extern int	rtnl_mdb_alloc_cache(struct nl_sock *, struct nl_cache **);
void rtnl_mdb_remove_mrport(struct rtnl_mdb *mdb, struct rtnl_mrport *mr);
void rtnl_mdb_add_mrport(struct rtnl_mdb *mdb, struct rtnl_mrport *mr);
void rtnl_mdb_mr_free(struct rtnl_mrport *mr);
struct rtnl_mrport *rtnl_mdb_mr_alloc(void);
struct rtnl_mgrp *rtnl_mdb_mgrp_alloc(void);
void rtnl_mdb_remove_mgrp(struct rtnl_mdb *mdb, struct rtnl_mgrp *mg);
void rtnl_mdb_add_mgrp(struct rtnl_mdb *mdb, struct rtnl_mgrp *mg);
void rtnl_mdb_mgrp_free(struct rtnl_mgrp *mg);
struct rtnl_mgport *rtnl_mdb_mgport_alloc(void);
void rtnl_mdb_remove_mgport(struct rtnl_mgrp *mgrp, struct rtnl_mgport *mgprt);
void rtnl_mdb_add_mgport(struct rtnl_mgrp *mgrp, struct rtnl_mgport *mgprt);
void rtnl_mdb_mgport_free(struct rtnl_mgport *mgprt);
void rtnl_mdb_foreach_mrport(struct rtnl_mdb *m,
				void (*cb)(struct rtnl_mrport *, void *), void *arg);
void rtnl_mdb_foreach_mgrp(struct rtnl_mdb *m,
				void (*cb)(struct rtnl_mgrp *, void *), void *arg);
void rtnl_mdb_foreach_mgport(struct rtnl_mgrp *grp,
				void (*cb)(struct rtnl_mgport *, void *), void *arg);
extern struct rtnl_mrport * rtnl_mdb_mrport_n(struct rtnl_mdb *m, int n);
extern struct rtnl_mgrp *rtnl_mdb_mgrp_n(struct rtnl_mdb *m, int n);
struct rtnl_mgport *rtnl_mdb_mgport_n(struct rtnl_mgrp *grp, int n);
struct rtnl_mrport *rtnl_mdb_mrport_clone(struct rtnl_mrport *src);
struct rtnl_mgport *rtnl_mdb_mgport_clone(struct rtnl_mgport *src);
struct rtnl_mgrp *rtnl_mdb_mgrp_clone(struct rtnl_mgrp *src);
unsigned int rtnl_mdb_get_family(struct rtnl_mdb *mdb);
unsigned int rtnl_mdb_get_brifindex(struct rtnl_mdb *mdb);
void rtnl_mdb_set_brifindex(struct rtnl_mdb *mdb, int ifindex);
struct nl_addr *rtnl_mdb_get_ipaddr(struct rtnl_mgrp *grp);
void rtnl_mdb_set_ipaddr(struct rtnl_mgrp *grp, int ip);
unsigned int rtnl_mdb_get_nr_rport(struct rtnl_mdb *mdb);
unsigned int rtnl_mrport_get_rpifindex(struct rtnl_mrport *mrprt);
unsigned int rtnl_mdb_get_nr_grps(struct rtnl_mdb *mdb);
unsigned int rtnl_mdb_get_grpifindex(struct rtnl_mgport *mgp);
void rtnl_mdb_set_grpifindex(struct rtnl_mgport *mgp, int ifindex);
int rtnl_mdb_add_group (struct nl_sock *sk, struct rtnl_mdb *mdb, struct rtnl_mgrp *grp, int flags);
int rtnl_mdb_del_group (struct nl_sock *sk, struct rtnl_mdb *mdb, struct rtnl_mgrp *grp, int flags);
#ifdef __cplusplus
}
#endif

#endif
