/*
 * \brief  Emulation of the Linux kernel API used by DRM
 * \author Norman Feske
 * \date   2015-08-19
 *
 * The content of this file, in particular data structures, is partially
 * derived from Linux-internal headers.
 */

#ifndef _LX_EMUL_H_
#define _LX_EMUL_H_

#include <stdarg.h>
#include <base/fixed_stdint.h>

#include <lx_emul/extern_c_begin.h>


/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100UL };

#define DEBUG_LINUX_PRINTK 1

#include <lx_emul/compiler.h>
#include <lx_emul/printf.h>
#include <lx_emul/bug.h>
#include <lx_emul/atomic.h>

static inline void atomic_or(int i, atomic_t *v) {
	v->counter = v->counter | i; }

#define smp_mb__before_atomic_inc() barrier()
#define smp_mb__before_atomic() barrier()

void atomic_set_mask(unsigned int mask, atomic_t *v);

#include <lx_emul/barrier.h>
#include <lx_emul/types.h>

typedef unsigned long kernel_ulong_t;
typedef unsigned int  u_int;

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]


/************************
 ** uapi/linux/types.h **
 ************************/

typedef __u16 __le16;
typedef __u16 __be16;
typedef __u32 __le32;
typedef __u32 __be32;
typedef __u64 __le64;
typedef __u64 __be64;


/********************
 ** linux/printk.h **
 ********************/

/* needed by drm_edid.c */
enum { DUMP_PREFIX_NONE, };

void print_hex_dump(const char *level, const char *prefix_str,
		int prefix_type, int rowsize, int groupsize,
		const void *buf, size_t len, bool ascii);

#define printk_once(fmt, ...) ({})


/*********************
 ** uapi/linux/fb.h **
 *********************/

#define KHZ2PICOS(a) (1000000000UL/(a))



/*******************
 ** linux/ctype.h **
 *******************/

#define isascii(c) (((unsigned char)(c))<=0x7f)
#define isprint(c) (isascii(c) && ((unsigned char)(c) >= 32))



/****************
 ** asm/page.h **
 ****************/

/*
 * For now, hardcoded
 */
#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE-1))


enum {
	PAGE_SHIFT = 12,
};

struct page
{
	atomic_t   _count;
	void      *addr;
	dma_addr_t paddr;
} __attribute((packed));

/* needed for agp/generic.c */
struct page *virt_to_page(void *addr);

dma_addr_t page_to_phys(struct page *page);

#include <lx_emul/bitops.h>

extern unsigned long find_next_bit(const unsigned long *addr, unsigned long
                                   size, unsigned long offset);
extern unsigned long find_next_zero_bit(const unsigned long *addr, unsigned
                                        long size, unsigned long offset);
#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)
#define find_first_zero_bit(addr, size) find_next_zero_bit((addr), (size), 0)

#define __const_hweight8(w)             \
	( (!!((w) & (1ULL << 0))) +       \
	  (!!((w) & (1ULL << 1))) +       \
	  (!!((w) & (1ULL << 2))) +       \
	  (!!((w) & (1ULL << 3))) +       \
	  (!!((w) & (1ULL << 4))) +       \
	  (!!((w) & (1ULL << 5))) +       \
	  (!!((w) & (1ULL << 6))) +       \
	  (!!((w) & (1ULL << 7))) )
#define hweight8(w)  __const_hweight8(w)
#define hweight16(w) (__const_hweight8(w)  + __const_hweight8((w)  >> 8 ))
#define hweight32(w) (hweight16(w) + hweight16((w) >> 16))
#define hweight64(w) (hweight32(w) + hweight32((w) >> 32))

#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#include <asm-generic/getorder.h>


#include <lx_emul/errno.h>

/* needed by 'virt_to_phys', which is needed by agp/generic.c */
typedef unsigned long phys_addr_t;

#include <lx_emul/string.h>

void *memchr_inv(const void *s, int c, size_t n);

#include <lx_emul/list.h>
#include <lx_emul/kernel.h>

#define SIZE_MAX (~(size_t)0)
#define U64_MAX  ((u64)~0ULL)

extern long simple_strtol(const char *,char **,unsigned int);
typedef __kernel_time_t time_t;

extern int oops_in_progress;

#define pr_debug(fmt, ...)      printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_info(fmt, ...)       printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_err(fmt, ...)        printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_warn(fmt, ...)       printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_info_once(fmt, ...)  printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_notice(fmt, ...)     printk(KERN_NOTICE fmt, ##__VA_ARGS__)


int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);

int sscanf(const char *, const char *, ...);

int vscnprintf(char *buf, size_t size, const char *fmt, va_list args);

enum { SPRINTF_STR_LEN = 64 };

#define kasprintf(gfp, fmt, ...) ({ \
		void *buf = kmalloc(SPRINTF_STR_LEN, 0); \
		sprintf(buf, fmt, __VA_ARGS__); \
		buf; \
		})

#define DIV_ROUND_UP_ULL(ll,d) \
	({ unsigned long long _tmp = (ll)+(d)-1; do_div(_tmp, d); _tmp; })

#define DIV_ROUND_CLOSEST_ULL(x, divisor)(              \
		{                                               \
		typeof(divisor) __d = divisor;                  \
		unsigned long long _tmp = (x) + (__d) / 2;      \
		do_div(_tmp, __d);                              \
		_tmp;                                           \
		}                                               \
		)

#define mult_frac(x, numer, denom) ({                   \
		typeof(x) quot = (x) / (denom);                 \
		typeof(x) rem  = (x) % (denom);                 \
		(quot * (numer)) + ((rem * (numer)) / (denom)); \
		})

extern int panic_timeout;
extern struct atomic_notifier_head panic_notifier_list;

#define min(x, y) ({                            \
		typeof(x) _min1 = (x);                  \
		typeof(y) _min2 = (y);                  \
		(void) (&_min1 == &_min2);              \
		_min1 < _min2 ? _min1 : _min2; })

#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)

/* linux/i2c.h */
#define __deprecated

#define rounddown(x, y) (                               \
		{                                               \
		typeof(x) __x = (x);                            \
		__x - (__x % (y));                              \
		}                                               \
		)

/************************
 ** linux/page-flags.h **
 ************************/

/* needed by agp/generic.c */
void SetPageReserved(struct page *page);
void ClearPageReserved(struct page *page);


/********************
 ** linux/printk.h **
 ********************/

extern void hex_dump_to_buffer(const void *buf, size_t len,
                               int rowsize, int groupsize,
                               char *linebuf, size_t linebuflen, bool ascii);


#include <lx_emul/module.h>

#define MODULE_ALIAS_MISCDEV(x)  /* needed by agp/backend.c */

/* i2c-core.c */
#define postcore_initcall(fn) void postcore_##fn(void) { fn(); }

#define symbol_get(x) ({ extern typeof(x) x __attribute__((weak)); &(x); })
#define symbol_put(x) do { } while (0)


