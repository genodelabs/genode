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
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

static inline void bt()
{
	lx_printf("BT: 0x%p\n", __builtin_return_address(0));
}


/*******************
 ** linux/sizes.h **
 *******************/

#define SZ_256K 0x40000  /* needed by 'dwc_otg_attr.c' */


/*****************
 ** linux/bcd.h **
 *****************/

#define bin2bcd(x) ((((x) / 10) << 4) + (x) % 10)


/***************
 ** asm/bug.h **
 ***************/

#include <lx_emul/bug.h>


/*********************
 ** linux/kconfig.h **
 *********************/

#define IS_ENABLED(x) x

/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100UL };


/*******************
 ** linux/magic.h **
 *******************/

enum { USBDEVICE_SUPER_MAGIC = 0x9fa2 };


/******************
 ** asm/atomic.h **
 ******************/

typedef struct atomic { unsigned int v; } atomic_t;
typedef void*  atomic_long_t;

void atomic_set(atomic_t *p, unsigned int v);
unsigned int atomic_read(atomic_t *p);

void atomic_inc(atomic_t *v);
void atomic_dec(atomic_t *v);

void atomic_add(int i, atomic_t *v);
void atomic_sub(int i, atomic_t *v);
int atomic_inc_return(atomic_t *v);

#define ATOMIC_INIT(i) { (i) }


/*******************
 ** linux/types.h **
 *******************/

#include <lx_emul/types.h>

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u64 __be64;

typedef __u16 __sum16;
typedef __u32 __wsum;

typedef u64 sector_t;
typedef int clockid_t;


#ifndef __cplusplus
typedef u16            wchar_t;
#endif

/*
 * Needed by 'dwc_common_port/usb.h'
 */
typedef unsigned int  u_int;
typedef unsigned char u_char;
typedef unsigned long u_long;
typedef uint8_t       u_int8_t;
typedef uint16_t      u_int16_t;
typedef uint32_t      u_int32_t;

/*
 * Needed by 'dwc_otg/dwc_otg/dwc_otg_fiq_fsm.h'
 */
typedef unsigned short ushort;

typedef unsigned long phys_addr_t;

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, 8 * sizeof(long))
#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]


#include <linux/usb/storage.h>


/**********************
 ** linux/compiler.h **
 **********************/

#include <lx_emul/compiler.h>

#define notrace /* needed by 'dwc_otg_hcd_intr.c' */
#define __must_hold(x)


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

u16 get_unaligned_le16(const void *p);
void put_unaligned_le16(u16 val, void *p);

void put_unaligned_le32(u32 val, void *p);
u32  get_unaligned_le32(const void *p);

void put_unaligned_be32(u32 val, void *p);

void put_unaligned_le64(u64 val, void *p);
u64 get_unaligned_le64(const void *p);

#define put_unaligned put_unaligned_le32
#ifdef __LP64__
#define get_unaligned get_unaligned_le64
#else
#define get_unaligned get_unaligned_le32
#endif

/*********************************
 ** linux/unaligned/access_ok.h **
 *********************************/

static inline u16 get_unaligned_be16(const void *p)
{
	return be16_to_cpup((__be16 *)p);
}


/****************
 ** asm/page.h **
 ****************/

/*
 * For now, hardcoded to x86_32
 */
enum {
	PAGE_SIZE = 4096,
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
	ENOEXEC       = 8,
	EISDIR        = 21,
	EXFULL        = 52,
	ERESTART      = 53,
	ESHUTDOWN     = 58,
	ECOMM         = 70,
	EIDRM         = 82,
	ENOSR         = 211,
};


/*******************
 ** linux/major.h **
 *******************/

enum {
	INPUT_MAJOR = 13
};


/********************
 ** linux/kernel.h **
 ********************/

#include <lx_emul/kernel.h>

#undef swap

char *bin2hex(char *dst, const void *src, size_t count);
int   hex2bin(u8 *dst, const char *src, size_t count);

char *kasprintf(gfp_t gfp, const char *fmt, ...);
int   kstrtouint(const char *s, unsigned int base, unsigned int *res);
int   kstrtoul(const char *s, unsigned int base, unsigned long *res);
int   kstrtou8(const char *s, unsigned int base, u8 *res);

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

#define PTR_ALIGN(p, a) ({              \
  unsigned long _p = (unsigned long)p;  \
  _p = (_p + a - 1) & ~(a - 1);         \
  p = (typeof(p))_p;                    \
	p;                                    \
})

int  strict_strtoul(const char *s, unsigned int base, unsigned long *res);
long simple_strtoul(const char *cp, char **endp, unsigned int base);
long simple_strtol(const char *,char **,unsigned int);

int hex_to_bin(char ch);

unsigned long int_sqrt(unsigned long);

/*
 * Needed by 'usb.h'
 */
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
int  roundup_pow_of_two(u32 n);
int  rounddown_pow_of_two(u32 n);


/********************
 ** linux/kdev_t.h **
 ********************/

#define MINORBITS 20
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))
#define MINOR(dev)   ((dev) & 0xff)


/********************
 ** linux/printk.h **
 ********************/

enum { DUMP_PREFIX_NONE };

void print_hex_dump(const char *level, const char *prefix_str,
                    int prefix_type, int rowsize, int groupsize,
                    const void *buf, size_t len, bool ascii);

#define pr_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_warn_once pr_warning
#define pr_warn pr_warning

bool printk_ratelimit();
bool printk_ratelimited();
bool printk_timed_ratelimit(unsigned long *, unsigned int);
#define printk_once(fmt, ...)

/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/


#define BIT(nr)       (1UL << (nr))
#define BITS_PER_LONG (__SIZEOF_LONG__ * 8)
#define BIT_MASK(nr)  (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)  ((nr) / BITS_PER_LONG)

/* normally declared in asm-generic/bitops/ffs.h */
int ffs(int x);
int fls(int x);

#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/fls64.h>

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

/**
 * Find first zero bit (limit to machine word size)
 */
long find_next_zero_bit_le(const void *addr, unsigned long size, unsigned long offset);
unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset);

#define find_next_zero_bit find_next_zero_bit_le
#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)

#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size)); \
	     (bit) < (size);                    \
	     (bit) = find_next_bit((addr), (size), (bit) + 1))


/*****************************************
 ** asm-generic//bitops/const_hweight.h **
 *****************************************/

/* count number of 1s in 'w' */
#define hweight32(w) __builtin_popcount((unsigned)w)


/********************
 ** linux/string.h **
 ********************/

#include <lx_emul/string.h>

int strtobool(const char *, bool *);


/*****************
 ** linux/nls.h **
 *****************/

enum utf16_endian { UTF16_LITTLE_ENDIAN = 1, };

int utf16s_to_utf8s(const wchar_t *pwcs, int len,
                    enum utf16_endian endian, u8 *s, int maxlen);


/*******************
 ** linux/ctype.h **
 *******************/

int isprint(int);


/******************
 ** linux/init.h **
 ******************/

#include <lx_emul/module.h>

#define __initconst
#define __initdata
#define __devinit
#define __devexit
#define __devexit_p(x) x

#define __exit_p(x) x


/********************
 ** linux/module.h **
 ********************/

#define MODULE_SOFTDEP(x)

#define THIS_MODULE 0

#define MODULE_DEVICE_TABLE(type, name)

#undef module_init
#undef module_exit

#define _MOD_CONCAT(a,b) module_##a##b
#define MOD_CONCAT(a,b) _MOD_CONCAT(a,b)
#define module_init(fn) void MOD_CONCAT(fn, MOD_SUFFIX)(void) { fn(); }
#define module_exit(exit_fn) void MOD_CONCAT(exit_fn, MOD_SUFFIX)(void) { exit_fn(); }

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


/*************************
 ** linux/moduleparam.h **
 *************************/

#define module_param_array_named(name, array, type, nump, perm)
#define module_param_string(name, string, len, perm)
#define core_param(name, var, type, perm)


/******************
 ** linux/slab.h **
 ******************/

enum {
	SLAB_HWCACHE_ALIGN    = 0x00002000UL,
	SLAB_CACHE_DMA        = 0x00004000UL,
	ARCH_KMALLOC_MINALIGN = 128, 
};

void *kzalloc(size_t size, gfp_t flags);
void  kfree(const void *);
void *kmalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);

/**
 * Genode specific for large DMA allocations
 */
void *dma_malloc(size_t size);
void  dma_free(void *ptr);

struct kmem_cache;

/**
 * Create slab cache
 */
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
                                     unsigned long, void (*)(void *));

/**
 * Destroy slab cache
 */
void kmem_cache_destroy(struct kmem_cache *);

/**
 * Alloc from cache
 */
void *kmem_cache_alloc(struct kmem_cache *, gfp_t);

/**
 * Allocate and zero slab
 */
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);

/**
 * Free slab
 */
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

int  mutex_lock_interruptible(struct mutex *m);


/***********************
 ** linux/semaphore.h **
 ***********************/

struct semaphore;

void down(struct semaphore *sem);


/*******************
 ** linux/rwsem.h **
 *******************/

#include <lx_emul/semaphore.h>


/******************
 ** linux/time.h **
 ******************/

#include <lx_emul/time.h>

enum { CLOCK_BOOTTIME  = 7, };


/*************************
 ** linux/timekeeping.h **
 *************************/

enum tk_offsets {
	TK_OFFS_BOOT = 1,
};

