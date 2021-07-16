/*
 * \brief  Emulation of the Linux kernel API
 * \author Sebastian Sumpf
 * \date   2013-08-19
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

#include <legacy/lx_emul/extern_c_begin.h>

#define DEBUG_PRINTK 1
#define DEBUG_SLAB   0
#define DEBUG_TIMER  0
#define DEBUG_LEVEL  0

#define KBUILD_MODNAME "mod-noname"


/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100UL };


/******************
 ** asm/atomic.h **
 ******************/

#include <legacy/lx_emul/atomic.h>


/*******************
 ** linux/types.h **
 *******************/

#include <legacy/lx_emul/types.h>

typedef __u8  u_int8_t;

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;

typedef __u64 __be64;

typedef __u16 __sum16;
typedef __u32 __wsum;

#define __aligned_u64 __u64 __attribute__((aligned(8)))

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]


/**********************
 ** linux/compiler.h **
 **********************/

#include <legacy/lx_emul/compiler.h>


/**************************
 ** asm-generic/bitops.h **
 **************************/

#include <legacy/lx_emul/bitops.h>

#define ffz(x) __ffs(~(x))

/*******************************
 ** asm-generic/bitops/find.h **
 *******************************/

unsigned long find_first_zero_bit(unsigned long const *addr, unsigned long size);

static inline unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
		unsigned long offset)
{
	unsigned long i  = offset / BITS_PER_LONG;
	offset -= (i * BITS_PER_LONG);

	for (; offset < size; offset++)
		if (addr[i] & (1UL << offset))
			return offset;

	return size;
}

#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)


/****************************
 ** bitops/const_hweight.h **
 ****************************/

static inline unsigned int hweight32(unsigned int w)
{
	w -= (w >> 1) & 0x55555555;
	w  = (w & 0x33333333) + ((w >> 2) & 0x33333333);
	w  = (w + (w >> 4)) & 0x0f0f0f0f;
	return (w * 0x01010101) >> 24;

}

unsigned int hweight64(__u64 w);


/****************************
 ** asm-generic/getorder.h **
 ****************************/

int get_order(unsigned long size);


/************************
 ** uapi/linux/types.h **
 ************************/

struct callback_head {
	struct callback_head *next;
	void (*func)(struct callback_head *head);
};
#define rcu_head callback_head


/*******************************
 ** linux/errno.h and friends **
 *******************************/

#include <legacy/lx_emul/errno.h>


/********************
 ** linux/module.h **
 ********************/

#include <legacy/lx_emul/module.h>

#ifdef __setup
#undef __setup
#endif

#define __setup(str, fn) void SETUP_CONCAT(fn, SETUP_SUFFIX)(void *addrs) { fn(addrs); }


/******************
 ** linux/init.h **
 ******************/

#define __initdata
#define  fs_initcall(fn)     void fs_##fn(void) { fn(); }
#define  late_initcall(fn)   void late_##fn(void) { fn(); }


/*******************
 ** asm/barrier.h **
 *******************/

#include <legacy/lx_emul/barrier.h>

#define smp_load_acquire(p)     *(p)
#define smp_store_release(p, v) *(p) = v;
#define smp_mb__before_atomic() mb()
#define smp_mb__after_atomic()  mb()


/*********************
 ** linux/kconfig.h **
 *********************/

#define CONFIG_DEFAULT_TCP_CONG "cubic"


/**********************
 ** linux/compiler.h **
 **********************/

#include <legacy/lx_emul/compiler.h>

#define __deprecated

#define __aligned(x)  __attribute__((aligned(x)))

#define noinline_for_stack

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

#define READ_ONCE(x) \
({                                               \
	union { typeof(x) __val; char __c[1]; } __u; \
	__read_once_size(&(x), __u.__c, sizeof(x));  \
	__u.__val;                                   \
})

#define __HAVE_BUILTIN_BSWAP32__
#define __HAVE_BUILTIN_BSWAP64__


/***************
 ** asm/bug.h **
 ***************/

#include <legacy/lx_emul/bug.h>


/********************
 ** linux/printk.h **
 ********************/

#include <legacy/lx_emul/printf.h>

#define pr_crit(fmt, ...)       lx_printf(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_emerg(fmt, ...)      lx_printf(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)        lx_printf(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)       lx_printf(KERN_WARN fmt, ##__VA_ARGS__)
#define pr_warn_once(fmt, ...)  lx_printf(KERN_WARN fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)       lx_printf(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...)     lx_printf(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_cont(fmt, ...)       lx_printf(KERN_CONT fmt, ##__VA_ARGS__)
#define pr_info_once(fmt, ...)  lx_printf(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_err_once(fmt, ...)   lx_printf(KERN_ERR fmt, ##__VA_ARGS__)

#if DEBUG_LEVEL
#define pr_debug(fmt, ...)  lx_printf(KERN_DEBUG fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif

static inline __printf(1, 2) int no_printk(const char *fmt, ...) { return 0; }


/********************
 ** linux/kernel.h **
 ********************/

#include <legacy/lx_emul/kernel.h>

#define KERN_CONT   ""

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

#define SIZE_MAX (~(size_t)0)

#define U32_MAX  ((u32)~0U)
#define S32_MAX  ((s32)(U32_MAX>>1))
#define S32_MIN  ((s32)(-S32_MAX - 1))

#define PTR_ALIGN(p, a) ({              \
	unsigned long _p = (unsigned long)p;  \
	_p = (_p + a - 1) & ~(a - 1);         \
	p = (typeof(p))_p;                    \
	p;                                    \
})

enum { SPRINTF_STR_LEN = 64 };

int   snprintf(char *buf, size_t size, const char *fmt, ...);
int   sprintf(char *buf, const char *fmt, ...);
int   sscanf(const char *, const char *, ...);
int   kstrtoul(const char *, unsigned int, unsigned long *);
int   scnprintf(char *, size_t, const char *, ...);

#define sprintf(buf, fmt, ...) snprintf(buf, SPRINTF_STR_LEN, fmt, __VA_ARGS__)

#define kasprintf(gfp, fmt, ...) ({ \
	void *buf = kmalloc(SPRINTF_STR_LEN, 0); \
	sprintf(buf, fmt, __VA_ARGS__); \
	buf; \
})

char *get_options(const char *str, int nints, int *ints);
int   hex_to_bin(char);

u32 reciprocal_scale(u32 val, u32 ep_ro);

#define sched_annotate_sleep() do { } while (0)

int kstrtou8(const char *s, unsigned int base, u8 *res);


/*********************
 ** linux/jiffies.h **
 *********************/

#include <legacy/lx_emul/jiffies.h>

enum {
	INITIAL_JIFFIES = 0,
};

static inline unsigned int jiffies_to_usecs(const unsigned long j) { return j * JIFFIES_TICK_US; }

void update_jiffies(void);


/******************
 ** linux/kmod.h **
 ******************/

int request_module(const char *name, ...);


/*******************************
 ** uapi/asm-generic/signal.h **
 *******************************/

enum { SIGPIPE };


/********************
 ** linux/bitmap.h **
 ********************/

void bitmap_fill(unsigned long *dst, int nbits);
void bitmap_zero(unsigned long *dst, int nbits);


/*******************
 ** linux/ctype.h **
 *******************/

#define __ismask(x) (_ctype[(int)(unsigned char)(x)])
#define isspace(c) ((unsigned char)c == 0x20U)


/*******************************
 ** asm-generic/scatterlist.h **
 *******************************/

struct scatterlist
{
	unsigned dummy;
};

struct page;

void sg_mark_end(struct scatterlist *sg);
void sg_set_buf(struct scatterlist *, const void *, unsigned int);
void sg_set_page(struct scatterlist *, struct page *, unsigned int,
                 unsigned int);


/******************
 ** linux/hash.h **
 ******************/

u32 hash_32(u32 val, unsigned int);
u32 hash32_ptr(const void *);


/****************************************************************
 ** arch/(*)/include/asm/(processor.h|segment.h|thread_info.h) **
 ****************************************************************/

typedef unsigned long mm_segment_t;

void prefetchw(const void *);
void prefetch(const void *);

void *current_text_addr(void);


/*************************************
 ** linux/byteorder/little_endian.h **
 *************************************/

#include <legacy/lx_emul/byteorder.h>

#define htons(x) __cpu_to_be16(x)
#define ntohs(x) __be16_to_cpu(x)
#define htonl(x) __cpu_to_be32(x)
#define ntohl(x) __be32_to_cpu(x)


/*********************************
 ** linux/unaligned/access_ok.h **
 *********************************/

static inline u16 get_unaligned_be16(const void *p) {
	return be16_to_cpup((__be16 *)p); }

static inline u32 get_unaligned_be32(const void *p) {
	return be32_to_cpup((__be32 *)p); }

static inline u32 __get_unaligned_cpu32(const void *p) {
	return *(u32*)p; }

static inline void put_unaligned_be32(u32 val, void *p) {
	*((__le32 *)p) = cpu_to_be32(val); }


/*********************
 ** linux/stringify **
 *********************/

#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)


/**************************
 ** uapi/linux/sysinfo.h **
 **************************/

struct sysinfo
{
	unsigned long totalram;
};


/****************************
 ** tools/perf/util/util.h **
 ****************************/

void dump_stack(void);


/********************
 ** linux/uidgid.h **
 ********************/

