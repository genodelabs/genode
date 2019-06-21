/*
 * \brief  Dummy functions
 * \author Norman Feske
 * \author Sebastian sumpf
 * \date   2011-01-29
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 * Copyright (C) 2014 Ksys Labs LLC
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kernel API */
#include <lx_emul.h>

/* Linux includes */
#include <linux/input.h>

#define SKIP_VERBOSE    0

#if DEBUG_TRACE
#define TRACE lx_printf("\033[32m%s\033[0m called from %p, not implemented\n", __PRETTY_FUNCTION__, __builtin_return_address(0))
#else
#define TRACE
#endif
#if SKIP_VERBOSE
#define SKIP lx_printf("\033[34m%s\033[0m: skipped\n", __PRETTY_FUNCTION__)
#else
#define SKIP
#endif

/******************
 ** linux/slab.h **
 ******************/

void *kmalloc_array(size_t n, size_t size, gfp_t flags) { TRACE; return (void *)0xdeadbeaf; }


/******************
 ** asm/atomic.h **
 ******************/

int  atomic_inc_return(atomic_t *v) { TRACE; return 0; }


/********************
 ** linux/kernel.h **
 ********************/

void might_sleep() { SKIP; }
char *kasprintf(gfp_t gfp, const char *fmt, ...) { TRACE; return NULL; }
int kstrtouint(const char *s, unsigned int base, unsigned int *res) { TRACE; return 0; }
int kstrtoul(const char *s, unsigned int base, unsigned long *res) { TRACE; return 0; }
int kstrtou8(const char *s, unsigned int base, u8 *x) { TRACE; return 1; }
int sprintf(char *buf, const char *fmt, ...) { TRACE; return 0; }
int sscanf(const char *b, const char *s, ...) { TRACE; return 0; }
int scnprintf(char *buf, size_t size, const char *fmt, ...);
int strict_strtoul(const char *s, unsigned int base, unsigned long *res) { TRACE; return 0; }
long simple_strtoul(const char *cp, char **endp, unsigned int base) { TRACE; return 0; }


/******************
 ** linux/log2.h **
 ******************/

int roundup_pow_of_two(u32 n) { TRACE; return 0; }


/********************
 ** linux/printk.h **
 ********************/

void print_hex_dump(const char *level, const char *prefix_str,
                    int prefix_type, int rowsize, int groupsize,
                    const void *buf, size_t len, bool ascii) { TRACE; }
bool printk_ratelimit() { TRACE; return 0; }
bool printk_ratelimited() { TRACE; return 0; }
bool printk_timed_ratelimit(unsigned long *caller_jiffies,
                            unsigned int interval_msec) { TRACE; return false; }


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
char  *strsep(char **s,const char *d) { TRACE; return NULL; }
char  *kstrdup(const char *s, gfp_t gfp) { TRACE; return 0; }
char  *strstr(const char *h, const char *n) { TRACE; return 0; }


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

int  mutex_lock_interruptible(struct mutex *m) { SKIP; return 0; }
void mutex_init  (struct mutex *m) { SKIP; }
void mutex_lock  (struct mutex *m) { SKIP; }
void mutex_unlock(struct mutex *m) { SKIP; }


/*******************
 ** linux/rwsem.h **
 *******************/

void down_read(struct rw_semaphore *sem) { SKIP; }
void up_read(struct rw_semaphore *sem) { SKIP; }
void down_write(struct rw_semaphore *sem) { SKIP; }
void up_write(struct rw_semaphore *sem) { SKIP; }


/*********************
 ** linux/lockdep.h **
 *********************/

bool lockdep_is_held(void *l) { TRACE; return 1; }


/********************
 ** linux/random.h **
 ********************/

void add_device_randomness(const void *buf, unsigned int size) { TRACE; }


/*******************
 ** linux/ktime.h **
 *******************/

#define KTIME_RET ({TRACE; ktime_t t = { 0 }; return t;})

ktime_t ktime_add_ns(const ktime_t kt, u64 nsec) { KTIME_RET; }
ktime_t ktime_get_monotonic_offset(void) { KTIME_RET; }
ktime_t ktime_sub(const ktime_t lhs, const ktime_t rhs) { KTIME_RET; }
ktime_t ktime_get_real(void) { TRACE; ktime_t ret; return ret; }
ktime_t ktime_get_boottime(void) { TRACE; KTIME_RET; }

s64 ktime_us_delta(const ktime_t later, const ktime_t earlier) { TRACE; return 0; };


