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

#define DEBUG_LINUX_PRINTK 0
#define DEBUG_DRIVER       0

/* XXX: acquire from firmware if this becomes necessary */
#define SOC_REVISION 0x20

#include <stdarg.h>
#include <base/fixed_stdint.h>

#include <lx_emul/extern_c_begin.h>

#include <lx_emul/irq.h>      /* dependency of lx_kit/irq.h */

#include <lx_emul/atomic.h>   /* dependency of lx_emul/work.h */
#include <lx_emul/compiler.h> /* dependency of lx_emul/kernel.h */
#include <lx_emul/printf.h>   /* dependency of lx_emul/kernel.h */
#include <lx_emul/kernel.h>   /* dependency of lx_emul/jiffies.h */
#include <lx_emul/types.h>    /* dependency of lx_emul/jiffies.h */

/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100UL };          /* dependency of lx_emul/jiffies.h */


#include <lx_emul/jiffies.h>  /* dependency of lx_emul/time.h */
#include <lx_emul/time.h>     /* dependency of lx_emul/timer.h */


/*******************
 ** linux/timer.h **
 *******************/

typedef int clockid_t;        /* dependency of lx_emul/timer.h */

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

struct timer_list;

void setup_timer(struct timer_list *timer, void (*function)(unsigned long),
                 unsigned long data);

#include <lx_emul/timer.h>    /* dependency of lx_emul/work.h */
#include <lx_emul/bitops.h>   /* dependency of lx_emul/work.h */


/***********************
 ** linux/interrupt.h **
 ***********************/

#define IRQF_TRIGGER_RISING	0x00000001
#define IRQF_TRIGGER_HIGH	0x00000004

#define IRQF_ONESHOT		0x00002000

struct tasklet_struct         /* dependency of lx_kit/work.h */
{
	unsigned long state;
	void (*func)(unsigned long);
	unsigned long data;
};

struct device;

int devm_request_irq(struct device *dev, unsigned int irq,
                     irq_handler_t handler,
                     unsigned long irqflags,
                     const char *devname, void *dev_id);

int devm_request_threaded_irq(struct device *dev, unsigned int irq,
			      irq_handler_t handler, irq_handler_t thread_fn,
			      unsigned long irqflags, const char *devname,
			      void *dev_id);

#include <lx_emul/spinlock.h> /* dependency of lx_emul/work.h */
#include <lx_emul/work.h>     /* dependency of lx_kit/work.h */

#include <lx_emul/mutex.h>    /* dependency of lx_emul/kobject.h */
#include <lx_emul/kobject.h>
#include <lx_emul/completion.h>
#include <lx_emul/errno.h>
#include <lx_emul/barrier.h>
#include <lx_emul/bug.h>
#include <lx_emul/gfp.h>
#include <lx_emul/ioport.h>
#include <lx_emul/module.h>
#include <lx_emul/pm.h>
#include <lx_emul/scatterlist.h>
#include <lx_emul/semaphore.h>
#include <lx_emul/string.h>

LX_MUTEX_INIT_DECLARE(component_mutex);
#define component_mutex LX_MUTEX(component_mutex)


/**********************
 ** asm-generic/io.h **
 **********************/

static inline u32 __raw_readl(const volatile void __iomem *addr)
{
	return *(const volatile u32 __force *) addr;
}

static inline void __raw_writel(u32 b, volatile void __iomem *addr)
{
	*(volatile u32 __force *) addr = b;
}


/***************************************
 ** drivers/gpu/drm/imx/hdp/imx-hdp.h **
 ***************************************/

void imx_hdp_register_audio_driver(struct device *dev);


/********************
 ** linux/module.h **
 ********************/

#define arch_initcall(fn) module_init(fn)


/********************
 ** linux/percpu.h **
 ********************/

void __percpu *__alloc_percpu(size_t size, size_t align);
void free_percpu(void __percpu *__pdata);


/*************************
 ** linux/timekeeping.h **
 *************************/

ktime_t ktime_mono_to_real(ktime_t mono);


/********************************
 ** soc/imx8/sc/svc/misc/api.h **
 ********************************/

#include <soc/imx8/soc.h>
#include <soc/imx8/sc/types.h>

sc_err_t sc_misc_set_control(sc_ipc_t ipc, sc_rsrc_t resource,
                             sc_ctrl_t ctrl, uint32_t val);


/************************
 ** uapi/linux/types.h **
 ************************/

typedef __u16 __le16;
typedef __u32 __le32;
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


/***********************
 ** linux/sync_file.h **
 ***********************/

struct sync_file {
	struct file *file;
};

struct dma_fence;
struct dma_fence *sync_file_get_fence(int);
struct sync_file *sync_file_create(struct dma_fence *);


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

