/*
 * \brief  Dummy functions
 * \author Norman Feske
 * \author Sebastian sumpf
 * \date   2011-01-29
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux kernel API */
#include <lx_emul.h>

/* DDE-Kit includes */
#include <dde_kit/printf.h>

/* Linux includes */
#include <linux/input.h>

#define SKIP_VERBOSE    0

#if VERBOSE_LX_EMUL
#define TRACE dde_kit_printf("\033[32m%s\033[0m called, not implemented\n", __PRETTY_FUNCTION__)
#else
#define TRACE
#endif
#if SKIP_VERBOSE
#define SKIP dde_kit_printf("\033[34m%s\033[0m: skipped\n", __PRETTY_FUNCTION__)
#else
#define SKIP
#endif


/******************
 ** asm/atomic.h **
 ******************/

int  atomic_inc_return(atomic_t *v) { TRACE; return 0; }


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

u16  get_unaligned_le16(const void *p) { TRACE; return 0; }
u32  get_unaligned_le32(const void *p) { TRACE; return 0; }


/*******************************
 ** linux/errno.h and friends **
 *******************************/

long PTR_ERR(const void *ptr) { TRACE; return 0; }


/********************
 ** linux/kernel.h **
 ********************/

void might_sleep() { SKIP; }
char *kasprintf(gfp_t gfp, const char *fmt, ...) { TRACE; return NULL; }
int kstrtouint(const char *s, unsigned int base, unsigned int *res) { TRACE; return 0; }
int sprintf(char *buf, const char *fmt, ...) { TRACE; return 0; }
int sscanf(const char *b, const char *s, ...) { TRACE; return 0; }
int scnprintf(char *buf, size_t size, const char *fmt, ...);
int strict_strtoul(const char *s, unsigned int base, unsigned long *res) { TRACE; return 0; }
long simple_strtoul(const char *cp, char **endp, unsigned int base) { TRACE; return 0; }


/******************
 ** linux/log2.h **
 ******************/

int  roundup_pow_of_two(u32 n) { TRACE; return 0; }


/********************
 ** linux/printk.h **
 ********************/

void print_hex_dump(const char *level, const char *prefix_str,
                    int prefix_type, int rowsize, int groupsize,
                    const void *buf, size_t len, bool ascii) { TRACE; }


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

int ffs(int x) { TRACE; return 0; }


/********************
 ** linux/string.h **
 ********************/

int    memcmp(const void *dst, const void *src, size_t s) { TRACE; return 0; }
char  *strcat(char *dest, const char *src) { TRACE; return 0; }
int    strncmp(const char *cs, const char *ct, size_t count) { TRACE; return 0; }
char  *strncpy(char *dst, const char *src, size_t s) { TRACE; return NULL; }
char  *strchr(const char *s, int n) { TRACE; return NULL; }
char  *strrchr(const char *s, int n) { TRACE; return NULL; }
size_t strlcpy(char *dest, const char *src, size_t size) { TRACE; return 0; }
char  *strsep(char **s,const char *d) { TRACE; return NULL; }
char  *kstrdup(const char *s, gfp_t gfp) { TRACE; return 0; }
char  *strstr(const char *h, const char *n) { TRACE; return 0; }


/*****************
 ** linux/nls.h **
 *****************/

int utf16s_to_utf8s(const wchar_t *pwcs, int len,
                    enum utf16_endian endian, u8 *s, int maxlen) { TRACE; return 0; }


/*******************
 ** linux/ctype.h **
 *******************/

int isprint(int v) { TRACE; return 0; }


/**********************
 ** linux/spinlock.h **
 **********************/

void spin_lock(spinlock_t *lock) { SKIP; }
void spin_lock_nested(spinlock_t *lock, int subclass) { TRACE; }
void spin_unlock(spinlock_t *lock) { SKIP; }
void spin_lock_init(spinlock_t *lock) { SKIP; }
void spin_lock_irqsave(spinlock_t *lock, unsigned long flags) { SKIP; }
void spin_lock_irqrestore(spinlock_t *lock, unsigned long flags) { SKIP; }
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags) { SKIP; }
void spin_lock_irq(spinlock_t *lock) { SKIP; }
void spin_unlock_irq(spinlock_t *lock) { SKIP; }
void assert_spin_locked(spinlock_t *lock) { TRACE;}



/*******************
 ** linux/mutex.h **
 *******************/

