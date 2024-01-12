/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright (c) 2022 Joachim Wiberg <troglobit@gmail.com>
 */

#include <net/if.h>
#include <netlink/cli/utils.h>
#include <netlink/cli/link.h>
#include <netlink/cli/mdb.h>

#include <linux/if.h>
#include <linux/netlink.h>

typedef enum {
	SHOW,
	CREATE,
	DELETE,
	ATTACH,
	DETACH,
	MDB_ADD,
	MDB_DEL,
	VLAN_ADD,
	VLAN_DEL,
} op_t;

static struct nl_sock *sk;
static char *ports[32] = { 0 };
static int num_ports = 0;

static void show_one(struct nl_object *obj, void *arg)
{
	struct rtnl_link *link = nl_object_priv(obj);
	struct rtnl_link *master = arg;
	struct nl_dump_params dp = {
		.dp_type = NL_DUMP_DETAILS,
		.dp_fd = stdout,
	};
	int ifindex;

	if (master) {
		ifindex = rtnl_link_get_master(link);
		if (ifindex != rtnl_link_get_ifindex(master))
			return;
	}

//	printf("%-15s  %d\n", rtnl_link_get_name(link), rtnl_link_get_family(link));
	nl_object_dump(OBJ_CAST(link), &dp);
}

static void dump_one_neigh(struct nl_object *obj, void *arg)
{
	struct rtnl_neigh *neigh = (struct rtnl_neigh *)obj;
	struct nl_dump_params dp = {
		.dp_type = NL_DUMP_LINE,
		.dp_fd = stdout,
	};

	if (rtnl_neigh_get_family(neigh) != AF_BRIDGE)
		return;

	nl_object_dump(obj, &dp);
}

static void dump_one_mdb(struct nl_object *obj, void *arg)
{
	struct nl_dump_params dp = {
		.dp_type = NL_DUMP_LINE,
		.dp_fd = stdout,
	};

	nl_object_dump(obj, &dp);
}

static void nl_cli_bridge_show(struct rtnl_link *master)
{
	struct nl_cache *all_links;
	struct nl_cache *links;
	struct nl_cache *cache;
	int err;

	all_links = nl_cli_link_alloc_cache(sk);
	links = nl_cli_link_alloc_cache_family(sk, AF_BRIDGE);

	nl_cache_foreach(links, show_one, master);

	printf("fdb ==================================================\n");
	err = rtnl_neigh_alloc_cache_flags(sk, &cache, NL_CACHE_AF_ITER);
	if (err)
		nl_cli_fatal(err, "Failed reading fdb");
	nl_cache_foreach(cache, dump_one_neigh, NULL);
	nl_cache_free(cache);

	printf("mdb ==================================================\n");
	err = rtnl_mdb_alloc_cache_flags(sk, &cache, 0);
	if (err)
		nl_cli_fatal(err, "Failed reading mdb");
	nl_cache_foreach(cache, dump_one_mdb, NULL);
	nl_cache_free(cache);

	nl_cache_free(all_links);
	nl_cache_free(links);
}

static void filter_cb(struct nl_object *obj, void *data)
{
	struct rtnl_link *link = nl_object_priv(obj);
	char *ifname;
	int i;

	ifname = rtnl_link_get_name(link);
	if (!ifname)
		return;

	for (i = 0; i < num_ports; i++) {
		if (ports[i] && !strcmp(ports[i], ifname))
			return;
	}

	nl_cache_remove(OBJ_CAST(link));
}

static void activate_cb(struct nl_object *obj, void *data)
{
	struct rtnl_link *link = nl_object_priv(obj);
	struct rtnl_link *change = data;

	if (!change)
		change = link;

	rtnl_link_change(sk, link, change, 0);
}