ktime_t ktime_get_boottime(void);
ktime_t ktime_mono_to_any(ktime_t tmono, enum tk_offsets offs);
ktime_t ktime_mono_to_real(ktime_t mono);


/*******************
 ** linux/timer.h **
 *******************/

#include <lx_emul/timer.h>


/*******************
 ** linux/delay.h **
 *******************/

void msleep(unsigned int msecs);
void udelay(unsigned long usecs);
void mdelay(unsigned long usecs);

void usleep_range(unsigned long min, unsigned long max);

extern unsigned long loops_per_jiffy;  /* needed by 'dwc_otg_attr.c' */


/***********************
 ** linux/workquque.h **
 ***********************/

#include <lx_emul/work.h>

enum {
	WORK_STRUCT_PENDING_BIT = 0,
	WQ_FREEZABLE            = (1 << 2),
};


#define work_data_bits(work) ((unsigned long *)(&(work)->data))

#define work_pending(work) \
        test_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))

#define delayed_work_pending(w) \
         work_pending(&(w)->work)

/* dummy for queue_delayed_work call in storage/usb.c */
#define system_freezable_wq 0

extern struct workqueue_struct *system_power_efficient_wq;


/******************
 ** linux/wait.h **
 ******************/

#define DECLARE_WAIT_QUEUE_HEAD_ONSTACK(name) DECLARE_WAIT_QUEUE_HEAD(name)

extern wait_queue_t wait;
void breakpoint();


#define wait_event_interruptible_timeout(wq, condition, timeout) \
({                        \
	_wait_event_timeout(wq, condition, timeout); \
	1;                      \
})


/*******************
 ** linux/sched.h **
 *******************/

enum { TASK_RUNNING = 0, TASK_INTERRUPTIBLE = 1, TASK_UNINTERRUPTIBLE = 2, TASK_NORMAL = 3 };

enum { MAX_SCHEDULE_TIMEOUT = (~0U >> 1) };

struct task_struct {
	char comm[16]; /* needed by usb/core/devio.c, only for debug output */
};

struct cred;
struct siginfo;
struct pid;

int kill_pid_info_as_cred(int, struct siginfo *, struct pid *,
                          const struct cred *, u32);
pid_t task_pid_nr(struct task_struct *tsk);

struct pid *task_pid(struct task_struct *task);

void __set_current_state(int state);
#define set_current_state(state) __set_current_state(state)
int signal_pending(struct task_struct *p);
void schedule(void);
signed long schedule_timeout(signed long);
signed long schedule_timeout_uninterruptible(signed long timeout);
void yield(void);

/* normally defined in asm/current.h, included by sched.h */
extern struct task_struct *current;

/* asm/processor.h */
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
	NOTIFY_BAD       = (NOTIFY_STOP_MASK | 0x0002),
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

int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                     struct notifier_block *nb);
int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
                                       struct notifier_block *nb);
int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long val, void *v);
int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
                                   struct notifier_block *nb);
int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh,
                                     struct notifier_block *nb);


/*************************
 ** linux/scatterlist.h **
 *************************/

#include <lx_emul/scatterlist.h>


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

struct kobj_attribute {
	struct attribute attr;
	void * show;
	void * store;
};

struct attribute_group {
	const char        *name;
	mode_t           (*is_visible)(struct kobject *, struct attribute *, int);
	struct attribute **attrs;
};

struct file;
struct bin_attribute {
	struct attribute attr;
	size_t           size;
	ssize_t        (*read)(struct file *, struct kobject *,
	                       struct bin_attribute *, char *, loff_t, size_t);
};

#define __ATTR(_name,_mode,_show,_store) { \
	.attr  = {.name = #_name, .mode = _mode }, \
	.show  = _show, \
	.store = _store, \
}

#define __ATTRIBUTE_GROUPS(_name)                               \
static const struct attribute_group *_name##_groups[] = {       \
        &_name##_group,                                         \
        NULL,                                                   \
}

#define ATTRIBUTE_GROUPS(_name)                                 \
static const struct attribute_group _name##_group = {           \
        .attrs = _name##_attrs,                                 \
};                                                              \
__ATTRIBUTE_GROUPS(_name)

#define __ATTR_NULL { .attr = { .name = NULL } }
#define __ATTR_RO(name) __ATTR_NULL
#define __ATTR_RW(name) __ATTR_NULL

static const char* modalias = "";

int sysfs_create_group(struct kobject *kobj,
                       const struct attribute_group *grp);
void sysfs_remove_group(struct kobject *kobj,
                        const struct attribute_group *grp);
int sysfs_create_link(struct kobject *kobj, struct kobject *target,
                      const char *name);
void sysfs_remove_link(struct kobject *kobj, const char *name);
int sysfs_create_files(struct kobject *kobj, const struct attribute **ptr);


/****************
 ** linux/pm.h **
 ****************/

#include <lx_emul/pm.h>

#define PMSG_AUTO_SUSPEND ((struct pm_message) \
                           { .event = PM_EVENT_AUTO_SUSPEND, })

/************************
 ** linux/pm_runtime.h **
 ************************/

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

int dev_pm_qos_add_request(struct device *dev, struct dev_pm_qos_request *req,
                           enum dev_pm_qos_req_type type, s32 value);
int dev_pm_qos_remove_request(struct dev_pm_qos_request *req);
int dev_pm_qos_expose_flags(struct device *dev, s32 value);


/******************
 ** linux/acpi.h **
 ******************/

#define ACPI_PTR(_ptr)  (NULL)


/********************
 ** linux/device.h **
 ********************/

#define dev_info(dev, format, arg...)      lx_printf("dev_info: "  format, ## arg)
#define dev_warn(dev, format, arg...)      lx_printf("dev_warn: "  format, ## arg)
#define dev_WARN(dev, format, arg...)      lx_printf("dev_WARN: "  format, ## arg)
#define dev_err( dev, format, arg...)      lx_printf("dev_error: " format, ## arg)
#define dev_notice(dev, format, arg...)    lx_printf("dev_notice: " format, ## arg)
#define dev_dbg_ratelimited(dev, format, arg...)

#define dev_WARN_ONCE(dev, condition, format, arg...) ({\
	lx_printf("dev_WARN_ONCE: "  format, ## arg); \
	!!condition; \
})

#if DEBUG_LINUX_PRINTK
#define dev_dbg(dev, format, arg...)  lx_printf("dev_dbg: " format, ## arg)
#define dev_vdbg(dev, format, arg...) lx_printf("dev_dbg: " format, ## arg)
#define CONFIG_DYNAMIC_DEBUG
#else
#define dev_dbg( dev, format, arg...)
#define dev_vdbg( dev, format, arg...)
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

struct device_driver;

struct bus_type {
	const char *name;
	struct device_attribute *dev_attrs;
	const struct attribute_group **dev_groups;
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

struct kobj_uevent_env;

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

struct dma_parms;

/* DEVICE */
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
	struct dev_pm_info             power;
	dev_t                          devt;
	const struct attribute_group **groups;
	void (*release)(struct device *dev);
	struct bus_type               *bus;
	struct class                  *class;
	void                          *driver_data;
	struct device_node            *of_node;
	struct device_dma_parameters  *dma_parms;
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

#define DRIVER_ATTR(_name, _mode, _show, _store) \
struct driver_attribute driver_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

#define DRIVER_ATTR_RW(_name) \
struct driver_attribute driver_attr_##_name = __ATTR_RW(_name)


void       *dev_get_drvdata(const struct device *dev);
int         dev_set_drvdata(struct device *dev, void *data);
int         dev_set_name(struct device *dev, const char *name, ...);
const char *dev_name(const struct device *dev);
int         dev_to_node(struct device *dev);
void        set_dev_node(struct device *dev, int node);

struct device *device_create(struct class *cls, struct device *parent,
                             dev_t devt, void *drvdata,
                             const char *fmt, ...);
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
int  device_create_bin_file(struct device *dev,
                            const struct bin_attribute *attr);
void device_remove_bin_file(struct device *dev,
                            const struct bin_attribute *attr);
int  device_create_file(struct device *device,
                        const struct device_attribute *entry);
void device_remove_file(struct device *dev,
                        const struct device_attribute *attr);
int device_for_each_child(struct device *dev, void *data,
                          int (*fn)(struct device *dev, void *data));


void           put_device(struct device *dev);
struct device *get_device(struct device *dev);

int  driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);
int  driver_attach(struct device_driver *drv);
int  driver_create_file(struct device_driver *driver,
                        const struct driver_attribute *attr);
void driver_remove_file(struct device_driver *driver,
                        const struct driver_attribute *attr);

struct device_driver *get_driver(struct device_driver *drv);
void                  put_driver(struct device_driver *drv);

struct device *bus_find_device(struct bus_type *bus, struct device *start,
                               void *data,
                               int (*match)(struct device *dev, void *data));
int  bus_register(struct bus_type *bus);
void bus_unregister(struct bus_type *bus);
int  bus_register_notifier(struct bus_type *bus,
                           struct notifier_block *nb);
int  bus_unregister_notifier(struct bus_type *bus,
                             struct notifier_block *nb);
int  bus_for_each_dev(struct bus_type *bus, struct device *start, void *data,
                      int (*fn)(struct device *dev, void *data));

struct class *__class_create(struct module *owner,
                             const char *name,
                             struct lock_class_key *key);
#define class_create(owner, name) \
({ \
	static struct lock_class_key __key; \
	__class_create(owner, name, &__key); \
})
int class_register(struct class *cls);
void class_unregister(struct class *cls);
void class_destroy(struct class *cls);

