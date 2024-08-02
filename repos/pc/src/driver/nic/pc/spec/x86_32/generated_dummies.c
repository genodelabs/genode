/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2024-08-20
 */

#include <lx_emul.h>


#include <linux/ratelimit_types.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpumask.h>

struct cpumask __cpu_active_mask;


#include <linux/ethtool.h>

int __ethtool_get_link_ksettings(struct net_device * dev,struct ethtool_link_ksettings * link_ksettings)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/netlink.h>

int __nla_parse(struct nlattr ** tb,int maxtype,const struct nlattr * head,int len,const struct nla_policy * policy,unsigned int validate,struct netlink_ext_ack * extack)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

struct nlmsghdr * __nlmsg_put(struct sk_buff * skb,u32 portid,u32 seq,int type,int len,int flags)
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


#include <linux/mm.h>

void __show_mem(unsigned int filter,nodemask_t * nodemask,int max_zone_idx)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void __skb_get_hash(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gso.h>

struct sk_buff * __skb_gso_segment(struct sk_buff * skb,netdev_features_t features,bool tx_path)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/srcu.h>

void __srcu_read_unlock(struct srcu_struct * ssp,int idx)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int _printk_deferred(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/acpi.h>

void acpi_device_notify_remove(struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int add_uevent_var(struct kobj_uevent_env * env,const char * format,...)
{
	lx_emul_trace_and_stop(__func__);
}


extern void arch_trigger_cpumask_backtrace(const cpumask_t * mask,int exclude_cpu);
void arch_trigger_cpumask_backtrace(const cpumask_t * mask,int exclude_cpu)
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


#include <linux/kernel.h>

void bust_spinlocks(int yes)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/bitrev.h>

u8 const byte_rev_table[256] = {};


#include <linux/console.h>

void console_flush_on_panic(enum con_flush_mode mode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/console.h>

void console_unblank(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void console_verbose(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/arch_topology.h>

const struct cpumask * cpu_clustergroup_mask(int cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pm_qos.h>

void cpu_latency_qos_remove_request(struct pm_qos_request * req)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpumask.h>

unsigned int cpumask_any_and_distribute(const struct cpumask * src1p,const struct cpumask * src2p)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/topology.h>

bool cpus_share_cache(int this_cpu,int that_cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

int device_create_managed_software_node(struct device * dev,const struct property_entry * properties,const struct software_node * parent)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

void do_trace_netlink_extack(const char * msg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack_lvl(const char * log_lvl)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

void emergency_restart(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool_netlink.h>

int ethnl_cable_test_alloc(struct phy_device * phydev,u8 cmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool_netlink.h>

void ethnl_cable_test_finished(struct phy_device * phydev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool_netlink.h>

void ethnl_cable_test_free(struct phy_device * phydev)
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


#include <linux/sched.h>

struct task_struct * find_task_by_vpid(pid_t vnr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcuwait.h>

void finish_rcuwait(struct rcuwait * w)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

void gen_kill_estimator(struct net_rate_estimator __rcu ** rate_est)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mii.h>

int generic_mii_ioctl(struct mii_if_info * mii_if,struct mii_ioctl_data * mii_data,int cmd,unsigned int * duplex_chg_out)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

bool gfp_pfmemalloc_allowed(gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

void gnet_stats_add_basic(struct gnet_stats_basic_sync * bstats,struct gnet_stats_basic_sync __percpu * cpu,struct gnet_stats_basic_sync * b,bool running)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

void gnet_stats_add_queue(struct gnet_stats_queue * qstats,const struct gnet_stats_queue __percpu * cpu,const struct gnet_stats_queue * q)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

int gnet_stats_copy_basic(struct gnet_dump * d,struct gnet_stats_basic_sync __percpu * cpu,struct gnet_stats_basic_sync * b,bool running)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

int gnet_stats_copy_queue(struct gnet_dump * d,struct gnet_stats_queue __percpu * cpu_q,struct gnet_stats_queue * q,__u32 qlen)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gpio/consumer.h>

int gpiod_get_value_cansleep(const struct gpio_desc * desc)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gpio/consumer.h>

void gpiod_put(struct gpio_desc * desc)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gpio/consumer.h>

int gpiod_set_consumer_name(struct gpio_desc * desc,const char * name)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gpio/consumer.h>

void gpiod_set_value_cansleep(struct gpio_desc * desc,int value)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void * high_memory;


#include <linux/pseudo_fs.h>

struct pseudo_fs_context * init_pseudo(struct fs_context * fc,unsigned long magic)
{
	lx_emul_trace_and_stop(__func__);
}


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


#include <linux/irq_work.h>

void irq_work_tick(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

struct kobject *kernel_kobj;


#include <linux/fs.h>

void kill_anon_super(struct super_block * sb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kmsg_dump.h>

void kmsg_dump(enum kmsg_dump_reason reason)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int kobject_synth_uevent(struct kobject * kobj,const char * buf,size_t count)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct pernet_operations __net_initdata loopback_net_ops;


#include <linux/delay.h>

unsigned long loops_per_jiffy;


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


#include <linux/mii.h>

void mii_ethtool_get_link_ksettings(struct mii_if_info * mii,struct ethtool_link_ksettings * cmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mii.h>

void mii_ethtool_gset(struct mii_if_info * mii,struct ethtool_cmd * ecmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mii.h>

int mii_ethtool_set_link_ksettings(struct mii_if_info * mii,const struct ethtool_link_ksettings * cmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mii.h>

int mii_ethtool_sset(struct mii_if_info * mii,struct ethtool_cmd * ecmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mii.h>

int mii_link_ok(struct mii_if_info * mii)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mii.h>

int mii_nway_restart(struct mii_if_info * mii)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

bool netlink_strict_get_check(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/netlink.h>

int nla_put(struct sk_buff * skb,int attrtype,int attrlen,const void * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq.h>

struct irq_chip no_irq_chip;


#include <linux/irq.h>

void note_interrupt(struct irq_desc * desc,irqreturn_t action_ret)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

enum reboot_mode panic_reboot_mode;


#include <linux/moduleparam.h>

const struct kernel_param_ops param_ops_int;


#include <linux/pci.h>

void pci_clear_mwi(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_prepare_to_sleep(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

void pci_release_selected_regions(struct pci_dev * pdev,int bars)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_reset_bus(struct pci_dev * pdev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_set_power_state(struct pci_dev * dev,pci_power_t state)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_status_get_and_clear_errors(struct pci_dev * pdev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_wake_from_d3(struct pci_dev * dev,bool enable)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_write_config_byte(const struct pci_dev * dev,int where,u8 val)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pcix_get_mmrbc(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pcix_set_mmrbc(struct pci_dev * dev,int mmrbc)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phylib_stubs.h>

const struct phylib_stubs *phylib_stubs;


#include <linux/interrupt.h>

int probe_irq_off(unsigned long val)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

unsigned long probe_irq_on(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sysctl.h>

int proc_dointvec_minmax(struct ctl_table * table,int write,void * buffer,size_t * lenp,loff_t * ppos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sysctl.h>

int proc_douintvec(struct ctl_table * table,int write,void * buffer,size_t * lenp,loff_t * ppos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/proc_ns.h>

void proc_free_inum(unsigned int inum)
{
	lx_emul_trace_and_stop(__func__);
}


extern void raw_spin_rq_lock_nested(struct rq * rq,int subclass);
void raw_spin_rq_lock_nested(struct rq * rq,int subclass)
{
	lx_emul_trace_and_stop(__func__);
}


extern void raw_spin_rq_unlock(struct rq * rq);
void raw_spin_rq_unlock(struct rq * rq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcuref.h>

bool rcuref_get_slowpath(rcuref_t * ref)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

enum reboot_mode reboot_mode;


#include <linux/firmware.h>

void release_firmware(const struct firmware * fw)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rhashtable.h>

struct rhash_lock_head __rcu ** rht_bucket_nested(const struct bucket_table * tbl,unsigned int hash)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

struct sk_buff * rtmsg_ifinfo_build_skb(int type,struct net_device * dev,unsigned int change,u32 event,gfp_t flags,int * new_nsid,int new_ifindex,u32 portid,const struct nlmsghdr * nlh)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

void rtmsg_ifinfo_send(struct sk_buff * skb,struct net_device * dev,gfp_t flags,u32 portid,const struct nlmsghdr * nlh)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/rtnetlink.h>

struct net * rtnl_get_net_ns_capable(struct sock * sk,int netnsid)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_notify(struct sk_buff * skb,struct net * net,u32 pid,u32 group,const struct nlmsghdr * nlh,gfp_t flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_set_sk_err(struct net * net,u32 group,int error)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

int rtnl_trylock(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

int rtnl_unicast(struct sk_buff * skb,struct net * net,u32 pid)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

void seq_printf(struct seq_file * m,const char * f,...)
{
	lx_emul_trace_and_stop(__func__);
}


extern void set_rq_offline(struct rq * rq);
void set_rq_offline(struct rq * rq)
{
	lx_emul_trace_and_stop(__func__);
}


extern void set_rq_online(struct rq * rq);
void set_rq_online(struct rq * rq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

unsigned int setup_max_cpus;


#include <linux/sched/debug.h>

void show_state_filter(unsigned int state_filter)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sk_error_report(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

void smp_call_function_many(const struct cpumask * mask,smp_call_func_t func,void * info,bool wait)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

int smp_call_function_single(int cpu,smp_call_func_t func,void * info,int wait)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

int smp_call_function_single_async(int cpu,struct __call_single_data * csd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sock_efree(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


extern void software_node_notify_remove(struct device * dev);
void software_node_notify_remove(struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jump_label.h>

bool static_key_initialized;


#include <linux/printk.h>

int suppress_printk;


#include <net/sock.h>

int sysctl_tstamp_allow_data;


#include <linux/sysctl.h>

const int sysctl_vals[] = {};


#include <linux/task_work.h>

int task_work_add(struct task_struct * task,struct callback_head * work,enum task_work_notify_mode notify)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/task_work.h>

struct callback_head * task_work_cancel_func(struct task_struct * task,task_work_func_t func)
{
	lx_emul_trace_and_stop(__func__);
}


extern void unregister_handler_proc(unsigned int irq,struct irqaction * action);
void unregister_handler_proc(unsigned int irq,struct irqaction * action)
{
	lx_emul_trace_and_stop(__func__);
}


extern void update_group_capacity(struct sched_domain * sd,int cpu);
void update_group_capacity(struct sched_domain * sd,int cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/wake_q.h>

void wake_q_add_safe(struct wake_q_head * head,struct task_struct * task)
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

