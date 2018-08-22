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

#define TRACE_AND_STOP \
	do { \
		lx_printf("%s not implemented\n", __func__); \
		BUG(); \
	} while (0)


/******************
 ** linux/slab.h **
 ******************/

void *kmalloc_array(size_t n, size_t size, gfp_t flags) { TRACE; return (void *)0xdeadbeaf; }


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

void __set_current_state(int state) { TRACE; }
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
char *kobject_name(const struct kobject *kobj) { TRACE; return 0; }
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

void  devres_add(struct device *dev, void *res) { TRACE; }
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
unsigned int get_next_ino(void) { TRACE; return 0; }
void init_special_inode(struct inode *i, umode_t m, dev_t d) { TRACE; }
int generic_delete_inode(struct inode *inode) { TRACE; return 0; }
void drop_nlink(struct inode *inode) { TRACE; }
void inc_nlink(struct inode *inode) { TRACE; }
void dentry_unhash(struct dentry *dentry) { TRACE; }
void iput(struct inode *i) { TRACE; }
int nonseekable_open(struct inode *inode, struct file *filp) { TRACE; return 0; }
const struct file_operations  simple_dir_operations;

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


/**********************
 ** linux/security.h **
 **********************/

void security_task_getsecid(struct task_struct *p, u32 *secid) { TRACE; }


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


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size) { TRACE; return 0; }


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


/**********************
 ** linux/inerrupt.h **
 **********************/

void tasklet_kill(struct tasklet_struct *t) { TRACE; }


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


/****************************
 ** drivers/usb/core/usb.h **
 ****************************/

#include <linux/usb.h>
#include <usb.h>

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

#include <uapi/linux/usb/charger.h>
#include <linux/usb/phy.h>

struct mii_bus *mdiobus_alloc(void) { TRACE; return 0; }
int  mdiobus_register(struct mii_bus *bus) { TRACE; return 0; }
void mdiobus_unregister(struct mii_bus *bus) { TRACE; }
void mdiobus_free(struct mii_bus *bus) { TRACE; }

int phy_init(struct phy *phy) { TRACE; return 0; }
int phy_exit(struct phy *phy) { TRACE; return 0; }
int phy_power_on(struct phy *phy) { TRACE; return 0; }
int phy_power_off(struct phy *phy) { TRACE; return 0; }
int  phy_create_lookup(struct phy *phy, const char *con_id, const char *dev_id) { TRACE; return 0; }
void phy_remove_lookup(struct phy *phy, const char *con_id, const char *dev_id) { TRACE; }

struct usb_phy *devm_usb_get_phy(struct device *dev, enum usb_phy_type type) { TRACE; return 0; }
struct usb_phy *devm_usb_get_phy_dev(struct device *dev, u8 index) { TRACE; return 0; }

struct usb_phy *usb_get_phy_dev(struct device *dev, u8 index) { TRACE; return 0; }
void   usb_put_phy(struct usb_phy *x) { TRACE; }

struct phy *devm_phy_get(struct device *dev, const char *string) { TRACE; return 0; }


/****************
 ** linux/of.h **
 ****************/

struct of_dev_auxdata;
unsigned of_usb_get_maximum_speed(struct device_node *np) { TRACE; return 0; }
unsigned of_usb_get_dr_mode(struct device_node *np) { TRACE; return 0; }
int      of_platform_populate(struct device_node *n, const struct of_device_id *of,
                              const struct of_dev_auxdata *a, struct device *d) { TRACE; return 0; }
int      of_device_is_compatible(const struct device_node *device, const char *compat) { TRACE; return 1; }


/**********************
 ** linux/property.h **
 **********************/

bool device_property_read_bool(struct device *dev, const char *propname) { TRACE; return false; }
int  device_property_read_u8(struct device *dev, const char *propname, u8 *val) { TRACE; return 0; }

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


int bus_for_each_drv(struct bus_type *bus, struct device_driver *start, void *data, int (*fn)(struct device_driver *, void *))
{
	TRACE_AND_STOP;
	return -1;
}

void device_set_of_node_from_dev(struct device *dev, const struct device *dev2)
{
	TRACE;
}

