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
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL_H_
#define _LX_EMUL_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* DDE Kit includes */
#include <dde_kit/types.h>
#include <dde_kit/printf.h>
#include <dde_kit/panic.h>
#include <dde_kit/lock.h>
#include <dde_kit/spin_lock.h>
#include <dde_kit/timer.h>
#include <dde_kit/resources.h>

#include <linux/usb/storage.h>

#define VERBOSE_LX_EMUL 0


#if VERBOSE_LX_EMUL
#define DEBUG_COMPLETION 0
#define DEBUG_DMA        0
#define DEBUG_DRIVER     0
#define DEBUG_IRQ        0
#define DEBUG_KREF       0
#define DEBUG_PCI        0
#define DEBUG_SLAB       0
#define DEBUG_TIMER      0
#define DEBUG_THREAD     0
#else
#define DEBUG_COMPLETION 0
#define DEBUG_DRIVER     0
#define DEBUG_DMA        0
#define DEBUG_IRQ        0
#define DEBUG_KREF       0
#define DEBUG_SLAB       0
#define DEBUG_PCI        0
#define DEBUG_TIMER      0
#define DEBUG_THREAD     0
#endif

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(3,2,2)

#define KBUILD_MODNAME "mod-noname"

/***************
 ** asm/bug.h **
 ***************/

#define WARN_ON(condition) ({ \
	int ret = !!(condition); \
	if (ret) dde_kit_debug("[%s] WARN_ON(" #condition ") ", __func__); \
	ret; })

#define WARN(condition, fmt, arg...) ({ \
	int ret = !!(condition); \
	if (ret) dde_kit_debug("[%s] *WARN* " fmt , __func__ , ##arg); \
	ret; })

