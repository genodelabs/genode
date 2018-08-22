/*
 * \brief  Emulation of the Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2012-01-28
 *
 * The content of this file, in particular data structures, is partially
 * derived from Linux-internal headers.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL_H_
#define _LX_EMUL_H_

#include <stdarg.h>
#include <base/fixed_stdint.h>

#include <lx_emul/extern_c_begin.h>

#define DEBUG_COMPLETION   0
#define DEBUG_DMA          0
#define DEBUG_DRIVER       0
#define DEBUG_KREF         0
#define DEBUG_LINUX_PRINTK 0
#define DEBUG_PCI          0
#define DEBUG_SKB          0
#define DEBUG_SLAB         0
#define DEBUG_TIMER        0
#define DEBUG_THREAD       0
#define DEBUG_TRACE        0

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(4,4,3)

#define KBUILD_MODNAME "mod-noname"

/* lx_printf */
#include <lx_emul/printf.h>


/*******************
 ** linux/sizes.h **
 *******************/

#define SZ_256K 0x40000


/*****************
 ** linux/bcd.h **
 *****************/

#define bin2bcd(x) ((((x) / 10) << 4) + (x) % 10)


/***************
 ** asm/bug.h **
 ***************/

#include <lx_emul/bug.h>

/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100UL };


/******************
 ** asm/atomic.h **
 ******************/

#include <lx_emul/atomic.h>


/*******************
 ** linux/types.h **
 *******************/

#include <lx_emul/types.h>

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u64 __be64;

typedef int clockid_t;

typedef unsigned int  u_int;
typedef unsigned char u_char;
typedef unsigned long u_long;
typedef uint8_t       u_int8_t;
typedef uint16_t      u_int16_t;
typedef uint32_t      u_int32_t;

typedef unsigned short ushort;

typedef unsigned long phys_addr_t;

typedef unsigned __poll_t;
typedef unsigned slab_flags_t;


/**********************
 ** linux/compiler.h **
 **********************/

#include <lx_emul/barrier.h>
#include <lx_emul/compiler.h>

#define __must_hold(x)

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

#ifdef __cplusplus
#define READ_ONCE(x) \
({ \
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


/*********************
 ** uapi/linux/uuid **
 *********************/

typedef struct { __u8 b[16]; } uuid_le;


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

#include <lx_emul/byteorder.h>

struct __una_u16 { u16 x; } __attribute__((packed));
struct __una_u32 { u32 x; } __attribute__((packed));
struct __una_u64 { u64 x; } __attribute__((packed));

void put_unaligned_le16(u16 val, void *p);
void put_unaligned_le32(u32 val, void *p);
u32  get_unaligned_le32(const void *p);

u64 get_unaligned_le64(const void *p);

#define put_unaligned put_unaligned_le32
#ifdef __LP64__
#define get_unaligned get_unaligned_le64
#else
#define get_unaligned get_unaligned_le32
#endif


/****************
 ** asm/page.h **
 ****************/

/*
 * For now, hardcoded to x86_32
 */
#define PAGE_SIZE 4096

enum {
	PAGE_SHIFT = 12,
};

struct page
{
	void      *virt;
	dma_addr_t phys;
};


/*******************************
 ** linux/errno.h and friends **
 *******************************/

#include <lx_emul/errno.h>

enum {
	EISDIR        = 21,
	EXFULL        = 52,
	ERESTART      = 53,
	ESHUTDOWN     = 58,
	ECOMM         = 70,
	EIDRM         = 82,
	ENOSR         = 211,
};


/********************
 ** linux/kernel.h **
 ********************/

#include <lx_emul/kernel.h>

char *kasprintf(gfp_t gfp, const char *fmt, ...);
int   kstrtouint(const char *s, unsigned int base, unsigned int *res);

#define clamp(val, min, max) ({           \
	typeof(val) __val = (val);              \
	typeof(min) __min = (min);              \
	typeof(max) __max = (max);              \
	(void) (&__val == &__min);              \
	(void) (&__val == &__max);              \
	__val = __val < __min ? __min: __val;   \
	__val > __max ? __max: __val; })

#define rounddown(x, y) (                 \
{                                         \
	typeof(x) __x = (x);                    \
	__x - (__x % (y));                      \
})

long simple_strtoul(const char *cp, char **endp, unsigned int base);
long simple_strtol(const char *,char **,unsigned int);

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int vsprintf(char *buf, const char *fmt, va_list args);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int sscanf(const char *, const char *, ...);
int scnprintf(char *buf, size_t size, const char *fmt, ...);


/*********************
 ** linux/preempt.h **
 *********************/

bool in_softirq(void);

/*********************
 ** linux/jiffies.h **
 *********************/

#include <lx_emul/jiffies.h>


/*********************
 ** linux/cpumask.h **
 *********************/

static inline unsigned num_online_cpus(void) { return 1U; }


/******************
 ** linux/log2.h **
 ******************/

int ilog2(u32 n);
int rounddown_pow_of_two(u32 n);


/********************
 ** linux/kdev_t.h **
 ********************/

#define MINORBITS 20
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))


/********************
 ** linux/printk.h **
 ********************/

#define pr_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_warn_once pr_warning
#define pr_warn_ratelimited(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
bool printk_ratelimit();
#define printk_ratelimited(fmt, ...) printk(fmt, ##__VA_ARGS__)
#define printk_once(fmt, ...) printk(fmt, ##__VA_ARGS__)


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

#define BIT(nr)       (1UL << (nr))
#define BITS_PER_LONG (__SIZEOF_LONG__ * 8)
#define BIT_MASK(nr)  (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)  ((nr) / BITS_PER_LONG)

int ffs(int x);
int fls(int x);

#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/fls64.h>

#define ffz(x)  __ffs(~(x))

static inline unsigned fls_long(unsigned long l)
{
	if (sizeof(l) == 4)
		return fls(l);
	return fls64(l);
}

#define test_and_clear_bit(nr, addr) \
	__test_and_clear_bit(nr, (volatile unsigned long *)(addr))
