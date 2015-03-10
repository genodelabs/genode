/*
 * \brief  Emulation of the Linux kernel API
 * \author Josef Soentgen
 * \date   2014-03-03
 *
 * The content of this file, in particular data structures, is partially
 * derived from Linux-internal headers.
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL_H_
#define _LX_EMUL_H_

#if defined(__cplusplus) && !defined(extern_c_begin)
#error Include extern_c_begin.h before lx_emul.h
#endif /* __cplusplus */

#include <stdarg.h>

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(3,14,5)

#define KBUILD_MODNAME "mod-noname"

#define DEBUG_LINUX_PRINTK 1


void lx_printf(char const *, ...) __attribute__((format(printf, 1, 2)));
void lx_vprintf(char const *, va_list);


/***************
 ** asm/bug.h **
 ***************/

#define WARN_ON(condition) ({ \
		int ret = !!(condition); \
		if (ret) lx_printf("[%s] WARN_ON(" #condition ") ", __func__); \
		ret; })

#define WARN(condition, fmt, arg...) ({ \
		int ret = !!(condition); \
		if (ret) lx_printf("[%s] *WARN* " fmt , __func__ , ##arg); \
		ret; })

#define BUG() do { \
	lx_printf("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	while (1) ; \
} while (0)

#define WARN_ON_ONCE WARN_ON
#define WARN_ONCE    WARN

#define BUG_ON(condition) do { if (condition) BUG(); } while(0)


/*********************
 ** linux/kconfig.h **
 *********************/

#define IS_ENABLED(x) x


/*********************
 ** asm/processor.h **
 *********************/

void cpu_relax(void);


/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100 };


/*******************
 ** asm/cmpxchg.h **
 *******************/

#define cmpxchg(ptr, o, n) ({ \
		typeof(*ptr) prev = *ptr; \
		*ptr = (*ptr == o) ? n : *ptr; \
		prev;\
		})

#define xchg(ptr, x) ({ \
	typeof(*ptr) old = *ptr; \
	*ptr = x; \
	old; \
})


/*******************************
 ** asm-generic/bitsperlong.h **
 *******************************/

#define BITS_PER_LONG (__SIZEOF_LONG__ * 8)


/******************
 ** asm/atomic.h **
 ******************/

#include <asm-generic/atomic64.h>

#define ATOMIC_INIT(i) { (i) }

typedef struct atomic { long counter; } atomic_t;
typedef atomic_t atomic_long_t;

static inline int  atomic_read(const atomic_t *p)          { return p->counter; }
static inline void atomic_set(atomic_t *p, int i)          { p->counter = i; }
static inline void atomic_sub(int i, atomic_t *p)          { p->counter -= i; }
static inline void atomic_add(int i, atomic_t *p)          { p->counter += i; }
static inline int  atomic_sub_return(int i, atomic_t *p)   { p->counter -= i; return p->counter; }
static inline int  atomic_add_return(int i, atomic_t *p)   { p->counter += i; return p->counter; }
static inline int  atomic_sub_and_test(int i, atomic_t *p) { return atomic_sub_return(i, p) == 0; }

static inline void atomic_dec(atomic_t *p)          { atomic_sub(1, p); }
static inline void atomic_inc(atomic_t *p)          { atomic_add(1, p); }
static inline int  atomic_dec_return(atomic_t *p)   { return atomic_sub_return(1, p); }
static inline int  atomic_inc_return(atomic_t *p)   { return atomic_add_return(1, p); }
static inline int  atomic_dec_and_test(atomic_t *p) { return atomic_sub_and_test(1, p); }
static inline int  atomic_inc_not_zero(atomic_t *p) { return p->counter ? atomic_inc_return(p) : 0; }

static inline void atomic_long_inc(atomic_long_t *p)                { atomic_add(1, p); }
static inline void atomic_long_sub(int i, atomic_long_t *p)         { atomic_sub(i, p); }
static inline long atomic_long_add_return(long i, atomic_long_t *p) { return atomic_add_return(i, p); }
static inline long atomic_long_read(atomic_long_t *p)               { return atomic_read(p); }

static inline int atomic_cmpxchg(atomic_t *v, int old, int n)
{
	return cmpxchg(&v->counter, old, n);
}

static inline int atomic_inc_not_zero_hint(atomic_t *v, int hint)
{
	int val, c = hint;

	/* sanity test, should be removed by compiler if hint is a constant */
	if (!hint)
		return atomic_inc_not_zero(v);

	do {
		val = atomic_cmpxchg(v, c, c + 1);
		if (val == c)
			return 1;
		c = val;
	} while (c);

	return 0;
}

static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
	int ret;
	unsigned long flags;
	(void)flags;

	ret = v->counter;
	if (ret != u)
		v->counter += a;

	return ret != u;
}

#define smp_mb__before_atomic_dec()


/*******************************
 ** asm-generic/atomic-long.h **
 *******************************/

#define ATOMIC_LONG_INIT(i) ATOMIC_INIT(i)


/*******************
 ** asm/barrier.h **
 *******************/

#define mb()  asm volatile ("": : :"memory")
#define smp_mb() mb()
#define smp_rmb() mb()
#define smp_wmb() mb()


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
	unsigned long flags;
	int       pfmemalloc;
	int       mapping;
	atomic_t _count;
	void     *addr;
	unsigned long private;
} __attribute((packed));


/************************
 ** linux/page-flags.h **
 ************************/

enum pageflags
{
	PG_slab = 0x1,
};

#define PageSlab(page) test_bit(PG_slab, &(page)->flags)


/**********************
 ** asm/cacheflush.h **
 **********************/

void flush_dcache_page(struct page *page);


/*******************
 ** linux/types.h **
 *******************/

#include <platform/types.h>

typedef uint32_t      uint;
typedef unsigned long ulong;

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

typedef __u16 __sum16;
typedef __u32 __wsum;

typedef u64 sector_t;
typedef int clockid_t;

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
typedef _Bool bool;
enum { true = 1, false = 0 };
#endif /* __cplusplus */

#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL ((void *)0)
#endif /* __cplusplus */
#endif /* NULL */

typedef unsigned       gfp_t;
typedef unsigned long  dma_addr_t;
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
typedef size_t         __kernel_size_t;
typedef long           __kernel_time_t;
typedef long           __kernel_suseconds_t;
typedef unsigned long  dma_addr_t;
typedef long           clock_t;

#ifndef __cplusplus
typedef u16            wchar_t;
#endif

#define __aligned_u64 __u64 __attribute__((aligned(8)))

/*
 * needed by include/net/cfg80211.h
 */
struct callback_head {
	struct callback_head *next;
	void (*func)(struct callback_head *head);
};
#define rcu_head callback_head

#if defined(__x86_64__)
typedef unsigned int mode_t;
#else
typedef unsigned short mode_t;
#endif

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]


/**********************
 ** linux/compiler.h **
 **********************/

#define likely
#define unlikely

#define __user
#define __iomem /* needed by drivers/net/wireless/iwlwifi/pcie/internal.h */
#define __rcu
#define __percpu
#define __must_check
#define __force
#define __read_mostly

#define __cond_lock(x,c) (c) /* needed by drivers/net/wireless/iwlwifi/dvm/main.c */

#define __releases(x) /* needed by usb/core/devio.c */
#define __acquires(x) /* needed by usb/core/devio.c */
#define __force
#define __maybe_unused
#define __bitwise

# define __acquire(x) (void)0
# define __release(x) (void)0

#define __must_check

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

#define __attribute_const__
#undef  __always_inline
#define __always_inline

#undef __unused

#define __printf(a, b) __attribute__((format(printf, a, b)))

#define noinline     __attribute__((noinline))


/**************************
 ** linux/compiler-gcc.h **
 **************************/

#define uninitialized_var(x) x = x
#ifdef __aligned
#undef __aligned
#endif
#define __aligned(x)  __attribute__((aligned(x)))
#define __noreturn    __attribute__((noreturn))

#define barrier() __asm__ __volatile__("": : :"memory")

#define OPTIMIZER_HIDE_VAR(var) __asm__ ("" : "=r" (var) : "0" (var))

#define __visible __attribute__((externally_visible))


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
#define LIST_POISON1 nullptr
#define LIST_POISON2 nullptr
#else
#define LIST_POISON1 ((void *)0x00100100)
#define LIST_POISON2 ((void *)0x00200200)
#endif  /* __cplusplus */


/**********************
 ** linux/mm_types.h **
 **********************/

struct vm_operations_struct;

struct vm_area_struct
{
	unsigned long vm_start;
	unsigned long vm_end;

	const struct vm_operations_struct *vm_ops;

	unsigned long vm_pgoff;

	struct file * vm_file;
};


/****************
 ** linux/mm.h **
 ****************/

int is_vmalloc_addr(const void *x);

extern unsigned long totalram_pages;
extern unsigned long num_physpages;

static inline struct page *compound_head(struct page *page) { return page; }
static inline void *page_address(struct page *page) { return page->addr; };

void get_page(struct page *page);
void put_page(struct page *page);

#define offset_in_page(p) ((unsigned long)(p) & ~PAGE_MASK)

struct page *virt_to_head_page(const void *x);
struct page *virt_to_page(const void *x);
struct page *vmalloc_to_page(const void *addr);

struct sysinfo;
void si_meminfo(struct sysinfo *);

#define page_private(page)      ((page)->private)
#define set_page_private(page, v)   ((page)->private = (v))

struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
};

int get_user_pages_fast(unsigned long start, int nr_pages, int write, struct page **pages);
int vm_insert_page(struct vm_area_struct *, unsigned long addr, struct page *);


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size);
void *vzalloc(unsigned long size);
void vfree(const void *addr);


/**********************
 ** linux/highmem.h  **
 **********************/

static inline void *kmap(struct page *page) { return page_address(page); }
static inline void *kmap_atomic(struct page *page) { return kmap(page); }
static inline void kunmap(struct page *page) { }
static inline void kunmap_atomic(void *addr) { }


/*****************
 ** linux/gfp.h **
 *****************/

enum {
	__GFP_DMA         = 0x00000001u,
	__GFP_HIGHMEM     = 0x00000002u,
	__GFP_DMA32       = 0x00000004u,
	__GFP_MOVABLE     = 0x00000008u,
	__GFP_WAIT        = 0x00000010u,
	__GFP_HIGH        = 0x00000020u,
	__GFP_IO          = 0x00000040u,
	__GFP_FS          = 0x00000080u,
	__GFP_COLD        = 0x00000100u,
	__GFP_NOWARN      = 0x00000200u,
	__GFP_REPEAT      = 0x00000400u,
	__GFP_NOFAIL      = 0x00000800u,
	__GFP_NORETRY     = 0x00001000u,
	__GFP_MEMALLOC    = 0x00002000u,
	__GFP_COMP        = 0x00004000u,
	__GFP_ZERO        = 0x00008000u,
	__GFP_NOMEMALLOC  = 0x00010000u,
	__GFP_HARDWALL    = 0x00020000u,
	__GFP_THISNODE    = 0x00040000u,
	__GFP_RECLAIMABLE = 0x00080000u,
	__GFP_KMEMCG      = 0x00100000u,
	__GFP_NOTRACK     = 0x00200000u,
	__GFP_NO_KSWAPD   = 0x00400000u,
	__GFP_OTHER_NODE  = 0x00800000u,
	__GFP_WRITE       = 0x01000000u,

	GFP_LX_DMA        = 0x80000000u,

	GFP_ATOMIC        = __GFP_HIGH,
	GFP_DMA           = __GFP_DMA,
	GFP_KERNEL        = __GFP_WAIT | __GFP_IO | __GFP_FS,
	GFP_USER          = __GFP_WAIT | __GFP_IO | __GFP_FS | __GFP_HARDWALL,
};

struct page *alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order);

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

unsigned long get_zeroed_page(gfp_t gfp_mask);
#define free_page(p) kfree((void *)p)

bool gfp_pfmemalloc_allowed(gfp_t);
unsigned long __get_free_page(gfp_t);
unsigned long __get_free_pages(gfp_t, unsigned int);
void free_pages(unsigned long, unsigned int);
void __free_pages(struct page *page, unsigned int order);


/******************
 ** linux/slab.h **
 ******************/

#define ARCH_KMALLOC_MINALIGN __alignof__(unsigned long long)

enum {
	SLAB_HWCACHE_ALIGN = 0x00002000ul,
	SLAB_CACHE_DMA     = 0x00004000ul,
	SLAB_PANIC         = 0x00040000ul,

	SLAB_LX_DMA        = 0x80000000ul,
};

void *kzalloc(size_t size, gfp_t flags);
void  kfree(const void *);
void  kzfree(const void *);
void *kmalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);

struct kmem_cache;
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));
void   kmem_cache_destroy(struct kmem_cache *);
void  *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);
void  kmem_cache_free(struct kmem_cache *, void *);
void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node);

static inline void *kmem_cache_alloc_node(struct kmem_cache *s, gfp_t flags, int node)
{
	return kmem_cache_alloc(s, flags);
}


/********************
 ** linux/string.h **
 ********************/
#undef memcpy

void  *memcpy(void *d, const void *s, size_t n);
void  *memset(void *s, int c, size_t n);
int    memcmp(const void *, const void *, size_t);
void  *memscan(void *addr, int c, size_t size);
char  *strcat(char *dest, const char *src);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *cs, const char *ct, size_t count);
char  *strcpy(char *to, const char *from);
char  *strncpy(char *, const char *, size_t);
char  *strchr(const char *, int);
char  *strrchr(const char *,int);
size_t strlcat(char *dest, const char *src, size_t n);
size_t strlcpy(char *dest, const char *src, size_t size);
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
char * strsep(char **,const char *);
char *strstr(const char *, const char *);
char  *kstrdup(const char *s, gfp_t gfp);
void  *kmemdup(const void *src, size_t len, gfp_t gfp);
void  *memmove(void *, const void *, size_t);
void * memchr(const void *, int, size_t);


/*************************
 ** linux/irq_cpustat.h **
 *************************/

int local_softirq_pending(void);


/**********************
 ** linux/irqflags.h **
 **********************/

#define local_irq_enable(a )     do { } while (0)
#define local_irq_disable()      do { } while (0)
#define local_irq_save(flags)    do { (void)flags; } while (0)
#define local_irq_restore(flags) do { (void)flags; } while (0)


/**********************
 ** linux/spinlock.h **
 **********************/

typedef struct spinlock { unsigned unused; } spinlock_t;
#define DEFINE_SPINLOCK(name) spinlock_t name

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
void spin_lock_bh(spinlock_t *lock);
void spin_unlock_bh(spinlock_t *lock);
int spin_trylock(spinlock_t *lock);


/****************************
 ** linux/spinlock_types.h **
 ****************************/

#define __SPIN_LOCK_UNLOCKED(x) 0


/*******************
 ** linux/mutex.h **
 *******************/

struct mutex
{
	int       state;
	void     *holder;
	void     *waiters;
	unsigned  id; /* for debugging */
};

#define DEFINE_MUTEX(mutexname) \
	struct mutex mutexname; \
	static void __attribute__((constructor)) mutex_init_ ## mutexname(void) \
	{ mutex_init(&mutexname); }

void mutex_init(struct mutex *m);
void mutex_destroy(struct mutex *m);
void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);
int mutex_trylock(struct mutex *m);
int mutex_is_locked(struct mutex *m);

/* special case in net/wireless/util.c:1357 */
#define mutex_lock_nested(lock, subclass) mutex_lock(lock)


/*******************
 ** linux/rwsem.h **
 *******************/

struct rw_semaphore { int dummy; };

#define DECLARE_RWSEM(name) \
	struct rw_semaphore name = { 0 }

#define init_rwsem(sem) do { (void)sem; } while (0)

void down_read(struct rw_semaphore *sem);
void up_read(struct rw_semaphore *sem);
void down_write(struct rw_semaphore *sem);
void up_write(struct rw_semaphore *sem);

#define __RWSEM_INITIALIZER(name) { 0 }


/*******************
 ** linux/timer.h **
 *******************/

struct tvec_base;
extern struct tvec_base boot_tvec_bases;  /* needed by 'dwc_common_linux.c' */

struct timer_list
{
	void (*function)(unsigned long);
	unsigned long data;
	void *timer;
	unsigned long expires;
	struct tvec_base *base;  /* needed by 'dwc_common_linux.c' */
};

void init_timer(struct timer_list *);
void init_timer_deferrable(struct timer_list *);
int mod_timer(struct timer_list *timer, unsigned long expires);
int del_timer(struct timer_list * timer);
void setup_timer(struct timer_list *timer, void (*function)(unsigned long),
                 unsigned long data);