#define BUG() do { \
	dde_kit_debug("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	while (1); \
} while (0)

#define WARN_ON_ONCE WARN_ON
#define WARN_ONCE    WARN

#define BUG_ON(condition) do { if (condition) BUG(); } while(0)


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

typedef dde_kit_int8_t   int8_t;
typedef dde_kit_uint8_t  uint8_t;
typedef dde_kit_int16_t  int16_t;
typedef dde_kit_uint16_t uint16_t;
typedef dde_kit_int32_t  int32_t;
typedef dde_kit_uint32_t uint32_t;
typedef dde_kit_size_t   size_t;
typedef dde_kit_int64_t  int64_t;
typedef dde_kit_uint64_t uint64_t;

typedef uint32_t uint;

typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;

typedef int8_t   __s8;
typedef uint8_t  __u8;
typedef int16_t  __s16;
typedef uint16_t __u16;
typedef int32_t  __s32;
typedef uint32_t __u32;
typedef int64_t  __s64;
typedef uint64_t __u64;

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u16 __be16;
typedef __u32 __be32;
typedef __u64 __be64;

typedef u64 sector_t;

struct list_head {
	struct list_head *next, *prev;
};

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#ifndef __cplusplus
typedef int bool;
enum { true = 1, false = 0 };
#endif /* __cplusplus */

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif /* __cplusplus */
#endif /* NULL */

typedef unsigned       gfp_t;
typedef unsigned long  pgoff_t;
typedef long long      loff_t;
typedef long           ssize_t;
typedef unsigned long  uintptr_t;
typedef int            dev_t;
typedef size_t         resource_size_t;
typedef long           off_t;
typedef int            pid_t;
typedef unsigned       fmode_t;
typedef u32            uid_t;
typedef u32            gid_t;
typedef long           __kernel_time_t;
typedef unsigned short umode_t;
typedef __u16          __be16;
typedef __u32          __be32;

#ifndef __cplusplus
typedef u16            wchar_t;
#endif

/*
 * needed by usb.h
 */
typedef unsigned long dma_addr_t;

/*
 * XXX 'mode_t' is 'unsigned int' on x86_64
 */
typedef unsigned short mode_t;


/******************
 ** asm/system.h **
 ******************/

#define mb()  asm volatile ("": : :"memory")
#define rmb() mb()
#define wmb() asm volatile ("": : :"memory")
#define smp_wmb() wmb()

static inline void barrier() { mb(); }


/**********************
 ** linux/compiler.h **
 **********************/

#define likely
#define unlikely

#define __user   /* needed by usb/core/devices.c */
#define __iomem  /* needed by usb/hcd.h */

#define __releases(x) /* needed by usb/core/devio.c */
#define __acquires(x) /* needed by usb/core/devio.c */
#define __force
#define __maybe_unused
#define __bitwise

#define __rcu
#define __must_check

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

#define __attribute_const__

#undef  __always_inline
#define __always_inline

#undef __unused


/***********************
 ** linux/irqreturn.h **
 ***********************/

enum irqreturn {
	IRQ_NONE,
	IRQ_HANDLED = (1 << 0),
};

typedef enum irqreturn irqreturn_t;


/*************************************
 ** linux/byteorder/little_endian.h **
 *************************************/

#include <linux/byteorder/little_endian.h>


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

#define le16_to_cpu  __le16_to_cpu
#define be16_to_cpu  __be16_to_cpu
#define le32_to_cpu  __le32_to_cpu
#define be32_to_cpu  __be32_to_cpu
#define le16_to_cpus __le16_to_cpus
#define cpu_to_le16p __cpu_to_le16p
#define cpu_to_be16p __cpu_to_be16p
#define cpu_to_le16  __cpu_to_le16
#define cpu_to_be16  __cpu_to_be16
#define cpu_to_le32  __cpu_to_le32
#define cpu_to_be32  __cpu_to_be32
#define le16_to_cpup __le16_to_cpup
#define be16_to_cpup __be16_to_cpup
#define le32_to_cpup __le32_to_cpup
#define be32_to_cpup __be32_to_cpup


struct __una_u32 { u32 x; } __attribute__((packed));
struct __una_u64 { u64 x; } __attribute__((packed));

u16 get_unaligned_le16(const void *p);

void put_unaligned_le32(u32 val, void *p);
u32  get_unaligned_le32(const void *p);

void put_unaligned_le64(u64 val, void *p);
u64 get_unaligned_le64(const void *p);

#define put_unaligned put_unaligned_le32
#define get_unaligned get_unaligned_le32


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


/********************
 ** linux/poison.h **
 ********************/

/*
 * In list.h, LIST_POISON1 and LIST_POISON2 are assigned to 'struct list_head
 * *' as well as 'struct hlist_node *' pointers. Consequently, C++ compiler
 * produces an error "cannot convert... in assignment". To compile 'list.h'
 * included by C++ source code, we have to define these macros to the only
 * value universally accepted for pointer assigments.h
 */

#ifdef __cplusplus
#define LIST_POISON1 0
#define LIST_POISON2 0
#else
#define LIST_POISON1 ((void *)0x00100100)
#define LIST_POISON2 ((void *)0x00200200)
#endif  /* __cplusplus */


/*******************************
 ** linux/errno.h and friends **
 *******************************/

/**
 * Error codes
 *
 * Note that the codes do not correspond to those of the Linux kernel.
 */
enum {
	EINVAL       =  1,
	ENODEV       =  2,
	ENOMEM       =  3,
	EFAULT       =  4,
	EBADF        =  5,
	EAGAIN       =  6,
	ERESTARTSYS  =  7,
	ENOSPC       =  8,
	EIO          =  9,
	EBUSY        = 10,
	EPERM        = 11,
	EINTR        = 12,
	ENOMSG       = 13,
	ECONNRESET   = 14,
	ENOENT       = 15,
	EHOSTUNREACH = 16,
	ESRCH        = 17,
	EPIPE        = 18,
	ENODATA      = 19,
	EREMOTEIO    = 20,
	ENOTTY       = 21,
	ENOIOCTLCMD  = 22,
	EADDRINUSE   = 23,
	ENFILE       = 23,
	EXFULL       = 24,
	EIDRM        = 25,
	ESHUTDOWN    = 26,
	EMSGSIZE     = 27,
	E2BIG        = 28,
	EINPROGRESS  = 29,
	ESPIPE       = 29,
	ETIMEDOUT    = 30,
	ENOSYS       = 31,
	ENOTCONN     = 32,
	EPROTO       = 33,
	ENOTSUPP     = 34,
	EISDIR       = 35,
	EEXIST       = 36,
	ENOTEMPTY    = 37,
	ENXIO        = 38,
	ENOEXEC      = 39,
	EXDEV        = 40,
	EOVERFLOW    = 41,
	ENOSR        = 42,
	ECOMM        = 43,
	EFBIG        = 44,
	EILSEQ       = 45,
	ETIME        = 46,
	EALREADY     = 47,
	EOPNOTSUPP   = 48,
};

static inline bool IS_ERR(void *ptr) {
	return (unsigned long)(ptr) > (unsigned long)(-1000); }

long PTR_ERR(const void *ptr);


/*******************
 ** linux/major.h **
 *******************/

enum {
	INPUT_MAJOR = 13
};

/********************
 ** linux/kernel.h **
 ********************/

/*
 * Log tags
 */
#define KERN_DEBUG  "DEBUG: "
#define KERN_ERR    "ERROR: "
#define KERN_INFO   "INFO: "
#define KERN_NOTICE "NOTICE: "

/*
 * Debug macros
 */
#if VERBOSE_LX_EMUL
#define printk  dde_kit_printf
#define vprintk dde_kit_vprintf
#define panic   dde_kit_panic
#else /* VERBOSE_LX_EMUL */
#define printk(...)
#define vprintk(...)
#define panic(...)
#endif /* VERBOSE_LX_EMUL */

/*
 * Bits and types
 */

/* needed by linux/list.h */
#define container_of(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})

/* normally provided by linux/stddef.h, needed by linux/list.h */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1: __max2; })

/**
 * Return minimum of two given values
 *
 * XXX check how this function is used (argument types)
 */
static inline size_t min(size_t a, size_t b) {
	return a < b ? a : b; }

/**
 * Return maximum of two given values
 *
 * XXX check how this function is used (argument types)
 */
#define max(x, y) ({                      \
	typeof(x) _max1 = (x);                  \
	typeof(y) _max2 = (y);                  \
	(void) (&_max1 == &_max2);              \
	_max1 > _max2 ? _max1 : _max2; })

#define min_t(type, x, y) ({ \
	type __min1 = (x); \
	type __min2 = (y); \
	__min1 < __min2 ? __min1: __min2; })

#define abs(x) ( { \
                  typeof (x) _x = (x); \
                  _x < 0 ? -_x : _x;  })

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void might_sleep();

#define INT_MAX ((int)(~0U>>1))

char *kasprintf(gfp_t gfp, const char *fmt, ...);
int kstrtouint(const char *s, unsigned int base, unsigned int *res);

