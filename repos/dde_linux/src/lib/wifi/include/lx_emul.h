/*
 * \brief  Emulation of the Linux kernel API
 * \author Josef Soentgen
 * \date   2014-03-03
 *
 * The content of this file, in particular data structures, is partially
 * derived from Linux-internal headers.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL_H_
#define _LX_EMUL_H_

#include <stdarg.h>
#include <base/fixed_stdint.h>

#include <lx_emul/extern_c_begin.h>

#define KBUILD_MODNAME "mod-noname"

/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100 };

#define DEBUG_LINUX_PRINTK 1

#include <lx_emul/printf.h>


/***************
 ** asm/bug.h **
 ***************/

#include <lx_emul/bug.h>


/*********************
 ** asm/processor.h **
 *********************/

void cpu_relax(void);


/******************
 ** asm/atomic.h **
 ******************/

#include <lx_emul/atomic.h>

// static inline int atomic_dec_if_positive(atomic_t *v)
// {
// 	int c, old, dec;
// 	c = atomic_read(v);
// 	for (;;) {
// 		dec = c - 1;
// 		if (dec < 0) break;
// 		old = atomic_cmpxchg((v), c, dec);
// 		if (old == c) break;
// 		c = old;
// 	}
// 	return dec;
// }

static inline void atomic_long_set(atomic_long_t *l, long i)
{
	l->counter = i;
}


static inline int atomic_long_cmpxchg(atomic_long_t *v, long old, long n)
{
	return cmpxchg(&v->counter, old, n);
}




/*******************
 ** linux/types.h **
 *******************/

#include <lx_emul/types.h>

typedef int clockid_t;

typedef size_t __kernel_size_t;
typedef long   __kernel_time_t;
typedef long   __kernel_suseconds_t;

#define __aligned_u64 __u64 __attribute__((aligned(8)))

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]

typedef unsigned __poll_t;


/******************
 ** linux/init.h **
 ******************/

#define fs_initcall(fn)   void fs_##fn(void) { fn(); }
#define late_initcall(fn) void late_##fn(void) { fn(); }


/*********************
 ** linux/cpumask.h **
 *********************/

#define NR_CPUS 1U
#define cpu_online_mask (NULL)
#define num_online_cpus() NR_CPUS
typedef struct cpumask { unsigned long bits[1]; } cpumask_t;

static inline void cpumask_set_cpu(unsigned int cpu, struct cpumask *dstp)
{
	dstp->bits[0] = 1U << cpu;
}

static inline unsigned int cpumask_next(int n, const struct cpumask *srcp)
{
	return n+1;
}


/************************
 ** uapi/linux/types.h **
 ************************/

#define __bitwise__

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u16 __be16;
typedef __u32 __be32;
typedef __u64 __be64;

typedef __u16 __sum16;
typedef __u32 __wsum;

/*
 * needed by include/net/cfg80211.h
 */
struct callback_head {
	struct callback_head *next;
	void (*func)(struct callback_head *head);
};
#define rcu_head callback_head


/*******************
 ** asm/barrier.h **
 *******************/

#include <lx_emul/barrier.h>

#define smp_load_acquire(p)     *(p)
#define smp_store_release(p, v) *(p) = v;
#define smp_mb__before_atomic() mb()


/**********************
 ** asm-generic/io.h **
 **********************/

#include <lx_emul/mmio.h>

#define mmiowb() barrier()
struct device;

void *ioremap(resource_size_t offset, unsigned long size);
void  iounmap(volatile void *addr);
void *devm_ioremap(struct device *dev, resource_size_t offset,
                   unsigned long size);
void *devm_ioremap_nocache(struct device *dev, resource_size_t offset,
                           unsigned long size);

void *ioremap_wc(resource_size_t phys_addr, unsigned long size);

#define ioremap_nocache ioremap

void *phys_to_virt(unsigned long address);


/**********************
 ** linux/compiler.h **
 **********************/

#include <lx_emul/compiler.h>

#define __cond_lock(x,c) (c)

#define noinline_for_stack noinline

static inline void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile __u8  *)p = *(__u8  *)res; break;
	case 2: *(volatile __u16 *)p = *(__u16 *)res; break;
	case 4: *(volatile __u32 *)p = *(__u32 *)res; break;
	case 8: *(volatile __u64 *)p = *(__u64 *)res; break;
	default:
		barrier();
		__builtin_memcpy((void *)p, (const void *)res, size);
		barrier();
	}
}

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
#endif /* __cplusplus */

#define __randomize_layout


/**************************
 ** linux/compiler-gcc.h **
 **************************/

#ifdef __aligned
#undef __aligned
#endif
#define __aligned(x)  __attribute__((aligned(x)))
#define __visible     __attribute__((externally_visible))

#define OPTIMIZER_HIDE_VAR(var) __asm__ ("" : "=r" (var) : "0" (var))

#define __pure            __attribute__((pure))
#define __weak            __attribute__((weak))
#define __aligned_largest __attribute__((aligned))


/**********************
 ** linux/refcount.h **
 **********************/

typedef struct
{
	atomic_t refs;
} refcount_t;


void refcount_add(unsigned int i, refcount_t *r);
void refcount_inc(refcount_t *r);
bool refcount_inc_not_zero(refcount_t *r);
void refcount_dec(refcount_t *r);
unsigned int refcount_read(const refcount_t *r);
bool refcount_dec_and_test(refcount_t *r);
void refcount_set(refcount_t *r, unsigned int n);
bool refcount_dec_and_test(refcount_t *r);
bool refcount_sub_and_test(unsigned int i, refcount_t *r);


/********************
 ** linux/module.h **
 ********************/

#include <lx_emul/module.h>

static inline bool module_sig_ok(struct module *module) { return true; }

#define module_name(mod) "foobar"


/*************************
 ** linux/moduleparam.h **
 *************************/

#define __MODULE_INFO(tag, name, info)

static inline void kernel_param_lock(struct module *mod) { }
static inline void kernel_param_unlock(struct module *mod) { }


/*******************
 ** linux/errno.h **
 *******************/

#include <lx_emul/errno.h>


/*****************
 ** linux/err.h **
 *****************/

static inline int PTR_ERR_OR_ZERO(const void *ptr)
{
	if (IS_ERR(ptr)) return PTR_ERR(ptr);
	else             return 0;
}


/********************
 ** linux/poison.h **
 ********************/

#include <lx_emul/list.h>


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


/**********************
 ** linux/page_ref.h **
 **********************/

static inline void page_ref_inc(struct page *page)
{
	atomic_inc(&page->_count);
}

static inline int page_ref_count(struct page *page)
{
	return atomic_read(&page->_count);
}


/************************
 ** linux/page-flags.h **
 ************************/

enum pageflags
{
	PG_slab = 0x1,
};

#define PageSlab(page) test_bit(PG_slab, &(page)->flags)
#define PageHighMem(__p) 0


/**********************
 ** asm/cacheflush.h **
 **********************/

void flush_dcache_page(struct page *page);
enum { ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE = 1 }; /* XXX */


/**********************
 ** linux/mm-types.h **
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

struct page_frag
{
	struct page *page;
	__u16        offset;
	__u16        size;
};

struct page_frag_cache
{
	bool pfmemalloc;
};


/*****************
 ** linux/gfp.h **
 *****************/

#include <lx_emul/gfp.h>

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
void __free_page_frag(void *addr);

#define __free_page(page) __free_pages((page), 0)

bool gfpflags_allow_blocking(const gfp_t gfp_flags);

void *__alloc_page_frag(struct page_frag_cache *nc,
                        unsigned int fragsz, gfp_t gfp_mask);

void page_frag_free(void *addr);
void *page_frag_alloc(struct page_frag_cache *nc, unsigned int fragsz, gfp_t gfp_mask);

/********************
 ** linux/string.h **
 ********************/

#include <lx_emul/string.h>

void memzero_explicit(void *s, size_t count);

bool sysfs_streq(const char *s1, const char *s2);


/**********************
 ** linux/spinlock.h **
 **********************/

#include <lx_emul/spinlock.h>

int spin_is_locked(spinlock_t *lock);
void free_bucket_spinlocks(spinlock_t *locks);
int alloc_bucket_spinlocks(spinlock_t **locks, unsigned int *locks_mask,
                           size_t max_size, unsigned int cpu_mult, gfp_t gfp);


/*******************
 ** linux/mutex.h **
 *******************/

#include <lx_emul/mutex.h>

LX_MUTEX_INIT_DECLARE(crypto_default_rng_lock);
LX_MUTEX_INIT_DECLARE(fanout_mutex);
LX_MUTEX_INIT_DECLARE(genl_mutex);
LX_MUTEX_INIT_DECLARE(proto_list_mutex);
LX_MUTEX_INIT_DECLARE(rate_ctrl_mutex);
LX_MUTEX_INIT_DECLARE(reg_regdb_apply_mutex);
LX_MUTEX_INIT_DECLARE(rfkill_global_mutex);
LX_MUTEX_INIT_DECLARE(rtnl_mutex);

#define crypto_default_rng_lock LX_MUTEX(crypto_default_rng_lock)
#define fanout_mutex            LX_MUTEX(fanout_mutex)
#define genl_mutex              LX_MUTEX(genl_mutex)
#define proto_list_mutex        LX_MUTEX(proto_list_mutex)
#define rate_ctrl_mutex         LX_MUTEX(rate_ctrl_mutex)
#define reg_regdb_apply_mutex   LX_MUTEX(reg_regdb_apply_mutex)
#define rfkill_global_mutex     LX_MUTEX(rfkill_global_mutex)
#define rtnl_mutex              LX_MUTEX(rtnl_mutex)