struct user_namespace;
#define GLOBAL_ROOT_UID (kuid_t)0

bool  gid_lte(kgid_t, kgid_t);
uid_t from_kuid_munged(struct user_namespace *, kuid_t);
gid_t from_kgid_munged(struct user_namespace *, kgid_t);
uid_t from_kuid(struct user_namespace *, kuid_t);
gid_t from_kgid(struct user_namespace *, kgid_t);
bool  uid_eq(kuid_t, kuid_t);
kgid_t make_kgid(struct user_namespace *from, gid_t gid);


/*****************
 ** linux/pid.h **
 ****************/

struct pid;

pid_t pid_vnr(struct pid *);
void put_pid(struct pid *);


/***********************
 ** linux/kmemcheck.h **
 ***********************/

#define kmemcheck_bitfield_begin(name)
#define kmemcheck_bitfield_end(name)
#define kmemcheck_annotate_bitfield(ptr, name)
#define kmemcheck_annotate_variable(var)


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


/********************
 ** linux/math64.h **
 ********************/

u64 div64_u64(u64, u64);
u64 div_u64(u64, u32);


/*************************
 ** asm-generic/cache.h **
 *************************/

enum {
	L1_CACHE_BYTES  = 32, //XXX is 64 for CA15 */
	SMP_CACHE_BYTES = L1_CACHE_BYTES,
};

#define ____cacheline_aligned_in_smp
#define ____cacheline_aligned


/********************
 ** linux/dcache.h **
 ********************/

#define __read_mostly
unsigned int full_name_hash(const unsigned char *, unsigned int);

/******************
 ** linux/numa.h **
 ******************/

enum { NUMA_NO_NODE = -1 };


/************************
 ** linux/jump_label.h **
 ************************/

struct static_key { unsigned dummy; };

#define STATIC_KEY_INIT_FALSE ((struct static_key) {})

void static_key_slow_inc(struct static_key *);
void static_key_slow_dec(struct static_key *);
bool static_key_false(struct static_key *);
bool static_key_enabled(struct static_key *);


/********************
 ** linux/poison.h **
 ********************/

#include <legacy/lx_emul/list.h>


/****************
 ** asm/page.h **
 ****************/

/*
 * For now, hardcoded
 */
#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE-1))

enum {
	PAGE_SHIFT = 12,
};

struct page
{
	int       pfmemalloc;
	int       mapping;
	atomic_t _count;
	void     *addr;
	unsigned long private;
} __attribute((packed));


/*************************
 ** linux/res_counter.h **
 *************************/

enum { RES_USAGE };
struct res_counter;
int res_counter_charge_nofail(struct res_counter *, unsigned long,
                              struct res_counter **);
u64 res_counter_uncharge(struct res_counter *, unsigned long);
u64 res_counter_read_u64(struct res_counter *counter, int member);


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

s64 percpu_counter_sum(struct percpu_counter *fbc);


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

struct sock;

enum { UNDER_LIMIT, SOFT_LIMIT, OVER_LIMIT };

void sock_update_memcg(struct sock *);
void sock_release_memcg(struct sock *);

struct cg_proto
{
	struct page_counter memory_allocated;
	struct percpu_counter sockets_allocated;
	int memory_pressure;
	long sysctl_mem[3];
};


/*********************
 ** mm/memcontrol.c **
 *********************/

struct mem_cgroup;


/**********************
 ** linux/mm-types.h **
 **********************/

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

struct page *virt_to_head_page(const void *x);
struct page *virt_to_page(const void *x);

void si_meminfo(struct sysinfo *);

bool page_is_pfmemalloc(struct page *page);

#define page_private(page)         ((page)->private)
#define set_page_private(page, v)  ((page)->private = (v))


/********************
 ** linux/mmzone.h **
 ********************/

enum { PAGE_ALLOC_COSTLY_ORDER = 3u };

int PageHighMem(struct page *);


/******************
 ** linux/swap.h **
 ******************/

unsigned long nr_free_buffer_pages(void);


/*****************
 ** linux/gfp.h **
 *****************/

#include <legacy/lx_emul/gfp.h>

enum {
	__GFP_COLD   = 0x00000100u,
	__GFP_REPEAT = 0x00000400u,
};

struct page *alloc_pages_node(int nid, gfp_t gfp_mask,
                              unsigned int order);

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

unsigned long get_zeroed_page(gfp_t gfp_mask);
#define free_page(p) kfree((void *)p)

bool gfp_pfmemalloc_allowed(gfp_t);
unsigned long __get_free_pages(gfp_t, unsigned int);
void free_pages(unsigned long, unsigned int);
void __free_page_frag(void *addr);
bool gfpflags_allow_blocking(const gfp_t gfp_flags);

void *__alloc_page_frag(struct page_frag_cache *nc, unsigned int fragsz, gfp_t gfp_mask);


/******************
 ** linux/slab.h **
 ******************/

enum {
	SLAB_HWCACHE_ALIGN  = 0x2000,
	SLAB_PANIC          = 0x40000,
	SLAB_DESTROY_BY_RCU = 0x80000,
 /* set to 1M, increase if necessary */
	KMALLOC_MAX_SIZE = 1 << 20,
};

void *kmalloc(size_t size, gfp_t flags);
void *kmalloc_array(size_t n, size_t size, gfp_t flags);
void *kzalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *krealloc(const void *, size_t, gfp_t);
void *kzalloc_node(size_t size, gfp_t flags, int node);
void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node);
void  kfree(const void *);
void  kvfree(const void *);
void  kzfree(const void *);
size_t ksize(void *objp); /* XXX is void const* */

struct kmem_cache;

struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
                                     unsigned long, void (*)(void *));
void *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void *kmem_cache_alloc_node(struct kmem_cache *, gfp_t flags, int node);

void kmem_cache_free(struct kmem_cache *cache, void *objp);
void kmem_cache_destroy(struct kmem_cache *);
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vmalloc(unsigned long size);
void *vzalloc(unsigned long size);
void vfree(const void *addr);


/**********************
 ** linux/highmem.h  **
 **********************/

void kunmap_atomic(void *);

static inline void *kmap(struct page *page) { return page_address(page); }
static inline void *kmap_atomic(struct page *page) { return kmap(page); }
void                kunmap(struct page *);


/*********************
 ** linux/bootmem.h **
 *********************/

void *alloc_large_system_hash(const char *tablename,
                              unsigned long bucketsize,
                              unsigned long numentries,
                              int scale,
                              int flags,
                              unsigned int *_hash_shift,
                              unsigned int *_hash_mask,
                              unsigned long low_limit,
                              unsigned long high_limit);


/************************
 ** linux/debug_lock.h **
 ************************/

void debug_check_no_locks_freed(const void *from, unsigned long len);


/**********************
 ** linux/spinlock.h **
 **********************/

#include <legacy/lx_emul/spinlock.h>


/*******************
 ** linux/mutex.h **
 *******************/

#include <legacy/lx_emul/mutex.h>

LX_MUTEX_INIT_DECLARE(dst_gc_mutex);
LX_MUTEX_INIT_DECLARE(proto_list_mutex);

#define dst_gc_mutex     LX_MUTEX(dst_gc_mutex)
#define proto_list_mutex LX_MUTEX(proto_list_mutex)


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

void read_lock(rwlock_t *);
void read_unlock(rwlock_t *);
void read_lock_bh(rwlock_t *);
void read_unlock_bh(rwlock_t *);


/*******************
 ** linux/rwsem.h **
 *******************/

#include <legacy/lx_emul/semaphore.h>


/*********************
 ** linux/seqlock.h **
 *********************/

typedef unsigned seqlock_t;

void seqlock_init (seqlock_t *);

#define __SEQLOCK_UNLOCKED(x) 0


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
	CLOCK_MONOTONIC = 1,

	MSEC_PER_SEC  = 1000L,
	USEC_PER_SEC  = MSEC_PER_SEC * 1000L,
	NSEC_PER_MSEC = 1000L * 1000L,
	NSEC_PER_USEC = NSEC_PER_MSEC * 1000L,
	NSEC_PER_SEC  = MSEC_PER_SEC * NSEC_PER_MSEC,
	USEC,
	USEC_PER_MSEC = 1000L,
};

unsigned long get_seconds(void);
void          getnstimeofday(struct timespec *);


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

ktime_t ns_to_ktime(u64 ns);


/*************************
 ** linux/timekeeping.h **
 *************************/

u64 ktime_get_ns(void);


/*******************
 ** linux/timer.h **
 *******************/

struct timer_list
{
	unsigned long expires;
	void        (*function)(unsigned long);
	unsigned long data;
	void         *timer;
};


#define TIMER_INITIALIZER(_function, _expires, _data) { \
	.function = (_function), \
	.expires  = (_expires), \
	.data     = (_data), \
}

void init_timer(struct timer_list *timer);
void add_timer(struct timer_list *timer);
int  mod_timer(struct timer_list *timer, unsigned long expires);
int mod_timer_pending(struct timer_list *timer, unsigned long expires);
int mod_timer_pinned(struct timer_list *timer, unsigned long expires);
void setup_timer(struct timer_list *timer,void (*function)(unsigned long),
                 unsigned long data);
int  timer_pending(const struct timer_list * timer);
int  del_timer(struct timer_list *timer);
void timer_stats_timer_clear_start_info(struct timer_list *);
unsigned long round_jiffies_relative(unsigned long);

