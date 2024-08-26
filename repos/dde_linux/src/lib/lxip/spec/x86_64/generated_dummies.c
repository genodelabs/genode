/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2024-07-30
 */

#include <lx_emul.h>


#include <linux/root_dev.h>

dev_t ROOT_DEV;


#include <linux/filter.h>

noinline u64 __bpf_call_base(u64 r1,u64 r2,u64 r3,u64 r4,u64 r5)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

void __bpf_prog_free(struct bpf_prog * fp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpumask.h>

struct cpumask __cpu_active_mask;


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


#include <linux/cred.h>

void __put_cred(struct cred * cred)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/task.h>

void __put_task_struct(struct task_struct * tsk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

int __receive_fd(struct file * file,int __user * ufd,unsigned int o_flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void __sock_recv_cmsgs(struct msghdr * msg,struct sock * sk,struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void __sock_recv_timestamp(struct msghdr * msg,struct sock * sk,struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void __sock_tx_timestamp(__u16 tsflags,__u8 * tx_flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

pid_t __task_pid_nr_ns(struct task_struct * task,enum pid_type type,struct pid_namespace * ns)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void * __vmalloc(unsigned long size,gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int _printk_deferred(const char * fmt,...)
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


#include <linux/bpf.h>

const struct bpf_func_proto bpf_get_current_pid_tgid_proto;


#include <linux/bpf.h>

const struct bpf_func_proto bpf_get_current_uid_gid_proto;


#include <linux/bpf.h>

const struct bpf_func_proto bpf_get_smp_processor_id_proto;


#include <linux/filter.h>

void * bpf_internal_load_pointer_neg_helper(const struct sk_buff * skb,int k,unsigned int size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/bpf.h>

const struct bpf_func_proto bpf_ktime_get_coarse_ns_proto;


#include <linux/filter.h>

struct bpf_prog * bpf_prog_alloc(unsigned int size,gfp_t gfp_extra_flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

void bpf_prog_free(struct bpf_prog * fp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

struct bpf_prog * bpf_prog_realloc(struct bpf_prog * fp_old,unsigned int size,gfp_t gfp_extra_flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

struct bpf_prog * bpf_prog_select_runtime(struct bpf_prog * fp,int * err)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/bpf.h>

void bpf_user_rnd_init_once(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uaccess.h>

int check_zeroed_user(const void __user * from,size_t size)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned long __must_check copy_mc_to_kernel(void * dst,const void * src,unsigned len);
unsigned long __must_check copy_mc_to_kernel(void * dst,const void * src,unsigned len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/arch_topology.h>

const struct cpumask * cpu_clustergroup_mask(int cpu)
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


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_enter(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_exit(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/device.h>

const char * dev_driver_string(const struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dev_printk.h>

int dev_printk_emit(int level,const struct device * dev,const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/device.h>

int device_rename(struct device * dev,const char * new_name)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool.h>

int ethtool_get_phc_vclocks(struct net_device * dev,int ** vclock_index)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool.h>

int ethtool_op_get_ts_info(struct net_device * dev,struct ethtool_ts_info * info)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

void fd_install(unsigned int fd,struct file * file)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

struct file * fget_raw(unsigned int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/capability.h>

bool file_ns_capable(const struct file * file,struct user_namespace * ns,int cap)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pid.h>

struct pid * find_get_pid(pid_t nr)
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


#include <linux/file.h>

void fput(struct file * file)
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


#include <linux/netdevice.h>

int get_user_ifreq(struct ifreq * ifr,void __user ** ifrdata,void __user * arg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int get_user_pages_fast(unsigned long start,int nr_pages,unsigned int gup_flags,struct page ** pages)
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


#include <net/protocol.h>

const struct net_offload __rcu *inet6_offloads[MAX_INET_PROTOS] = {};


#include <linux/utsname.h>

struct user_namespace init_user_ns;


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


#include <linux/interrupt.h>

int irq_can_set_affinity(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

int irq_set_affinity(unsigned int irq,const struct cpumask * cpumask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int is_vmalloc_or_module_addr(const void * x)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/llist.h>

bool llist_add_batch(struct llist_node * new_first,struct llist_node * new_last,struct llist_head * head)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

struct vm_area_struct * lock_vma_under_rcu(struct mm_struct * mm,unsigned long address)
{
	lx_emul_trace_and_stop(__func__);
}


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


#include <linux/maple_tree.h>

void * mtree_load(struct maple_tree * mt,unsigned long index)
{
	lx_emul_trace_and_stop(__func__);
}


extern void netdev_unregister_kobject(struct net_device * ndev);
void netdev_unregister_kobject(struct net_device * ndev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pipe_fs_i.h>

const struct pipe_buf_operations nosteal_pipe_buf_ops;


#include <linux/highuid.h>

int overflowuid;


#include <linux/panic.h>

void panic(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pid.h>

pid_t pid_vnr(struct pid * pid)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pid.h>

int pidfd_prepare(struct pid * pid,unsigned int flags,struct file ** ret)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int pin_user_pages_fast(unsigned long start,int nr_pages,unsigned int gup_flags,struct page ** pages)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/device.h>

void put_device(struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

void put_unused_fd(unsigned int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

int put_user_ifreq(struct ifreq * ifr,void __user * arg)
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


#include <linux/refcount.h>

bool refcount_dec_if_one(refcount_t * r)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/refcount.h>

void refcount_warn_saturate(refcount_t * r,enum refcount_saturation_type t)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int send_sigurg(struct fown_struct * fown)
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


#include <linux/net.h>

struct socket * sock_from_file(struct file * file)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

bool sock_is_registered(int family)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

int sock_wake_async(struct socket_wq * wq,int how,int band)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/splice.h>

ssize_t splice_to_pipe(struct pipe_inode_info * pipe,struct splice_pipe_desc * spd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jump_label.h>

bool static_key_initialized;


#include <linux/pid_namespace.h>

struct pid_namespace * task_active_pid_ns(struct task_struct * tsk)
{
	lx_emul_trace_and_stop(__func__);
}


extern void update_group_capacity(struct sched_domain * sd,int cpu);
void update_group_capacity(struct sched_domain * sd,int cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 uuid_index[16] = {};


#include <linux/vmalloc.h>

void vfree_atomic(const void * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int vm_insert_pages(struct vm_area_struct * vma,unsigned long addr,struct page ** pages,unsigned long * num)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

struct page * vmalloc_to_page(const void * vmalloc_addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/device/driver.h>

void __init wait_for_init_devices_probe(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

void __sched yield(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void zap_page_range_single(struct vm_area_struct * vma,unsigned long address,unsigned long size,struct zap_details * details)
{
	lx_emul_trace_and_stop(__func__);
}