#define clamp(val, min, max) ({                 \
	typeof(val) __val = (val);              \
	typeof(min) __min = (min);              \
	typeof(max) __max = (max);              \
	(void) (&__val == &__min);              \
	(void) (&__val == &__max);              \
	__val = __val < __min ? __min: __val;   \
	__val > __max ? __max: __val; })

int strict_strtoul(const char *s, unsigned int base, unsigned long *res);
long simple_strtoul(const char *cp, char **endp, unsigned int base);


/*
 * Needed by 'usb.h'
 */
int snprintf(char *buf, size_t size, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int sscanf(const char *, const char *, ...);
int scnprintf(char *buf, size_t size, const char *fmt, ...);

struct completion;
void complete_and_exit(struct completion *, long);

/******************
 ** linux/log2.h **
 ******************/

int ilog2(u32 n);
int  roundup_pow_of_two(u32 n);


/********************
 ** linux/kdev_t.h **
 ********************/

#define MINORBITS 20
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))


/********************
 ** linux/printk.h **
 ********************/

enum { DUMP_PREFIX_NONE };

void print_hex_dump(const char *level, const char *prefix_str,
                    int prefix_type, int rowsize, int groupsize,
                    const void *buf, size_t len, bool ascii);

#define KERN_WARNING "<4>"
#define pr_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_warn pr_warning

#define printk_ratelimit() \
	({ dde_kit_debug("printk_ratelimit called - not implemented\n"); (0); })

static inline bool printk_timed_ratelimit(unsigned long *caller_jiffies,
                                          unsigned int interval_msec) {
	return false; }


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

#include <asm-generic/bitops/__ffs.h>

#define BIT(nr)	      (1UL << (nr))
#define BITS_PER_LONG (sizeof(long) * 8)

#define BIT_MASK(nr)  (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)  ((nr) / BITS_PER_LONG)

#include <asm-generic/bitops/non-atomic.h>

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
long find_next_zero_bit_le(const void *addr,
                           unsigned long size, unsigned long offset);

#define find_next_zero_bit find_next_zero_bit_le

/* normally declared in asm-generic/bitops/ffs.h */
int ffs(int x);
int fls(int x);


/********************
 ** linux/string.h **
 ********************/

#ifndef __cplusplus
void  *memcpy(void *dest, const void *src, size_t n);
#endif

void  *memset(void *s, int c, size_t n);
int    memcmp(const void *, const void *, size_t);
void  *memscan(void *addr, int c, size_t size);
char  *strcat(char *dest, const char *src);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *cs, const char *ct, size_t count);
char  *strncpy(char *, const char *, size_t);
char  *strchr(const char *, int);
char  *strrchr(const char *,int);
size_t strlcat(char *dest, const char *src, size_t n);
size_t strlcpy(char *dest, const char *src, size_t size);
size_t strlen(const char *);
char * strsep(char **,const char *);
char *strstr(const char *, const char *);
char  *kstrdup(const char *s, gfp_t gfp);
void  *kmemdup(const void *src, size_t len, gfp_t gfp);


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

#define __init
#define __initdata
#define __devinit
#define __devinitconst
#define __devexit
#define __exit

#define __exit_p(x) x

#define subsys_initcall(fn) void subsys_##fn(void) { fn(); }


/********************
 ** linux/module.h **
 ********************/

#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)
#define MODULE_PARM_DESC(x, y)
//#define MODULE_ALIAS_MISCDEV(x)  /* needed by agp/backend.c */
#define MODULE_ALIAS(x)

#define THIS_MODULE 0

#define MODULE_DEVICE_TABLE(type, name)

struct module;
#define _MOD_CONCAT(a,b) module_##a##b
#define MOD_CONCAT(a,b) _MOD_CONCAT(a,b)
#define module_init(fn) void MOD_CONCAT(fn, MOD_SUFFIX)(void) { fn(); }
#define module_exit(exit_fn) void MOD_CONCAT(exit_fn, MOD_SUFFIX)(void) { exit_fn(); }

static inline void module_put(struct module *module) { }
static inline void __module_get(struct module *module) { }

/*************************
 ** linux/moduleparam.h **
 *************************/

#define module_param(name, type, perm)
#define module_param_named(name, value, type, perm)
#define module_param_array_named(name, array, type, nump, perm)
#define module_param_string(name, string, len, perm)
#define core_param(name, var, type, perm)


/******************
 ** linux/slab.h **
 ******************/

enum {
	SLAB_HWCACHE_ALIGN = 0x00002000UL,
	SLAB_CACHE_DMA     = 0x00004000UL,
};

void *kzalloc(size_t size, gfp_t flags);
void  kfree(const void *);
void *kmalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);

struct kmem_cache;

/**
 * Create slab cache using DDE kit
 */
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
                                     unsigned long, void (*)(void *));

/**
 * Destroy slab cache using DDE kit
 */
void kmem_cache_destroy(struct kmem_cache *);

/**
 * Allocate and zero slab
 */
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);

/**
 * Free slab
 */
void  kmem_cache_free(struct kmem_cache *, void *);


/**********************
 ** linux/spinlock.h **
 **********************/

typedef dde_kit_spin_lock spinlock_t;
#define DEFINE_SPINLOCK(name) spinlock_t name = 0;