unsigned long round_jiffies(unsigned long);
unsigned long round_jiffies_up(unsigned long);

#define del_timer_sync del_timer


/*********************
 ** linux/hrtimer.h **
 *********************/

enum hrtimer_restart
{
	HRTIMER_NORESTART,
};

enum hrtimer_mode {
	HRTIMER_MODE_REL_PINNED = 0x03,
};

struct hrtimer
{
	enum hrtimer_restart (*function)(struct hrtimer *);
};

typedef int clockid_t;

void hrtimer_init(struct hrtimer *timer, clockid_t which_clock, enum hrtimer_mode mode);
void hrtimer_start(struct hrtimer *timer, ktime_t tim, const enum hrtimer_mode mode);
int  hrtimer_cancel(struct hrtimer *timer);


/*******************
 ** linux/delay.h **
 *******************/

void msleep(unsigned int);
void ssleep(unsigned int);


/***********************
 ** linux/ratelimit.h **
 ***********************/

struct ratelimit_state
{
	int burst;
	int interval;
};

#define DEFINE_RATELIMIT_STATE(name, interval_init, burst_init) \
	struct ratelimit_state name = {                           \
		.interval       = interval_init,                        \
		.burst          = burst_init,                           \
	}

extern int ___ratelimit(struct ratelimit_state *, const char *);
#define __ratelimit(state) ___ratelimit(state, __func__)


/*******************
 ** linux/sched.h **
 *******************/

enum {
	PF_EXITING  = 0x4,
	PF_MEMALLOC = 0x800,

	//MAX_SCHEDULE_TIMEOUT = (~0U >> 1)
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

/* normally defined in asm/current.h, included by sched.h */
extern struct task_struct *current;

signed long schedule_timeout_interruptible(signed long);
long        schedule_timeout_uninterruptible(signed long);
signed long schedule_timeout(signed long timeout);

void  cond_resched(void);
void  cond_resched_softirq(void);
int   signal_pending(struct task_struct *);
int   send_sig(int, struct task_struct *, int);
void  tsk_restore_flags(struct task_struct *, unsigned long, unsigned long);
pid_t task_pid_nr(struct task_struct *);
void  schedule(void);
int   need_resched(void);
void  yield(void);
void __set_current_state(int);
void  set_current_state(int);
pid_t task_tgid_vnr(struct task_struct *);

u64 local_clock(void);


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


/****************************
 ** linux/rcu_list_nulls.h **
 ****************************/

#include <linux/list_nulls.h>

#define hlist_nulls_add_head_rcu hlist_nulls_add_head

static inline void hlist_nulls_del_init_rcu(struct hlist_nulls_node *n)
{
	if (!hlist_nulls_unhashed(n)) {
		__hlist_nulls_del(n);
		n->pprev = NULL;
	}
}


/*********************
 ** linux/lockdep.h **
 *********************/

enum {
	SINGLE_DEPTH_NESTING = 1,
};

#define lockdep_set_class(lock, key)
#define lockdep_set_class_and_name(lock, key, name)
#define mutex_acquire(l, s, t, i)
#define mutex_release(l, n, i)

struct lock_class_key { };
struct lockdep_map { };

void lockdep_init_map(struct lockdep_map *, const char *,
                      struct lock_class_key *, int);


/*****************
 ** linux/smp.h **
 *****************/

#define raw_smp_processor_id() 0
#define smp_processor_id() 0
#define put_cpu()

typedef void (*smp_call_func_t)(void *info);

int on_each_cpu(smp_call_func_t, void *, int);


/**********************
 ** linux/rcupdate.h **
 **********************/

#define kfree_rcu(ptr, offset) kfree(ptr)

#define rcu_dereference(p) p
#define rcu_dereference_bh(p) p
#define rcu_dereference_check(p, c) p
#define rcu_dereference_protected(p, c) p
#define rcu_dereference_raw(p) p
#define rcu_dereference_rtnl(p) p
#define rcu_dereference_index_check(p, c) p

#define rcu_assign_pointer(p, v) p = v
#define rcu_access_pointer(p) p

void rcu_read_lock(void);
void rcu_read_unlock(void);

void rcu_read_lock_bh(void);
void rcu_read_unlock_bh(void);

void synchronize_rcu(void);

static inline int rcu_read_lock_held(void) { return 1; }
static inline int rcu_read_lock_bh_held(void) { return 1; }

static inline void call_rcu(struct rcu_head *head,
                            void (*func)(struct rcu_head *head))
{ func(head); }

#define RCU_INIT_POINTER(p, v) p = (typeof(*v) *)v


/*********************
 ** linux/rculist.h **
 *********************/

#define hlist_first_rcu(head) (*((struct hlist_node **)(&(head)->first)))

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

#define list_entry_rcu(ptr, type, member) container_of(ptr, type, member)

#define list_del_rcu  list_del
#define hlist_del_rcu hlist_del

#define hlist_del_init_rcu hlist_del_init

#define free_percpu(pdata) kfree(pdata)

void hlist_add_after_rcu(struct hlist_node *, struct hlist_node *);
void hlist_add_before_rcu(struct hlist_node *,struct hlist_node *);

void list_replace_rcu(struct list_head *, struct list_head *);
void hlist_replace_rcu(struct hlist_node *old, struct hlist_node *new);
void hlist_add_behind_rcu(struct hlist_node *n, struct hlist_node *prev);


/***************************
 ** linux/rculist_nulls.h **
 ***************************/

#define hlist_nulls_for_each_entry_rcu(tpos, pos, head, member) \
	hlist_nulls_for_each_entry(tpos, pos, head, member)


/*********************
 ** linux/rcutree.h **
 *********************/

void rcu_barrier(void);
void synchronize_rcu_expedited(void);


/***********************
 ** linux/hashtable.h **
 ***********************/

#define HASH_SIZE(name) (ARRAY_SIZE(name))

#define HLIST_HEAD_INIT { .first = NULL }

#define DEFINE_HASHTABLE(name, bits)                        \
	struct hlist_head name[1 << (bits)] =                   \
		{ [0 ... ((1 << (bits)) - 1)] = HLIST_HEAD_INIT }


/*************************
 ** linux/percpu-defs.h **
 *************************/

#define DECLARE_PER_CPU_ALIGNED(type, name) \
	extern typeof(type) name

#define DECLARE_PER_CPU(type, name) \
	extern typeof(type) name

#define DEFINE_PER_CPU_ALIGNED(type, name) \
	typeof(type) name

#define DEFINE_PER_CPU(type, name) \
	typeof(type) name

#define EXPORT_PER_CPU_SYMBOL(x)


/********************
 ** linux/percpu.h **
 ********************/

void *__alloc_percpu(size_t size, size_t align);

#define __alloc_percpu_gfp(size, align, gfp) __alloc_percpu(size, align)

#define alloc_percpu_gfp(type, gfp) \
	(typeof(type) __percpu *)__alloc_percpu_gfp(sizeof(type), __alignof__(type), gfp)

#define alloc_percpu(type) \
	(typeof(type) __percpu *)__alloc_percpu(sizeof(type), __alignof__(type))

#define per_cpu(var, cpu) var
#define per_cpu_ptr(ptr, cpu)   ({ (void)(cpu);(typeof(*(ptr)) *)(ptr); })
#define get_cpu_var(var) var
#define put_cpu_var(var) ({ (void)&(var); })
#define __get_cpu_var(var) var
#define __this_cpu_ptr(ptr) per_cpu_ptr(ptr, 0)
#define this_cpu_ptr(ptr) per_cpu_ptr(ptr, 0)
#define __this_cpu_write(pcp, val) pcp = val
#define __this_cpu_read(pcp) pcp
#define this_cpu_read(pcp) __this_cpu_read(pcp)

#define this_cpu_inc(pcp) pcp += 1
#define this_cpu_dec(pcp) pcp -= 1

#define __this_cpu_inc(pcp) this_cpu_inc(pcp)
#define __this_cpu_dec(pcp) this_cpu_dec(pcp)

#define this_cpu_add(pcp, val) pcp += val
#define __this_cpu_add(pcp, val) this_cpu_add(pcp, val)
#define get_cpu() 0

#define raw_cpu_ptr(ptr) per_cpu_ptr(ptr, 0)
#define raw_cpu_inc(pcp) pcp += 1


/*********************
 ** linux/cpumask.h **
 *********************/

#define num_online_cpus() 1U

unsigned num_possible_cpus(void);


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


/*******************
 ** linux/preempt **
 *******************/

void preempt_enable(void);
void preempt_disable(void);


/*********************
 ** linux/kobject.h **
 *********************/

enum kobject_action
{
	KOBJ_ADD,
	KOBJ_REMOVE,
};

struct kobject { };

void kobject_put(struct kobject *);
int kobject_uevent(struct kobject *, enum kobject_action);


/***********************
 ** linux/interrupt.h **
 ***********************/

enum {
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	NET_SOFTIRQS,
};

struct tasklet_struct;
struct softirq_action { };

void raise_softirq_irqoff(unsigned int);
void __raise_softirq_irqoff(unsigned int );

bool irqs_disabled(void);
void do_softirq(void);

void open_softirq(int, void (*)(struct softirq_action *));

void tasklet_init(struct tasklet_struct *, void (*)(unsigned long),
                  unsigned long);
void tasklet_schedule(struct tasklet_struct *);

struct task_struct *this_cpu_ksoftirqd(void);


/***********************
 ** /linux/irqflags.h **
 ***********************/

void local_irq_save(unsigned long);
void local_irq_restore(unsigned long);
void local_irq_enable(void);
void local_irq_disable(void);


/*********************
 ** linux/hardirq.h **
 *********************/

int in_softirq(void);
int in_irq(void);


/*************************
 ** linux/irq_cpustat.h **
 *************************/

bool local_softirq_pending(void);


/*************************
 ** linux/bottom_half.h **
 *************************/

void local_bh_disable(void);
void local_bh_enable(void);


/********************
 ** linux/string.h **
 ********************/

#include <legacy/lx_emul/string.h>

char *strnchr(const char *, size_t, int);


/***************************
 ** asm-generic/uaccess.h **
 ***************************/

enum { VERIFY_READ = 0 };

static inline
long copy_from_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}