/**************************
 ** linux/preempt_mask.h **
 **************************/

/* needed bu i2c-core.c */
bool in_atomic();


/**********************
 ** linux/irqflags.h **
 **********************/

/* needed bu i2c-core.c */
bool irqs_disabled();

void local_irq_enable();
void local_irq_disable();


/*********************
 ** linux/jiffies.h **
 *********************/

#include <lx_emul/jiffies.h>

#define time_before(a,b)	time_after(b,a)
#define time_before_eq(a,b)	time_after_eq(b,a)

#define time_in_range(a,b,c) \
	(time_after_eq(a,b) &&   \
	 time_before_eq(a,c))

extern u64 nsecs_to_jiffies64(u64 n);
unsigned int jiffies_to_usecs(const unsigned long j);


#include <lx_emul/spinlock.h>
#include <lx_emul/semaphore.h>
#include <lx_emul/mutex.h>


static inline int mutex_lock_interruptible(struct mutex *lock) {
	mutex_lock(lock);
	return 0;
}

void mutex_lock_nest_lock(struct mutex *, struct mutex *);

#define might_lock(lock) do { } while (0)


/*********************
 ** linux/rtmutex.h **
 *********************/

#define rt_mutex            mutex
#define rt_mutex_init(m)    mutex_init(m)
#define rt_mutex_lock(m)    mutex_lock(m)
#define rt_mutex_trylock(m) mutex_trylock(m)
#define rt_mutex_unlock(m)  mutex_unlock(m)


/*********************
 ** linux/ww_mutex.h **
 *********************/

struct ww_acquire_ctx { unsigned dummy; };
struct ww_class { int dummy; };
struct ww_mutex {
	bool locked;
	struct ww_acquire_ctx *ctx;
};

#define DEFINE_WW_CLASS(classname) \
	struct ww_class classname;
#define DEFINE_WW_MUTEX(mutexname, ww_class) \
	struct ww_mutex mutexname;


/******************
 ** linux/time.h **
 ******************/

#include <lx_emul/time.h>

void getrawmonotonic(struct timespec *ts);

struct timespec timespec_sub(struct timespec lhs, struct timespec rhs);
bool timespec_valid(const struct timespec *ts);
void set_normalized_timespec(struct timespec *ts, time_t sec, s64 nsec);
unsigned long get_seconds(void);
struct timespec ns_to_timespec(const s64 nsec);
s64 timespec_to_ns(const struct timespec *ts);


/*******************
 ** linux/timer.h **
 *******************/

typedef int clockid_t;

#include <lx_emul/timer.h>

#define del_singleshot_timer_sync(t) del_timer_sync(t)


#include <lx_emul/work.h>

extern bool flush_delayed_work(struct delayed_work *dwork);

extern unsigned long timespec_to_jiffies(const struct timespec *value);

#define wait_event_interruptible_timeout wait_event_timeout

#define setup_timer_on_stack setup_timer

void destroy_timer_on_stack(struct timer_list *timer);

unsigned long round_jiffies_up_relative(unsigned long j);

extern int mod_timer_pinned(struct timer_list *timer, unsigned long expires);


/*******************
 ** linux/sched.h **
 *******************/

enum { TASK_COMM_LEN = 16 };

enum { TASK_RUNNING, TASK_INTERRUPTIBLE, TASK_UNINTERRUPTIBLE };

struct mm_struct;
struct task_struct {
	struct mm_struct *mm;
	char comm[16]; /* needed by agp/generic.c, only for debug output */
	unsigned pid;
};

signed long schedule_timeout(signed long timeout);
void __set_current_state(int state);
int signal_pending(struct task_struct *p);
void schedule(void);
int wake_up_process(struct task_struct *tsk);
void io_schedule(void);

/* normally declared in linux/smp.h, included by sched.h */
int on_each_cpu(void (*func) (void *info), void *info, int wait);
#define get_cpu() 0
#define put_cpu()

/* normally defined in asm/current.h, included by sched.h */
extern struct task_struct *current;

void yield(void);

extern signed long schedule_timeout_uninterruptible(signed long timeout);

extern u64 local_clock(void);
extern bool need_resched(void);
extern int signal_pending_state(long state, struct task_struct *p);

struct pid;
extern struct pid *task_pid(struct task_struct *task);

#define cond_resched()


/************************
 ** linux/completion.h **
 ************************/

struct completion {
	unsigned done;
	void * task;
};

int __wait_completion(struct completion *work, unsigned long);
void complete(struct completion *); /* i2c-core.c */
void init_completion(struct completion *x);
void wait_for_completion(struct completion *);


/*********************
 ** linux/raid/pq.h **
 *********************/

void cpu_relax(void); /* i915_dma.c */
#define cpu_relax_lowlatency() cpu_relax()


/*******************
 ** linux/delay.h **
 *******************/

void msleep(unsigned int);
void udelay(unsigned long);
void mdelay(unsigned long);
void ndelay(unsigned long);

void usleep_range(unsigned long min, unsigned long max); /* intel_dp.c */


/*****************
 ** linux/idr.h **
 *****************/

/* needed by intel_drv.h */
struct idr { int dummy; };

void idr_init(struct idr *idp); /* i915_gem.c */
int idr_alloc(struct idr *idp, void *ptr, int start, int end, gfp_t gfp_mask);
void idr_remove(struct idr *idp, int id);
void *idr_find(struct idr *idr, int id);
void idr_destroy(struct idr *idp);
void *idr_get_next(struct idr *idp, int *nextid);
void *idr_replace(struct idr *idp, void *ptr, int id);

struct ida { int dummy; };

void ida_destroy(struct ida *ida);
void ida_init(struct ida *ida);
int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end, gfp_t gfp_mask);
void ida_remove(struct ida *ida, int id);

#define IDR_INIT(name) { .dummy = 0, }
#define DEFINE_IDR(name)	struct idr name = IDR_INIT(name)

#define idr_for_each_entry(idp, entry, id)                      \
	for (id = 0; ((entry) = idr_get_next(idp, &(id))) != NULL; ++id)

int idr_for_each(struct idr *idp, int (*fn)(int id, void *p, void *data), void *data);


/*************************
 ** linux/scatterlist.h **
 *************************/

#include <lx_emul/scatterlist.h>

struct page *sg_page_iter_page(struct sg_page_iter *piter);

void sg_free_table(struct sg_table *);
int sg_alloc_table(struct sg_table *, unsigned int, gfp_t);

void sg_mark_end(struct scatterlist *sg);

dma_addr_t sg_page_iter_dma_address(struct sg_page_iter *piter);


#include <lx_emul/kobject.h>

enum kobject_action {
	KOBJ_CHANGE,
};

int kobject_uevent_env(struct kobject *kobj, enum kobject_action action, char *envp[]);


/************************
 ** asm/memory_model.h **
 ************************/

dma_addr_t page_to_pfn(struct page *page);


/*********************
 ** linux/pagemap.h **
 *********************/