enum {
	PAGE_SHIFT = 12,
};

struct page
{
	atomic_t   _count;
	void      *addr;
	dma_addr_t paddr;
} __attribute((packed));

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

#define hweight16(w) (__const_hweight8(w)  + __const_hweight8((w)  >> 8 ))
#define hweight32(w) (hweight16(w) + hweight16((w) >> 16))
#define hweight64(w) (hweight32(w) + hweight32((w) >> 32))

#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

/* needed by 'virt_to_phys', which is needed by agp/generic.c */
typedef unsigned long phys_addr_t;

void *memchr_inv(const void *s, int c, size_t n);


/**********************
 ** linux/compiler.h **
 **********************/

static inline void __read_once_size(const volatile void *p, void *res, int size)
{
	switch (size) {
		case 1: *(__u8  *)res = *(volatile __u8  *)p; break;
		case 2: *(__u16 *)res = *(volatile __u16 *)p; break;
		case 4: *(__u32 *)res = *(volatile __u32 *)p; break;
		case 8: *(__u64 *)res = *(volatile __u64 *)p; break;
		default:
			barrier();
			__builtin_memcpy((void *)res, (const void *)p, size);
			barrier();
	}
}

/* dependency of lx_emul/list.h */
#ifdef __cplusplus
#define READ_ONCE(x) \
({                                               \
	barrier(); \
	x; \
})

#else
#define READ_ONCE(x) \
({                                               \
	union { typeof(x) __val; char __c[1]; } __u; \
	__read_once_size(&(x), __u.__c, sizeof(x));  \
	__u.__val;                                   \
})
#endif

#include <lx_emul/list.h>

#define ULLONG_MAX	(~0ULL)
#define SIZE_MAX (~(size_t)0)

#define U64_MAX  ((u64)~0ULL)

extern long simple_strtol(const char *,char **,unsigned int);


/****************************
 ** kernel/printk/printk.c **
 ****************************/

extern int oops_in_progress;

#define pr_debug(fmt, ...)      printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_info(fmt, ...)       printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_err(fmt, ...)        printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_warn(fmt, ...)       printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_notice(fmt, ...)     printk(KERN_NOTICE fmt, ##__VA_ARGS__)

int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);

int sscanf(const char *, const char *, ...);

enum { SPRINTF_STR_LEN = 64 };

#define kasprintf(gfp, fmt, ...) ({ \
		void *buf = kmalloc(SPRINTF_STR_LEN, 0); \
		sprintf(buf, fmt, __VA_ARGS__); \
		buf; \
		})

#define DIV_ROUND_CLOSEST_ULL(x, divisor)(              \
		{                                               \
		typeof(divisor) __d = divisor;                  \
		unsigned long long _tmp = (x) + (__d) / 2;      \
		do_div(_tmp, __d);                              \
		_tmp;                                           \
		}                                               \
		)

/* linux/i2c.h */
#define __deprecated

/* i2c-core.c */
#define postcore_initcall(fn) void postcore_##fn(void) { fn(); }


/**************************
 ** linux/preempt_mask.h **
 **************************/

/* needed bu i2c-core.c */
bool in_atomic();

void preempt_enable(void);
void preempt_disable(void);


/**********************
 ** linux/irqflags.h **
 **********************/

/* needed by drmP.h */
bool irqs_disabled();


/********************
 ** linux/kernel.h **
 ********************/

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

#define u64_to_user_ptr(x) ({ \
	(void __user *)(uintptr_t)x; \
})

char *kvasprintf(gfp_t, const char *, va_list);

/******************
 ** linux/kref.h **
 ******************/

struct kref;
unsigned int kref_read(const struct kref*);

LX_MUTEX_INIT_DECLARE(bridge_lock);
LX_MUTEX_INIT_DECLARE(core_lock);

#define bridge_lock LX_MUTEX(bridge_lock)
#define core_lock   LX_MUTEX(core_lock)

void might_lock(struct mutex *);


/*********************
 ** linux/rtmutex.h **
 *********************/

#define rt_mutex            mutex
#define rt_mutex_init(m)    mutex_init(m)
#define rt_mutex_lock(m)    mutex_lock(m)
#define rt_mutex_lock_nested(lock, subclass) rt_mutex_lock(lock)

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

#ifndef CONFIG_DEBUG_OBJECTS_WORK
#define INIT_WORK_ONSTACK(_work, _func) while(0) { }
static inline void destroy_work_on_stack(struct work_struct *work) { }
#endif


/*******************
 ** linux/sched.h **
 *******************/

enum {
	TASK_RUNNING         = 0x0,
	TASK_INTERRUPTIBLE   = 0x1,
	TASK_UNINTERRUPTIBLE = 0x2,
	TASK_NORMAL          = TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE,
};