static inline
long copy_to_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}

int access_ok(int, const void *, unsigned long);

#define get_user(src, dst) ({ \
	src = *dst; \
	0; })

#define put_user(x, ptr) ({ \
	*ptr = x; \
	0; })

#define __copy_from_user copy_from_user
#define __copy_from_user_nocache copy_from_user

long strncpy_from_user(char *, const char *src, long);

mm_segment_t get_fs(void);
mm_segment_t get_ds(void);

void set_fs(mm_segment_t);


/*****************************
 ** uapi/linux/capability.h **
 *****************************/

enum {
	CAP_NET_BIND_SERVICE = 10,
	CAP_NET_BROADCAST    = 11,
	CAP_NET_ADMIN        = 12,
	CAP_NET_RAW          = 13,
};


/*************************
 ** linux/capability.h **
 *************************/

bool capable(int cap);
bool ns_capable(struct user_namespace *, int);
struct file;
bool file_ns_capable(const struct file *file, struct user_namespace *ns, int cap);


/********************
 ** linux/sysctl.h **
 ********************/

struct ctl_table;

typedef int proc_handler (struct ctl_table *ctl, int write,
                          void __user *buffer, size_t *lenp, loff_t *ppos);


/************************
 ** fs/proc/internal.h **
 ************************/

struct proc_dir_entry { };


/*******************
 ** linux/proc_fs **
 *******************/

void remove_proc_entry(const char *, struct proc_dir_entry *);


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


/********************
 ** linux/pm_qos.h **
 ********************/

struct pm_qos_request { };


/*********************************
 ** (uapi|linux|kernel)/audit.h **
 *********************************/

enum {
	AUDIT_ANOM_PROMISCUOUS = 1700,
};

extern int audit_enabled;

struct audit_context { };

void   audit_log(struct audit_context *, gfp_t, int, const char *, ...);
kuid_t audit_get_loginuid(struct task_struct *);
int    audit_get_sessionid(struct task_struct *);


/********************
 ** linux/device.h **
 ********************/

struct device_driver
{
	char const      *name;
};


struct device
{
	struct kobject                 kobj;
	struct device                 *parent;
	struct device_driver          *driver;
};

struct class_attribute { };

const char *dev_driver_string(const struct device *);
const char *dev_name(const struct device *);
int         device_rename(struct device *, const char *);
void        put_device(struct device *);

int dev_printk_emit(int, const struct device *, const char *, ...);


/*************************
 ** linux/dma-direction **
 *************************/

enum dma_data_direction { D = 1};


/*************************
 ** linux/dma-mapping.h **
 *************************/

dma_addr_t dma_map_page(struct device *dev, struct page *page,
                        size_t offset, size_t size,
                        enum dma_data_direction dir);


/***********************
 ** linux/dmaenging.h **
 ***********************/

void net_dmaengine_get(void);
void net_dmaengine_put(void);


/*****************
 ** linux/phy.h **
 *****************/

struct ethtool_ts_info;
struct ethtool_modinfo;
struct ethtool_eeprom;

struct phy_device;

struct phy_driver
{
	int (*ts_info)(struct phy_device *phydev, struct ethtool_ts_info *ti);
	int (*module_info)(struct phy_device *dev, struct ethtool_modinfo *modinfo);
	int (*module_eeprom)(struct phy_device *dev, struct ethtool_eeprom *ee, u8 *data);
};

struct phy_device
{
	struct phy_driver *drv;
};


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


/***********************
 ** linux/workqueue.h **
 ***********************/

#include <legacy/lx_emul/work.h>

#define wait_queue_t wait_queue_entry_t

void INIT_DEFERRABLE_WORK(struct delayed_work *, void (*func)(struct work_struct *));

#define system_power_efficient_wq 0
#define work_pending(work) 0


/******************
 ** linux/wait.h **
 ******************/

long wait_woken(wait_queue_t *wait, unsigned mode, long timeout);


/******************
 ** linux/poll.h **
 ******************/

typedef struct poll_table_struct { } poll_table;

struct file;
void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p);
bool poll_does_not_wait(const poll_table *);


/****************************
 ** linux/user_namespace.h **
 ****************************/

struct user_namespace { };


/******************
 ** linux/cred.h **
 ******************/

enum {
	NGROUPS_PER_BLOCK = PAGE_SIZE /  sizeof(kgid_t)
};

struct cred
{
	struct user_namespace *user_ns;
	kuid_t euid;
	kgid_t egid;
};

struct group_info
{
	int     ngroups;
	int     nblocks;
	kgid_t *blocks[0]; 
};

extern struct user_namespace init_user_ns;
#define current_user_ns() (&init_user_ns)

struct group_info *get_current_groups();
void put_cred(const struct cred *);
void put_group_info(struct group_info*);

static inline void current_uid_gid(kuid_t *u, kgid_t *g)
{
	*u = 0;
	*g =0;
}

kgid_t current_egid(void);


/*************************
 ** asm-generic/fcntl.h **
 *************************/

enum { O_NONBLOCK = 0x4000 };


/*********************
 ** uapi/linux/fs.h **
 *********************/

enum {
	NR_FILE = 8192,
};


/****************
 ** linux/fs.h **
 ****************/

struct fown_struct { };

struct file
{
	unsigned int       f_flags;
	const struct cred *f_cred;
	struct fown_struct  f_owner;
};

typedef struct
{
	size_t  count;
	union
	{
		void *data;
	} arg;
} read_descriptor_t;

struct inode
{
	umode_t       i_mode;
	kuid_t        i_uid;
	unsigned long i_ino;
};

struct inode *file_inode(struct file *f);
int send_sigurg(struct fown_struct *);


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

extern const struct pipe_buf_operations nosteal_pipe_buf_ops;


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


/*****************
 ** linux/aio.h **
 *****************/

struct kiocb
{
	void *private;
};


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

int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len);

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

static inline size_t iov_iter_count(struct iov_iter *i) { return i->count; }

size_t copy_to_iter(void *addr, size_t bytes, struct iov_iter *i);
size_t copy_from_iter(void *addr, size_t bytes, struct iov_iter *i);
size_t copy_from_iter_nocache(void *addr, size_t bytes, struct iov_iter *i);

size_t copy_page_to_iter(struct page *page, size_t offset, size_t bytes, struct iov_iter *i);
size_t copy_page_from_iter(struct page *page, size_t offset, size_t bytes, struct iov_iter *i);

void iov_iter_advance(struct iov_iter *i, size_t bytes);
ssize_t iov_iter_get_pages(struct iov_iter *i, struct page **pages,
                           size_t maxsize, unsigned maxpages, size_t *start);

size_t csum_and_copy_to_iter(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);
size_t csum_and_copy_from_iter(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);

bool iter_is_iovec(struct iov_iter *i);


/*******************************
 ** uapi/asm-generic/ioctls.h **
 *******************************/

enum { 
	TIOCOUTQ = 0x5411,
	FIONREAD = 0x541b,
};


/*********************
 ** linux/utsname.h **
 *********************/

enum { __NEW_UTS_LEN = 64 };

struct new_utsname
{
	char nodename[65];
	char domainname[65];
};

struct uts_name {
	struct new_utsname name;
};

extern struct uts_name init_uts_ns;

struct new_utsname *init_utsname(void);
struct new_utsname *utsname(void);


/*************************
 ** uapi/linux/filter.h **
 *************************/

struct sock_fprog { };


/********************
 ** linux/filter.h **
 ********************/

struct sk_buff;
struct sock_filter;
struct sk_filter
{
	atomic_t        refcnt;
	struct rcu_head rcu;
};

unsigned int sk_filter_len(const struct sk_filter *);
int sk_filter(struct sock *, struct sk_buff *);
bool sk_filter_charge(struct sock *sk, struct sk_filter *fp);
void sk_filter_uncharge(struct sock *sk, struct sk_filter *fp);
int sk_attach_filter(struct sock_fprog *, struct sock *);
int sk_detach_filter(struct sock *);
int sk_get_filter(struct sock *, struct sock_filter *, unsigned);
int sk_attach_bpf(u32 ufd, struct sock *sk);

int bpf_tell_extensions(void);


/*****************************
 ** uapi/linux/hdlc/ioctl.h **
 *****************************/

typedef struct { } fr_proto;
typedef struct { } fr_proto_pvc;
typedef struct { } fr_proto_pvc_info;
typedef struct { } cisco_proto;
typedef struct { } raw_hdlc_proto;
typedef struct { } te1_settings;
typedef struct { } sync_serial_settings;