int devm_add_action(struct device *dev, void (*action)(void *), void *data)
{
	TRACE_AND_STOP;
	return -1;
}

int devm_add_action_or_reset(struct device *dev, void (*action)(void *), void *data)
{
	TRACE_AND_STOP;
	return -1;
}

char *devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...)
{
	TRACE_AND_STOP;
	return NULL;
}

void devres_close_group(struct device *dev, void *id)
{
	TRACE_AND_STOP;
}

void * devres_open_group(struct device *dev, void *id, gfp_t gfp)
{
	TRACE_AND_STOP;
	return NULL;
}

int devres_release_group(struct device *dev, void *id)
{
	TRACE_AND_STOP;
	return -1;
}

void *idr_get_next(struct idr *idp, int *nextid)
{
	TRACE_AND_STOP;
	return NULL;
}

void idr_remove(struct idr *idp, int id)
{
	TRACE_AND_STOP;
}

unsigned int jiffies_to_usecs(const unsigned long j)
{
	TRACE_AND_STOP;
	return -1;
}

struct device *kobj_to_dev(struct kobject *kobj)
{
	TRACE_AND_STOP;
	return NULL;
}

ktime_t ktime_mono_to_real(ktime_t mono)
{
	TRACE_AND_STOP;
	return -1;
}

bool mod_delayed_work(struct workqueue_struct *q, struct delayed_work *w, unsigned long v)
{
	TRACE;
	return false;
}

int  mutex_lock_killable(struct mutex *lock)
{
	TRACE_AND_STOP;
	return -1;
}

loff_t no_seek_end_llseek(struct file *f, loff_t o, int v)
{
	TRACE_AND_STOP;
	return -1;
}

int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs, unsigned int max_vecs, unsigned int flags, const struct irq_affinity *affd)
{
	TRACE;
	return 1;
}

void pci_clear_mwi(struct pci_dev *dev)
{
	TRACE_AND_STOP;
}

void pci_free_irq_vectors(struct pci_dev *dev)
{
	TRACE_AND_STOP;
}

int pci_reset_function_locked(struct pci_dev *dev)
{
	TRACE_AND_STOP;
	return -1;
}

void reinit_completion(struct completion *x)
{
	TRACE_AND_STOP;
}

size_t sg_pcopy_from_buffer(struct scatterlist *sgl, unsigned int nents, const void *buf, size_t buflen, off_t skip)
{
	TRACE_AND_STOP;
	return -1;
}

size_t sg_pcopy_to_buffer(struct scatterlist *sgl, unsigned int nents, void *buf, size_t buflen, off_t skip)
{
	TRACE_AND_STOP;
	return -1;
}

struct device_node *usb_of_get_device_node(struct usb_device *hub, int port1)
{
	TRACE;
	return NULL;
}

struct device_node *usb_of_get_interface_node(struct usb_device *udev, u8 config, u8 ifnum)
{
	TRACE_AND_STOP;
	return NULL;
}

bool usb_of_has_combined_node(struct usb_device *udev)
{
	TRACE;
	return -1;
}

void of_node_put(struct device_node *node)
{
	TRACE;
}
int claim_fiq(struct fiq_handler *f)
{
	TRACE_AND_STOP;
	return -1;
}
struct fiq_state;
void dwc_otg_fiq_nop(struct fiq_state *state)
{
	TRACE_AND_STOP;
}

struct dwc_otg_pcd;
typedef struct dwc_otg_pcd dwc_otg_pcd_t;

void dwc_otg_pcd_disconnect_us(dwc_otg_pcd_t * pcd, int no_of_usecs)
{
	TRACE_AND_STOP;
}

int dwc_otg_pcd_get_rmwkup_enable(dwc_otg_pcd_t * pcd)
{
	TRACE_AND_STOP;
	return -1;
}

void dwc_otg_pcd_initiate_srp(dwc_otg_pcd_t * pcd)
{
	TRACE_AND_STOP;
}

void dwc_otg_pcd_remote_wakeup(dwc_otg_pcd_t * pcd, int set)
{
	TRACE_AND_STOP;
}