/*******************
 ** linux/rwsem.h **
 *******************/

#include <lx_emul/semaphore.h>


/********************
 ** linux/kernel.h **
 ********************/

#include <lx_emul/kernel.h>

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

char *kasprintf(gfp_t gfp, const char *fmt, ...);
int kstrtouint(const char *s, unsigned int base, unsigned int *res);

#define PTR_ALIGN(p, a)     ((typeof(p))ALIGN((unsigned long)(p), (a)))

static inline u32 reciprocal_scale(u32 val, u32 ep_ro)
{
	return (u32)(((u64) val * ep_ro) >> 32);
}

int kstrtoul(const char *s, unsigned int base, unsigned long *res);

int strict_strtoul(const char *s, unsigned int base, unsigned long *res);
long simple_strtoul(const char *cp, char **endp, unsigned int base);
long simple_strtol(const char *,char **,unsigned int);

int hex_to_bin(char ch);

/* needed by drivers/net/wireless/iwlwifi/iwl-drv.c */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) __attribute__((format(printf, 3, 0)));
int sprintf(char *buf, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int scnprintf(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

int sscanf(const char *, const char *, ...);

/* XXX */
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)
#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)

#define SIZE_MAX (~(size_t)0)

#define U8_MAX   ((u8)~0U)
#define S8_MAX   ((s8)(U8_MAX>>1))
#define S8_MIN   ((s8)(-S8_MAX - 1))
#define U16_MAX  ((u16)~0U)
#define U32_MAX  ((u32)~0U)
#define S32_MAX  ((s32)(U32_MAX>>1))
#define S32_MIN  ((s32)(-S32_MAX - 1))

#define sched_annotate_sleep() do { } while (0)


/*********************
 ** linux/jiffies.h **
 *********************/

#include <lx_emul/jiffies.h>

static inline unsigned int jiffies_to_usecs(const unsigned long j) { return j * JIFFIES_TICK_US; }

#define time_is_before_jiffies(a) time_after(jiffies, a)


/******************
 ** linux/time.h **
 ******************/

#include <lx_emul/time.h>

enum {
	MSEC_PER_SEC  = 1000L,
	USEC_PER_SEC  = MSEC_PER_SEC * 1000L,
	USEC_PER_MSEC = 1000L,
};

unsigned long get_seconds(void);
void          getnstimeofday(struct timespec *);
#define do_posix_clock_monotonic_gettime(ts) ktime_get_ts(ts)

#define ktime_to_ns(kt) ((kt)

struct timespec ktime_to_timespec(const ktime_t kt);
bool ktime_to_timespec_cond(const ktime_t kt, struct timespec *ts);

int     ktime_equal(const ktime_t, const ktime_t);
s64     ktime_us_delta(const ktime_t, const ktime_t);

static inline s64 ktime_to_ms(const ktime_t kt)
{
	return kt / NSEC_PER_MSEC;
}

static inline void ktime_get_ts(struct timespec *ts)
{
	ts->tv_sec  = jiffies * (1000/HZ);
	ts->tv_nsec = 0;
}


/*******************
 ** linux/timer.h **
 *******************/

#include <lx_emul/timer.h>

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

enum {
	TIMER_DEFERRABLE = 0x00080000,
};


/*********************
 ** linux/kconfig.h **
 *********************/

#define config_enabled(cfg) 0


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

#include <lx_emul/byteorder.h>

#define cpu_to_be64  __cpu_to_be64
#define be64_to_cpup __be64_to_cpup

#define htonl(x) __cpu_to_be32(x)
#define htons(x) __cpu_to_be16(x)
#define ntohl(x) __be32_to_cpu(x)
#define ntohs(x) __be16_to_cpu(x)


/*************************************
 ** linux/unaligned/packed_struct.h **
 *************************************/

struct __una_u16 { u16 x; } __attribute__((packed));
struct __una_u32 { u32 x; } __attribute__((packed));
struct __una_u64 { u64 x; } __attribute__((packed));


/*******************************
 ** linux/unaligned/generic.h **
 *******************************/

static inline void put_unaligned_le16(u16 val, void *p) {
	*((__le16 *)p) = cpu_to_le16(val); }

static inline void put_unaligned_be16(u16 val, void *p) {
	*((__be16 *)p) = cpu_to_be16(val); }

static inline void put_unaligned_le32(u32 val, void *p) {
	*((__le32 *)p) = cpu_to_le32(val); }

static inline void put_unaligned_be64(u64 val, void *p) {
	*((__be64 *)p) = cpu_to_be64(val); }

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

static inline u16 get_unaligned_be16(const void *p)
{
	const __u8 *be = (__u8*)p;
	return (be[1]<<0)|(be[0]<<8);
}

static inline u64 get_unaligned_be64(const void *p)
{
	const __u64 *be = (__u64*)p;
	return ((((u64)(be[3]<<0)|(be[2]<<8)|(be[1]<<16)|(be[0]<<24))<<32)|
	        (((u32)(be[7]<<0)|(be[6]<<8)|(be[5]<<16)|(be[4]<<24))));
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

static inline void le16_add_cpu(__le16 *var, u16 val) {
	*var = cpu_to_le16(le16_to_cpu(*var) + val); }

static inline void le32_add_cpu(__le32 *var, u32 val) {
	*var = cpu_to_le32(le32_to_cpu(*var) + val); }

static inline u32 __get_unaligned_cpu32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;
	return ptr->x;
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


/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

#include <lx_emul/bitops.h>

static inline unsigned long hweight_long(unsigned long w) {
	return sizeof(w) == 4 ? hweight32(w) : hweight64(w); }

#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))


/*************************************
 ** asm-generic/bitops/non-atomic.h **
 *************************************/

static inline int test_bit(int nr, const volatile unsigned long *addr);


/*******************************
 ** asm-generic/bitops/find.h **
 *******************************/

unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
unsigned long find_last_bit(const unsigned long *addr, unsigned long size);

#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)
#define find_first_zero_bit(addr, size) find_next_zero_bit((addr), (size), 0)


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

bool page_is_pfmemalloc(struct page *page);

#define PAGE_ALIGNED(addr) IS_ALIGNED((unsigned long)addr, PAGE_SIZE)


/**********************
 ** linux/sched/mm.h **
 **********************/

void memalloc_noreclaim_restore(unsigned int flags);
unsigned int memalloc_noreclaim_save(void);


/*********************
 ** linux/kobject.h **
 *********************/

#include <lx_emul/kobject.h>

enum kobject_action
{
	KOBJ_ADD,
	KOBJ_REMOVE,
	KOBJ_CHANGE,
};

int kobject_uevent(struct kobject *, enum kobject_action);
int kobject_uevent_env(struct kobject *kobj, enum kobject_action action, char *envp[]);


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
void *kvzalloc(size_t size, gfp_t flags);
void  kfree(const void *);
void  kzfree(const void *);
void *kmalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *kmalloc_array(size_t n, size_t size, gfp_t flags);
void *kvmalloc(size_t size, gfp_t flags);
void  kvfree(const void *);

struct kmem_cache;
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));
struct kmem_cache *kmem_cache_create_usercopy(const char *name,
                                              size_t size, size_t align, slab_flags_t flags,
                                              size_t useroffset, size_t usersize,
                                              void (*ctor)(void *));
void   kmem_cache_destroy(struct kmem_cache *);
void  *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);
void  kmem_cache_free(struct kmem_cache *, void *);
void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node);

static inline void *kmem_cache_alloc_node(struct kmem_cache *s, gfp_t flags, int node)
{
	return kmem_cache_alloc(s, flags);
}

void kmem_cache_free_bulk(struct kmem_cache *, size_t, void **);

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


/********************
 ** linux/printk.h **
 ********************/

static inline int _printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static inline int no_printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static inline int no_printk(const char *fmt, ...) { return 0; }

#define printk_ratelimit(x) (0)

#define printk_ratelimited(fmt, ...) printk(fmt, ##__VA_ARGS__)

#define pr_emerg(fmt, ...)    printk(KERN_EMERG  fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...)    printk(KERN_ALERT  fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...)     printk(KERN_CRIT   fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)      printk(KERN_ERR    fmt, ##__VA_ARGS__)
#define pr_err_once(fmt, ...) printk(KERN_ERR    fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...)  printk(KERN_WARN   fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)     printk(KERN_WARN   fmt, ##__VA_ARGS__)
#define pr_warn_once          pr_warn
#define pr_notice(fmt, ...)   printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)  no_printk(KERN_INFO   fmt, ##__VA_ARGS__)
#define pr_cont(fmt, ...)     printk(KERN_CONT   fmt, ##__VA_ARGS__)
/* pr_devel() should produce zero code unless DEBUG is defined */
#ifdef DEBUG
#define pr_devel(fmt, ...)    printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)    printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#else
#define pr_devel(fmt, ...) no_printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) no_printk(KERN_DEBUG  fmt, ##__VA_ARGS__)
#endif

#define pr_warn_ratelimited(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)

enum {
	DUMP_PREFIX_OFFSET
};

int snprintf(char *str, size_t size, const char *format, ...) __attribute__((format(printf, 3, 4)));

static inline void print_hex_dump(const char *level, const char *prefix_str,
                                  int prefix_type, int rowsize, int groupsize,
                                  const void *buf, size_t len, bool ascii)
{
	_printk("hex_dump: ");
	size_t i;
	for (i = 0; i < len; i++) {
		_printk("%x ", ((char*)buf)[i]);
	}
	_printk("\n");
}

void hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize,
                        char *linebuf, size_t linebuflen, bool ascii);
void dump_stack(void);

unsigned long int_sqrt(unsigned long);