int timer_pending(const struct timer_list * timer);
unsigned long round_jiffies(unsigned long j);
unsigned long round_jiffies_relative(unsigned long j);
unsigned long round_jiffies_up(unsigned long j);

void set_timer_slack(struct timer_list *time, int slack_hz);
static inline void add_timer(struct timer_list *timer) { mod_timer(timer, timer->expires); }

static inline
int del_timer_sync(struct timer_list * timer) { return del_timer(timer); }


/***********************
 ** linux/workqueue.h **
 ***********************/

enum {
	WQ_MEM_RECLAIM,
	WQ_CPU_INTENSIVE,
};

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	atomic_long_t data;
	work_func_t func;
	struct list_head entry;
};

struct delayed_work {
	struct timer_list timer;
	struct work_struct work;
};

bool cancel_work_sync(struct work_struct *work);
bool cancel_delayed_work_sync(struct delayed_work *work);
bool cancel_delayed_work(struct delayed_work *dwork);
int schedule_delayed_work(struct delayed_work *work, unsigned long delay);
int schedule_work(struct work_struct *work);

bool flush_work(struct work_struct *work);
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


/* dummy for queue_delayed_work call in storage/usb.c */
#define system_freezable_wq 0
struct workqueue_struct { unsigned unused; };

struct workqueue_struct *create_singlethread_workqueue(const char *name);
struct workqueue_struct *alloc_ordered_workqueue(const char *fmt, unsigned int flags, ...) __printf(1, 3);
struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags,
                                         int max_active, ...) __printf(1, 4);
void destroy_workqueue(struct workqueue_struct *wq);
void flush_workqueue(struct workqueue_struct *wq);
bool queue_delayed_work(struct workqueue_struct *, struct delayed_work *, unsigned long);
bool flush_delayed_work(struct delayed_work *dwork);
bool queue_work(struct workqueue_struct *wq, struct work_struct *work);

#define DECLARE_DELAYED_WORK(n, f) \
	struct delayed_work n

bool mod_delayed_work(struct workqueue_struct *, struct delayed_work *,
                      unsigned long);

extern struct workqueue_struct *system_wq;

enum {
	WORK_STRUCT_STATIC      = 0,

	WORK_STRUCT_COLOR_SHIFT = 4,
	WORK_STRUCT_COLOR_BITS  = 4,
	WORK_STRUCT_FLAG_BITS   = WORK_STRUCT_COLOR_SHIFT + WORK_STRUCT_COLOR_BITS,
	WORK_OFFQ_FLAG_BASE     = WORK_STRUCT_FLAG_BITS,

	WORK_OFFQ_FLAG_BITS     = 1,
	WORK_OFFQ_POOL_SHIFT    = WORK_OFFQ_FLAG_BASE + WORK_OFFQ_FLAG_BITS,
	WORK_OFFQ_LEFT          = BITS_PER_LONG - WORK_OFFQ_POOL_SHIFT,
	WORK_OFFQ_POOL_BITS     = WORK_OFFQ_LEFT <= 31 ? WORK_OFFQ_LEFT : 31,
	WORK_OFFQ_POOL_NONE     = (1LU << WORK_OFFQ_POOL_BITS) - 1,

	WORK_STRUCT_NO_POOL     = (unsigned long)WORK_OFFQ_POOL_NONE << WORK_OFFQ_POOL_SHIFT,
};

#define WORK_DATA_STATIC_INIT() \
	ATOMIC_LONG_INIT(WORK_STRUCT_NO_POOL | WORK_STRUCT_STATIC)

#define __WORK_INIT_LOCKDEP_MAP(n, k)

#define __WORK_INITIALIZER(n, f) {          \
	.data = WORK_DATA_STATIC_INIT(),        \
	.entry  = { &(n).entry, &(n).entry },   \
	.func = (f),                            \
	__WORK_INIT_LOCKDEP_MAP(#n, &(n))       \
}

#define DECLARE_WORK(n, f)                      \
	struct work_struct n = __WORK_INITIALIZER(n, f)


/********************
 ** linux/kernel.h **
 ********************/

/*
 * Log tags
 */
#define KERN_ALERT   "ALERT: "
#define KERN_CRIT    "CRTITCAL: "
#define KERN_DEBUG   "DEBUG: "
#define KERN_EMERG   "EMERG: "
#define KERN_ERR     "ERROR: "
#define KERN_INFO    "INFO: "
#define KERN_NOTICE  "NOTICE: "
#define KERN_WARNING "WARNING: "
#define KERN_WARN   "WARNING: "

/*
 * Debug macros
 */
#if DEBUG_LINUX_PRINTK
#define printk  _printk
#define vprintk lx_vprintf
#else
#define printk(...)
#define vprintk(...)
#endif

static inline __printf(1, 2) void panic(const char *fmt, ...) __noreturn;
static inline void panic(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	lx_vprintf(fmt, args);
	va_end(args);
	lx_printf("panic()");
	while (1) ;
}

/*
 * Bits and types
 */

/* needed by linux/list.h */
#define container_of(ptr, type, member) ({ \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})

/* normally provided by linux/stddef.h, needed by linux/list.h */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define max_t(type, x, y) ({                    \
        type __max1 = (x);                      \
        type __max2 = (y);                      \
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

#define lower_32_bits(n) ((u32)(n))
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))

#define roundup(x, y) (                           \
{                                                 \
        const typeof(y) __y = y;                        \
        (((x) + (__y - 1)) / __y) * __y;                \
})

#define clamp_val(val, min, max) ({             \
        typeof(val) __val = (val);              \
        typeof(val) __min = (min);              \
        typeof(val) __max = (max);              \
        __val = __val < __min ? __min: __val;   \
        __val > __max ? __max: __val; })


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define BUILD_BUG_ON(condition)

#define _RET_IP_  (unsigned long)__builtin_return_address(0)

void might_sleep();
#define might_sleep_if(cond) do { if (cond) might_sleep(); } while (0)

#define INT_MAX  ((int)(~0U>>1))
#define UINT_MAX (~0U)

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

#define DIV_ROUND_CLOSEST(x, divisor)(      \
{                                           \
        typeof(x) __x = x;                        \
        typeof(divisor) __d = divisor;            \
        (((typeof(x))-1) > 0 ||                   \
        ((typeof(divisor))-1) > 0 || (__x) > 0) ? \
        (((__x) + ((__d) / 2)) / (__d)) :         \
        (((__x) - ((__d) / 2)) / (__d));          \
})

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define PTR_ALIGN(p, a) ({              \
  unsigned long _p = (unsigned long)p;  \
  _p = (_p + a - 1) & ~(a - 1);         \
  p = (typeof(p))_p;                    \
        p;                                    \
})

static inline u32 reciprocal_scale(u32 val, u32 ep_ro)
{
	return (u32)(((u64) val * ep_ro) >> 32);
}

int kstrtoul(const char *s, unsigned int base, unsigned long *res);

int strict_strtoul(const char *s, unsigned int base, unsigned long *res);
long simple_strtoul(const char *cp, char **endp, unsigned int base);
long simple_strtol(const char *,char **,unsigned int);

int hex_to_bin(char ch);

#define INT_MIN       (-INT_MAX - 1)
#define USHRT_MAX     ((u16)(~0U))
#define LONG_MAX      ((long)(~0UL>>1))

/* needed by drivers/net/wireless/iwlwifi/iwl-drv.c */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) __attribute__((format(printf, 3, 0)));
int sprintf(char *buf, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int scnprintf(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

int sscanf(const char *, const char *, ...);

/* XXX */
#define ALIGN(x, a)                   __ALIGN_KERNEL((x), (a))
#define __ALIGN_KERNEL(x, a)          __ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)  (((x) + (mask)) & ~(mask))

#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)


/********************
 ** linux/printk.h **
 ********************/

static inline int _printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static inline int _printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	lx_vprintf(fmt, args);
	va_end(args);
	return 0;
}

static inline int no_printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static inline int no_printk(const char *fmt, ...) { return 0; }

#define printk_ratelimit(x) (0)

#define printk_ratelimited(fmt, ...) printk(fmt, ##__VA_ARGS__)

#define pr_emerg(fmt, ...)    printk(KERN_EMERG  fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)    printk(KERN_ALERT  fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)     printk(KERN_CRIT   fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)      printk(KERN_ERR    fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...)  printk(KERN_WARN   fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)     printk(KERN_WARN   fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...)   printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)     printk(KERN_INFO   fmt, ##__VA_ARGS__)
#define pr_cont(fmt, ...)     printk(KERN_CONT   fmt, ##__VA_ARGS__)
/* pr_devel() should produce zero code unless DEBUG is defined */
#ifdef DEBUG
#define pr_devel(fmt, ...)    printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)    printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#else
#define pr_devel(fmt, ...) no_printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) no_printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#endif

enum {
	DUMP_PREFIX_OFFSET
};

struct va_format
{
	const char *fmt;
	va_list *va;
};

int snprintf(char *str, size_t size, const char *format, ...) __attribute__((format(printf, 3, 4)));

static inline void print_hex_dump(const char *level, const char *prefix_str,
                                  int prefix_type, int rowsize, int groupsize,
                                  const void *buf, size_t len, bool ascii)
{
	printk("hex_dump: ");
	size_t i;
	for (i = 0; i < len; i++) {
		printk("%x ", ((char*)buf)[i]);
	}
	printk("\n");
}

void hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize, char *linebuf, size_t linebuflen, bool ascii);
void dump_stack(void);


/*************************************
 ** linux/byteorder/little_endian.h **
 *************************************/

#include <uapi/linux/byteorder/little_endian.h>


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
#define cpu_to_le16s __cpu_to_le16s
#define cpu_to_be16  __cpu_to_be16
#define cpu_to_le32  __cpu_to_le32
#define cpu_to_be32  __cpu_to_be32
#define cpu_to_le32s __cpu_to_le32s
#define cpu_to_le64  __cpu_to_le64
#define cpu_to_be64  __cpu_to_be64
#define le16_to_cpup __le16_to_cpup
#define be16_to_cpup __be16_to_cpup
#define le32_to_cpup __le32_to_cpup
#define le32_to_cpus __le32_to_cpus
#define be32_to_cpup __be32_to_cpup
#define be64_to_cpup __be64_to_cpup
#define le64_to_cpu  __le64_to_cpu

#define htonl(x) __cpu_to_be32(x)
#define htons(x) __cpu_to_be16(x)
#define ntohl(x) __be32_to_cpu(x)
#define ntohs(x) __be16_to_cpu(x)

struct __una_u16 { u16 x; } __attribute__((packed));
struct __una_u32 { u32 x; } __attribute__((packed));
struct __una_u64 { u64 x; } __attribute__((packed));

u32 __get_unaligned_cpu32(const void *p);

void put_unaligned_le16(u16 val, void *p);
void put_unaligned_be16(u16 val, void *p);

static inline void put_unaligned_le32(u32 val, void *p)
{
	*((__le32 *)p) = cpu_to_le32(val);
}

static inline u16 get_unaligned_le16(const void *p)
{
	const struct __una_u16 *ptr = (const struct __una_u16 *)p;
	return ptr->x;
}

static inline u32 get_unaligned_le32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
}

void put_unaligned_le64(u64 val, void *p);

#define put_unaligned(val, ptr) ({              \
	void *__gu_p = (ptr);                       \
	switch (sizeof(*(ptr))) {                   \
	case 1:                                     \
		*(u8 *)__gu_p = (u8)(val);              \
		break;                                  \
	case 2:                                     \
		put_unaligned_le16((u16)(val), __gu_p); \
		break;                                  \
	case 4:                                     \
		put_unaligned_le32((u32)(val), __gu_p); \
		break;                                  \
	case 8:                                     \
		put_unaligned_le64((u64)(val), __gu_p); \
		break;                                  \
	}                                           \
	(void)0; })

/* needed by net/wireless/util.c */
#define htons(x) __cpu_to_be16(x)

static inline void le32_add_cpu(__le32 *var, u32 val)
{
	*var = cpu_to_le32(le32_to_cpu(*var) + val);
}


/**********************
 ** linux/if_ether.h **
 **********************/

enum {
	ETH_ALEN     = 6,      /* octets in one ethernet addr */
	ETH_HLEN     = 14,     /* total octets in header */
	ETH_DATA_LEN = 1500,   /* Max. octets in payload */
	ETH_P_8021Q  = 0x8100, /* 802.1Q VLAN Extended Header  */

	ETH_FRAME_LEN = 1514,
};

#define ETH_P_TDLS     0x890D          /* TDLS */


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

#define BITS_PER_BYTE     8
#define BIT(nr)           (1UL << (nr))
#define BITS_TO_LONGS(nr) DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

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

#define smp_mb__before_clear_bit()
#define smp_mb__after_clear_bit() smp_mb()

/**
 * Find first zero bit (limit to machine word size)
 */
long find_next_zero_bit_le(const void *addr,
                           unsigned long size, unsigned long offset);


#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/ffs.h>
#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/fls64.h>

static inline unsigned fls_long(unsigned long l)
{
	if (sizeof(l) == 4)
		return fls(l);
	return fls64(l);
}

static inline unsigned long __ffs64(u64 word)
{
#if BITS_PER_LONG == 32
	if (((u32)word) == 0UL)
		return __ffs((u32)(word >> 32)) + 32;
#elif BITS_PER_LONG != 64
#error BITS_PER_LONG not 32 or 64
#endif
	return __ffs((unsigned long)word);
}

#include <linux/log2.h>

#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size));        \
	     (bit) < (size);                    \
	     (bit) = find_next_bit((addr), (size), (bit) + 1))

static inline int get_bitmask_order(unsigned int count) {
	return __builtin_clz(count) ^ 0x1f; }

static inline __s32 sign_extend32(__u32 value, int index)
{
	__u8 shift = 31 - index;
	return (__s32)(value << shift) >> shift;
}

static inline __u32 rol32(__u32 word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}

static inline __u32 ror32(__u32 word, unsigned int shift)
{
	return (word >> shift) | (word << (32 - shift));
}

static inline __u16 ror16(__u16 word, unsigned int shift)
{
	return (word >> shift) | (word << (16 - shift));
}


/****************************************
 ** asm-generic/bitops/const_hweight.h **
 ****************************************/

#define __const_hweight8(w)     \
	( (!!((w) & (1ULL << 0))) + \
	  (!!((w) & (1ULL << 1))) + \
	  (!!((w) & (1ULL << 2))) + \
	  (!!((w) & (1ULL << 3))) + \
	  (!!((w) & (1ULL << 4))) + \
	  (!!((w) & (1ULL << 5))) + \
	  (!!((w) & (1ULL << 6))) + \
	  (!!((w) & (1ULL << 7))) )

#define hweight8(w)  (__const_hweight8(w))

unsigned int hweight16(unsigned int w);
unsigned int hweight32(unsigned int w);
unsigned int hweight64(__u64 w);


/*********************
 ** linux/kobject.h **
 *********************/

enum kobject_action
{
	KOBJ_ADD,
	KOBJ_REMOVE,
	KOBJ_CHANGE,
};

struct kobject { struct kobject *parent; };

void kobject_put(struct kobject *);
int kobject_uevent(struct kobject *, enum kobject_action);
int kobject_uevent_env(struct kobject *kobj, enum kobject_action action, char *envp[]);

struct kobj_uevent_env
{
        char buf[32];
        int buflen;
};

struct kobj_uevent_env;

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...);
char *kobject_name(const struct kobject *kobj);
char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask);


typedef struct poll_table_struct { int dummy; } poll_table;


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

#define __ATTRIBUTE_GROUPS(_name)               \
	static const struct attribute_group *_name##_groups[] = {   \
		&_name##_group,                     \
		NULL,                           \
	}

#define ATTRIBUTE_GROUPS(_name)                 \
	static const struct attribute_group _name##_group = {       \
		.attrs = _name##_attrs,                 \
	};                              \
__ATTRIBUTE_GROUPS(_name)

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

#define __ATTR_NULL { .attr = { .name = NULL } }
#define __ATTR_RO(name) __ATTR_NULL
#define __ATTR_RW(name) __ATTR_NULL


int sysfs_create_group(struct kobject *kobj,
                       const struct attribute_group *grp);
void sysfs_remove_group(struct kobject *kobj,
                        const struct attribute_group *grp);
int sysfs_create_link(struct kobject *kobj, struct kobject *target,
                      const char *name);
void sysfs_remove_link(struct kobject *kobj, const char *name);


/****************
 ** linux/pm.h **
 ****************/

struct device;

typedef struct pm_message { int event; } pm_message_t;

struct dev_pm_info { pm_message_t power_state; };