/**********************
 ** linux/interrupt.h **
 **********************/

struct tasklet_struct
{ };


/********************
 ** linux/crypto.h **
 ********************/

struct hash_desc { };


/************************
 ** linux/cryptohash.h **
 ************************/

enum { 
	SHA_DIGEST_WORDS    = 5,
	SHA_MESSAGE_BYTES   = 512 * 8,
	SHA_WORKSPACE_WORDS = 16,
};

void sha_transform(__u32 *, const char *, __u32 *);


/***********************
 ** linux/rtnetlink.h **
 ***********************/

struct net_device;
struct nlmsghdr;
struct net;
struct sk_buff;
struct netlink_callback;
struct rtnl_af_ops;
struct dst_entry;

typedef int (*rtnl_doit_func)(struct sk_buff *, struct nlmsghdr *, void *);
typedef int (*rtnl_dumpit_func)(struct sk_buff *, struct netlink_callback *);
typedef u16 (*rtnl_calcit_func)(struct sk_buff *, struct nlmsghdr *);


void rtnetlink_init(void);
void rtnl_register(int protocol, int msgtype,
                   rtnl_doit_func, rtnl_dumpit_func,
                   rtnl_calcit_func);
int  rtnl_af_register(struct rtnl_af_ops *);

struct netdev_queue *dev_ingress_queue(struct net_device *dev);
void rtnl_notify(struct sk_buff *, struct net *, u32, u32, struct nlmsghdr *,
                 gfp_t);
int  rtnl_unicast(struct sk_buff *, struct net *, u32);
int  rtnetlink_put_metrics(struct sk_buff *, u32 *);
void rtnl_set_sk_err(struct net *, u32, int);
void ASSERT_RTNL(void);
void rtnl_lock(void);
void rtnl_unlock(void);
void __rtnl_unlock(void);
int  rtnl_is_locked(void);
int  rtnl_put_cacheinfo(struct sk_buff *, struct dst_entry *, u32, long, u32);

bool lockdep_rtnl_is_held(void);

struct sk_buff *rtmsg_ifinfo_build_skb(int type, struct net_device *dev, unsigned change, gfp_t flags);
void rtmsg_ifinfo_send(struct sk_buff *skb, struct net_device *dev, gfp_t flags);


/***********************
 ** linux/genetlink.h **
 ***********************/

extern atomic_t genl_sk_destructing_cnt;
extern wait_queue_head_t genl_sk_destructing_waitq;


/*********************
 ** net/flow_keys.h **
 *********************/

enum flow_dissector_key_id
{
	FLOW_DISSECTOR_KEY_IPV4_ADDRS,
};

struct flow_dissector_key_control
{
	u16 thoff;
	u16 addr_type;
	u32 flags;
};

struct flow_dissector_key_ipv4_addrs {
	/* (src,dst) must be grouped, in the same way than in IP header */
	__be32 src;
	__be32 dst;
};

struct flow_dissector_key_addrs {
	union {
		struct flow_dissector_key_ipv4_addrs v4addrs;
	};
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
	struct flow_dissector_key_addrs   addrs;
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


/********************
 ** net/netevent.h **
 ********************/

enum netevent_notif_type {
	NETEVENT_NEIGH_UPDATE = 1, /* arg is struct neighbour ptr */
};

int call_netevent_notifiers(unsigned long, void *);


/***************
 ** net/scm.h **
 ***************/

struct scm_creds
{
};

struct scm_cookie
{
	struct scm_creds creds; 
};

struct msghdr;
struct socket;

int  scm_send(struct socket *, struct msghdr *, struct scm_cookie *, bool);
void scm_recv(struct socket *, struct msghdr *, struct scm_cookie *, int);
void scm_destroy(struct scm_cookie *);


/*********************
 ** net/fib_rules.h **
 *********************/

enum
{
	FIB_LOOKUP_NOREF            = 1,
	FIB_LOOKUP_IGNORE_LINKSTATE = 2,
};


/****************************
 ** linux/u64_stats_sync.h **
 ****************************/

struct u64_stats_sync { unsigned dummy; };

void u64_stats_init(struct u64_stats_sync *syncp);
void u64_stats_update_begin(struct u64_stats_sync *syncp);
void u64_stats_update_end(struct u64_stats_sync *syncp);

unsigned int u64_stats_fetch_begin_irq(const struct u64_stats_sync *p);
bool u64_stats_fetch_retry_irq(const struct u64_stats_sync *p, unsigned int s);


/**********************
 ** net/netns/core.h **
 **********************/

struct netns_core
{
	int sysctl_somaxconn;
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

extern struct net init_net;

struct net
{
	struct list_head         list;
	struct list_head         exit_list;
	struct list_head         dev_base_head;
	struct hlist_head       *dev_name_head;
	struct hlist_head       *dev_index_head;
	unsigned int             dev_base_seq;
	int                      ifindex;
	unsigned int             dev_unreg_count;
	struct net_device       *loopback_dev;
	struct netns_core        core;
	struct user_namespace   *user_ns;
	struct proc_dir_entry   *proc_net_stat;
	struct sock             *rtnl;
	struct netns_mib         mib;
	struct netns_ipv4        ipv4;
	atomic_t                 rt_genid;
	atomic_t                 fnhe_genid;
};


struct pernet_operations
{
	int (*init)(struct net *net);
	void (*exit)(struct net *net);
	void (*exit_batch)(struct list_head *net_exit_list);
};

extern struct list_head net_namespace_list;

#define __net_initdata
#define __net_init
#define __net_exit

#define for_each_net(VAR)                               \
	for(VAR = &init_net; VAR; VAR = 0)

#define read_pnet(pnet) (&init_net)
#define write_pnet(pnet, net) do { (void)(net);} while (0)

static inline struct net *hold_net(struct net *net) { return net; }
static inline struct net *get_net(struct net *net) { return net; }
static inline void put_net(struct net *net) { }
static inline int net_eq(const struct net *net1, const struct net *net2) {
	return net1 == net2; }

typedef struct { unsigned dummy; } possible_net_t;

struct net *get_net_ns_by_pid(pid_t pid);
struct net *get_net_ns_by_fd(int pid);

int  register_pernet_subsys(struct pernet_operations *);
int  register_pernet_device(struct pernet_operations *);
void release_net(struct net *net);

int  rt_genid(struct net *);
int  rt_genid_ipv4(struct net *net);
void rt_genid_bump(struct net *);
void rt_genid_bump_ipv4(struct net *net);

int fnhe_genid(struct net *net);

int peernet2id(struct net *net, struct net *peer);
bool peernet_has_id(struct net *net, struct net *peer);


/**************************
 ** linux/seq_file_net.h **
 **************************/

struct seq_net_private {
	struct net *net;
};

struct seq_operations { };


/**************************
 ** linux/seq_file.h **
 **************************/

struct seq_file { };


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

unsigned raw_seqcount_begin(const seqcount_t *s);
int read_seqcount_retry(const seqcount_t *s, unsigned start);


/**********************
 ** net/secure_seq.h **
 **********************/

u32   secure_ipv4_port_ephemeral(__be32, __be32, __be16);
__u32 secure_tcp_sequence_number(__be32, __be32, __be16, __be16);

__u32 secure_ip_id(__be32 );
__u32 secure_ipv6_id(const __be32 [4]);


/**********************
 ** linux/notifier.h **
 **********************/

enum {
	NOTIFY_DONE      = 0,
	NOTIFY_OK        = 0x1,
	NOTIFY_STOP_MASK = 0x8000,

	NETLINK_URELEASE = 0x1,
};


struct notifier_block;

typedef int (*notifier_fn_t)(struct notifier_block *nb,
                             unsigned long action, void *data);

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

struct blocking_notifier_head {
	struct rw_semaphore rwsem;
	struct notifier_block *head;
};

struct atomic_notifier_head {
	struct notifier_block *head;

};

int atomic_notifier_chain_register(struct atomic_notifier_head *,
                                   struct notifier_block *);

int atomic_notifier_chain_unregister(struct atomic_notifier_head *,
                                     struct notifier_block *);

int atomic_notifier_call_chain(struct atomic_notifier_head *,
                               unsigned long, void *);

int raw_notifier_chain_register(struct raw_notifier_head *nh,
                                struct notifier_block *n);

int raw_notifier_chain_unregister(struct raw_notifier_head *nh,
                                  struct notifier_block *nb);

int raw_notifier_call_chain(struct raw_notifier_head *nh,
                            unsigned long val, void *v);

int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                     struct notifier_block *n);

int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
                                       struct notifier_block *nb);

int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long val, void *v);

int notifier_to_errno(int);
int notifier_from_errno(int);

#define BLOCKING_NOTIFIER_INIT(name) { \
	.rwsem = __RWSEM_INITIALIZER((name).rwsem), .head = NULL }

#define BLOCKING_NOTIFIER_HEAD(name) \
	struct blocking_notifier_head name = BLOCKING_NOTIFIER_INIT(name)

#define ATOMIC_NOTIFIER_INIT(name) { \
	.head = NULL }

#define ATOMIC_NOTIFIER_HEAD(name) \
	struct atomic_notifier_head name =   ATOMIC_NOTIFIER_INIT(name)


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