void spin_lock(spinlock_t *lock);
void spin_lock_nested(spinlock_t *lock, int subclass);
void spin_unlock(spinlock_t *lock);
void spin_lock_init(spinlock_t *lock);
void spin_lock_irqsave(spinlock_t *lock, unsigned long flags);
void spin_lock_irqrestore(spinlock_t *lock, unsigned long flags);
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);
void spin_lock_irq(spinlock_t *lock);
void spin_unlock_irq(spinlock_t *lock);
void assert_spin_locked(spinlock_t *lock);


/*******************
 ** linux/mutex.h **
 *******************/

struct mutex { struct dde_kit_lock *lock; };

void mutex_init(struct mutex *m);
void mutex_lock(struct mutex *m);
void mutex_lock_nested(struct mutex *lock, unsigned int subclass);
void mutex_unlock(struct mutex *m);
int  mutex_lock_interruptible(struct mutex *m);

#define DEFINE_MUTEX(mutexname) struct mutex mutexname = { NULL };


/*******************
 ** linux/rwsem.h **
 *******************/

struct rw_semaphore { int dummy; };

#define DECLARE_RWSEM(name) \
	struct rw_semaphore name = { 0 };

void down_read(struct rw_semaphore *sem);
void up_read(struct rw_semaphore *sem);
void down_write(struct rw_semaphore *sem);
void up_write(struct rw_semaphore *sem);

#define __RWSEM_INITIALIZER(name) { 0 }


/*********************
 ** linux/jiffies.h **
 *********************/

/*
 * XXX check how the jiffies variable is used
 */
extern volatile unsigned long jiffies;
unsigned long msecs_to_jiffies(const unsigned int m);
long time_after(long a, long b);
long time_after_eq(long a, long b);


/*******************
 ** linux/ktime.h **
 *******************/

union ktime { s64 tv64; };

typedef union ktime ktime_t;

ktime_t ktime_add_ns(const ktime_t kt, u64 nsec);

static inline ktime_t ktime_add_us(const ktime_t kt, const u64 usec)
{
	return ktime_add_ns(kt, usec * 1000);
}

s64 ktime_us_delta(const ktime_t later, const ktime_t earlier);


/*******************
 ** linux/timer.h **
 *******************/

struct timer_list {
	void (*function)(unsigned long);
	unsigned long data;
	void *timer;
};

void init_timer(struct timer_list *);
int mod_timer(struct timer_list *timer, unsigned long expires);
int del_timer(struct timer_list * timer);
int del_timer_sync(struct timer_list * timer);
void setup_timer(struct timer_list *timer,void (*function)(unsigned long),
                 unsigned long data);
int timer_pending(const struct timer_list * timer);
unsigned long round_jiffies(unsigned long j);


/*********************
 ** linux/hrtimer.h **
 *********************/

ktime_t ktime_get_real(void);


/*******************
 ** linux/delay.h **
 *******************/

void msleep(unsigned int msecs);
void udelay(unsigned long usecs);
void mdelay(unsigned long usecs);


/***********************
 ** linux/workquque.h **
 ***********************/

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	work_func_t func;
	struct list_head entry;
};

struct delayed_work {
	struct timer_list timer;
	struct work_struct work;
};

bool cancel_work_sync(struct work_struct *work);
int cancel_delayed_work_sync(struct delayed_work *work);
int schedule_delayed_work(struct delayed_work *work, unsigned long delay);
int schedule_work(struct work_struct *work);
bool flush_work_sync(struct work_struct *work);


#define PREPARE_WORK(_work, _func) \
	do { (_work)->func = (_func); } while (0)

#define PREPARE_DELAYED_WORK(_work, _func) \
	PREPARE_WORK(&(_work)->work, (_func))

#define __INIT_WORK(_work, _func, on_stack) \
	do { \
		INIT_LIST_HEAD(&(_work)->entry); \
		PREPARE_WORK((_work), (_func)); \
	} while (0)

#define INIT_WORK(_work, _func)\
	do { __INIT_WORK((_work), (_func), 0); } while (0)

#define INIT_DELAYED_WORK(_work, _func) \
	do { \
		INIT_WORK(&(_work)->work, (_func)); \
		init_timer(&(_work)->timer); \
	} while (0)


/******************
 ** linux/wait.h **
 ******************/

typedef struct wait_queue_head { int dummy; } wait_queue_head_t;

void init_waitqueue_head(wait_queue_head_t *q);

#define __WAIT_QUEUE_HEAD_INITIALIZER(name) { 0 }

typedef struct wait_queue { int dummy; } wait_queue_t;

#define __WAIT_QUEUE_INITIALIZER(name, tsk) { 0 }

#define DECLARE_WAITQUEUE(name, tsk) \
	wait_queue_t name = __WAIT_QUEUE_INITIALIZER(name, tsk)

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	wait_queue_head_t name = __WAIT_QUEUE_HEAD_INITIALIZER(name)

void __wake_up();
void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait);
void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait);

#define wake_up(x) __wake_up()
#define wake_up_all(x) __wake_up()
#define wake_up_interruptible(x) __wake_up()

extern wait_queue_t wait;
void breakpoint();

/* our wait event implementation */
void __wait_event(void);

#define wait_event(wq, condition) \
	({  dde_kit_printf("wait_event, not yet implemented\n"); 0; })

#define _wait_event(condition) \
	while(!(condition)) { \
		__wait_event();     \
	if (!(condition))     \
		msleep(1);          \
	}                     \