static int usage(int rc)
{
	printf("Usage: nl-bridge [OPTION] [bridge [ports]]\n"
	       "\n"
	       "Options\n"
	       " -h, --help            Show this help.\n"
	       " -l, --debug=LEVEL     Set libnl debug level { 0 - 7 }\n"
	       "\n"
	       " -c, --create          Create a bridge.  To create a VLAN filtering bridge, set\n"
	       "                       '-v VID', this sets the vlan_default_pvid to VID, which\n"
	       "                       is the VLAN new ports are assigned to by default.  Use\n"
	       "                       '-v 0' to disable automatic VLAN assignment\n"
	       " -r, --remove          Remove a bridge\n"
	       "\n"
	       " -a, --attach          Attach bridge port(s)\n"
	       " -d, --detach          Detach bridge port(s)\n"
	       "\n"
	       " -m, --mdb-add=ADDR    Add MDB entry ADDR\n"
	       " -M, --mdb-del=ADDR    Del MDB entry ADDR\n"
	       "\n"
	       " -p, --pvid            Set VID as the PVID (default VLAN) for the port\n"
	       " -u, --untagged        Set port(s) as untagged member(s) of VLAN VID\n"
	       " -v, --vlan-add=VID    Associate port(s) with VLAN VID, see -p and -u\n"
	       " -V, --vlan-del=VID    Dissociate port(s) from VLAN VID\n"
	       "\n"
	       "Without any arguments, nl-bridge shows info for all bridges.\n");
        return rc;
}