struct dev_pm_ops {
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
	int (*freeze)(struct device *dev);
	int (*thaw)(struct device *dev);
	int (*poweroff)(struct device *dev);
	int (*restore)(struct device *dev);
};

#define PMSG_IS_AUTO(msg) 0

enum { PM_EVENT_AUTO_SUSPEND = 0x402 };

#define PM_EVENT_SUSPEND   0x0002
#define PM_EVENT_HIBERNATE 0x0004
#define PM_EVENT_SLEEP     (PM_EVENT_SUSPEND | PM_EVENT_HIBERNATE)

#ifdef CONFIG_PM_SLEEP
#define SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn) \
	.suspend = suspend_fn, \
	.resume = resume_fn, \
	.freeze = suspend_fn, \
	.thaw = resume_fn, \
	.poweroff = suspend_fn, \
	.restore = resume_fn,
#else
#define SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn)
#endif


/* needed by drivers/net/wireless/iwlwifi/pcie/drv.c */
#define SIMPLE_DEV_PM_OPS(name, suspend_fn, resume_fn) \
	const struct dev_pm_ops name = { \
		SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn) \
	}


/************************
 ** linux/pm_runtime.h **
 ************************/

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

struct pm_qos_request { unsigned unused; };

enum { PM_QOS_FLAG_NO_POWER_OFF = 1 };

enum { PM_QOS_NETWORK_LATENCY = 2 };

int pm_qos_request(int pm_qos_class);
struct notifier_block;
int pm_qos_add_notifier(int pm_qos_class, struct notifier_block *notifier);
int pm_qos_remove_notifier(int pm_qos_class, struct notifier_block *notifier);

int dev_pm_qos_expose_flags(struct device *dev, s32 value);


/**********************
 ** linux/notifier.h **
 **********************/

enum {
	NOTIFY_DONE      = 0x0000,
	NOTIFY_OK        = 0x0001,
	NOTIFY_STOP_MASK = 0x8000,
	NOTIFY_BAD       = (NOTIFY_STOP_MASK | 0x0002),
	NOTIFY_STOP      = (NOTIFY_OK|NOTIFY_STOP_MASK),

	NETLINK_URELEASE = 0x1,
};

struct notifier_block;

typedef int (*notifier_fn_t)(struct notifier_block *nb, unsigned long action,
                             void *data);


struct notifier_block
{
	notifier_fn_t          notifier_call;
	struct notifier_block *next;
	int                    priority;
};

struct raw_notifier_head
{
	struct notifier_block *head;
};

#define RAW_NOTIFIER_HEAD(name) \
	struct raw_notifier_head name;

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
int atomic_notifier_call_chain(struct atomic_notifier_head *nh, unsigned long val, void *v);
int raw_notifier_chain_register(struct raw_notifier_head *nh, struct notifier_block *n);
int raw_notifier_chain_unregister(struct raw_notifier_head *nh, struct notifier_block *nb);
int blocking_notifier_call_chain(struct blocking_notifier_head *nh, unsigned long val, void *v);

static inline int notifier_to_errno(int ret)
{
	return ret > 0 ? ret : 0;
}

static inline int notifier_from_errno(int err)
{
	return err ? (NOTIFY_STOP_MASK | (NOTIFY_OK - err)) : NOTIFY_OK;
}

#define ATOMIC_NOTIFIER_INIT(name) { \
	.head = NULL }

#define ATOMIC_NOTIFIER_HEAD(name) \
	struct atomic_notifier_head name =   ATOMIC_NOTIFIER_INIT(name)


/********************
 ** linux/device.h **
 ********************/

#define dev_info(dev, format, arg...) lx_printf("dev_info: "  format , ## arg)
#define dev_warn(dev, format, arg...) lx_printf("dev_warn: "  format , ## arg)
#define dev_WARN(dev, format, arg...) lx_printf("dev_WARN: "  format , ## arg)
#define dev_err( dev, format, arg...) lx_printf("dev_error: " format , ## arg)
#define dev_notice(dev, format, arg...) lx_printf("dev_notice: " format , ## arg)
#define dev_crit(dev, format, arg...) lx_printf("dev_crit: "  format , ## arg)

#ifndef DEBUG_DEV_DBG
#define DEBUG_DEV_DBG 1
#endif
#if DEBUG_DEV_DBG
#define dev_dbg(dev, format, arg...) lx_printf("dev_dbg: " format , ## arg)
#else
#define dev_dbg( dev, format, arg...)
#endif

#define dev_printk(level, dev, format, arg...) \
	lx_printf("dev_printk: " format , ## arg)

#define dev_warn_ratelimited(dev, format, arg...) \
	lx_printf("dev_warn_ratelimited: " format , ## arg)

enum {
	BUS_NOTIFY_ADD_DEVICE = 0x00000001,
	BUS_NOTIFY_DEL_DEVICE = 0x00000002,
};

struct device;
struct device_driver;

struct bus_type
{
	const char *name;
	struct device_attribute *dev_attrs;
	int (*match)(struct device *dev, struct device_driver *drv);
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
};

struct device_driver
{
	char const      *name;
	struct bus_type *bus;
	struct module   *owner;
	const char      *mod_name;
	const struct of_device_id  *of_match_table;
	const struct acpi_device_id *acpi_match_table;
	int            (*probe)  (struct device *dev);
	int            (*remove) (struct device *dev);

	const struct dev_pm_ops *pm;
};

struct kobj_uevent_env;

struct device_type
{
	const char *name;
	const struct attribute_group **groups;
	void      (*release)(struct device *dev);
	int       (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	char     *(*devnode)(struct device *dev, mode_t *mode);
	const struct dev_pm_ops *pm;
};

struct class
{
	const char *name;
	struct module *owner;

	const struct attribute_group    **dev_groups;

	int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);
	char *(*devnode)(struct device *dev, mode_t *mode);
	void (*dev_release)(struct device *dev);

	int (*suspend)(struct device *dev, pm_message_t state);
	int (*resume)(struct device *dev);

	const struct kobj_ns_type_operations *ns_type;
	const void *(*_namespace)(struct device *dev);

	const struct dev_pm_ops *pm;
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

	/**
	 * XXX this field is not in the original struct and is only
	 *     used by pci_{g,s}et_drvdata
	 */
	void *__private__;
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

#define DEVICE_ATTR(_name, _mode, _show, _store) \
	struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)

#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)

#define DEVICE_ATTR_RW(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RW(_name)

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
int  device_create_bin_file(struct device *dev, const struct bin_attribute *attr);
void device_remove_bin_file(struct device *dev, const struct bin_attribute *attr);
int  device_create_file(struct device *device, const struct device_attribute *entry);
void device_remove_file(struct device *dev, const struct device_attribute *attr);
int  device_reprobe(struct device *dev);
int device_rename(struct device *dev, const char *new_name);

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

struct lock_class_key;
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

static inline const char *dev_driver_string(const struct device *dev)
{
	struct device_driver *drv = dev->driver;

	return drv ? drv->name : (dev->bus ? dev->bus->name : (dev->class ? dev->class->name : ""));
}

int dev_printk_emit(int, const struct device *, const char *, ...);


/************************
 ** linux/miscdevice.h **
 ************************/

#define MISC_DYNAMIC_MINOR 255

struct miscdevice  {
	int minor;
	const char *name;
	const struct file_operations *fops;
	struct list_head list;
	struct device *parent;
	struct device *this_device;
	const char *nodename;
	umode_t mode;
};

int misc_register(struct miscdevice * misc);
int misc_deregister(struct miscdevice *misc);


/*********************
 ** linux/uapi/if.h **
 *********************/

enum {
	IFF_UP               = 0x1,
	IFF_BROADCAST        = 0x2,
	IFF_LOOPBACK         = 0x8,
	IFF_NOARP            = 0x80,   /* no ARP protocol */
	IFF_PROMISC          = 0x100,  /* receive all packets */
	IFF_ALLMULTI         = 0x200,  /* receive all multicast packets */
	IFF_MULTICAST        = 0x1000, /* supports multicast */
	IFF_DONT_BRIDGE      = 0x800,
	IFF_BRIDGE_PORT      = 0x4000,
	IFF_TX_SKB_SHARING   = 0x10000,
	IFF_LIVE_ADDR_CHANGE = 0x100000,

	IFNAMSIZ             = 16,
	IFALIASZ             = 256,
};

enum {
	IF_OPER_UNKNOWN,
	IF_OPER_NOTPRESENT,
	IF_OPER_DOWN,
	IF_OPER_LOWERLAYERDOWN,
	IF_OPER_TESTING,
	IF_OPER_DORMANT,
	IF_OPER_UP,
};

struct ifmap {
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned short base_addr; 
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
	/* 3 bytes spare */
};



/*************************
 ** linux/uapi/if_arp.h **
 *************************/

enum {
	ARPHRD_ETHER = 1,
	ARPHRD_IEEE80211_RADIOTAP = 803,
	ARPHRD_NETLINK = 824,
};


/***************************
 ** linux/uapi/if_ether.h **
 ***************************/

enum {
	ETH_P_IP   = 0x0800,
	ETH_P_IPV6 = 0x86DD,
};


/****************************
 ** uapi/linux/if_packet.h **
 ****************************/

enum {
	PACKET_HOST      = 0,
	PACKET_BROADCAST = 1,
	PACKET_MULTICAST = 2,
	PACKET_OTHERHOST = 3,
	PACKET_USER      = 6,
	PACKET_KERNEL    = 7,
};


/*****************************
 ** uapi/linux/virtio_net.h **
 *****************************/

struct virtio_net_hdr
{
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1   // Use csum_start, csum_offset
#define VIRTIO_NET_HDR_F_DATA_VALID 2   // Csum is valid
	__u8 flags;
#define VIRTIO_NET_HDR_GSO_NONE     0   // Not a GSO frame
#define VIRTIO_NET_HDR_GSO_TCPV4    1   // GSO frame, IPv4 TCP (TSO)
#define VIRTIO_NET_HDR_GSO_UDP      3   // GSO frame, IPv4 UDP (UFO)
#define VIRTIO_NET_HDR_GSO_TCPV6    4   // GSO frame, IPv6 TCP
#define VIRTIO_NET_HDR_GSO_ECN      0x80    // TCP has ECN set
	__u8 gso_type;
	__u16 hdr_len;      /* Ethernet + IP + tcp/udp hdrs */
	__u16 gso_size;     /* Bytes to append to hdr_len per frame */
	__u16 csum_start;   /* Position to start checksumming from */
	__u16 csum_offset;  /* Offset after that to place checksum */
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
};

#define ETH_GSTRING_LEN 32
enum ethtool_stringset {
	ETH_SS_TEST = 0,
	ETH_SS_STATS,
	ETH_SS_PRIV_FLAGS,
	ETH_SS_NTUPLE_FILTERS,
	ETH_SS_FEATURES,
};

struct ethtool_cmd
{
	u32 cmd;
	u8  duplex;
};

struct ethtool_regs
{
	u32 version;
	u32 len;
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
	/* For PCI devices, use pci_name(pci_de
	 * v). */
	u32     eedump_len;
};

struct ethtool_wolinfo {
	u32 supported;
	u32     wolopts;
};

struct ethtool_ts_info;

struct net_device;
struct ethtool_ringparam;
struct ethtool_stats;

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
	void    (*get_ringparam)(struct net_device *, struct ethtool_ringparam *);
	int     (*set_ringparam)(struct net_device *, struct ethtool_ringparam *);
	void    (*get_strings)(struct net_device *, u32 stringset, u8 *);
	void    (*get_ethtool_stats)(struct net_device *, struct ethtool_stats *, u64 *);
	int     (*get_sset_count)(struct net_device *, int);
	u32     (*get_msglevel)(struct net_device *);
	void    (*set_msglevel)(struct net_device *, u32);
	void    (*get_wol)(struct net_device *, struct ethtool_wolinfo *);
	int     (*set_wol)(struct net_device *, struct ethtool_wolinfo *);
	int     (*get_ts_info)(struct net_device *, struct ethtool_ts_info *);

};

__u32 ethtool_cmd_speed(const struct ethtool_cmd *);
int __ethtool_get_settings(struct net_device *dev, struct ethtool_cmd *cmd);
u32 ethtool_op_get_link(struct net_device *);
int ethtool_op_get_ts_info(struct net_device *, struct ethtool_ts_info *);


/**************************
 ** uapi/linux/ethtool.h **
 **************************/

enum {
	SPEED_UNKNOWN = -1,
};

struct ethtool_stats {
	__u32   cmd;            /* ETHTOOL_GSTATS */
	__u32   n_stats;        /* number of u64's being returned */
	__u64   data[0];
};


struct ethtool_ringparam {
	u32 rx_max_pending;
	u32 tx_max_pending;
	u32 rx_pending;
	u32 rx_mini_pending;
	u32 rx_jumbo_pending;
	u32 tx_pending;
};

/*********************
 ** linux/average.h **
 *********************/

struct ewma {
	unsigned long internal;
	unsigned long factor;
	unsigned long weight;
};

extern void ewma_init(struct ewma *avg, unsigned long factor,
                      unsigned long weight);

extern struct ewma *ewma_add(struct ewma *avg, unsigned long val);

static inline unsigned long ewma_read(const struct ewma *avg)
{
	return avg->internal >> avg->factor;
}


/********************************
 ** include/uapi/linux/types.h **
 ********************************/

#define __bitwise__


/*******************************
 ** linux/errno.h and friends **
 *******************************/

/**
 * Error codes
 *
 * Note that the codes do not correspond to those of the Linux kernel.
 */
enum {
	/*
	 * The following numbers correspond to FreeBSD
	 */
	EPERM           = 1,
	ENOENT          = 2,
	ESRCH           = 3,
	EINTR           = 4,
	EIO             = 5,
	ENXIO           = 6,
	E2BIG           = 7,
	EDEADLK         = 11,
	ENOMEM          = 12,
	EACCES          = 13,
	EFAULT          = 14,
	EBUSY           = 16,
	EEXIST          = 17,
	EXDEV           = 18,
	ENODEV          = 19,
	EINVAL          = 22,
	ENFILE          = 23,
	EFBIG           = 27,
	ENOSPC          = 28,
	ESPIPE          = 29,
	EPIPE           = 32,
	EDOM            = 33,
	ERANGE          = 34,
	EAGAIN          = 35,
	EINPROGRESS     = 36,
	EALREADY        = 37,
	ENOTSOCK        = 38,
	EDESTADDRREQ    = 39,
	EMSGSIZE        = 40,
	ENOPROTOOPT     = 42,
	EPROTONOSUPPORT = 43,
	ESOCKTNOSUPPORT = 44,
	EOPNOTSUPP      = 45,
	EPFNOSUPPORT    = 46,
	EAFNOSUPPORT    = 47,
	EADDRINUSE      = 48,
	EADDRNOTAVAIL   = 49,
	ENETDOWN        = 50,
	ENETUNREACH     = 51,
	ECONNABORTED    = 53,
	ECONNRESET      = 54,
	ENOBUFS         = 55,
	EISCONN         = 56,
	ENOTCONN        = 57,
	ETIMEDOUT       = 60,
	ECONNREFUSED    = 61,
	ENAMETOOLONG    = 63,
	EHOSTDOWN       = 64,
	EHOSTUNREACH    = 65,
	ENOSYS          = 78,
	ENOMSG          = 83,
	EOVERFLOW       = 84,
	ECANCELED       = 85,
	EILSEQ          = 86,
	EBADMSG         = 89,
	ENOLINK         = 91,
	EPROTO          = 92,

	/*
	 * The following numbers correspond to nothing
	 */
	EREMOTEIO       = 200,
	ERESTARTSYS     = 201,
	ENODATA         = 202,
	ETOOSMALL       = 203,
	ENOIOCTLCMD     = 204,
	ENONET          = 205,
	ENOTSUPP        = 206,
	ENOTUNIQ        = 207,
	ERFKILL         = 208,

	MAX_ERRNO       = 4095,
};


/**************************
 ** linux/preempt_mask.h **
 **************************/

int in_interrupt(void);
int in_softirq(void);
int in_irq(void);

int softirq_count(void);


/*********************
 ** linux/preempt.h **
 *********************/

#define preempt_disable() barrier()
#define preempt_enable()  barrier()


/**********************
 ** linux/inerrupt.h **
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


/*****************
 ** linux/idr.h **
 *****************/

#define IDR_BITS 8
#define IDR_SIZE (1 << IDR_BITS)
#define IDR_MASK ((1 << IDR_BITS)-1)

struct idr_layer {
	int                     prefix; /* the ID prefix of this idr_layer */
	DECLARE_BITMAP(bitmap, IDR_SIZE); /* A zero bit means "space here" */
	struct idr_layer __rcu  *ary[1<<IDR_BITS];
	int                     count;  /* When zero, we can release it */
	int                     layer;  /* distance from leaf */
	struct rcu_head         rcu_head;
};

struct idr {
	struct idr_layer __rcu  *hint;  /* the last layer allocated from */
	struct idr_layer __rcu  *top;
	struct idr_layer        *id_free;
	int                     layers; /* only valid w/o concurrent changes */
	int                     id_free_cnt;
	spinlock_t              lock;
};

int idr_alloc(struct idr *idp, void *ptr, int start, int end, gfp_t gfp_mask);
int idr_for_each(struct idr *idp, int (*fn)(int id, void *p, void *data), void *data);
void idr_remove(struct idr *idp, int id);
void idr_destroy(struct idr *idp);
void idr_init(struct idr *idp);
void *idr_find(struct idr *idr, int id);

#define DEFINE_IDA(name) struct ida name;
struct ida { unsigned unused; };


/**********************
 ** linux/rcupdate.h **
 **********************/

static inline void rcu_read_lock(void) { }
static inline void rcu_read_unlock(void) { }
static inline void synchronize_rcu(void) { }

#define rcu_dereference(p) p
#define rcu_dereference_bh(p) p
#define rcu_dereference_check(p, c) p
#define rcu_dereference_protected(p, c) p
#define rcu_dereference_raw(p) p
#define rcu_dereference_index_check(p, c) p

#define rcu_assign_pointer(p, v) p = v
#define rcu_access_pointer(p) p

#define __kfree_rcu(x, y)

#define kfree_rcu(ptr, rcu_head)                                        \
      __kfree_rcu(&((ptr)->rcu_head), offsetof(typeof(*(ptr)), rcu_head))

static inline int rcu_read_lock_held(void) { return 1; }
static inline int rcu_read_lock_bh_held(void) { return 1; }

#define RCU_INIT_POINTER(p, v) do { p = (typeof(*v) *)v; } while (0)

void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *head));