#define	MAX_SCHEDULE_TIMEOUT LONG_MAX

struct task_struct {
	char comm[16]; /* only for debug output */
};

signed long schedule_timeout(signed long timeout);

void __set_current_state(int state);

int signal_pending(struct task_struct *p);

int wake_up_state(struct task_struct *tsk, unsigned int state);

/* normally defined in asm/current.h, included by sched.h */
extern struct task_struct *current;

void set_current_state(int);


/************************
 ** linux/completion.h **
 ************************/

struct completion {
	unsigned done;
	void * task;
};

long __wait_completion(struct completion *work, unsigned long);

void reinit_completion(struct completion *work);


/*********************
 ** asm/processor.h **
 *********************/
 
void cpu_relax(void);


/*******************
 ** linux/delay.h **
 *******************/

void msleep(unsigned int);

void udelay(unsigned long);
void mdelay(unsigned long);
void ndelay(unsigned long);
void usleep_range(unsigned long min, unsigned long max);


/************************
 ** asm/memory_model.h **
 ************************/

dma_addr_t page_to_pfn(struct page *page);


/*********************
 ** linux/pagemap.h **
 *********************/

struct address_space {
	unsigned long flags;
	struct page * my_page;
};

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
extern pgprot_t pgprot_decrypted(pgprot_t prot);


/**********************
 ** linux/mm_types.h **
 **********************/

struct vm_operations_struct;

struct vm_area_struct {
	unsigned long                      vm_start;
	unsigned long                      vm_end;
	pgprot_t                           vm_page_prot;
	unsigned long                      vm_flags;
	const struct vm_operations_struct *vm_ops;
	unsigned long                      vm_pgoff;
	void                               *vm_private_data;
};


/****************
 ** linux/mm.h **
 ****************/

enum {
	VM_WRITE        = 0x00000002,
	VM_MAYWRITE     = 0x00000020,
	VM_PFNMAP       = 0x00000400,
	VM_IO           = 0x00004000,
	VM_DONTEXPAND   = 0x00040000,
	VM_NORESERVE    = 0x00200000,
	VM_DONTDUMP     = 0x04000000,
};

struct vm_fault;

int set_page_dirty(struct page *page);

void put_page(struct page *page);

struct vm_area_struct;

struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	int (*fault)(struct vm_fault *vmf);
};

void free_pages(unsigned long addr, unsigned int order);

pgprot_t vm_get_page_prot(unsigned long vm_flags);
void kvfree(const void *p);

void *kvmalloc_array(size_t, size_t, gfp_t);
void unmap_mapping_range(struct address_space *, loff_t const, loff_t const, int);

unsigned long vma_pages(struct vm_area_struct *);


/******************
 ** linux/slab.h **
 ******************/

#define ARCH_KMALLOC_MINALIGN __alignof__(unsigned long long)

enum {
	SLAB_RECLAIM_ACCOUNT = 0x00020000ul,
	SLAB_PANIC           = 0x00040000ul,
};

void *kzalloc(size_t size, gfp_t flags);
void *kvzalloc(size_t size, gfp_t flags);
void kfree(const void *);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *kmalloc(size_t size, gfp_t flags);
void *krealloc(const void *, size_t, gfp_t);
void *kmalloc_array(size_t n, size_t size, gfp_t flags);
void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node);

struct kmem_cache;

struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));

void  kmem_cache_free(struct kmem_cache *, void *);

void  *kmem_cache_alloc(struct kmem_cache *, gfp_t);


/**********************
 ** linux/kmemleak.h **
 **********************/

#ifndef CONFIG_DEBUG_KMEMLEAK
static inline void kmemleak_update_trace(const void *ptr) { }
#endif

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

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);


/*************************
 ** linux/percpu-defs.h **
 *************************/

#define DECLARE_PER_CPU(type, name) \
	extern typeof(type) name

#define DEFINE_PER_CPU(type, name) \
	typeof(type) name

#define this_cpu_xchg(pcp, nval) \
({									\
	typeof(pcp) before = pcp; \
	pcp = nval; \
	before; \
})

#define this_cpu_ptr(ptr) ptr
#define this_cpu_read(val) val
#define per_cpu(var, cpu) var
#define this_cpu_cmpxchg(pcp, oval, nval) \
	cmpxchg(&pcp, oval, nval)

#define cpuhp_setup_state_nocalls(a, b, c, d) 0


/******************
 ** linux/gfp.h  **
 ******************/

#define __GFP_BITS_SHIFT (25 + IS_ENABLED(CONFIG_LOCKDEP))

#define __GFP_BITS_MASK ((__force gfp_t)((1 << __GFP_BITS_SHIFT) - 1))

