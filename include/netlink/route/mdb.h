/* SPDX-License-Identifier: LGPL-2.1-only */

#ifndef NETLINK_MDB_H_
#define NETLINK_MDB_H_

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/link.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtnl_mdb;
struct rtnl_mdb_entry;

struct rtnl_mdb *rtnl_mdb_alloc(void);
void rtnl_mdb_put(struct rtnl_mdb *mdb);

struct rtnl_mdb_entry *rtnl_mdb_entry_alloc(void);
void rtnl_mdb_entry_free(struct rtnl_mdb_entry *mdb_entry);

int rtnl_mdb_alloc_cache(struct nl_sock *sk, struct nl_cache **result);
int rtnl_mdb_alloc_cache_flags(struct nl_sock *sock,
			       struct nl_cache **result,
			       unsigned int flags);

void     rtnl_mdb_set_ifindex(struct rtnl_mdb *mdb, uint32_t ifindex);
uint32_t rtnl_mdb_get_ifindex(struct rtnl_mdb *mdb);

void rtnl_mdb_add_entry(struct rtnl_mdb *mdb,
			struct rtnl_mdb_entry *_entry);

void rtnl_mdb_foreach_entry(struct rtnl_mdb *mdb,
			    void (*cb)(struct rtnl_mdb_entry *, void *),
			    void *arg);

struct rtnl_mdb_entry *rtnl_mdb_find_entry(struct rtnl_mdb *mdb, struct rtnl_mdb_entry *tmpl);

void rtnl_mdb_entry_set_ifindex(struct rtnl_mdb_entry *mdb_entry, uint32_t ifindex);
int  rtnl_mdb_entry_get_ifindex(struct rtnl_mdb_entry *mdb_entry);

void rtnl_mdb_entry_set_vid(struct rtnl_mdb_entry *mdb_entry, uint16_t vid);
int  rtnl_mdb_entry_get_vid(struct rtnl_mdb_entry *mdb_entry);

void rtnl_mdb_entry_set_state(struct rtnl_mdb_entry *mdb_entry, int state);
int  rtnl_mdb_entry_get_state(struct rtnl_mdb_entry *mdb_entry);

void rtnl_mdb_entry_set_addr(struct rtnl_mdb_entry *mdb_entry, struct nl_addr *addr);
struct nl_addr *rtnl_mdb_entry_get_addr(struct rtnl_mdb_entry *mdb_entry);

void     rtnl_mdb_entry_set_proto(struct rtnl_mdb_entry *mdb_entry, uint16_t proto);
uint16_t rtnl_mdb_entry_get_proto(struct rtnl_mdb_entry *mdb_entry);

int rtnl_mdb_add(struct nl_sock *sk, struct rtnl_mdb_entry *mdb_entry,
		 int ifindex, int flags);
int rtnl_mdb_del(struct nl_sock *sk, struct rtnl_mdb_entry *mdb_entry,
		 int ifindex, int flags);

#ifdef __cplusplus
}
#endif
#endif