void enable_fiq()
{
	TRACE_AND_STOP;
}

typedef spinlock_t fiq_lock_t;
void fiq_fsm_spin_lock(fiq_lock_t *lock)
{
	TRACE_AND_STOP;
}

void fiq_fsm_spin_unlock(fiq_lock_t *lock)
{
	TRACE_AND_STOP;
}

int fiq_fsm_too_late(struct fiq_state *st, int n)
{
	TRACE_AND_STOP;
	return -1;
}

int in_irq()
{
	TRACE_AND_STOP;
	return -1;
}

void local_fiq_disable()
{
	TRACE_AND_STOP;
}

void local_fiq_enable()
{
	TRACE_AND_STOP;
}

struct dwc_otg_device;
typedef struct dwc_otg_device dwc_otg_device_t;

dwc_otg_pcd_t *dwc_otg_pcd_init(dwc_otg_device_t *otg_dev)
{
	TRACE_AND_STOP;
	return NULL;
}

void dwc_otg_pcd_remove(dwc_otg_pcd_t * pcd)
{
	TRACE_AND_STOP;
}

unsigned long __phys_to_virt(phys_addr_t x)
{
	TRACE_AND_STOP;
	return -1;
}

void set_fiq_handler(void *start, unsigned int length)
{
	TRACE_AND_STOP;
}

void set_fiq_regs(struct pt_regs const *regs)
{
	TRACE_AND_STOP;
}

typedef struct platform_device dwc_bus_dev_t;

int pcd_init(dwc_bus_dev_t *_dev)
{
	TRACE;
	return 0;
}

void pcd_remove(dwc_bus_dev_t *_dev)
{
	TRACE_AND_STOP;
}

void dwc_otg_fiq_fsm(struct fiq_state *state, int num_channels)
{
	TRACE_AND_STOP;
}

unsigned char _dwc_otg_fiq_stub;
unsigned char _dwc_otg_fiq_stub_end;

bool is_acpi_device_node(struct fwnode_handle *fwnode)
{
	TRACE_AND_STOP;
	return false;
}

bool is_of_node(const struct fwnode_handle *fwnode)
{
	TRACE;
	return true;
}

const void *of_device_get_match_data(const struct device *dev)
{
	TRACE;
	return NULL;
}

int of_usb_get_phy_mode(struct device_node *np)
{
	TRACE;
	return 0;
}

int phy_calibrate(struct phy *phy)
{
	TRACE;
	return 0;
}

int phy_set_mode(struct phy *phy, enum phy_mode mode)
{
	TRACE;
	return 0;
}

int platform_device_add_properties(struct platform_device *pdev, const struct property_entry *properties)
{
	TRACE;
	return 0;
}

struct regulator *__must_check devm_regulator_get(struct device *dev, const char *id) { TRACE; return NULL; }
int disable_irq_nosync(unsigned int irq) { TRACE; return 0; }
int enable_irq(unsigned int irq) { TRACE; return 0; }
void flush_workqueue(struct workqueue_struct *wq) { TRACE; }
int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end,  gfp_t gfp_mask) { TRACE; return 0; }
void ida_simple_remove(struct ida *ida, unsigned int id) { TRACE; }
int of_alias_get_id(struct device_node *np, const char *stem) { TRACE; return 0; }
int  pm_runtime_get(struct device *dev) { TRACE; return 0; }
void pm_runtime_mark_last_busy(struct device *dev) { TRACE; }
int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val) { TRACE; return 0; }
int regmap_write(struct regmap *map, unsigned int reg, unsigned int val) { TRACE; return 0; }
int stmp_reset_block(void __iomem *addr) { TRACE; return 0; }
int usb_gadget_vbus_connect(struct usb_gadget *gadget) { TRACE; return 0; }
int usb_gadget_vbus_disconnect(struct usb_gadget *gadget) { TRACE; return 0; }
void usb_remove_phy(struct usb_phy *phy) { TRACE; }

struct ci_hdrc;

int dbg_create_files(struct ci_hdrc *ci)
{
	TRACE;
	return 0;
}

void dbg_remove_files(struct ci_hdrc *ci)
{
	TRACE;
}