#define _wait_event_timeout(condition, timeout) \
({ \
	unsigned long _j = jiffies + (timeout / HZ); \
	while(1) { \
		__wait_event();     \
		if (condition || _j <= jiffies)     \
			break;          \
		msleep(1); \
	}  \
})

#define wait_event_interruptible(wq, condition) \
({                        \
	_wait_event(condition); \
	0;                      \
})

#define wait_event_interruptible_timeout(wq, condition, timeout) \
({                        \
	_wait_event_timeout(condition, timeout); \
	1;                      \
})

#define wait_event_timeout(wq, condition, timeout) \
({                                \
	_wait_event_timeout(condition, timeout); \
	1;                              \
})


/******************
 ** linux/time.h **
 ******************/

struct timespec {
	__kernel_time_t tv_sec;
	long            tv_nsec;
};

struct timeval { };
struct timespec current_kernel_time(void);
void do_gettimeofday(struct timeval *tv);

#define CURRENT_TIME (current_kernel_time())


/*******************
 ** linux/sched.h **
 *******************/

enum { TASK_RUNNING = 0, TASK_INTERRUPTIBLE = 1, TASK_NORMAL = 3 };

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
signed long schedule_timeout_uninterruptible(signed long timeout);
void yield(void);
int wake_up_process(struct task_struct *tsk);

/* normally defined in asm/current.h, included by sched.h */
extern struct task_struct *current;

/* asm/processor.h */
void cpu_relax(void);


/*********************
 ** linux/kthread.h **
 *********************/

int kthread_should_stop(void);
int kthread_stop(struct task_struct *k);

struct task_struct *kthread_run(int (*)(void *), void *, const char *, ...);

struct task_struct *kthread_create(int (*threadfn)(void *data),
                                   void *data,
                                   const char namefmt[], ...);


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

struct scatterlist {
	unsigned long page_link;
	unsigned int  offset;
	unsigned int  length;
	dma_addr_t    dma_address;
	int           last;
};

struct sg_table
{
	struct scatterlist *sgl;  /* the list */
	unsigned int nents;       /* number of mapped entries */
};

struct page        *sg_page(struct scatterlist *sg);
void               *sg_virt(struct scatterlist *sg);
struct scatterlist *sg_next(struct scatterlist *);

size_t sg_copy_from_buffer(struct scatterlist *sgl, unsigned int nents,
                           void *buf, size_t buflen);
size_t sg_copy_to_buffer(struct scatterlist *sgl, unsigned int nents,
                         void *buf, size_t buflen);

#define for_each_sg(sglist, sg, nr, __i) \
	for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

#define sg_dma_address(sg) ((sg)->dma_address)
#define sg_dma_len(sg)     ((sg)->length)


/******************
 ** linux/kref.h **
 ******************/

struct kref { atomic_t refcount; };

void kref_init(struct kref *kref);
void kref_get(struct kref *kref);
int  kref_put(struct kref *kref, void (*release) (struct kref *kref));


/*********************
 ** linux/kobject.h **
 *********************/

struct kobject { int dummy; };
struct kobj_uevent_env
{
	char buf[32];
	int buflen;
};

struct kobj_uevent_env;

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...);
char *kobject_name(const struct kobject *kobj);
char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask);


/*******************
 ** linux/sysfs.h **
 *******************/