/*******************
 ** linux/timer.h **
 *******************/

unsigned long round_jiffies(unsigned long j) { TRACE; return 1; }
void set_timer_slack(struct timer_list *time, int slack_hz) { TRACE; }


/*******************
 ** linux/delay.h **
 *******************/

void usleep_range(unsigned long min, unsigned long max) { TRACE; }


/***********************
 ** linux/workquque.h **
 ***********************/

void destroy_workqueue(struct workqueue_struct *wq) { TRACE; }

bool flush_work(struct work_struct *work) { TRACE; return 0; }
bool flush_work_sync(struct work_struct *work) { TRACE; return 0; }


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
void cpu_relax(void) { SKIP; }

struct task_struct *current;


/*********************
 ** linux/kthread.h **
 *********************/

int kthread_should_stop(void) { SKIP; return 0; }
int kthread_stop(struct task_struct *k) { TRACE; return 0; }


/**********************
 ** linux/notifier.h **
 **********************/

int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
                                       struct notifier_block *nb) { TRACE; return 0; }
int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
                                   struct notifier_block *nb) { TRACE; return 0; }
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
                                     struct notifier_block *nb) { TRACE; return 0; }



/*********************
 ** linux/kobject.h **
 *********************/

int   add_uevent_var(struct kobj_uevent_env *env, const char *format, ...) { TRACE; return 0; }
const char *kobject_name(const struct kobject *kobj) { TRACE; return 0; }
char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask) { TRACE; return 0; }


/*******************
 ** linux/sysfs.h **
 *******************/

int sysfs_create_group(struct kobject *kobj,
                       const struct attribute_group *grp) { TRACE; return 0; }
void sysfs_remove_group(struct kobject *kobj,
                        const struct attribute_group *grp) { TRACE; }
int sysfs_create_link(struct kobject *kobj, struct kobject *target,
                      const char *name) { TRACE; return 0; }
void sysfs_remove_link(struct kobject *kobj, const char *name) { TRACE; }

int sysfs_create_files(struct kobject *kobj, const struct attribute **attr) { TRACE; return 1; }

int fasync_helper(int fd, struct file * filp, int on, struct fasync_struct **fapp) { TRACE; return 0; }

ssize_t simple_read_from_buffer(void __user *to, size_t count,
                                loff_t *ppos, const void *from, size_t available)  { TRACE; return 0; }

/************************
 ** linux/pm_runtime.h **
 ************************/

bool pm_runtime_active(struct device *dev) { SKIP; return true; }
int  pm_runtime_set_active(struct device *dev) { SKIP; return 0; }
void pm_suspend_ignore_children(struct device *dev, bool enable) { SKIP; }
void pm_runtime_enable(struct device *dev) { SKIP; }
void pm_runtime_disable(struct device *dev) { SKIP; }
void pm_runtime_allow(struct device *dev) { SKIP; }
void pm_runtime_forbid(struct device *dev) { SKIP; }
void pm_runtime_set_suspended(struct device *dev) { SKIP; }
void pm_runtime_get_noresume(struct device *dev) { SKIP; }
void pm_runtime_put_noidle(struct device *dev) { SKIP; }
void pm_runtime_use_autosuspend(struct device *dev) { SKIP; }
int  pm_runtime_put_sync_autosuspend(struct device *dev) { SKIP; return 0; }
void pm_runtime_no_callbacks(struct device *dev) { SKIP; }
void pm_runtime_set_autosuspend_delay(struct device *dev, int delay) { SKIP; }
int  pm_runtime_get_sync(struct device *dev) { SKIP; return 0; }
int  pm_runtime_put_sync(struct device *dev) { SKIP; return 0; }
int  pm_runtime_put(struct device *dev) { SKIP; return 0; }
int  pm_runtime_barrier(struct device *dev) { SKIP; return 0; }


/***********************
 ** linux/pm_wakeup.h **
 ***********************/

int  device_init_wakeup(struct device *dev, bool val) { TRACE; return 0; }
int  device_wakeup_enable(struct device *dev) { TRACE; return 0; }
bool device_may_wakeup(struct device *dev) { TRACE; return 1; }
int  device_set_wakeup_enable(struct device *dev, bool enable) { TRACE; return 0; }
bool device_can_wakeup(struct device *dev) { TRACE; return 0; }


/********************
 ** linux/pm_qos.h **
 ********************/

int dev_pm_qos_expose_flags(struct device *dev, s32 value) { TRACE; return 0; }
int dev_pm_qos_add_request(struct device *dev, struct dev_pm_qos_request *req,
                           enum dev_pm_qos_req_type type, s32 value) { TRACE; return 0; }