typedef void (*dr_release_t)(struct device *dev, void *res);
typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp);
void  devres_add(struct device *dev, void *res);
int   devres_destroy(struct device *dev, dr_release_t release,
                   dr_match_t match, void *match_data);
void  devres_free(void *res);

void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);

struct resource;

void *devm_ioremap_resource(struct device *dev, struct resource *res);

void devm_kfree(struct device *dev, void *p);

void *dev_get_platdata(const struct device *dev);


/*****************************
 ** linux/platform_device.h **
 *****************************/

struct platform_device;
void *platform_get_drvdata(const struct platform_device *pdev);
void platform_set_drvdata(struct platform_device *pdev, void *data);

/* needed by 'dwc_otg_driver.c' */
struct platform_driver;
void platform_driver_unregister(struct platform_driver *);


/*********************
 ** linux/dmapool.h **
 *********************/

struct dma_pool;

/*
 * Needed by usb/core/buffer.c
 */
struct dma_pool *dma_pool_create(const char *, struct device *, size_t,
                                 size_t, size_t);
void  dma_pool_destroy(struct dma_pool *);
void *dma_pool_alloc(struct dma_pool *, gfp_t, dma_addr_t *);
void  dma_pool_free(struct dma_pool *, void *, dma_addr_t);


/*
 * Actually defined in asm-generic/dma-mapping-broken.h
 */
void *dma_alloc_coherent(struct device *, size_t, dma_addr_t *, gfp_t);
void  dma_free_coherent(struct device *, size_t, void *, dma_addr_t);


/*************************
 ** linux/dma-mapping.h **
 *************************/

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
static inline int dma_set_coherent_mask(struct device *dev, u64 mask) { dev->coherent_dma_mask = mask; return 0; }
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

/*********************
 ** linux/uaccess.h **
 *********************/

enum { VERIFY_READ = 0, VERIFY_WRITE = 1 };

bool access_ok(int access, void *addr, size_t size);

size_t copy_from_user(void *to, void const *from, size_t len);
size_t copy_to_user(void *dst, void const *src, size_t len);

#define get_user(x, ptr) ({ lx_printf("get_user not implemented"); (0);})
#define put_user(x, ptr) ({ lx_printf("put_user not implemented"); (0);})


unsigned long clear_user(void *to, unsigned long n);


/*****************
 ** linux/dmi.h **
 *****************/

struct dmi_system_id;

static inline int dmi_check_system(const struct dmi_system_id *list) { return 0; }
static inline const char * dmi_get_system_info(int field) { return NULL; }


/*****************************
 ** linux/mod_devicetable.h **
 *****************************/

/*
 * Deal with C++ keyword used as member name of 'pci_device_id'
 */
#define USB_DEVICE_ID_MATCH_VENDOR       0x0001
#define USB_DEVICE_ID_MATCH_PRODUCT      0x0002
#define USB_DEVICE_ID_MATCH_DEV_LO       0x0004
#define USB_DEVICE_ID_MATCH_DEV_HI       0x0008
#define USB_DEVICE_ID_MATCH_DEV_CLASS    0x0010
#define USB_DEVICE_ID_MATCH_DEV_SUBCLASS 0x0020
#define USB_DEVICE_ID_MATCH_DEV_PROTOCOL 0x0040
#define USB_DEVICE_ID_MATCH_INT_CLASS    0x0080
#define USB_DEVICE_ID_MATCH_INT_SUBCLASS 0x0100
#define USB_DEVICE_ID_MATCH_INT_PROTOCOL 0x0200

/********************
 ** linux/dcache.h **
 ********************/

#define __read_mostly

enum dentry_d_lock_class
{
	DENTRY_D_LOCK_NESTED
};

struct qstr {
	const unsigned char *name;
};

struct dentry {
	struct inode    *d_inode;
	struct qstr      d_name;
	struct list_head d_subdirs;
	spinlock_t       d_lock;
	struct dentry   *d_parent;
	union { struct list_head d_child; } d_u;
};

void           d_instantiate(struct dentry *, struct inode *);
int            d_unhashed(struct dentry *dentry);
void           d_delete(struct dentry *);
struct dentry *d_alloc_root(struct inode *);
struct dentry *dget(struct dentry *dentry);
void           dput(struct dentry *);

void dont_mount(struct dentry *dentry);


/******************
 ** linux/poll.h **
 ******************/

/* needed by usb/core/devices.c, defined in asm-generic/poll.h */
#define POLLIN     0x0001
#define POLLOUT    0x0004
#define POLLERR    0x0008
#define POLLHUP    0x0010
#define POLLRDNORM 0x0040
#define POLLWRNORM 0x0100

typedef struct poll_table_struct { int dummy; } poll_table;

struct file;

void poll_wait(struct file *, wait_queue_head_t *, poll_table *);


/********************
 ** linux/statfs.h **
 ********************/

struct kstatfs;
loff_t default_llseek(struct file *file, loff_t offset, int origin);


/*************************
 ** asm-generic/fcntl.h **
 *************************/

enum { O_NONBLOCK = 0x4000 };


/****************
 ** linux/fs.h **
 ****************/

#define FMODE_WRITE 0x2

enum { S_DEAD = 16 };

enum inode_i_mutex_lock_class { I_MUTEX_PARENT };

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

typedef unsigned fl_owner_t;

struct file_operations {
	struct module *owner;
	int          (*open) (struct inode *, struct file *);
	ssize_t      (*read) (struct file *, char __user *, size_t, loff_t *);
	loff_t       (*llseek) (struct file *, loff_t, int);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	long         (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	int          (*flush) (struct file *, fl_owner_t id);
	int          (*release) (struct inode *, struct file *);
	ssize_t      (*write) (struct file *, const char __user *, size_t, loff_t *);
	int          (*fasync) (int, struct file *, int);
};

struct inode_operations { int dummy; };

struct inode {
	umode_t                        i_mode;
	struct mutex                   i_mutex;
	dev_t                          i_rdev;
	struct timespec                i_mtime;
	struct timespec                i_atime;
	struct timespec                i_ctime;
	uid_t                          i_uid;
	gid_t                          i_gid;
	unsigned long                  i_ino;
	const struct file_operations  *i_fop;
	const struct inode_operations *i_op;
	struct super_block            *i_sb;
	unsigned int                   i_flags;
	loff_t                         i_size;
	union {
		struct cdev             *i_cdev;
	};
	void                          *i_private;
};

struct seq_file;
struct vfsmount;
struct super_operations {
	int (*show_options)(struct seq_file *, struct vfsmount *);
	int (*drop_inode) (struct inode *);
	int (*remount_fs) (struct super_block *, int *, char *);
	int (*statfs) (struct dentry *, struct kstatfs *);
};

struct super_block {
	struct dentry                 *s_root;
	const struct super_operations *s_op;
	u32                            s_time_gran;
	unsigned long                  s_magic;
	unsigned char                  s_blocksize_bits;
	unsigned long                  s_blocksize;
};

struct file_system_type {
	const char      *name;
	struct module   *owner;
	struct dentry *(*mount)(struct file_system_type *, int,
	                        const char *, void *);
	void           (*kill_sb)(struct super_block *);
};

struct fasync_struct { };

unsigned iminor(const struct inode *inode);
unsigned imajor(const struct inode *inode);

int  register_chrdev_region(dev_t, unsigned, const char *);
void unregister_chrdev_region(dev_t, unsigned);
static inline struct file_operations const *
fops_get(struct file_operations const *fops) { return fops; }
void fops_put(struct file_operations const *);
loff_t noop_llseek(struct file *file, loff_t offset, int origin);
int register_chrdev(unsigned int major, const char *name,
                    const struct file_operations *fops);
void unregister_chrdev(unsigned int major, const char *name);
struct inode *new_inode(struct super_block *sb);
unsigned int get_next_ino(void);
void init_special_inode(struct inode *, umode_t, dev_t);
int generic_delete_inode(struct inode *inode);
void drop_nlink(struct inode *inode);
void inc_nlink(struct inode *inode);
void dentry_unhash(struct dentry *dentry);
void iput(struct inode *);
struct dentry *mount_single(struct file_system_type *fs_type,
                            int flags, void *data,
                            int (*fill_super)(struct super_block *,
                                              void *, int));
int nonseekable_open(struct inode *inode, struct file *filp);
int simple_statfs(struct dentry *, struct kstatfs *);
int simple_pin_fs(struct file_system_type *, struct vfsmount **mount, int *count);
ssize_t simple_read_from_buffer(void __user *to, size_t count,
                                loff_t *ppos, const void *from, size_t available);
void simple_release_fs(struct vfsmount **mount, int *count);
void kill_litter_super(struct super_block *sb);
int register_filesystem(struct file_system_type *);
int unregister_filesystem(struct file_system_type *);

void kill_fasync(struct fasync_struct **, int, int);
int fasync_helper(int, struct file *, int, struct fasync_struct **);

extern const struct file_operations  simple_dir_operations;
extern const struct inode_operations simple_dir_inode_operations;

static inline loff_t no_llseek(struct file *file, loff_t offset, int origin) {
	 return -ESPIPE; }

struct inode *file_inode(struct file *f);

#define replace_fops(f, fops) \
do {    \
        struct file *__file = (f); \
        fops_put(__file->f_op); \
        BUG_ON(!(__file->f_op = (fops))); \
} while(0)

/*******************
 ** linux/namei.h **
 *******************/

struct dentry *lookup_one_len(const char *, struct dentry *, int);


/*******************
 ** linux/mount.h **
 *******************/

struct vfsmount {
	int dummy;
	struct super_block *mnt_sb;
};


/*************************
 ** asm-<arch>/signal.h **
 *************************/

enum { SIGIO =  29 };


/**********************
 ** linux/seq_file.h **
 **********************/

struct seq_file { int dummy; };

int seq_printf(struct seq_file *, const char *, ...);
int seq_putc(struct seq_file *, char);

/*****************
 ** linux/gfp.h **
 *****************/

#include <lx_emul/gfp.h>

enum {
	GFP_NOIO     = GFP_LX_DMA,
	GFP_NOWAIT   = 0x2000000u,
};

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
#define __get_free_page(gfp_mask)  __get_free_pages((gfp_mask), 0)

void __free_pages(struct page *p, unsigned int order);
#define __free_page(p) __free_pages((p), 0)

void free_pages(unsigned long addr, unsigned int order);
#define free_page(addr) free_pages((addr), 0)


/*********************
 ** linux/proc_fs.h **
 *********************/

struct proc_dir_entry *proc_mkdir(const char *,struct proc_dir_entry *);
void remove_proc_entry(const char *name, struct proc_dir_entry *parent);


/********************
 * linux/debugfs.h **
 ********************/

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent);
struct dentry *debugfs_create_file(const char *name, mode_t mode,
                                   struct dentry *parent, void *data,
                                   const struct file_operations *fops);