struct attribute {
	const char *name;
	mode_t      mode;
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

int sysfs_create_group(struct kobject *kobj,
                       const struct attribute_group *grp);
void sysfs_remove_group(struct kobject *kobj,
                        const struct attribute_group *grp);


/****************
 ** linux/pm.h **
 ****************/

typedef struct pm_message { int event; } pm_message_t;

struct dev_pm_info { bool is_prepared; };


/************************
 ** linux/pm_runtime.h **
 ************************/

struct device;

int  pm_runtime_set_active(struct device *dev);
void pm_suspend_ignore_children(struct device *dev, bool enable);
void pm_runtime_enable(struct device *dev);
void pm_runtime_disable(struct device *dev);
void pm_runtime_set_suspended(struct device *dev);
void pm_runtime_get_noresume(struct device *dev);
void pm_runtime_put_noidle(struct device *dev);
void pm_runtime_use_autosuspend(struct device *dev);
int  pm_runtime_put_sync_autosuspend(struct device *dev);
void pm_runtime_no_callbacks(struct device *dev);


/***********************
 ** linux/pm_wakeup.h **
 ***********************/

int  device_init_wakeup(struct device *dev, bool val);
int  device_wakeup_enable(struct device *dev);
bool device_may_wakeup(struct device *dev);
int  device_set_wakeup_enable(struct device *dev, bool enable);
bool device_can_wakeup(struct device *dev);


/********************
 ** linux/device.h **
 ********************/

#ifdef __cplusplus
#define class device_class
#endif


#define dev_info(dev, format, arg...) dde_kit_printf("dev_info: "  format, ## arg)
#define dev_warn(dev, format, arg...) dde_kit_printf("dev_warn: "  format, ## arg)
#define dev_WARN(dev, format, arg...) dde_kit_printf("dev_WARN: "  format, ## arg)
#define dev_err( dev, format, arg...) dde_kit_printf("dev_error: " format, ## arg)
#define dev_notice(dev, format, arg...) dde_kit_printf("dev_notice: " format, ## arg)

#if VERBOSE_LX_EMUL
#define dev_dbg(dev, format, arg...) dde_kit_printf("dev_dbg: " format, ## arg)
#else
#define dev_dbg( dev, format, arg...)
#endif

#define dev_printk(level, dev, format, arg...) \
	dde_kit_printf("dev_printk: " format, ## arg)

enum {
	BUS_NOTIFY_ADD_DEVICE = 0x00000001,
	BUS_NOTIFY_DEL_DEVICE = 0x00000002,
};

struct device_driver;

struct bus_type {
	const char *name;
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
	int            (*probe)  (struct device *dev);
	int            (*remove) (struct device *dev);
};

struct kobj_uevent_env;

struct device_type {
	const char *name;
	const struct attribute_group **groups;
	void      (*release)(struct device *dev);
	int       (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	char     *(*devnode)(struct device *dev, mode_t *mode);
};

struct class
{
	const char *name;
	char *(*devnode)(struct device *dev, mode_t *mode);
};

/* DEVICE */
struct device {
	const char                    *name;
	struct device                 *parent;
	struct kobject                 kobj;
	const struct device_type      *type;
	struct device_driver          *driver;
	void                          *platform_data;
	u64                           *dma_mask; /* needed by usb/hcd.h */
	u64                            coherent_dma_mask; /* omap driver */
	struct dev_pm_info             power;
	dev_t                          devt;
	const struct attribute_group **groups;
	void (*release)(struct device *dev);
	struct bus_type               *bus;
	struct class                  *class;
	void                          *driver_data;
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

#define DRIVER_ATTR(_name, _mode, _show, _store) \
struct driver_attribute driver_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

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

#ifdef __cplusplus
#undef class
#endif


/*****************************
 ** linux/platform_device.h **
 *****************************/

struct platform_device;
void *platform_get_drvdata(const struct platform_device *pdev);


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

static inline u64 DMA_BIT_MASK(unsigned n) {
	return (n == 64) ? ~0ULL : (1ULL << n) - 1; }


/*********************
 ** linux/uaccess.h **
 *********************/

enum { VERIFY_READ = 0, VERIFY_WRITE = 1 };

bool access_ok(int access, void *addr, size_t size);

size_t copy_from_user(void *to, void const *from, size_t len);
size_t copy_to_user(void *dst, void const *src, size_t len);

#define get_user(x, ptr) ({ dde_kit_printf("get_user not implemented"); (0);})
#define put_user(x, ptr) ({ dde_kit_printf("put_user not implemented"); (0);})


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
	void                          *i_private;
	loff_t                         i_size;
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


/*****************
 ** linux/gfp.h **
 *****************/

enum {
	__GFP_DMA  = 0x01u,
	GFP_DMA    = __GFP_DMA,
	__GFP_WAIT = 0x10u,
	GFP_ATOMIC = 0x20u,
	GFP_KERNEL = 0x0u,
	GFP_NOIO   = __GFP_WAIT,
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

void *ioremap(resource_size_t offset, unsigned long size);
void  iounmap(volatile void *addr);

/**
 * Map I/O memory write combined
 */
void *ioremap_wc(resource_size_t phys_addr, unsigned long size);

#define ioremap_nocache ioremap_wc

#define writel(value, addr) (*(volatile uint32_t *)(addr) = (value))
#define readl(addr) (*(volatile uint32_t *)(addr))
#define readb(addr) (*(volatile uint8_t  *)(addr))

static inline void outb(u8  value, u32 port) { dde_kit_outb(port, value); }
static inline void outw(u16 value, u32 port) { dde_kit_outw(port, value); }
static inline void outl(u32 value, u32 port) { dde_kit_outl(port, value); }

static inline u8  inb(u32 port) { return dde_kit_inb(port); }
static inline u16 inw(u32 port) { return dde_kit_inw(port); }
static inline u32 inl(u32 port) { return dde_kit_inl(port); }

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

#define IORESOURCE_IO  0x00000100
#define IORESOURCE_MEM 0x00000200
#define IORESOURCE_IRQ 0x00000400

struct resource
{
	resource_size_t start;
	resource_size_t end;
	const char     *name;
	unsigned long   flags;
};

struct resource *request_region(resource_size_t start, resource_size_t n,
                                const char *name);
struct resource *request_mem_region(resource_size_t start, resource_size_t n,
                                    const char *name);

void release_region(resource_size_t start, resource_size_t n);
void release_mem_region(resource_size_t start, resource_size_t n);

resource_size_t resource_size(const struct resource *res);

/***********************
 ** linux/interrupt.h **
 ***********************/

#define IRQF_SHARED   0x00000080
#define IRQF_DISABLED 0x00000020

void local_irq_enable(void);
void local_irq_disable(void);

typedef irqreturn_t (*irq_handler_t)(int, void *);

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
                const char *name, void *dev);
void free_irq(unsigned int, void *);


/*********************
 ** linux/hardirq.h **
 *********************/

void synchronize_irq(unsigned int irq);


/*****************
 ** linux/pci.h **
 *****************/

#include <linux/pci_ids.h>
#include <linux/pci_regs.h>

/*
 * Definitions normally found in pci_regs.h
 */
//enum { PCI_BASE_ADDRESS_MEM_MASK = ~0x0fUL };
//enum { PCI_CAP_ID_AGP            = 0x02 };
extern struct  bus_type pci_bus_type;
enum { PCI_ANY_ID = ~0 };
enum { DEVICE_COUNT_RESOURCE = 6 };
//enum { PCIBIOS_MIN_MEM = 0UL };

#define PCI_DEVICE_CLASS(dev_class,dev_class_mask) \
	.class = (dev_class), .class_mask = (dev_class_mask), \
	.vendor = PCI_ANY_ID, .device = PCI_ANY_ID, \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)

#define PCI_DEVICE(vend,dev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

typedef enum { PCI_D0 = 0 } pci_power_t;

/*
 * Deal with C++ keyword used as member name of 'pci_dev'
 */
#ifdef __cplusplus
#define class device_class
#endif /* __cplusplus */

#include <linux/mod_devicetable.h>

/*
 * PCI types
 */
struct pci_bus;

struct pci_dev {
	unsigned int devfn;
	unsigned int irq;
	struct resource resource[DEVICE_COUNT_RESOURCE];
	struct pci_bus *bus; /* needed for i915_dma.c */
	unsigned short vendor;  /* needed for intel-agp.c */
	unsigned short device;
	unsigned int   class;    /* needed by usb/host/pci-quirks.c */
	u8             revision; /* needed for ehci-pci.c */
	struct device dev; /* needed for intel-agp.c */
	pci_power_t     current_state;
};

#ifdef __cplusplus
#undef class
#endif /* __cplusplus */

enum {
	PCI_ROM_RESOURCE = 6
};

/*
 * Interface functions provided by the EHCI USB HCD driver.
 */
struct pci_driver {
	char                        *name;
	const struct pci_device_id  *id_table;
	int                        (*probe)  (struct pci_dev *dev,
	                                      const struct pci_device_id *id);
	void                       (*remove) (struct pci_dev *dev);
	void                       (*shutdown) (struct pci_dev *dev);
	struct device_driver         driver;
};


static inline uint32_t PCI_DEVFN(unsigned slot, unsigned func) {
	return ((slot & 0x1f) << 3) | (func & 0x07); }

int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn, int where, u8 *val);
int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn, int where, u16 *val);
int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn, int where, u32 *val);
int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn, int where, u8 val);
int pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn, int where, u16 val);
int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn, int where, u32 val);

