/*
 * lib/route/shdsl.c	SHDSL configuration
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2017 Volodymyr Bendiuga <volodymyr.bendiuga@gmail.com>
 */

#ifndef NETLINK_SHDSL_H_
#define NETLINK_SHDSL_H_

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/addr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDC_VER_STR 8*4

struct rtnl_shdsl;

struct rtnl_shdsl *rtnl_shdsl_alloc(void);
void rtnl_shdsl_put(struct rtnl_shdsl *shdsl);

int rtnl_shdsl_alloc_cache(struct nl_sock *sock, struct nl_cache **result);
struct rtnl_shdsl *rtnl_shdsl_get(struct nl_cache *cache, int channo);
struct rtnl_shdsl *rtnl_shdsl_get_by_ifindex(struct nl_cache *cache,
					     int ifindex);
int rtnl_shdsl_build_add_request(struct rtnl_shdsl *tmpl, int flags,
				 struct nl_msg **result);
int rtnl_shdsl_add(struct nl_sock *sk, struct rtnl_shdsl *tmpl, int flags);

void rtnl_shdsl_set_ifindex(struct rtnl_shdsl *shdsl, int ifindex);
int rtnl_shdsl_get_ifindex(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_ifname(struct rtnl_shdsl *shdsl, char *ifname);
char *rtnl_shdsl_get_ifname(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_enabled(struct rtnl_shdsl *shdsl, int enabled);
int rtnl_shdsl_get_enabled(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_channo(struct rtnl_shdsl *shdsl, int channo);
int rtnl_shdsl_get_channo(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_role(struct rtnl_shdsl *shdsl, int role);
int rtnl_shdsl_get_role(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_lff(struct rtnl_shdsl *shdsl, int lff);
int rtnl_shdsl_get_lff(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_ghs_thr(struct rtnl_shdsl *shdsl, int ghs_thr);
int rtnl_shdsl_get_ghs_thr(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_rate(struct rtnl_shdsl *shdsl, int rate);
int rtnl_shdsl_get_rate(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_average_bps(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_peak_bps(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_noise_margin(struct rtnl_shdsl *shdsl, int nm);
int rtnl_shdsl_get_noise_margin(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_nonstrict(struct rtnl_shdsl *shdsl, int nonstrict);
int rtnl_shdsl_get_nonstrict(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_low_jitter(struct rtnl_shdsl *shdsl, int lj);
int rtnl_shdsl_get_low_jitter(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_emf(struct rtnl_shdsl *shdsl, int emf);
int rtnl_shdsl_get_emf(struct rtnl_shdsl *shdsl);
void rtnl_shdsl_set_region(struct rtnl_shdsl *shdsl, int region);
int rtnl_shdsl_get_region(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_actual_rate(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_measured_snr(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_bits_p_sym(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_pow_backoff(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_pow_backoff_farend(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_attenuation(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_max_rate(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_capability_region(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_num_repeaters(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_num_wirepair(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_psd(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_remote_enabled(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_power_feeding(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_cc_noise_margin_up(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_cc_noise_margin_down(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_wc_noise_margin_up(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_wc_noise_margin_down(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_used_target_margins(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_ref_clk(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_line_probe(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_link_state(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_link_status(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_link_uptime(struct rtnl_shdsl *shdsl);
int rtnl_shdsl_get_no_of_negs(struct rtnl_shdsl *shdsl);
char *rtnl_shdsl_get_idc_ver(struct rtnl_shdsl *shdsl);
char *rtnl_shdsl_role2str(uint8_t role);
char *rtnl_shdsl_state2str(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif	/* NETLINK_SHDSL_H_ */