int dev_pm_qos_remove_request(struct dev_pm_qos_request *req) { TRACE; return 0; }


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
void device_lock(struct device *dev) { TRACE; }
int  device_trylock(struct device *dev) { TRACE; return 0; }
void device_unlock(struct device *dev) { TRACE; }
void device_initialize(struct device *dev) { TRACE; }
int  device_attach(struct device *dev) { TRACE; return 0; }
int  device_bind_driver(struct device *dev) { TRACE; return 0; }
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
int device_for_each_child(struct device *dev, void *data,
                          int (*fn)(struct device *dev, void *data)) { TRACE; return 0; }

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
int  bus_for_each_dev(struct bus_type *bus, struct device *start, void *data,
                      int (*fn)(struct device *dev, void *data)) { TRACE; return 0; }

struct class *__class_create(struct module *owner,
                             const char *name,
                             struct lock_class_key *key) { TRACE; return NULL; }
int class_register(struct class *cls) { TRACE; return 0; }
void class_unregister(struct class *cls) { TRACE; }
void class_destroy(struct class *cls) { TRACE; }

void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp) { TRACE; return 0; }
void  devres_add(struct device *dev, void *res) { TRACE; }
int   devres_destroy(struct device *dev, dr_release_t release,
                     dr_match_t match, void *match_data) { TRACE; return 0; }
void devres_free(void *res) { TRACE; }

void devm_kfree(struct device *dev, void *p) { TRACE; }


/*****************************
 ** linux/platform_device.h **
 *****************************/

int platform_device_del(struct platform_device *pdev) { TRACE; return 0; }
int platform_device_put(struct platform_device *pdev) { TRACE; return 0; }
void platform_device_unregister(struct platform_device *pdev) { TRACE; }


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


struct inode *file_inode(struct file *f)
{
	TRACE;
	static struct inode _i;
	return &_i;
}

/*******************
 ** linux/namei.h **
 *******************/

struct dentry *lookup_one_len(const char *c, struct dentry *e, int v) { TRACE; return NULL; }


/**********************
 ** linux/seq_file.h **
 **********************/

int seq_printf(struct seq_file *f, const char *fmt, ...) { TRACE; return 0; }
int seq_putc(struct seq_file *f, char c) { TRACE; return 0;}


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
int    is_vmalloc_addr(const void *x) { TRACE; return 0; }
void   kvfree(const void *addr) { TRACE; }


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
bool in_interrupt(void) { TRACE; return 1; }


/*****************
 ** linux/pci.h **
 *****************/


void *pci_get_drvdata(struct pci_dev *pdev) { TRACE; return NULL; }
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

int  pci_enable_msi(struct pci_dev *pdev) { TRACE; return -1; }
void pci_disable_msi(struct pci_dev *pdev) { TRACE; }

int  pci_enable_msix(struct pci_dev *pdev, struct msix_entry *entries, int vec) { TRACE; return -1; }
int  pci_enable_msix_exact(struct pci_dev *pdef,struct msix_entry *entries, int vec) { TRACE; return -1; }
void pci_disable_msix(struct pci_dev *pdev) { TRACE; }


int pci_set_power_state(struct pci_dev *dev, pci_power_t state) { TRACE; return 0; }


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
unsigned smp_processor_id() { return 0; }

/*************************
 ** linux/scatterlist.h **
 *************************/

void sg_init_table(struct scatterlist *sg, unsigned int nents) { TRACE; }
void sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen) { TRACE; }
void sg_set_page(struct scatterlist *sg, struct page *page,
                 unsigned int len, unsigned int offset) { TRACE; }
int sg_nents(struct scatterlist *sg) { TRACE; return 0; }

void sg_miter_start(struct sg_mapping_iter *miter, struct scatterlist *sgl,
                    unsigned int nents, unsigned int flags) { TRACE; }
bool sg_miter_skip(struct sg_mapping_iter *miter, off_t offset) { TRACE; return false;}
bool sg_miter_next(struct sg_mapping_iter *miter) { TRACE; return false; }
void sg_miter_stop(struct sg_mapping_iter *miter) { TRACE; }


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
                    enum dma_data_direction direction) { SKIP; }

int dma_mapping_error(struct device *dev, dma_addr_t dma_addr) { SKIP; return 0; }


/*********************
 ** linux/uaccess.h **
 *********************/

unsigned long clear_user(void *to, unsigned long n) { TRACE; return 0; }

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