void mutex_lock_nested(struct mutex *lock, unsigned int subclass) { TRACE; }
int  mutex_lock_interruptible(struct mutex *m) { TRACE; return 0; }


/*******************
 ** linux/rwsem.h **
 *******************/

void down_read(struct rw_semaphore *sem) { TRACE; }
void up_read(struct rw_semaphore *sem) { TRACE; }
void down_write(struct rw_semaphore *sem) { TRACE; }
void up_write(struct rw_semaphore *sem) { TRACE; }


/*******************
 ** linux/ktime.h **
 *******************/

ktime_t ktime_add_ns(const ktime_t kt, u64 nsec) { TRACE; ktime_t ret = { 0 }; return ret; }

s64 ktime_us_delta(const ktime_t later, const ktime_t earlier) { TRACE; return 0; };


/*******************
 ** linux/timer.h **
 *******************/

int del_timer_sync(struct timer_list *timer) { TRACE; return 0; }
unsigned long round_jiffies(unsigned long j) { TRACE; return 1; }


/*********************
 ** linux/hrtimer.h **
 *********************/

ktime_t ktime_get_real(void) { TRACE; ktime_t ret; return ret; }


/*******************
 ** linux/delay.h **
 *******************/

void mdelay(unsigned long msecs) { TRACE; }


/***********************
 ** linux/workquque.h **
 ***********************/

bool cancel_work_sync(struct work_struct *work) { TRACE; return 0; }
int cancel_delayed_work_sync(struct delayed_work *work) { TRACE; return 0; }
bool flush_work_sync(struct work_struct *work) { TRACE; return 0; }


/******************
 ** linux/wait.h **
 ******************/

void init_waitqueue_head(wait_queue_head_t *q) { TRACE; }
void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait) { TRACE; }
void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait) { TRACE; }


/******************
 ** linux/time.h **
 ******************/

struct timespec current_kernel_time(void)
{
	struct timespec t = { 0, 0 };
	return t;
}

void do_gettimeofday(struct timeval *tv) { TRACE; }


/*******************
 ** linux/sched.h **
 *******************/

int kill_pid_info_as_cred(int i, struct siginfo *s, struct pid *p,
                          const struct cred *c, u32 v) { TRACE; return 0; }
pid_t task_pid_nr(struct task_struct *tsk) { TRACE; return 0; }
struct pid *task_pid(struct task_struct *task) { TRACE; return NULL; }
void __set_current_state(int state) { TRACE; }
int signal_pending(struct task_struct *p) { TRACE; return 0; }
void schedule(void) { TRACE; }
void yield(void) { TRACE; }
void cpu_relax(void) { TRACE; udelay(1); }
signed long schedule_timeout(signed long timeout) { TRACE; return 0; }

struct task_struct *current;


/*********************
 ** linux/kthread.h **
 *********************/

int kthread_should_stop(void) { SKIP; return 0; }
int kthread_stop(struct task_struct *k) { TRACE; return 0; }


/**********************
 ** linux/notifier.h **
 **********************/

int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                     struct notifier_block *nb) { TRACE; return 0; }
int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
                                       struct notifier_block *nb) { TRACE; return 0; }
int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long val, void *v) { TRACE; return 0; }
int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
                                   struct notifier_block *nb) { TRACE; return 0; }
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
                                     struct notifier_block *nb) { TRACE; return 0; }



/*********************
 ** linux/kobject.h **
 *********************/

int   add_uevent_var(struct kobj_uevent_env *env, const char *format, ...) { TRACE; return 0; }
char *kobject_name(const struct kobject *kobj) { TRACE; return 0; }
char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask) { TRACE; return 0; }


/*******************
 ** linux/sysfs.h **
 *******************/

int sysfs_create_group(struct kobject *kobj,
                       const struct attribute_group *grp) { TRACE; return 0; }
void sysfs_remove_group(struct kobject *kobj,
                        const struct attribute_group *grp) { TRACE; }

int fasync_helper(int fd, struct file * filp, int on, struct fasync_struct **fapp) { TRACE; return 0; }

ssize_t simple_read_from_buffer(void __user *to, size_t count,
                                loff_t *ppos, const void *from, size_t available)  { TRACE; return 0; }

/************************
 ** linux/pm_runtime.h **
 ************************/