#define test_and_set_bit(nr, addr) \
	__test_and_set_bit(nr, (volatile unsigned long *)(addr))
#define set_bit(nr, addr) \
	__set_bit(nr, (volatile unsigned long *)(addr))
#define clear_bit(nr, addr) \
	__clear_bit(nr, (volatile unsigned long *)(addr))

extern unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
extern unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
static inline unsigned long find_next_zero_bit_le(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	return find_next_zero_bit(addr, size, offset);
}

#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)

#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size)); \
	     (bit) < (size);                    \
	     (bit) = find_next_bit((addr), (size), (bit) + 1))


/*****************************************
 ** asm-generic/bitops/const_hweight.h **
 *****************************************/

#define hweight32(w) __builtin_popcount((unsigned)w)


/********************
 ** linux/string.h **
 ********************/

#include <lx_emul/string.h>

int strtobool(const char *, bool *);

#define kbasename(path) (path)

int match_string(const char * const *array, size_t n, const char *string);

int strcmp(const char *a,const char *b);


/*****************
 ** linux/nls.h **
 *****************/

enum utf16_endian { UTF16_LITTLE_ENDIAN = 1, };

int utf16s_to_utf8s(const wchar_t *pwcs, int len, enum utf16_endian endian, u8 *s, int maxlen);


/******************
 ** linux/init.h **
 ******************/

#include <lx_emul/module.h>

#define __initconst


/********************
 ** linux/module.h **
 ********************/

#define MODULE_SOFTDEP(x)

#define module_driver(__driver, __register, __unregister, ...) \
	static int __init __driver##_init(void)                      \
	{                                                            \
		return __register(&(__driver) , ##__VA_ARGS__);            \
	}                                                            \
	module_init(__driver##_init);                                \
	static void __exit __driver##_exit(void)                     \
	{                                                            \
		__unregister(&(__driver) , ##__VA_ARGS__);                 \
	}                                                            \
	module_exit(__driver##_exit);

#define postcore_initcall(fn) void postcore_##fn(void) { fn(); }


/******************
 ** linux/slab.h **
 ******************/

enum {
	ARCH_KMALLOC_MINALIGN = 128, 
};

void *kzalloc(size_t size, gfp_t flags);
void  kfree(const void *);
void *kmalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *dma_malloc(size_t size);
void  dma_free(void *ptr);

struct kmem_cache;
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));
void  kmem_cache_destroy(struct kmem_cache *);
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);
void  kmem_cache_free(struct kmem_cache *, void *);
void *kmalloc_array(size_t n, size_t size, gfp_t flags);


/**********************
 ** linux/spinlock.h **
 **********************/

#include <lx_emul/spinlock.h>


/*******************
 ** linux/mutex.h **
 *******************/

#include <lx_emul/mutex.h>


/*******************
 ** linux/rwsem.h **
 *******************/

#include <lx_emul/semaphore.h>


/******************
 ** linux/time.h **
 ******************/

#include <lx_emul/time.h>


/*******************
 ** linux/timer.h **
 *******************/

#include <lx_emul/timer.h>

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

/*******************
 ** linux/delay.h **
 *******************/

void msleep(unsigned int msecs);
void udelay(unsigned long usecs);
void mdelay(unsigned long usecs);
void usleep_range(unsigned long min, unsigned long max);


/***********************
 ** linux/workquque.h **
 ***********************/

#include <lx_emul/work.h>

enum {
	WORK_STRUCT_PENDING_BIT = 0,
};


#define work_data_bits(work) ((unsigned long *)(&(work)->data))

#define work_pending(work) \
        test_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))

#define delayed_work_pending(w) \
         work_pending(&(w)->work)

#define create_freezable_workqueue(name) \
	alloc_workqueue("%s", WQ_FREEZABLE | \
	                      WQ_MEM_RECLAIM, 1, (name))

extern struct workqueue_struct *system_power_efficient_wq;


/******************
 ** linux/wait.h **
 ******************/

#define wait_event_interruptible_timeout(wq, condition, timeout) \
({                        \
	_wait_event_timeout(wq, condition, timeout); \
	1;                      \
})


/*******************
 ** linux/sched.h **
 *******************/

enum { MAX_SCHEDULE_TIMEOUT = (~0U >> 1) };

struct task_struct {
	char comm[16]; /* needed by usb/core/devio.c, only for debug output */
};
signed long schedule_timeout(signed long);
signed long schedule_timeout_uninterruptible(signed long timeout);
void yield(void);

extern struct task_struct *current;

void cpu_relax(void);

#define memalloc_noio_save() 0
#define memalloc_noio_restore(x)


/*********************
 ** linux/kthread.h **
 *********************/

int kthread_should_stop(void);
int kthread_stop(struct task_struct *k);

struct task_struct *kthread_run(int (*)(void *), void *, const char *, ...);


/**********************
 ** linux/notifier.h **
 **********************/

enum {
	NOTIFY_DONE      = 0x0000,
	NOTIFY_OK        = 0x0001,
	NOTIFY_STOP_MASK = 0x8000,
};

struct notifier_block {
	int (*notifier_call)(struct notifier_block *, unsigned long, void *);
	struct notifier_block *next;
	int                    priority;
};

struct raw_notifier_head
{
	struct notifier_block *head;
};

struct atomic_notifier_head {
	spinlock_t lock;
	struct notifier_block *head;
};

struct blocking_notifier_head {
	struct rw_semaphore rwsem;
	struct notifier_block *head;
};

#define BLOCKING_NOTIFIER_INIT(name) { \
	.rwsem = __RWSEM_INITIALIZER((name).rwsem), .head = NULL }
#define BLOCKING_NOTIFIER_HEAD(name) \
	struct blocking_notifier_head name = BLOCKING_NOTIFIER_INIT(name)

int blocking_notifier_chain_register(struct blocking_notifier_head *nh, struct notifier_block *nb);
int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh, struct notifier_block *nb);
int blocking_notifier_call_chain(struct blocking_notifier_head *nh, unsigned long val, void *v);
int atomic_notifier_chain_register(struct atomic_notifier_head *nh, struct notifier_block *nb);
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh, struct notifier_block *nb);