/***********************
 ** linux/workqueue.h **
 ***********************/

struct workqueue_struct; /* XXX fix in lx_emul/work.h */

#include <lx_emul/work.h>

enum {
	WQ_UNBOUND        = 1<<1,
	// WQ_MEM_RECLAIM    = 1<<3,
	// WQ_HIGHPRI        = 1<<4,
	// WQ_CPU_INTENSIVE  = 1<<5,
};

extern struct workqueue_struct *system_power_efficient_wq;


/******************
 ** linux/wait.h **
 ******************/

#define key_to_poll(m) ((__force __poll_t)(uintptr_t)(void *)(m))

long wait_woken(wait_queue_entry_t *wait, unsigned mode, long timeout);

bool wq_has_sleeper(struct wait_queue_head *wq_head);


/******************
 ** linux/poll.h **
 ******************/

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

#include <lx_emul/pm.h>

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
bool pm_runtime_suspended(struct device *dev);
bool pm_runtime_active(struct device *dev);
int pm_runtime_resume(struct device *dev);
int pm_runtime_get(struct device *dev);
void pm_runtime_mark_last_busy(struct device *dev);
int pm_runtime_put_autosuspend(struct device *dev);


/***********************
 ** linux/pm_wakeup.h **
 ***********************/

int  device_init_wakeup(struct device *dev, bool val);
int  device_wakeup_enable(struct device *dev);
bool device_may_wakeup(struct device *dev);
int  device_set_wakeup_enable(struct device *dev, bool enable);
bool device_can_wakeup(struct device *dev);

void pm_wakeup_event(struct device *dev, unsigned int msec);


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
	const void *(*namespace)(struct device *dev);

	const struct dev_pm_ops *pm;
};

struct dma_parms;

/* DEVICE */
struct device
{
	const char                    *name;
	struct device                 *parent;
	struct kobject                 kobj;
	const struct device_type      *type;
	struct device_driver          *driver;
	void                          *platform_data;
	u64                            _dma_mask_buf;
	u64                           *dma_mask;
	u64                            coherent_dma_mask;
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
struct device *device_create_with_groups(struct class *cls,
                                         struct device *parent, dev_t devt,
                                         void *drvdata,
                                         const struct attribute_group **groups,
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
struct device *class_find_device(struct class *cls, struct device *start,
                                 const void *data,
                                 int (*match)(struct device *, const void *));

typedef void (*dr_release_t)(struct device *dev, void *res);
typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp);
void  devres_add(struct device *dev, void *res);
int   devres_destroy(struct device *dev, dr_release_t release,
                     dr_match_t match, void *match_data);
void  devres_free(void *res);
int devres_release(struct device *dev, dr_release_t release,
                   dr_match_t match, void *match_data);

void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);

#define devm_kasprintf(device, ftp, fmt, ...) kasprintf(ftp, "wifi_drv" # fmt, __VA_ARGS__)

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
	ARPHRD_INFINIBAND = 32,
	ARPHRD_IEEE80211_RADIOTAP = 803,
	ARPHRD_NETLINK = 824,
};


/**********************
 ** linux/if_ether.h **
 **********************/

enum {
	ETH_ALEN     = 6,      /* octets in one ethernet addr */
	ETH_HLEN     = 14,     /* total octets in header */
	ETH_TLEN     = 2,
	ETH_DATA_LEN = 1500,   /* Max. octets in payload */
	ETH_P_8021Q  = 0x8100, /* 802.1Q VLAN Extended Header  */
	ETH_P_8021AD = 0x88A8, /* 802.1ad Service VLAN         */
	ETH_P_PAE    = 0x888E, /* Port Access Entity (IEEE 802.1X) */

	ETH_P_802_3_MIN = 0x0600,

	ETH_FRAME_LEN = 1514,
};

#define ETH_P_TDLS     0x890D          /* TDLS */


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


/************************
 ** linux/virtio_net.h **
 ************************/

struct sk_buff;

int virtio_net_hdr_from_skb(const struct sk_buff *skb,
                            struct virtio_net_hdr *hdr, bool little_endian,
                            bool has_data_valid);
int virtio_net_hdr_to_skb(struct sk_buff *skb,
                          const struct virtio_net_hdr *hdr, bool little_endian);


/**************************
 ** uapi/linux/ethtool.h **
 **************************/

struct ethtool_link_settings
{
	__u32 speed;
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


struct ethtool_link_ksettings
{
	struct ethtool_link_settings base;
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

int __ethtool_get_link_ksettings(struct net_device *dev,
                                 struct ethtool_link_ksettings *link_ksettings);


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

#define in_task() (1)


/***********************
 ** linux/interrupt.h **
 ***********************/

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
void *idr_remove(struct idr *, unsigned long id);
void idr_destroy(struct idr *idp);
void idr_init(struct idr *idp);
void *idr_find(struct idr *idr, int id);

#define DEFINE_IDA(name) struct ida name;
struct ida { unsigned unused; };

void *idr_get_next(struct idr *idp, int *nextid);

#define idr_for_each_entry(idp, entry, id)                      \
	    for (id = 0; ((entry) = idr_get_next(idp, &(id))) != NULL; ++id)

#define IDR_INIT_BASE(base) { 0 }
#define IDR_INIT            IDR_INIT_BASE(0)
#define DEFINE_IDR(name)    struct idr name = IDR_INIT


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

#define kfree_rcu(ptr, rcu_head) kfree((ptr))

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

void hlist_add_tail_rcu(struct hlist_node *n, struct hlist_head *h);

#include <linux/hashtable.h>


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

static inline int percpu_counter_init(struct percpu_counter *fbc, s64 amount, gfp_t gfp)
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

unsigned int u64_stats_fetch_begin_irq(const struct u64_stats_sync *syncp);
bool u64_stats_fetch_retry_irq(const struct u64_stats_sync *syncp, unsigned int start);
void u64_stats_init(struct u64_stats_sync *syncp);


/********************
 ** linux/uidgid.h **
 ********************/

typedef unsigned  kuid_t;
typedef unsigned  kgid_t;

#define GLOBAL_ROOT_UID (kuid_t)0

struct user_namespace;
extern kuid_t make_kuid(struct user_namespace *from, uid_t uid);

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

static inline void eth_zero_addr(u8 *addr) {
	memset(addr, 0x00, ETH_ALEN); }

static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
	*(u32 *)dst      = *(const u32 *)src;
	*(u16 *)(dst+ 4) = *(const u16 *)(src + 4);
}

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

static inline bool is_multicast_ether_addr_64bits(const u8 addr[6+2])
{
	return is_multicast_ether_addr(addr);
}

static inline bool ether_addr_equal_64bits(const u8 addr1[6+2], const u8 addr2[6+2])
{
	u64 fold = (*(const u64 *)addr1) ^ (*(const u64 *)addr2);

	return (fold << 16) == 0;
}

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

static inline bool eth_proto_is_802_3(__be16 proto)
{
	proto &= htons(0xFF00);
	return (u16)proto >= (u16)htons(ETH_P_802_3_MIN);
}

static inline unsigned long compare_ether_header(const void *a, const void *b)
{
	u32 *a32 = (u32 *)((u8 *)a + 2); 
	u32 *b32 = (u32 *)((u8 *)b + 2); 

	return (*(u16 *)a ^ *(u16 *)b) | (a32[0] ^ b32[0]) |
	       (a32[1] ^ b32[1]) | (a32[2] ^ b32[2]);

}

unsigned char *arch_get_platform_mac_address(void);


/************************
 ** net/netns/packet.h **
 ************************/

struct netns_packet {
	struct mutex            sklist_lock;
	struct hlist_head       sklist;
};


/******************************
 ** uapi/net/net_namespace.h **
 ******************************/

enum {
	NETNSA_NSID_NOT_ASSIGNED = -1,
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
extern struct net init_net;

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

typedef struct { unsigned dummy; } possible_net_t;

struct net *get_net_ns_by_pid(pid_t pid);
struct net *get_net_ns_by_fd(int pid);

int register_pernet_subsys(struct pernet_operations *);
void unregister_pernet_subsys(struct pernet_operations *);
int register_pernet_device(struct pernet_operations *);
void unregister_pernet_device(struct pernet_operations *);
void release_net(struct net *net);

int  rt_genid(struct net *);
void rt_genid_bump(struct net *);

int peernet2id(struct net *net, struct net *peer);
int peernet2id_alloc(struct net *net, struct net *peer);
struct net *get_net_ns_by_id(struct net *net, int id);
bool peernet_has_id(struct net *net, struct net *peer);


/*************************
 ** net/netns/generic.h **
 *************************/

enum { MAX_NET_GENERIC_PTR = 16, };

struct net_generic
{
	unsigned int len;
	void *ptr[0];
};

void *net_generic(const struct net *net, unsigned int id);


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
	NETDEV_REBOOT             = 0x0003,
	NETDEV_CHANGE             = 0x0004,
	NETDEV_REGISTER           = 0x0005,
	NETDEV_UNREGISTER         = 0x0006,
	NETDEV_CHANGEMTU          = 0x0007,
	NETDEV_CHANGEADDR         = 0x0008,
	NETDEV_GOING_DOWN         = 0x0009,
	NETDEV_CHANGENAME         = 0x000A,
	NETDEV_FEAT_CHANGE        = 0x000B,
	NETDEV_BONDING_FAILOVER   = 0x000C,
	NETDEV_PRE_UP             = 0x000D,
	NETDEV_PRE_TYPE_CHANGE    = 0x000E,
	NETDEV_POST_TYPE_CHANGE   = 0x000F,
	NETDEV_POST_INIT          = 0x0010,
	NETDEV_UNREGISTER_FINAL   = 0x0011,
	NETDEV_RELEASE            = 0x0012,
	NETDEV_NOTIFY_PEERS       = 0x0013,
	NETDEV_JOIN               = 0x0014,
	NETDEV_CHANGEUPPER        = 0x0015,
	NETDEV_RESEND_IGMP        = 0x0016,
	NETDEV_CHANGEINFODATA     = 0x0018,
	NETDEV_BONDING_INFO       = 0x0019,
	NETDEV_CHANGELOWERSTATE   = 0x001B,
	NETDEV_CHANGE_TX_QUEUE_LEN = 0x001E,
};

enum {
	NET_NAME_UNKNOWN = 0,
	NET_NAME_ENUM    = 1,
	NET_NAME_USER    = 3,
};

enum netdev_priv_flags
{
	IFF_EBRIDGE = 1<<1,
	IFF_NO_QUEUE = 1<<21,

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
struct ifla_vf_stats;
struct nlattr;
struct ndmsg;
struct netlink_callback;
struct nlmsghdr;

struct netdev_phys_item_id;

struct netlink_ext_ack;

struct netdev_bpf
{
	struct {
		u8 prog_attached;
		u32 prog_id;
		/* flags with which program was installed */
		// u32 prog_flags;
	};
};

struct rtnl_link_stats64;

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
	void        (*ndo_get_stats64)(struct net_device *dev,
	                               struct rtnl_link_stats64 *storage);
	bool        (*ndo_has_offload_stats)(const struct net_device *dev, int attr_id);
	int         (*ndo_get_offload_stats)(int attr_id,
	                                     const struct net_device *dev,
	                                     void *attr_data);
	int         (*ndo_set_features)(struct net_device *dev, netdev_features_t features);
	int         (*ndo_set_vf_mac)(struct net_device *dev,
	                              int queue, u8 *mac);
	int         (*ndo_set_vf_vlan)(struct net_device *dev,
	                               int queue, u16 vlan, u8 qos, __be16 proto);
	int         (*ndo_set_vf_rate)(struct net_device *dev,
	                               int vf, int min_tx_rate,
	                               int max_tx_rate);
	int         (*ndo_set_vf_tx_rate)(struct net_device *dev,
	                                  int vf, int rate);
	int         (*ndo_set_vf_spoofchk)(struct net_device *dev,
	                                   int vf, bool setting);
	int         (*ndo_set_vf_trust)(struct net_device *dev,
	                                int vf, bool setting);
	int         (*ndo_get_vf_config)(struct net_device *dev,
	                                 int vf,
	                                 struct ifla_vf_info *ivf);
	int         (*ndo_set_vf_link_state)(struct net_device *dev,
	                                     int vf, int link_state);
	int         (*ndo_get_vf_stats)(struct net_device *dev,
	                                int vf, struct ifla_vf_stats *vf_stats);
	int         (*ndo_set_vf_port)(struct net_device *dev, int vf,
	                               struct nlattr *port[]);
	int         (*ndo_get_vf_port)(struct net_device *dev,
	                               int vf, struct sk_buff *skb);
	int         (*ndo_set_vf_guid)(struct net_device *dev, int vf, u64 guid, int guid_type);
	int         (*ndo_set_vf_rss_query_en)(struct net_device *dev,
	                                       int vf, bool setting);
	int         (*ndo_del_slave)(struct net_device *dev,
	                             struct net_device *slave_dev);
	int         (*ndo_add_slave)(struct net_device *dev,
	                             struct net_device *slave_dev,
	                             struct netlink_ext_ack *extack);
	int         (*ndo_fdb_add)(struct ndmsg *ndm,
	                           struct nlattr *tb[],
	                           struct net_device *dev,
	                           const unsigned char *addr,
	                           u16 vid,
	                           u16 flags);
	int         (*ndo_fdb_del)(struct ndmsg *ndm,
	                           struct nlattr *tb[],
	                           struct net_device *dev,
	                           const unsigned char *addr,
	                           u16 vid);
	int         (*ndo_fdb_dump)(struct sk_buff *skb,
	                            struct netlink_callback *cb,
	                            struct net_device *dev,
	                            struct net_device *filter_dev,
	                            int *idx);
	int         (*ndo_bridge_setlink)(struct net_device *dev,
	                                  struct nlmsghdr *nlh,
	                                  u16 flags);
	int         (*ndo_bridge_getlink)(struct sk_buff *skb,
	                                  u32 pid, u32 seq,
	                                  struct net_device *dev,
	                                  u32 filter_mask,
	                                  int nlflags);
	int         (*ndo_bridge_dellink)(struct net_device *dev,
	                                  struct nlmsghdr *nlh,
	                                  u16 flags);
	int         (*ndo_get_iflink)(const struct net_device *dev);
	// int         (*ndo_change_proto_down)(struct net_device *dev,
	//                                      bool proto_down);
	int         (*ndo_bpf)(struct net_device *dev, struct netdev_bpf *bpf);
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