int fault_in_multipages_writeable(char __user *uaddr, int size);
int fault_in_multipages_readable(const char __user *uaddr, int size);

#define page_cache_release(page) put_page(page)

struct address_space {
	unsigned long flags;
	struct page * my_page;
};

gfp_t mapping_gfp_mask(struct address_space * mapping);
void mapping_set_gfp_mask(struct address_space *m, gfp_t mask);

gfp_t mapping_gfp_constraint(struct address_space *mapping, gfp_t gfp_mask);


/******************
 ** linux/swap.h **
 ******************/

void mark_page_accessed(struct page *);


/***************************
 ** linux/pgtable_types.h **
 ***************************/

typedef unsigned long   pgprotval_t;
struct pgprot { pgprotval_t pgprot; };
typedef struct pgprot pgprot_t;

extern pgprot_t pgprot_writecombine(pgprot_t prot);


/**********************
 ** linux/mm_types.h **
 **********************/

struct vm_area_struct {
	unsigned long  vm_start;
	unsigned long  vm_end;
	pgprot_t       vm_page_prot;
	unsigned long  vm_flags;
	void          *vm_private_data;
};

struct mm_struct { struct rw_semaphore mmap_sem; };


/**********************
 ** linux/shrinker.h **
 **********************/

struct shrink_control {
	unsigned long nr_to_scan;
};

struct shrinker {
	int (*shrink)(int nr_to_scan, gfp_t gfp_mask);
	unsigned long (*scan_objects)(struct shrinker *, struct shrink_control *sc);
	unsigned long (*count_objects)(struct shrinker *, struct shrink_control *sc); /* i915_gem.c */
	int seeks;
};

int register_shrinker(struct shrinker *);
void unregister_shrinker(struct shrinker *);

#define SHRINK_STOP (~0UL)


/****************
 ** linux/mm.h **
 ****************/

enum {
	VM_FAULT_OOM    = 0x001,
	VM_FAULT_SIGBUS = 0x002,
	VM_FAULT_NOPAGE = 0x100,
};

enum { FAULT_FLAG_WRITE = 0x1 };

enum { DEFAULT_SEEKS = 2 };

#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

#define offset_in_page(p) ((unsigned long)(p) & ~PAGE_MASK)

struct vm_fault {
	void *virtual_address;
	unsigned int flags;
};

int vm_insert_pfn(struct vm_area_struct *vma, unsigned long addr,
		unsigned long pfn);

int set_page_dirty(struct page *page);

void get_page(struct page *page);
void put_page(struct page *page);

extern unsigned long totalram_pages;


struct vm_area_struct;
struct vm_fault;

struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
};

struct file;

extern unsigned long vm_mmap(struct file *, unsigned long,
                             unsigned long, unsigned long,
                             unsigned long, unsigned long);

static inline void *page_address(struct page *page) { return page ? page->addr : 0; };

int is_vmalloc_addr(const void *x);

void free_pages(unsigned long addr, unsigned int order);

extern void kvfree(const void *addr);

extern struct page * nth_page(struct page * page, int n);

extern struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr);

pgprot_t vm_get_page_prot(unsigned long vm_flags);

#include <asm/agp.h>


/***************
 ** asm/smp.h **
 ***************/

void wbinvd();
void wbinvd_on_all_cpus();


/*********************
 ** linux/vmalloc.h **
 *********************/

/* needed by agp/generic.c */
void *vmalloc(unsigned long size);
void *vzalloc(unsigned long size);
void vfree(const void *addr);


/************************
 ** uapi/linux/const.h **
 ************************/

#define _AT(T,X) ((T)(X))


/*************************************************
 ** asm/pgtable_64_types.h, asm/pgtable_types.h **
 *************************************************/

typedef unsigned long pteval_t;


#define _PAGE_BIT_PRESENT	0	/* is present */
#define _PAGE_BIT_RW		1	/* writeable */
#define _PAGE_BIT_PWT		3	/* page write through */
#define _PAGE_BIT_PCD		4	/* page cache disabled */
#define _PAGE_BIT_PAT		7	/* on 4KB pages */

#define _PAGE_PRESENT	(_AT(pteval_t, 1) << _PAGE_BIT_PRESENT)
#define _PAGE_RW	(_AT(pteval_t, 1) << _PAGE_BIT_RW)
#define _PAGE_PWT	(_AT(pteval_t, 1) << _PAGE_BIT_PWT)
#define _PAGE_PCD	(_AT(pteval_t, 1) << _PAGE_BIT_PCD)
#define _PAGE_PAT	(_AT(pteval_t, 1) << _PAGE_BIT_PAT)


/**********************
 ** asm/cacheflush.h **
 **********************/

int set_pages_wb(struct page *page, int numpages);
int set_pages_uc(struct page *page, int numpages);


/******************
 ** linux/slab.h **
 ******************/

enum {
	SLAB_HWCACHE_ALIGN = 0x00002000ul,
};

void *kzalloc(size_t size, gfp_t flags);
void kfree(const void *);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *kmalloc(size_t size, gfp_t flags);
void *krealloc(const void *, size_t, gfp_t);
void *kmalloc_array(size_t n, size_t size, gfp_t flags);

struct kmem_cache;
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));
void kmem_cache_destroy(struct kmem_cache *);
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);
void  kmem_cache_free(struct kmem_cache *, void *);


/**********************
 ** linux/byteorder/ **
 **********************/

#include <lx_emul/byteorder.h>


/******************
 ** linux/swab.h **
 ******************/

#define swab16 __swab16


/**********************
 ** linux/highmem.h  **
 **********************/

static inline void *kmap(struct page *page) { return page_address(page); }
static inline void *kmap_atomic(struct page *page) { return kmap(page); }
static inline void kunmap(struct page *page) { }
static inline void kunmap_atomic(void *addr) { }


#include <lx_emul/gfp.h>

void __free_pages(struct page *page, unsigned int order);

#define __free_page(page) __free_pages((page), 0)

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);


/**************************************
 ** asm-generic/dma-mapping-common.h **
 **************************************/

struct dma_attrs;
struct device;

enum dma_data_direction { DMA_DATA_DIRECTION_DUMMY };

int dma_map_sg_attrs(struct device *dev, struct scatterlist *sg,
                     int nents, enum dma_data_direction dir,
                     struct dma_attrs *attrs);

#define dma_map_sg(d, s, n, r) dma_map_sg_attrs(d, s, n, r, NULL)


void dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sg,
                        int nents, enum dma_data_direction dir,
                        struct dma_attrs *attrs);

#define dma_unmap_sg(d, s, n, r) dma_unmap_sg_attrs(d, s, n, r, NULL)

extern dma_addr_t dma_map_page(struct device *dev, struct page *page,
                               unsigned long offset, size_t size,
                               enum dma_data_direction direction);
extern void dma_unmap_page(struct device *dev, dma_addr_t dma_address,
                           size_t size, enum dma_data_direction direction);