void debugfs_remove(struct dentry *dentry);
static inline void debugfs_remove_recursive(struct dentry *dentry) { }


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
void   kvfree(const void *addr);

/*********************
 ** linux/pagemap.h **
 *********************/

enum { PAGE_CACHE_SHIFT = PAGE_SHIFT };
enum { PAGE_CACHE_SIZE  = PAGE_SIZE };


/**********************
 ** linux/highmem.h  **
 **********************/

void *kmap(struct page *page);
void kunmap(struct page *page);


/**********************
 ** asm-generic/io.h **
 **********************/

#include <lx_emul/mmio.h>

void *ioremap(phys_addr_t addr, unsigned long size);
void  iounmap(volatile void *addr);
void *devm_ioremap(struct device *dev, resource_size_t offset,
                   unsigned long size);
void *devm_ioremap_nocache(struct device *dev, resource_size_t offset,
                           unsigned long size);

#define ioremap_nocache ioremap

void *phys_to_virt(unsigned long address);

void outb(u8  value, u32 port);
void outw(u16 value, u32 port);
void outl(u32 value, u32 port);

u8  inb(u32 port);
u16 inw(u32 port);
u32 inl(u32 port);

void native_io_delay(void);

static inline void outb_p(u8  value, u32 port) { outb(value, port); native_io_delay(); }
static inline void outw_p(u16 value, u32 port) { outw(value, port); native_io_delay(); }
static inline void outl_p(u32 value, u32 port) { outl(value, port); native_io_delay(); }

static inline u8  inb_p(u8  port) { u8  ret = inb(port); native_io_delay(); return ret; }
static inline u16 inw_p(u16 port) { u16 ret = inw(port); native_io_delay(); return ret; }
static inline u32 inl_p(u32 port) { u32 ret = inl(port); native_io_delay(); return ret; }


/********************
 ** linux/ioport.h **
 ********************/

#include <lx_emul/ioport.h>



/***********************
 ** linux/irqreturn.h **
 ***********************/

#include <lx_emul/irq.h>

/* needed by 'dwc_otg_hcd_linux.c' */
#define IRQ_RETVAL(x) ((x) != IRQ_NONE)


/***********************
 ** linux/interrupt.h **
 ***********************/

#define IRQF_SHARED   0x00000080
#define IRQF_DISABLED 0x00000020

void local_irq_enable(void);
void local_irq_disable(void);

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
                const char *name, void *dev);

void free_irq(unsigned int, void *);


/*****************
 ** linux/irq.h **
 *****************/

enum { IRQ_TYPE_LEVEL_LOW = 0x00000008 }; /* needed by 'dwc_otg_driver.c' */


/*********************
 ** linux/hardirq.h **
 *********************/

/* needed by 'dwc_common_linux.c' */
int in_irq();


/***************
 ** asm/fiq.h **
 ***************/

/*
 * Needed by 'dwc_otg_hcd_linux.c'
 */
struct fiq_handler {
	char const *name;
};

void __FIQ_Branch(unsigned long *regs);


/*********************
 ** linux/hardirq.h **
 *********************/

void synchronize_irq(unsigned int irq);
bool in_interrupt(void);


/*****************
 ** linux/pci.h **
 *****************/


/*
 * Definitions normally found in pci_regs.h
 */
extern struct  bus_type pci_bus_type;

enum { DEVICE_COUNT_RESOURCE = 6 };


/*
 * PCI types
 */
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

/* quirks */
#define DECLARE_PCI_FIXUP_CLASS_FINAL(vendor, device, class,            \
                                         class_shift, hook)             \
                     void __pci_fixup_##hook(void *data) { hook(data); }

#define for_each_pci_dev(d) printk("for_each_pci_dev called\n"); while(0)

enum { PCI_ROM_RESOURCE = 6 };

/*
 * Deal with C++ keyword used as member name of 'pci_dev'
 */
struct msix_entry
{
	u32 vector;
	u16 entry;
};


int  pci_enable_msix(struct pci_dev *, struct msix_entry *, int);
void pci_disable_msix(struct pci_dev *);
int  pci_enable_msix_exact(struct pci_dev *,struct msix_entry *, int);


int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask);
int pci_set_power_state(struct pci_dev *dev, pci_power_t state);



/**********************
 ** linux/irqflags.h **
 **********************/

unsigned long local_irq_save(unsigned long flags);
unsigned long local_irq_restore(unsigned long flags);


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

dma_addr_t dma_map_single_attrs(struct device *dev, void *ptr,
                                size_t size,
                                enum dma_data_direction dir,
                                struct dma_attrs *attrs);

void dma_unmap_single_attrs(struct device *dev, dma_addr_t addr,
                            size_t size,
                            enum dma_data_direction dir,
                            struct dma_attrs *attrs);

void dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sg,
                        int nents, enum dma_data_direction dir,
                        struct dma_attrs *attrs);

dma_addr_t dma_map_page(struct device *dev, struct page *page,
                        size_t offset, size_t size,
                        enum dma_data_direction dir);

int dma_map_sg_attrs(struct device *dev, struct scatterlist *sg,
                     int nents, enum dma_data_direction dir,
                     struct dma_attrs *attrs);

#define dma_map_single(d, a, s, r) dma_map_single_attrs(d, a, s, r, NULL)
#define dma_unmap_single(d, a, s, r) dma_unmap_single_attrs(d, a, s, r, NULL)
#define dma_map_sg(d, s, n, r) dma_map_sg_attrs(d, s, n, r, NULL)
#define dma_unmap_sg(d, s, n, r) dma_unmap_sg_attrs(d, s, n, r, NULL)

/* linux/dma-mapping-broken.h */
void dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size,
                    enum dma_data_direction direction);
int dma_mapping_error(struct device *dev, dma_addr_t dma_addr);

/*****************
 ** linux/pid.h **
 *****************/

struct pid;
void put_pid(struct pid *pid);
struct pid *get_pid(struct pid *pid);


/******************
 ** linux/cred.h **
 ******************/

struct cred;
void put_cred(struct cred const *);
const struct cred *get_cred(const struct cred *cred);

#define get_current_cred() 0

#define current_fsuid() 0
#define current_fsgid() 0


/***************************
 ** asm-generic/siginfo.h **
 ***************************/

/* needed by usb/core/devio.c */
struct siginfo {
	int   si_signo;
	int   si_errno;
	int   si_code;
	void *si_addr;
};

enum {
	SI_ASYNCIO = -4, 
	_P         = 2 << 16,
	POLL_IN    = _P | 1,
	POLL_HUP   = _P | 6,
};


/**********************
 ** linux/security.h **
 **********************/

void security_task_getsecid(struct task_struct *p, u32 *secid);


/*************************
 ** asm-generic/ioctl.h **
 *************************/

/*
 * Needed by drivers/input/evdev.c, used to calculate ioctl opcodes
 */
#include <asm-generic/ioctl.h>


/******************
 ** linux/cdev.h **
 ******************/

struct cdev { struct kobject kobj; };
void cdev_init(struct cdev *, const struct file_operations *);
int cdev_add(struct cdev *, dev_t, unsigned);
void cdev_del(struct cdev *);


/******************
 ** linux/stat.h **
 ******************/

#define S_IALLUGO 0007777

#define S_IRUGO   00444
#define S_IXUGO   00111
#define S_IRWXUGO 00777

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
struct new_utsname *utsname(void);

/*********************
 ** linux/freezer.h **
 *********************/

void set_freezable(void);
static inline void set_freezable_with_signal(void) {}

#define wait_event_freezable(wq, cond) wait_event_interruptible(wq, cond)


/********************
 ** linux/parser.h **
 ********************/

/*
 * needed for usb/core/inode.c
 */

enum { MAX_OPT_ARGS = 3 };

struct match_token {
	int token;
	const char *pattern;
};

typedef struct substring { int dummy; } substring_t;

typedef struct match_token match_table_t[];

int match_token(char *, const match_table_t table, substring_t args[]);
int match_int(substring_t *, int *result);
int match_octal(substring_t *, int *result);


