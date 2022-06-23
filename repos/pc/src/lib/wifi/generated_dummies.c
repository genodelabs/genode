/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2022-06-24
 */

#include <lx_emul.h>


#include <linux/proc_fs.h>

void * PDE_DATA(const struct inode * inode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ratelimit_types.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/clk-provider.h>

const char * __clk_get_name(const struct clk * clk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ethtool.h>

int __ethtool_get_link_ksettings(struct net_device * dev,struct ethtool_link_ksettings * link_ksettings)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

unsigned long __fdget(unsigned int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

int __get_unused_fd_flags(unsigned flags,unsigned long nofile)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

void __gnet_stats_copy_basic(const seqcount_t * running,struct gnet_stats_basic_packed * bstats,struct gnet_stats_basic_cpu __percpu * cpu,struct gnet_stats_basic_packed * b)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

void __gnet_stats_copy_queue(struct gnet_stats_queue * qstats,const struct gnet_stats_queue __percpu * cpu,const struct gnet_stats_queue * q,__u32 qlen)
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


#include <net/scm.h>

void __scm_destroy(struct scm_cookie * scm)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/scm.h>

int __scm_send(struct socket * sock,struct msghdr * msg,struct scm_cookie * p)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

u32 __skb_get_hash_symmetric(const struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sock_diag.h>

u64 __sock_gen_cookie(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/srcu.h>

void __srcu_read_unlock(struct srcu_struct * ssp,int idx)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void * __vmalloc_node(unsigned long size,unsigned long align,gfp_t gfp_mask,int node,const void * caller)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uaccess.h>

unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


extern void ack_bad_irq(unsigned int irq);
void ack_bad_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int add_uevent_var(struct kobj_uevent_env * env,const char * format,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

struct file * alloc_file_pseudo(struct inode * inode,struct vfsmount * mnt,const char * name,int flags,const struct file_operations * fops)
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/arc4.h>

void arc4_crypt(struct arc4_ctx * ctx,u8 * out,const u8 * in,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/arc4.h>

int arc4_setkey(struct arc4_ctx * ctx,const u8 * in_key,unsigned int key_len)
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

int bpf_prog_create_from_user(struct bpf_prog ** pfp,struct sock_fprog * fprog,bpf_aux_classic_check_t trans,bool save_orig)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

void bpf_prog_destroy(struct bpf_prog * fp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

void bpf_warn_invalid_xdp_action(u32 act)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

void bust_spinlocks(int yes)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/console.h>

void console_flush_on_panic(enum con_flush_mode mode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int console_printk[] = {};


#include <linux/console.h>

void console_unblank(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

int copy_bpf_fprog_from_user(struct sock_fprog * dst,sockptr_t src,int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uio.h>

size_t copy_page_from_iter(struct page * page,size_t offset,size_t bytes,struct iov_iter * i)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

int dev_ifconf(struct net * net,struct ifconf * ifc,int size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

int dev_ioctl(struct net * net,unsigned int cmd,struct ifreq * ifr,bool * need_copyout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

asmlinkage __visible void do_softirq(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst.h>

void dst_release(struct dst_entry * dst)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dcache.h>

char * dynamic_dname(struct dentry * dentry,char * buffer,int buflen,const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

void emergency_restart(void)
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


#include <linux/fs.h>

pid_t f_getown(struct file * filp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int f_setown(struct file * filp,unsigned long arg,int force)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int fasync_helper(int fd,struct file * filp,int on,struct fasync_struct ** fapp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

void fd_install(unsigned int fd,struct file * file)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

struct task_struct * find_task_by_vpid(pid_t vnr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/flow_dissector.h>

struct flow_dissector flow_keys_basic_dissector;


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

struct fwnode_handle * fwnode_create_software_node(const struct property_entry * properties,const struct fwnode_handle * parent)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

void fwnode_remove_software_node(struct fwnode_handle * fwnode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gcd.h>

unsigned long gcd(unsigned long a,unsigned long b)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

void gen_kill_estimator(struct net_rate_estimator __rcu ** rate_est)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

ssize_t generic_file_splice_read(struct file * in,loff_t * ppos,struct pipe_inode_info * pipe,size_t len,unsigned int flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

ssize_t generic_splice_sendpage(struct pipe_inode_info * pipe,struct file * out,loff_t * ppos,size_t len,unsigned int flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

int get_option(char ** str,int * pint)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

char * get_options(const char * str,int nints,int * ints)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

int get_unused_fd_flags(unsigned flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/gfp.h>

bool gfp_pfmemalloc_allowed(gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

int gnet_stats_copy_basic(const seqcount_t * running,struct gnet_dump * d,struct gnet_stats_basic_cpu __percpu * cpu,struct gnet_stats_basic_packed * b)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/gen_stats.h>

int gnet_stats_copy_queue(struct gnet_dump * d,struct gnet_stats_queue __percpu * cpu_q,struct gnet_stats_queue * q,__u32 qlen)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 guid_index[16] = {};


#include <linux/uio.h>

ssize_t import_iovec(int type,const struct iovec __user * uvec,unsigned nr_segs,unsigned fast_segs,struct iovec ** iovp,struct iov_iter * i)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uio.h>

int import_single_range(int rw,void __user * buf,size_t len,struct iovec * iov,struct iov_iter * i)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/init.h>

bool initcall_debug;


#include <linux/fs.h>

void inode_init_once(struct inode * inode)
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


#include <linux/uio.h>

void iov_iter_kvec(struct iov_iter * i,unsigned int direction,const struct kvec * kvec,unsigned long nr_segs,size_t count)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uio.h>

void iov_iter_revert(struct iov_iter * i,size_t unroll)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq_work.h>

void irq_work_tick(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

bool is_software_node(const struct fwnode_handle * fwnode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

struct kobject *kernel_kobj;


#include <linux/key.h>

key_ref_t key_create_or_update(key_ref_t keyring_ref,const char * type,const char * description,const void * payload,size_t plen,key_perm_t perm,unsigned long flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/key.h>

void key_put(struct key * key)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

void kill_anon_super(struct super_block * sb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

void kill_fasync(struct fasync_struct ** fp,int sig,int band)
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


#include <linux/kernel.h>

unsigned long long memparse(const char * ptr,char ** retptr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/neighbour.h>

const struct nla_policy nda_policy[] = {};


#include <linux/netdevice.h>

void netdev_rss_key_fill(void * buffer,size_t len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq.h>

struct irq_chip no_irq_chip;


#include <linux/fs.h>

loff_t no_llseek(struct file * file,loff_t offset,int whence)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq.h>

void note_interrupt(struct irq_desc * desc,irqreturn_t action_ret)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/proc_fs.h>

int open_related_ns(struct ns_common * ns,struct ns_common * (* get_ns)(struct ns_common * ns))
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

enum reboot_mode panic_reboot_mode;


#include <linux/pci.h>

void pci_assign_unassigned_bridge_resources(struct pci_dev * bridge)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

void pci_assign_unassigned_bus_resources(struct pci_bus * bus)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned long pci_cardbus_resource_alignment(struct resource * res);
unsigned long pci_cardbus_resource_alignment(struct resource * res)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

unsigned int pci_flags;


extern int pci_idt_bus_quirk(struct pci_bus * bus,int devfn,u32 * l,int timeout);
int pci_idt_bus_quirk(struct pci_bus * bus,int devfn,u32 * l,int timeout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_mmap_resource_range(struct pci_dev * pdev,int bar,struct vm_area_struct * vma,enum pci_mmap_state mmap_state,int write_combine)
{
	lx_emul_trace_and_stop(__func__);
}


extern void __init pci_realloc_get_opt(char * str);
void __init pci_realloc_get_opt(char * str)
{
	lx_emul_trace_and_stop(__func__);
}


extern void pci_restore_vc_state(struct pci_dev * dev);
void pci_restore_vc_state(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


extern int pci_save_vc_state(struct pci_dev * dev);
int pci_save_vc_state(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

void pci_stop_and_remove_bus_device(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

void pci_stop_and_remove_bus_device_locked(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


extern void pci_vpd_release(struct pci_dev * dev);
void pci_vpd_release(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned int pcibios_assign_all_busses(void);
unsigned int pcibios_assign_all_busses(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern void pcie_aspm_init_link_state(struct pci_dev * pdev);
void pcie_aspm_init_link_state(struct pci_dev * pdev)
{
	lx_emul_trace_and_stop(__func__);
}


extern void pcie_aspm_pm_state_change(struct pci_dev * pdev);
void pcie_aspm_pm_state_change(struct pci_dev * pdev)
{
	lx_emul_trace_and_stop(__func__);
}


extern void pcie_aspm_powersave_config_link(struct pci_dev * pdev);
void pcie_aspm_powersave_config_link(struct pci_dev * pdev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/poll.h>

int poll_select_set_timeout(struct timespec64 * to,time64_t sec,long nsec)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int printk_deferred(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void printk_safe_flush_on_panic(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/proc_ns.h>

void proc_free_inum(unsigned int inum)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/socket.h>

int put_cmsg(struct msghdr * msg,int level,int type,int len,void * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/socket.h>

void put_cmsg_scm_timestamping(struct msghdr * msg,struct scm_timestamping_internal * tss_internal)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/socket.h>

void put_cmsg_scm_timestamping64(struct msghdr * msg,struct scm_timestamping_internal * tss_internal)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/file.h>

void put_unused_fd(unsigned int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int raw_pci_read(unsigned int domain,unsigned int bus,unsigned int devfn,int reg,int len,u32 * val)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

enum reboot_mode reboot_mode;


#include <linux/proc_fs.h>

void remove_proc_entry(const char * name,struct proc_dir_entry * parent)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock_reuseport.h>

int reuseport_detach_prog(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock_reuseport.h>

void reuseport_detach_sock(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/scm.h>

void scm_detach_fds(struct msghdr * msg,struct scm_cookie * scm)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

struct hlist_node * seq_hlist_next_rcu(void * v,struct hlist_head * head,loff_t * ppos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

struct hlist_node * seq_hlist_start_head_rcu(struct hlist_head * head,loff_t pos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

struct list_head * seq_list_next(void * v,struct list_head * head,loff_t * ppos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

struct list_head * seq_list_start(struct list_head * head,loff_t pos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

struct list_head * seq_list_start_head(struct list_head * head,loff_t pos)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

void seq_printf(struct seq_file * m,const char * f,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

void seq_putc(struct seq_file * m,char c)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

void seq_puts(struct seq_file * m,const char * s)
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/sha2.h>

void sha224_final(struct sha256_state * sctx,u8 * out)
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/sha2.h>

void sha256_final(struct sha256_state * sctx,u8 * out)
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/sha2.h>

void sha256_update(struct sha256_state * sctx,const u8 * data,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void show_mem(unsigned int filter,nodemask_t * nodemask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/debug.h>

void show_state_filter(unsigned int state_filter)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int simple_setattr(struct user_namespace * mnt_userns,struct dentry * dentry,struct iattr * iattr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int simple_statfs(struct dentry * dentry,struct kstatfs * buf)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

int sk_attach_bpf(u32 ufd,struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

int sk_attach_filter(struct sock_fprog * fprog,struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

int sk_detach_filter(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

void sk_filter_uncharge(struct sock * sk,struct sk_filter * fp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

int sk_get_filter(struct sock * sk,struct sock_filter __user * ubuf,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

int sk_reuseport_attach_bpf(u32 ufd,struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/filter.h>

int sk_reuseport_attach_filter(struct sock_fprog * fprog,struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

int smp_call_function_single(int cpu,void (* func)(void * info),void * info,int wait)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sock_diag.h>

void sock_diag_broadcast_destroy(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/srcutiny.h>

void srcu_drive_gp(struct work_struct * wp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jump_label.h>

bool static_key_initialized;


#include <linux/fs.h>

int stream_open(struct inode * inode,struct file * filp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/string_helpers.h>

int string_escape_mem(const char * src,size_t isz,char * dst,size_t osz,unsigned int flags,const char * only)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int suppress_printk;


#include <linux/srcutiny.h>

void synchronize_srcu(struct srcu_struct * ssp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sysfs.h>

int sysfs_rename_dir_ns(struct kobject * kobj,const char * new_name,const void * new_ns)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sysfs.h>

int sysfs_rename_link_ns(struct kobject * kobj,struct kobject * targ,const char * old,const char * new,const void * new_ns)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/task_work.h>

struct callback_head * task_work_cancel(struct task_struct * task,task_work_func_t func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

void tasklet_kill(struct tasklet_struct * t)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vt_kern.h>

void unblank_screen(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int unregister_filesystem(struct file_system_type * fs)
{
	lx_emul_trace_and_stop(__func__);
}


extern void unregister_handler_proc(unsigned int irq,struct irqaction * action);
void unregister_handler_proc(unsigned int irq,struct irqaction * action)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 uuid_index[16] = {};


#include <linux/mm.h>

int vm_insert_page(struct vm_area_struct * vma,unsigned long addr,struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

struct page * vmalloc_to_page(const void * vmalloc_addr)
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


#include <linux/sched.h>

void __sched yield(void)
{
	lx_emul_trace_and_stop(__func__);
}