/*********************
 ** linux/rcutree.h **
 *********************/

void rcu_barrier(void);


/*******************************
 ** net/mac80211/driver-ops.h **
 *******************************/

struct ieee80211_local;
struct ieee80211_low_level_stats;


/*********************
 ** linux/rculist.h **
 *********************/

#include <linux/list.h>

#define list_for_each_entry_rcu(pos, head, member) \
	list_for_each_entry(pos, head, member)

static inline void list_add(struct list_head *, struct list_head *head);
static inline void list_add_tail(struct list_head *, struct list_head *head);
static inline void list_del(struct list_head *entry);
static inline void list_move_tail(struct list_head *list, struct list_head *head);

static inline void list_add_rcu(struct list_head *n, struct list_head *head) {
	list_add(n, head); }

static inline void list_add_tail_rcu(struct list_head *n,
                                     struct list_head *head) {
	list_add_tail(n, head); }

static inline void list_del_rcu(struct list_head *entry) {
	list_del(entry); }

#define list_entry_rcu(ptr, type, member) \
	({typeof (*ptr) __rcu *__ptr = (typeof (*ptr) __rcu __force *)ptr; \
	container_of((typeof(ptr))rcu_dereference_raw(__ptr), type, member); \
	})

#define hlist_for_each_entry_rcu(pos, head, member) \
hlist_for_each_entry(pos, head, member)
#define list_next_rcu(list) (*((struct list_head  **)(&(list)->next)))

#define list_for_each_entry_rcu(pos, head, member)  \
list_for_each_entry(pos, head, member)

#define list_first_or_null_rcu(ptr, type, member) ({ \
struct list_head *__ptr = (ptr); \
struct list_head __rcu *__next = list_next_rcu(__ptr); \
__ptr != __next ? container_of(__next, type, member) : NULL; \
})

#define list_add_rcu       list_add
#define list_add_tail_rcu  list_add_tail
#define hlist_add_head_rcu hlist_add_head

#define list_del_rcu  list_del
#define hlist_del_rcu hlist_del

#define hlist_del_init_rcu hlist_del_init
#define free_percpu(pdata) kfree(pdata)

void hlist_add_after_rcu(struct hlist_node *, struct hlist_node *);
void hlist_add_before_rcu(struct hlist_node *,struct hlist_node *);

void list_replace_rcu(struct list_head *, struct list_head *);

#include <linux/hashtable.h>


/*********************
 ** linux/jiffies.h **
 *********************/

#define MAX_JIFFY_OFFSET ((LONG_MAX >> 1)-1)

extern unsigned long jiffies;

unsigned int jiffies_to_msecs(const unsigned long);
unsigned long usecs_to_jiffies(const unsigned int u);
unsigned long msecs_to_jiffies(const unsigned int);
clock_t jiffies_to_clock_t(unsigned long x);
static inline clock_t jiffies_delta_to_clock_t(long delta)
{
	return jiffies_to_clock_t(max(0L, delta));
}

#define time_after(a,b)     ((long)((b) - (a)) < 0)
#define time_after_eq(a,b)  ((long)((a) - (b)) >= 0)
#define time_before(a,b)    time_after(b,a)
#define time_before_eq(a,b) time_after_eq(b,a)

#define time_is_after_jiffies(a) time_before(jiffies, a)


/******************
 ** linux/time.h **
 ******************/

struct timeval
{
	__kernel_time_t         tv_sec;
	__kernel_suseconds_t    tv_usec;
};

struct timespec {
	__kernel_time_t tv_sec;
	long            tv_nsec;
};

enum {
	MSEC_PER_SEC  = 1000L,
	USEC_PER_SEC  = MSEC_PER_SEC * 1000L,
	NSEC_PER_USEC = 1000L,
	NSEC_PER_MSEC = NSEC_PER_USEC * 1000L,
	NSEC_PER_SEC  = MSEC_PER_SEC * NSEC_PER_MSEC,
	USEC,
	USEC_PER_MSEC = 1000L,
};

unsigned long get_seconds(void);
void          getnstimeofday(struct timespec *);
#define do_posix_clock_monotonic_gettime(ts) ktime_get_ts(ts)


/*******************
 ** linux/ktime.h **
 *******************/

union ktime {
	s64 tv64;
};

typedef union ktime ktime_t;

#define ktime_to_ns(kt) ((kt).tv64)

struct timeval ktime_to_timeval(const ktime_t);
struct timespec ktime_to_timespec(const ktime_t kt);
bool ktime_to_timespec_cond(const ktime_t kt, struct timespec *ts);

ktime_t ktime_sub(const ktime_t, const ktime_t);
ktime_t ktime_get(void);
int     ktime_equal(const ktime_t, const ktime_t);
s64     ktime_us_delta(const ktime_t, const ktime_t);

static inline ktime_t ktime_set(const long secs, const unsigned long nsecs)
{
	return (ktime_t) { .tv64 = (s64)secs * NSEC_PER_SEC + (s64)nsecs };
}

static inline s64 ktime_to_ms(const ktime_t kt)
{
	return kt.tv64 / NSEC_PER_MSEC;
}

static inline ktime_t ktime_get_real(void)
{
	return (ktime_t) { .tv64 = (s64)(jiffies * (1000 / HZ) * NSEC_PER_MSEC) };
}

static inline void ktime_get_ts(struct timespec *ts)
{
	ts->tv_sec  = jiffies * (1000/HZ);
	ts->tv_nsec = 0;
}


/***********************
 ** linux/kmemcheck.h **
 ***********************/

#define kmemcheck_bitfield_begin(name)
#define kmemcheck_bitfield_end(name)
#define kmemcheck_annotate_bitfield(ptr, name)
#define kmemcheck_annotate_variable(var)


/******************
 ** linux/numa.h **
 ******************/

enum { NUMA_NO_NODE = -1 };


/*************************
 ** asm-generic/cache.h **
 *************************/

#define L1_CACHE_BYTES  32
#define SMP_CACHE_BYTES L1_CACHE_BYTES

#define ____cacheline_aligned __attribute__((__aligned__(SMP_CACHE_BYTES)))
#define ____cacheline_aligned_in_smp ____cacheline_aligned
#define __cacheline_aligned          ____cacheline_aligned


/*********************
 ** linux/seqlock.h **
 *********************/

typedef unsigned seqlock_t;

void seqlock_init (seqlock_t *);

#define __SEQLOCK_UNLOCKED(x) 0


/******************
 ** linux/init.h **
 ******************/

#define __init
#define __exit
#define __devinitconst

#define _SETUP_CONCAT(a, b) __##a##b
#define SETUP_CONCAT(a, b) _SETUP_CONCAT(a, b)
#define __setup(str, fn) static void SETUP_CONCAT(fn, SETUP_SUFFIX)(void *addrs) { fn(addrs); }

#define  core_initcall(fn)   void core_##fn(void) { fn(); }
#define subsys_initcall(fn) void subsys_##fn(void) { fn(); }
#define pure_initcall(fd) void pure_##fn(void) { printk("PURE_INITCALL"); fn(); }


/********************
 ** linux/module.h **
 ********************/

#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define MODULE_LICENSE(x)
#define MODULE_NAME_LEN (64 - sizeof(long))
#define MODULE_ALIAS(name)
#define MODULE_AUTHOR(name)
#define MODULE_DESCRIPTION(desc)
#define MODULE_VERSION(version)
#define THIS_MODULE 0
#define MODULE_FIRMWARE(_firmware)
#define MODULE_DEVICE_TABLE(type, name)


struct module;
#define module_init(fn) void module_##fn(void) { fn(); }
#define module_exit(fn) void module_exit_##fn(void) { fn(); }
void module_put_and_exit(int);

void module_put(struct module *);
void __module_get(struct module *module);
int try_module_get(struct module *);


/*************************
 ** linux/moduleparam.h **
 *************************/

#define module_param(name, type, perm)
#define module_param_named(name, value, type, perm)
#define MODULE_PARM_DESC(_parm, desc)
#define kparam_block_sysfs_write(name)
#define kparam_unblock_sysfs_write(name)


/***********************************
 ** linux/rwlock_types.h/rwlock.h **
 ***********************************/

typedef unsigned rwlock_t;

#define DEFINE_RWLOCK(x) rwlock_t x;
#define __RW_LOCK_UNLOCKED(x) 0

void rwlock_init(rwlock_t *);

void write_lock_bh(rwlock_t *);
void write_unlock_bh(rwlock_t *);
void write_lock(rwlock_t *);
void write_unlock(rwlock_t *);
void write_lock_irq(rwlock_t *);
void write_unlock_irq(rwlock_t *);
void write_lock_irqsave(rwlock_t *, unsigned long);
void write_unlock_irqrestore(rwlock_t *, unsigned long);

void read_lock(rwlock_t *);
void read_unlock(rwlock_t *);
void read_lock_bh(rwlock_t *);
void read_unlock_bh(rwlock_t *);


/*********************
 ** linux/seqlock.h **
 *********************/

typedef struct seqcount {
	unsigned sequence;
} seqcount_t;

unsigned read_seqbegin(const seqlock_t *sl);
unsigned read_seqretry(const seqlock_t *sl, unsigned start);
void write_seqlock_bh(seqlock_t *);
void write_sequnlock_bh(seqlock_t *);
void write_seqlock(seqlock_t *);
void write_sequnlock(seqlock_t *);
void write_seqcount_begin(seqcount_t *);
void write_seqcount_end(seqcount_t *);


/****************************
 ** linux/percpu_counter.h **
 ****************************/

struct percpu_counter
{
	s64 count;
};

static inline int percpu_counter_init(struct percpu_counter *fbc, s64 amount)
{
	fbc->count = amount;
	return 0;
}

static inline s64 percpu_counter_read(struct percpu_counter *fbc)
{
	return fbc->count;
}

static inline
void percpu_counter_add(struct percpu_counter *fbc, s64 amount)
{
	fbc->count += amount;
}

static inline
void __percpu_counter_add(struct percpu_counter *fbc, s64 amount, s32 batch)
{
	percpu_counter_add(fbc, amount);
}

s64 percpu_counter_sum_positive(struct percpu_counter *fbc);

static inline void percpu_counter_inc(struct percpu_counter *fbc)
{
	percpu_counter_add(fbc, 1);
}

static inline void percpu_counter_dec(struct percpu_counter *fbc)
{
	percpu_counter_add(fbc, -1);
}

static inline
s64 percpu_counter_read_positive(struct percpu_counter *fbc)
{
	return fbc->count;
}

void percpu_counter_destroy(struct percpu_counter *fbc);


/****************************
 ** linux/u64_stats_sync.h **
 ****************************/

struct u64_stats_sync { unsigned unused; };


/********************
 ** linux/uidgid.h **
 ********************/

typedef unsigned  kuid_t;
typedef unsigned  kgid_t;

#define GLOBAL_ROOT_UID (kuid_t)0

struct user_namespace;
extern uid_t from_kuid_munged(struct user_namespace *to, kuid_t uid);
extern gid_t from_kgid_munged(struct user_namespace *to, kgid_t gid);


 /*************************
  ** linux/bottom_half.h **
  *************************/

void local_bh_disable(void);
void local_bh_enable(void);


/***************
 ** net/scm.h **
 ***************/

struct scm_creds { unsigned unused; };

struct scm_cookie
{
	struct scm_creds creds;
};

void scm_destroy(struct scm_cookie *scm);
struct socket;
struct msghdr;
void scm_recv(struct socket *sock, struct msghdr *msg, struct scm_cookie *scm, int flags);
int scm_send(struct socket *sock, struct msghdr *msg, struct scm_cookie *scm, bool forcecreds);


/*************************
 ** linux/etherdevice.h **
 *************************/

struct sk_buff;

int eth_mac_addr(struct net_device *, void *);
int eth_validate_addr(struct net_device *);
__be16 eth_type_trans(struct sk_buff *, struct net_device *);
int is_valid_ether_addr(const u8 *);

void random_ether_addr(u8 *addr);

struct net_device *alloc_etherdev(int);

void eth_hw_addr_random(struct net_device *dev);
void eth_random_addr(u8 *addr);

static inline void eth_broadcast_addr(u8 *addr) {
	memset(addr, 0xff, ETH_ALEN); }

static inline bool is_broadcast_ether_addr(const u8 *addr)
{
	return (*(const u16 *)(addr + 0) &
			*(const u16 *)(addr + 2) &
			*(const u16 *)(addr + 4)) == 0xffff;
}

static inline bool ether_addr_equal(const u8 *addr1, const u8 *addr2)
{
	const u16 *a = (const u16 *)addr1;
	const u16 *b = (const u16 *)addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) == 0;
}

static inline bool is_multicast_ether_addr(const u8 *addr)
{
	return 0x01 & addr[0];
}

static inline bool ether_addr_equal_64bits(const u8 addr1[6+2], const u8 addr2[6+2])
{
	u64 fold = (*(const u64 *)addr1) ^ (*(const u64 *)addr2);

	return (fold << 16) == 0;
}


/************************
 ** net/netns/packet.h **
 ************************/

struct netns_packet {
	struct mutex            sklist_lock;
	struct hlist_head       sklist;
};


/*************************
 ** net/net_namespace.h **
 *************************/

#include <uapi/linux/snmp.h>
#include <net/netns/mib.h>
#include <net/netns/ipv4.h>

enum {
	LOOPBACK_IFINDEX   = 1,
	NETDEV_HASHBITS    = 8,
	NETDEV_HASHENTRIES = 1 << NETDEV_HASHBITS,
};

struct user_namespace;
struct proc_net;

extern struct net init_net;

struct net
{
	atomic_t                  passive;
	atomic_t                  count;
	struct list_head          list;
	struct list_head          cleanup_list;
	struct list_head          exit_list;
	struct proc_dir_entry    *proc_net;
	struct list_head          dev_base_head;
	struct hlist_head        *dev_name_head;
	struct hlist_head        *dev_index_head;
	unsigned int              dev_base_seq;
	int                       ifindex;
	unsigned int              dev_unreg_count;
	struct net_device        *loopback_dev;
	struct user_namespace    *user_ns;
	unsigned int              proc_inum;
	struct proc_dir_entry    *proc_net_stat;
	struct sock              *rtnl;
	struct sock              *genl_sock;
	struct netns_mib          mib;
	struct netns_packet       packet;
	struct netns_ipv4         ipv4;
	struct net_generic __rcu *gen;
	atomic_t                  rt_genid;
};


struct pernet_operations
{
	struct list_head list;
	int (*init)(struct net *net);
	void (*exit)(struct net *net);
	void (*exit_batch)(struct list_head *net_exit_list);
	int *id;
	size_t size;
};