/************************
 ** linux/completion.h **
 ************************/

#include <lx_emul/completion.h>

struct completion { unsigned int done; void *task; };
long  __wait_completion(struct completion *work, unsigned long timeout);;


/*******************
 ** linux/input.h **
 *******************/

struct input_dev;

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, 8 * sizeof(long))


/*********************
 ** linux/semaphore **
 *********************/

struct semaphore { };
void sema_init(struct semaphore *sem, int val);
int  down_trylock(struct semaphore *sem);
void up(struct semaphore *sem);
int  down_interruptible(struct semaphore *sem);


/***********************
 ** linux/hid-debug.h **
 ***********************/

enum { HID_DEBUG_BUFSIZE=512 };

#define hid_debug_init()            do { } while (0)
#define hid_dump_input(a,b,c)       do { } while (0)
#define hid_debug_event(a,b)        do { } while (0)
#define hid_debug_register(a, b)    do { } while (0)
#define hid_debug_unregister(a)     do { } while (0)
#define hid_debug_exit()            do { } while (0)
#define hid_dump_report(a, b, c, d) do { } while (0)


/******************
 ** linux/list.h **
 ******************/

/*
 * Important to decl here, otherwise the compiler will generate
 * a non-static function and all hell breaks loose.
 */
static inline void barrier();

#include <lx_emul/list.h>


/********************
 ** linux/hidraw.h **
 ********************/

struct hidraw { u32 minor; };
struct hid_device;

static inline int hidraw_init(void) { return 0; }
static inline void hidraw_exit(void) { }
static inline int hidraw_report_event(struct hid_device *hid, u8 *data, int len) { return 0; }
static inline int hidraw_connect(struct hid_device *hid) { return -1; }
static inline void hidraw_disconnect(struct hid_device *hid) { }


/**********************
 ** linux/rcupdate.h **
 **********************/

static inline void rcu_read_lock(void) { }
static inline void rcu_read_unlock(void) { }
static inline void synchronize_rcu(void) { }

#define rcu_dereference(p) p
#define rcu_assign_pointer(p,v) p = v

#define rcu_dereference_protected(p, c) p


/*********************
 ** linux/rculist.h **
 *********************/

#define list_for_each_entry_rcu(pos, head, member) \
        list_for_each_entry(pos, head, member)

static inline void list_add_rcu(struct list_head *n, struct list_head *head) {
	list_add(n, head); }

static inline void list_add_tail_rcu(struct list_head *n,
                       struct list_head *head) {
	list_add_tail(n, head); }

static inline void list_del_rcu(struct list_head *entry) {
	list_del(entry); }


/*********************
 ** linux/lockdep.h **
 *********************/

enum { SINGLE_DEPTH_NESTING = 1 };

bool lockdep_is_held(void *);


/********************
 ** linux/random.h **
 ********************/

static inline void add_input_randomness(unsigned int type, unsigned int code,
                                        unsigned int value) { }
void add_device_randomness(const void *, unsigned int);

/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size);
void *vzalloc(unsigned long size);
void vfree(void *addr);


/*******************
 ** linux/genhd.h **
 *******************/

struct gendisk
{
	void *private_data;
};


/********************
 ** linux/blkdev.h **
 ********************/

enum { BLK_BOUNCE_HIGH = -1ULL };
enum blk_eh_timer_return { DUMMY };

#define BLK_MAX_CDB 16

#define blk_bidi_rq(rq) ((rq)->next_rq != NULL)

struct request_queue
{
	spinlock_t *queue_lock;
};

enum rq_cmd_type_bits
{
	REQ_TYPE_BLOCK_PC = 2,
};

struct request
{
	enum   rq_cmd_type_bits cmd_type;
	struct gendisk         *rq_disk;
	void                   *special;  /* opaque pointer available for LLD use */
	struct request         *next_rq;
};

void blk_queue_bounce_limit(struct request_queue *, u64);
void blk_queue_dma_alignment(struct request_queue *, int);
void blk_queue_max_hw_sectors(struct request_queue *, unsigned int);
int  blk_queue_resize_tags(struct request_queue *, int);
int  blk_queue_tagged(struct request_queue *);
void blk_queue_update_dma_alignment(struct request_queue *, int);

void blk_complete_request(struct request *);

sector_t blk_rq_pos(const struct request *rq);

unsigned int queue_max_hw_sectors(struct request_queue *q);


/********************
 ** linux/blk-mq.h **
 ********************/

struct blk_mq_tag_set
{
	unsigned dummy;
};


/************************
 ** scsi/scsci_proto.h **
 ************************/

enum {
	SAM_STAT_GOOD                      = 0x00,
	SAM_STAT_INTERMEDIATE              = 0x10,
	SAM_STAT_INTERMEDIATE_CONDITION_MET= 0x14,
	SAM_STAT_COMMAND_TERMINATED        = 0x22,
};

#include <scsi/scsi_host.h>


/*************************
 ** scsi/scsi_devinfo.h **
 *************************/

enum Blist {
	BLIST_FORCELUN = 2,
};


/********************
 ** scsi/scsi_eh.h **
 *******************/

struct scsi_eh_save
{
	unsigned char cmd_len;
};

struct scsi_sense_hdr
{
	u8 response_code;
	u8 sense_key;
	u8 asc;
	u8 ascq;
	u8 additional_length;   /* always 0 for fixed sense format */
};

void scsi_report_device_reset(struct Scsi_Host *, int, int);
void scsi_report_bus_reset(struct Scsi_Host *, int);

void scsi_eh_prep_cmnd(struct scsi_cmnd *scmd,
                       struct scsi_eh_save *ses, unsigned char *cmnd,
                       int cmnd_size, unsigned sense_bytes);

void scsi_eh_restore_cmnd(struct scsi_cmnd* scmd,
                          struct scsi_eh_save *ses);

int scsi_normalize_sense(const u8 *sense_buffer, int sb_len,
                         struct scsi_sense_hdr *sshdr);

const u8 * scsi_sense_desc_find(const u8 * sense_buffer, int sb_len,
                                int desc_type);
int scsi_sense_valid(struct scsi_sense_hdr *);
int scsi_sense_is_deferred(struct scsi_sense_hdr *);


/*********************
 ** scsi/scsi_tcq.h **
 *********************/

enum {
	MSG_SIMPLE_TAG  = 0x20,
	MSG_ORDERED_TAG = 0x22,
};


/***********************
 ** drivers/scsi/sd.h **
 **********************/

struct scsi_disk
{
	sector_t capacity; /* size in 512-byte sectors */
};

struct scsi_disk *scsi_disk(struct gendisk *disk);


/**********************
 ** scsi/scsi_cmnd.h **
 **********************/

enum {
	MAX_COMMAND_SIZE      = 16,
	SCSI_SENSE_BUFFERSIZE = 96,
};

struct scsi_data_buffer
{
	struct sg_table table;
	unsigned length;
};

struct scsi_cmnd
{
	struct scsi_device *device;
	struct list_head    list;  /* scsi_cmnd participates in queue lists */
	struct delayed_work abort_work;
	unsigned long       serial_number;

	/*
	 * This is set to jiffies as it was when the command was first
	 * allocated.  It is used to time how long the command has
	 * been outstanding
	 */
	unsigned long jiffies_at_alloc;

	unsigned short cmd_len;
	enum dma_data_direction sc_data_direction;
	unsigned char *cmnd;

	struct scsi_data_buffer  sdb;
	struct scsi_data_buffer *prot_sdb;

	unsigned underflow;  /* return error if less than
	                        this amount is transferred */

	struct request *request;     /* the command we are working on */
	unsigned char *sense_buffer; /* obtained by REQUEST SENSE when
	                              * CHECK CONDITION is received on original
	                              * command (auto-sense) */
	/**
	 * Low-level done function
	 *
	 * This function can be used by low-level driver to point to completion
	 * function.  Not used by mid/upper level code.
	 */
	void (*scsi_done) (struct scsi_cmnd *);

	int   result;           /* status code from lower level driver */
	void *back;             /* uur completion */

	void *packet;
	void *session;
};


struct scatterlist *scsi_sglist(struct scsi_cmnd *cmd);
unsigned scsi_sg_count(struct scsi_cmnd *cmd);
unsigned scsi_bufflen(struct scsi_cmnd *cmd);
void scsi_set_resid(struct scsi_cmnd *cmd, int resid);
int scsi_get_resid(struct scsi_cmnd *cmd);

struct scsi_driver *scsi_cmd_to_driver(struct scsi_cmnd *cmd);
struct scsi_target *scsi_target(struct scsi_device *sdev);

void trace_scsi_dispatch_cmd_start(struct scsi_cmnd *);
void trace_scsi_dispatch_cmd_error(struct scsi_cmnd *, int);
void trace_scsi_dispatch_cmd_done(struct scsi_cmnd *);


/************************
 ** scsi/scsi_device.h **
 ************************/

#define scmd_printk(prefix, scmd, fmt, a...)
#define sdev_printk(prefix, sdev, fmt, a...)

struct scsi_target
{
	struct list_head devices;
	struct device    dev;
	unsigned int     channel;
	unsigned int     id;
	unsigned int     pdt_1f_for_no_lun:1;    /* PDT = 0x1f */
	unsigned int     no_report_luns:1;       /* Don't use
	                                          * REPORT LUNS for scanning. */
	atomic_t         target_blocked;
	char             scsi_level;
};