	unsigned long state;
};

enum {
	NETDEV_QUEUE_START = 1<<1,
};

struct pcpu_sw_netstats {
	u64     rx_packets;
	u64     rx_bytes;
	u64     tx_packets;
	u64     tx_bytes;
	struct u64_stats_sync   syncp;
};

#define netdev_alloc_pcpu_stats(type) alloc_percpu(type)

/* NET_DEVICE */
struct net_device
{
	char           name[IFNAMSIZ];
	char            *ifalias;

	unsigned long       mem_end;    /* shared mem end   */
	unsigned long       mem_start;  /* shared mem start */
	unsigned long       base_addr;  /* device I/O address   */
	int         irq;        /* device IRQ number    */

	atomic_t carrier_changes;

	u32            features;
	u32            hw_features;

	struct net_device_stats      stats;
	atomic_long_t       tx_dropped;

	atomic_t        carrier_up_count;
	atomic_t        carrier_down_count;

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
	unsigned char  min_header_len;
	unsigned int   mtu;
	unsigned int   min_mtu;
	unsigned int   max_mtu;
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

	struct bpf_prog *xdp_prog;

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
		NETREG_UNINITIALIZED=0,
		NETREG_REGISTERED,
		// NETREG_UNREGISTERING,
		// NETREG_UNREGISTERED,
		// NETREG_RELEASED,
		NETREG_DUMMY,
	} reg_state;

	union {
		struct pcpu_sw_netstats  *tstats;
	};

	enum {
		RTNL_LINK_INITIALIZED,
		RTNL_LINK_INITIALIZING,
	} rtnl_link_state:16;

	bool needs_free_netdev;

	void (*priv_destructor)(struct net_device *dev);
	const struct rtnl_link_ops *rtnl_link_ops;

	unsigned int        gso_max_size;
	u16         gso_max_segs;

	struct device  dev;
	void          *priv;
	unsigned       net_ip_align;

	struct phy_device *phydev;

	int group;

	void *lx_nic_device; /* our own Nic_device */

	bool proto_down;
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
bool netif_xmit_frozen_or_drv_stopped(const struct netdev_queue *dev_queue);

void netif_set_gso_max_size(struct net_device *dev, unsigned int size);

static inline void netif_addr_lock_bh(struct net_device *dev)   { }
static inline void netif_addr_unlock_bh(struct net_device *dev) { }

static inline void netdev_set_default_ethtool_ops(struct net_device *dev,
                                                const struct ethtool_ops *ops)
{
	dev->ethtool_ops = ops;
}
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
struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name, unsigned char name_assign_type,
                                    void (*setup)(struct net_device *), unsigned int txqs, unsigned int rxqs);
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
int dev_get_phys_port_id(struct net_device *dev, struct netdev_phys_item_id *ppid);
unsigned int dev_get_flags(const struct net_device *);
struct rtnl_link_stats64 *dev_get_stats(struct net_device *dev, struct rtnl_link_stats64 *storage);
int dev_change_net_namespace(struct net_device *, struct net *, const char *);
int dev_alloc_name(struct net_device *dev, const char *name);
void dev_close(struct net_device *dev);
int dev_set_mac_address(struct net_device *, struct sockaddr *);
int dev_set_mtu(struct net_device *, int);
int dev_set_promiscuity(struct net_device *dev, int inc);
int dev_set_allmulti(struct net_device *dev, int inc);
void dev_set_group(struct net_device *, int);
int dev_change_name(struct net_device *, const char *);
int dev_set_alias(struct net_device *, const char *, size_t);
int dev_get_alias(const struct net_device *, char *, size_t);
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

int dev_change_proto_down(struct net_device *dev, bool proto_down);
int dev_get_iflink(const struct net_device *dev);
int dev_get_phys_port_name(struct net_device *dev, char *name, size_t len);

typedef int (*bpf_op_t)(struct net_device *dev, struct netdev_bpf *bpf);
int dev_change_xdp_fd(struct net_device *dev, struct netlink_ext_ack *extack, int fd, u32 flags);
void __dev_xdp_query(struct net_device *dev, bpf_op_t xdp_op, struct netdev_bpf *xdp);

int dev_change_tx_queue_len(struct net_device *, unsigned long);
bool dev_validate_header(const struct net_device *dev, char *ll_header, int len);

void dev_consume_skb_any(struct sk_buff *skb);

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
#define for_each_netdev_safe(net, d, n) \
	list_for_each_entry_safe(d, n, &(net)->dev_base_head, dev_list)
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
	u16 flush;
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

struct sk_buff *validate_xmit_skb_list(struct sk_buff *skb, struct net_device *dev, bool *again);