extern struct list_head net_namespace_list;

#define __net_initdata
#define __net_init
#define __net_exit

#define for_each_net(VAR)                               \
	for(VAR = &init_net; VAR; VAR = 0)

#define for_each_net_rcu(VAR)               \
	list_for_each_entry_rcu(VAR, &net_namespace_list, list)


#define read_pnet(pnet) (&init_net)
#define write_pnet(pnet, net) do { (void)(net);} while (0)

static inline struct net *hold_net(struct net *net) { return net; }
static inline struct net *get_net(struct net *net) { return net; }
static inline void put_net(struct net *net) { }
/* always return true because there is only the init_net namespace */
static inline int net_eq(const struct net *net1, const struct net *net2) {
	return 1; }

struct net *get_net_ns_by_pid(pid_t pid);
struct net *get_net_ns_by_fd(int pid);

int register_pernet_subsys(struct pernet_operations *);
void unregister_pernet_subsys(struct pernet_operations *);
int register_pernet_device(struct pernet_operations *);
void unregister_pernet_device(struct pernet_operations *);
void release_net(struct net *net);

int  rt_genid(struct net *);
void rt_genid_bump(struct net *);


/*************************
 ** net/netns/generic.h **
 *************************/

struct net_generic
{
	unsigned int len;
	void *ptr[0];
};


/********************
 ** linux/socket.h **
 ********************/

struct sockaddr;


/****************************
 ** uapi/linux/netdevice.h **
 ****************************/

#define MAX_ADDR_LEN    32

static inline struct net *dev_net(const struct net_device *dev) {
	return &init_net; }


/***********************
 ** linux/netdevice.h **
 ***********************/

enum {
	NETDEV_ALIGN = 32,

	NETDEV_UP                 = 0x0001,
	NETDEV_DOWN               = 0x0002,
	NETDEV_CHANGE             = 0x0004,
	NETDEV_REGISTER           = 0x0005,
	NETDEV_UNREGISTER         = 0x0006,
	NETDEV_CHANGEADDR         = 0x0008,
	NETDEV_GOING_DOWN         = 0x0009,
	NETDEV_CHANGENAME         = 0x000A,
	NETDEV_PRE_UP             = 0x000D,
	NETDEV_PRE_TYPE_CHANGE    = 0x000E,
	NETDEV_POST_INIT          = 0x0010,
	NETDEV_UNREGISTER_FINAL   = 0x0011,
	NETDEV_RELEASE            = 0x0012,
	NETDEV_JOIN               = 0x0014,

};

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

enum netdev_tx {
	NETDEV_TX_OK = 0,
	NETDEV_TX_BUSY = 0x10,
};
typedef enum netdev_tx netdev_tx_t;

enum {
	NET_RX_SUCCESS  = 0,
	NET_ADDR_RANDOM = 1,
	NET_ADDR_SET    = 3,

	NET_XMIT_DROP   = 0x01,
	NET_XMIT_CN     = 0x02,

	NETIF_MSG_DRV   = 0x1,
	NETIF_MSG_PROBE = 0x2,
	NETIF_MSG_LINK  = 0x4,

};

#define net_xmit_errno(e) ((e) != NET_XMIT_CN ? -ENOBUFS : 0)

struct ifreq;
typedef u16 (*select_queue_fallback_t)(struct net_device *dev,
                                       struct sk_buff *skb);

struct ifla_vf_info;
struct nlattr;
struct ndmsg;
struct netlink_callback;
struct nlmsghdr;

struct net_device_ops
{
	int         (*ndo_init)(struct net_device *dev);
	void        (*ndo_uninit)(struct net_device *dev);
	int         (*ndo_open)(struct net_device *dev);
	int         (*ndo_stop)(struct net_device *dev);
	netdev_tx_t (*ndo_start_xmit) (struct sk_buff *skb, struct net_device *dev);
	u16         (*ndo_select_queue)(struct net_device *dev, struct sk_buff *skb,
	                                void *accel_priv,
	                                select_queue_fallback_t fallback);
	void        (*ndo_set_rx_mode)(struct net_device *dev);
	int         (*ndo_set_mac_address)(struct net_device *dev, void *addr);
	int         (*ndo_validate_addr)(struct net_device *dev);
	int         (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
	int         (*ndo_set_config)(struct net_device *dev,
	                              struct ifmap *map);
	void        (*ndo_tx_timeout) (struct net_device *dev);
	int         (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
	int         (*ndo_set_features)(struct net_device *dev, netdev_features_t features);
	int         (*ndo_set_vf_mac)(struct net_device *dev,
	                              int queue, u8 *mac);
	int         (*ndo_set_vf_vlan)(struct net_device *dev,
	                               int queue, u16 vlan, u8 qos);
	int         (*ndo_set_vf_tx_rate)(struct net_device *dev,
	                                  int vf, int rate);
	int         (*ndo_set_vf_spoofchk)(struct net_device *dev,
	                                   int vf, bool setting);
	int         (*ndo_get_vf_config)(struct net_device *dev,
	                                 int vf,
	                                 struct ifla_vf_info *ivf);
	int         (*ndo_set_vf_link_state)(struct net_device *dev,
	                                     int vf, int link_state);
	int         (*ndo_set_vf_port)(struct net_device *dev, int vf,
	                               struct nlattr *port[]);
	int         (*ndo_get_vf_port)(struct net_device *dev,
	                               int vf, struct sk_buff *skb);
	int         (*ndo_del_slave)(struct net_device *dev,
	                             struct net_device *slave_dev);
	int         (*ndo_add_slave)(struct net_device *dev,
	                             struct net_device *slave_dev);
	int         (*ndo_fdb_add)(struct ndmsg *ndm,
	                           struct nlattr *tb[],
	                           struct net_device *dev,
	                           const unsigned char *addr,
	                           u16 flags);
	int         (*ndo_fdb_del)(struct ndmsg *ndm,
	                           struct nlattr *tb[],
	                           struct net_device *dev,
	                           const unsigned char *addr);
	int         (*ndo_fdb_dump)(struct sk_buff *skb,
	                            struct netlink_callback *cb,
	                            struct net_device *dev,
	                            int idx);
	int         (*ndo_bridge_setlink)(struct net_device *dev,
	                                  struct nlmsghdr *nlh);
	int         (*ndo_bridge_getlink)(struct sk_buff *skb,
	                                  u32 pid, u32 seq,
	                                  struct net_device *dev,
	                                  u32 filter_mask);
	int         (*ndo_bridge_dellink)(struct net_device *dev,
	                                  struct nlmsghdr *nlh);
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

struct netdev_hw_addr_list {
	struct list_head list;
	int              count;
};

#define netdev_hw_addr_list_count(l) ((l)->count)
#define netdev_hw_addr_list_empty(l) (netdev_hw_addr_list_count(l) == 0)
#define netdev_hw_addr_list_for_each(ha, l) \
	list_for_each_entry(ha, &(l)->list, list)


enum {
	GSO_MAX_SIZE = 65536,
	GSO_MAX_SEGS = 65535,
};

struct Qdisc;

struct netdev_queue
{
	struct net_device *dev;
	int numa_node;
};

/* NET_DEVICE */
struct net_device
{
	char           name[IFNAMSIZ];
	char            *ifalias;

	unsigned long       mem_end;    /* shared mem end   */
	unsigned long       mem_start;  /* shared mem start */
	unsigned long       base_addr;  /* device I/O address   */
	int         irq;        /* device IRQ number    */

	u32            features;
	u32            hw_features;

	struct net_device_stats      stats;
	const struct net_device_ops *netdev_ops;
	const struct ethtool_ops *ethtool_ops;

	unsigned long  state;
	struct list_head    dev_list;
	int         iflink;
	int            ifindex;

	const struct header_ops *header_ops;

	unsigned int   flags;
	unsigned int   priv_flags;
	unsigned short gflags;
	unsigned char  operstate;
	unsigned char  link_mode;

	unsigned char       if_port;    /* Selectable AUI, TP,..*/
	unsigned char       dma;        /* DMA channel      */
	unsigned short hard_header_len; /* hardware hdr length  */
	unsigned int   mtu;
	unsigned short type;
	unsigned short needed_headroom;
	unsigned short needed_tailroom;
	unsigned char  perm_addr[MAX_ADDR_LEN];
	unsigned char  addr_assign_type;
	unsigned char  addr_len;
	struct netdev_hw_addr_list  uc; /* Unicast mac addresses */
	struct netdev_hw_addr_list  mc;

	unsigned int        promiscuity;
	struct wireless_dev *ieee80211_ptr;

	unsigned char *dev_addr;
	unsigned char  _dev_addr[ETH_ALEN];

	unsigned int        real_num_tx_queues;
	struct netdev_queue *_tx;

	struct netdev_queue __rcu *ingress_queue;
	unsigned char       broadcast[MAX_ADDR_LEN];

	unsigned int        num_tx_queues;

	struct Qdisc        *qdisc;

	unsigned long tx_queue_len;
	unsigned long  trans_start;    /* Time (in jiffies) of last Tx */

	int            watchdog_timeo; /* used by dev_watchdog() */
	struct hlist_node   index_hlist;

	enum {
		RTNL_LINK_INITIALIZED,
		RTNL_LINK_INITIALIZING,
	} rtnl_link_state:16;

	void (*destructor)(struct net_device *dev);
	const struct rtnl_link_ops *rtnl_link_ops;

	unsigned int        gso_max_size;
	u16         gso_max_segs;

	struct device  dev;
	void          *priv;
	unsigned       net_ip_align;

	struct phy_device *phydev;

	int group;

	void *lx_nic_device; /* our own Nic_device */
};


struct netdev_hw_addr
{
	struct list_head list;
	unsigned char    addr[MAX_ADDR_LEN];
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
int dev_addr_init(struct net_device *dev);
void dev_uc_init(struct net_device *dev);
void dev_mc_init(struct net_device *dev);
void free_netdev(struct net_device *);
int  netif_rx(struct sk_buff *);
int netif_rx_ni(struct sk_buff *skb);
int netif_receive_skb(struct sk_buff *skb);
void netif_tx_start_queue(struct netdev_queue *dev_queue);
void netif_tx_stop_queue(struct netdev_queue *dev_queue);
void netif_tx_start_all_queues(struct net_device *dev);
void netif_tx_stop_all_queues(struct net_device *dev);
void netif_tx_wake_all_queues(struct net_device *);
void __netif_tx_lock_bh(struct netdev_queue *txq);
void __netif_tx_unlock_bh(struct netdev_queue *txq);
void netif_start_subqueue(struct net_device *dev, u16 queue_index);
void netif_stop_subqueue(struct net_device *dev, u16 queue_index);
void netif_wake_subqueue(struct net_device *dev, u16 queue_index);
bool netif_dormant(const struct net_device *dev);
netdev_features_t netif_skb_features(struct sk_buff *skb);
bool netif_supports_nofcs(struct net_device *dev);
bool netif_xmit_frozen_or_stopped(const struct netdev_queue *dev_queue);

static inline void netif_addr_lock_bh(struct net_device *dev)   { }
static inline void netif_addr_unlock_bh(struct net_device *dev) { }

void netdev_set_default_ethtool_ops(struct net_device *dev, const struct ethtool_ops *ops);
int netdev_mc_empty(struct net_device *);
unsigned netdev_mc_count(struct net_device * dev);
int register_netdev(struct net_device *);
void unregister_netdev(struct net_device *);
void netdev_rx_csum_fault(struct net_device *dev);
void netdev_run_todo(void);
int register_netdevice(struct net_device *dev);
void unregister_netdevice_many(struct list_head *head);
void unregister_netdevice_queue(struct net_device *dev, struct list_head *head);
static inline void unregister_netdevice(struct net_device *dev)
{
	unregister_netdevice_queue(dev, NULL);
}
struct net_device *netdev_master_upper_dev_get(struct net_device *dev);
void netdev_state_change(struct net_device *dev);
int call_netdevice_notifiers(unsigned long val, struct net_device *dev);
struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name, void (*setup)(struct net_device *), unsigned int txqs, unsigned int rxqs);
struct netdev_notifier_info;
struct net_device * netdev_notifier_info_to_dev(struct netdev_notifier_info *info);
int register_netdevice_notifier(struct notifier_block *nb);
int unregister_netdevice_notifier(struct notifier_block *nb);
struct netdev_queue *netdev_get_tx_queue(const struct net_device *dev, unsigned int index);
u16 netdev_cap_txqueue(struct net_device *dev, u16 queue_index);

static inline bool netdev_uses_dsa_tags(struct net_device *dev) { return false; }
static inline bool netdev_uses_trailer_tags(struct net_device *dev) { return false; }
int __init netdev_boot_setup(char *str);

void synchronize_net(void);

void ether_setup(struct net_device *dev);

void dev_put(struct net_device *dev);
void dev_hold(struct net_device *dev);
struct net_device *__dev_get_by_index(struct net *net, int ifindex);
struct net_device *__dev_get_by_name(struct net *net, const char *name);
struct net_device *dev_get_by_index(struct net *net, int ifindex);
struct net_device *dev_get_by_index_rcu(struct net *net, int ifindex);
struct net_device *dev_get_by_name(struct net *net, const char *name);
struct net_device *dev_get_by_name_rcu(struct net *net, const char *name);
int dev_queue_xmit(struct sk_buff *skb);
struct netdev_phys_port_id;
int dev_get_phys_port_id(struct net_device *dev, struct netdev_phys_port_id *ppid);
unsigned int dev_get_flags(const struct net_device *);
struct rtnl_link_stats64 *dev_get_stats(struct net_device *dev, struct rtnl_link_stats64 *storage);
int dev_change_net_namespace(struct net_device *, struct net *, const char *);
int dev_alloc_name(struct net_device *dev, const char *name);
int dev_close(struct net_device *dev);
int dev_set_mac_address(struct net_device *, struct sockaddr *);
int dev_set_mtu(struct net_device *, int);
int dev_set_promiscuity(struct net_device *dev, int inc);
int dev_set_allmulti(struct net_device *dev, int inc);
void dev_set_group(struct net_device *, int);
int dev_change_name(struct net_device *, const char *);
int dev_set_alias(struct net_device *, const char *, size_t);
int __dev_change_flags(struct net_device *, unsigned int flags);
void __dev_notify_flags(struct net_device *, unsigned int old_flags, unsigned int gchanges);
int dev_change_flags(struct net_device *, unsigned int);
int dev_change_carrier(struct net_device *, bool new_carrier);
void dev_net_set(struct net_device *dev, struct net *net);
struct packet_type;
void dev_add_pack(struct packet_type *pt);
void __dev_remove_pack(struct packet_type *pt);
void dev_remove_pack(struct packet_type *pt);
bool dev_xmit_complete(int rc);
int dev_hard_header(struct sk_buff *skb, struct net_device *dev, unsigned short type, const void *daddr, const void *saddr, unsigned int len);
int dev_parse_header(const struct sk_buff *skb, unsigned char *haddr);
void dev_set_uevent_suppress(struct device *dev, int val);

int dev_uc_add(struct net_device *dev, const unsigned char *addr);
int dev_uc_add_excl(struct net_device *dev, const unsigned char *addr);
int dev_uc_del(struct net_device *dev, const unsigned char *addr);
int dev_mc_add(struct net_device *dev, const unsigned char *addr);
int dev_mc_add_excl(struct net_device *dev, const unsigned char *addr);
int dev_mc_del(struct net_device *dev, const unsigned char *addr);

enum {
	LL_MAX_HEADER  = 96, /* XXX check CONFIG_WLAN_MESH */
};

struct hh_cache
{
	u16     hh_len;
	u16     __pad;
	seqlock_t   hh_lock;

	/* cached hardware header; allow for machine alignment needs.        */
#define HH_DATA_MOD 16
#define HH_DATA_OFF(__len) \
	(HH_DATA_MOD - (((__len - 1) & (HH_DATA_MOD - 1)) + 1))
#define HH_DATA_ALIGN(__len) \
	(((__len)+(HH_DATA_MOD-1))&~(HH_DATA_MOD - 1))
	unsigned long   hh_data[HH_DATA_ALIGN(LL_MAX_HEADER) / sizeof(long)];

};

extern rwlock_t             dev_base_lock;

#define for_each_netdev(net, d)     \
	list_for_each_entry(d, &(net)->dev_base_head, dev_list)
#define for_each_netdev_rcu(net, d)     \
	list_for_each_entry_rcu(d, &(net)->dev_base_head, dev_list)

#define net_device_entry(lh)    list_entry(lh, struct net_device, dev_list)

static inline struct net_device *first_net_device(struct net *net)
{
	return list_empty(&net->dev_base_head) ? NULL :
	                  net_device_entry(net->dev_base_head.next);
}

#define NAPI_GRO_CB(skb) ((struct napi_gro_cb *)(skb)->cb)

enum {
	NAPI_GRO_FREE = 1,
	NAPI_GRO_FREE_STOLEN_HEAD = 2,
};

struct napi_gro_cb
{
	u16 count;
	u8  same_flow;
	u8 free;
	struct sk_buff *last;
};

struct neighbour;

struct header_ops {
	int  (*create) (struct sk_buff *skb, struct net_device *dev,
	                unsigned short type, const void *daddr,
	                const void *saddr, unsigned int len);
	int  (*parse)(const struct sk_buff *skb, unsigned char *haddr);
	int  (*rebuild)(struct sk_buff *skb);
	int  (*cache)(const struct neighbour *neigh, struct hh_cache *hh, __be16 type);
	void (*cache_update)(struct hh_cache *hh,
	                     const struct net_device *dev,
	                     const unsigned char *haddr);
};

extern struct kobj_ns_type_operations net_ns_type_operations;

enum skb_free_reason {
	SKB_REASON_CONSUMED,
	SKB_REASON_DROPPED,
};

void consume_skb(struct sk_buff *skb);
unsigned int skb_gro_offset(const struct sk_buff *skb);
unsigned int skb_gro_len(const struct sk_buff *skb);
__be16 skb_network_protocol(struct sk_buff *skb, int *depth);
bool can_checksum_protocol(netdev_features_t features, __be16 protocol);

/* XXX dev_kfree_skb_any */
static inline void dev_kfree_skb_any(struct sk_buff *skb)
{
	dev_dbg(0, "%s called\n", __func__);
	/* __dev_kfree_skb_any(skb, SKB_REASON_DROPPED); */
	consume_skb(skb);
}

struct packet_type
{
	__be16             type;   /* This is really htons(ether_type). */
	struct net_device *dev;   /* NULL is wildcarded here       */
	int                (*func) (struct sk_buff *,
	                            struct net_device *,
	                            struct packet_type *,
	                            struct net_device *);
	bool               (*id_match)(struct packet_type *ptype,
	                               struct sock *sk);
	void              *af_packet_priv;
	struct list_head   list;
};

enum {
	MAX_PHYS_PORT_ID_LEN = 32,
};
 
struct netdev_phys_port_id {
	unsigned char id[MAX_PHYS_PORT_ID_LEN];
	unsigned char id_len;
};

/* XXX */ size_t LL_RESERVED_SPACE(struct net_device*);

bool net_gso_ok(netdev_features_t features, int gso_type);
void net_enable_timestamp(void);
void net_disable_timestamp(void);
void txq_trans_update(struct netdev_queue *txq);
int __hw_addr_sync(struct netdev_hw_addr_list *to_list, struct netdev_hw_addr_list *from_list, int addr_len);
void __hw_addr_unsync(struct netdev_hw_addr_list *to_list, struct netdev_hw_addr_list *from_list, int addr_len);
void __hw_addr_init(struct netdev_hw_addr_list *list);


/*************************
 ** linux/percpu-defs.h **
 *************************/

#define DECLARE_PER_CPU_ALIGNED(type, name) \
	extern typeof(type) name

#define DEFINE_PER_CPU_ALIGNED(type, name) \
	typeof(type) name

#define DEFINE_PER_CPU(type, name) \
	typeof(type) name

#define EXPORT_PER_CPU_SYMBOL(x)


/*********************
 ** linux/lockdep.h **
 *********************/

#include <linux/lockdep.h>


/*************************
 ** linux/etherdevice.h **
 *************************/

static inline bool is_unicast_ether_addr(const u8 *addr) {
	return !(0x01 & addr[0]); }

static inline bool is_zero_ether_addr(const u8 *addr)
{
	return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

/* Reserved Ethernet Addresses per IEEE 802.1Q */
static const u8 eth_reserved_addr_base[ETH_ALEN] __aligned(2) =
{ 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };

static inline bool is_link_local_ether_addr(const u8 *addr)
{
	__be16 *a = (__be16 *)addr;
	static const __be16 *b = (const __be16 *)eth_reserved_addr_base;
	static const __be16 m = cpu_to_be16(0xfff0);

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | ((a[2] ^ b[2]) & m)) == 0;
}


/******************
 ** linux/wait.h **
 ******************/

typedef struct wait_queue_head { void *list; } wait_queue_head_t;
typedef struct wait_queue { unsigned unused; } wait_queue_t;

#define DEFINE_WAIT(name) \
	wait_queue_t name;

#define __WAIT_QUEUE_HEAD_INITIALIZER(name) { 0 }

#define DECLARE_WAITQUEUE(name, tsk) \
	wait_queue_t name

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	wait_queue_head_t name = __WAIT_QUEUE_HEAD_INITIALIZER(name)

#define DEFINE_WAIT_FUNC(name, function) \
	wait_queue_t name

/* simplified signature */
void __wake_up(wait_queue_head_t *q, bool all);

#define wake_up(x)                   __wake_up(x, false)
#define wake_up_all(x)               __wake_up(x, true)
#define wake_up_interruptible(x)     __wake_up(x, false)
#define wake_up_interruptible_all(x) __wake_up(x, true)

void init_waitqueue_head(wait_queue_head_t *);
int waitqueue_active(wait_queue_head_t *);

/* void wake_up_interruptible(wait_queue_head_t *); */
void wake_up_interruptible_sync_poll(wait_queue_head_t *, int);
void wake_up_interruptible_poll(wait_queue_head_t *, int);

void prepare_to_wait(wait_queue_head_t *, wait_queue_t *, int);
void prepare_to_wait_exclusive(wait_queue_head_t *, wait_queue_t *, int);
void finish_wait(wait_queue_head_t *, wait_queue_t *);

int  autoremove_wake_function(wait_queue_t *, unsigned, int, void *);
void add_wait_queue(wait_queue_head_t *, wait_queue_t *);
void add_wait_queue_exclusive(wait_queue_head_t *, wait_queue_t *);
void remove_wait_queue(wait_queue_head_t *, wait_queue_t *);

/* our wait event implementation - it's okay as value */
void __wait_event(wait_queue_head_t);

#define _wait_event(wq, condition) while (!(condition)) { __wait_event(wq); }
#define wait_event(wq, condition) ({ _wait_event(wq, condition); })
#define wait_event_interruptible(wq, condition) ({ _wait_event(wq, condition); 0; })

#define _wait_event_timeout(wq, condition, timeout)    \
	({ int res = 1;                                    \
		prepare_to_wait(&wq, 0, 0);                    \
		while (1) {                                    \
			if ((condition) || !res) {                 \
				break;                                 \
			}                                          \
			res = schedule_timeout(jiffies + timeout); \
		}                                              \
		finish_wait(&wq, 0);                           \
		res;                                           \
	})

#define wait_event_timeout(wq, condition, timeout)               \
	({                                                           \
		int ret = _wait_event_timeout(wq, (condition), timeout); \
		ret;                                                     \
	})


/************************
 ** linux/capability.h **
 ************************/

enum {
	CAP_NET_ADMIN = 12,
	CAP_NET_RAW   = 13,
	CAP_SYS_ADMIN = 21,
};

bool capable(int cap);
bool ns_capable(struct user_namespace *ns, int cap);


/******************
 ** linux/stat.h **
 ******************/

#define S_IFMT  00170000
#define S_IFSOCK 0140000

#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#define S_IRUGO 00444
#define S_IWUSR 00200
#define S_IRUSR 00400


/****************
 ** linux/fs.h **
 ****************/

struct fown_struct { unsigned unused; };

struct file {
	unsigned int       f_flags;
	const struct cred *f_cred;
	struct fown_struct f_owner;
	void              *private_data;
};

typedef unsigned fl_owner_t;

struct inode;

struct inode *file_inode(struct file *f);

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

static inline loff_t no_llseek(struct file *file, loff_t offset, int origin) {
	return -ESPIPE; }
int nonseekable_open(struct inode * inode, struct file * filp);

struct inode
{
	umode_t       i_mode;
	kuid_t        i_uid;
	unsigned long i_ino;
};

int send_sigurg(struct fown_struct *fown);


/*************************
 ** asm-generic/fcntl.h **
 *************************/

enum { O_NONBLOCK = 0x4000 };


/*****************************
 ** linux/platform_device.h **
 *****************************/

struct platform_device {
	char  * name;
	int     id;

	struct device dev;

	u32 num_resources;
	struct resource * resource;

};

/* needed by net/rfkill/rfkill-gpio.c */
struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	struct device_driver driver;
};

void *platform_get_drvdata(const struct platform_device *pdev);
void platform_set_drvdata(struct platform_device *pdev, void *data);
struct platform_device *platform_device_register_simple( const char *name, int id, const struct resource *res, unsigned int num);
void platform_device_unregister(struct platform_device *);

#define module_platform_driver(x)


/************************
 ** linux/tracepoint.h **
 ************************/

#define DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print)
#define DEFINE_EVENT(template, name, proto, args) \
	static inline void trace_##name(proto) { }
#define TRACE_EVENT(name, proto, args, struct, assign, print) \
	static inline void trace_##name(proto) { }

/* needed by drivers/net/wireless/iwlwifi/iwl-devtrace.h */
#define TP_PROTO(args...)     args


/********************
 ** linux/dcache.h **
 ********************/

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


/***************************
 ** linux/dma-direction.h **
 ***************************/

enum dma_data_direction
{
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE     = 1,
	DMA_FROM_DEVICE   = 2
};


/**************************************
 ** asm-generic/dma-mapping-common.h **
 **************************************/

dma_addr_t dma_map_page(struct device *dev, struct page *page, size_t offset, size_t size, enum dma_data_direction dir);
void dma_unmap_page(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);
dma_addr_t dma_map_single(struct device *dev, void *ptr, size_t size, enum dma_data_direction dir);
void dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);

struct scatterlist;
int dma_map_sg(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir);
void dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction dir);
void dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);
void dma_sync_single_for_device(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);
void dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nelems, enum dma_data_direction dir);
void dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg, int nelems, enum dma_data_direction dir);