/*************************
 ** linux/scatterlist.h **
 *************************/

#include <lx_emul/scatterlist.h>

size_t sg_pcopy_from_buffer(struct scatterlist *sgl, unsigned int nents, const void *buf, size_t buflen, off_t skip);
size_t sg_pcopy_to_buffer(struct scatterlist *sgl, unsigned int nents, void *buf, size_t buflen, off_t skip);


/*********************
 ** linux/kobject.h **
 *********************/

#include <lx_emul/kobject.h>


/*******************
 ** linux/sysfs.h **
 *******************/

struct attribute {
	const char *name;
	mode_t      mode;
};


struct attribute_group {
	const char        *name;
	struct attribute **attrs;
};

#define __ATTR(_name,_mode,_show,_store) { \
	.attr  = {.name = #_name, .mode = _mode }, \
	.show  = _show, \
	.store = _store, \
}

#define __ATTR_NULL { .attr = { .name = NULL } }
#define __ATTR_RO(name) __ATTR_NULL
#define __ATTR_RW(name) __ATTR_NULL
int  sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);
void sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp);
int sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name);
void sysfs_remove_link(struct kobject *kobj, const char *name);


/****************
 ** linux/pm.h **
 ****************/

#include <lx_emul/pm.h>

#define PMSG_AUTO_SUSPEND ((struct pm_message) \
                           { .event = PM_EVENT_AUTO_SUSPEND, })

struct pm_ops_dummy {};

#define SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn)
#define SET_RUNTIME_PM_OPS(suspend_fn, resume_fn, idle_fn)
#define SIMPLE_DEV_PM_OPS(name, suspend_fn, resume_fn) struct pm_ops_dummy name;


/************************
 ** linux/pm_runtime.h **
 ************************/

int  pm_runtime_get(struct device *dev);
bool pm_runtime_active(struct device *dev);
int  pm_runtime_set_active(struct device *dev);
void pm_suspend_ignore_children(struct device *dev, bool enable);
void pm_runtime_enable(struct device *dev);
void pm_runtime_disable(struct device *dev);
void pm_runtime_allow(struct device *dev);
void pm_runtime_forbid(struct device *dev);
void pm_runtime_set_suspended(struct device *dev);
void pm_runtime_get_noresume(struct device *dev);
void pm_runtime_put_noidle(struct device *dev);
void pm_runtime_use_autosuspend(struct device *dev);
int  pm_runtime_put_sync_autosuspend(struct device *dev);
void pm_runtime_no_callbacks(struct device *dev);
void pm_runtime_set_autosuspend_delay(struct device *dev, int delay);
int  pm_runtime_get_sync(struct device *dev);
int  pm_runtime_put_sync(struct device *dev);
int  pm_runtime_put(struct device *dev);
int  pm_runtime_barrier(struct device *dev);
void pm_runtime_mark_last_busy(struct device *dev);


/***********************
 ** linux/pm_wakeup.h **
 ***********************/

int  device_init_wakeup(struct device *dev, bool val);
int  device_wakeup_enable(struct device *dev);
bool device_may_wakeup(struct device *dev);
int  device_set_wakeup_enable(struct device *dev, bool enable);
bool device_can_wakeup(struct device *dev);


/********************
 ** linux/pm_qos.h **
 ********************/

enum { PM_QOS_FLAG_NO_POWER_OFF = 1 };
enum  dev_pm_qos_req_type { DEV_PM_QOS_FLAGS = 3 };

struct dev_pm_qos_request { unsigned dummy; };

int dev_pm_qos_add_request(struct device *dev, struct dev_pm_qos_request *req, enum dev_pm_qos_req_type type, s32 value);
int dev_pm_qos_remove_request(struct dev_pm_qos_request *req);
int dev_pm_qos_expose_flags(struct device *dev, s32 value);


/******************
 ** linux/acpi.h **
 ******************/

struct fwnode_handle;

bool is_acpi_device_node(struct fwnode_handle *fwnode);
#define ACPI_PTR(_ptr)  (NULL)


/********************
 ** linux/device.h **
 ********************/

#define dev_info(dev, format, arg...)      lx_printf("dev_info: "  format, ## arg)
#define dev_warn(dev, format, arg...)      lx_printf("dev_warn: "  format, ## arg)
#define dev_WARN(dev, format, arg...)      lx_printf("dev_WARN: "  format, ## arg)
#define dev_err( dev, format, arg...)      lx_printf("dev_error: " format, ## arg)
#define dev_dbg_ratelimited(dev, format, arg...)

#define dev_WARN_ONCE(dev, condition, format, arg...) ({\
	lx_printf("dev_WARN_ONCE: "  format, ## arg); \
	!!condition; \
})

#if DEBUG_LINUX_PRINTK
#define dev_dbg(dev, format, arg...)  lx_printf("dev_dbg: " format, ## arg)
#define CONFIG_DYNAMIC_DEBUG 1
#else
#define dev_dbg( dev, format, arg...)
#endif