static inline void dev_kfree_skb_any(struct sk_buff *skb)
{
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

struct napi_struct
{
	int (*poll)(struct napi_struct *, int);
	struct net_device *dev;
};

enum { MAX_PHYS_ITEM_ID_LEN = 32 };

struct netdev_phys_item_id {
	unsigned char id[MAX_PHYS_ITEM_ID_LEN];
	unsigned char id_len;
};

struct offload_callbacks {
	struct sk_buff  *(*gso_segment)(struct sk_buff *skb, netdev_features_t features);
	struct sk_buff **(*gro_receive)(struct sk_buff **head, struct sk_buff *skb);
	int              (*gro_complete)(struct sk_buff *skb, int nhoff);
};

struct packet_offload
{
	__be16 type;
	u16 priority;
	struct offload_callbacks callbacks;
	struct list_head list;
};

/* XXX */
#define HARD_TX_LOCK(dev, txq, cpu)
#define HARD_TX_UNLOCK(dev, txq)

void netif_napi_add(struct net_device *dev, struct napi_struct *napi,
                    int (*poll)(struct napi_struct *, int), int weight);
void netif_napi_del(struct napi_struct *napi);

typedef int gro_result_t;

gro_result_t napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb);
void napi_gro_flush(struct napi_struct *napi, bool flush_old);

int init_dummy_netdev(struct net_device *dev);

void dev_add_offload(struct packet_offload *po);

struct net_device *netdev_master_upper_dev_get_rcu(struct net_device *dev);

void netdev_rss_key_fill(void *buffer, size_t len);

struct packet_offload *gro_find_complete_by_type(__be16 type);
struct packet_offload *gro_find_receive_by_type(__be16 type);
void skb_gro_flush_final(struct sk_buff *skb, struct sk_buff **pp, int flush);
void *skb_gro_header_fast(struct sk_buff *skb, unsigned int offset);
int skb_gro_header_hard(struct sk_buff *skb, unsigned int hlen);
void *skb_gro_header_slow(struct sk_buff *skb, unsigned int hlen, unsigned int offset);

void skb_gro_pull(struct sk_buff *skb, unsigned int len);
void skb_gro_postpull_rcsum(struct sk_buff *skb, const void *start, unsigned int len);
typedef struct sk_buff **(*gro_receive_t)(struct sk_buff **, struct sk_buff *);
struct sk_buff **call_gro_receive(gro_receive_t cb, struct sk_buff **head, struct sk_buff *skb);
int dev_recursion_level(void);

struct sk_buff *skb_gso_segment(struct sk_buff *skb, netdev_features_t features);
int skb_checksum_help(struct sk_buff *skb);

struct netdev_queue *skb_get_tx_queue(const struct net_device *dev, const struct sk_buff *skb);

netdev_tx_t netdev_start_xmit(struct sk_buff *skb, struct net_device *dev, struct netdev_queue *txq, bool more);


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


/*****************************
 ** uapi/linux/capability.h **
 *****************************/

enum {
	CAP_NET_BROADCAST = 11,
	CAP_NET_ADMIN     = 12,
	CAP_NET_RAW       = 13,
	CAP_IPC_LOCK      = 14,
	CAP_SYS_ADMIN     = 21,
};

bool capable(int cap);
bool ns_capable(struct user_namespace *ns, int cap);


/************************
 ** linux/capability.h **
 ************************/

bool file_ns_capable(const struct file *file, struct user_namespace *ns, int cap);


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
	long         (*compat_ioctl) (struct file *, unsigned int, unsigned long);
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

typedef struct
{
} read_descriptor_t;


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

#define TRACE_DEFINE_ENUM(x)
#define TRACE_DEFINE_SIZEOF(x)


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
static inline int is_device_dma_capable(struct device *dev) { return *dev->dma_mask; }


/************************
 ** linux/completion.h **
 ************************/

struct completion
{
	unsigned  done;
	void     *task;
};

long __wait_completion(struct completion *work, unsigned long timeout);
void complete(struct completion *);
void complete_all(struct completion *);
void init_completion(struct completion *x);
int wait_for_completion_killable(struct completion *x);
unsigned long wait_for_completion_timeout(struct completion *x, unsigned long timeout);
void wait_for_completion(struct completion *x);
int wait_for_completion_interruptible(struct completion *x);
long wait_for_completion_interruptible_timeout(struct completion *x, unsigned long timeout);
long wait_for_completion_killable_timeout(struct completion *x, unsigned long timeout);
void reinit_completion(struct completion *x);



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


/********************
 ** linux/ioport.h **
 ********************/

#include <lx_emul/ioport.h>


/***********************
 ** linux/irqreturn.h **
 ***********************/

#include <lx_emul/irq.h>

/***********************
 ** linux/interrupt.h **
 ***********************/

enum {
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	NET_SOFTIRQS,
};

#define IRQF_SHARED   0x00000080

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
void free_irq(unsigned int, void *);
int request_threaded_irq(unsigned int irq, irq_handler_t handler, irq_handler_t thread_fn, unsigned long flags, const char *name, void *dev);

#define devm_request_threaded_irq(__device__, ...) request_threaded_irq(__VA_ARGS__)

void tasklet_disable(struct tasklet_struct *t);
void tasklet_enable(struct tasklet_struct *t);

int irq_set_affinity_hint(unsigned int irq, const struct cpumask *m);


/***********************
 ** uapi/linux/uuid.h **
 ***********************/

typedef struct uuid_le uuid_le;
struct uuid_le
{
	__u8 b[16];
};


/*****************
 ** linux/pci.h **
 *****************/

struct pci_dev;

struct pci_bus
{
	struct pci_dev *self;
};

enum { DEVICE_COUNT_RESOURCE = 6 };

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

#include <lx_emul/pci.h>

void pci_set_drvdata(struct pci_dev *pdev, void *data);

/* XXX needed for iwl device table, maybe remove later? */
#include <linux/mod_devicetable.h>

//#include <asm-generic/pci-dma-compat.h>

struct msix_entry {
	u32 vector;
	u16 entry;
};

void * const *pcim_iomap_table(struct pci_dev *pdev);
int pcim_enable_device(struct pci_dev *pdev);
int pcim_iomap_regions_request_all(struct pci_dev *pdev, int mask, const char *name);


int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries, int minvec, int maxvec);
int pci_find_ext_capability(struct pci_dev *dev, int cap);
int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask);
int pci_set_dma_mask(struct pci_dev *dev, u64 mask);
struct pci_dev *pcie_find_root_port(struct pci_dev *dev);

struct device_node * pci_device_to_OF_node(const struct pci_dev *pdev);


 /******************
  ** linux/kmod.h **
  ******************/

int __request_module(bool wait, const char *name, ...);
int request_module(const char *name, ...);
#define try_then_request_module(x, mod...) \
	((x) ?: (__request_module(true, mod), (x)))


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

struct iov_iter {
	int type;
	size_t iov_offset;
	size_t count;
	union {
		const struct iovec *iov;
		const struct kvec *kvec;
		const struct bio_vec *bvec;
	};
	unsigned long nr_segs;
};

static inline size_t iov_length(const struct iovec *iov, unsigned long nr_segs)
{
	unsigned long seg;
	size_t ret = 0;

	for (seg = 0; seg < nr_segs; seg++)
		ret += iov[seg].iov_len;
	return ret;
}

static inline size_t iov_iter_count(struct iov_iter *i) {
	return i->count; }

int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len);
int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len);

void iov_iter_advance(struct iov_iter *i, size_t bytes);

size_t copy_page_to_iter(struct page *page, size_t offset, size_t bytes, struct iov_iter *i);
size_t copy_page_from_iter(struct page *page, size_t offset, size_t bytes, struct iov_iter *i);
size_t copy_to_iter(void *addr, size_t bytes, struct iov_iter *i);
size_t copy_from_iter(void *addr, size_t bytes, struct iov_iter *i);
size_t copy_from_iter_nocache(void *addr, size_t bytes, struct iov_iter *i);

ssize_t iov_iter_get_pages(struct iov_iter *i, struct page **pages,
                           size_t maxsize, unsigned maxpages, size_t *start);

size_t csum_and_copy_to_iter(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);
size_t csum_and_copy_from_iter(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);

bool csum_and_copy_from_iter_full(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);
bool copy_from_iter_full(void *addr, size_t bytes, struct iov_iter *i);
bool copy_from_iter_full_nocache(void *addr, size_t bytes, struct iov_iter *i);

void iov_iter_revert(struct iov_iter *i, size_t bytes);

/***********************
 ** linux/if_bridge.h **
 ***********************/

enum {
	BR_HAIRPIN_MODE         = BIT(0),
	BR_BPDU_GUARD           = BIT(1),
	BR_ROOT_BLOCK           = BIT(2),
	BR_MULTICAST_FAST_LEAVE = BIT(3),
	BR_LEARNING             = BIT(5),
	BR_FLOOD                = BIT(6),
	BR_PROXYARP             = BIT(8),
	BR_LEARNING_SYNC        = BIT(9),
	BR_PROXYARP_WIFI        = BIT(10),
};

/*********************
 ** linux/if_vlan.h **
 *********************/

enum {
	VLAN_HLEN       = 4,
	VLAN_ETH_HLEN   = 18,
	VLAN_PRIO_SHIFT = 13,
	VLAN_PRIO_MASK  = 0xe000,
	VLAN_VID_MASK   = 0x0fff,
};