static inline bool gfpflags_allow_blocking(const gfp_t gfp_flags)
{
	return !!(gfp_flags & __GFP_DIRECT_RECLAIM);
}


/*********************
 ** linux/dma-buf.h **
 *********************/

struct dma_buf;
void dma_buf_put(struct dma_buf *);

struct dma_buf {
	size_t size;
	void *priv;
	struct reservation_object *resv;
};

struct dma_buf_attachment {
	struct dma_buf *dmabuf;
};


/********************
 ** linux/pm_qos.h **
 ********************/

struct pm_qos_request { int dummy; };


/***********************
 ** linux/pm_wakeup.h **
 ***********************/

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


/****************
 ** linux/pm.h **
 ****************/

#define SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn)

#define SET_RUNTIME_PM_OPS(suspend_fn, resume_fn, idle_fn)

#define SIMPLE_DEV_PM_OPS(name, suspend_fn, resume_fn) struct dev_pm_ops name;


struct dev_pm_domain;

enum rpm_status {
	RPM_ACTIVE = 0,
	RPM_SUSPENDED,
};


/***********************
 ** linux/pm_domain.h **
 ***********************/

static inline int dev_pm_domain_attach(struct device *dev, bool power_on) {
	return -ENODEV; }
static inline void dev_pm_domain_detach(struct device *dev, bool power_off) {}


/******************
 ** linux/numa.h **
 ******************/

enum { NUMA_NO_NODE = -1 };

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

struct fwnode_operations { int dummy; };

struct fwnode_handle {
	const struct fwnode_operations *ops;
};

struct device {
	const char               *name;
	struct device            *parent;
	struct kobject            kobj;
	u64                       _dma_mask_buf;
	u64                      *dma_mask;
	u64                       coherent_dma_mask;
	struct device_driver     *driver;
	void                     *drvdata;  /* not in Linux */
	const struct device_type *type;
	void                     *platform_data;
	void                     *driver_data;
	struct dev_pm_info        power;
	struct dev_archdata       archdata;
	struct bus_type          *bus;
	struct device_node       *of_node;
	struct fwnode_handle     *fwnode;
	spinlock_t		          devres_lock;
	struct list_head	      devres_head;
};

static inline const char *dev_name(const struct device *dev)
{
	return dev->name;
}

struct device_attribute {
	struct attribute attr;
};

#define DEVICE_ATTR(_name, _mode, _show, _store) \
	struct device_attribute dev_attr_##_name = { { 0 } }

#define DEVICE_ATTR_IGNORE_LOCKDEP(_name, _mode, _show, _store) \
	struct device_attribute dev_attr_##_name = { { 0 } }

#define dev_info(  dev, format, arg...) lx_printf("dev_info: "   format , ## arg)
#define dev_warn(  dev, format, arg...) lx_printf("dev_warn: "   format , ## arg)
#define dev_err(   dev, format, arg...) lx_printf("dev_error: "  format , ## arg)

#define dev_printk(level, dev, format, arg...) \
	lx_printf("dev_printk: " format , ## arg)

#define dev_dbg(dev, format, arg...) \
	lx_printf("dev_dbg: " format, ## arg)

#define dev_err_ratelimited(dev, fmt, ...)                              \
	dev_err(dev, fmt, ##__VA_ARGS__)

struct device_driver
{
	char const      *name;
	struct bus_type *bus;
	struct module   *owner;
	const struct of_device_id *of_match_table;
	const struct acpi_device_id *acpi_match_table;
	const struct dev_pm_ops *pm;
	int            (*probe)  (struct device *dev);
};

int driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);

static inline void *dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
}

int dev_set_name(struct device *dev, const char *name, ...);

int bus_register(struct bus_type *bus);
void bus_unregister(struct bus_type *bus);

struct device *get_device(struct device *dev);
void put_device(struct device *dev);

int device_for_each_child(struct device *dev, void *data, int (*fn)(struct device *dev, void *data));

int  device_add(struct device *dev);

int device_register(struct device *dev);
void device_unregister(struct device *dev);

const char *dev_name(const struct device *dev);

int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,
                     void *data, int (*fn)(struct device_driver *, void *));

int bus_for_each_dev(struct bus_type *bus, struct device *start, void *data,
                     int (*fn)(struct device *dev, void *data));

/* needed by linux/i2c.h */
struct acpi_device;

void devm_kfree(struct device *dev, void *p);
void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);

typedef void (*dr_release_t)(struct device *dev, void *res);
typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

void devres_add(struct device *dev, void *res);

void *devres_alloc_node(dr_release_t release, size_t size, gfp_t gfp, int nid);

static inline void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp)
{
	return devres_alloc_node(release, size, gfp, NUMA_NO_NODE);
}