void sema_init(struct semaphore *sem, int val) { SKIP; }
int  down_trylock(struct semaphore *sem) { SKIP; return 0; }
int  down_interruptible(struct semaphore *sem) { SKIP; return 0; }
void up(struct semaphore *sem) { SKIP; }


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

void scsi_set_resid(struct scsi_cmnd *cmd, int resid) { SKIP; }
int scsi_get_resid(struct scsi_cmnd *cmd) { SKIP; return 0; }


/********************
 ** scsi/scsi_eh.h **
 *******************/

void scsi_report_bus_reset(struct Scsi_Host *shost, int channel) { TRACE; }
void scsi_report_device_reset(struct Scsi_Host *shost, int channel, int target) { TRACE; }



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
int scsi_is_host_device(const struct device *dev) { TRACE; return 0; }


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


/*****************
 ** linux/net.h **
 *****************/

int net_ratelimit(void) { TRACE; return 0; }

/********************
 ** linux/skbuff.h **
 ********************/

unsigned char *__skb_put(struct sk_buff *skb, unsigned int len) { TRACE; return 0; }
int skb_checksum_start_offset(const struct sk_buff *skb) { TRACE; return 0; }
struct sk_buff *skb_copy_expand(const struct sk_buff *skb,
                                int newheadroom, int newtailroom,
                                gfp_t gfp_mask) { TRACE; return 0; }

int skb_queue_empty(const struct sk_buff_head *list) { TRACE; return 1; }
void skb_queue_purge(struct sk_buff_head *list) { TRACE; }

void skb_tx_timestamp(struct sk_buff *skb) { TRACE; }
bool skb_defer_rx_timestamp(struct sk_buff *skb) { TRACE; return 0; }

int skb_linearize(struct sk_buff *skb) { TRACE; return 0; }

unsigned int skb_headlen(const struct sk_buff *skb) { TRACE; return 0; }
unsigned int skb_frag_size(const skb_frag_t *frag) { TRACE; return 0; }

int pskb_expand_head(struct sk_buff *skb, int nhead, int ntail, gfp_t mask) { TRACE; return 0; }

/*********************
 ** linux/ethtool.h **
 *********************/

__u32 ethtool_cmd_speed(const struct ethtool_cmd *ep) { TRACE; return 0; }
u32 ethtool_op_get_link(struct net_device *dev) { TRACE; return 0; }
int ethtool_op_get_ts_info(struct net_device *dev, struct ethtool_ts_info *eti) {
	TRACE; return 0; }


/***********************
 ** linux/netdevice.h **
 ***********************/

u32 netif_msg_init(int debug_value, int default_msg_enable_bits) { TRACE; return 0; }

void netif_start_queue(struct net_device *dev) { TRACE; }
void netif_device_detach(struct net_device *dev) { TRACE; }
void netif_stop_queue(struct net_device *dev) { TRACE; }
void netif_wake_queue(struct net_device *dev) { TRACE; }
void netif_tx_wake_all_queues(struct net_device *dev) { TRACE; }
void netif_device_attach(struct net_device *dev) { TRACE; }
void unregister_netdev(struct net_device *dev) { TRACE; }
void free_netdev(struct net_device *dev) { TRACE; }

int netdev_mc_empty(struct net_device *dev) { TRACE; return 1; }
unsigned netdev_mc_count(struct net_device * dev) { TRACE; return 1; }


/*****************
 ** linux/mii.h **
 *****************/

int mii_ethtool_sset(struct mii_if_info *mii, struct ethtool_cmd *ecmd) { TRACE; return 0; }

int generic_mii_ioctl(struct mii_if_info *mii_if,
                      struct mii_ioctl_data *mii_data, int cmd,
                      unsigned int *duplex_changed) { TRACE; return 0; }
struct mii_ioctl_data *if_mii(struct ifreq *rq) { TRACE; return 0; }


/***********************
 ** uapi/linux/mdio.h **
 ***********************/

u16 ethtool_adv_to_mmd_eee_adv_t(u32 adv) { TRACE; return 0; }
u32 mmd_eee_adv_to_ethtool_adv_t(u16 eee_adv) { TRACE; return 0; }
u32 mmd_eee_cap_to_ethtool_sup_t(u16 eee_cap) { TRACE; return 0; }


/*************************
 ** linux/etherdevice.h **
 *************************/