/***********************
 ** asm/dma-mapping.h **
 ***********************/

int dma_supported(struct device *hwdev, u64 mask);
int dma_set_mask(struct device *dev, u64 mask);
int dma_mapping_error(struct device *dev, dma_addr_t dma_addr);
void *dma_alloc_coherent(struct device *, size_t size, dma_addr_t *dma, gfp_t);
void dma_free_coherent(struct device *, size_t size, void *vaddr, dma_addr_t bus);


/*************************
 ** linux/dma-mapping.h **
 *************************/

#define DEFINE_DMA_UNMAP_ADDR(ADDR_NAME)        dma_addr_t ADDR_NAME
#define DEFINE_DMA_UNMAP_LEN(LEN_NAME)          u32 LEN_NAME
#define dma_unmap_addr(PTR, ADDR_NAME)           ((PTR)->ADDR_NAME)
#define dma_unmap_addr_set(PTR, ADDR_NAME, VAL)  (((PTR)->ADDR_NAME) = (VAL))
#define dma_unmap_len(PTR, LEN_NAME)             ((PTR)->LEN_NAME)
#define dma_unmap_len_set(PTR, LEN_NAME, VAL)    (((PTR)->LEN_NAME) = (VAL))

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

int dma_set_coherent_mask(struct device *dev, u64 mask);
void *dma_zalloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag);


/************************
 ** linux/completion.h **
 ************************/

struct completion { unsigned done; };

void complete(struct completion *);
void complete_all(struct completion *);
void init_completion(struct completion *x);
unsigned long wait_for_completion_timeout(struct completion *x, unsigned long timeout);
void wait_for_completion(struct completion *x);
int wait_for_completion_interruptible(struct completion *x);
long wait_for_completion_interruptible_timeout(struct completion *x, unsigned long timeout);


/**********************
 ** linux/firmware.h **
 **********************/

struct firmware {
	size_t size;
	const u8 *data;
	struct page **pages;

	void *priv;
};

int request_firmware(const struct firmware **fw, const char *name, struct device *device);
void release_firmware(const struct firmware *fw);
int request_firmware_nowait( struct module *module, bool uevent, const char *name, struct device *device, gfp_t gfp, void *context, void (*cont)(const struct firmware *fw, void *context));


/***********************
 ** linux/irqreturn.h **
 ***********************/

typedef enum irqreturn {
	IRQ_NONE        = 0,
	IRQ_HANDLED     = 1,
	IRQ_WAKE_THREAD = 2,
} irqreturn_t;


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


/***********************
 ** linux/interrupt.h **
 ***********************/

enum {
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	NET_SOFTIRQS,
};

#define IRQF_SHARED   0x00000080
/* #define IRQF_DISABLED 0x00000020 */

/* void local_irq_enable(void); */
/* void local_irq_disable(void); */

typedef irqreturn_t (*irq_handler_t)(int, void *);

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
void free_irq(unsigned int, void *);
int request_threaded_irq(unsigned int irq, irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags, const char *name, void *dev);

void tasklet_disable(struct tasklet_struct *t);
void tasklet_enable(struct tasklet_struct *t);


/*****************
 ** linux/pci.h **
 *****************/

#include <linux/pci_ids.h>
#include <uapi/linux/pci_regs.h>

enum {
	PCI_DMA_BIDIRECTIONAL = 0,
	PCI_DMA_TODEVICE,
	PCI_DMA_FROMDEVICE,
	PCI_DMA_NONE
};


enum { PCI_ANY_ID = ~0U };
enum { DEVICE_COUNT_RESOURCE = 6 };


typedef enum {
	PCI_D3cold = 4
} pci_power_t;


#include <linux/mod_devicetable.h>

/*
 * PCI types
 */
struct pci_bus;

struct pci_dev {
	unsigned int devfn;
	unsigned int irq;
	struct resource resource[DEVICE_COUNT_RESOURCE];
	struct pci_bus *bus;
	unsigned short vendor;
	unsigned short device;
	unsigned short subsystem_device;
	unsigned int class;
	struct device dev;
	u8 revision;
	u8 pcie_cap;
	u16 pcie_flags_reg;
};

struct pci_driver {
	char                        *name;
	const struct pci_device_id  *id_table;
	int                        (*probe)  (struct pci_dev *dev,
                                          const struct pci_device_id *id);
	void                       (*remove) (struct pci_dev *dev);
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
void pci_dev_put(struct pci_dev *dev);
struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device, struct pci_dev *from);

int pci_enable_device(struct pci_dev *dev);
void pci_disable_device(struct pci_dev *dev);
int pci_register_driver(struct pci_driver *driver);
void pci_unregister_driver(struct pci_driver *driver);
const char *pci_name(const struct pci_dev *pdev);
bool pci_dev_run_wake(struct pci_dev *dev);
unsigned int pci_resource_flags(struct pci_dev *dev, unsigned bar);
void pci_set_master(struct pci_dev *dev);
int pci_set_mwi(struct pci_dev *dev);
bool pci_pme_capable(struct pci_dev *dev, pci_power_t state);
int pci_find_capability(struct pci_dev *dev, int cap);
struct pci_dev *pci_get_slot(struct pci_bus *bus, unsigned int devfn);
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids, struct pci_dev *dev);
int pci_request_regions(struct pci_dev *dev, const char *res_name);
void pci_release_regions(struct pci_dev *dev);
void *pci_ioremap_bar(struct pci_dev *pdev, int bar);
void pci_disable_link_state(struct pci_dev *pdev, int state);

int pci_enable_msi(struct pci_dev *dev);
void pci_disable_msi(struct pci_dev *dev);

#define DEFINE_PCI_DEVICE_TABLE(_table) \
	const struct pci_device_id _table[] __devinitconst

#define to_pci_dev(n) container_of(n, struct pci_dev, dev)

int pci_register_driver(struct pci_driver *driver);

int pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val);

static inline void *pci_get_drvdata(struct pci_dev *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}

static inline void pci_set_drvdata(struct pci_dev *pdev, void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

#define dev_is_pci(d) (1)

int pci_num_vf(struct pci_dev *dev);

/* XXX will this cast ever work? */
#define dev_num_vf(d) ((dev_is_pci(d) ? pci_num_vf((struct pci_dev *)d) : 0))

#include <asm-generic/pci-dma-compat.h>


/**********************
 ** linux/pci-aspm.h **
 **********************/

#define PCIE_LINK_STATE_L0S   1
#define PCIE_LINK_STATE_L1    2
#define PCIE_LINK_STATE_CLKPM 4


 /******************
  ** linux/kmod.h **
  ******************/

int __request_module(bool wait, const char *name, ...);
int request_module(const char *name, ...);
#define try_then_request_module(x, mod...) \
	((x) ?: (__request_module(true, mod), (x)))


 /*****************
  ** linux/err.h **
  *****************/

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)