static inline
int pci_read_config_byte(struct pci_dev *dev, int where, u8 *val) {
	return pci_bus_read_config_byte(dev->bus, dev->devfn, where, val); }

static inline
int pci_read_config_word(struct pci_dev *dev, int where, u16 *val) {
	return pci_bus_read_config_word(dev->bus, dev->devfn, where, val); }

static inline
int pci_read_config_dword(struct pci_dev *dev, int where, u32 *val) {
	return pci_bus_read_config_dword(dev->bus, dev->devfn, where, val); }

static inline
int pci_write_config_byte(struct pci_dev *dev, int where, u8 val) {
	return pci_bus_write_config_byte(dev->bus, dev->devfn, where, val); }

static inline
int pci_write_config_word(struct pci_dev *dev, int where, u16 val) {
	return pci_bus_write_config_word(dev->bus, dev->devfn, where, val); }

static inline
int pci_write_config_dword(struct pci_dev *dev, int where, u32 val) {
	return pci_bus_write_config_dword(dev->bus, dev->devfn, where, val); }

size_t pci_resource_len(struct pci_dev *dev, unsigned bar);
size_t pci_resource_start(struct pci_dev *dev, unsigned bar);
void *pci_get_drvdata(struct pci_dev *pdev);
void pci_dev_put(struct pci_dev *dev);
struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device,
                               struct pci_dev *from);
int pci_enable_device(struct pci_dev *dev);
void pci_disable_device(struct pci_dev *dev);
int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask);
int pci_register_driver(struct pci_driver *driver);
void pci_unregister_driver(struct pci_driver *driver);
const char *pci_name(const struct pci_dev *pdev);
bool pci_dev_run_wake(struct pci_dev *dev);
unsigned int pci_resource_flags(struct pci_dev *dev, unsigned bar);
void pci_set_master(struct pci_dev *dev);
int pci_set_mwi(struct pci_dev *dev);
int pci_find_capability(struct pci_dev *dev, int cap);
struct pci_dev *pci_get_slot(struct pci_bus *bus, unsigned int devfn);
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
                                         struct pci_dev *dev);
void *pci_ioremap_bar(struct pci_dev *pdev, int bar);

#define to_pci_dev(n) container_of(n, struct pci_dev, dev)

#define DECLARE_PCI_FIXUP_FINAL(vendor, device, hook)

#define DEFINE_PCI_DEVICE_TABLE(_table) \
	const struct pci_device_id _table[] __devinitconst


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
 * Needed by usb/core/devio.h, used to calculate ioctl opcodes
 */
#include <asm-generic/ioctl.h>


/******************
 ** linux/cdev.h **
 ******************/

struct cdev { int dummy; };
void cdev_init(struct cdev *, const struct file_operations *);
int cdev_add(struct cdev *, dev_t, unsigned);
void cdev_del(struct cdev *);


/******************
 ** linux/stat.h **
 ******************/

#define S_IFMT   00170000
#define S_IFDIR   0040000
#define S_IFREG   0100000
#define S_ISVTX   0001000
#define S_IALLUGO 0007777

#define S_IRUGO   00444
#define S_IWUSR   00200
#define S_IXUGO   00111
#define S_IRWXUGO 00777

#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)


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

struct completion { unsigned int done; };
void complete(struct completion *);
void init_completion(struct completion *x);
unsigned long wait_for_completion_timeout(struct completion *x,
                                          unsigned long timeout);