__be16 eth_type_trans(struct sk_buff *skb, struct net_device *dev) { TRACE; return 0; }
int eth_mac_addr(struct net_device *dev, void *p) { TRACE; return 0; }
int eth_validate_addr(struct net_device *dev) { TRACE; return 0; }
bool ether_addr_equal(const u8 *addr1, const u8 *addr2) { TRACE; return 0; }


/**********************
 ** linux/inerrupt.h **
 **********************/

void tasklet_kill(struct tasklet_struct *t) { TRACE; }


/********************
 ** asm/checksum.h **
 ********************/

__wsum csum_partial(const void *buff, int len, __wsum wsum) { TRACE; return 0; }
__sum16 csum_fold(__wsum sum) { TRACE; return 0; }


/*****************
 ** linux/clk.h **
 *****************/

struct clk { };

struct clk *clk_get(struct device *dev, const char *id)
{
	static struct clk _c;
	TRACE;
	return &_c;
}

int    clk_enable(struct clk *clk) { TRACE; return 0; }
void   clk_disable(struct clk *clk) { TRACE; }
void   clk_put(struct clk *clk) { TRACE; }

struct clk *devm_clk_get(struct device *dev, const char *id) { TRACE; return 0; }
int    clk_prepare_enable(struct clk *clk) { TRACE; return 0; }
void   clk_disable_unprepare(struct clk *clk) { TRACE; }


/********************
 ** linux/bitmap.h **
 ********************/

int bitmap_subset(const unsigned long *src1,
                  const unsigned long *src2, int nbits) { TRACE; return 1; }

int bitmap_weight(const unsigned long *src, unsigned int nbits) { TRACE; return 0; }


/*****************
 ** linux/idr.h **
 *****************/

int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end,
                   gfp_t gfp_mask) { TRACE; return 0; }
void ida_simple_remove(struct ida *ida, unsigned int id) { TRACE; }


/****************************
 ** drivers/usb/core/usb.h **
 ****************************/

#include <linux/usb.h>
#include <drivers/usb/core/usb.h>

const struct attribute_group *usb_interface_groups[1];
const struct attribute_group *usb_device_groups[1];
struct usb_driver usbfs_driver;

DEFINE_MUTEX(usbfs_mutex);

void usb_create_sysfs_intf_files(struct usb_interface *intf) { TRACE; }
void usb_remove_sysfs_intf_files(struct usb_interface *intf) { TRACE; }

int usb_create_sysfs_dev_files(struct usb_device *dev) { TRACE; return 0; }
void usb_remove_sysfs_dev_files(struct usb_device *dev) { TRACE; }

int usb_devio_init(void) { TRACE; return 0; }
void usb_devio_cleanup(void) { TRACE; }


/*******************
 ** linux/crc16.h **
 *******************/

u16 crc16(u16 crc, const u8 *buffer, size_t len) { TRACE; return 0; }


/*******************
 ** linux/birev.h **
 *******************/

u16 bitrev16(u16 in) { TRACE; return 0; }


/************************
 ** linux/radix-tree.h **
 ************************/

void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index) { TRACE; return 0; }
void *radix_tree_delete(struct radix_tree_root *root, unsigned long index) { TRACE; return 0; }
void  radix_tree_preload_end(void) { TRACE; }

int radix_tree_insert(struct radix_tree_root *root, unsigned long index, void *item) { TRACE; return 0; }
int radix_tree_maybe_preload(gfp_t gfp_mask) { TRACE; return 0; }

/******************
 ** linux/gpio.h **
 ******************/

bool gpio_is_valid(int number) { TRACE; return false; }
void gpio_set_value_cansleep(unsigned gpio, int value) { TRACE; }
int  gpio_request_one(unsigned gpio, unsigned long flags, const char *label) { TRACE; return 0; }

int devm_gpio_request_one(struct device *dev, unsigned gpio,
                          unsigned long flags, const char *label) { TRACE; return 0; }


/*********************
 ** linux/of_gpio.h **
 *********************/

 int of_get_named_gpio(struct device_node *np,
                       const char *propname, int index) { TRACE; return 0; }


/********************
 ** linux/module.h **
 ********************/

void module_put(struct module *m)   { TRACE; }
void __module_get(struct module *m) { TRACE; }


/******************
 ** linux/phy.h  **
 ******************/

#include <linux/usb/phy.h>

struct mii_bus *mdiobus_alloc(void) { TRACE; return 0; }
int  mdiobus_register(struct mii_bus *bus) { TRACE; return 0; }
void mdiobus_unregister(struct mii_bus *bus) { TRACE; }
void mdiobus_free(struct mii_bus *bus) { TRACE; }