static inline bool IS_ERR(void const *ptr) {
	return (unsigned long)(ptr) > (unsigned long)(-1000); }

static inline void * ERR_PTR(long error) {
	return (void *) error; }

static inline void * ERR_CAST(const void *ptr) {
	return (void *) ptr; }

static inline long IS_ERR_OR_NULL(const void *ptr) {
	return !ptr || IS_ERR_VALUE((unsigned long)ptr); }

static inline long PTR_ERR(const void *ptr) { return (long) ptr; }


/*****************
** linux/uio.h **
*****************/

enum { UIO_MAXIOV = 1024 };

struct iovec
{
	void           *iov_base;
	__kernel_size_t iov_len;
};

struct kvec
{
	void  *iov_base;
	size_t iov_len;
};

static inline size_t iov_length(const struct iovec *iov, unsigned long nr_segs)
{
	unsigned long seg;
	size_t ret = 0;

	for (seg = 0; seg < nr_segs; seg++)
		ret += iov[seg].iov_len;
	return ret;
}

int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len);
int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len);


/**********************
 ** asm-generic/io.h **
 **********************/

#define mmiowb() do { } while (0)

void *ioremap(resource_size_t offset, unsigned long size);
void  iounmap(volatile void *addr);
void *devm_ioremap(struct device *dev, resource_size_t offset,
                   unsigned long size);
void *devm_ioremap_nocache(struct device *dev, resource_size_t offset,
                           unsigned long size);


/**
 * Map I/O memory write combined
 */
void *ioremap_wc(resource_size_t phys_addr, unsigned long size);

#define ioremap_nocache ioremap

void *phys_to_virt(unsigned long address);

#define writel(value, addr) (*(volatile uint32_t *)(addr) = (value))
#define writeb(value, addr) (*(volatile uint8_t *)(addr) = (value))
#define readl(addr) (*(volatile uint32_t *)(addr))
#define readb(addr) (*(volatile uint8_t  *)(addr))


/*********************
 ** linux/if_vlan.h **
 *********************/

enum {
	VLAN_HLEN       = 4,
	VLAN_PRIO_SHIFT = 13,
	VLAN_PRIO_MASK  = 0xe000,
};

struct vlan_hdr
{
	__be16 h_vlan_encapsulated_proto;
};

 struct vlan_ethhdr
{
	__be16  h_vlan_encapsulated_proto;
};

static inline struct net_device *vlan_dev_real_dev(const struct net_device *dev) { return NULL; }

#define vlan_tx_tag_get(__skb) 0
struct sk_buff *__vlan_put_tag(struct sk_buff *, u16, u16);
struct sk_buff *vlan_untag(struct sk_buff *);
int             is_vlan_dev(struct net_device *);
u16             vlan_tx_tag_present(struct sk_buff *);
bool            vlan_do_receive(struct sk_buff **);
bool            vlan_tx_nonzero_tag_present(struct sk_buff *);


/********************
 ** linux/percpu.h **
 ********************/

void *__alloc_percpu(size_t size, size_t align);

#define alloc_percpu(type)      \
	(typeof(type) __percpu *)__alloc_percpu(sizeof(type), __alignof__(type))


#define per_cpu(var, cpu) var
#define per_cpu_ptr(ptr, cpu)   ({ (void)(cpu);(typeof(*(ptr)) *)(ptr); })
#define __get_cpu_var(var) var

#define this_cpu_inc(pcp) pcp += 1
#define this_cpu_dec(pcp) pcp -= 1

#define __this_cpu_inc(pcp) this_cpu_inc(pcp)
#define __this_cpu_dec(pcp) this_cpu_dec(pcp)


/*********************
 ** linux/hrtimer.h **
 *********************/

struct hrtimer { unsigned unused; };


/*******************
 ** asm/current.h **
 *******************/

extern struct task_struct *current;


/*************************
 ** linux/res_counter.h **
 *************************/

enum { RES_USAGE };

struct res_counter { unsigned unused; };

int res_counter_charge_nofail(struct res_counter *counter, unsigned long val, struct res_counter **limit_fail_at);
u64 res_counter_uncharge(struct res_counter *counter, unsigned long val);
u64 res_counter_read_u64(struct res_counter *counter, int member);

/************************
 ** linux/memcontrol.h **
 ************************/

struct mem_cgroup;

enum { UNDER_LIMIT, SOFT_LIMIT, OVER_LIMIT };

void sock_update_memcg(struct sock *sk);


/**********************
 ** linux/mm-types.h **
 **********************/

struct page_frag
{
	struct page *page;
	__u16        offset;
	__u16        size;
};


/*******************
 ** linux/sched.h **
 *******************/

enum {
	PF_MEMALLOC = 0x800,

	MAX_SCHEDULE_TIMEOUT = 1000,
};

enum {
	TASK_RUNNING         = 0,
	TASK_INTERRUPTIBLE   = 1,
	TASK_UNINTERRUPTIBLE = 2,
	TASK_COMM_LEN        = 16,
};

struct task_struct
{
	unsigned int          flags;
	struct page_frag      task_frag;
	char                  comm[TASK_COMM_LEN];
	struct audit_context *audit_context;
};

void tsk_restore_flags(struct task_struct *task, unsigned long orig_flags, unsigned long flags);
pid_t task_pid_nr(struct task_struct *tsk);
pid_t task_tgid_vnr(struct task_struct *tsk);

void set_current_state(int);
void __set_current_state(int);

void schedule(void);
void yield(void);
int signal_pending(struct task_struct *p);
signed long schedule_timeout(signed long timeout);
bool need_resched(void);
int cond_resched(void);
int cond_resched_softirq(void);


/************************
 ** uapi/linux/sched.h **
 ************************/

enum {
	CLONE_NEWNET = 0x40000000,
};


/*********************************
 ** (uapi|linux|kernel)/audit.h **
 *********************************/

enum {
	AUDIT_ANOM_PROMISCUOUS = 1700,
};

extern int audit_enabled;


/******************
 ** linux/cred.h **
 ******************/

struct cred
{
	struct user_namespace *user_ns;
	kuid_t euid;
	kgid_t egid;
};

extern struct user_namespace init_user_ns;

static inline void current_uid_gid(kuid_t *u, kgid_t *g)
{
	*u = 0;
	*g =0;
}


extern struct user_namespace init_user_ns;
#define current_user_ns() (&init_user_ns)

void put_cred(const struct cred *_cred);


/********************
 ** net/if_inet6.h **
 ********************/

struct inet6_dev;


/*********************
 ** uapi/linux/in.h **
 *********************/

enum {
	IPPROTO_TCP = 6,
	IPPROTO_UDP = 17,
	IPPROTO_AH  = 51,
};


/**********************
 ** uapi/linux/in6.h **
 **********************/

enum {
	IPPROTO_HOPOPTS  = 0,
	IPPROTO_ROUTING  = 43,
	IPPROTO_FRAGMENT = 44,
	IPPROTO_DSTOPTS  = 60,
};

struct in6_addr
{
};


/****************
 ** net/ipv6.h **
 ****************/

enum {
	IP6_MF     = 0x0001,
	IP6_OFFSET = 0xfff8,
};


/*********************
 ** uapi/linux/ip.h **
 *********************/

enum {
	IP_OFFSET = 0x1FFF,
	IP_MF     = 0x2000,
};

struct iphdr {
	__u8    ihl:4,
	        version:4;
	__u8    tos;
	__be16  tot_len;
	__be16  frag_off;
	__u8    ttl;
	__u8    protocol;
	__sum16 check;
	__be32  saddr;
	__be32  daddr;
};

struct iphdr *ip_hdr(const struct sk_buff *skb);

struct ip_auth_hdr
{
	__u8 nexthdr;
	__u8 hdrlen;
};


/***********************
 ** uapi/linux/ipv6.h **
 ***********************/

struct ipv6hdr
{
	__be16          payload_len;
	__u8            nexthdr;
	struct in6_addr saddr;
	struct in6_addr daddr;

};

struct ipv6hdr *ipv6_hdr(const struct sk_buff *skb);

struct ipv6_opt_hdr
{
	__u8 nexthdr;
	__u8 hdrlen;
} __attribute__((packed));

struct frag_hdr
{
	__u8    nexthdr;
	/* __u8    reserved; */
	__be16  frag_off;
	/* __be32  identification; */
};


/******************
 ** linux/ipv6.h **
 ******************/

#define ipv6_optlen(p)  (((p)->hdrlen+1) << 3)
#define ipv6_authlen(p) (((p)->hdrlen+2) << 2)


/***************
 ** net/tcp.h **
 ***************/

__sum16 tcp_v4_check(int len, __be32 saddr, __be32 daddr, __wsum base);


/**********************
 ** uapi/linux/tcp.h **
 **********************/

struct tcphdr
{
	__be16  source;
	__be16  dest;
	__be32  seq;
	__be32  ack_seq;
	__u16   res1:4,
	        doff:4,
	        fin:1,
	        syn:1,
	        rst:1,
	        psh:1,
	        ack:1,
	        urg:1,
	        ece:1,
	        cwr:1;

	__be16  window;
	__sum16 check;
};

struct tcphdr *tcp_hdr(const struct sk_buff *skb);


/*****************
 ** linux/tcp.h **
 *****************/

unsigned int tcp_hdrlen(const struct sk_buff *skb);


/**********************
 ** uapi/linux/udp.h **
 **********************/

struct udphdr
{
	__sum16 check;
};

struct udphdr *udp_hdr(const struct sk_buff *skb);


/*****************
 ** linux/cpu.h **
 *****************/

enum {
	CPU_DEAD         = 0x7,
	CPU_TASKS_FROZEN = 0x10,
	CPU_DEAD_FROZEN  = CPU_DEAD | CPU_TASKS_FROZEN,
};


/*********************
 ** linux/cpumask.h **
 *********************/

extern const struct cpumask *const cpu_possible_mask;

#define nr_cpu_ids 1

#define for_each_cpu(cpu, mask)                 \
for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)

#define for_each_possible_cpu(cpu) for_each_cpu((cpu), cpu_possible_mask)
#define hotcpu_notifier(fn, pri)


/************************
 ** linux/jump_label.h **
 ************************/

struct static_key { unsigned unused; };

#define STATIC_KEY_INIT_FALSE ((struct static_key) {})

bool static_key_false(struct static_key *key);
void static_key_slow_inc(struct static_key *key);
void static_key_slow_dec(struct static_key *key);


/***********************
 ** linux/pipe_fs_i.h **
 ***********************/

struct pipe_buffer
{
	struct page *page;
};

struct pipe_inode_info;

struct pipe_buf_operations
{
	int can_merge;
	void * (*map)(struct pipe_inode_info *, struct pipe_buffer *, int);
	void (*unmap)(struct pipe_inode_info *, struct pipe_buffer *, void *);
	int (*confirm)(struct pipe_inode_info *, struct pipe_buffer *);
	void (*release)(struct pipe_inode_info *, struct pipe_buffer *);
	int (*steal)(struct pipe_inode_info *, struct pipe_buffer *);
	void (*get)(struct pipe_inode_info *, struct pipe_buffer *);
};

void *generic_pipe_buf_map(struct pipe_inode_info *, struct pipe_buffer *, int);
void  generic_pipe_buf_unmap(struct pipe_inode_info *, struct pipe_buffer *, void *);
int   generic_pipe_buf_confirm(struct pipe_inode_info *, struct pipe_buffer *);


/********************
 ** linux/splice.h **
 ********************/

struct partial_page
{
unsigned int offset;
unsigned int len;
};

struct splice_pipe_desc
{
	struct page        **pages;
	struct partial_page *partial;
	int                  nr_pages;
	unsigned int         nr_pages_max;
	unsigned int         flags;
	const struct pipe_buf_operations *ops;
	void (*spd_release)(struct splice_pipe_desc *, unsigned int);
};

ssize_t splice_to_pipe(struct pipe_inode_info *, struct splice_pipe_desc *);


/************************
 ** linux/textsearch.h **
 ************************/

struct ts_state
{
	char cb[40];
};

struct ts_config
{
	unsigned int  (*get_next_block)(unsigned int consumed,
			const u8 **dst,
			struct ts_config *conf,
			struct ts_state *state);
	void          (*finish)(struct ts_config *conf,
			struct ts_state *state);
};

unsigned int textsearch_find(struct ts_config *, struct ts_state *);


/*****************
 ** linux/aio.h **
 *****************/

struct kiocb
{
	void *private;
};


/*************************
 ** uapi/linux/filter.h **
 *************************/

struct sock_filter {    /* Filter block */
	__u16   code;   /* Actual filter code */
	__u8    jt; /* Jump true */
	__u8    jf; /* Jump false */
	__u32   k;      /* Generic multiuse field */
};


/********************
 ** linux/filter.h **
 ********************/

struct sk_buff;
struct sock_filter;
struct sock_fprog;
struct sk_filter
{
	atomic_t        refcnt;
	unsigned int    len;
	struct rcu_head rcu;

	struct sock_filter      insns[0];
};

unsigned int sk_filter_size(unsigned int proglen);
int sk_filter(struct sock *, struct sk_buff *);
unsigned int sk_run_filter(const struct sk_buff *skb,
                           const struct sock_filter *filter);
int sk_attach_filter(struct sock_fprog *, struct sock *);
int sk_detach_filter(struct sock *);
int sk_get_filter(struct sock *, struct sock_filter *, unsigned);

#define SK_RUN_FILTER(FILTER, SKB) sk_run_filter(SKB, FILTER->insns)

int bpf_tell_extensions(void);


/**************************
 ** linux/seq_file_net.h **
 **************************/

struct seq_net_private {
	struct net *net;
};

struct seq_operations { unsigned unused; };


/**************************
 ** linux/seq_file.h **
 **************************/

struct seq_file { unsigned unused; };

int seq_printf(struct seq_file *, const char *, ...);


/********************
 ** linux/sysctl.h **
 ********************/

struct ctl_table;

typedef int proc_handler (struct ctl_table *ctl, int write,
                          void __user *buffer, size_t *lenp, loff_t *ppos);


/*****************
 ** linux/pid.h **
 *****************/

struct pid;

pid_t pid_vnr(struct pid *pid);
void put_pid(struct pid *pid);


/***************************
 ** asm-generic/uaccess.h **
 ***************************/

enum { VERIFY_READ = 0 };

#define get_user(x, ptr) ({  x   = *ptr; 0; })
#define put_user(x, ptr) ({ *ptr =  x;   0; })

static inline long copy_from_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}

static inline long copy_to_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}

#define access_ok(type, addr, size) __access_ok((unsigned long)(addr),(size))

int __access_ok(unsigned long addr, unsigned long size);


/*********************
 ** linux/uaccess.h **
 *********************/

static inline unsigned long __copy_from_user_nocache(void *to, const void __user *from, unsigned long n)
{
	return copy_from_user(to, from, n);
}


/*******************************
 ** asm-generic/scatterlist.h **
 *******************************/

struct scatterlist
{
	unsigned long page_link;
	unsigned int  offset;
	unsigned int  length;
};

void sg_set_page(struct scatterlist *sg, struct page *page, unsigned int len, unsigned int offset);
void sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen);
void sg_mark_end(struct scatterlist *sg);
struct scatterlist *sg_next(struct scatterlist *);
void sg_init_table(struct scatterlist *, unsigned int);
void sg_init_one(struct scatterlist *, const void *, unsigned int);

#define sg_is_last(sg)   ((sg)->page_link & 0x02)
#define sg_chain_ptr(sg) ((struct scatterlist *) ((sg)->page_link & ~0x03))

static inline struct page *sg_page(struct scatterlist *sg) {
	return (struct page *)((sg)->page_link & ~0x3); }


/**************
 ** net/ip.h **
 **************/

enum { IP_DEFRAG_AF_PACKET }; /* XXX original value is not 0 */

struct inet_skb_parm { unsigned unused; };

unsigned int ip_hdrlen(const struct sk_buff *skb);
struct sk_buff *ip_check_defrag(struct sk_buff *skb, u32 user);


/********************
 ** linux/dcache.h **
 ********************/

unsigned int full_name_hash(const unsigned char *, unsigned int);


/******************
 ** linux/hash.h **
 ******************/

u32 hash_32(u32 val, unsigned int);


/*******************************
 ** asm-generic/bitops/find.h **
 *******************************/

unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset);

#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)
#define find_first_zero_bit(addr, size) find_next_zero_bit((addr), (size), 0)


/********************
 ** net/checksum.h **
 ********************/

#define CSUM_MANGLED_0 ((__sum16)0xffff)

__wsum csum_and_copy_from_user(const void __user *src, void *dst,
                               int len, __wsum sum, int *err_ptr);
__wsum csum_add(__wsum csum, __wsum addend);
__wsum csum_block_add(__wsum csum, __wsum csum2, int offset);
__wsum csum_block_sub(__wsum, __wsum, int);
__wsum csum_sub(__wsum csum, __wsum addend);
__wsum csum_unfold(__sum16 n);

__wsum csum_partial_ext(const void *buff, int len, __wsum sum);
__wsum csum_block_add_ext(__wsum csum, __wsum csum2, int offset, int len);

void csum_replace2(__sum16 *, __be16, __be16);


/************************
 ** net/ip6_checksum.h **
 ************************/

__sum16 csum_ipv6_magic(const struct in6_addr *saddr, const struct in6_addr *daddr, __u32 len, unsigned short proto, __wsum csum);


/****************************
 ** asm-generic/checksum.h **
 ****************************/

__sum16 csum_fold(__wsum csum);
__sum16 ip_fast_csum(const void *iph, unsigned int ihl);

__wsum csum_partial(const void *buff, int len, __wsum sum);
__wsum csum_partial_copy(const void *src, void *dst, int len, __wsum sum);
__wsum csum_partial_copy_from_user(const void __user *src, void *dst, int len,
                                   __wsum sum, int *csum_err);

#define csum_partial_copy_nocheck(src, dst, len, sum)   \
	csum_partial_copy((src), (dst), (len), (sum))

#define csum_and_copy_from_user csum_partial_copy_from_user

__wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr, unsigned short len,
                          unsigned short proto, __wsum sum);
static inline
__sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr, unsigned short len,
                          unsigned short proto, __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}

static inline
__wsum csum_and_copy_to_user(const void *src, void __user *dst, int len,
                             __wsum sum, int *err_ptr)
{
	sum = csum_partial(src, len, sum);

	memcpy(dst, src, len);
	return sum;
}


/*******************
 ** linux/delay.h **
 *******************/

void msleep(unsigned int);
void ssleep(unsigned int);
void usleep_range(unsigned long min, unsigned long max);


/*****************
 ** linux/smp.h **
 *****************/

#define smp_processor_id() 0
#define raw_smp_processor_id() smp_processor_id()
#define put_cpu()

typedef void (*smp_call_func_t)(void *info);

int on_each_cpu(smp_call_func_t, void *, int);


/****************************
 ** uapi/linux/genetlink.h **
 ****************************/

/* #define GENL_NAMSIZ      16 */
/* #define GENL_ID_GENERATE 0 */
/* #define GENL_ADMIN_PERM  0x01 */

/* struct genlmsghdr { */
/* 	u8 cmd; */
/* 	u8 version; */
/* 	u16 reserved; */
/* }; */

/* #define GENL_HDRLEN   NLMSG_ALIGN(sizeof(struct genlmsghdr)) */

/* */
/* struct nlattr { */
/* 	u16 nla_len; */
/* 	u16 nla_type; */
/* }; */
/* */

/* #define NLM_F_MULTI             2 */

/* */
/* struct nlmsghdr { */
/* 	u32       nlmsg_len; */
/* 	u16       nlmsg_type; */
/* 	u16       nlmsg_flags; */
/* 	u32       nlmsg_seq; */
/* 	u32       nlmsg_pid; */
/* }; */
/* */

/* #define NLMSG_HDRLEN     ((int) NLMSG_ALIGN(sizeof(struct nlmsghdr))) */

/* #define NLMSG_ALIGNTO   4U */
/* #define NLMSG_ALIGN(len) ( ((len)+NLMSG_ALIGNTO-1) & ~(NLMSG_ALIGNTO-1) ) */

/**************************
 ** uapi/linux/netlink.h **
 **************************/

#define NLA_ALIGNTO     4
#define NLA_ALIGN(len)      (((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))


/*******************
 ** net/netlink.h **
 *******************/

/*
enum {
	NLA_U8,
	NLA_U16,
	NLA_U32,
	NLA_U64,
	NLA_STRING,
	NLA_FLAG,
	NLA_NESTED,
	NLA_NUL_STRING,
	NLA_BINARY,
};

struct nla_policy {
	u16 type;
	u16 len;
};


static inline int nla_ok(const struct nlattr *nla, int remaining)
{
	return remaining >= (int) sizeof(*nla) &&
	       nla->nla_len >= sizeof(*nla) &&
	       nla->nla_len <= remaining;
}

static inline struct nlattr *nla_next(const struct nlattr *nla, int *remaining)
{
	int totlen = NLA_ALIGN(nla->nla_len);
	*remaining -= totlen;
	return (struct nlattr *) ((char *) nla + totlen);
}

#define nla_for_each_attr(pos, head, len, rem) \
	for (pos = head, rem = len; \
	nla_ok(pos, rem); \
	pos = nla_next(pos, &(rem)))

#define nla_for_each_nested(pos, nla, rem) \
	nla_for_each_attr(pos, nla_data(nla), nla_len(nla), rem)
*/


/***********************
 ** linux/genetlink.h **
 ***********************/

/* struct genl_info { */
/* 	u32 snd_seq; */
/* 	u32 snd_portid; */
/* 	struct genlmsghdr *genlhdr; */
/* 	struct nlattr **attrs; */
/* 	void *user_ptr[2]; */
/* }; */

/* struct netlink_callback; */

/* struct genl_ops { */
/* 	u8          cmd; */
/* 	u8          internal_flags; */
/* 	unsigned int        flags; */
/* 	const struct nla_policy *policy; */
/* 	int            (*doit)(struct sk_buff *skb, */
/* 			struct genl_info *info); */
/* 	int            (*dumpit)(struct sk_buff *skb, */
/* 			struct netlink_callback *cb); */
/* 	int            (*done)(struct netlink_callback *cb); */
/* 	struct list_head    ops_list; */
/* }; */

/* struct genl_multicast_group { */
/* 	char name[GENL_NAMSIZ]; */
/* 	u32 id; */
/* }; */

/* struct genl_family { */
/* 	unsigned int        id; */
/* 	unsigned int        hdrsize; */
/* 	char            name[GENL_NAMSIZ]; */
/* 	unsigned int        version; */
/* 	unsigned int        maxattr; */
/* 	bool            netnsok; */
/* 	int         (*pre_doit)(struct genl_ops *ops, */
/* 	                        struct sk_buff *skb, */
/* 	                        struct genl_info *info); */
/* 	void            (*post_doit)(struct genl_ops *ops, */
/* 	                             struct sk_buff *skb, */
/* 	                             struct genl_info *info); */
/* 	struct nlattr **    attrbuf;    /1* private *1/ */
/* 	struct list_head    ops_list;   /1* private *1/ */
/* 	struct list_head    family_list;    /1* private *1/ */
/* 	struct list_head    mcast_groups;   /1* private *1/ */
/* }; */

#define MODULE_ALIAS_GENL_FAMILY(family) \
	MODULE_ALIAS_NET_PF_PROTO_NAME(PF_NETLINK, NETLINK_GENERIC, "-family-" family)



/*****************************
 ** uapi/linux/net_tstamp.h **
 *****************************/

enum {
	SOF_TIMESTAMPING_TX_HARDWARE  = 1 << 0,
	SOF_TIMESTAMPING_TX_SOFTWARE  = 1 << 1,
	SOF_TIMESTAMPING_RX_HARDWARE  = 1 << 2,
	SOF_TIMESTAMPING_RX_SOFTWARE  = 1 << 3,
	SOF_TIMESTAMPING_SOFTWARE     = 1 << 4,
	SOF_TIMESTAMPING_SYS_HARDWARE =  1 << 5,
	SOF_TIMESTAMPING_RAW_HARDWARE =  1 << 6,
	SOF_TIMESTAMPING_MASK = (SOF_TIMESTAMPING_RAW_HARDWARE - 1) |
	                         SOF_TIMESTAMPING_RAW_HARDWARE,
};


/*************************
 ** uapi/linux/filter.h **
 *************************/

struct sock_fprog { unsigned unused; };


/* short-cut for net/core/sock.c */
#include <net/tcp_states.h>


/*****************************
 ** uapi/asm-generic/poll.h **
 *****************************/

enum {
	POLLIN     = 0x1,
	POLLPRI    = 0x2,
	POLLOUT    = 0x4,
	POLLERR    = 0x8,
	POLLHUP    = 0x10,
	POLLRDNORM = 0x40,
	POLLRDBAND = 0x80,
	POLLWRNORM = 0x100,
	POLLWRBAND = 0x200,
	POLLRDHUP  = 0x2000,
};

void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p);
bool poll_does_not_wait(const poll_table *p);


/*********************************
 ** uapi/asm-generic/siginfo..h **
 *********************************/

enum
{
	POLL_IN  = 1,
	POLL_OUT = 2,
	POLL_ERR = 4,
	POLL_PRI = 5,
	POLL_HUP = 6,
};


size_t ksize(void *);
void * krealloc(void *, size_t, gfp_t);


/*********************
 ** net/flow_keys.h **
 *********************/

struct flow_keys
{
	/* (src,dst) must be grouped, in the same way than in IP header */
	__be32 src;
	__be32 dst;
	union {
		__be32 ports;
		__be16 port16[2];
	};
	u16 thoff;
	u8 ip_proto;
};

bool skb_flow_dissect(const struct sk_buff *skb, struct flow_keys *flow);
__be32 skb_flow_get_ports(const struct sk_buff *skb, int thoff, u8 ip_proto);


/***********************
 ** linux/pipe_fs_i.h **
 ***********************/

extern const struct pipe_buf_operations nosteal_pipe_buf_ops;


/******************
 ** linux/acpi.h **
 ******************/

#define ACPI_PTR(_ptr)   (NULL)
#define ACPI_HANDLE(dev) (NULL)

const struct acpi_device_id *acpi_match_device(const struct acpi_device_id *ids, const struct device *dev);


/********************
 ** linux/random.h **
 ********************/

static inline void get_random_bytes(void *buf, int nbytes)
{
	char *b = (char *)buf;

	/* FIXME not random */
	int i;
	for (i = 0; i < nbytes; ++i)
		b[i] = i + 1;
}

u32 prandom_u32(void);
static inline u32 prandom_u32_max(u32 ep_ro) { return (u32)(((u64) prandom_u32() * ep_ro) >> 32); }


/*********************
 ** linux/proc_fs.h **
 *********************/

#define remove_proc_entry(name, parent) do { } while (0)


/*********************
 ** linux/proc_ns.h **
 *********************/

struct nsproxy;

struct proc_ns_operations
{
	const char *name;
	int type;
	void *(*get)(struct task_struct *task);
	void (*put)(void *ns);
	int (*install)(struct nsproxy *nsproxy, void *ns);
	unsigned int (*inum)(void *ns);
};


struct proc_ns
{
	void *ns;
	const struct proc_ns_operations *ns_ops;
};

extern const struct proc_ns_operations netns_operations;


/*********************
 ** linux/nsproxy.h **
 *********************/

struct nsproxy
{
	struct net *net_ns;
};


/********************
 ** linux/bitmap.h **
 ********************/

static inline void bitmap_zero(unsigned long *dst, int nbits)
{
	if (nbits <= BITS_PER_LONG)
		*dst = 0UL;
	else {
		int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memset(dst, 0, len);
	}
}

void bitmap_fill(unsigned long *dst, int nbits);
int bitmap_empty(const unsigned long *src, int nbits);


/*******************************
 ** uapi/asm-generic/ioctls.h **
 *******************************/

enum {
	TIOCOUTQ = 0x5411,
	FIONREAD = 0x541B,
};


/********************************
 ** uapi/asm-generic/sockios.h **
 ********************************/

enum {
	SIOCGSTAMP   = 0x8906,
	SIOCGSTAMPNS = 0x8907,
};


/*************************
 ** linux/sch_generic.h **
 *************************/

struct Qdisc_ops
{
	char id[IFNAMSIZ];
};

struct Qdisc
{
	const struct Qdisc_ops *ops;
};

bool qdisc_all_tx_empty(const struct net_device *dev);


/*********************
 ** linux/hardirq.h **
 *********************/

void synchronize_irq(unsigned int irq);


/**************************
 ** asm-generic/udelay.h **
 **************************/

void udelay(unsigned long usecs);


/****************************
 ** asm-generic/getorder.h **
 ****************************/

int get_order(unsigned long size);


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


/***************************
 ** linux/rculist_nulls.h **
 ***************************/

struct hlist_nulls_node;
struct hlist_nulls_head;

void hlist_nulls_add_head_rcu(struct hlist_nulls_node *n, struct hlist_nulls_head *h);
void hlist_nulls_del_init_rcu(struct hlist_nulls_node *n);


/**********************
 ** linux/security.h **
 **********************/

struct socket;

void security_sock_graft(struct sock* sk, struct socket *parent);
int security_socket_getpeersec_stream(struct socket *sock, char __user *optval, int __user *optlen, unsigned len);
int security_sk_alloc(struct sock *sk, int family, gfp_t priority);
void security_sk_free(struct sock *sk);
int security_netlink_send(struct sock *sk, struct sk_buff *skb);


/*********************
 ** linux/pagemap.h **
 *********************/

void release_pages(struct page **pages, int nr, int cold);


/*********************
 ** net/busy_poll.h **
 *********************/

bool sk_can_busy_loop(struct sock *sk);
bool sk_busy_loop(struct sock *sk, int nonblock);


/**********************
 ** linux/prefetch.h **
 **********************/

#define prefetch(x)  __builtin_prefetch(x)
#define prefetchw(x) __builtin_prefetch(x,1)


/****************
 ** net/xfrm.h **
 ****************/

static inline void secpath_reset(struct sk_buff *skb) { }
int __xfrm_sk_clone_policy(struct sock *sk);
void xfrm_sk_free_policy(struct sock *sk);
int xfrm_sk_clone_policy(struct sock *sk);


/*************************
 ** linux/debug_locks.h **
 *************************/

static inline void debug_check_no_locks_freed(const void *from, unsigned long len) { }


/**********************
 ** net/cls_cgroup.h **
 **********************/

void sock_update_classid(struct sock *sk);


/**************************
 ** net/netprio_cgroup.h **
 **************************/

void sock_update_netprioidx(struct sock *sk);


/*******************
 ** linux/crc32.h **
 *******************/

#define CONFIG_CRC32_SLICEBY8 /* the default from lib/Kconfig */

extern u32  crc32_le(u32 crc, unsigned char const *p, size_t len);
extern u32  crc32_be(u32 crc, unsigned char const *p, size_t len);


/********************************
 ** linux/regulator/consumer.h **
 ********************************/

struct regulator;

int regulator_enable(struct regulator *regulator);
int regulator_disable(struct regulator *regulator);
int regulator_is_enabled(struct regulator *regulator);
struct regulator * regulator_get_exclusive(struct device *dev, const char *id);
void regulator_put(struct regulator *regulator);


/***************************
 ** linux/gpio/consumer.h **
 ***************************/

struct gpio_desc;

struct gpio_desc * devm_gpiod_get_index(struct device *dev, const char *con_id, unsigned int idx);
int gpiod_direction_output(struct gpio_desc *desc, int value);
void gpiod_set_value(struct gpio_desc *desc, int value);


/*****************
 ** linux/clk.h **
 *****************/

struct clk;

struct clk *devm_clk_get(struct device *dev, const char *id);
int clk_enable(struct clk *clk);
void clk_disable(struct clk *clk);


/***************************
 ** uapi/linux/wireless.h **
 ***************************/

struct iw_freq;
struct iw_point;


/**********************
 ** net/iw_handler.h **
 **********************/

struct iw_request_info;


/*********************
 ** linux/debugfs.h **
 *********************/

struct dentry *debugfs_rename(struct dentry *old_dir, struct dentry *old_dentry, struct dentry *new_dir, const char *new_name);
struct dentry *debugfs_create_dir(const char *name, struct dentry *parent);
void debugfs_remove(struct dentry *dentry);
void debugfs_remove_recursive(struct dentry *dentry);


/*********************
 ** linux/kthread.h **
 *********************/

void *kthread_run(int (*threadfn)(void *), void *data, char const *name);


/*****************
 ** crypto test **
 *****************/

int alg_test(const char *driver, const char *alg, u32 type, u32 mask);

#endif /* _LX_EMUL_H_ */