void devres_close_group(struct device *dev, void *id);
void *devres_open_group(struct device *dev, void *id, gfp_t gfp);
int devres_release_group(struct device *dev, void *id);
void devres_remove_group(struct device *dev, void *id);

int dev_to_node(struct device *dev);


/****************
 ** linux/io.h **
 ****************/

#define writel(value, addr) (*(volatile uint32_t *)(addr) = (value))
#define readl(addr)         (*(volatile uint32_t *)(addr))

#define readl_relaxed  readl
#define writel_relaxed(v, a) writel(v, a)

void memset_io(void *s, int c, size_t n);

void *devm_ioremap(struct device *dev, resource_size_t offset,
                   unsigned long size);

void *devm_ioremap_resource(struct device *dev, struct resource *res);


/*********************
 ** linux/uaccess.h **
 *********************/

#define get_user(x, ptr) ({  (x)   = *(ptr); 0; })
#define put_user(x, ptr) ({ *(ptr) =  (x);   0; })

size_t copy_from_user(void *to, void const *from, size_t len);
size_t copy_to_user(void *dst, void const *src, size_t len);


/*************************
 ** linux/dma-mapping.h **
 *************************/

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

static inline int dma_set_coherent_mask(struct device *dev, u64 mask)
{
	dev->coherent_dma_mask = mask;
	return 0;
}

int dma_get_sgtable_attrs(struct device *dev, struct sg_table *sgt,
                          void *cpu_addr, dma_addr_t dma_addr, size_t size,
                          unsigned long attrs);

#define dma_get_sgtable(d, t, v, h, s) dma_get_sgtable_attrs(d, t, v, h, s, 0)

void *dma_alloc_wc(struct device *dev, size_t size,
                   dma_addr_t *dma_addr, gfp_t gfp);
void dma_free_wc(struct device *dev, size_t size,
                 void *cpu_addr, dma_addr_t dma_addr);
int dma_mmap_wc(struct device *dev,
                struct vm_area_struct *vma,
                void *cpu_addr, dma_addr_t dma_addr,
                size_t size);

void *dmam_alloc_coherent(struct device *dev, size_t size,
                          dma_addr_t *dma_handle, gfp_t gfp);

void dmam_free_coherent(struct device *dev, size_t size, void *vaddr,
                        dma_addr_t dma_handle);


/********************
 ** linux/ioport.h **
 ********************/

#define IORESOURCE_BITS 0x000000ff


/********************
 ** linux/of_irq.h **
 ********************/

extern int of_irq_get(struct device_node *dev, int index);
extern int of_irq_get_byname(struct device_node *dev, const char *name);

struct irq_data *irq_get_irq_data(unsigned int);


/*****************
 ** linux/irq.h **
 *****************/

#include <linux/irqhandler.h>

struct irq_chip {
	struct device	*parent_device;
	const char	*name;

	void		(*irq_ack)(struct irq_data *data);
	void		(*irq_mask)(struct irq_data *data);
	void		(*irq_mask_ack)(struct irq_data *data);
	void		(*irq_unmask)(struct irq_data *data);
	void		(*irq_eoi)(struct irq_data *data);
};

struct irq_data {
	unsigned long    hwirq;
	struct irq_chip *chip;
	void            *chip_data;
};

void irq_chip_eoi_parent(struct irq_data *data);

struct irq_common_data {
	void *handler_data;
};

struct irq_desc;

void handle_level_irq(struct irq_desc *desc);


void irqd_set_trigger_type(struct irq_data *, u32);

void irq_set_chip_and_handler(unsigned int, struct irq_chip *,
                              irq_flow_handler_t);

int irq_set_chip_data(unsigned int irq, void *data);

extern struct irq_chip dummy_irq_chip;

void handle_simple_irq(struct irq_desc *);

enum {
	IRQ_NOAUTOEN		= (1 << 12),
};

void irq_set_chained_handler_and_data(unsigned int irq,
                                      irq_flow_handler_t handle,
                                      void *data);

void irq_set_status_flags(unsigned int irq, unsigned long set);


/************************
 ** linux/capability.h **
 ************************/

#define CAP_SYS_ADMIN 21

bool capable(int);


/**********************
 ** linux/notifier.h **
 **********************/

struct notifier_block { int dummy; };


/****************
 ** linux/fs.h **
 ****************/

struct inode {
	const struct inode_operations *i_op;
	struct address_space *i_mapping;
};

struct file
{
	atomic_long_t         f_count;
	struct inode         *f_inode;
	struct address_space *f_mapping;
	void                 *private_data;
};

struct poll_table_struct;

/* drm_gem_cma_helper.h */
struct file_operations {
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*open) (struct inode *, struct file *);
	int (*release) (struct inode *, struct file *);
};

loff_t noop_llseek(struct file *file, loff_t offset, int whence);