void wait_for_completion(struct completion *x);
int wait_for_completion_interruptible(struct completion *x);
long wait_for_completion_interruptible_timeout(struct completion *x, unsigned long timeout);


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

#define hid_debug_init()         do { } while (0)
#define hid_dump_input(a,b,c)    do { } while (0)
#define hid_debug_event(a,b)     do { } while (0)
#define hid_debug_register(a, b) do { } while (0)
#define hid_debug_unregister(a)  do { } while (0)
#define hid_debug_exit()         do { } while (0)


/******************
 ** linux/list.h **
 ******************/

#define new _new
#include <linux/list.h>
#undef new


/********************
 ** linux/hidraw.h **
 ********************/

struct hidraw { u32 minor; };
struct hid_device;

static inline int hidraw_init(void) { return 0; }
static inline void hidraw_exit(void) { }
static inline void hidraw_report_event(struct hid_device *hid, u8 *data, int len) { }
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


/********************
 ** linux/random.h **
 ********************/

static inline void add_input_randomness(unsigned int type, unsigned int code,
                                        unsigned int value) { }


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
sector_t blk_rq_pos(const struct request *rq);
unsigned int queue_max_hw_sectors(struct request_queue *q);

#include <scsi/scsi_host.h>


/********************
 ** scsi/scsi_eh.h **
 *******************/

struct scsi_eh_save { };

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
	struct list_head list;  /* scsi_cmnd participates in queue lists */

	unsigned long serial_number;

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



/************************
 ** scsi/scsi_device.h **
 ************************/

struct scsi_target
{
	struct list_head devices;
	struct device    dev;
	unsigned int     channel;
	unsigned int     id;
	unsigned int     pdt_1f_for_no_lun;      /* PDT = 0x1f */
	unsigned int     target_blocked;
	char             scsi_level;
};

enum scsi_device_state
{
	SDEV_DEL, /* device deleted 
	           * no commands allowed */
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

	unsigned long         id, lun, channel;
	char                  type;
	char                  scsi_level;
	unsigned char         inquiry_len;            /* valid bytes in 'inquiry' */
	struct scsi_target   *sdev_target;            /* used only for single_lun */

	unsigned              lockable:1;             /* able to prevent media removal */
	unsigned              simple_tags:1;          /* simple queue tag messages are enabled */
	unsigned              ordered_tags:1;         /* ordered queue tag messages are enabled */
	unsigned              use_10_for_rw:1;        /* first try 10-byte read / write */
	unsigned              use_10_for_ms:1;        /* first try 10-byte mode sense/select */
	unsigned              skip_ms_page_8:1;       /* do not use MODE SENSE page 0x08 */
	unsigned              skip_ms_page_3f:1;      /* do not use MODE SENSE page 0x3f */
	unsigned              use_192_bytes_for_3f:1; /* ask for 192 bytes from page 0x3f */
	unsigned              allow_restart:1;        /* issue START_UNIT in error handler */
	unsigned              fix_capacity:1;         /* READ_CAPACITY is too high by 1 */
	unsigned              guess_capacity:1;       /* READ_CAPACITY might be too high by 1 */
	unsigned              no_read_capacity_16:1;  /* avoid READ_CAPACITY_16 cmds */
	unsigned              retry_hwerror:1;        /* retry HARDWARE_ERROR */
	unsigned              last_sector_bug:1;      /* do not use multisector accesses on
	                                                 SD_LAST_BUGGY_SECTORS */
	unsigned              no_read_disc_info:1;    /* avoid READ_DISC_INFO cmds */

	unsigned int          device_blocked;         /* device returned QUEUE_FULL. */

	atomic_t              iorequest_cnt;
	struct device         sdev_gendev;
	enum scsi_device_state sdev_state;
};

#define to_scsi_device(d)       \
        container_of(d, struct scsi_device, sdev_gendev)

#define shost_for_each_device(sdev, shost) dde_kit_printf("shost_for_each_device called\n");
#define __shost_for_each_device(sdev, shost) dde_kit_printf("__shost_for_each_device called\n");


/************************
 ** scsi/scsi_driver.h **
 ************************/

struct scsi_driver
{
	int (*done)(struct scsi_cmnd *);
};


/**********************************
 ** Platform specific defintions **
 *********************************/
#include <platform/lx_emul.h>


/**********
 ** misc **
 **********/

static inline void dump_stack(void) { }

static inline void * __must_check ERR_PTR(long error) {
	return (void *) error;
}


/**
 * Genode's evdev event handler
 */
enum input_event_type {
	EVENT_TYPE_PRESS, EVENT_TYPE_RELEASE, /* key press and release */
	EVENT_TYPE_MOTION,                    /* any type of (pointer) motion */
	EVENT_TYPE_WHEEL                      /* mouse scroll wheel */
};

struct input_handle;

/**
 * Input event callback
 *
 * \param   type        input event type
 * \param   keycode     key code if type is EVENT_TYPE_PRESS or
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
                                      unsigned keycode,
                                      int absolute_x, int absolute_y,
                                      int relative_x, int relative_y);

/**
 * Register input handle
 *
 * \param   handler  call-back function on input events
 *
 * \return  0 on success; !0 otherwise
 */
void genode_input_register(genode_input_event_cb handler);


void genode_evdev_event(struct input_handle *handle, unsigned int type,
                        unsigned int code, int value);

void start_input_service(void *ep);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LX_EMUL_H_ */