enum scsi_device_state
{
	SDEV_CANCEL = 1,
	SDEV_DEL, /* device deleted 
	           * no commands allowed */
};

enum {
	SCSI_VPD_PG_LEN  = 255,
};

struct scsi_device
{
	struct Scsi_Host     *host;
	struct request_queue *request_queue;

	struct list_head      siblings;               /* list of all devices on this host */
	struct list_head      same_target_siblings;   /* just the devices sharing same target id */

	spinlock_t            list_lock;
	struct                list_head cmd_list;     /* queue of in use SCSI Command structures */

	unsigned short         queue_depth;           /* How deep of a queue we want */

	unsigned short        last_queue_full_depth;  /* These two are used by */
	unsigned short        last_queue_full_count;  /* scsi_track_queue_full() */
	unsigned long         last_queue_full_time;   /* last queue full time */

	unsigned int          id, channel;
	u64                   lun;
	char                  type;
	char                  scsi_level;
	unsigned char         inquiry_len;            /* valid bytes in 'inquiry' */

	int                   vpd_pg83_len;
	unsigned char        *vpd_pg83;
	int                   vpd_pg80_len;
	unsigned char        *vpd_pg80;
	struct scsi_target   *sdev_target;            /* used only for single_lun */

	unsigned              sdev_bflags;
	unsigned              lockable:1;             /* able to prevent media removal */
	unsigned              simple_tags:1;          /* simple queue tag messages are enabled */
	unsigned              ordered_tags:1;         /* ordered queue tag messages are enabled */
	unsigned              use_10_for_rw:1;        /* first try 10-byte read / write */
	unsigned              use_10_for_ms:1;        /* first try 10-byte mode sense/select */
	unsigned              no_report_opcodes:1;    /* no REPORT SUPPORTED OPERATION CODES */
	unsigned              no_write_same:1;        /* no WRITE SAME command */
	unsigned              skip_ms_page_8:1;       /* do not use MODE SENSE page 0x08 */
	unsigned              skip_ms_page_3f:1;      /* do not use MODE SENSE page 0x3f */
	unsigned              skip_vpd_pages:1;       /* do not read VPD pages */
	unsigned              use_192_bytes_for_3f:1; /* ask for 192 bytes from page 0x3f */
	unsigned              allow_restart:1;        /* issue START_UNIT in error handler */
	unsigned              fix_capacity:1;         /* READ_CAPACITY is too high by 1 */
	unsigned              guess_capacity:1;       /* READ_CAPACITY might be too high by 1 */
	unsigned              no_read_capacity_16:1;  /* avoid READ_CAPACITY_16 cmds */
	unsigned              retry_hwerror:1;        /* retry HARDWARE_ERROR */
	unsigned              last_sector_bug:1;      /* do not use multisector accesses on
	                                                 SD_LAST_BUGGY_SECTORS */
	unsigned              no_read_disc_info:1;    /* avoid READ_DISC_INFO cmds */
	unsigned              try_rc_10_first:1;      /* Try READ_CAPACACITY_10 first */
	unsigned              wce_default_on:1;       /* Cache is ON by default */
	unsigned              broken_fua:1;           /* Don't set FUA bit */

	atomic_t              device_blocked;         /* device returned QUEUE_FULL. */

	atomic_t              iorequest_cnt;
	struct device         sdev_gendev;
	enum scsi_device_state sdev_state;
};

#define to_scsi_device(d)       \
        container_of(d, struct scsi_device, sdev_gendev)

#define shost_for_each_device(sdev, shost) lx_printf("shost_for_each_device called\n");
#define __shost_for_each_device(sdev, shost) lx_printf("__shost_for_each_device called\n");

int scsi_device_blocked(struct scsi_device *);
int scsi_device_get(struct scsi_device *);
int scsi_execute_req(struct scsi_device *, const unsigned char *, int, void *,
                     unsigned, struct scsi_sense_hdr *, int , int , int *);


/************************
 ** scsi/scsi_driver.h **
 ************************/

struct scsi_driver
{
	int (*done)(struct scsi_cmnd *);
};


/************************
 ** Networking support **
 ************************/

/*********************
 ** linux/if_vlan.h **
 *********************/

enum { VLAN_HLEN = 4 };

/*****************
 ** linux/net.h **
 *****************/

int net_ratelimit(void);

/********************
 ** linux/skbuff.h **
 ********************/

struct net_device;

enum {
	CHECKSUM_NONE        = 0,
	CHECKSUM_UNNECESSARY = 1,
	CHECKSUM_COMPLETE    = 2,
	CHECKSUM_PARTIAL     = 3,

	NET_IP_ALIGN         = 2,
	MAX_SKB_FRAGS        = 16,
};


typedef struct skb_frag_struct
{
	struct
	{
		struct page *p;
	} page;

	__u32 page_offset;
	__u32 size;
} skb_frag_t;

struct skb_shared_info
{
	unsigned short nr_frags;
	unsigned short gso_size;

	skb_frag_t     frags[MAX_SKB_FRAGS];
};

struct sk_buff
{
	struct sk_buff          *next;
	struct sk_buff          *prev;

	/*
	 * This is the control buffer. It is free to use for every
	 * layer. Please put your private variables there. If you
	 * want to keep them across layers you have to do a skb_clone()
	 * first. This is owned by whoever has the skb queued ATM.
	 */
	char  cb[48] __attribute__((aligned(8)));
	
	unsigned int len;
	union
	{
		__wsum csum;
		struct
		{
			u16 csum_start;
			u16 csum_offset;
		};
	};
	u8  local_df:1,
	    cloned:1,
	    ip_summed:2,
	    nohdr:1,
	    nfctinfo:3;
	__be16         protocol;
	unsigned char *start;
	unsigned char *end;
	unsigned char *head;
	unsigned char *data;
	unsigned char *tail;
	unsigned char *phys;
	unsigned int   truesize;
	void *packet;
	unsigned char *clone;
};

struct sk_buff_head
{
	struct sk_buff  *next;
	struct sk_buff  *prev;
	u32        qlen;
	spinlock_t lock;
};

#define skb_queue_walk(queue, skb)                       \
                       for (skb = (queue)->next;         \
                       skb != (struct sk_buff *)(queue); \
                       skb = skb->next)

struct skb_shared_info *skb_shinfo(struct sk_buff const *);
struct sk_buff *alloc_skb(unsigned int, gfp_t);
unsigned char *skb_push(struct sk_buff *, unsigned int);
unsigned char *skb_pull(struct sk_buff *, unsigned int);
unsigned char *skb_put(struct sk_buff *, unsigned int);
unsigned char *__skb_put(struct sk_buff *, unsigned int);
void skb_trim(struct sk_buff *, unsigned int);
unsigned int skb_headroom(const struct sk_buff *);
int skb_checksum_start_offset(const struct sk_buff *);
struct sk_buff *skb_copy_expand(const struct sk_buff *, int, int, gfp_t);
unsigned char *skb_tail_pointer(const struct sk_buff *);
int skb_tailroom(const struct sk_buff *);
void skb_set_tail_pointer(struct sk_buff *, const int);
struct sk_buff *skb_clone(struct sk_buff *, gfp_t);
void skb_reserve(struct sk_buff *, int);
int skb_header_cloned(const struct sk_buff *);
unsigned int skb_headlen(const struct sk_buff *);
int skb_linearize(struct sk_buff *);


struct sk_buff *netdev_alloc_skb_ip_align(struct net_device *dev, unsigned int length);

static inline 
struct sk_buff *__netdev_alloc_skb_ip_align(struct net_device *dev,
                                            unsigned int length, gfp_t gfp)
{
	return netdev_alloc_skb_ip_align(dev, length);
}


static inline int skb_cloned(const struct sk_buff *skb) {
	return skb->cloned; }
static inline void skb_copy_to_linear_data(struct sk_buff *skb,
                                           const void *from,
                                           const unsigned int len) {
	memcpy(skb->data, from, len); }

struct sk_buff *skb_dequeue(struct sk_buff_head *);
void skb_queue_head_init(struct sk_buff_head *);
void skb_queue_tail(struct sk_buff_head *, struct sk_buff *);
void __skb_queue_tail(struct sk_buff_head *, struct sk_buff *);
int skb_queue_empty(const struct sk_buff_head *);
void skb_queue_purge(struct sk_buff_head *);
void __skb_unlink(struct sk_buff *, struct sk_buff_head *);

void skb_tx_timestamp(struct sk_buff *);
bool skb_defer_rx_timestamp(struct sk_buff *);

void dev_kfree_skb(struct sk_buff *);
void dev_kfree_skb_any(struct sk_buff *);
void kfree_skb(struct sk_buff *);


int pskb_expand_head(struct sk_buff *, int, int ntail, gfp_t);

unsigned int skb_frag_size(const skb_frag_t *frag);

/*********************
 ** linux/uapi/if.h **
 *********************/

enum {
	IFF_NOARP     = 0x80,   /* no APR protocol */
	IFF_PROMISC   = 0x100,  /* receive all packets */
	IFF_ALLMULTI  = 0x200,  /* receive all multicast packets */
	IFF_MULTICAST = 0x1000, /* supports multicast */
	IFNAMSIZ      = 16,
};

struct ifreq { };


/**********************
 ** linux/if_ether.h **
 **********************/

enum {
	ETH_ALEN    = 6,      /* octets in one ethernet addr */
	ETH_HLEN    = 14,     /* total octets in header */
	ETH_P_8021Q = 0x8100, /* 802.1Q VLAN Extended Header  */