int  pm_runtime_set_active(struct device *dev) { TRACE; return 0; }
void pm_suspend_ignore_children(struct device *dev, bool enable) { TRACE; }
void pm_runtime_enable(struct device *dev) { TRACE; }
void pm_runtime_disable(struct device *dev) { TRACE; }
void pm_runtime_set_suspended(struct device *dev) { TRACE; }
void pm_runtime_get_noresume(struct device *dev) { TRACE; }
void pm_runtime_put_noidle(struct device *dev) { TRACE; }
void pm_runtime_use_autosuspend(struct device *dev) { TRACE; }
int  pm_runtime_put_sync_autosuspend(struct device *dev) { TRACE; return 0; }
void pm_runtime_no_callbacks(struct device *dev) { TRACE; }


/***********************
 ** linux/pm_wakeup.h **
 ***********************/

int  device_init_wakeup(struct device *dev, bool val) { TRACE; return 0; }
int  device_wakeup_enable(struct device *dev) { TRACE; return 0; }
bool device_may_wakeup(struct device *dev) { TRACE; return 0; }
int  device_set_wakeup_enable(struct device *dev, bool enable) { TRACE; return 0; }
bool device_can_wakeup(struct device *dev) { TRACE; return 0; }


/********************
 ** linux/device.h **
 ********************/

int         dev_set_name(struct device *dev, const char *name, ...) { TRACE; return 0; }
int         dev_to_node(struct device *dev) { TRACE; return 0; }
void        set_dev_node(struct device *dev, int node) { TRACE; }

struct device *device_create(struct class *cls, struct device *parent,
                             dev_t devt, void *drvdata,
                             const char *fmt, ...) { TRACE; return NULL; }
void device_destroy(struct class *cls, dev_t devt) { TRACE; }
void device_unregister(struct device *dev) { TRACE; }
void device_lock(struct device *dev) { TRACE; }
int  device_trylock(struct device *dev) { TRACE; return 0; }
void device_unlock(struct device *dev) { TRACE; }
void device_del(struct device *dev) { TRACE; }
void device_initialize(struct device *dev) { TRACE; }
int  device_attach(struct device *dev) { TRACE; return 0; }
int  device_is_registered(struct device *dev) { TRACE; return 0; }
int  device_bind_driver(struct device *dev) { TRACE; return 0; }
void device_release_driver(struct device *dev) { TRACE; }
void device_enable_async_suspend(struct device *dev) { TRACE; }
void device_set_wakeup_capable(struct device *dev, bool capable) { TRACE; }
int  device_create_bin_file(struct device *dev,
                            const struct bin_attribute *attr) { TRACE; return 0; }
void device_remove_bin_file(struct device *dev,
                            const struct bin_attribute *attr) { TRACE; }
int  device_create_file(struct device *device,
                        const struct device_attribute *entry) { TRACE; return 0; }
void device_remove_file(struct device *dev,
                        const struct device_attribute *attr) { TRACE; }

void put_device(struct device *dev) { TRACE; }

void driver_unregister(struct device_driver *drv) { TRACE; }
int  driver_attach(struct device_driver *drv) { TRACE; return 0; }
int  driver_create_file(struct device_driver *driver,
                        const struct driver_attribute *attr) { TRACE; return 0; }
void driver_remove_file(struct device_driver *driver,
                        const struct driver_attribute *attr) { TRACE; }

struct device_driver *get_driver(struct device_driver *drv) { TRACE; return NULL; }
void                  put_driver(struct device_driver *drv) { TRACE; }

struct device *bus_find_device(struct bus_type *bus, struct device *start,
                               void *data,
                               int (*match)(struct device *dev, void *data)) { TRACE; return NULL; }
int  bus_register(struct bus_type *bus) { TRACE; return 0; }
void bus_unregister(struct bus_type *bus) { TRACE; }
int  bus_register_notifier(struct bus_type *bus,
                           struct notifier_block *nb) { TRACE; return 0; }
int  bus_unregister_notifier(struct bus_type *bus,
                             struct notifier_block *nb) { TRACE; return 0; }

struct class *__class_create(struct module *owner,
                             const char *name,
                             struct lock_class_key *key) { TRACE; return NULL; }
int class_register(struct class *cls) { TRACE; return 0; }
void class_unregister(struct class *cls) { TRACE; }
void class_destroy(struct class *cls) { TRACE; }


/*****************************
 ** linux/platform_device.h **
 *****************************/