#define dev_printk(level, dev, format, arg...) \
	lx_printf("dev_printk: " format, ## arg)

#define dev_warn_ratelimited(dev, format, arg...) \
	lx_printf("dev_warn_ratelimited: " format "\n", ## arg)

#define dev_warn_once(dev, format, ...) \
	lx_printf("dev_warn_ratelimited: " format "\n", ##__VA_ARGS__)

enum {
	BUS_NOTIFY_ADD_DEVICE = 0x00000001,
	BUS_NOTIFY_DEL_DEVICE = 0x00000002,
};

struct device;
struct device_driver;

struct bus_type {
	const char *name;
	struct device_attribute *dev_attrs;
	const struct attribute_group **dev_groups;
	const struct attribute_group **drv_groups;

	int (*match)(struct device *dev, struct device_driver *drv);
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
};

struct device_driver {
	char const      *name;
	struct bus_type *bus;
	struct module   *owner;
	const char      *mod_name;
	const struct of_device_id   *of_match_table;
	const struct acpi_device_id *acpi_match_table;
	int            (*probe)  (struct device *dev);
	int            (*remove) (struct device *dev);

	const struct dev_pm_ops *pm;
};

struct device_type {
	const char *name;
	const struct attribute_group **groups;
	void      (*release)(struct device *dev);
	int       (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	char     *(*devnode)(struct device *dev, mode_t *mode, kuid_t *, kgid_t *);
	const struct dev_pm_ops *pm;
};

struct class
{
	const char *name;
	char *(*devnode)(struct device *dev, mode_t *mode);
};

struct device {
	const char                    *name;
	struct device                 *parent;
	struct kobject                 kobj;
	const struct device_type      *type;
	struct device_driver          *driver;
	void                          *platform_data;
	u64                           _dma_mask_buf;
	u64                           *dma_mask; /* needed by usb/hcd.h */
	u64                            coherent_dma_mask; /* omap driver */
	unsigned long                  dma_pfn_offset;
	struct dev_pm_info             power;
	dev_t                          devt;
	const struct attribute_group **groups;
	void (*release)(struct device *dev);
	struct bus_type               *bus;
	struct class                  *class;
	void                          *driver_data;
	struct device_node            *of_node;
	struct fwnode_handle          *fwnode;
	struct device_dma_parameters  *dma_parms;
	unsigned                       ref;
};

struct device_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *dev, struct device_attribute *attr,
	                char *buf);
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,
	                 const char *buf, size_t count);
};

struct driver_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device_driver *driver, char *buf);
	ssize_t (*store)(struct device_driver *driver, const char *buf,
	                 size_t count);
};

struct lock_class_key { int dummy; };

#define DEVICE_ATTR(_name, _mode, _show, _store) \
struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)

#define DEVICE_ATTR_RO(_name) \
struct device_attribute dev_attr_##_name = __ATTR_RO(_name)

#define DEVICE_ATTR_RW(_name) \
struct device_attribute dev_attr_##_name = __ATTR_RW(_name)

#define DRIVER_ATTR_RW(_name) \
struct driver_attribute driver_attr_##_name = __ATTR_RW(_name)

#define DRIVER_ATTR_RO(_name) \
struct driver_attribute driver_attr_##_name = __ATTR_RO(_name)

void       *dev_get_drvdata(const struct device *dev);
int         dev_set_drvdata(struct device *dev, void *data);
int         dev_set_name(struct device *dev, const char *name, ...);
const char *dev_name(const struct device *dev);
int         dev_to_node(struct device *dev);
void        set_dev_node(struct device *dev, int node);

struct device *device_create(struct class *cls, struct device *parent, dev_t devt, void *drvdata, const char *fmt, ...);
int  device_add(struct device *dev);
void device_destroy(struct class *cls, dev_t devt);
int  device_register(struct device *dev);
void device_unregister(struct device *dev);
void device_lock(struct device *dev);
int  device_trylock(struct device *dev);
void device_unlock(struct device *dev);
void device_del(struct device *dev);
void device_initialize(struct device *dev);
int  device_attach(struct device *dev);
int  device_is_registered(struct device *dev);
int  device_bind_driver(struct device *dev);
void device_release_driver(struct device *dev);
void device_enable_async_suspend(struct device *dev);
void device_set_wakeup_capable(struct device *dev, bool capable);
int  device_create_file(struct device *device, const struct device_attribute *entry);
void device_remove_file(struct device *dev, const struct device_attribute *attr);
int device_for_each_child(struct device *dev, void *data,
                          int (*fn)(struct device *dev, void *data));
void           put_device(struct device *dev);
struct device *get_device(struct device *dev);
int  driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);
int  driver_attach(struct device_driver *drv);
int  driver_create_file(struct device_driver *driver, const struct driver_attribute *attr);
void driver_remove_file(struct device_driver *driver, const struct driver_attribute *attr);
struct device *bus_find_device(struct bus_type *bus, struct device *start, void *data, int (*match)(struct device *dev, void *data));
int  bus_register(struct bus_type *bus);
void bus_unregister(struct bus_type *bus);
int  bus_register_notifier(struct bus_type *bus, struct notifier_block *nb);
int  bus_unregister_notifier(struct bus_type *bus, struct notifier_block *nb);
int  bus_for_each_dev(struct bus_type *bus, struct device *start, void *data, int (*fn)(struct device *dev, void *data));
struct class *__class_create(struct module *owner, const char *name, struct lock_class_key *key);
#define class_create(owner, name) \
({ \
	static struct lock_class_key __key; \
	__class_create(owner, name, &__key); \
})
void class_destroy(struct class *cls);
void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);
struct resource;
void *devm_ioremap_resource(struct device *dev, struct resource *res);
void *dev_get_platdata(const struct device *dev);
void device_set_of_node_from_dev(struct device *dev, const struct device *dev2);


/*****************************
 ** linux/platform_device.h **
 *****************************/

#define module_platform_driver(__platform_driver) \
        module_driver(__platform_driver, platform_driver_register, \
                      platform_driver_unregister)

enum { PLATFORM_DEVID_AUTO = -2 };

struct platform_device {
	char *           name;
	int              id;
	struct device    dev;
	u32              num_resources;
	struct resource *resource;
};

struct platform_device *platform_device_alloc(const char *name, int id);
int platform_device_add(struct platform_device *pdev);
int platform_device_del(struct platform_device *pdev);
int platform_device_put(struct platform_device *pdev);
int platform_device_register(struct platform_device *);
void platform_device_unregister(struct platform_device *);
void *platform_get_drvdata(const struct platform_device *pdev);
void platform_set_drvdata(struct platform_device *pdev, void *data);
int platform_device_add_data(struct platform_device *pdev, const void *data, size_t size);
int platform_device_add_resources(struct platform_device *pdev, const struct resource *res, unsigned int num);

int platform_get_irq(struct platform_device *, unsigned int);
int platform_get_irq_byname(struct platform_device *, const char *);
struct resource *platform_get_resource(struct platform_device *, unsigned, unsigned);
struct resource *platform_get_resource_byname(struct platform_device *, unsigned int, const char *);

struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	struct device_driver driver;
	const struct platform_device_id *id_table;
};

int platform_driver_register(struct platform_driver *);
void platform_driver_unregister(struct platform_driver *);

struct property_entry;
int platform_device_add_properties(struct platform_device *pdev, const struct property_entry *properties);

#define to_platform_driver(drv) (container_of((drv), struct platform_driver, \
                                 driver))

#define to_platform_device(x) container_of((x), struct platform_device, dev)


/*********************
 ** linux/dmapool.h **
 *********************/

struct dma_pool;

struct dma_pool *dma_pool_create(const char *, struct device *, size_t, size_t, size_t);
void  dma_pool_destroy(struct dma_pool *);
void *dma_pool_alloc(struct dma_pool *, gfp_t, dma_addr_t *);
void  dma_pool_free(struct dma_pool *, void *, dma_addr_t);
void *dma_pool_zalloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle);
void *dma_alloc_coherent(struct device *, size_t, dma_addr_t *, gfp_t);
void  dma_free_coherent(struct device *, size_t, void *, dma_addr_t);


/*************************
 ** linux/dma-mapping.h **
 *************************/

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

static inline int dma_set_coherent_mask(struct device *dev, u64 mask)
{
	dev->coherent_dma_mask = mask;
	return 0;
}

static inline int dma_set_mask(struct device *dev, u64 mask) { *dev->dma_mask = mask; return 0; }

static inline int dma_coerce_mask_and_coherent(struct device *dev, u64 mask)
{
	dma_set_mask(dev, mask);
	return dma_set_coherent_mask(dev, mask);
}

static inline int dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	dma_set_mask(dev, mask);
	dma_set_coherent_mask(dev, mask);
	return 0;
}

void *dma_zalloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag);


/*********************
 ** linux/uaccess.h **
 *********************/

enum { VERIFY_READ = 0, VERIFY_WRITE = 1 };

bool access_ok(int access, void *addr, size_t size);

size_t copy_to_user(void *dst, void const *src, size_t len);


/*****************
 ** linux/dmi.h **
 *****************/

struct dmi_system_id;

static inline int dmi_check_system(const struct dmi_system_id *list) { return 0; }
static inline const char * dmi_get_system_info(int field) { return NULL; }


/******************
 ** linux/poll.h **
 ******************/

typedef struct poll_table_struct { int dummy; } poll_table;

struct file;

void poll_wait(struct file *, wait_queue_head_t *, poll_table *);


/********************
 ** linux/statfs.h **
 ********************/

loff_t default_llseek(struct file *file, loff_t offset, int origin);


/****************
 ** linux/fs.h **
 ****************/

struct dentry;
struct file_operations;

struct path {
	struct dentry *dentry;
};

struct file {
	u64                           f_version;
	loff_t                        f_pos;
	struct dentry                *f_dentry;
	struct path                   f_path;
	unsigned int                  f_flags;
	fmode_t                       f_mode;
	const struct file_operations *f_op;
	void                         *private_data;
};

struct inode;

struct file_operations {
	struct module *owner;
	int          (*open) (struct inode *, struct file *);
	ssize_t      (*read) (struct file *, char __user *, size_t, loff_t *);
	loff_t       (*llseek) (struct file *, loff_t, int);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	int          (*release) (struct inode *, struct file *);
};

struct inode {
	void *i_private;
};
unsigned iminor(const struct inode *inode);
static inline struct file_operations const * fops_get(struct file_operations const *fops) { return fops; }
void fops_put(struct file_operations const *);
loff_t noop_llseek(struct file *file, loff_t offset, int origin);
int register_chrdev(unsigned int major, const char *name, const struct file_operations *fops);
void unregister_chrdev(unsigned int major, const char *name);
ssize_t simple_read_from_buffer(void __user *to, size_t count, loff_t *ppos, const void *from, size_t available);

#define replace_fops(f, fops) \
do {    \
        struct file *__file = (f); \
        fops_put(__file->f_op); \
        BUG_ON(!(__file->f_op = (fops))); \
} while(0)

loff_t no_seek_end_llseek(struct file *, loff_t, int);


/**********************
 ** linux/seq_file.h **
 **********************/

struct seq_file { int dummy; };


/*****************
 ** linux/gfp.h **
 *****************/

#include <lx_emul/gfp.h>

enum {
	GFP_NOIO     = GFP_LX_DMA,
};

unsigned long get_zeroed_page(gfp_t gfp_mask);
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
void free_pages(unsigned long addr, unsigned int order);
#define free_page(addr) free_pages((addr), 0)


/********************
 * linux/debugfs.h **
 ********************/

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent);
struct dentry *debugfs_create_file(const char *name, mode_t mode, struct dentry *parent, void *data, const struct file_operations *fops);
void debugfs_remove(struct dentry *dentry);
static inline void debugfs_remove_recursive(struct dentry *dentry) { }

struct debugfs_regset32 {};


/************************
 ** linux/page-flags.h **
 ************************/

bool is_highmem(void *);
#define PageHighMem(__p) is_highmem(page_zone(__p))


/****************
 ** linux/mm.h **
 ****************/

struct zone *page_zone(const struct page *page);
int    is_vmalloc_addr(const void *x);


/**********************
 ** asm-generic/io.h **
 **********************/

#include <lx_emul/mmio.h>

void *ioremap(phys_addr_t addr, unsigned long size);
void  iounmap(volatile void *addr);

#define ioremap_nocache ioremap

void outb(u8  value, u32 port);
void outw(u16 value, u32 port);
void outl(u32 value, u32 port);

u8  inb(u32 port);
u16 inw(u32 port);
u32 inl(u32 port);

void native_io_delay(void);

static inline void outb_p(u8  value, u32 port) { outb(value, port); native_io_delay(); }
static inline void outl_p(u32 value, u32 port) { outl(value, port); native_io_delay(); }
static inline u8  inb_p(u32 port) { u8  ret = inb(port); native_io_delay(); return ret; }
static inline u32 inl_p(u32 port) { u32 ret = inl(port); native_io_delay(); return ret; }