extern int dma_mapping_error(struct device *dev, dma_addr_t dma_addr);


/*********************
 ** linux/dma-buf.h **
 *********************/

struct dma_buf;


/********************
 ** linux/pm_qos.h **
 ********************/

enum { PM_QOS_CPU_DMA_LATENCY, };

struct pm_qos_request { int dummy; };

void pm_qos_remove_request(struct pm_qos_request *req);
void pm_qos_update_request(struct pm_qos_request *req, s32 new_value);
void pm_qos_add_request(struct pm_qos_request *req, int pm_qos_class, s32 value);


#define PM_QOS_DEFAULT_VALUE -1


/***********************
 ** linux/pm_wakeup.h **
 ***********************/

bool device_can_wakeup(struct device *dev);
int device_init_wakeup(struct device *dev, bool val);


/*******************
 ** linux/sysfs.h **
 *******************/

struct attribute { int dummy; };

struct attribute_group {
	struct attribute	**attrs;
};

#define __ATTRIBUTE_GROUPS(_name)                               \
	static const struct attribute_group *_name##_groups[] = {   \
		&_name##_group,                                         \
		NULL,                                                   \
	}

#define ATTRIBUTE_GROUPS(_name)                                 \
	static const struct attribute_group _name##_group = {       \
		.attrs = _name##_attrs,                                 \
	};                                                          \
__ATTRIBUTE_GROUPS(_name)

int sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name);
void sysfs_remove_link(struct kobject *kobj, const char *name);


/****************
 ** linux/pm.h **
 ****************/

#include <lx_emul/pm.h>

enum {
	PM_EVENT_QUIESCE = 0x0008,
	PM_EVENT_PRETHAW = PM_EVENT_QUIESCE,
};

#define SET_RUNTIME_PM_OPS(suspend_fn, resume_fn, idle_fn)

enum rpm_status {
	RPM_ACTIVE = 0,
	RPM_RESUMING,
	RPM_SUSPENDED,
	RPM_SUSPENDING,
};


/***********************
 ** linux/pm_domain.h **
 ***********************/

static inline int dev_pm_domain_attach(struct device *dev, bool power_on) {
	return -ENODEV; }
static inline void dev_pm_domain_detach(struct device *dev, bool power_off) {}

/********************
 ** linux/device.h **
 ********************/

struct device_driver;
struct subsys_private;

struct bus_type
{
	const char *name;

	int (*match)(struct device *dev, struct device_driver *drv);
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
	void (*shutdown)(struct device *dev);
	int (*suspend)(struct device *dev, pm_message_t state);
	int (*resume)(struct device *dev);

	const struct dev_pm_ops *pm;
	struct subsys_private *p;
};

struct device_type
{
	const struct attribute_group **groups;
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	void (*release)(struct device *dev);
};

struct dev_archdata
{
	struct dma_map_ops *dma_ops;
};

struct fwnode_handle { int dummy; };

struct device {
	struct device            *parent;
	struct kobject            kobj;
	u64                       _dma_mask_buf;
	u64                      *dma_mask;
	u64                       coherent_dma_mask;
	struct device_driver     *driver;
	void                     *drvdata;  /* not in Linux */
	const struct device_type *type;
	void                     *platform_data;
	struct dev_archdata       archdata;
	struct bus_type          *bus;
	struct device_node       *of_node;
	struct fwnode_handle     *fwnode;
};

struct device_attribute {
	struct attribute attr;
};

#define DEVICE_ATTR(_name, _mode, _show, _store) \
	struct device_attribute dev_attr_##_name = { { 0 } }

#define DEVICE_ATTR_IGNORE_LOCKDEP(_name, _mode, _show, _store) \
	struct device_attribute dev_attr_##_name = { { 0 } }

#define dev_info(  dev, format, arg...) lx_printf("dev_info: "   format , ## arg)
#define dev_warn(  dev, format, arg...) lx_printf("dev_warn: "   format , ## arg)
#define dev_WARN(  dev, format, arg...) lx_printf("dev_WARN: "   format , ## arg)
#define dev_err(   dev, format, arg...) lx_printf("dev_error: "  format , ## arg)
#define dev_notice(dev, format, arg...) lx_printf("dev_notice: " format , ## arg)
#define dev_crit(  dev, format, arg...) lx_printf("dev_crit: "   format , ## arg)