void *platform_get_drvdata(const struct platform_device *pdev) { TRACE; return NULL; }


/********************
 ** linux/dcache.h **
 ********************/

void           d_instantiate(struct dentry *dentry, struct inode *i) { TRACE; }
int            d_unhashed(struct dentry *dentry) { TRACE; return 0; }
void           d_delete(struct dentry *d) { TRACE; }
struct dentry *d_alloc_root(struct inode *i) { TRACE; return NULL; }
struct dentry *dget(struct dentry *dentry) { TRACE; return NULL; }
void           dput(struct dentry *dentry) { TRACE; }

void dont_mount(struct dentry *dentry) { TRACE; }


/******************
 ** linux/poll.h **
 ******************/

void poll_wait(struct file *f, wait_queue_head_t *w, poll_table *p) { TRACE; }


/********************
 ** linux/statfs.h **
 ********************/

loff_t default_llseek(struct file *file, loff_t offset, int origin) { TRACE; return 0; }


/****************
 ** linux/fs.h **
 ****************/

unsigned iminor(const struct inode *inode) { TRACE; return 0; }
unsigned imajor(const struct inode *inode) { TRACE; return 0; }

int  register_chrdev_region(dev_t d, unsigned v, const char *s) { TRACE; return 0; }
void unregister_chrdev_region(dev_t d, unsigned v) { TRACE; }
void fops_put(struct file_operations const *fops) { TRACE; }
loff_t noop_llseek(struct file *file, loff_t offset, int origin) { TRACE; return 0; }
int register_chrdev(unsigned int major, const char *name,
                    const struct file_operations *fops) { TRACE; return 0; }
void unregister_chrdev(unsigned int major, const char *name) { TRACE; }
struct inode *new_inode(struct super_block *sb) { TRACE; return NULL; }
unsigned int get_next_ino(void) { TRACE; return 0; }
void init_special_inode(struct inode *i, umode_t m, dev_t d) { TRACE; }
int generic_delete_inode(struct inode *inode) { TRACE; return 0; }
void drop_nlink(struct inode *inode) { TRACE; }
void inc_nlink(struct inode *inode) { TRACE; }
void dentry_unhash(struct dentry *dentry) { TRACE; }
void iput(struct inode *i) { TRACE; }
struct dentry *mount_single(struct file_system_type *fs_type,
                            int flags, void *data,
                            int (*fill_super)(struct super_block *,
                                              void *, int)) { TRACE; return NULL; }
int nonseekable_open(struct inode *inode, struct file *filp) { TRACE; return 0; }
int simple_statfs(struct dentry *d, struct kstatfs *k) { TRACE; return 0; }
int simple_pin_fs(struct file_system_type *t, struct vfsmount **mount, int *count) { TRACE; return 0; }
void simple_release_fs(struct vfsmount **mount, int *count) { TRACE; }
void kill_litter_super(struct super_block *sb) { TRACE; }
int register_filesystem(struct file_system_type *t) { TRACE; return 0; }
int unregister_filesystem(struct file_system_type *t) { TRACE; return 0; }

void kill_fasync(struct fasync_struct **fp, int sig, int band) { TRACE; }
int fasync_add_entry(int fd, struct file *filp, struct fasync_struct **fapp) { TRACE; return 0; }
const struct file_operations  simple_dir_operations;
const struct inode_operations simple_dir_inode_operations;


/*******************
 ** linux/namei.h **
 *******************/

struct dentry *lookup_one_len(const char *c, struct dentry *e, int v) { TRACE; return NULL; }


/**********************
 ** linux/seq_file.h **
 **********************/

int seq_printf(struct seq_file *f, const char *fmt, ...) { TRACE; return 0; }


/*****************
 ** linux/gfp.h **
 *****************/

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order) { TRACE; return 0; }
void __free_pages(struct page *p, unsigned int order) { TRACE; }
void free_pages(unsigned long addr, unsigned int order) { TRACE; }


/*********************
 ** linux/proc_fs.h **
 *********************/

struct proc_dir_entry *proc_mkdir(const char *s,struct proc_dir_entry *e) { TRACE; return NULL; }
void remove_proc_entry(const char *name, struct proc_dir_entry *parent) { TRACE; }


/********************
 * linux/debugfs.h **
 ********************/

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent) { TRACE; return (struct dentry *)1; }
struct dentry *debugfs_create_file(const char *name, mode_t mode,
                                   struct dentry *parent, void *data,
                                   const struct file_operations *fops) { TRACE; return (struct dentry *)1; }
