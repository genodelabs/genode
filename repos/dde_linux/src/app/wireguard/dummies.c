/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>


#include <net/ipv6_stubs.h>

const struct ipv6_stub *ipv6_stub = NULL;


#include <linux/netdevice.h>

int register_netdevice(struct net_device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <net/net_namespace.h>

int register_pernet_device(struct pernet_operations * ops)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <net/sock.h>

void sk_set_memalloc(struct sock * sk)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void synchronize_net(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_lock(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_unlock(void)
{
	lx_emul_trace(__func__);
}


#include <linux/netlink.h>

void do_trace_netlink_extack(const char * msg)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void napi_enable(struct napi_struct * n)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void napi_disable(struct napi_struct * n)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void __netif_napi_del(struct napi_struct * napi)
{
	lx_emul_trace(__func__);
}


#include <linux/mmzone.h>

struct mem_section ** mem_section = NULL;


unsigned long phys_base = 0;


#include <asm/atomic.h>

DEFINE_STATIC_KEY_FALSE(bpf_stats_enabled_key);


#include <linux/net.h>

int net_ratelimit(void)
{
	lx_emul_trace(__func__);
	return 0;
}


__wsum csum_partial(const void * buff,int len,__wsum sum)
{
	lx_emul_trace_and_stop(__func__);
}