struct vlan_hdr
{
	__be16 h_vlan_TCI;
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

#define skb_vlan_tag_present(__skb)  (0)
#define skb_vlan_tag_get(__skb) (0xffff)

void __vlan_hwaccel_put_tag(struct sk_buff *skb, __be16 vlan_proto, u16 vlan_tci);
void vlan_set_encap_proto(struct sk_buff *skb, struct vlan_hdr *vhdr);

static inline bool eth_type_vlan(__be16 ethertype)
{
	return false;
}


static inline int __vlan_insert_tag(struct sk_buff *skb, __be16 vlan_proto, u16 vlan_tci)
{
	return 1;
}


/********************
 ** linux/percpu.h **
 ********************/

void *__alloc_percpu(size_t size, size_t align);

#define alloc_percpu_gfp(type, gfp) \
	    (typeof(type) __percpu *)__alloc_percpu(sizeof(type), __alignof__(type))

#define alloc_percpu(type)      \
	(typeof(type) __percpu *)__alloc_percpu(sizeof(type), __alignof__(type))


#define per_cpu(var, cpu) var
#define per_cpu_ptr(ptr, cpu)   ({ (void)(cpu);(typeof(*(ptr)) *)(ptr); })
#define __get_cpu_var(var) var

#define this_cpu_ptr(ptr) ptr
#define this_cpu_inc(pcp) pcp += 1
#define this_cpu_dec(pcp) pcp -= 1

#define __this_cpu_inc(pcp) this_cpu_inc(pcp)
#define __this_cpu_dec(pcp) this_cpu_dec(pcp)


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


/**************************
 ** linux/page_counter.h **
 **************************/

struct page_counter
{
	atomic_long_t count;
	unsigned long limit;
};

static inline unsigned long page_counter_read(struct page_counter *counter) {
	return atomic_long_read(&counter->count); }

void page_counter_charge(struct page_counter *counter, unsigned long nr_pages);
void page_counter_uncharge(struct page_counter *counter, unsigned long nr_pages);


/************************
 ** linux/memcontrol.h **
 ************************/

struct mem_cgroup;

enum { UNDER_LIMIT, SOFT_LIMIT, OVER_LIMIT };

void sock_update_memcg(struct sock *sk);

struct cg_proto
{
	struct page_counter memory_allocated;
	struct percpu_counter sockets_allocated;
	int memory_pressure;
	long sysctl_mem[3];
};

#define mem_cgroup_sockets_enabled 0

static inline bool mem_cgroup_under_socket_pressure(struct mem_cgroup *memcg)
{
	return false;
}

static inline void mem_cgroup_sk_alloc(struct sock *sk) { };
static inline void mem_cgroup_sk_free(struct sock *sk)  { };

bool mem_cgroup_charge_skmem(struct mem_cgroup *memcg, unsigned int nr_pages);
void mem_cgroup_uncharge_skmem(struct mem_cgroup *memcg, unsigned int nr_pages);


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

u64 local_clock(void);

int fatal_signal_pending(struct task_struct *p);


/************************
 ** linux/sched/user.h **
 ************************/

struct user_struct
{
	atomic_long_t locked_vm;
};

void free_uid(struct user_struct *);
struct user_struct *get_uid(struct user_struct *u);


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

struct group_info
{
	int ngroups;
	kgid_t gid[0];
};

struct cred
{
	struct user_namespace *user_ns;
	kuid_t euid;
	kgid_t egid;

	struct group_info *group_info;
};

static inline void current_uid_gid(kuid_t *u, kgid_t *g)
{
	*u = 0;
	*g = 0;
}


extern struct user_namespace init_user_ns;
#define current_user_ns() (&init_user_ns)

struct user_struct *current_user();

void put_cred(const struct cred *_cred);


/********************
 ** net/if_inet6.h **
 ********************/

struct inet6_dev;


/*********************
 ** uapi/linux/in.h **
 *********************/

enum {
	IPPROTO_IP  = 0,
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
unsigned int inner_tcp_hdrlen(const struct sk_buff *skb);
bool skb_is_tcp_pure_ack(const struct sk_buff *skb);



/**********************
 ** uapi/linux/tcp.h **
 **********************/

enum { TCP_CA_NAME_MAX = 16 };

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

char *tcp_ca_get_name_by_key(u32 key, char *buffer);


/*****************
 ** linux/tcp.h **
 *****************/

struct tcp_sock
{
	u32 snd_una;
};

unsigned int tcp_hdrlen(const struct sk_buff *skb);
static inline struct tcp_sock *tcp_sk(const struct sock *sk) {
	return (struct tcp_sock *)sk; }


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

#define num_possible_cpus() 1U


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

struct sock_filter { /* Filter block */
	__u16   code;    /* Actual filter code */
	__u8    jt;      /* Jump true */
	__u8    jf;      /* Jump false */
	__u32   k;       /* Generic multiuse field */
};


/**********************
 ** uapi/linux/bpf.h **
 **********************/

enum bpf_prog_type {
	BPF_PROG_TYPE_SOCKET_FILTER,
};


/********************
 ** linux/filter.h **
 ********************/

struct sk_buff;
struct sock_filter;
struct sock_fprog;
struct bpf_prog_aux;

struct bpf_prog
{
	u32                len;
	enum bpf_prog_type type;

	struct bpf_prog_aux *aux;

	union {
		struct sock_filter insns[0];
	};
};

struct sk_filter
{
	atomic_t        refcnt;
	struct rcu_head rcu;
	struct bpf_prog *prog;
};

unsigned int sk_filter_size(unsigned int proglen);
int sk_filter(struct sock *, struct sk_buff *);
unsigned int sk_run_filter(const struct sk_buff *skb,
                           const struct sock_filter *filter);
int sk_attach_filter(struct sock_fprog *, struct sock *);
int sk_detach_filter(struct sock *);
int sk_get_filter(struct sock *, struct sock_filter *, unsigned);
int sk_filter_trim_cap(struct sock *sk, struct sk_buff *skb, unsigned int cap);
void sk_filter_uncharge(struct sock *sk, struct sk_filter *fp);
bool sk_filter_charge(struct sock *sk, struct sk_filter *fp);

#define SK_RUN_FILTER(FILTER, SKB) sk_run_filter(SKB, FILTER->insns)

int bpf_tell_extensions(void);

typedef int (*bpf_aux_classic_check_t)(struct sock_filter *filter,
                                       unsigned int flen);

int bpf_prog_create_from_user(struct bpf_prog **pfp, struct sock_fprog *fprog,
                              bpf_aux_classic_check_t trans, bool save_orig);
void bpf_prog_destroy(struct bpf_prog *fp);
u32 bpf_prog_run_clear_cb(const struct bpf_prog *prog, struct sk_buff *skb);

int sk_reuseport_attach_bpf(u32 ufd, struct sock *sk);
int sk_reuseport_attach_filter(struct sock_fprog *fprog, struct sock *sk);
int sk_attach_bpf(u32 ufd, struct sock *sk);


/*****************
 ** linux/bpf.h **
 *****************/

struct bpf_prog_aux
{
	u32 id;
};

static inline struct bpf_prog *bpf_prog_get(u32 ufd) {
	return (struct bpf_prog*)ERR_PTR(-EOPNOTSUPP); }

static inline void bpf_prog_put(struct bpf_prog *prog) { }
static inline struct bpf_prog *bpf_prog_get_type(u32 ufd, enum bpf_prog_type type) {
	return (struct bpf_prog*)ERR_PTR(-EOPNOTSUPP); }


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

#define get_user(x, ptr) ({  x   = *(ptr); 0; })
#define put_user(x, ptr) ({ *(ptr) =  x;   0; })

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

/*
 * XXX cannot use emul header, see comment below.
 */
// #include <lx_emul/scatterlist.h>

struct scatterlist
{
	/*
	 * We use a dummy struct page member because there is
	 * none for abitrary addresses that were not allocated
	 * with alloc_page() and save the buf pointer given in
	 * sg_set_buf directly to page_dummy.addr and hope for
	 * the best. This dummy is then stored in page_link.
	 * The offset is always 0.
	 */
	struct page  page_dummy;
	unsigned int page_flags;

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

#define sg_is_chain(sg)  ((sg)->page_flags & 0x01)
#define sg_is_last(sg)   ((sg)->page_flags & 0x02)
#define sg_chain_ptr(sg) ((struct scatterlist *) ((sg)->page_link))

static inline struct page *sg_page(struct scatterlist *sg) {
	return (struct page *)((sg)->page_link); }

void sg_chain(struct scatterlist *prv, unsigned int prv_nents, struct scatterlist *sgl);

int sg_nents(struct scatterlist *sg);

struct scatterlist *sg_next(struct scatterlist *);

#define for_each_sg(sglist, sg, nr, __i) \
	    for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

size_t sg_pcopy_from_buffer(struct scatterlist *sgl, unsigned int nents, const void *buf, size_t buflen, off_t skip);


/**************
 ** net/ip.h **
 **************/

enum { IP_DEFRAG_AF_PACKET = 42, };

struct inet_skb_parm
{
	int iif;
};

unsigned int ip_hdrlen(const struct sk_buff *skb);
struct sk_buff *ip_check_defrag(struct net *net, struct sk_buff *skb, u32 user);


/********************
 ** linux/dcache.h **
 ********************/

unsigned int full_name_hash(const unsigned char *, unsigned int);


/******************
 ** linux/hash.h **
 ******************/

u32 hash_32(u32 val, unsigned int);


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

__wsum remcsum_adjust(void *ptr, __wsum csum, int start, int offset);


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


/**************************
 ** uapi/linux/netlink.h **
 **************************/

#define NLA_ALIGNTO     4
#define NLA_ALIGN(len)      (((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))


/***********************
 ** linux/genetlink.h **
 ***********************/

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
	SOF_TIMESTAMPING_SYS_HARDWARE = 1 << 5,
	SOF_TIMESTAMPING_RAW_HARDWARE = 1 << 6,
	SOF_TIMESTAMPING_OPT_ID       = 1 << 7,
	SOF_TIMESTAMPING_TX_SCHED     = 1 << 8,
	SOF_TIMESTAMPING_TX_ACK       = 1 << 9,
	SOF_TIMESTAMPING_OPT_TSONLY   = 1 << 11,
	SOF_TIMESTAMPING_OPT_STATS    = 1 << 12,
	SOF_TIMESTAMPING_OPT_TX_SWHW  = 1 << 14,
	SOF_TIMESTAMPING_MASK = (SOF_TIMESTAMPING_RAW_HARDWARE - 1) |
	                         SOF_TIMESTAMPING_RAW_HARDWARE,
};

#define SOF_TIMESTAMPING_TX_RECORD_MASK (SOF_TIMESTAMPING_TX_HARDWARE | \
	SOF_TIMESTAMPING_TX_SOFTWARE | \
	SOF_TIMESTAMPING_TX_SCHED | \
	SOF_TIMESTAMPING_TX_ACK)


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

struct flow_dissector_key_control
{
	u16 thoff;
	u16 addr_type;
	u32 flags;
};

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