#define dev_printk(level, dev, format, arg...) \
	lx_printf("dev_printk: " format , ## arg)
#define dev_dbg(dev, format, arg...) \
	lx_printf("dev_dbg: " format, ## arg)

#define dev_err_ratelimited(dev, fmt, ...)                              \
	dev_err(dev, fmt, ##__VA_ARGS__)


struct device_driver
{
	int dummy;
	char const      *name;
	struct bus_type *bus;
	struct module   *owner;
	const struct dev_pm_ops *pm;
};

int driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);

void *dev_get_drvdata(const struct device *dev);
int dev_set_drvdata(struct device *dev, void *data);
int dev_set_name(struct device *dev, const char *name, ...);

int bus_register(struct bus_type *bus);
void bus_unregister(struct bus_type *bus);

struct device *get_device(struct device *dev);
void put_device(struct device *dev);

int device_for_each_child(struct device *dev, void *data, int (*fn)(struct device *dev, void *data));
int device_register(struct device *dev);
void device_unregister(struct device *dev);

const char *dev_name(const struct device *dev);

int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,
                     void *data, int (*fn)(struct device_driver *, void *));

int bus_for_each_dev(struct bus_type *bus, struct device *start, void *data,
                     int (*fn)(struct device *dev, void *data));

/* needed by linux/i2c.h */
struct acpi_device;

struct acpi_dev_node { struct acpi_device *companion; };


/****************
 ** linux/io.h **
 ****************/

#define writel(value, addr) (*(volatile uint32_t *)(addr) = (value))
#define readl(addr)         (*(volatile uint32_t *)(addr))

#define writel_relaxed(v, a) writel(v, a)
#define iowrite32(v, addr)	writel((v), (addr))
#define ioread32(addr)		readl(addr)

void outb(u8  value, u32 port);
void outw(u16 value, u32 port);
void outl(u32 value, u32 port);

u8  inb(u32 port);
u16 inw(u32 port);
u32 inl(u32 port);

void  iounmap(volatile void *addr);

void __iomem *ioremap(phys_addr_t offset, unsigned long size);

#define mmiowb() barrier()

/**
 * Map I/O memory write combined
 */
void *ioremap_wc(resource_size_t phys_addr, unsigned long size);

#define ioremap_nocache ioremap_wc

int arch_phys_wc_add(unsigned long base, unsigned long size);
static inline void arch_phys_wc_del(int handle) { }

phys_addr_t virt_to_phys(volatile void *address);

void memset_io(void *s, int c, size_t n);
void memcpy_toio(volatile void __iomem *dst, const void *src, size_t count);
void memcpy_fromio(void *dst, const volatile void __iomem *src, size_t count);


/*********************
 ** linux/uaccess.h **
 *********************/

enum { VERIFY_READ = 0, VERIFY_WRITE = 1 };

#define get_user(x, ptr) ({  (x)   = *(ptr); 0; })
#define put_user(x, ptr) ({ *(ptr) =  (x);   0; })

bool access_ok(int access, void *addr, size_t size);

size_t copy_from_user(void *to, void const *from, size_t len);
size_t copy_to_user(void *dst, void const *src, size_t len);

#define __copy_from_user                  copy_from_user
#define __copy_to_user                    copy_to_user
#define __copy_from_user_inatomic         copy_from_user
#define __copy_to_user_inatomic           copy_to_user
#define __copy_from_user_inatomic_nocache copy_from_user


/*************************
 ** linux/dma-mapping.h **
 *************************/

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

int dma_set_coherent_mask(struct device *dev, u64 mask);


/************************
 ** linux/io-mapping.h **
 ************************/

struct io_mapping;

void *io_mapping_map_atomic_wc(struct io_mapping *mapping, unsigned long offset);
void io_mapping_unmap_atomic(void *vaddr);

struct io_mapping *io_mapping_create_wc(resource_size_t base, unsigned long size);
void io_mapping_free(struct io_mapping *mapping);

void __iomem * io_mapping_map_wc(struct io_mapping *mapping, unsigned long offset);
void io_mapping_unmap(void __iomem *vaddr);

#include <lx_emul/mmio.h>


/********************
 ** linux/ioport.h **
 ********************/

#include <lx_emul/ioport.h>

extern struct resource iomem_resource;

int request_resource(struct resource *root, struct resource *); /* intel-gtt.c */

int release_resource(struct resource *r);  /* i915_dma.c */


/*****************
 ** linux/pci.h **
 *****************/

enum { DEVICE_COUNT_RESOURCE = 6 };

struct pci_dev {
	unsigned int devfn;
	unsigned int irq;
	struct resource resource[DEVICE_COUNT_RESOURCE];
	struct pci_bus *bus; /* i915_dma.c */
	unsigned short vendor;  /* intel-agp.c */
	unsigned short device;
	u8 hdr_type;
	bool msi_enabled;
	struct device dev; /* intel-agp.c */
	unsigned short subsystem_vendor; /* intel_display.c */
	unsigned short subsystem_device;
	u8 revision; /* i915_gem.c */
	u8 pcie_cap;
	u16 pcie_flags_reg;
	unsigned int class;
};

struct pci_device_id {
	u32 vendor, device, subvendor, subdevice, class, class_mask;
	unsigned long driver_data;
};

#include <lx_emul/pci.h>

struct pci_dev *pci_get_bus_and_slot(unsigned int bus, unsigned int devfn);
int pci_bus_alloc_resource(struct pci_bus *bus,
                           struct resource *res, resource_size_t size,
                           resource_size_t align, resource_size_t min,
                           unsigned int type_mask,
                           resource_size_t (*alignf)(void *,
                                                     const struct resource *,
                                                     resource_size_t,
                                                     resource_size_t),
                           void *alignf_data);
resource_size_t pcibios_align_resource(void *, const struct resource *,
                                       resource_size_t,
                                       resource_size_t);
int pci_set_power_state(struct pci_dev *dev, pci_power_t state);
struct pci_dev *pci_get_class(unsigned int device_class, struct pci_dev *from);
int pci_save_state(struct pci_dev *dev);

enum { PCIBIOS_MIN_MEM = 0UL };


static inline void pci_set_drvdata(struct pci_dev *pdev, void *data)
{
	pdev->dev.drvdata = data;
}

static inline int pci_set_dma_mask(struct pci_dev *dev, u64 mask)
{
	*dev->dev.dma_mask = mask;
	return 0;
}

static inline int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask)
{
	dev->dev.coherent_dma_mask = mask;
	return 0;
}

/* agp/generic.c */
#define for_each_pci_dev(d) while ((d = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, d)) != NULL)

static inline dma_addr_t pci_bus_address(struct pci_dev *pdev, int bar)
{
	lx_printf("pci_bus_address called\n");
	return (dma_addr_t)pci_resource_start(pdev, bar);
}

struct pci_dev *pci_dev_get(struct pci_dev *dev);

void __iomem __must_check *pci_map_rom(struct pci_dev *pdev, size_t *size);
void pci_unmap_rom(struct pci_dev *pdev, void __iomem *rom);


/**********************************
 ** asm-generic/pci-dma-compat.h **
 **********************************/

void pci_unmap_page(struct pci_dev *hwdev, dma_addr_t dma_address, size_t size, int direction);

dma_addr_t pci_map_page(struct pci_dev *hwdev, struct page *page, unsigned long offset, size_t size, int direction);

int pci_dma_mapping_error(struct pci_dev *pdev, dma_addr_t dma_addr);

int pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction);
void pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction);


/*****************************
 ** asm-generic/pci_iomap.h **
 *****************************/

void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long max);
void pci_iounmap(struct pci_dev *dev, void __iomem *p);


/***********************
 ** linux/irqreturn.h **
 ***********************/

#include <lx_emul/irq.h>

/********************
 ** linux/of_irq.h **
 ********************/

extern int of_irq_get(struct device_node *dev, int index);
extern int of_irq_get_byname(struct device_node *dev, const char *name);


/*********************
 ** linux/hardirq.h **
 *********************/

extern void synchronize_irq(unsigned int irq);


/************************
 ** linux/capability.h **
 ************************/

bool capable(int cap);

#define CAP_SYS_ADMIN 21


/*************************
 ** linux/agp_backend.h **
 *************************/

#include <linux/agp_backend.h>


/********************
 ** linux/vgaarb.h **
 ********************/

/*
 * needed for compiling i915_dma.c
 */

enum {
	VGA_RSRC_LEGACY_IO  = 0x01,
	VGA_RSRC_LEGACY_MEM = 0x02,
	VGA_RSRC_NORMAL_IO  = 0x04,
	VGA_RSRC_NORMAL_MEM = 0x08,
};

int vga_client_register(struct pci_dev *pdev, void *cookie,
		void (*irq_set_state)(void *cookie, bool state),
		unsigned int (*set_vga_decode)(void *cookie, bool state));

int vga_get_uninterruptible(struct pci_dev *pdev, unsigned int rsrc);

void vga_put(struct pci_dev *pdev, unsigned int rsrc);


/**********************
 ** linux/notifier.h **
 **********************/

/* needed by intel_lvds.c */

struct notifier_block;

typedef int (*notifier_fn_t)(struct notifier_block *nb, unsigned long action, void *data);

struct notifier_block { notifier_fn_t notifier_call; };

enum { NOTIFY_OK = 0x0001 };

struct atomic_notifier_head { unsigned dummy; };

extern int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh, struct notifier_block *nb);
extern int atomic_notifier_chain_register(struct atomic_notifier_head *nh, struct notifier_block *nb);