void debugfs_remove(struct dentry *dentry) { TRACE; }


/************************
 ** linux/page-flags.h **
 ************************/

bool is_highmem(void *ptr) { TRACE; return 0; }


/****************
 ** linux/mm.h **
 ****************/

struct zone *page_zone(const struct page *page) { TRACE; return NULL; }


/**********************
 ** linux/highmem.h  **
 **********************/

void *kmap(struct page *page) { TRACE; return 0; }
void kunmap(struct page *page) { TRACE; }


/**********************
 ** asm-generic/io.h **
 **********************/

void iounmap(volatile void *addr) { TRACE; }
void native_io_delay(void) { TRACE; }


/********************
 ** linux/ioport.h **
 ********************/

void release_region(resource_size_t start, resource_size_t n) { TRACE; }
void release_mem_region(resource_size_t start, resource_size_t n) { TRACE; }

/**
 * SKIP
 */

/* implemented in Pci_driver */
struct resource *request_region(resource_size_t start, resource_size_t n,
                                const char *name) { SKIP; return (struct resource *)1; }
/* implemented in Pci_driver */
struct resource *request_mem_region(resource_size_t start, resource_size_t n,
                                    const char *name) { SKIP; return (struct resource *)1; }

/***********************
 ** linux/interrupt.h **
 ***********************/

void local_irq_enable(void) { TRACE; }
void local_irq_disable(void) { TRACE; }
void free_irq(unsigned int i, void *p) { TRACE; }


/*********************
 ** linux/hardirq.h **
 *********************/

void synchronize_irq(unsigned int irq) { TRACE; }


/*****************
 ** linux/pci.h **
 *****************/
int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn, int where, u32 *val) { TRACE; return 0; }
int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn, int where, u32 val) { TRACE; return 0; }

void *pci_get_drvdata(struct pci_dev *pdev) { TRACE; return NULL; }
void pci_dev_put(struct pci_dev *dev) { TRACE; }
struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device,
                               struct pci_dev *from) { TRACE; return NULL; }


void pci_disable_device(struct pci_dev *dev) { TRACE; }
int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask) { TRACE; return 0; }

void pci_unregister_driver(struct pci_driver *drv) { TRACE; }

bool pci_dev_run_wake(struct pci_dev *dev) { TRACE; return 0; }
int pci_set_mwi(struct pci_dev *dev) { TRACE; return 0; }
int pci_find_capability(struct pci_dev *dev, int cap) { TRACE; return 0; }
struct pci_dev *pci_get_slot(struct pci_bus *bus, unsigned int devfn) { TRACE; return NULL; }
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
                                         struct pci_dev *dev) { TRACE; return 0; }
void *pci_ioremap_bar(struct pci_dev *pdev, int bar);

/**
 * Omitted PCI functions
 */
/* scans resources, this is already implemented in 'pci_driver.cc' */
int pci_enable_device(struct pci_dev *dev) { SKIP; return 0; }

/* implemented in Pci_driver::_setup_pci_device */
void pci_set_master(struct pci_dev *dev) { SKIP; }

/**********************
 ** linux/irqflags.h **
 **********************/

unsigned long local_irq_save(unsigned long flags) { SKIP; return 0; }
unsigned long local_irq_restore(unsigned long flags) { SKIP; return 0; }


/*************************
 ** linux/dma-mapping.h **
 *************************/

void dma_unmap_single_attrs(struct device *dev, dma_addr_t addr,
                            size_t size,
                            enum dma_data_direction dir,
                            struct dma_attrs *attrs) { SKIP; }

void dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sg,
                        int nents, enum dma_data_direction dir,
                        struct dma_attrs *attrs) { SKIP; }


void dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size,
                    enum dma_data_direction direction) { SKIP;; }

int dma_mapping_error(struct device *dev, dma_addr_t dma_addr) { SKIP; return 0; }

/*****************
 ** linux/pid.h **
 *****************/

void put_pid(struct pid *pid) { TRACE; }
struct pid *get_pid(struct pid *pid) { TRACE; return NULL; }


/******************
 ** linux/cred.h **
 ******************/

void put_cred(struct cred const *c) { TRACE; }
const struct cred *get_cred(const struct cred *cred) { TRACE; return NULL; }