	struct flow_dissector_key_control control;
};

struct flow_dissector_key
{
	unsigned dummy;
};

struct flow_dissector
{
	unsigned dummy;
};

extern struct flow_dissector flow_keys_dissector;
extern struct flow_dissector flow_keys_buf_dissector;

bool flow_keys_have_l4(struct flow_keys *keys);


/****************
 ** net/flow.h **
 ****************/

enum {
	FLOWI_FLAG_ANYSRC = 0x01,
};

struct flowi4
{
	unsigned dummy;
};

struct flowi6
{
	unsigned dummy;
};

struct flowi
{
	union {
		struct flowi4 ip4;
		struct flowi6 ip6;
	} u;
};

__u32 __get_hash_from_flowi4(const struct flowi4 *fl4, struct flow_keys *keys);
__u32 __get_hash_from_flowi6(const struct flowi6 *fl6, struct flow_keys *keys);


/**************************
 ** net/flow_dissector.h **
 **************************/

#define FLOW_DISSECTOR_F_PARSE_1ST_FRAG     BIT(0)


/***********************
 ** linux/pipe_fs_i.h **
 ***********************/

extern const struct pipe_buf_operations nosteal_pipe_buf_ops;


/******************
 ** linux/acpi.h **
 ******************/

#define ACPI_PTR(_ptr)   (NULL)
#define ACPI_HANDLE(dev) (NULL)

struct acpi_device;

const struct acpi_device_id *acpi_match_device(const struct acpi_device_id *ids, const struct device *dev);

struct acpi_gpio_params {
	unsigned int crs_entry_index;
	unsigned int line_index;
	bool active_low;
};

struct acpi_gpio_mapping {
	const char *name;
	const struct acpi_gpio_params *data;
	unsigned int size;
};

static inline int acpi_dev_add_driver_gpios(struct acpi_device *adev,
                                           const struct acpi_gpio_mapping *gpios)
{
	return -ENXIO;
}

static inline void acpi_dev_remove_driver_gpios(struct acpi_device *adev) {}

static inline int devm_acpi_dev_add_driver_gpios(struct device *dev, const struct acpi_gpio_mapping *gpios)
{
	return -ENXIO;
}


/********************
 ** linux/random.h **
 ********************/

u32 prandom_u32(void);
void get_random_bytes(void *buf, int nbytes);

static inline int get_random_bytes_wait(void *buf, int nbytes)
{
	get_random_bytes(buf, nbytes);
	return 0;
}

static inline u32 get_random_u32(void)
{
	return prandom_u32();
}

static inline u32 prandom_u32_max(u32 ep_ro) { return (u32)(((u64) prandom_u32() * ep_ro) >> 32); }

static inline void prandom_bytes(void *buf, size_t nbytes)
{
	get_random_bytes(buf, nbytes);
}


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

#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))


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


/*********************************
 ** uapi/asm-generic/resource.h **
 *********************************/

#define RLIMIT_MEMLOCK 8


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
void mdelay(unsigned long msecs);


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
int xfrm_sk_clone_policy(struct sock *sk, const struct sock *osk);

struct dst_entry;
bool xfrm_dst_offload_ok(struct dst_entry *dst);


/*************************
 ** linux/debug_locks.h **
 *************************/

static inline void debug_check_no_locks_freed(const void *from, unsigned long len) { }


/**********************
 ** net/cls_cgroup.h **
 **********************/

struct sock_cgroup_data;

void sock_update_classid(struct sock_cgroup_data *skcd);


/**************************
 ** net/netprio_cgroup.h **
 **************************/

void sock_update_netprioidx(struct sock_cgroup_data *skcd);


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

enum gpiod_flags {
	GPIOD_FLAGS_BIT_DIR_SET = 1 << 0,
	GPIOD_FLAGS_BIT_DIR_OUT = 1 << 1,
	GPIOD_OUT_LOW = GPIOD_FLAGS_BIT_DIR_SET | GPIOD_FLAGS_BIT_DIR_OUT,
};


struct gpio_desc * devm_gpiod_get_index(struct device *dev, const char *con_id, unsigned int idx);
int gpiod_direction_output(struct gpio_desc *desc, int value);
void gpiod_set_value(struct gpio_desc *desc, int value);
void gpiod_set_value_cansleep(struct gpio_desc *desc, int value);
struct gpio_desc *devm_gpiod_get_optional(struct device *dev, const char *con_id, enum gpiod_flags flags);


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

void wireless_nlevent_flush(void);


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

enum { fips_enabled = 0 };


/*********************
 ** net/switchdev.h **
 *********************/

enum switchdev_attr_id {
	SWITCHDEV_ATTR_ID_UNDEFINED,
	SWITCHDEV_ATTR_ID_PORT_PARENT_ID,
};

struct switchdev_attr
{
	struct net_device *orig_dev;
	enum switchdev_attr_id id;
	u32 flags;
	union {
		struct netdev_phys_item_id ppid; /* PORT_PARENT_ID */
		u8 stp_state;                    /* PORT_STP_STATE */
		unsigned long brport_flags;      /* PORT_BRIDGE_FLAGS */
		u32 ageing_time;                 /* BRIDGE_AGEING_TIME */
	} u;

};

int switchdev_port_attr_get(struct net_device *dev, struct switchdev_attr *attr);

#define SWITCHDEV_F_NO_RECURSE BIT(0)


/************************************
 ** uapi/linux/input-event-codes.h **
 ************************************/

enum {
	EV_KEY = 0x01,
	EV_SW  = 0x05,

	SW_RFKILL_ALL = 0x03,

	KEY_BLUETOOTH = 237,
	KEY_WLAN      = 238,
	KEY_UWB       = 239,
	KEY_WIMAX     = 246,
	KEY_RFKILL    = 247,
};


/*******************
 ** linux/input.h **
 *******************/

struct input_dev
{
	unsigned long evbit[1];//[BITS_TO_LONGS(EV_CNT)];
	unsigned long swbit[1];//[BITS_TO_LONGS(SW_CNT)];
	unsigned long sw[1];//[BITS_TO_LONGS(SW_CNT)];

	spinlock_t event_lock;
};

struct input_handle;
struct input_device_id;

struct input_handler
{
	void (*event)(struct input_handle *handle, unsigned int type, unsigned int code, int value);
	int (*connect)(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id);
	void (*disconnect)(struct input_handle *handle);
	void (*start)(struct input_handle *handle);

	const char *name;
	const struct input_device_id *id_table;
};

struct input_handle
{
	const char *name;

	struct input_dev *dev;
	struct input_handler *handler;
};

int input_register_handle(struct input_handle *);
int input_open_device(struct input_handle *);
void input_unregister_handle(struct input_handle *);
void input_close_device(struct input_handle *);
int input_register_handler(struct input_handler *);
void input_unregister_handler(struct input_handler *);


/***********************
 ** uapi/linux/mlps.h **
 ***********************/

enum {
	MPLS_LS_TC_MASK  = 0x00000E00,
	MPLS_LS_TC_SHIFT = 9u,
};

struct mpls_label
{
	__be32 entry;
};


/*************************
 ** linux/devcoredump.h **
 *************************/

static inline void dev_coredumpm(struct device *dev, struct module *owner,
                                 const void *data, size_t datalen, gfp_t gfp,
                                 ssize_t (*read)(char *buffer, loff_t offset,
                                 size_t count, const void *data, size_t datalen),
                                 void (*free)(const void *data))
{
	free(data);
}

static inline void dev_coredumpsg(struct device *dev, struct scatterlist *table,
                                  size_t datalen, gfp_t gfp)
{
	// _devcd_free_sgtable(table);
	lx_printf("%s:%d: WARNING: leaking memory for table: %p\n",
	          __func__, __LINE__, table);
}


/*************************
 ** linux/timekeeping.h **
 *************************/

typedef __s64 time64_t;

time64_t ktime_get_seconds(void);

u64 ktime_get_ns(void);
u64 ktime_get_boot_ns(void);


/***********************************
 ** uapi/linux/virtio_types.h:42: **
 ***********************************/

typedef __u16 __virtio16;


/*******************************
 ** linux/virtio_byteorder.h **
 *******************************/

static inline bool virtio_legacy_is_little_endian(void) { return true; }

static inline u16 __virtio16_to_cpu(bool little_endian, __virtio16 val)
{
	if (little_endian) return le16_to_cpu((__le16)val);
	else               return be16_to_cpu((__be16)val);
}