__wsum remcsum_adjust(void *ptr, __wsum csum, int start, int offset);
void csum_replace4(__sum16 *sum, __be32 from, __be32 to);


/*********************
 ** linux/if_vlan.h **
 *********************/

enum {
	VLAN_HLEN     =  4,
	VLAN_ETH_HLEN = 18,
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

static inline
struct net_device *vlan_dev_real_dev(const struct net_device *dev)
{
	return NULL;
}

#define vlan_tx_tag_get(__skb) 0
struct sk_buff *__vlan_put_tag(struct sk_buff *, u16);
struct sk_buff *vlan_untag(struct sk_buff *);
int             is_vlan_dev(struct net_device *);
u16             vlan_tx_tag_present(struct sk_buff *);
bool            vlan_do_receive(struct sk_buff **);
bool            vlan_tx_nonzero_tag_present(struct sk_buff *);

enum {
	VLAN_VID_MASK    = 0x0fff,
	VLAN_CFI_MASK    = 0x1000, /* Canonical Format Indicator */
	VLAN_TAG_PRESENT = VLAN_CFI_MASK,
};

#define skb_vlan_tag_get(__skb)      ((__skb)->vlan_tci & ~VLAN_TAG_PRESENT)
#define skb_vlan_tag_present(__skb)  ((__skb)->vlan_tci & VLAN_TAG_PRESENT)
#define skb_vlan_tag_get_id(__skb)   ((__skb)->vlan_tci & VLAN_VID_MASK)

__be16 __vlan_get_protocol(struct sk_buff *skb, __be16 type, int *depth);
void __vlan_hwaccel_put_tag(struct sk_buff *skb, __be16 vlan_proto, u16 vlan_tci);
int __vlan_insert_tag(struct sk_buff *skb, __be16 vlan_proto, u16 vlan_tci);


typedef u64 netdev_features_t; /* XXX actually defined in netdev_features.h */

bool skb_vlan_tagged(const struct sk_buff *skb);
netdev_features_t vlan_features_check(const struct sk_buff *skb, netdev_features_t features);
bool vlan_hw_offload_capable(netdev_features_t features, __be16 proto);
struct sk_buff *__vlan_hwaccel_push_inside(struct sk_buff *skb);
void vlan_set_encap_proto(struct sk_buff *skb, struct vlan_hdr *vhdr);


/*****************************
 ** uapi/linux/if_bonding.h **
 *****************************/

typedef struct ifbond { unsigned dummy; } ifbond;

typedef struct ifslave { unsigned dummy; } ifslave;


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

static inline __wsum csum_partial_ext(const void *buff, int len, __wsum sum)
{
	return csum_partial(buff, len, sum);
}

__wsum csum_block_add_ext(__wsum csum, __wsum csum2, int offset, int len);

void csum_replace2(__sum16 *, __be16, __be16);

static inline void remcsum_unadjust(__sum16 *psum, __wsum delta) {
	*psum = csum_fold(csum_sub(delta, *psum)); }


/*****************************
 ** uapi/linux/net_tstamp.h **
 *****************************/

#include <uapi/linux/if_link.h>
#include <net/netlink.h>

enum {
	SOF_TIMESTAMPING_TX_HARDWARE  = 1 << 0,
	SOF_TIMESTAMPING_TX_SOFTWARE  = 1 << 1,
	SOF_TIMESTAMPING_RX_HARDWARE  = 1 << 2,
	SOF_TIMESTAMPING_RX_SOFTWARE  = 1 << 3,
	SOF_TIMESTAMPING_SOFTWARE     = 1 << 4,
	SOF_TIMESTAMPING_SYS_HARDWARE = 1 << 5,
	SOF_TIMESTAMPING_RAW_HARDWARE = 1 << 6,
	SOF_TIMESTAMPING_OPT_ID       = 1 << 7,
	SOF_TIMESTAMPING_TX_ACK       = 1 << 9,
	SOF_TIMESTAMPING_OPT_CMSG     = 1 << 10,
	SOF_TIMESTAMPING_OPT_TSONLY   = 1 << 11,
	SOF_TIMESTAMPING_MASK = (SOF_TIMESTAMPING_RAW_HARDWARE - 1) |
	                         SOF_TIMESTAMPING_RAW_HARDWARE,
};


struct rtnl_link_ops
{
	struct list_head        list;
	const char             *kind;
	size_t                  priv_size;
	void                  (*setup)(struct net_device *dev);
	int                      maxtype;
	const struct nla_policy *policy;
	void                  (*dellink)(struct net_device *dev,
	                                 struct list_head *head);
	size_t                (*get_size)(const struct net_device *dev);
	size_t                (*get_xstats_size)(const struct net_device *dev);

	int                   (*fill_info)(struct sk_buff *skb,
	                                   const struct net_device *dev);
	int                   (*fill_xstats)(struct sk_buff *skb,
	                                     const struct net_device *dev);
	unsigned int          (*get_num_tx_queues)(void);
	unsigned int          (*get_num_rx_queues)(void);
	int                   (*changelink)(struct net_device *dev,
	                                    struct nlattr *tb[],
	                                    struct nlattr *data[]);
	int                     (*validate)(struct nlattr *tb[],
	                                    struct nlattr *data[]);
	int                     (*newlink)(struct net *src_net,
	                                   struct net_device *dev,
	                                   struct nlattr *tb[],
	                                   struct nlattr *data[]);
};

struct rtnl_af_ops
{
	struct list_head    list;
	int                 family;

	size_t             (*get_link_af_size)(const struct net_device *dev);
	int                (*fill_link_af)(struct sk_buff *skb,
	                                   const struct net_device *dev);
	int                (*validate_link_af)(const struct net_device *dev,
	                                       const struct nlattr *attr);
	int                (*set_link_af)(struct net_device *dev,
	                                  const struct nlattr *attr);
};

typedef int (*rtnl_doit_func)(struct sk_buff *, struct nlmsghdr *, void *);
typedef int (*rtnl_dumpit_func)(struct sk_buff *, struct netlink_callback *);
typedef u16 (*rtnl_calcit_func)(struct sk_buff *, struct nlmsghdr *);

#define rtnl_dereference(p) p

extern const struct nla_policy ifla_policy[IFLA_MAX+1];

void rtmsg_ifinfo(int type, struct net_device *dev, unsigned change, gfp_t flags);


/**********************
 ** net/gent_stats.h **
 **********************/

struct gnet_dump { };


/***************
 ** net/tcp.h **
 ***************/

enum {
	TFO_SERVER_ENABLE      = 2,
	TFO_SERVER_WO_SOCKOPT1 = 0x400,
	TFO_SERVER_WO_SOCKOPT2 = 0x800,
};

extern int sysctl_tcp_fastopen;


/**********************
 ** net/ip_tunnels.h **
 **********************/

enum
{
	IP_TUNNEL_INFO_TX   = 0x01,
	IP_TUNNEL_INFO_IPV6 = 0x02,
};

struct ip_tunnel_key {
	__be64          tun_id;
	union {
		struct {
			__be32  src;
			__be32  dst;
		} ipv4;
		struct {
			struct in6_addr src;
			struct in6_addr dst;
		} ipv6;
	} u;
	__be16          tun_flags;
	u8          tos;        /* TOS for IPv4, TC for IPv6 */
	u8          ttl;        /* TTL for IPv4, HL for IPv6 */
	__be16          tp_src;
	__be16          tp_dst;
};

struct ip_tunnel_info
{
	struct ip_tunnel_key key;

