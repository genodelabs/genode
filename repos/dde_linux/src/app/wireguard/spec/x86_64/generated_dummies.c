/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2022-07-19
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


#include <net/ipv6.h>

int __ipv6_addr_type(const struct in6_addr * addr)
{
	lx_emul_trace_and_stop(__func__);
}


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


#include <linux/uaccess.h>

unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ctype.h>

const unsigned char _ctype[] = {};


extern const char * _parse_integer_fixup_radix(const char * s,unsigned int * base);
const char * _parse_integer_fixup_radix(const char * s,unsigned int * base)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned int _parse_integer_limit(const char * s,unsigned int base,unsigned long long * p,size_t max_chars);
unsigned int _parse_integer_limit(const char * s,unsigned int base,unsigned long long * p,size_t max_chars)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

atomic_long_t _totalram_pages;


extern void ack_bad_irq(unsigned int irq);
void ack_bad_irq(unsigned int irq)
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


#include <linux/interrupt.h>

bool force_irqthreads;


#include <linux/netdevice.h>

void free_netdev(struct net_device * dev)
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


extern bool irq_fpu_usable(void);
bool irq_fpu_usable(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

int irq_set_affinity(unsigned int irq,const struct cpumask * cpumask)
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


extern void kernel_fpu_begin_mask(unsigned int kfpu_mask);
void kernel_fpu_begin_mask(unsigned int kfpu_mask)
{
	lx_emul_trace_and_stop(__func__);
}


extern void kernel_fpu_end(void);
void kernel_fpu_end(void)
{
	lx_emul_trace_and_stop(__func__);
}


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


#include <linux/smp.h>

int smp_call_function_single(int cpu,smp_call_func_t func,void * info,int wait)
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