/*******************
 ** acpi/button.h **
 *******************/

int acpi_lid_open(void);
int acpi_lid_notifier_register(struct notifier_block *nb);
int acpi_lid_notifier_unregister(struct notifier_block *nb);


/****************************
 ** linux/vga_switcheroo.h **
 ****************************/

/*
 * needed for compiling i915_dma.c
 */

enum vga_switcheroo_state { VGA_SWITCHEROO_OFF, VGA_SWITCHEROO_ON };

struct vga_switcheroo_client_ops {
	void (*set_gpu_state)(struct pci_dev *dev, enum vga_switcheroo_state);
	void (*reprobe)(struct pci_dev *dev);
	bool (*can_switch)(struct pci_dev *dev);
};

int vga_switcheroo_register_client(struct pci_dev *dev,
                                   const struct vga_switcheroo_client_ops *ops,
                                   bool driver_power_control);

void vga_switcheroo_unregister_client(struct pci_dev *dev);

int vga_switcheroo_process_delayed_switch(void);


/******************
 ** acpi/video.h **
 ******************/

int acpi_video_register(void);
void acpi_video_unregister(void);


/*****************
 ** asm/ioctl.h **
 *****************/

#include <asm-generic/ioctl.h>


/****************
 ** linux/fs.h **
 ****************/

struct inode {
	const struct inode_operations *i_op;
	struct address_space *i_mapping;
};

struct file
{
	atomic_long_t f_count;
	struct inode *f_inode;
};

struct poll_table_struct;
struct inode;
struct inode_operations { void (*truncate) (struct inode *); };

/* i915_drv.c */
struct file_operations {
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	int (*release) (struct inode *, struct file *);
};

enum { PROT_READ  = 0x1, PROT_WRITE = 0x2 };

enum { MAP_SHARED = 0x1 };

loff_t noop_llseek(struct file *file, loff_t offset, int whence);

struct inode *file_inode(struct file *f);

unsigned long invalidate_mapping_pages(struct address_space *mapping,
		pgoff_t start, pgoff_t end);


/**********************
 ** linux/shmem_fs.h **
 **********************/

extern void shmem_truncate_range(struct inode *inode, loff_t start, loff_t end);

extern struct page *shmem_read_mapping_page_gfp(struct address_space *mapping,
		pgoff_t index, gfp_t gfp_mask);
extern struct page *shmem_read_mapping_page( struct address_space *mapping, pgoff_t index);


/********************
 ** linux/math64.h **
 ********************/

static inline u64 div_u64(u64 dividend, u32 divisor) { return dividend / divisor; }

static inline s64 div_s64(s64 dividend, s32 divisor)
{
	u64 quotient;
	if (dividend < 0) {
		quotient = div_u64(-dividend, abs(divisor));
		if (divisor > 0)
			quotient = -quotient;
	} else {
		quotient = div_u64(dividend, abs(divisor));
		if (divisor < 0)
			quotient = -quotient;
	}
	return quotient;
}

static inline u64 div64_u64(u64 dividend, u64 divisor)
{
	u32 high = divisor >> 32;
	u64 quot;

	if (high == 0) {
		quot = div_u64(dividend, divisor);
	} else {
		int n = 1 + fls(high);
		quot = div_u64(dividend >> n, divisor >> n);

		if (quot != 0)
			quot--;
		if ((dividend - quot * divisor) >= divisor)
			quot++;
	}

	return quot;
}

/*****************************
 ** linux/mod_devicetable.h **
 *****************************/

enum dmi_field {
	DMI_SYS_VENDOR,
	DMI_PRODUCT_NAME,
	DMI_PRODUCT_VERSION,
	DMI_BOARD_VENDOR,
	DMI_BOARD_NAME,
};

struct dmi_strmatch {
	unsigned char slot:7;
	unsigned char exact_match:1;
	char substr[79];
};

struct dmi_system_id {
	int (*callback)(const struct dmi_system_id *);
	const char *ident;
	struct dmi_strmatch matches[4];
	void *driver_data;
};

extern int dmi_check_system(const struct dmi_system_id *list);

#define DMI_MATCH(a, b)       { .slot = a, .substr = b }
#define DMI_EXACT_MATCH(a, b) { .slot = a, .substr = b, .exact_match = 1 }

#define I2C_MODULE_PREFIX "i2c:"
#define I2C_NAME_SIZE 20

struct i2c_device_id {
	char name[I2C_NAME_SIZE];
	kernel_ulong_t driver_data;	/* Data private to the driver */
};

/*********************
 ** asm/processor.h **
 *********************/

struct boot_cpu_data {
	unsigned x86_clflush_size;
};

extern struct boot_cpu_data boot_cpu_data;


/***********************
 ** linux/backlight.h **
 ***********************/

enum backlight_type {
	BACKLIGHT_RAW = 1,
	BACKLIGHT_PLATFORM,
	BACKLIGHT_FIRMWARE,
	BACKLIGHT_TYPE_MAX,
};

struct backlight_properties {
	int brightness;
	int max_brightness;
	int power;
	enum backlight_type type;
};

struct backlight_device {
	struct backlight_properties props;
};

extern void* bl_get_data(struct backlight_device *bl_dev);

struct fb_info;
struct backlight_ops {
	unsigned int options;
	int (*update_status)(struct backlight_device *);
	int (*get_brightness)(struct backlight_device *);
	int (*check_fb)(struct backlight_device *, struct fb_info *);
};

extern struct backlight_device *backlight_device_register(const char *name,
		struct device *dev, void *devdata, const struct backlight_ops *ops,
		const struct backlight_properties *props);
extern void backlight_device_unregister(struct backlight_device *bd);

/*****************
 ** linux/i2c.h **
 *****************/

struct i2c_adapter;
struct i2c_msg;

enum i2c_slave_event { DUMMY };


/****************
 ** linux/of.h **
 ****************/

int of_alias_get_id(struct device_node *np, const char *stem);


/***********************
 ** linux/of_device.h **
 ***********************/

int of_driver_match_device(struct device *dev, const struct device_driver *drv);


/******************
 ** linux/acpi.h **
 ******************/

struct kobject_uevent_env;

bool acpi_driver_match_device(struct device *dev, const struct device_driver *drv);
int acpi_device_uevent_modalias(struct device *, struct kobj_uevent_env *);

int acpi_dev_pm_attach(struct device *dev, bool power_on);
void acpi_dev_pm_detach(struct device *dev, bool power_off);

int acpi_device_modalias(struct device *, char *, int);

#define ACPI_COMPANION(dev)		(NULL)
#define ACPI_COMPANION_SET(dev, adev)	do { } while (0)

const char *acpi_dev_name(struct acpi_device *adev);
int acpi_dev_gpio_irq_get(struct acpi_device *adev, int index);


/******************
 ** linux/gpio.h **
 ******************/