	u8 options_len;
	u8 mode;
};

void ip_tunnel_key_init(struct ip_tunnel_key *key,
		__be32 saddr, __be32 daddr,
		u8 tos, u8 ttl,
		__be16 tp_src, __be16 tp_dst,
		__be64 tun_id, __be16 tun_flags);

struct lwtunnel_state;
struct ip_tunnel_info *lwt_tun_info(struct lwtunnel_state *lwtstate);
struct metadata_dst *iptunnel_metadata_reply(struct metadata_dst *md, gfp_t flags);

void ip_tunnel_core_init(void);


/********************************
 ** uapi/asm-generic/sockios.h **
 ********************************/

enum {
	SIOCATMARK   = 0x8905,
	SIOCGSTAMP   = 0x8906,
	SIOCGSTAMPNS = 0x8907,
};


/**********************
 ** net/cls_cgroup.h **
 **********************/

void sock_update_classid(struct sock *);


/****************
 ** linux/ip.h **
 ****************/

#include <uapi/linux/ip.h>

struct ip_hdr;

struct iphdr *ip_hdr(const struct sk_buff *skb);


/*************************
 ** uapi/linux/icmpv6.h **
 *************************/

enum
{
	ICMPV6_ECHO_REQUEST = 128,
};


struct icmp6hdr
{
	__u8 icmp6_type;
	__u8 icmp6_code;
};


/********************
 ** linux/icmpv6.h **
 ********************/

struct icmp6hdr *icmp6_hdr(const struct sk_buff *skb);


/***********************
 ** uapi/linux/ipv6.h **
 ***********************/

struct ipv6hdr
{
	__u8 priority:4, version:4;
	__be16              payload_len;
	__u8                hop_limit;
	__u8                nexthdr;
	struct  in6_addr    saddr;
	struct  in6_addr    daddr;
};

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


static inline __u8 ipv6_get_dsfield(const struct ipv6hdr *ipv6h);

struct ipv6hdr *ipv6_hdr(const struct sk_buff *skb);


/********************************
 ** uapi/linux/netfilter_arp.h **
 ********************************/

enum {
	NF_ARP_IN  = 0,
	NF_ARP_OUT = 1,
};


/***************************
 ** uapi/linux/lwtunnel.h **
 ***************************/

enum lwtunnel_encap_types
{
	LWTUNNEL_ENCAP_NONE,
};


/********************
 ** net/lwtunnel.h **
 ********************/

struct lwtunnel_state
{
	int (*orig_output)(struct net *net, struct sock *sk, struct sk_buff *skb);
	int (*orig_input)(struct sk_buff *);
};

static inline int lwtunnel_output(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	return -EOPNOTSUPP;
}

static inline int lwtunnel_input(struct sk_buff *skb)
{
	return -EOPNOTSUPP;
}

static inline int lwtunnel_build_state(struct net_device *dev, u16 encap_type,
                                       struct nlattr *encap,
                                       unsigned int family, const void *cfg,
                                       struct lwtunnel_state **lws)
{
	return -EOPNOTSUPP;
}

void lwtstate_free(struct lwtunnel_state *lws);
void lwtstate_put(struct lwtunnel_state *lws);
int lwtunnel_fill_encap(struct sk_buff *skb, struct lwtunnel_state *lwtstate);

static inline bool lwtunnel_output_redirect(struct lwtunnel_state *lwtstate) { return false; }
static inline bool lwtunnel_input_redirect(struct lwtunnel_state *lwtstate) { return false; }

struct lwtunnel_state * lwtstate_get(struct lwtunnel_state *lws);
int lwtunnel_cmp_encap(struct lwtunnel_state *a, struct lwtunnel_state *b);
int lwtunnel_get_encap_size(struct lwtunnel_state *lwtstate);


/****************
 ** net/ax25.h **
 ****************/

enum { AX25_P_IP = 0xcc };


/********************
 ** net/addrconf.h **
 ********************/

enum {
	ADDR_CHECK_FREQUENCY      = (120*HZ),
	ADDRCONF_TIMER_FUZZ_MINUS = (HZ > 50 ? HZ / 50 : 1),
	ADDRCONF_TIMER_FUZZ       = (HZ / 4),
	ADDRCONF_TIMER_FUZZ_MAX   = (HZ),
};

unsigned long addrconf_timeout_fixup(u32, unsigned int);
int addrconf_finite_timeout(unsigned long);


/***********************
 ** uapi/linux/xfrm.h **
 ***********************/

enum {
	XFRM_POLICY_IN  = 0,
	XFRM_POLICY_FWD = 2,

	XFRM_MAX_DEPTH  = 6,
	XFRM_STATE_ICMP = 16,
};


/****************
 ** net/xfrm.h **
 ****************/

struct flowi;
struct xfrm_state
{
	struct
	{
		u8 flags;
	} props;
};

struct sec_path
{
	int                len;
	struct xfrm_state *xvec[XFRM_MAX_DEPTH];
};

int  xfrm_sk_clone_policy(struct sock *, const struct sock *osk);
int  xfrm_decode_session_reverse(struct sk_buff *, struct flowi *,
                                 unsigned int);
void xfrm_sk_free_policy(struct sock *);
int  xfrm4_policy_check(struct sock *sk, int, struct sk_buff *);
int  xfrm4_policy_check_reverse(struct sock *, int, struct sk_buff *);
int  xfrm4_route_forward(struct sk_buff *);
int  xfrm_user_policy(struct sock *, int, u8 *, int);
void secpath_reset(struct sk_buff *);
int  secpath_exists(struct sk_buff *);


/*********************
 ** net/incet_ecn.h **
 *********************/

enum {
	INET_ECN_CE   = 3,
	INET_ECN_MASK = 3,
};


/******************
 ** linux/igmp.h **
 ******************/

extern int sysctl_igmp_max_msf;

struct in_device;
struct ip_mreqn;
struct ip_msfilter;
struct ip_mreq_source;
struct group_filter;

int ip_check_mc_rcu(struct in_device *, __be32, __be32, u16);

void ip_mc_init_dev(struct in_device *);
void ip_mc_up(struct in_device *);
void ip_mc_down(struct in_device *);
void ip_mc_destroy_dev(struct in_device *);
void ip_mc_drop_socket(struct sock *);
int  ip_mc_gsfget(struct sock *, struct group_filter *, struct group_filter *, int *);
int  ip_mc_join_group(struct sock *, struct ip_mreqn *);
int  ip_mc_leave_group(struct sock *, struct ip_mreqn *);
int  ip_mc_msfilter(struct sock *, struct ip_msfilter *,int);
int  ip_mc_msfget(struct sock *, struct ip_msfilter *, struct ip_msfilter *, int *);
void ip_mc_remap(struct in_device *);
int  ip_mc_sf_allow(struct sock *, __be32, __be32, int);
int  ip_mc_source(int, int, struct sock *, struct ip_mreq_source *, int);
void ip_mc_unmap(struct in_device *);


/**************************
 ** uapi/linux/pkg_sched **
 **************************/

#include <uapi/linux/inet_diag.h>

enum {
	TC_PRIO_BESTEFFORT       = 0,
	TC_PRIO_BULK             = 2,
	TC_PRIO_INTERACTIVE_BULK = 4,
	TC_PRIO_INTERACTIVE      = 6,
};


/***********************
 ** net/inet_common.h **
 ***********************/

int  __inet_stream_connect(struct socket *, struct sockaddr *, int, int);
void inet_sock_destruct(struct sock *);
int  inet_ctl_sock_create(struct sock **, unsigned short,
                          unsigned short, unsigned char,
                          struct net *);
void inet_ctl_sock_destroy(struct sock *);
int inet_recv_error(struct sock *sk, struct msghdr *msg, int len, int *addr_len);


/***********************
 ** linux/inet_diag.h **
 ***********************/

struct inet_hashinfo;

struct inet_diag_handler
{
	void (*dump)(struct sk_buff *skb,
	             struct netlink_callback *cb,
	             struct inet_diag_req_v2 *r,
	             struct nlattr *bc);

	int (*dump_one)(struct sk_buff *in_skb,
	                const struct nlmsghdr *nlh,
	                struct inet_diag_req_v2 *req);