/**********************
 ** linux/security.h **
 **********************/

void security_task_getsecid(struct task_struct *p, u32 *secid) { TRACE; }


/******************
 ** linux/cdev.h **
 ******************/

void cdev_init(struct cdev *c, const struct file_operations *fops) { TRACE; }
int cdev_add(struct cdev *c, dev_t d, unsigned v) { TRACE; return 0; }
void cdev_del(struct cdev *c) { TRACE; }


/*********************
 ** linux/utsname.h **
 *********************/

struct new_utsname *init_utsname(void)
{
	static struct new_utsname uts = { .sysname = "Genode.UTS", .release = "1.0" };
	return &uts;
}
struct new_utsname *utsname(void) { TRACE; return NULL; }


/*********************
 ** linux/freezer.h **
 *********************/

void set_freezable(void) { TRACE; }


/********************
 ** linux/parser.h **
 ********************/

int match_token(char *s, const match_table_t table, substring_t args[]) { TRACE; return 0; }
int match_int(substring_t *s, int *result) { TRACE; return 0; }
int match_octal(substring_t *s, int *result) { TRACE; return 0; }


/*********************
 ** linux/semaphore **
 *********************/
void sema_init(struct semaphore *sem, int val) { TRACE; }
int  down_trylock(struct semaphore *sem) { TRACE; return 0; }
int  down_interruptible(struct semaphore *sem) { TRACE; return 0; }
void up(struct semaphore *sem) { TRACE; }


/*******************
 ** linux/input.h **
 *******************/

void input_ff_destroy(struct input_dev *dev) { TRACE; }
int input_ff_event(struct input_dev *dev, unsigned int type, unsigned int code, int value) { TRACE; return 0; }
int input_ff_upload(struct input_dev *dev, struct ff_effect *effect, struct file *file) { TRACE; return 0; }
int input_ff_erase(struct input_dev *dev, int effect_id, struct file *file) { TRACE; return 0; }

/*********************
 ** input-compat.h" **
 *********************/

int input_event_from_user(const char __user *buffer, struct input_event *event) { TRACE; return 0; }
int input_event_to_user(char __user *buffer, const struct input_event *event) { TRACE; return 0; }
int input_ff_effect_from_user(const char __user *buffer, size_t size, struct ff_effect *effect) { TRACE; return 0;}

/****************
 ** linux/mt.h **
 ****************/

void input_mt_destroy_slots(struct input_dev *dev) { TRACE; }


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size) { TRACE; return 0; }


/********************
 ** linux/blkdev.h **
 ********************/

void blk_queue_bounce_limit(struct request_queue *q, u64 dma_mask) { TRACE; }
void blk_queue_update_dma_alignment(struct request_queue *q, int mask) { TRACE; }
void blk_queue_max_hw_sectors(struct request_queue *q, unsigned int max_hw_sectors) { TRACE; }
unsigned int queue_max_hw_sectors(struct request_queue *q) { TRACE; return 0; }


/**********************
 ** scsi/scsi_cmnd.h **
 **********************/

void scsi_set_resid(struct scsi_cmnd *cmd, int resid) { TRACE; }
int scsi_get_resid(struct scsi_cmnd *cmd) { TRACE; return 0; }


/********************
 ** scsi/scsi_eh.h **
 *******************/

void scsi_report_bus_reset(struct Scsi_Host *shost, int channel) { TRACE; }
void scsi_report_device_reset(struct Scsi_Host *shost, int channel, int target) { TRACE; }

void scsi_eh_prep_cmnd(struct scsi_cmnd *scmd,
                       struct scsi_eh_save *ses, unsigned char *cmnd,
                       int cmnd_size, unsigned sense_bytes) { TRACE; }

void scsi_eh_restore_cmnd(struct scsi_cmnd* scmd,
                          struct scsi_eh_save *ses) { TRACE; }


int scsi_normalize_sense(const u8 *sense_buffer, int sb_len,
                         struct scsi_sense_hdr *sshdr) { TRACE; return 0; }

const u8 * scsi_sense_desc_find(const u8 * sense_buffer, int sb_len,
                                int desc_type) { TRACE; return 0; }


/***********************
 ** drivers/scsi/sd.h **
 **********************/

struct scsi_disk *scsi_disk(struct gendisk *disk) { TRACE; return 0; }


/**********************
 ** scsi/scsi_host.h **
 **********************/