/*******************
 ** linux/types.h **
 *******************/
struct rcu_head { int dummy; };

typedef unsigned long irq_hw_number_t;


/*********************
 ** linux/seqlock.h **
 *********************/

typedef unsigned seqlock_t;

void seqlock_init (seqlock_t *);

void write_seqlock(seqlock_t *);
void write_sequnlock(seqlock_t *);

unsigned read_seqbegin(const seqlock_t *);
unsigned read_seqretry(const seqlock_t *, unsigned);


/*************************
 ** linux/reservation.h **
 *************************/

struct reservation_object {
	struct dma_fence *fence_excl;
};

struct dma_fence * reservation_object_get_excl_rcu(struct reservation_object *);


/**********************
 ** linux/shmem_fs.h **
 **********************/

extern struct page *shmem_read_mapping_page( struct address_space *mapping, pgoff_t index);
struct file *shmem_file_setup(const char *, loff_t, unsigned long);


/*****************
 ** linux/i2c.h **
 *****************/

enum i2c_slave_event { DUMMY };


/****************
 ** linux/of.h **
 ****************/

int of_alias_get_id(struct device_node *np, const char *stem);
struct device_node *of_node_get(struct device_node *node);
void of_node_put(struct device_node *);

int of_property_match_string(const struct device_node *, const char *,
                             const char *);
int of_property_read_u32_index(const struct device_node *, const char *, u32,
                               u32 *);

#define for_each_child_of_node(parent, child) \
	for (child = of_get_next_child(parent, NULL); child != NULL; \
	     child = of_get_next_child(parent, child))

struct property {
	const char      * name;
	void            * value;
	struct property * next;
};

struct device_node {
	const char           *name;
	const char           *full_name;
	struct fwnode_handle  fwnode;
	struct property      *properties;
	struct device_node   *parent;
};

bool of_device_is_available(const struct device_node *device);

int of_device_is_compatible(const struct device_node *device,
                            const char *compat);
extern struct device_node *of_get_next_child(const struct device_node *node,
                                             struct device_node *prev);
struct device_node *of_get_parent(const struct device_node *node);
const void *of_get_property(const struct device_node *node, const char *name, int *lenp);
struct device_node *of_parse_phandle(const struct device_node *np,
                                     const char *phandle_name, int index);
bool of_property_read_bool(const struct device_node *np, const char *propname);
int of_property_read_string(const struct device_node *np, const char *propname,
                            const char **out_string);
int of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value);

bool is_of_node(const struct fwnode_handle *fwnode);

#define to_of_node(__fwnode)						\
	({								\
		typeof(__fwnode) __to_of_node_fwnode = (__fwnode);	\
									\
		is_of_node(__to_of_node_fwnode) ?			\
			container_of(__to_of_node_fwnode,		\
				     struct device_node, fwnode) :	\
			NULL;						\
	})


/***********************
 ** linux/of_device.h **
 ***********************/

const void *of_device_get_match_data(const struct device *dev);

const struct of_device_id *of_match_device(const struct of_device_id *matches,
                                           const struct device *dev);


/******************
 ** linux/acpi.h **
 ******************/

bool acpi_driver_match_device(struct device *dev, const struct device_driver *drv);
int acpi_device_uevent_modalias(struct device *, struct kobj_uevent_env *);

int acpi_device_modalias(struct device *, char *, int);

#define ACPI_COMPANION(dev)		(NULL)

const char *acpi_dev_name(struct acpi_device *adev);

int acpi_dev_gpio_irq_get(struct acpi_device *adev, int index);

void acpi_device_clear_enumerated(struct acpi_device *);

int acpi_reconfig_notifier_register(struct notifier_block *);
int acpi_reconfig_notifier_unregister(struct notifier_block *);

/******************
 ** linux/gpio.h **
 ******************/

/* make these flag values available regardless of GPIO kconfig options */

#define GPIOF_DIR_OUT	(0 << 0)

#define GPIOF_DIR_IN	(1 << 0)

#define GPIOF_INIT_HIGH	(1 << 1)

#define GPIOF_IN		(GPIOF_DIR_IN)

#define GPIOF_OUT_INIT_HIGH	(GPIOF_DIR_OUT | GPIOF_INIT_HIGH)

#define GPIOF_OPEN_DRAIN	(1 << 3)

void gpio_free(unsigned gpio);
int gpio_get_value(unsigned int gpio);
bool gpio_is_valid(int number);
int gpio_request_one(unsigned gpio, unsigned long flags, const char *label);
void gpio_set_value(unsigned int gpio, int value);


/* needed by drivers/gpu/drm/drm_modes.c */
#include <linux/list_sort.h>


/**********************
 ** drm/drm_device.h **
 **********************/