static inline __virtio16 __cpu_to_virtio16(bool little_endian, u16 val)
{
	if (little_endian) return (__virtio16)cpu_to_le16(val);
	else               return (__virtio16)cpu_to_be16(val);
}


/********************
 ** linux/mmzone.h **
 ********************/

enum { PAGE_ALLOC_COSTLY_ORDER = 3u };


/****************************
 ** linux/u64_stats_sync.h **
 ****************************/

struct u64_stats_sync;

static inline void u64_stats_update_begin(struct u64_stats_sync *p) { }

static inline void u64_stats_update_end(struct u64_stats_sync *p) { }

unsigned int u64_stats_fetch_begin(const struct u64_stats_sync *syncp);
unsigned int u64_stats_fetch_begin_irq(const struct u64_stats_sync *p);
bool u64_stats_fetch_retry(const struct u64_stats_sync *syncp, unsigned int start);
bool u64_stats_fetch_retry_irq(const struct u64_stats_sync *p, unsigned int s);


/************************
 ** uapi/linux/uleds.h **
 ************************/

#define LED_MAX_NAME_SIZE 64


/*************************
 ** linux/cgroup-defs.h **
 *************************/

struct sock_cgroup_data
{
	union {
		u64 val;
	};
};

struct cgroup
{
	unsigned long foo;
};


/*********************
 ** net/pkg_sched.h **
 *********************/

#define DEFAULT_TX_QUEUE_LEN 1000


/****************************
 ** uapi/linux/eventpoll.h **
 ****************************/

#define EPOLLIN     0x00000001
#define EPOLLPRI    0x00000002
#define EPOLLOUT    0x00000004
#define EPOLLERR    0x00000008
#define EPOLLHUP    0x00000010
// #define EPOLLNVAL   0x00000020
#define EPOLLRDNORM 0x00000040
#define EPOLLRDBAND 0x00000080
#define EPOLLWRNORM 0x00000100
#define EPOLLWRBAND 0x00000200
// #define EPOLLMSG    0x00000400
#define EPOLLRDHUP  0x00002000


/******************
 ** linux/sctp.h **
 ******************/

struct sctphdr
{
	unsigned unused;
};

struct sctp_header
{
	unsigned unused;
};


/****************************
 ** uapi/linux/sock_diag.h **
 ****************************/

enum {
	SK_MEMINFO_RMEM_ALLOC,
	SK_MEMINFO_RCVBUF,
	SK_MEMINFO_WMEM_ALLOC,
	SK_MEMINFO_SNDBUF,
	SK_MEMINFO_FWD_ALLOC,
	SK_MEMINFO_WMEM_QUEUED,
	SK_MEMINFO_OPTMEM,
	SK_MEMINFO_BACKLOG,
	SK_MEMINFO_DROPS,

	SK_MEMINFO_VARS,
};


/***********************
 ** linux/sock_diag.h **
 ***********************/

u64 sock_gen_cookie(struct sock *sk);
bool sock_diag_has_destroy_listeners(const struct sock *sk);
void sock_diag_broadcast_destroy(struct sock *sk);


/**************************
  ** net/timewait_sock.h **
 **************************/

struct timewait_sock_ops
{
	struct kmem_cache *twsk_slab;
	char *twsk_slab_name;
	unsigned int twsk_obj_size;
};


/********************
 ** linux/stddef.h **
 ********************/

#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))
#define offsetofend(TYPE, MEMBER) \
	(offsetof(TYPE, MEMBER) + sizeof_field(TYPE, MEMBER))



/*****************
 ** net/codel.h **
 *****************/

#define CODEL_SHIFT 10
#define MS2TIME(a) ((a * NSEC_PER_MSEC) >> CODEL_SHIFT)

#define CODEL_DISABLED_THRESHOLD INT_MAX

typedef u32 codel_time_t;

struct codel_params
{
	codel_time_t    target;
	codel_time_t    ce_threshold;
	codel_time_t    interval;
	u32     mtu;
	bool        ecn;
};

struct codel_vars
{
	unsigned unused;
};

struct codel_stats
{
	unsigned unused;
};

codel_time_t codel_get_time(void);

typedef u32 (*codel_skb_len_t)(const struct sk_buff *skb);
typedef codel_time_t (*codel_skb_time_t)(const struct sk_buff *skb);
typedef void (*codel_skb_drop_t)(struct sk_buff *skb, void *ctx);
typedef struct sk_buff * (*codel_skb_dequeue_t)(struct codel_vars *vars, void *ctx);


/**********************
 ** net/codel_impl.h **
 **********************/

void codel_params_init(struct codel_params *params);
void codel_vars_init(struct codel_vars *vars);
void codel_stats_init(struct codel_stats *stats);

struct sk_buff *codel_dequeue(void *ctx, u32 *backlog, struct codel_params *params,
                               struct codel_vars *vars, struct codel_stats *stats,
                               codel_skb_len_t skb_len_func, codel_skb_time_t skb_time_func,
                               codel_skb_drop_t drop_func, codel_skb_dequeue_t dequeue_func);


/**************
 ** net/fq.h **
 **************/

struct fq_tin;

#include <linux/skbuff.h>

struct fq_flow
{
	struct sk_buff_head queue;
	u32 backlog;
};

struct fq_tin
{
	u32 backlog_bytes;
	u32 backlog_packets;
};

struct fq
{
	struct fq_flow *flows;
	spinlock_t lock;
	u32 flows_cnt;
	u32 memory_limit;
	u32 backlog;
};


typedef struct sk_buff *fq_tin_dequeue_t(struct fq *, struct fq_tin *, struct fq_flow *flow);
typedef void fq_skb_free_t(struct fq *, struct fq_tin *, struct fq_flow *, struct sk_buff *); 
typedef bool fq_skb_filter_t(struct fq *, struct fq_tin *, struct fq_flow *, struct sk_buff *, void *);
typedef struct fq_flow *fq_flow_get_default_t(struct fq *, struct fq_tin *, int idx, struct sk_buff *);


/*******************
 ** net/fq_impl.h **
 *******************/

struct sk_buff *fq_flow_dequeue(struct fq *fq, struct fq_flow *flow);
void fq_tin_enqueue(struct fq *fq, struct fq_tin *tin, struct sk_buff *skb,
                    fq_skb_free_t free_func, fq_flow_get_default_t get_default_func);
void fq_tin_filter(struct fq *fq, struct fq_tin *tin, fq_skb_filter_t filter_func,
                   void *filter_data, fq_skb_free_t free_func);
void fq_tin_init(struct fq_tin *tin);
void fq_flow_init(struct fq_flow *flow);
void fq_tin_reset(struct fq *fq, struct fq_tin *tin, fq_skb_free_t free_func);
int fq_init(struct fq *fq, int flows_cnt);
void fq_reset(struct fq *fq, fq_skb_free_t free_func);
struct fq_flow *fq_flow_classify(struct fq *fq, struct fq_tin *tin, struct sk_buff *skb, fq_flow_get_default_t get_default_func);
void fq_recalc_backlog(struct fq *fq, struct fq_tin *tin, struct fq_flow *flow);
struct sk_buff *fq_tin_dequeue(struct fq *fq, struct fq_tin *tin, fq_tin_dequeue_t dequeue_func);


/*******************************
 ** XXX needed by af_packet.c **
 *******************************/

#include <linux/if_ether.h>


/*******************
 ** linux/cache.h **
 *******************/

#define __ro_after_init


/******************
 ** net/l3mdev.h **
 ******************/

int l3mdev_master_ifindex_by_index(struct net *net, int ifindex);


/********************
 ** linux/cgroup.h **
 ********************/

static inline void cgroup_sk_alloc(struct sock_cgroup_data *skcd) { }
static inline void cgroup_sk_free(struct sock_cgroup_data *skcd) { }


/*******************
 ** acpi/actype.h **
 *******************/

typedef char *acpi_string;


/**********************
 ** linux/bitfield.h **
 **********************/

#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define FIELD_PREP(_mask, _val) \
	({ ((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask); })
#define FIELD_GET(_mask, _reg) \
	({ (typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask)); })

/********************
 ** linux/nospec.h **
 ********************/

static inline unsigned long array_index_mask_nospec(unsigned long index,
                                                    unsigned long size)
{
	(void)index;
	return ~(long)(index | (size - 1UL - index)) >> (BITS_PER_LONG - 1); 
}


#define array_index_nospec(index, size)                 \
	({                                  \
		typeof(index) _i = (index);                 \
		typeof(size) _s = (size);                   \
		unsigned long _mask = array_index_mask_nospec(_i, _s);      \
		\
		BUILD_BUG_ON(sizeof(_i) > sizeof(long));            \
		BUILD_BUG_ON(sizeof(_s) > sizeof(long));            \
		\
		(typeof(_i)) (_i & _mask);                  \
	})


/****************************
 ** linux/sock_reuseport.h **
 ****************************/

void reuseport_detach_sock(struct sock *sk);

/* XXX figure out why we need to include this header */
#include <net/sock.h>


/***************
 ** net/dsa.h **
 ***************/

bool netdev_uses_dsa(struct net_device *dev);


/********************
 ** linux/of_net.h **
 ********************/

const void *of_get_mac_address(struct device_node *np);


/**************************
 ** linux/sched/signal.h **
 **************************/

unsigned long rlimit(unsigned int limit);


/**********************
 ** linux/property.h **
 **********************/

int device_property_read_string(struct device *dev, const char *propname, const char **val);

#include <lx_emul/extern_c_end.h>


/******************************
 ** uapi/asm-generic/ioctl.h **
 ******************************/

#define _IOC_NR(nr) (nr)
#define _IOC_TYPE(nr) (nr)

#endif /* _LX_EMUL_H_ */