int scsi_add_host_with_dma(struct Scsi_Host *shost, struct device *dev,
                           struct device *dma_dev) { TRACE; return 0; }
void scsi_remove_host(struct Scsi_Host *shost) { TRACE; }
void scsi_host_put(struct Scsi_Host *shost) { TRACE; }
struct scsi_device *scsi_get_host_dev(struct Scsi_Host *shost) { TRACE; return 0; }


/********************************
 ** linux/regulator/consumer.h **
 ********************************/
struct regulator;
int    regulator_enable(struct regulator *r)  { TRACE; return 0; }
int    regulator_disable(struct regulator *r) { TRACE; return 0; }
void   regulator_put(struct regulator *r)     { TRACE; }
struct regulator *regulator_get(struct device *dev, const char *id) { TRACE; return 0; }


/*******************************************
 ** arch/arm/plat-omap/include/plat/usb.h **
 *******************************************/

int omap_usbhs_enable(struct device *dev) { TRACE; return 0; }
void omap_usbhs_disable(struct device *dev) { TRACE; }


/********************
 ** linux/skbuff.h **
 ********************/

unsigned char *__skb_put(struct sk_buff *skb, unsigned int len) { TRACE; return 0; }
int skb_checksum_start_offset(const struct sk_buff *skb) { TRACE; return 0; }
struct sk_buff *skb_copy_expand(const struct sk_buff *skb,
                                int newheadroom, int newtailroom,
                                gfp_t gfp_mask) { TRACE; return 0; }
int skb_tailroom(const struct sk_buff *skb) { TRACE; return 0; }

int skb_queue_empty(const struct sk_buff_head *list) { TRACE; return 1; }
void skb_queue_purge(struct sk_buff_head *list) { TRACE; }

void skb_tx_timestamp(struct sk_buff *skb) { TRACE; }
bool skb_defer_rx_timestamp(struct sk_buff *skb) { TRACE; return 0; }


/*********************
 ** linux/ethtool.h **
 *********************/

__u32 ethtool_cmd_speed(const struct ethtool_cmd *ep) { TRACE; return 0; }
u32 ethtool_op_get_link(struct net_device *dev) { TRACE; return 0; }


/***********************
 ** linux/netdevice.h **
 ***********************/

u32 netif_msg_init(int debug_value, int default_msg_enable_bits) { TRACE; return 0; }

void netif_start_queue(struct net_device *dev) { TRACE; }
void netif_device_detach(struct net_device *dev) { TRACE; }
void netif_stop_queue(struct net_device *dev) { TRACE; }
void netif_wake_queue(struct net_device *dev) { TRACE; }
void netif_device_attach(struct net_device *dev) { TRACE; }
void unregister_netdev(struct net_device *dev) { TRACE; }
void free_netdev(struct net_device *dev) { TRACE; }
void netif_carrier_off(struct net_device *dev) { TRACE; }

int netdev_mc_empty(struct net_device *dev) { TRACE; return 1; }

/*****************
 ** linux/mii.h **
 *****************/

unsigned int mii_check_media (struct mii_if_info *mii,
                              unsigned int ok_to_print,
                              unsigned int init_media) { TRACE; return 0; }
int mii_ethtool_sset(struct mii_if_info *mii, struct ethtool_cmd *ecmd) { TRACE; return 0; }
int mii_link_ok (struct mii_if_info *mii) { TRACE; return 0; }

int generic_mii_ioctl(struct mii_if_info *mii_if,
                      struct mii_ioctl_data *mii_data, int cmd,
                      unsigned int *duplex_changed) { TRACE; return 0; }
struct mii_ioctl_data *if_mii(struct ifreq *rq) { TRACE; return 0; }


/*************************
 ** linux/etherdevice.h **
 *************************/

__be16 eth_type_trans(struct sk_buff *skb, struct net_device *dev) { TRACE; return 0; }
int eth_mac_addr(struct net_device *dev, void *p) { TRACE; return 0; }
int eth_validate_addr(struct net_device *dev) { TRACE; return 0; }


/**********************
 ** linux/inerrupt.h **
 **********************/

void tasklet_kill(struct tasklet_struct *t) { TRACE; }


/********************
 ** asm/checksum.h **
 ********************/

__wsum csum_partial(const void *buff, int len, __wsum wsum) { TRACE; return 0; }
__sum16 csum_fold(__wsum sum) { TRACE; return 0; }