struct drm_device;


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


/****************
 ** linux/fb.h **
 ****************/

extern int fb_get_options(const char *name, char **option);


/****************************
 ** drm/drm_modeset_lock.h **
 ****************************/

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


/************************
 ** linux/jump_label.h **
 ************************/

struct static_key { int dummy; };

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


/*********************
 ** linux/clk/clk.h **
 *********************/

struct clk
{
	const char * name;
	unsigned long rate;
};

struct clk *devm_clk_get(struct device *dev, const char *id);

void clk_disable_unprepare(struct clk *clk);
struct clk *clk_get_parent(struct clk *clk);
unsigned long clk_get_rate(struct clk * clk);
bool clk_is_match(const struct clk *p, const struct clk *q);
int clk_prepare_enable(struct clk *clk);
int clk_set_parent(struct clk *clk, struct clk *parent);
int clk_set_rate(struct clk *clk, unsigned long rate);


/**************************
 ** linux/clk/clk-conf.h **
 **************************/

static inline int of_clk_set_defaults(struct device_node *node, bool clk_supplier) {
	return 0; }


/****************
 ** linux/of.h **
 ****************/

enum {
	OF_POPULATED     = 3,
};

void of_node_clear_flag(struct device_node *n, unsigned long flag);

extern int of_alias_get_highest_id(const char *stem);

static inline int of_reconfig_notifier_register(struct notifier_block *nb) {
	return -EINVAL; }
static inline int of_reconfig_notifier_unregister(struct notifier_block *nb) {
	return -EINVAL; }


/*********************
 ** drm/drm_lease.h **
 *********************/

struct drm_file;
bool drm_lease_held(struct drm_file *, int);
bool _drm_lease_held(struct drm_file *, int);

uint32_t drm_lease_filter_crtcs(struct drm_file *, uint32_t);


/********************
 ** linux/rwlock.h **
 ********************/

typedef unsigned long rwlock_t;
void rwlock_init(rwlock_t *);
void read_lock(rwlock_t *);
void read_unlock(rwlock_t *);
void write_lock(rwlock_t *);
void write_unlock(rwlock_t *);


/************************
 ** linux/tracepoint.h **
 ************************/

#define EXPORT_TRACEPOINT_SYMBOL(name)

void tracepoint_synchronize_unregister(void);


/*******************
 ** linux/ktime.h **
 *******************/

#define ktime_to_ns(kt) kt

#define ktime_sub_ns(kt, nsval)		((kt) - (nsval))

static inline s64 timeval_to_ns(const struct timeval *tv)
{
	return ((s64) tv->tv_sec * NSEC_PER_SEC) +
		tv->tv_usec * NSEC_PER_USEC;
}


/***********************
 ** include/lockdep.h **
 ***********************/

#define MAX_LOCKDEP_SUBCLASSES 8UL

#define lockdep_assert_held(l) do { (void)(l); } while (0)


/********************
 ** linux/bitmap.h **
 ********************/

extern void bitmap_set(unsigned long *map, unsigned int start, int len);

#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))

#define small_const_nbits(nbits) \
	(__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG)

static inline int bitmap_empty(const unsigned long *src, unsigned nbits)
{
	if (small_const_nbits(nbits))
		return ! (*src & BITMAP_LAST_WORD_MASK(nbits));

	return find_first_bit(src, nbits) == nbits;
}

static inline int bitmap_full(const unsigned long *src, unsigned int nbits)
{
	if (small_const_nbits(nbits))
		return ! (~(*src) & BITMAP_LAST_WORD_MASK(nbits));

	return find_first_zero_bit(src, nbits) == nbits;
}

static inline void bitmap_fill(unsigned long *dst, unsigned int nbits)
{
	unsigned int nlongs = BITS_TO_LONGS(nbits);
	if (!small_const_nbits(nbits)) {
		unsigned int len = (nlongs - 1) * sizeof(unsigned long);
		memset(dst, 0xff,  len);
	}
	dst[nlongs - 1] = BITMAP_LAST_WORD_MASK(nbits);
}


/*****************
 ** linux/err.h **
 *****************/

int PTR_ERR_OR_ZERO(__force const void *ptr);


/*********************
 ** linux/preempt.h **
 *********************/

#define in_interrupt() 1


/********************
 * linux/property.h *
 ********************/

struct property_entry;
struct property_entry * property_entries_dup(const struct property_entry *);

int device_add_properties(struct device *, const struct property_entry *);
void device_remove_properties(struct device *);

int device_property_read_u32(struct device *, const char *, u32 *);


/**********************
 ** linux/rcupdate.h **
 **********************/

#define kfree_rcu(ptr, offset) kfree(ptr)