	ETH_FRAME_LEN = 1514,
};


/*********************
 ** linux/ethtool.h **
 *********************/

enum {
	DUPLEX_HALF  = 0x0,
	DUPLEX_FULL  = 0x1,
	ETHTOOL_GSET = 0x1,
	ETHTOOL_FWVERS_LEN = 32,
	ETHTOOL_BUSINFO_LEN = 32,

	WAKE_PHY     = 0,
	WAKE_UCAST   = (1 << 1),
	WAKE_MCAST   = (1 << 2),
	WAKE_BCAST   = (1 << 3),
	WAKE_ARP     = (1 << 4),
	WAKE_MAGIC   = (1 << 5),

	SPEED_100    = 100,
	SPEED_1000   = 1000,
	SUPPORTED_100baseT_Full  = (1 << 3),
	SUPPORTED_1000baseT_Full = (1 << 5),
};


struct ethtool_cmd
{
	u32 cmd;
	u16 speed;
	u8  duplex;
};

struct ethtool_regs
{
	 u32   version;
};

struct ethtool_eeprom
{
	u32 magic;
	u32 offset;
	u32 len;
};

struct ethtool_drvinfo
{
	char    driver[32];     /* driver short name, "tulip", "eepro100" */
	char    version[32];    /* driver version string */
	char    fw_version[ETHTOOL_FWVERS_LEN]; /* firmware version string */
	char    bus_info[ETHTOOL_BUSINFO_LEN];  /* Bus info for this IF. */
	                                        /* For PCI devices, use pci_name(pci_dev). */
	u32     eedump_len;
};

struct ethtool_wolinfo {
	u32 supported;
	u32 wolopts;
};

struct ethhdr { };
struct ethtool_ts_info; 

struct ethtool_eee
{
	u32 supported;
	u32 advertised;
	u32 lp_advertised;
	u32 eee_active;
	u32 eee_enabled;
};


struct ethtool_ops
{
	int     (*get_settings)(struct net_device *, struct ethtool_cmd *);
	int     (*set_settings)(struct net_device *, struct ethtool_cmd *);
	void    (*get_drvinfo)(struct net_device *, struct ethtool_drvinfo *);
	int     (*get_regs_len)(struct net_device *);
	void    (*get_regs)(struct net_device *, struct ethtool_regs *, void *);
	int     (*nway_reset)(struct net_device *);
	u32     (*get_link)(struct net_device *);
	int     (*get_eeprom_len)(struct net_device *);
	int     (*get_eeprom)(struct net_device *, struct ethtool_eeprom *, u8 *);
	int     (*set_eeprom)(struct net_device *, struct ethtool_eeprom *, u8 *);
	u32     (*get_msglevel)(struct net_device *);
	void    (*set_msglevel)(struct net_device *, u32);
	void    (*get_wol)(struct net_device *, struct ethtool_wolinfo *);
	int     (*set_wol)(struct net_device *, struct ethtool_wolinfo *);
	int     (*get_ts_info)(struct net_device *, struct ethtool_ts_info *);
	int     (*get_eee)(struct net_device *, struct ethtool_eee *);
	int     (*set_eee)(struct net_device *, struct ethtool_eee *);
};

__u32 ethtool_cmd_speed(const struct ethtool_cmd *);
u32 ethtool_op_get_link(struct net_device *);
int ethtool_op_get_ts_info(struct net_device *, struct ethtool_ts_info *);


/***********************
 ** linux/netdevice.h **
 ***********************/

#include <linux/netdev_features.h>

#define netif_err(priv, type, dev, fmt, args...) lx_printf("netif_err: " fmt, ## args);
#define netif_info(priv, type, dev, fmt, args...) lx_printf("netif_info: " fmt, ## args);

#define netdev_err(dev, fmt, args...) lx_printf("nedev_err: " fmt, ##args)
#define netdev_warn(dev, fmt, args...) lx_printf("nedev_warn: " fmt, ##args)
#define netdev_info(dev, fmt, args...) lx_printf("nedev_info: " fmt, ##args)

#define netdev_for_each_mc_addr(a, b) if (0)

#if DEBUG_LINUX_PRINTK
#define netif_dbg(priv, type, dev, fmt, args...) lx_printf("netif_dbg: "  fmt, ## args)
#define netdev_dbg(dev, fmt, args...)  lx_printf("netdev_dbg: " fmt, ##args)
#else
#define netif_dbg(priv, type, dev, fmt, args...)
#define netdev_dbg(dev, fmt, args...)
#endif

#define SET_NETDEV_DEV(net, pdev)        ((net)->dev.parent = (pdev))
#define SET_NETDEV_DEVTYPE(net, devtype) ((net)->dev.type = (devtype))

enum netdev_tx { NETDEV_TX_OK = 0 };
typedef enum netdev_tx netdev_tx_t;

enum {
	MAX_ADDR_LEN    = 32,
	NET_RX_SUCCESS  = 0,
	NET_ADDR_RANDOM = 1,

	NETIF_MSG_DRV   = 0x1,
	NETIF_MSG_PROBE = 0x2,
	NETIF_MSG_LINK  = 0x4,
};

