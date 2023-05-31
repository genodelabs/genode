/**
 * \brief  Dummy definitions of lx_emul
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2022-01-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* app/wireguard includes */
#include <lx_emul.h>

/* dde_linux/src/include/lx_emul */
#include <lx_emul/random.h>


#include <net/icmp.h>

void __icmp_send(struct sk_buff * skb_in,int type,int code,__be32 info,const struct ip_options * opt)
{
	printk("Warning: sending ICMP not supported\n");
	kfree_skb(skb_in);
}


#include <linux/random.h>

int wait_for_random_bytes(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/prandom.h>

u8 get_random_u8(void)
{
	u8 ret;
	lx_emul_random_gen_bytes(&ret, sizeof(ret));
	return ret;
}


#include <linux/mm.h>

void * kvmalloc_node(size_t size,gfp_t flags,int node)
{
	return kmalloc(size, flags);
}


#include <net/rtnetlink.h>

extern void genode_wg_rtnl_link_ops(struct rtnl_link_ops * ops);

int rtnl_link_register(struct rtnl_link_ops * ops)
{
	genode_wg_rtnl_link_ops(ops);
	return 0;
}


#include <net/genetlink.h>

extern void genode_wg_genl_family(struct genl_family * family);

int genl_register_family(struct genl_family * family)
{
	genode_wg_genl_family(family);
	return 0;
}


#include <linux/netdevice.h>

extern struct net_device * genode_wg_net_device(void);

struct net_device * dev_get_by_name(struct net * net,const char * name)
{
	return genode_wg_net_device();
}


#include <net/udp_tunnel.h>

int udp_sock_create4(struct net * net,struct udp_port_cfg * cfg,struct socket ** sockp)
{
	*sockp = (struct socket*) kmalloc(sizeof(struct socket), GFP_KERNEL);
	(*sockp)->sk = (struct sock*) kmalloc(sizeof(struct sock), GFP_KERNEL);
	return 0;
}


#include <net/udp_tunnel.h>

extern void genode_wg_udp_tunnel_sock_cfg(struct udp_tunnel_sock_cfg * cfg);

void setup_udp_tunnel_sock(struct net * net,struct socket * sock,struct udp_tunnel_sock_cfg * cfg)
{
	genode_wg_udp_tunnel_sock_cfg(cfg);
}


#include <linux/ipv6.h>

bool ipv6_mod_enabled(void)
{
	return false;
}


#include <net/udp_tunnel.h>
#include <genode_c_api/wireguard.h>

void udp_tunnel_xmit_skb(
	struct rtable * rt,struct sock * sk,struct sk_buff * skb,
	__be32 src,__be32 dst,__u8 tos,__u8 ttl,__be16 df,
	__be16 src_port,__be16 dst_port,bool xnet,bool nocheck)
{
	/*
	 * FIXME
	 *
	 * I don't propagate these values because I found that, at the call to
	 * udp_tunnel_xmit_skb, Linux sets them hard to the values I'm checking
	 * for in the following. Furthermore, I assume that they should not be
	 * relevant for the port anyway.
	 */
	if (xnet != false) {
		pr_info("Error: XNET != false is not expected\n");
		while (1) { }
	}
	if (nocheck != false) {
		pr_info("Error: NOCHECK != false is not expected\n");
		while (1) { }
	}
	if (df != 0) {
		pr_info("Error: DF != 0 is not expected\n");
		while (1) { }
	}
	/*
	 * FIXME
	 *
	 * Manually set TTL. In the Linux reference scenario this argument is
	 * observed to be 64. Here it is 0. Further Linux contrib code would have
	 * to be incorporated in order to make Wireguard provide a correct TTL
	 * argument. However, it is simpler to set it manually.
	 */
	if (ttl != 0) {
		pr_info("Error: TTL != 0 is not expected\n");
		while (1) { }
	}
	ttl = 64;

	/*
	 * FIXME
	 *
	 * Manually set UDP destination port. In the Linux reference scenario this
	 * argument is observed to be the listen port. Here it is som other port.
	 * Further Linux contrib code would have to be incorporated in order to
	 * make Wireguard provide a correct argument. However, it is simpler to
	 * set it manually.
	 */
	src_port = htons(genode_wg_listen_port());

	/*
	 * This is the SKB that we put into WireGuard earlier in
	 * _genode_wg_uplink_connection_receive. WireGuard modified it to become
	 * the SKB that we want to send at the NIC connection. WireGuard
	 * assumes that we free the SKB after having sent it.
	 */
	genode_wg_send_wg_prot_at_nic_connection(
		skb->data, skb->len, src_port, dst_port, src, dst,
		tos, ttl);

	kfree_skb(skb);
}


#include <net/sock.h>

DEFINE_STATIC_KEY_FALSE(memalloc_socks_key);
EXPORT_SYMBOL_GPL(memalloc_socks_key);


#include <linux/slab.h>

struct kmem_cache * kmem_cache_create_usercopy(const char * name,
                                               unsigned int size,
                                               unsigned int align,
                                               slab_flags_t flags,
                                               unsigned int useroffset,
                                               unsigned int usersize,
                                               void (* ctor)(void *))
{
	return kmem_cache_create(name, size, align, flags, ctor);
}


#include <net/ip_tunnels.h>

/* Returns either the correct skb->protocol value, or 0 if invalid. */
__be16 ip_tunnel_parse_protocol(const struct sk_buff *skb)
{
	//FIXME: we just assume IPv4
	return htons(ETH_P_IP);
}


#include <linux/inetdevice.h>

__be32 inet_confirm_addr(struct net * net,struct in_device * in_dev,__be32 dst,__be32 local,int scope)
{
	lx_emul_trace(__func__);
	return local;
}


#include <net/route.h>

struct rtable * ip_route_output_flow(struct net * net,struct flowi4 * flp4,const struct sock * sk)
{
	static bool initialized = false;
	static struct dst_metrics dst_default_metrics;
	static struct rtable rt;
	if (!initialized) {
		rt.dst.dev = genode_wg_net_device();
		dst_init_metrics(&rt.dst, dst_default_metrics.metrics, true);
		initialized = true;
	}
	return &rt;
}


#include <linux/slab.h>

void kfree_sensitive(const void * p)
{
	kfree(p);
}


#include <linux/netdevice.h>

gro_result_t napi_gro_receive(struct napi_struct * napi,struct sk_buff * skb)
{
	/*
	 * This is the SKB that we put into WireGuard earlier in
	 * _genode_wg_nic_connection_receive. WireGuard modified it to become
	 * the SKB that we want to send at the Uplink connection. WireGuard
	 * assumes that we free the SKB after having sent it.
	 */
	genode_wg_send_ip_at_uplink_connection(skb->data, skb->len);
	kfree_skb(skb);

	/*
	 * FIXME
	 *
	 * The Wireguard contrib code currently ignores this return value.
	 * Considering the possibility that, one day, it does otherwise, I return
	 * an invalid value here in the hope that it complains. The reason is that
	 * I don't understand GRO and its return values fully and don't want to
	 * dive into it because I hope that it will not become relevant anyway.
	 */
	return -1;
}


#include <linux/netdevice.h>

void netif_napi_add_weight(struct net_device *dev, struct napi_struct *napi,
                           int (*poll)(struct napi_struct *, int), int weight)
{
	napi->dev = dev;
	napi->poll = poll;
	napi->weight = weight;
}


#include <linux/netdevice.h>

bool napi_schedule_prep(struct napi_struct * n)
{
	return true;
}


#include <linux/netdevice.h>

void __napi_schedule(struct napi_struct * n)
{
	int weight = n->weight;
	if (n->poll(n, n->weight) >= weight) {
		printk("Warning: more work to do?\n");
		lx_emul_trace_and_stop(__func__);
	}
}


#include <linux/netdevice.h>

bool napi_complete_done(struct napi_struct * n,int work_done)
{
	lx_emul_trace(__func__);
	return true;
}


#include <linux/once.h>

void __do_once_done(bool * done,struct static_key_true * once_key,unsigned long * flags,struct module * mod)
{
	*done = true;
}


#include <linux/once.h>

bool __do_once_start(bool * done,unsigned long * flags)
{
	return !*done;
}


#ifdef CONFIG_X86_64
DEFINE_PER_CPU(void *, hardirq_stack_ptr);
#endif
DEFINE_PER_CPU(bool, hardirq_stack_inuse);