#define rcu_access_pointer(p) p
#define rcu_assign_pointer(p, v) p = v
#define rcu_dereference(p) p
#define rcu_dereference_protected(p, c) p
#define rcu_dereference_raw(p) p
#define rcu_pointer_handoff(p) (p)

void call_rcu(struct rcu_head *, void (*)(struct rcu_head *));


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


/*********************
 ** drm/drm_sysfs.h **
 *********************/

void drm_sysfs_hotplug_event(struct drm_device *);


/* XXX */
void put_unused_fd(unsigned int);
void fd_install(unsigned int, struct file *);
void fput(struct file *);
enum { O_CLOEXEC = 0xbadaffe };
int get_unused_fd_flags(unsigned);


/*********************
 ** linux/irqdesc.h **
 *********************/

struct irq_desc {
	struct irq_common_data irq_common_data;
	struct irq_data        irq_data;
	irq_flow_handler_t     handle_irq;
};

int generic_handle_irq(unsigned int);

static inline struct irq_chip *irq_desc_get_chip(struct irq_desc *desc)
{
	return desc->irq_data.chip;
}

static inline void *irq_desc_get_handler_data(struct irq_desc *desc)
{
	return desc->irq_common_data.handler_data;
}


/*******************
 ** Configuration **
 *******************/

#define CONFIG_I2C                             1
#define CONFIG_I2C_BOARDINFO                   1
#define CONFIG_BASE_SMALL                      0
#define CONFIG_IRQ_DOMAIN                      1
#define CONFIG_MMU                             1
#define CONFIG_OF                              1
#define CONFIG_VIDEOMODE_HELPERS               1


/**************************
 ** Dummy trace funtions **
 **************************/

#define trace_drm_vblank_event_delivered(...)
#define trace_drm_vblank_event_queued(...)
#define trace_drm_vblank_event(...)

#define trace_i2c_read(...)
#define trace_i2c_write(...)
#define trace_i2c_reply(...)
#define trace_i2c_result(...)

#define trace_dma_fence_enable_signal(...) while (0) { }
#define trace_dma_fence_init(...)           while (0) { }
#define trace_dma_fence_signaled(...)       while (0) { }
#define trace_dma_fence_destroy(...)        while (0) { }
#define trace_dma_fence_wait_start(...)     while (0) { }
#define trace_dma_fence_wait_end(...)       while (0) { }


/***********************
 ** linux/interrupt.h **
 ***********************/

void enable_irq(unsigned int);
void disable_irq(unsigned int);
int disable_irq_nosync(unsigned int);


/*****************************
 ** linux/platform_device.h **
 *****************************/

struct platform_device {
	char            *name;
	int              id;
	struct device    dev;
	u32              num_resources;
	struct resource *resource;
};

#define to_platform_device(x) container_of((x), struct platform_device, dev)

int platform_device_add(struct platform_device *pdev);
int platform_device_add_data(struct platform_device *pdev, const void *data, size_t size);
struct platform_device *platform_device_alloc(const char *name, int id);
int platform_device_put(struct platform_device *pdev);
int platform_device_register(struct platform_device *);
void platform_device_unregister(struct platform_device *);

static inline void *platform_get_drvdata(const struct platform_device *pdev)
{
	return pdev->dev.driver_data;
}

int platform_get_irq(struct platform_device *dev, unsigned int num);

int platform_get_irq_byname(struct platform_device *dev, const char *name);

struct resource *platform_get_resource(struct platform_device *pdev,
                                       unsigned int type, unsigned int num);

struct resource *platform_get_resource_byname(struct platform_device *, unsigned int, const char *);

static inline void platform_set_drvdata(struct platform_device *pdev,
					void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	struct device_driver driver;
};

int platform_driver_register(struct platform_driver *);
void platform_driver_unregister(struct platform_driver *);

#define module_driver(__driver, __register, __unregister, ...) \
static int __init __driver##_init(void) \
{ \
	return __register(&(__driver) , ##__VA_ARGS__); \
} \
module_init(__driver##_init); \
static void __exit __driver##_exit(void) \
{ \
	__unregister(&(__driver) , ##__VA_ARGS__); \
} \
module_exit(__driver##_exit);

#define module_platform_driver(__platform_driver) \
	module_driver(__platform_driver, platform_driver_register, \
			platform_driver_unregister)

/***********************
 ** uapi/linux/uuid.h **
 ***********************/

typedef struct {
	__u8 b[16];
} uuid_le;


/************************
 ** linux/cpuhotplug.h **
 ************************/

enum cpuhp_state {
	CPUHP_RADIX_DEAD = 29,
};


/*******************
 ** linux/sizes.h **
 *******************/

enum {
	SZ_4K = 0x00001000,
	SZ_16K = 0x00004000,
};


#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_H_ */