/* make these flag values available regardless of GPIO kconfig options */
#define GPIOF_DIR_OUT	(0 << 0)
#define GPIOF_DIR_IN	(1 << 0)

#define GPIOF_INIT_LOW	(0 << 1)
#define GPIOF_INIT_HIGH	(1 << 1)

#define GPIOF_IN		(GPIOF_DIR_IN)
#define GPIOF_OUT_INIT_HIGH	(GPIOF_DIR_OUT | GPIOF_INIT_HIGH)

#define GPIOF_OPEN_DRAIN	(1 << 3)

/* needed bu drivers/i2c/i2c-core.c */
int gpio_get_value(unsigned int gpio);
void gpio_set_value(unsigned int gpio, int value);
int gpio_request_one(unsigned gpio, unsigned long flags, const char *label);
void gpio_free(unsigned gpio);
bool gpio_is_valid(int number);


/* needed by drivers/gpu/drm/drm_modes.c */
#include <linux/list_sort.h>


/*********************
 ** linux/cpufreq.h **
 *********************/

struct cpufreq_cpuinfo {
	unsigned int max_freq;
	unsigned int min_freq;
};

struct cpufreq_policy {
	struct cpufreq_cpuinfo cpuinfo;
};

struct cpufreq_policy *cpufreq_cpu_get(unsigned int cpu);
void cpufreq_cpu_put(struct cpufreq_policy *policy);


/********************************
 ** arch/x86/include/asm/tsc.h **
 ********************************/

extern unsigned int tsc_khz;


/**************************************
 ** drivers/platform/x86/intel_ips.h **
 **************************************/

void ips_link_to_i915_driver(void);


/**************************************
 ** drivers/gpu/drm/i915/intel_drv.h **
 **************************************/

struct drm_device;
bool intel_has_pending_fb_unpin(struct drm_device *dev);


/******************
 ** linux/kgdb.h **
 ******************/

#define in_dbg_master() (0)


/*************************
 ** asm-generic/div64.h **
 *************************/

#define do_div(n,base) ({ \
		unsigned long __base = (base); \
		unsigned long __rem; \
		__rem = ((uint64_t)(n)) % __base; \
		(n)   = ((uint64_t)(n)) / __base; \
		__rem; \
		})


/**************************************
 ** definitions needed by intel_pm.c **
 **************************************/

void trace_intel_gpu_freq_change(int);


/****************
 ** linux/fb.h **
 ****************/

#include <uapi/linux/fb.h>

enum {
	FBINFO_STATE_RUNNING = 0,
	FBINFO_STATE_SUSPENDED = 1,
	FBINFO_CAN_FORCE_OUTPUT = 0x200000,
	FBINFO_DEFAULT = 0,
};

extern int fb_get_options(const char *name, char **option);


/***************************
 ** linux vgaswitcheroo.h **
 ***************************/

void vga_switcheroo_client_fb_set(struct pci_dev *dev, struct fb_info *info);


/*******************
 ** linux/sysrq.h **
 *******************/

struct sysrq_key_op { unsigned dummy; };

int register_sysrq_key(int key, struct sysrq_key_op *op);
int unregister_sysrq_key(int key, struct sysrq_key_op *op);


/*******************
 ** linux/sysrq.h **
 *******************/

struct fence;
void fence_put(struct fence *fence);
signed long fence_wait(struct fence *fence, bool intr);


/****************************
 ** drm/drm_modeset_lock.h **
 ****************************/

#include <video/videomode.h>

extern struct ww_class crtc_ww_class;

void ww_mutex_init(struct ww_mutex *lock, struct ww_class *ww_class);
bool ww_mutex_is_locked(struct ww_mutex *lock);
int  ww_mutex_trylock(struct ww_mutex *lock);
void ww_acquire_fini(struct ww_acquire_ctx *ctx);
void ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *ww_class);
int  ww_mutex_lock_slow_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx);
void ww_mutex_unlock(struct ww_mutex *lock);
int  ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx);
void ww_mutex_lock_slow(struct ww_mutex *lock, struct ww_acquire_ctx *ctx);
int  ww_mutex_lock_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx);


struct drm_device;
struct drm_plane;
struct drm_crtc;


/************************
 ** linux/jump_label.h **
 ************************/

struct static_key { int dummy; };

#define STATIC_KEY_INIT_TRUE { .dummy = 0 }
#define STATIC_KEY_INIT_FALSE { .dummy = 0 }

extern void static_key_slow_inc(struct static_key *key);
extern void static_key_slow_dec(struct static_key *key);
extern bool static_key_false(struct static_key *key);


/**********************
 ** linux/seq_file.h **
 **********************/

struct seq_file { unsigned dummy; };

void seq_printf(struct seq_file *m, const char *fmt, ...);
void seq_puts(struct seq_file *m, const char *s);


/**********************
 ** linux/hastable.h **
 **********************/

/* needed by intel_ringbuffer */
#define DECLARE_HASHTABLE(name, bits)


/**************************
 ** linux/clk/clk-conf.h **
 **************************/

static inline int of_clk_set_defaults(struct device_node *node, bool clk_supplier) {
	return 0; }


/****************
 ** linux/of.h **
 ****************/

enum {
	OF_DYNAMIC       = 1,
	OF_DETACHED      = 2,
	OF_POPULATED     = 3,
	OF_POPULATED_BUS = 4,
};

void of_node_clear_flag(struct device_node *n, unsigned long flag);
extern int of_alias_get_highest_id(const char *stem);
static inline int of_reconfig_notifier_register(struct notifier_block *nb) {
	return -EINVAL; }
static inline int of_reconfig_notifier_unregister(struct notifier_block *nb) {
	return -EINVAL; }


/***********************************
 ** include/drm/drm_vma_manager.h **
 ***********************************/

struct drm_vma_offset_node { int dummy; };

void drm_vma_node_unmap(struct drm_vma_offset_node *node,
                        struct address_space *file_mapping);
bool drm_vma_node_has_offset(struct drm_vma_offset_node *node);
__u64 drm_vma_node_offset_addr(struct drm_vma_offset_node *node);


/************************
 ** linux/tracepoint.h **
 ************************/

void tracepoint_synchronize_unregister(void);


/*******************
 ** linux/async.h **
 *******************/

typedef u64 async_cookie_t;
typedef void (*async_func_t) (void *data, async_cookie_t cookie);

extern async_cookie_t async_schedule(async_func_t func, void *data);
extern void async_synchronize_full(void);


/*************************
 ** linux/timekeeping.h **
 *************************/

extern u64 ktime_get_raw_ns(void);

ktime_t ktime_mono_to_real(ktime_t mono);

#define ktime_sub_ns(kt, nsval) \
	({ (ktime_t){ .tv64 = (kt).tv64 - (nsval) }; })
#define ktime_to_ns(kt)                 ((kt).tv64)
#define ktime_sub(lhs, rhs) \
	({ (ktime_t){ .tv64 = (lhs).tv64 - (rhs).tv64 }; })