#define readl_relaxed  readl
#define writel_relaxed writel

#define ioread32(addr)      readl(addr)
#define iowrite32(v, addr)  writel((v), (addr))


/********************
 ** linux/ioport.h **
 ********************/

#include <lx_emul/ioport.h>


/***********************
 ** linux/irqreturn.h **
 ***********************/

#include <lx_emul/irq.h>

#define IRQ_RETVAL(x) ((x) != IRQ_NONE)

int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id);


/***********************
 ** linux/interrupt.h **
 ***********************/

enum {
	IRQF_SHARED = 0x00000080,
};

void local_irq_enable(void);
void local_irq_disable(void);
int  request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
void free_irq(unsigned int, void *);
int disable_irq_nosync(unsigned int irq);
int enable_irq(unsigned int irq);
void disable_irq(unsigned int irq);


/*********************
 ** linux/hardirq.h **
 *********************/

int  in_irq();
bool in_interrupt(void);
void synchronize_irq(unsigned int irq);


/***************
 ** asm/fiq.h **
 ***************/

struct fiq_handler {
	char const *name;
};

struct pt_regs;

int claim_fiq(struct fiq_handler *f);
void set_fiq_handler(void *start, unsigned int length);
void set_fiq_regs(struct pt_regs const *regs);
void enable_fiq();


/*****************
 ** linux/pci.h **
 *****************/

extern struct  bus_type pci_bus_type;

enum { DEVICE_COUNT_RESOURCE = 6 };

struct pci_dev {
	unsigned int devfn;
	unsigned int irq;
	struct resource resource[DEVICE_COUNT_RESOURCE];
	struct pci_bus *bus;
	unsigned short vendor;
	unsigned short device;
	unsigned short subsystem_vendor;
	unsigned short subsystem_device;
	unsigned int   class;
	u8             revision;
	u8             pcie_cap;
	u16            pcie_flags_reg;
	struct         device dev;
	unsigned       current_state;
};

struct pci_fixup {
	u16 vendor;             /* You can use PCI_ANY_ID here of course */
	u16 device;             /* You can use PCI_ANY_ID here of course */
	u32 class;              /* You can use PCI_ANY_ID here too */
	unsigned int class_shift;       /* should be 0, 8, 16 */
	void (*hook)(struct pci_dev *dev);
};

#include <lx_emul/pci.h>
#include <linux/mod_devicetable.h>

#define PCI_DEVICE_CLASS(dev_class,dev_class_mask) \
	.class = (dev_class), .class_mask = (dev_class_mask), \
	.vendor = PCI_ANY_ID, .device = PCI_ANY_ID, \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

#define PCI_DEVICE(vend,dev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

#define PCI_VDEVICE(vendor, device)             \
        PCI_VENDOR_ID_##vendor, (device),       \
        PCI_ANY_ID, PCI_ANY_ID, 0, 0

#define DECLARE_PCI_FIXUP_CLASS_FINAL(vendor, device, class,            \
                                         class_shift, hook)             \
                     void __pci_fixup_##hook(void *data) { hook(data); }

#define for_each_pci_dev(d) printk("for_each_pci_dev called\n"); while(0)

enum { PCI_ROM_RESOURCE = 6 };

int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask);
int pci_set_power_state(struct pci_dev *dev, pci_power_t state);
void pci_clear_mwi(struct pci_dev *dev);

struct irq_affinity;

int pci_reset_function_locked(struct pci_dev *dev);
int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs, unsigned int max_vecs, unsigned int flags, const struct irq_affinity *affd);
void pci_free_irq_vectors(struct pci_dev *dev);
int pci_irq_vector(struct pci_dev *dev, unsigned int nr);

static inline int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
                                        unsigned int max_vecs, unsigned int flags)
{
	return pci_alloc_irq_vectors_affinity(dev, min_vecs, max_vecs, flags, NULL);
}

enum {
	PCI_IRQ_MSI  = (1 << 1),
	PCI_IRQ_MSIX = (1 << 2),
};


/**********************
 ** linux/irqflags.h **
 **********************/

unsigned long local_irq_save(unsigned long flags);
unsigned long local_irq_restore(unsigned long flags);
void local_fiq_disable();
void local_fiq_enable();
unsigned smp_processor_id(void);


/*************************
 ** linux/dma-direction **
 *************************/

enum dma_data_direction
{
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE     = 1,
	DMA_FROM_DEVICE   = 2
};


/*************************
 ** linux/dma-mapping.h **
 *************************/

struct dma_attrs;

dma_addr_t dma_map_single_attrs(struct device *dev, void *ptr, size_t size, enum dma_data_direction dir, struct dma_attrs *attrs);

void dma_unmap_single_attrs(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir, struct dma_attrs *attrs);

void dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir, struct dma_attrs *attrs);

dma_addr_t dma_map_page(struct device *dev, struct page *page, size_t offset, size_t size, enum dma_data_direction dir);

int dma_map_sg_attrs(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir, struct dma_attrs *attrs);

#define dma_map_single(d, a, s, r) dma_map_single_attrs(d, a, s, r, NULL)
#define dma_unmap_single(d, a, s, r) dma_unmap_single_attrs(d, a, s, r, NULL)
#define dma_map_sg(d, s, n, r) dma_map_sg_attrs(d, s, n, r, NULL)
#define dma_unmap_sg(d, s, n, r) dma_unmap_sg_attrs(d, s, n, r, NULL)

void dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size, enum dma_data_direction direction);
int dma_mapping_error(struct device *dev, dma_addr_t dma_addr);
static inline int is_device_dma_capable(struct device *dev) { return *dev->dma_mask; }


/******************
 ** linux/stat.h **
 ******************/

enum {
	S_IRUGO = 444,
};

#include <uapi/linux/stat.h>


/*********************
 ** linux/utsname.h **
 *********************/

#define __NEW_UTS_LEN 64