	void (*idiag_get_info)(struct sock *sk,
	                       struct inet_diag_msg *r,
	                       void *info);
	__u16                  idiag_type;
	__u16                  idiag_info_size;
};


void inet_diag_dump_icsk(struct inet_hashinfo *, struct sk_buff *,
                         struct netlink_callback *, const struct inet_diag_req_v2 *,
                         struct nlattr *);
int inet_diag_dump_one_icsk(struct inet_hashinfo *, struct sk_buff *,
                            const struct nlmsghdr *, const struct inet_diag_req_v2 *);

int  inet_diag_register(const struct inet_diag_handler *);
void inet_diag_unregister(const struct inet_diag_handler *);


/********************
 ** net/inet_ecn.h **
 ********************/

enum {
	INET_ECN_NOT_ECT = 0,
};

int  INET_ECN_is_not_ect(__u8);
void INET_ECN_xmit(struct sock *);
void INET_ECN_dontxmit(struct sock *);


/*****************
 ** net/xfrm4.h **
 *****************/

int xfrm4_udp_encap_rcv(struct sock *sk, struct sk_buff *skb);


/*********************
 ** linux/netpoll.h **
 *********************/

struct napi_struct;

void *netpoll_poll_lock(struct napi_struct *);
int   netpoll_rx_disable(struct net_device *);
void  netpoll_rx_enable(struct net_device *);
bool  netpoll_rx(struct sk_buff *);
bool  netpoll_rx_on(struct sk_buff *);
int   netpoll_receive_skb(struct sk_buff *);
void  netpoll_poll_unlock(void *);

void netpoll_poll_disable(struct net_device *dev);
void netpoll_poll_enable(struct net_device *dev);


/************************
 ** net/ethernet/eth.c **
 ************************/

extern const struct header_ops eth_header_ops;


/***********************
 ** linux/netfilter.h **
 ***********************/

#define NF_HOOK(pf, hook, net, sk, skb, indev, outdev, okfn) (okfn)(net, sk, skb)
#define NF_HOOK_COND(pf, hook, net, sk, skb, indev, outdev, okfn, cond) (okfn)(net, sk, skb)

int nf_hook(u_int8_t pf, unsigned int hook, struct net *net,
            struct sock *sk, struct sk_buff *skb,
            struct net_device *indev, struct net_device *outdev,
            int (*okfn)(struct net *, struct sock *, struct sk_buff *));
void nf_ct_attach(struct sk_buff *, struct sk_buff *);


/*******************************
 ** linux/netfilter_ingress.h **
 *******************************/

void nf_hook_ingress_init(struct net_device *dev);


/******************************
 ** linux/netfilter_bridge.h **
 ******************************/

unsigned int nf_bridge_pad(const struct sk_buff *);


/****************
 ** linux/in.h **
 ****************/

bool ipv4_is_local_multicast(__be32);

#define INADDR_BROADCAST        ((unsigned long int) 0xffffffff)

static inline bool ipv4_is_multicast(__be32 addr)
{
	return (addr & htonl(0xf0000000)) == htonl(0xe0000000);
}

static inline bool ipv4_is_zeronet(__be32 addr)
{
	return (addr & htonl(0xff000000)) == htonl(0x00000000);
}

static inline bool ipv4_is_lbcast(__be32 addr)
{
	/* limited broadcast */
	return addr == htonl(INADDR_BROADCAST);
}

static inline bool ipv4_is_loopback(__be32 addr)
{
	return (addr & htonl(0xff000000)) == htonl(0x7f000000);
}

#define IP_FMT "%u.%u.%u.%u"
#define IP_ARG(x) (x >> 0) & 0xff, (x >> 8) & 0xff, (x >> 16) & 0xff, (x >> 24) & 0xff
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARG(x) x[0], x[1], x[2], x[3], x[4], x[5]

#define __UAPI_DEF_IPPROTO_V6 1
#define __UAPI_DEF_IN6_ADDR 1
#include <uapi/linux/in6.h>
#undef __UAPI_DEF_IN6_ADDR
#undef __UAPI_DEF_IPPROTO_V6

#define __UAPI_DEF_IN_IPPROTO 1
#define __UAPI_DEF_IN_ADDR 1
#define __UAPI_DEF_SOCKADDR_IN 1
#define __UAPI_DEF_IN_CLASS 1
#define __UAPI_DEF_IP_MREQ 1
#define __UAPI_DEF_IN_PKTINFO 1
#include <uapi/linux/in.h>
#undef __UAPI_DEF_IN_PKTINFO
#undef __UAPI_DEF_IP_MREQ
#undef __UAPI_DEF_IN_CLASS
#undef __UAPI_DEF_SOCKADDR_IN
#undef __UAPI_DEF_IN_IPPROTO
#undef __UAPI_DEF_IN_ADDR


/********************
 ** linux/random.h **
 ********************/

void get_random_bytes(void *buf, int nbytes);

#define get_random_once(buf, nbytes)           \
	({                                         \
		static bool initialized = false;       \
		if (!initialized) {                    \
			get_random_bytes((buf), (nbytes)); \
			initialized = true;                \
		}                                      \
	})

u32 prandom_u32(void);

static inline void prandom_bytes(void *buf, size_t nbytes)
{
	get_random_bytes(buf, nbytes);
}

u32  random32(void);
void add_device_randomness(const void *, unsigned int);
u32  next_pseudo_random32(u32);
void srandom32(u32 seed);
u32 prandom_u32_max(u32 ep_ro);
void prandom_seed(u32 seed);


/**********************
 ** linux/security.h **
 **********************/

struct request_sock;

void security_sock_graft(struct sock *, struct socket *);
void security_sk_classify_flow(struct sock *, struct flowi *);
int  security_socket_getpeersec_stream(struct socket *, char *, int *, unsigned );
int  security_sk_alloc(struct sock *, int, gfp_t);
void security_sk_free(struct sock *);
void security_req_classify_flow(const struct request_sock *, struct flowi *);
int  security_inet_conn_request(struct sock *, struct sk_buff *, struct request_sock *);
void security_inet_csk_clone(struct sock *, const struct request_sock *);
int  security_socket_getpeersec_dgram(struct socket *, struct sk_buff *, u32 *);
int  security_secid_to_secctx(u32 secid, char **secdata, u32 *);
void security_release_secctx(char *secdata, u32 seclen);
void security_skb_classify_flow(struct sk_buff *, struct flowi *);
void security_skb_owned_by(struct sk_buff *, struct sock *);
void security_inet_conn_established(struct sock *, struct sk_buff *);
int  security_netlink_send(struct sock *, struct sk_buff *);


/**********************
 ** net/netns/hash.h **
 **********************/

unsigned int net_hash_mix(struct net const *);


/**************************
 ** net/netprio_cgroup.h **
 **************************/

void sock_update_netprioidx(struct sock *);


/****************
 ** net/ipv6.h **
 ****************/

enum {
	IP6_MF     = 0x0001,
	IP6_OFFSET = 0xfff8,
};


/******************
 ** linux/ipv6.h **
 ******************/

struct inet6_skb_parm { unsigned dummy; };

int inet_v6_ipv6only(const struct sock *);
int ipv6_only_sock(const struct sock *);

#define ipv6_optlen(p)  (((p)->hdrlen+1) << 3)
#define ipv6_authlen(p) (((p)->hdrlen+2) << 2)
#define ipv6_sk_rxinfo(sk) 0

/* XXX not the sanest thing to do... */
struct ipv6_pinfo {
	__u16           recverr:1;
};
static inline struct ipv6_pinfo * inet6_sk(const struct sock *__sk) { return NULL; }


/********************
 ** linux/mroute.h **
 ********************/

int ip_mroute_opt(int);
int ip_mroute_getsockopt(struct sock *, int, char*, int *);
int ip_mroute_setsockopt(struct sock *, int, char *, unsigned int);


/******************
 ** linux/inet.h **
 ******************/

__be32 in_aton(const char *);


/**********************
 ** net/cipso_ipv4.h **
 **********************/

int cipso_v4_validate(const struct sk_buff *, unsigned char **option);


/***********************
 ** uapi/linux/stat.h **
 ***********************/

bool S_ISSOCK(int);


/*********************
 ** net/gen_stats.h **
 *********************/

struct gnet_stats_basic_cpu {
	unsigned bstats; /* normally gnet_stats_basic_packed */
	unsigned syncp;  /* normally u64_stats_sync */
};

struct gnet_stats_basic_packed;
struct gnet_stats_rate_est;

void gen_kill_estimator(struct gnet_stats_basic_packed *,
                        struct gnet_stats_rate_est *);


/*******************
 ** linux/sysfs.h **
 *******************/

struct attribute {
	const char *name;
	mode_t      mode;
};

int sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name);
void sysfs_remove_link(struct kobject *kobj, const char *name);
void sysfs_remove_link_from_group(struct kobject *kobj, const char *group_name, const char *link_name);


/*********************
 ** net/pusy_poll.h **
 *********************/

bool sk_busy_loop(struct sock *sk, int nonblock);
bool sk_can_busy_loop(struct sock *sk);
void sk_mark_napi_id(struct sock *sk, struct sk_buff *skb);


/******************
 ** net/l3mdev.h **
 ******************/

int l3mdev_get_saddr(struct net *net, int ifindex, struct flowi4 *fl4);
struct rtable *l3mdev_get_rtable(const struct net_device *dev, const struct flowi4 *fl4);
bool netif_index_is_l3_master(struct net *net, int ifindex);
u32 l3mdev_fib_table(const struct net_device *dev);
int l3mdev_master_ifindex_rcu(struct net_device *dev);
int l3mdev_fib_oif_rcu(struct net_device *dev);
u32 l3mdev_fib_table_by_index(struct net *net, int ifindex);
int l3mdev_master_ifindex(struct net_device *dev);


/***********************
 ** linux/sock_diag.h **
 ***********************/

bool sock_diag_has_destroy_listeners(const struct sock *sk);
void sock_diag_broadcast_destroy(struct sock *sk);


/************************
 ** net/ip6_checksum.h **
 ************************/

__sum16 csum_ipv6_magic(const struct in6_addr *saddr, const struct in6_addr *daddr,
                        __u32 len, unsigned short proto, __wsum csum);


/***********************
 ** linux/switchdev.h **
 ***********************/

struct fib_info;

int switchdev_fib_ipv4_add(u32 dst, int dst_len, struct fib_info *fi, u8 tos, u8 type, u32 nlflags, u32 tb_id);
int switchdev_fib_ipv4_del(u32 dst, int dst_len, struct fib_info *fi, u8 tos, u8 type, u32 tb_id);
void switchdev_fib_ipv4_abort(struct fib_info *fi);


/*******************
 ** Tracing stuff **
 *******************/

struct proto;

void trace_kfree_skb(struct sk_buff *, void *);
void trace_consume_skb(struct sk_buff *);
void trace_sock_exceed_buf_limit(struct sock *, struct proto *, long);
void trace_sock_rcvqueue_full(struct sock *, struct sk_buff *);
void trace_net_dev_xmit(struct sk_buff *, int , struct net_device *,
                        unsigned int);
void trace_net_dev_queue(struct sk_buff *);
void trace_netif_rx(struct sk_buff *);
void trace_netif_receive_skb(struct sk_buff *);
void trace_napi_poll(struct napi_struct *);
void trace_skb_copy_datagram_iovec(const struct sk_buff *, int);
void trace_udp_fail_queue_rcv_skb(int, struct sock*);
void trace_net_dev_start_xmit(struct sk_buff*, struct net_device*);
void trace_netif_rx_entry(struct sk_buff*);
void trace_netif_rx_ni_entry(struct sk_buff*);
void trace_netif_receive_skb_entry(struct sk_buff*);
void trace_napi_gro_receive_entry(struct sk_buff*);
void trace_napi_gro_frags_entry(struct sk_buff*);
void trace_fib_validate_source(const struct net_device *, const struct flowi4 *);
void trace_fib_table_lookup(const void *, const void *);
void trace_fib_table_lookup_nh(const void *);


/******************
 ** Lxip private **
 ******************/

void set_sock_wait(struct socket *sock, unsigned long ptr);
int  socket_check_state(struct socket *sock);
void log_sock(struct socket *sock);

void lx_trace_event(char const *, ...) __attribute__((format(printf, 1, 2)));

#include <legacy/lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_H_ */
