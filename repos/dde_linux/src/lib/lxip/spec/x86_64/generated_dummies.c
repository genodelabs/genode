/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2024-07-26
 */

#include <lx_emul.h>


extern unsigned long KSTK_ESP(struct task_struct * task);
unsigned long KSTK_ESP(struct task_struct * task)
{
	lx_emul_trace_and_stop(__func__);
}


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


#include <linux/sched.h>

char * __get_task_comm(char * buf,size_t buf_size,struct task_struct * tsk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ipv6.h>

int __ipv6_addr_type(const struct in6_addr * addr)
{
	lx_emul_trace_and_stop(__func__);
}


extern void __memcpy_flushcache(void * _dst,const void * _src,size_t size);
void __memcpy_flushcache(void * _dst,const void * _src,size_t size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int __mm_populate(unsigned long start,unsigned long len,int ignore_errors)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/mm.h>

void __mmdrop(struct mm_struct * mm)
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

void __sock_recv_wifi_status(struct msghdr * msg,struct sock * sk,struct sk_buff * skb)
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


#include <linux/mm.h>

int access_process_vm(struct task_struct * tsk,unsigned long addr,void * buf,int len,unsigned int gup_flags)
{
	lx_emul_trace_and_stop(__func__);
}


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


#include <linux/if_bridge.h>

int br_ioctl_call(struct net * net,struct net_bridge * br,unsigned int cmd,struct ifreq * ifr,void __user * uarg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/security.h>

int cap_settime(const struct timespec64 * ts,const struct timezone * tz)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uaccess.h>

int check_zeroed_user(const void __user * from,size_t size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uaccess.h>

long copy_from_kernel_nofault(void * dst,const void * src,size_t size)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned long __must_check copy_mc_to_kernel(void * dst,const void * src,unsigned len);
unsigned long __must_check copy_mc_to_kernel(void * dst,const void * src,unsigned len)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned long __must_check copy_mc_to_user(void * dst,const void * src,unsigned len);
unsigned long __must_check copy_mc_to_user(void * dst,const void * src,unsigned len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/arch_topology.h>

const struct cpumask * cpu_clustergroup_mask(int cpu)
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


#include <linux/splice.h>

const struct pipe_buf_operations default_pipe_buf_ops;


#include <linux/device.h>

const char * dev_driver_string(const struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

int dev_ethtool(struct net * net,struct ifreq * ifr,void __user * useraddr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

struct fwnode_handle * dev_fwnode(const struct device * dev)
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


#include <linux/device.h>

int devm_add_action(struct device * dev,void (* action)(void *),void * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

unsigned long do_mmap(struct file * file,unsigned long addr,unsigned long len,unsigned long prot,unsigned long flags,unsigned long pgoff,unsigned long * populate,struct list_head * uf)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/restart_block.h>

long do_no_restart_syscall(struct restart_block * param)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern void enter_lazy_tlb(struct mm_struct * mm,struct task_struct * tsk);
void enter_lazy_tlb(struct mm_struct * mm,struct task_struct * tsk)
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


#include <linux/pagemap.h>

size_t fault_in_readable(const char __user * uaddr,size_t size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pagemap.h>

size_t fault_in_safe_writeable(const char __user * uaddr,size_t size)
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


#include <linux/mmzone.h>

struct pglist_data * first_online_pgdat(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/task.h>

struct task_struct * __init fork_idle(int cpu)
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


#include <linux/property.h>

int fwnode_property_read_u8_array(const struct fwnode_handle * fwnode,const char * propname,u8 * val,size_t nval)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/device.h>

struct device * get_device(struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

int get_option(char ** str,int * pint)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcutree.h>

unsigned long get_state_synchronize_rcu(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/mm.h>

struct mm_struct * get_task_mm(struct task_struct * task)
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


#include <linux/pid_namespace.h>

struct pid_namespace init_pid_ns;


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


#include <linux/net.h>

int kernel_bind(struct socket * sock,struct sockaddr * addr,int addrlen)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

int kernel_connect(struct socket * sock,struct sockaddr * addr,int addrlen,int flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

int kernel_sendmsg(struct socket * sock,struct msghdr * msg,struct kvec * vec,size_t num,size_t size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

int kernel_sendmsg_locked(struct sock * sk,struct msghdr * msg,struct kvec * vec,size_t num,size_t size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

int kernel_sendpage(struct socket * sock,struct page * page,int offset,size_t size,int flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

int kernel_sendpage_locked(struct sock * sk,struct page * page,int offset,size_t size,int flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/net.h>

int kernel_sock_shutdown(struct socket * sock,enum sock_shutdown_cmd how)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int kobject_uevent_env(struct kobject * kobj,enum kobject_action action,char * envp_ext[])
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/llist.h>

bool llist_add_batch(struct llist_node * new_first,struct llist_node * new_last,struct llist_head * head)
{
	lx_emul_trace_and_stop(__func__);
}


extern void __init memblock_free_pages(struct page * page,unsigned long pfn,unsigned int order);
void __init memblock_free_pages(struct page * page,unsigned long pfn,unsigned int order)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/string.h>

ssize_t memory_read_from_buffer(void * to,size_t count,loff_t * ppos,const void * from,size_t available)
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


#include <linux/mman.h>

void mm_compute_batch(int overcommit_policy)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/mm.h>

void mmput(struct mm_struct * mm)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/maple_tree.h>

void * mtree_load(struct maple_tree * mt,unsigned long index)
{
	lx_emul_trace_and_stop(__func__);
}


extern int netdev_queue_update_kobjects(struct net_device * dev,int old_num,int new_num);
int netdev_queue_update_kobjects(struct net_device * dev,int old_num,int new_num)
{
	lx_emul_trace_and_stop(__func__);
}


extern void netdev_unregister_kobject(struct net_device * ndev);
void netdev_unregister_kobject(struct net_device * ndev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mmzone.h>

struct pglist_data * next_online_pgdat(struct pglist_data * pgdat)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pipe_fs_i.h>

const struct pipe_buf_operations nosteal_pipe_buf_ops;


#include <linux/highuid.h>

int overflowuid;


#include <linux/splice.h>

const struct pipe_buf_operations page_cache_pipe_buf_ops;


#include <linux/panic.h>

void panic(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/phy.h>

int phy_loopback(struct phy_device * phydev,bool enable)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pid.h>

pid_t pid_vnr(struct pid * pid)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcutree.h>

bool poll_state_synchronize_rcu(unsigned long oldstate)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sysctl.h>

int proc_dointvec(struct ctl_table * table,int write,void * buffer,size_t * lenp,loff_t * ppos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sysctl.h>

int proc_dointvec_minmax(struct ctl_table * table,int write,void * buffer,size_t * lenp,loff_t * ppos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sysctl.h>

int proc_doulongvec_minmax(struct ctl_table * table,int write,void * buffer,size_t * lenp,loff_t * ppos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/device.h>

void put_device(struct device * dev)
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

bool refcount_dec_and_mutex_lock(refcount_t * r,struct mutex * lock)
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


#include <linux/mm.h>

void __meminit reserve_bootmem_region(phys_addr_t start,phys_addr_t end)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int send_sigurg(struct fown_struct * fown)
{
	lx_emul_trace_and_stop(__func__);
}


extern void set_cpus_allowed_common(struct task_struct * p,const struct cpumask * new_mask,u32 flags);
void set_cpus_allowed_common(struct task_struct * p,const struct cpumask * new_mask,u32 flags)
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


#include <linux/rcutree.h>

unsigned long start_poll_synchronize_rcu(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jump_label.h>

bool static_key_initialized;


extern void switch_mm_irqs_off(struct mm_struct * prev,struct mm_struct * next,struct task_struct * tsk);
void switch_mm_irqs_off(struct mm_struct * prev,struct mm_struct * next,struct task_struct * tsk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void sync_mm_rss(struct mm_struct * mm)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/device/class.h>

struct kobject *sysfs_dev_char_kobj;


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


#include <linux/timekeeper_internal.h>

void update_vsyscall_tz(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 uuid_index[16] = {};


#include <linux/mman.h>

s32 vm_committed_as_batch;


#include <linux/mm.h>

int vm_insert_pages(struct vm_area_struct * vma,unsigned long addr,struct page ** pages,unsigned long * num)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

bool vmalloc_dump_obj(void * object)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void * vmalloc_huge(unsigned long size,gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/device/driver.h>

void __init wait_for_init_devices_probe(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/wake_q.h>

void wake_q_add_safe(struct wake_q_head * head,struct task_struct * task)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

void __sched yield(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void zap_page_range(struct vm_area_struct * vma,unsigned long start,unsigned long size)
{
	lx_emul_trace_and_stop(__func__);
}