int main(int argc, char *argv[])
{
	struct rtnl_link *link, *change;
	struct bridge_vlan_info vinfo;
	struct nl_cache *links = NULL;
	struct rtnl_mdb_entry *entry;
	struct nl_addr *group = NULL;
	struct rtnl_link *br = NULL;
	int untagged = 0;
	char *nm = NULL;
	op_t op = SHOW;
	int vlan = -1;
	int pvid = 0;
	int err = 0;
	int i;

	for (;;) {
		int c;
		static struct option long_opts[] = {
			{ "create",   0, 0, 'c' },
			{ "remove",   0, 0, 'r' },
			{ "attach",   0, 0, 'a' },
			{ "detach",   0, 0, 'd' },
			{ "debug",    1, 0, 'l' },
			{ "help",     0, 0, 'h' },
			{ "mdb-add",  1, 0, 'm' },
			{ "mdb-del",  1, 0, 'M' },
			{ "untagged", 0, 0, 'u' },
			{ "vlan-add", 1, 0, 'v' },
			{ "vlan-del", 1, 0, 'V' },
			{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "acdhl:m:M:pruv:V:", long_opts, NULL);
		if (c == -1)
                        break;

                switch (c) {
		case 'a':
			op = ATTACH;
			break;
		case 'd':
			op = DETACH;
			break;
		case 'c':
			op = CREATE;
			break;
		case 'h':
			return usage(0);
		case 'l':
			nl_debug = (int)nl_cli_parse_u32(optarg);
			break;
		case 'm':
			op = MDB_ADD;
			group = nl_cli_addr_parse(optarg, AF_UNSPEC);
			break;
		case 'M':
			op = MDB_DEL;
			group = nl_cli_addr_parse(optarg, AF_UNSPEC);
			break;
		case 'p':
			pvid = 1;
			break;
		case 'u':
			untagged = 1;
			break;
		case 'v':
			if (op == SHOW)
				op = VLAN_ADD;
			vlan = (int)nl_cli_parse_u32(optarg);
			break;
		case 'V':
			op = VLAN_DEL;
			vlan = (int)nl_cli_parse_u32(optarg);
			break;
		default:
			return usage(1);
		}
	}

	sk = nl_cli_alloc_socket();
	nl_cli_connect(sk, NETLINK_ROUTE);
	change = nl_cli_link_alloc();

	for (i = optind; argc > i && num_ports < (sizeof(ports) / sizeof(ports[0])); i++) {
		if (!nm) {
			nm = argv[i];
			continue;
		}
		ports[num_ports++] = argv[i];
	}

	/* Fallback if bridge arg is missing */
	if (!nm)
		op = SHOW;

	switch (op) {
	case CREATE:
		br = rtnl_link_bridge_alloc();
		if (!br)
			nl_cli_fatal(err, "Failed creating bridge %s", nm);

		rtnl_link_set_name(br, nm);
		if (vlan != -1) {
			rtnl_link_bridge_set_vlan_filtering(br, 1);
			rtnl_link_bridge_set_vlan_default_pvid(br, vlan);
		}
		err = rtnl_link_add(sk, br, NLM_F_CREATE);
		rtnl_link_put(br);
		break;

	case ATTACH:
	case DETACH:
		err = rtnl_link_get_kernel(sk, 0, nm, &br);
		if (err)
			nl_cli_fatal(err, "Cannot find bridge %s", nm);

		links = nl_cli_link_alloc_cache(sk);
		nl_cache_foreach(links, filter_cb, ports);

		for (i = 0; i < num_ports; i++) {
			link = rtnl_link_get_by_name(links, ports[i]);
			if (!link) {
				fprintf(stderr, "Cannot find %s, skipping\n", ports[i]);
				continue;
			}

			if (op == ATTACH) {
				/* port must be up before attaching to a bridge */
				rtnl_link_set_flags(change, IFF_UP);
				rtnl_link_change(sk, link, change, 0);
				rtnl_link_set_master(change, rtnl_link_get_ifindex(br));
			} else
				rtnl_link_set_master(change, 0);
		}
		nl_cache_foreach(links, activate_cb, change);
		rtnl_link_put(br);
		break;

	case MDB_ADD:
	case MDB_DEL:
		err = rtnl_link_get_kernel(sk, 0, nm, &br);
		if (err)
			nl_cli_fatal(err, "Cannot find bridge %s", nm);

		entry = rtnl_mdb_entry_alloc();
		if (!entry)
			nl_cli_fatal(1, "Failed allocating MDB entry");

		rtnl_mdb_entry_set_addr(entry, group);
		rtnl_mdb_entry_set_state(entry, MDB_TEMPORARY);
		if (vlan != -1)
			rtnl_mdb_entry_set_vid(entry, vlan);

		links = nl_cli_link_alloc_cache(sk);
		for (i = 0; i < num_ports; i++) {
			int nlflags = 0;
			char buf[256];

			link = rtnl_link_get_by_name(links, ports[i]);
			if (!link) {
				fprintf(stderr, "Cannot find %s, skipping\n", ports[i]);
				continue;
			}

			nl_addr2str(group, buf, sizeof(buf));
			rtnl_mdb_entry_set_ifindex(entry, rtnl_link_get_ifindex(link));
			if (op == MDB_ADD) {
				nlflags = NLM_F_EXCL | NLM_F_CREATE;
				err = rtnl_mdb_add(sk, entry, rtnl_link_get_ifindex(br), nlflags);
				if (err)
					nl_cli_fatal(err, "Failed adding group %s to vid %d: %s",
						     buf, vlan, nl_geterror(err));
			} else {
				err = rtnl_mdb_del(sk, entry, rtnl_link_get_ifindex(br), nlflags);
				if (err)
					nl_cli_fatal(err, "Failed deleting group %s to vid %d: %s",
						     buf, vlan, nl_geterror(err));
			}
		}
		break;

	case VLAN_ADD:
	case VLAN_DEL:
		vinfo.vid   = vlan;
		vinfo.flags = 0;

		if (pvid)
			vinfo.flags |= BRIDGE_VLAN_INFO_PVID;
		if (untagged)
			vinfo.flags |= BRIDGE_VLAN_INFO_UNTAGGED;

		links = nl_cli_link_alloc_cache_family(sk, AF_BRIDGE);
		nl_cache_foreach(links, filter_cb, ports);

		for (i = 0; i < num_ports; i++) {
			link = rtnl_link_get_by_name(links, ports[i]);
			if (!link) {
				fprintf(stderr, "Cannot find %s, skipping\n", ports[i]);
				continue;
			}

			if (op == VLAN_ADD)
				rtnl_link_bridge_vlan_add(link, &vinfo);
			else
				rtnl_link_bridge_vlan_del(link, vlan);
		}
		nl_cache_foreach(links, activate_cb, NULL);
		break;

	default:
	case SHOW:
		if (nm)
			rtnl_link_get_kernel(sk, 0, nm, &br);
		nl_cli_bridge_show(br);
		rtnl_link_put(br);
		break;
	}

	nl_socket_free(sk);

	return err;
}