struct new_utsname {
	char sysname[__NEW_UTS_LEN + 1];
	char release[__NEW_UTS_LEN + 1];
};

struct new_utsname *init_utsname(void);


/************************
 ** linux/completion.h **
 ************************/

#include <lx_emul/completion.h>

struct completion { unsigned int done; void *task; };
long  __wait_completion(struct completion *work, unsigned long timeout);;
void reinit_completion(struct completion *x);


/******************
 ** linux/list.h **
 ******************/

#include <lx_emul/list.h>


/********************
 ** linux/random.h **
 ********************/

void add_device_randomness(const void *, unsigned int);


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size);
void vfree(void *addr);


/**********************
 ** linux/inerrupt.h **
 **********************/

extern struct workqueue_struct *tasklet_wq;

struct tasklet_struct
{
	void (*func)(unsigned long);
	unsigned long data;
	unsigned      pending;
};

void tasklet_schedule(struct tasklet_struct *t);
void tasklet_hi_schedule(struct tasklet_struct *t);
void tasklet_init(struct tasklet_struct *t, void (*)(unsigned long), unsigned long);


/*****************
 ** linux/idr.h **
 *****************/

#define DEFINE_IDA(name) struct ida name;
struct ida { };

int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end,
                   gfp_t gfp_mask);
void ida_simple_remove(struct ida *ida, unsigned int id);

struct idr { int dummy; };

int idr_alloc(struct idr *idp, void *ptr, int start, int end, gfp_t gfp_mask);
void idr_remove(struct idr *idp, int id);
void idr_destroy(struct idr *);
void *idr_get_next(struct idr *idp, int *nextid);

#define idr_for_each_entry(idp, entry, id)                      \
	for (id = 0; ((entry) = idr_get_next(idp, &(id))) != NULL; ++id)

#define IDR_INIT(name) { .dummy = 0, }
#define DEFINE_IDR(name)	struct idr name = IDR_INIT(name)


/*******************************
 ** uapi/linux/usbdevice_fs.h **
 *******************************/

enum { USBDEVFS_HUB_PORTINFO };

struct usbdevfs_hub_portinfo
{
	char nports;
	char port [127];
};


/********************
 ** linux/bitmap.h **
 ********************/

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))


/******************
 ** linux/phy.h  **
 ******************/

struct phy;

enum phy_mode {
	PHY_MODE_INVALID,
	PHY_MODE_USB_HOST,
	PHY_MODE_USB_DEVICE,
	PHY_MODE_USB_OTG,
	PHY_MODE_SGMII,
	PHY_MODE_10GKR,
	PHY_MODE_UFS_HS_A,
	PHY_MODE_UFS_HS_B,
};

int         phy_init(struct phy *phy);
int         phy_exit(struct phy *phy);
struct phy *phy_get(struct device *dev, const char *string);
void        phy_put(struct phy *phy);
int         phy_power_on(struct phy *phy);
int         phy_power_off(struct phy *phy);

struct phy *devm_phy_get(struct device *dev, const char *string);
struct phy *devm_of_phy_get(struct device *dev, struct device_node *np, const char *con_id);

int  phy_create_lookup(struct phy *phy, const char *con_id, const char *dev_id);
void phy_remove_lookup(struct phy *phy, const char *con_id, const char *dev_id);

int phy_set_mode(struct phy *phy, enum phy_mode mode);
int phy_calibrate(struct phy *phy);


/************************
 ** linux/usb/gadget.h **
 ************************/

struct usb_ep { };
struct usb_request { };
struct usb_gadget { struct device dev; };

int usb_gadget_vbus_connect(struct usb_gadget *gadget);
int usb_gadget_vbus_disconnect(struct usb_gadget *gadget);


/*********************************
 ** linux/usb/usb_phy_generic.h **
 *********************************/

struct usb_phy_generic_platform_data
{
	int  type;
	int  gpio_reset;
};


/****************
 ** linux/of.h **
 ****************/

struct property {
	const char      * name;
	void            * value;
	struct property * next;
};

struct device_node {
	struct property * properties;
	struct device * dev;
};

struct of_phandle_args {
	struct device_node *np;
	int args_count;
	uint32_t args[32];
};

bool of_property_read_bool(const struct device_node *np, const char *propname);
void of_node_put(struct device_node *node);
int  of_device_is_compatible(const struct device_node *device, const char *);
int  of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value);
bool is_of_node(const struct fwnode_handle *fwnode);
const void *of_get_property(const struct device_node *node, const char *name, int *lenp);
int of_parse_phandle_with_args(struct device_node *np, const char *list_name, const char *cells_name, int index, struct of_phandle_args *out_args);
const struct of_device_id *of_match_device(const struct of_device_id *matches, const struct device *dev);
extern struct platform_device *of_find_device_by_node(struct device_node *np);
struct property *of_find_property(const struct device_node *np, const char *name, int *lenp);

#define of_match_ptr(ptr) NULL
#define for_each_available_child_of_node(parent, child) while (0)

const void *of_device_get_match_data(const struct device *dev);
int of_alias_get_id(struct device_node *np, const char *stem);


/*********************
 ** linux/of_gpio.h **
 *********************/

int of_get_named_gpio(struct device_node *, const char *, int);


/*************************
 ** linux/of_platform.h **
 *************************/

struct of_dev_auxdata;

int of_platform_populate(struct device_node *, const struct of_device_id *,
                         const struct of_dev_auxdata *, struct device *);


/******************
 ** linux/gpio.h **
 ******************/

enum { GPIOF_OUT_INIT_HIGH = 0x2 };

bool gpio_is_valid(int);
int devm_gpio_request_one(struct device *dev, unsigned gpio, unsigned long flags, const char *label);


/*****************
 ** linux/clk.h **
 *****************/

struct clk *devm_clk_get(struct device *dev, const char *id);
int    clk_prepare_enable(struct clk *);
void   clk_disable_unprepare(struct clk *);


/**********************
 ** linux/property.h **
 **********************/

struct property_entry { const char *name; };