struct net_device_ops
{
	int (*ndo_open)(struct net_device *dev);
	int (*ndo_stop)(struct net_device *dev);
	netdev_tx_t (*ndo_start_xmit) (struct sk_buff *skb, struct net_device *dev);
	void        (*ndo_set_rx_mode)(struct net_device *dev);
	int         (*ndo_set_mac_address)(struct net_device *dev, void *addr);
	int         (*ndo_validate_addr)(struct net_device *dev);
	int         (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
	void        (*ndo_tx_timeout) (struct net_device *dev);
	int         (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
	int         (*ndo_set_features)(struct net_device *dev, netdev_features_t features);
};

struct net_device_stats
{
	unsigned long rx_packets;
	unsigned long tx_packets;
	unsigned long rx_bytes;
	unsigned long tx_bytes;
	unsigned long rx_errors;
	unsigned long tx_errors;
	unsigned long rx_dropped;
	unsigned long tx_dropped;
	unsigned long rx_length_errors;
	unsigned long rx_over_errors;
	unsigned long rx_crc_errors;
	unsigned long rx_frame_errors;
};

/* NET_DEVICE */
struct net_device
{
	char           name[IFNAMSIZ];
	u32            features;
	u32            hw_features;

	struct net_device_stats      stats;
	const struct net_device_ops *netdev_ops;
	const struct ethtool_ops *ethtool_ops;

	unsigned long  state;

	unsigned int   flags;
	unsigned short hard_header_len; /* hardware hdr length  */
	unsigned int   mtu;
	unsigned short needed_headroom;
	unsigned short needed_tailroom;
	unsigned char  perm_addr[MAX_ADDR_LEN];
	unsigned char  addr_assign_type;
	unsigned char *dev_addr;
	unsigned char  _dev_addr[ETH_ALEN];
	unsigned long  trans_start;    /* Time (in jiffies) of last Tx */
	int            watchdog_timeo; /* used by dev_watchdog() */
	struct device  dev;
	void          *priv;
	unsigned       net_ip_align;

	struct phy_device *phydev;
};


struct netdev_hw_addr
{
	unsigned char addr[MAX_ADDR_LEN];
};

enum netdev_state_t {
	__LINK_STATE_START,
	__LINK_STATE_PRESENT,
	__LINK_STATE_NOCARRIER,
	__LINK_STATE_LINKWATCH_PENDING,
	__LINK_STATE_DORMANT,
};

#define netif_msg_tx_err(p) ({ printk("netif_msg_tx_err called not implemented\n"); 0; })
#define netif_msg_rx_err(p) ({ printk("netif_msg_rx_err called not implemented\n"); 0; })
#define netif_msg_tx_queued(p) ({ printk("netif_msg_tx_queued called not implemented\n"); 0; })

u32 netif_msg_init(int, int);

static inline void *netdev_priv(const struct net_device *dev) { return dev->priv; }

int  netif_running(const struct net_device *);
int  netif_carrier_ok(const struct net_device *dev);
int  netif_device_present(struct net_device *);

void netif_carrier_on(struct net_device *dev);
void netif_carrier_off(struct net_device *dev);

void netif_device_detach(struct net_device *);
void netif_start_queue(struct net_device *);
void netif_stop_queue(struct net_device *);
void netif_wake_queue(struct net_device *);
void netif_device_attach(struct net_device *);
void unregister_netdev(struct net_device *);
void free_netdev(struct net_device *);
int  netif_rx(struct sk_buff *);
void netif_tx_wake_all_queues(struct net_device *);

int netdev_mc_empty(struct net_device *);
unsigned netdev_mc_count(struct net_device * dev);
int register_netdev(struct net_device *);

/*****************
 ** linux/mii.h **
 *****************/

enum {
	FLOW_CTRL_TX = 0x1,
	FLOW_CTRL_RX = 0x2,

	MII_BMCR      = 0x0,
	MII_BMSR      = 0x1,
	MII_PHYSID1   = 0x2,
	MII_PHYSID2   = 0x3,
	MII_ADVERTISE = 0x4,
	MII_LPA       = 0x5,
	MII_CTRL1000  = 0x9,
	MII_MMD_CTRL  = 0xd,
	MII_MMD_DATA  = 0xe,
	MII_PHYADDR   = 0x19,

	MII_MMD_CTRL_NOINCR = 0x4000,

	BMCR_RESET    = 0x8000, /* reset to default state */
	BMCR_ANENABLE = 0x1000, /* enable auto negotiation */

	BMSR_LSTATUS = 0x4,

	ADVERTISE_PAUSE_CAP  = 0x0400, /* try for pause */
	ADVERTISE_CSMA       = 0x0001, /* only selector supported */
	ADVERTISE_PAUSE_ASYM = 0x0800, /* try for asymetric pause  */
	ADVERTISE_10HALF     = 0x0020,
	ADVERTISE_10FULL     = 0x0040,
	ADVERTISE_100HALF    = 0x0080,
	ADVERTISE_100FULL    = 0x0100,
	ADVERTISE_1000FULL   = 0x0200,
	ADVERTISE_ALL        = ADVERTISE_10HALF | ADVERTISE_10FULL |
	                       ADVERTISE_100HALF | ADVERTISE_100FULL
};

struct mii_if_info
{
	int phy_id;
	int phy_id_mask;
	int reg_num_mask;
	struct net_device *dev;
	int (*mdio_read) (struct net_device *dev, int phy_id, int location);
	void (*mdio_write) (struct net_device *dev, int phy_id, int location, int val);

	unsigned int supports_gmii : 1; /* are GMII registers supported? */
};


unsigned int mii_check_media (struct mii_if_info *, unsigned int,
                              unsigned int);
int mii_ethtool_gset(struct mii_if_info *, struct ethtool_cmd *);
int mii_ethtool_sset(struct mii_if_info *, struct ethtool_cmd *);
u8  mii_resolve_flowctrl_fdx(u16, u16);
int mii_nway_restart (struct mii_if_info *);
int mii_link_ok (struct mii_if_info *);

struct mii_ioctl_data { };
int generic_mii_ioctl(struct mii_if_info *,
                      struct mii_ioctl_data *, int,
                      unsigned int *);
struct mii_ioctl_data *if_mii(struct ifreq *);


/***********************
 ** uapi/linux/mdio.h **
 ***********************/

enum {
	MDIO_MMD_PCS       = 3,
	MDIO_MMD_AN        = 7,
	MDIO_PCS_EEE_ABLE  = 20,
	MDIO_AN_EEE_ADV    = 60,
	MDIO_AN_EEE_LPABLE = 61,
};


u32 mmd_eee_cap_to_ethtool_sup_t(u16 eee_cap);
u32 mmd_eee_adv_to_ethtool_adv_t(u16 eee_adv);
u16 ethtool_adv_to_mmd_eee_adv_t(u32 adv);


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
void tasklet_kill(struct tasklet_struct *);
void tasklet_init(struct tasklet_struct *t, void (*)(unsigned long), unsigned long);


/*************************
 ** linux/etherdevice.h **
 *************************/

int eth_mac_addr(struct net_device *, void *);
int eth_validate_addr(struct net_device *);
__be16 eth_type_trans(struct sk_buff *, struct net_device *);
int is_valid_ether_addr(const u8 *);

void random_ether_addr(u8 *addr);

struct net_device *alloc_etherdev(int);

void eth_hw_addr_random(struct net_device *dev);
void eth_random_addr(u8 *addr);

bool ether_addr_equal(const u8 *addr1, const u8 *addr2);


/********************
 ** asm/checksum.h **
 ********************/

__wsum csum_partial(const void *, int, __wsum);
__sum16 csum_fold(__wsum);


/********************
 ** linux/socket.h **
 ********************/

struct sockaddr {
	unsigned short sa_family;
	char           sa_data[14];
};


/*****************
 ** linux/idr.h **
 *****************/

#define DEFINE_IDA(name) struct ida name;
struct ida { };

int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end,
                   gfp_t gfp_mask);
void ida_simple_remove(struct ida *ida, unsigned int id);

/*******************
 ** linux/async.h **
 *******************/

struct async_domain { };

#define ASYNC_DOMAIN(name) struct async_domain name = { };
void async_unregister_domain(struct async_domain *domain);
#define ASYNC_DOMAIN_EXCLUSIVE(_name)



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

int bitmap_subset(const unsigned long *,
                  const unsigned long *, int);

int bitmap_weight(const unsigned long *src, unsigned int nbits);

/*******************
 ** linux/crc16.h **
 *******************/

u16 crc16(u16 crc, const u8 *buffer, size_t len);


/*******************
 ** linux/crc32.h **
 *******************/

u32 ether_crc(int, unsigned char *);

/*******************
 ** linux/birev.h **
 *******************/

u16 bitrev16(u16 in);


/******************
 ** linux/phy.h  **
 ******************/

typedef enum {
	PHY_INTERFACE_MODE_MII = 1,
} phy_interface_t;

struct phy;

int phy_init(struct phy *phy);
int phy_exit(struct phy *phy);

struct phy *phy_get(struct device *dev, const char *string);
void   phy_put(struct phy *phy);

int phy_power_on(struct phy *phy);
int phy_power_off(struct phy *phy);

/************************
 ** linux/usb/gadget.h **
 ************************/

struct usb_ep { };
struct usb_request { };
struct usb_gadget { };


/****************
 ** linux/of.h **
 ****************/

bool of_property_read_bool(const struct device_node *np, const char *propname);


/**********************
 ** linux/property.h **
 **********************/

int device_property_read_string(struct device *dev, const char *propname,
                                const char **val);
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
int   radix_tree_preload(gfp_t gfp_mask);
void  radix_tree_preload_end(void);
int   radix_tree_maybe_preload(gfp_t gfp_mask);


/**********************************
 ** Platform specific defintions **
 **********************************/

#include <platform/lx_emul.h>

 
/**********
 ** misc **
 **********/

static inline void dump_stack(void) { }



/**
 * Genode's evdev event handler
 */
enum input_event_type {
	EVENT_TYPE_PRESS, EVENT_TYPE_RELEASE, /* key press and release */
	EVENT_TYPE_MOTION,                    /* any type of (pointer) motion */
	EVENT_TYPE_WHEEL,                     /* mouse scroll wheel */
	EVENT_TYPE_TOUCH                      /* touchscreen events */
};

struct input_handle;

/**
 * Input event callback
 *
 * \param   type        input event type
 * \param   code        key code if type is EVENT_TYPE_PRESS or
 *                      EVENT_TYPE_RELEASE
 * \param   absolute_x  absolute horizontal coordinate if type is
 *                      EVENT_TYPE_MOTION
 * \param   absolute_y  absolute vertical coordinate if type is
 *                      EVENT_TYPE_MOTION
 * \param   relative_x  relative horizontal coordinate if type is
 *                      EVENT_TYPE_MOTION or EVENT_TYPE_WHEEL
 * \param   relative_y  relative vertical coordinate if type is
 *                      EVENT_TYPE_MOTION or EVENT_TYPE_WHEEL
 *
 * Key codes conform to definitions in os/include/input/event.h, which is C++
 * and therefore not included here.
 *
 * Relative coordinates are only significant if absolute_x and absolute_y are
 * 0.
 */
typedef void (*genode_input_event_cb)(enum input_event_type type,
                                      unsigned code,
                                      int absolute_x, int absolute_y,
                                      int relative_x, int relative_y);

/**
 * Register input handle
 *
 * \param   handler  call-back function on input events
 * \param   res_x    pixels of screen (width)  - used by usb touch devices
 * \param   res_y    pixels of screen (height) - used by usb touch devices
 *
 * \return  0 on success; !0 otherwise
 */
void genode_input_register(genode_input_event_cb handler, unsigned long res_x,
                           unsigned long res_y, bool multitouch);


void genode_evdev_event(struct input_handle *handle, unsigned int type,
                        unsigned int code, int value);

void start_input_service(void *ep, void *);


/******************
 ** asm/ptrace.h **
 ******************/

/*
 * Needed by 'dwc_otg_hcd_linux.c'
 */
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


/*********************
 ** otg_whitelist.h **
 *********************/

struct usb_device;
static inline int is_targeted(struct usb_device *dev) { return 0; }

/************************
 ** linux/tracepoint.h **
 ************************/

#define TRACE_EVENT(name, proto, args, struct, assign, print)
#define DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print)
#define DEFINE_EVENT(template, name, proto, args)

/* needed by drivers/net/wireless/iwlwifi/iwl-devtrace.h */
#define TP_PROTO(args...)     args
#define TP_STRUCT__entry(args...) (args)
#define TP_ARGS(args...)      (args)
#define TP_printk(fmt, args...) (fmt "\n" args)
#define TP_fast_assign(args...) (args)


/*******************
 ** Tracing stuff **
 *******************/

static inline void trace_xhci_cmd_completion(void *p1, void *p2) { }
static inline void trace_xhci_address_ctx(void *p1, void *p2, unsigned long v) { }

static inline void trace_xhci_dbg_init(struct va_format *v) { }
static inline void trace_xhci_dbg_ring_expansion(struct va_format *v) { }
static inline void trace_xhci_dbg_context_change(struct va_format *v) { }
static inline void trace_xhci_dbg_cancel_urb(struct va_format *v) { }
static inline void trace_xhci_dbg_reset_ep(struct va_format *v) { }
static inline void trace_xhci_dbg_quirks(struct va_format *v) { }
static inline void trace_xhci_dbg_address(struct va_format *v) { }

static inline void trace_dwc3_readl(struct va_format *v) { }
static inline void trace_dwc3_writel(struct va_format *v) { }
static inline void trace_dwc3_core(struct va_format *v) { }

void backtrace(void);

#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_H_ */
