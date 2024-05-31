/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Sebastian Sumpf
 * \date   2023-07-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>


DEFINE_STATIC_KEY_FALSE(force_irqthreads_key);
DEFINE_STATIC_KEY_FALSE(bpf_stats_enabled_key);
DEFINE_STATIC_KEY_FALSE(bpf_master_redirect_enabled_key);
DEFINE_STATIC_KEY_FALSE(memalloc_socks_key);

DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);

unsigned long __FIXADDR_TOP = 0xfffff000;

bool arm64_use_ng_mappings = false;

const struct ipv6_stub *ipv6_stub;


#ifdef __i386__
asmlinkage __wsum csum_partial(const void * buff,int len,__wsum sum)
#else
__wsum csum_partial(const void * buff,int len,__wsum sum)
#endif
{
	lx_emul_trace_and_stop(__func__);
}


#ifdef __arm__
#include <asm/uaccess.h>

unsigned long arm_copy_to_user(void *to, const void *from, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}

asmlinkage void __div0(void);
asmlinkage void __div0(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-map-ops.h>

void arch_teardown_dma_ops(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern void arm_heavy_mb(void);
void arm_heavy_mb(void)
{
	// FIXME: on Cortex A9 we potentially need to flush L2-cache
	lx_emul_trace(__func__);
}

#else

#include <linux/timekeeper_internal.h>
void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}

#endif



#include <linux/filter.h>
#include <linux/jump_label.h> /* for DEFINE_STATIC_KEY_FALSE */

void bpf_prog_change_xdp(struct bpf_prog *prev_prog, struct bpf_prog *prog)
{
	lx_emul_trace(__func__);
}


#ifdef CONFIG_TREE_RCU
#include <linux/rcutree.h>

void synchronize_rcu_expedited(void)
{
	lx_emul_trace(__func__);
}
#endif


#include <linux/rcupdate.h>

void synchronize_rcu(void)
{
	lx_emul_trace(__func__);
}


#include <net/net_namespace.h>

void __init net_ns_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/kobject.h>

int kobject_uevent(struct kobject * kobj,enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/usb/hcd.h>

void __init usb_init_pool_max(void)
{
	lx_emul_trace(__func__);
}


extern int usb_major_init(void);
int usb_major_init(void)
{
	lx_emul_trace(__func__);
		return 0;
}


extern int __init usb_devio_init(void);
int __init usb_devio_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/usb/hcd.h>

void usb_hcd_synchronize_unlinks(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


#include <linux/usb/hcd.h>

struct usb_hcd * usb_get_hcd(struct usb_hcd * hcd)
{
	lx_emul_trace(__func__);
	return hcd;
}


#include <linux/usb/hcd.h>

void usb_put_hcd(struct usb_hcd * hcd)
{
	lx_emul_trace(__func__);
}


extern int __init netdev_kobject_init(void);
int __init netdev_kobject_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/netdevice.h>

void dev_add_offload(struct packet_offload * po)
{
	lx_emul_trace(__func__);
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/kernel.h>

bool parse_option_str(const char * str,const char * option)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/rtnetlink.h>

int rtnl_lock_killable(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/rtnetlink.h>

void rtnl_lock(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rtnetlink.h>

int rtnl_is_locked(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/rtnetlink.h>

struct sk_buff * rtmsg_ifinfo_build_skb(int type,struct net_device * dev,unsigned int change,u32 event,gfp_t flags,int * new_nsid,int new_ifindex)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <linux/stringhash.h>

unsigned int full_name_hash(const void * salt,const char * name,unsigned int len)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/random.h>

void add_device_randomness(const void * buf,size_t len)
{
	lx_emul_trace(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_unlock(void)
{
	lx_emul_trace(__func__);
}


#include <net/gen_stats.h>

void gnet_stats_basic_sync_init(struct gnet_stats_basic_sync * b)
{
	lx_emul_trace(__func__);
}


#include <net/gen_stats.h>

void gen_kill_estimator(struct net_rate_estimator __rcu ** rate_est)
{
	lx_emul_trace(__func__);
}


#include <asm-generic/softirq_stack.h>

void do_softirq_own_stack(void)
{
	lx_emul_trace(__func__);
}


#include <linux/of_net.h>
#if defined(CONFIG_OF) && defined(CONFIG_NET)
int of_get_mac_address(struct device_node * np,u8 * addr)
{
	lx_emul_trace(__func__);
	return -1;
}
#endif


#include <linux/irqdomain.h>

struct fwnode_handle * __irq_domain_alloc_fwnode(unsigned int type,int id,const char * name,phys_addr_t * pa)
{
	lx_emul_trace(__func__);
	return (struct fwnode_handle *)0x1;
}


struct irq_domain * __irq_domain_add(struct fwnode_handle * fwnode,unsigned int size,irq_hw_number_t hwirq_max,int direct_max,const struct irq_domain_ops * ops,void * host_data)
{
	lx_emul_trace(__func__);
	return (struct irq_domain *)0x1;
}


unsigned int irq_create_mapping_affinity(struct irq_domain * domain,irq_hw_number_t hwirq,const struct irq_affinity_desc * affinity)
{
	lx_emul_trace(__func__);
	return 1;
}


#include <linux/irq.h>

void irq_set_chip_and_handler_name(unsigned int irq,const struct irq_chip * chip,irq_flow_handler_t handle,const char * name)
{
	lx_emul_trace(__func__);
}


#include <linux/phy.h>

struct mii_bus * mdiobus_alloc_size(size_t size)
{
	static struct mii_bus _m;
	lx_emul_trace(__func__);
	return &_m;
}


int __mdiobus_register(struct mii_bus * bus,struct module * owner)
{
	lx_emul_trace(__func__);
	return 0;
}


struct phy_device * phy_find_first(struct mii_bus * bus)
{
	static struct phy_device _p;
	lx_emul_trace(__func__);
	return &_p;
}


int phy_connect_direct(struct net_device * dev,struct phy_device * phydev,void (* handler)(struct net_device *),phy_interface_t interface)
{
	lx_emul_trace(__func__);
	return 0;
}


void phy_attached_info(struct phy_device * phydev)
{
	lx_emul_trace(__func__);
}


void phy_start(struct phy_device * phydev)
{
	lx_emul_trace(__func__);
}


extern void software_node_notify(struct device * dev);
void software_node_notify(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern void software_node_notify_remove(struct device * dev);
void software_node_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern int usb_create_sysfs_dev_files(struct usb_device * udev);
int usb_create_sysfs_dev_files(struct usb_device * udev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void usb_remove_sysfs_dev_files(struct usb_device * udev);
void usb_remove_sysfs_dev_files(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


extern void usb_remove_sysfs_intf_files(struct usb_interface * intf);
void usb_remove_sysfs_intf_files(struct usb_interface * intf)
{
	lx_emul_trace(__func__);
}


extern void usb_create_sysfs_intf_files(struct usb_interface * intf);
void usb_create_sysfs_intf_files(struct usb_interface * intf)
{
	lx_emul_trace(__func__);
}


extern void usb_notify_add_device(struct usb_device * udev);
void usb_notify_add_device(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


extern void usb_notify_remove_device(struct usb_device * udev);
void usb_notify_remove_device(struct usb_device * udev)
{
	lx_emul_trace(__func__);
}


extern int usb_create_ep_devs(struct device * parent,struct usb_host_endpoint * endpoint,struct usb_device * udev);
int usb_create_ep_devs(struct device * parent,struct usb_host_endpoint * endpoint,struct usb_device * udev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void usb_remove_ep_devs(struct usb_host_endpoint * endpoint);
void usb_remove_ep_devs(struct usb_host_endpoint * endpoint)
{
	lx_emul_trace(__func__);
}


extern void netdev_unregister_kobject(struct net_device * ndev);
void netdev_unregister_kobject(struct net_device * ndev)
{
	lx_emul_trace(__func__);
}


void usb_hcd_flush_endpoint(struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	lx_emul_trace(__func__);
}


void usb_hcd_disable_endpoint(struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	lx_emul_trace(__func__);
}


void usb_hcd_reset_endpoint(struct usb_device *udev,
			    struct usb_host_endpoint *ep)
{
	lx_emul_trace(__func__);
}

const struct attribute_group *usb_interface_groups[] = { NULL };


#include <linux/usb/hcd.h>

int usb_hcd_alloc_bandwidth(struct usb_device * udev,struct usb_host_config * new_config,struct usb_host_interface * cur_alt,struct usb_host_interface * new_alt)
{
	lx_emul_trace(__func__);
	return 0;
}


#if defined(CONFIG_OF)
#include <linux/usb/of.h>

bool usb_of_has_combined_node(struct usb_device * udev)
{
	lx_emul_trace(__func__);
	return true;
}
#endif
