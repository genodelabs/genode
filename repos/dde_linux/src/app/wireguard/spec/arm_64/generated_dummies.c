/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2022-07-21
 */

#include <lx_emul.h>


#include <linux/ratelimit_types.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

struct page * __alloc_pages(gfp_t gfp,unsigned int order,int preferred_nid,nodemask_t * nodemask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/clk-provider.h>

const char * __clk_get_name(const struct clk * clk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

unsigned long __get_free_pages(gfp_t gfp_mask,unsigned int order)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ipv6.h>

int __ipv6_addr_type(const struct in6_addr * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

struct irq_domain * __irq_domain_add(struct fwnode_handle * fwnode,int size,irq_hw_number_t hwirq_max,int direct_max,const struct irq_domain_ops * ops,void * host_data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

struct irq_desc * __irq_resolve_mapping(struct irq_domain * domain,irq_hw_number_t hwirq,unsigned int * irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <asm-generic/percpu.h>

unsigned long __per_cpu_offset[NR_CPUS] = {};


#include <linux/mm.h>

void __put_page(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/task.h>

void __put_task_struct(struct task_struct * tsk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct sk_buff * __skb_gso_segment(struct sk_buff * skb,netdev_features_t features,bool tx_path)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ctype.h>

const unsigned char _ctype[] = {};


#include <linux/mm.h>

atomic_long_t _totalram_pages;


#include <crypto/internal/hash.h>

int crypto_register_shash(struct shash_alg * alg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct net_device * dev_get_by_index(struct net * net,int ifindex)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void dev_get_tstats64(struct net_device * dev,struct rtnl_link_stats64 * s)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern void flush_dcache_page(struct page * page);
void flush_dcache_page(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

bool force_irqthreads;


#include <linux/netdevice.h>

void free_netdev(struct net_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

void free_pages(unsigned long addr,unsigned int order)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/user.h>

void free_uid(struct user_struct * up)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

unsigned int fwnode_count_parents(const struct fwnode_handle * fwnode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

const char * fwnode_get_name(const struct fwnode_handle * fwnode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

const char * fwnode_get_name_prefix(const struct fwnode_handle * fwnode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

struct fwnode_handle * fwnode_get_nth_parent(struct fwnode_handle * fwnode,unsigned int depth)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

void fwnode_handle_put(struct fwnode_handle * fwnode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/genetlink.h>

void * genlmsg_put(struct sk_buff * skb,u32 portid,u32 seq,const struct genl_family * family,int flags,u8 cmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

bool gfp_pfmemalloc_allowed(gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 guid_index[16] = {};


#include <linux/irq.h>

void handle_fasteoi_irq(struct irq_desc * desc)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

const char hex_asc[] = {};


#include <linux/kernel.h>

const char hex_asc_upper[] = {};


#include <linux/icmpv6.h>

void icmp6_send(struct sk_buff * skb,u8 type,u8 code,__u32 info,const struct in6_addr * force_saddr,const struct inet6_skb_parm * parm)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/in6.h>

const struct in6_addr in6addr_any;


#include <net/netfilter/nf_conntrack.h>

struct net init_net;


#include <linux/sched.h>

void io_schedule_finish(int token)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

int io_schedule_prepare(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

long __sched io_schedule_timeout(long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ipv6.h>

int ip6_dst_hoplimit(struct dst_entry * dst)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ip_tunnels.h>

const struct header_ops ip_tunnel_header_ops;


#include <net/addrconf.h>

int ipv6_chk_addr(struct net * net,const struct in6_addr * addr,const struct net_device * dev,int strict)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

int irq_can_set_affinity(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

void irq_domain_free_irqs_common(struct irq_domain * domain,unsigned int virq,unsigned int nr_irqs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

void irq_domain_set_info(struct irq_domain * domain,unsigned int virq,irq_hw_number_t hwirq,struct irq_chip * chip,void * chip_data,irq_flow_handler_t handler,void * handler_data,const char * handler_name)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq.h>

void irq_modify_status(unsigned int irq,unsigned long clr,unsigned long set)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

int irq_set_affinity(unsigned int irq,const struct cpumask * cpumask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

void irq_set_default_host(struct irq_domain * domain)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqnr.h>

struct irq_desc * irq_to_desc(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq_work.h>

void irq_work_tick(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jiffies.h>

unsigned long volatile __cacheline_aligned_in_smp __jiffy_arch_data jiffies;


#include <linux/slab.h>

int kmem_cache_alloc_bulk(struct kmem_cache * s,gfp_t flags,size_t size,void ** p)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

void kmem_cache_destroy(struct kmem_cache * s)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kstrtox.h>

int kstrtoll(const char * s,unsigned int base,long long * res)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/delay.h>

unsigned long lpj_fine;


#include <linux/preempt.h>

void migrate_disable(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/preempt.h>

void migrate_enable(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void netif_carrier_off(struct net_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/of.h>

struct property * of_find_property(const struct device_node * np,const char * name,int * lenp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/of.h>

const struct fwnode_operations of_fwnode_ops;


#include <linux/of.h>

const char * of_prop_next_string(struct property * prop,const char * cur)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/of.h>

int of_property_read_string(const struct device_node * np,const char * propname,const char ** out_string)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/osq_lock.h>

bool osq_lock(struct optimistic_spin_queue * lock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/osq_lock.h>

void osq_unlock(struct optimistic_spin_queue * lock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

void page_frag_free(void * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/percpu_counter.h>

void percpu_counter_add_batch(struct percpu_counter * fbc,s64 amount,s32 batch)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int printk_deferred(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcutree.h>

void rcu_irq_enter_irqson(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcutree.h>

void rcu_irq_exit_irqson(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/rtnetlink.h>

void rtnl_link_unregister(struct rtnl_link_ops * ops)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sk_clear_memalloc(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sk_error_report(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sk_free(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

int skb_checksum_help(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void skb_set_owner_w(struct sk_buff * skb,struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sock_edemux(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jump_label.h>

bool static_key_initialized;


#include <linux/string_helpers.h>

int string_escape_mem(const char * src,size_t isz,char * dst,size_t osz,unsigned int flags,const char * only)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/clockchips.h>

void tick_broadcast(const struct cpumask * mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timerqueue.h>

bool timerqueue_add(struct timerqueue_head * head,struct timerqueue_node * node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timerqueue.h>

bool timerqueue_del(struct timerqueue_head * head,struct timerqueue_node * node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timerqueue.h>

struct timerqueue_node * timerqueue_iterate_next(struct timerqueue_node * node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/udp_tunnel.h>

int udp_sock_create6(struct net * net,struct udp_port_cfg * cfg,struct socket ** sockp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/udp_tunnel.h>

int udp_tunnel6_xmit_skb(struct dst_entry * dst,struct sock * sk,struct sk_buff * skb,struct net_device * dev,struct in6_addr * saddr,struct in6_addr * daddr,__u8 prio,__u8 ttl,__be32 label,__be16 src_port,__be16 dst_port,bool nocheck)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/udp_tunnel.h>

void udp_tunnel_sock_release(struct socket * sock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/net_namespace.h>

void unregister_pernet_device(struct pernet_operations * ops)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 uuid_index[16] = {};


#include <linux/sched/wake_q.h>

void wake_q_add_safe(struct wake_q_head * head,struct task_struct * task)
{
	lx_emul_trace_and_stop(__func__);
}