int  phy_mii_ioctl(struct phy_device *phydev, struct ifreq *ifr, int cmd) { TRACE; return 0; }
void phy_print_status(struct phy_device *phydev) { TRACE; }
int  phy_ethtool_sset(struct phy_device *phydev, struct ethtool_cmd *cmd) { TRACE; return 0; }
int  phy_ethtool_gset(struct phy_device *phydev, struct ethtool_cmd *cmd) { TRACE; return 0; }
int  phy_start_aneg(struct phy_device *phydev) { TRACE; return 0; }
void phy_start(struct phy_device *phydev) { TRACE; }
void phy_stop(struct phy_device *phydev) { TRACE; }
int phy_init(struct phy *phy) { TRACE; return 0; }
int phy_exit(struct phy *phy) { TRACE; return 0; }
int phy_power_on(struct phy *phy) { TRACE; return 0; }
int phy_power_off(struct phy *phy) { TRACE; return 0; }
int  phy_create_lookup(struct phy *phy, const char *con_id, const char *dev_id) { TRACE; return 0; }
void phy_remove_lookup(struct phy *phy, const char *con_id, const char *dev_id) { TRACE; }

int  genphy_resume(struct phy_device *phydev) { TRACE; return 0; }

struct phy_device * phy_connect(struct net_device *dev, const char *bus_id,
                                void (*handler)(struct net_device *),
                                phy_interface_t interface) { TRACE; return 0; }
void phy_disconnect(struct phy_device *phydev) { TRACE; }

struct usb_phy *devm_usb_get_phy_by_phandle(struct device *dev, const char *phandle, u8 index) { TRACE; return 0; }
struct usb_phy *devm_usb_get_phy(struct device *dev, enum usb_phy_type type) { TRACE; return 0; }
struct usb_phy *devm_usb_get_phy_dev(struct device *dev, u8 index) { TRACE; return 0; }

struct usb_phy *usb_get_phy_dev(struct device *dev, u8 index) { TRACE; return 0; }
void   usb_put_phy(struct usb_phy *x) { TRACE; }

struct phy *devm_phy_get(struct device *dev, const char *string) { TRACE; return 0; }


/****************
 ** linux/of.h **
 ****************/

struct of_dev_auxdata;
bool     of_property_read_bool(const struct device_node *np, const char *propname) { TRACE; return false; }
unsigned of_usb_get_maximum_speed(struct device_node *np) { TRACE; return 0; }
unsigned of_usb_get_dr_mode(struct device_node *np) { TRACE; return 0; }
int      of_platform_populate(struct device_node *root, const struct of_device_id *matches,
                              const struct of_dev_auxdata *lookup, struct device *parent) { TRACE; return 0; }
int      of_device_is_compatible(const struct device_node *device, const char *compat) { TRACE; return 1; }


/**********************
 ** linux/property.h **
 **********************/

bool device_property_read_bool(struct device *dev, const char *propname) { TRACE; return false; }
int  device_property_read_u8(struct device *dev, const char *propname, u8 *val) { TRACE; return 0; }

int device_property_read_string(struct device *dev, const char *propname,
                                const char **val) { TRACE; *val = 0; return -EINVAL; }
int  device_property_read_u32(struct device *dev, const char *propname, u32 *val) { TRACE; return 0; }


/******************************
 ** drivers/usb/dwc3/debug.h **
 ******************************/

struct dwc3;

int dwc3_debugfs_init(struct dwc3 *d){ SKIP;  return 0;  }
void dwc3_debugfs_exit(struct dwc3 *d) { SKIP; }
void dwc3_trace(void (*trace)(struct va_format *), const char *fmt, ...) { SKIP; }


/**************************
 ** linux/power_supply.h **
 **************************/

#include <linux/power_supply.h>
struct power_supply *
power_supply_register(struct device *parent,  const struct power_supply_desc *desc,
                      const struct power_supply_config *cfg) { TRACE; return 0; }
void power_supply_unregister(struct power_supply *psy) { TRACE; }
int power_supply_powers(struct power_supply *psy, struct device *dev) { TRACE; return 0; }
void *power_supply_get_drvdata(struct power_supply *psy) { TRACE; return 0; }
void power_supply_changed(struct power_supply *psy) { TRACE; }


/*********************
 ** linux/kobject.h **
 *********************/

void kobject_put(struct kobject *kobj) { TRACE; }
struct kobject *kobject_create_and_add(const char *name, struct kobject *kobj) { TRACE; return 0; }