static inline s64 timeval_to_ns(const struct timeval *tv)
{
	return tv->tv_sec * NSEC_PER_SEC + tv->tv_usec * NSEC_PER_USEC;
}

static inline s64 ktime_to_us(const ktime_t kt) {
	return kt.tv64 / NSEC_PER_USEC; }


/*****************
 ** linux/pid.h **
 *****************/

struct pid { atomic_t count; };

static inline struct pid *get_pid(struct pid *pid)
{
	if (pid)
		atomic_inc(&pid->count);
	return pid;
}

extern void put_pid(struct pid *pid);


/***********************
 ** include/lockdep.h **
 ***********************/

#define lockdep_assert_held(l)


/***************************************
 ** arch/x86/include/asm/cpufeature.h **
 ***************************************/

#define cpu_has_pat 1


/********************
 ** linux/bitmap.h **
 ********************/

extern int bitmap_empty(const unsigned long *src, unsigned nbits);
extern void bitmap_set(unsigned long *map, unsigned int start, int len);
extern void bitmap_zero(unsigned long *dst, unsigned int nbits);
extern void bitmap_or(unsigned long *dst, const unsigned long *src1,
		const unsigned long *src2, unsigned int nbits);
extern int bitmap_weight(const unsigned long *src, unsigned int nbits);


/*****************
 ** linux/oom.h **
 *****************/

extern int unregister_oom_notifier(struct notifier_block *nb);


/************************
 ** drm/drm_mem_util.h **
 ************************/

void *drm_malloc_ab(size_t nmemb, size_t size);
void drm_free_large(void *ptr);


/************************************
 ** include/asm-generic/int-ll64.h **
 ************************************/

#define U64_C(x) x ## ULL


/********************
 ** linux/reboot.h **
 ********************/

#define SYS_RESTART 0x0001

extern int register_reboot_notifier(struct notifier_block *);
extern int unregister_reboot_notifier(struct notifier_block *);


/*****************
 ** linux/err.h **
 *****************/

int PTR_ERR_OR_ZERO(__force const void *ptr);


/*********************
 ** linux/preempt.h **
 *********************/

#define in_interrupt() 1


/***********************
 ** linux/interrupt.h **
 ***********************/

#define IRQF_SHARED 0x00000080

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
void free_irq(unsigned int, void *);


/*****************
 ** linux/pwm.h **
 *****************/

struct pwm_device { int dummy; };
unsigned int pwm_get_duty_cycle(const struct pwm_device *pwm);
int pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns);
int pwm_enable(struct pwm_device *pwm);
void pwm_disable(struct pwm_device *pwm);
struct pwm_device *pwm_get(struct device *dev, const char *con_id);
void pwm_put(struct pwm_device *pwm);


/************************
 ** drm/drm_os_linux.h **
 ************************/

#define DRM_WAIT_ON( ret, queue, timeout, condition )       \
	do {                                                    \
		DECLARE_WAITQUEUE(entry, current);                  \
		unsigned long end = jiffies + (timeout);            \
		add_wait_queue(&(queue), &entry);                   \
		                                                    \
		for (;;) {                                          \
			__set_current_state(TASK_INTERRUPTIBLE);        \
			if (condition)                                  \
			break;                                          \
			if (time_after_eq(jiffies, end)) {              \
				ret = -EBUSY;                               \
				break;                                      \
			}                                               \
			schedule_timeout((HZ/100 > 1) ? HZ/100 : 1);    \
			if (signal_pending(current)) {                  \
				ret = -EINTR;                               \
				break;                                      \
			}                                               \
		}                                                   \
		__set_current_state(TASK_RUNNING);                  \
		remove_wait_queue(&(queue), &entry);                \
	} while (0)


/*************************
 ** drm/drm_fb_helper.h **
 *************************/

struct drm_fb_helper { unsigned dummy; };


/*******************
 ** Configuration **
 *******************/

#define CONFIG_AGP                             1
#define CONFIG_AGP_INTEL                       1
#define CONFIG_BACKLIGHT_CLASS_DEVICE          1
#define CONFIG_DRM_I915                        1
#define CONFIG_DRM_I915_PRELIMINARY_HW_SUPPORT 1
#define CONFIG_EXTRA_FIRMWARE                 ""
#define CONFIG_I2C                             1
#define CONFIG_I2C_BOARDINFO                   1
#define CONFIG_OF_DYNAMIC                      0
#define CONFIG_PCI                             1


/**************************
 ** Dummy trace funtions **
 **************************/

/* normally provided by i915_trace.h */

#define trace_drm_vblank_event_delivered(...)
#define trace_drm_vblank_event_queued(...)
#define trace_drm_vblank_event(...)

#define trace_i915_context_free(...)
#define trace_i915_context_create(...)
#define trace_i915_gem_object_pread(...)
#define trace_i915_gem_object_pwrite(...)
#define trace_i915_gem_request_wait_begin(...)
#define trace_i915_gem_request_wait_end(...)
#define trace_i915_gem_object_fault(...)
#define trace_i915_gem_request_add(...)
#define trace_i915_gem_request_retire(...)
#define trace_i915_gem_ring_sync_to(...)
#define trace_i915_gem_object_change_domain(...)
#define trace_i915_vma_unbind(...)
#define trace_i915_vma_bind(...)
#define trace_i915_gem_object_clflush(...)
#define trace_i915_gem_object_create(...)
#define trace_i915_gem_object_destroy(...)
#define trace_i915_flip_complete(...)
#define trace_i915_flip_request(...)
#define trace_i915_reg_rw(...)
#define trace_i915_gem_request_complete(...)
#define trace_i915_gem_ring_flush(...)
#define trace_i915_page_table_entry_alloc(...)
#define trace_i915_page_directory_entry_alloc(...)
#define trace_i915_page_directory_pointer_entry_alloc(...)
#define trace_i915_page_table_entry_map(...)
#define trace_i915_pipe_update_end(...)
#define trace_i915_pipe_update_start(...)
#define trace_i915_pipe_update_vblank_evaded(...)
#define trace_i915_ppgtt_create(...)
#define trace_i915_ppgtt_release(...)
#define trace_i915_va_alloc(...)
#define trace_i915_gem_request_notify(...)

#define trace_i2c_read(...)
#define trace_i2c_write(...)
#define trace_i2c_reply(...)
#define trace_i2c_result(...)

#define trace_smbus_read(...)
#define trace_smbus_write(...)
#define trace_smbus_reply(...)
#define trace_smbus_result(...)

#define trace_switch_mm(...)


/**********************
 ** linux/interrupt.h **
 **********************/

struct tasklet_struct
{
	void (*func)(unsigned long);
	unsigned long data;
};

void tasklet_schedule(struct tasklet_struct *t);
void tasklet_hi_schedule(struct tasklet_struct *t);
void tasklet_kill(struct tasklet_struct *);
void tasklet_init(struct tasklet_struct *t, void (*)(unsigned long), unsigned long);


#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_H_ */
