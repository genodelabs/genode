/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2024-07-17
 */

#include <lx_emul.h>


#include <linux/clk-provider.h>

const char * __clk_get_name(const struct clk * clk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

int __devm_mdiobus_register(struct device * dev,struct mii_bus * bus,struct module * owner)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ipv6.h>

int __ipv6_addr_type(const struct in6_addr * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

struct irq_desc * __irq_resolve_mapping(struct irq_domain * domain,irq_hw_number_t hwirq,unsigned int * irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int __printk_ratelimit(const char * func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void __printk_safe_enter(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void __printk_safe_exit(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/task.h>

void __put_task_struct(struct task_struct * tsk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void __skb_get_hash(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int _printk_deferred(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int add_uevent_var(struct kobj_uevent_env * env,const char * format,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/async.h>

async_cookie_t async_schedule_node(async_func_t func,void * data,int node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/async.h>

void async_synchronize_full(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

void bpf_warn_invalid_xdp_action(struct net_device * dev,struct bpf_prog * prog,u32 act)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

struct mii_bus * devm_mdiobus_alloc_size(struct device * dev,int sizeof_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/math64.h>

u64 div64_u64(u64 dividend,u64 divisor)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/math64.h>

u64 div64_u64_rem(u64 dividend,u64 divisor,u64 * remainder)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/math64.h>

s64 div_s64_rem(s64 dividend,s32 divisor,s32 * remainder)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-map-ops.h>

bool dma_default_coherent;


#include <linux/netlink.h>

void do_trace_netlink_extack(const char * msg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst.h>

void dst_release(struct dst_entry * dst)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool.h>

void ethtool_convert_legacy_u32_to_link_mode(unsigned long * dst,u32 legacy_u32)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool.h>

bool ethtool_convert_link_mode_to_legacy_u32(u32 * legacy_u32,const unsigned long * src)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool.h>

u32 ethtool_op_get_link(struct net_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool.h>

int ethtool_op_get_ts_info(struct net_device * dev,struct ethtool_ts_info * info)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/capability.h>

bool file_ns_capable(const struct file * file,struct user_namespace * ns,int cap)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcuwait.h>

void finish_rcuwait(struct rcuwait * w)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

int genphy_resume(struct phy_device * phydev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

int get_option(char ** str,int * pint)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

bool gfp_pfmemalloc_allowed(gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct packet_offload * gro_find_complete_by_type(__be16 type)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct packet_offload * gro_find_receive_by_type(__be16 type)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 guid_index[16] = {};


#include <net/addrconf.h>

void in6_dev_finish_destroy(struct inet6_dev * idev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/utsname.h>

struct user_namespace init_user_ns;


#include <linux/init.h>

bool initcall_debug;


#include <linux/sched.h>

void __sched io_schedule(void)
{
	lx_emul_trace_and_stop(__func__);
}


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


#include <linux/irqdomain.h>

void irq_set_default_host(struct irq_domain * domain)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq_work.h>

void irq_work_tick(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int kobject_synth_uevent(struct kobject * kobj,const char * buf,size_t count)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int kobject_uevent_env(struct kobject * kobj,enum kobject_action action,char * envp_ext[])
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct pernet_operations __net_initdata loopback_net_ops;


#include <linux/delay.h>

unsigned long lpj_fine;


#include <linux/phy.h>

void mdiobus_free(struct mii_bus * bus)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mdio.h>

struct phy_device * mdiobus_get_phy(struct mii_bus * bus,int addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

void mdiobus_unregister(struct mii_bus * bus)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sch_generic.h>

struct Qdisc_ops mq_qdisc_ops;


#include <linux/netdevice.h>

void napi_gro_flush(struct napi_struct * napi,bool flush_old)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

int net_ratelimit(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/selftests.h>

void net_selftest(struct net_device * ndev,struct ethtool_test * etest,u64 * buf)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/selftests.h>

int net_selftest_get_count(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/selftests.h>

void net_selftest_get_strings(u8 * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/netlink.h>

int nla_put(struct sk_buff * skb,int attrtype,int attrlen,const void * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

loff_t noop_llseek(struct file * file,loff_t offset,int whence)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/of.h>

bool of_device_is_available(const struct device_node * device)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/of_device.h>

void of_device_uevent(struct device * dev,struct kobj_uevent_env * env)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/of.h>

struct property * of_find_property(const struct device_node * np,const char * name,int * lenp)
{
	lx_emul_trace_and_stop(__func__);
}


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


#include <linux/panic.h>

void panic(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

struct phy_device * phy_connect(struct net_device * dev,const char * bus_id,void (* handler)(struct net_device *),phy_interface_t interface)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

void phy_disconnect(struct phy_device * phydev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

int phy_do_ioctl_running(struct net_device * dev,struct ifreq * ifr,int cmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

int phy_ethtool_get_link_ksettings(struct net_device * ndev,struct ethtool_link_ksettings * cmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

int phy_ethtool_nway_reset(struct net_device * ndev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

int phy_ethtool_set_link_ksettings(struct net_device * ndev,const struct ethtool_link_ksettings * cmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

void phy_print_status(struct phy_device * phydev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

void phy_stop(struct phy_device * phydev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

int phy_suspend(struct phy_device * phydev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

int phylink_connect_phy(struct phylink * pl,struct phy_device * phy)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

struct phylink * phylink_create(struct phylink_config * config,struct fwnode_handle * fwnode,phy_interface_t iface,const struct phylink_mac_ops * mac_ops)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

void phylink_destroy(struct phylink * pl)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

void phylink_disconnect_phy(struct phylink * pl)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

void phylink_ethtool_get_pauseparam(struct phylink * pl,struct ethtool_pauseparam * pause)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

int phylink_ethtool_set_pauseparam(struct phylink * pl,struct ethtool_pauseparam * pause)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

void phylink_generic_validate(struct phylink_config * config,unsigned long * supported,struct phylink_link_state * state)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

void phylink_resume(struct phylink * pl)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

void phylink_start(struct phylink * pl)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

void phylink_stop(struct phylink * pl)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylink.h>

void phylink_suspend(struct phylink * pl,bool mac_wol)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/refcount.h>

void refcount_warn_saturate(refcount_t * r,enum refcount_saturation_type t)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

void rtmsg_ifinfo_send(struct sk_buff * skb,struct net_device * dev,gfp_t flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sk_error_report(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct sk_buff * skb_mac_gso_segment(struct sk_buff * skb,netdev_features_t features)
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


#include <net/sock.h>

int sysctl_tstamp_allow_data;


#include <linux/tcp.h>

struct sk_buff * tcp_get_timestamping_opt_stats(const struct sock * sk,const struct sk_buff * orig_skb,const struct sk_buff * ack_skb)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_devio_cleanup(void);
void usb_devio_cleanup(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb.h>

int usb_free_streams(struct usb_interface * interface,struct usb_host_endpoint ** eps,unsigned int num_eps,gfp_t mem_flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/hcd.h>

int usb_hcd_find_raw_port_number(struct usb_hcd * hcd,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


extern int usb_hub_create_port_device(struct usb_hub * hub,int port1);
int usb_hub_create_port_device(struct usb_hub * hub,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_hub_remove_port_device(struct usb_hub * hub,int port1);
void usb_hub_remove_port_device(struct usb_hub * hub,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_major_cleanup(void);
void usb_major_cleanup(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/of.h>

struct device_node * usb_of_get_device_node(struct usb_device * hub,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/of.h>

struct device_node * usb_of_get_interface_node(struct usb_device * udev,u8 config,u8 ifnum)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/ch9.h>

const char * usb_speed_string(enum usb_device_speed speed)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 uuid_index[16] = {};


#include <linux/vmstat.h>

atomic_long_t vm_node_stat[NR_VM_NODE_STAT_ITEMS] = {};


#include <linux/sched/wake_q.h>

void wake_q_add_safe(struct wake_q_head * head,struct task_struct * task)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcupdate_wait.h>

void wakeme_after_rcu(struct rcu_head * head)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

int xdp_do_generic_redirect(struct net_device * dev,struct sk_buff * skb,struct xdp_buff * xdp,struct bpf_prog * xdp_prog)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

u32 xdp_master_redirect(struct xdp_buff * xdp)
{
	lx_emul_trace_and_stop(__func__);
}