int device_property_read_string(struct device *dev, const char *propname, const char **val);
bool device_property_read_bool(struct device *dev, const char *propname);
int  device_property_read_u8(struct device *dev, const char *propname, u8 *val);
int  device_property_read_u32(struct device *dev, const char *propname, u32 *val);


/************************
 ** linux/radix-tree.h **
 ************************/

#define INIT_RADIX_TREE(root, mask) lx_printf("INIT_RADIX_TREE not impelemnted\n")
struct radix_tree_root { };
void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index);
int   radix_tree_insert(struct radix_tree_root *, unsigned long, void *);
void *radix_tree_delete(struct radix_tree_root *, unsigned long);
void  radix_tree_preload_end(void);
int   radix_tree_maybe_preload(gfp_t gfp_mask);


/******************
 ** asm/ptrace.h **
 ******************/

struct pt_regs { unsigned long dummy; };

#define ARM_r8 dummy
#define ARM_r9 dummy
#define ARM_sp dummy
#define ARM_fp dummy


/*****************
 ** linux/smp.h **
 *****************/

typedef void (*smp_call_func_t)(void *info);

int smp_call_function_single(int cpuid, smp_call_func_t func, void *info,
                             int wait);


/***********************
 ** linux/eventpoll.h **
 ***********************/

#define EPOLLIN     (__force __poll_t)0x00000001
#define EPOLLRDNORM (__force __poll_t)0x00000040


/******************************
 ** linux/sched/task_stack.h **
 ******************************/

int object_is_on_stack(const void *obj);


/*****************
 ** linux/tty.h **
 *****************/

struct tty_port {};


/***********************************
 ** arch/arm/include/asm/memory.h **
 ***********************************/

/*
 * deprecated internal function not really part of Linux kernel anymore,
 * but needed by dwc_otg patches
 */
unsigned long __phys_to_virt(phys_addr_t x);


/********************************
 ** linux/regulator/consumer.h **
 ********************************/

struct regulator { };

int    regulator_enable(struct regulator *);
int    regulator_disable(struct regulator *);
struct regulator *__must_check devm_regulator_get(struct device *dev, const char *id);


/*************************
 ** linux/stmp_device.h **
 *************************/

extern int stmp_reset_block(void __iomem *);


/********************
 ** linux/regmap.h **
 ********************/

struct regmap;

int regmap_write(struct regmap *map, unsigned int reg, unsigned int val);
int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val);


/************************
 ** linux/mfd/syscon.h **
 ************************/

struct regmap *syscon_regmap_lookup_by_phandle(struct device_node *np, const char *property);


static inline u32 __raw_readl(const volatile void __iomem *addr)
{
	return *(const volatile u32 __force *) addr;
}

static inline void __raw_writel(u32 b, volatile void __iomem *addr)
{
	*(volatile u32 __force *) addr = b;
}

struct usb_phy *devm_usb_get_phy_by_phandle(struct device *dev, const char *phandle, u8 index);
struct usb_phy *devm_usb_get_phy_dev(struct device *dev, u8 index);

extern int usb_add_phy_dev(struct usb_phy *);


/************************
 ** linux/tracepoint.h **
 ************************/

#define DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print)
#define DEFINE_EVENT(template, name, proto, args)

#define TP_PROTO(args...)     args
#define TP_STRUCT__entry(args...) (args)
#define TP_ARGS(args...)      (args)
#define TP_printk(fmt, args...) (fmt "\n" args)
#define TP_fast_assign(args...) (args)


/*******************
 ** Tracing stuff **
 *******************/

#define trace_xhci_address_ctx(p1, p2, v)
#define trace_xhci_alloc_dev(p)
#define trace_xhci_alloc_virt_device(p)
#define trace_xhci_configure_endpoint(p)
#define trace_xhci_discover_or_reset_device(p)
#define trace_xhci_free_dev(p)
#define trace_xhci_free_virt_device(p)
#define trace_xhci_get_port_status(p1, p2)
#define trace_xhci_handle_cmd_addr_dev(p)
#define trace_xhci_handle_cmd_config_ep(p)
#define trace_xhci_handle_cmd_disable_slot(p)
#define trace_xhci_handle_cmd_reset_dev(p)
#define trace_xhci_handle_cmd_reset_ep(p)
#define trace_xhci_handle_cmd_set_deq(p)
#define trace_xhci_handle_cmd_set_deq_ep(p)
#define trace_xhci_handle_cmd_stop_ep(p)
#define trace_xhci_handle_command(p1, p2)
#define trace_xhci_handle_event(p1, p2)
#define trace_xhci_handle_port_status(p1, p2)
#define trace_xhci_handle_transfer(p1, p2)
#define trace_xhci_hub_status_data(p1, p2)
#define trace_xhci_inc_deq(p)
#define trace_xhci_inc_enq(p)
#define trace_xhci_queue_trb(p1, p2)
#define trace_xhci_ring_alloc(p)
#define trace_xhci_ring_expansion(p)
#define trace_xhci_ring_free(p)
#define trace_xhci_setup_addressable_virt_device(p)
#define trace_xhci_setup_device(p)
#define trace_xhci_setup_device_slot(p)
#define trace_xhci_stop_device(p1)
#define trace_xhci_urb_dequeue(p)
#define trace_xhci_urb_enqueue(p)
#define trace_xhci_urb_giveback(p)

static inline void trace_xhci_dbg_init(struct va_format *v) {}
static inline void trace_xhci_dbg_ring_expansion(struct va_format *v) {}
static inline void trace_xhci_dbg_context_change(struct va_format *v) {}
static inline void trace_xhci_dbg_cancel_urb(struct va_format *v) {}
static inline void trace_xhci_dbg_reset_ep(struct va_format *v) {}
static inline void trace_xhci_dbg_quirks(struct va_format *v) {}
static inline void trace_xhci_dbg_address(struct va_format *v) {}

#define trace_dwc3_readl(v0, v1, v2)
#define trace_dwc3_writel(v0, v1, v2)

void lx_backtrace(void);

#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_H_ */
