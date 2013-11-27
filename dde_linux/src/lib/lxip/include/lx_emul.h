 /*
 * \brief  Emulation of the Linux kernel API
 * \author Sebastian Sumpf
 * \date   2013-08-19
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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <dde_kit/panic.h>
#include <dde_kit/printf.h>
#include <dde_kit/types.h>

#define DEBUG_PRINTK 1
#define DEBUG_SLAB   0
#define DEBUG_TIMER  0
#define DEBUG_CONG   0
#define DEBUG_LEVEL  0


#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(3,9,0)

#define KBUILD_MODNAME "mod-noname"


/***************
 ** asm/bug.h **
 ***************/

#define WARN_ON(condition) ({ \
	int ret = !!(condition); \
	if (ret) dde_kit_printf("[%s] WARN_ON(" #condition ")\n", __func__); \
	ret; })

#define WARN(condition, format, ...) ({ \
	int ret = !!(condition); \
  if (ret) dde_kit_printf("[%s] WARN(" #condition ") " format "\n" , __func__,  __VA_ARGS__); \
	ret; })

#define WARN_ON_ONCE WARN_ON

#define BUG() do { \
	dde_kit_debug("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	while (1); \
} while (0)

#define BUG_ON(condition) do { if (condition) BUG(); } while(0)


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
	EHOSTDOWN       = 64,
	EHOSTUNREACH    = 65,
	ENOSYS          = 78,
	ENOMSG          = 83,
	EPROTO          = 92,
	EOVERFLOW       = 84,

	/*
	 * The following numbers correspond to nothing
	 */
	EREMOTEIO       = 200,
	ERESTARTSYS     = 201,
	ENODATA         = 202,
	ETOOSMALL       = 203,
	ENOIOCTLCMD     = 204,
	ENONET          = 205,

	MAX_ERRNO       = 4095,
};


/**********************
 ** linux/compiler.h **
 **********************/

#define likely
#define unlikely

#define __user
#define __rcu
#define __percpu
#define __must_check
#define __force

#define __releases(x)
#define __acquires(x)

#define __aligned(x)  __attribute__((aligned(x)))

#define noinline_for_stack
#define noinline     __attribute__((noinline))

#define __printf(a, b) __attribute__((format(printf, a, b)))
#define __attribute_const__  __attribute__((__const__))

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

#define __HAVE_BUILTIN_BSWAP32__
#define __HAVE_BUILTIN_BSWAP64__


/******************
 ** linux/init.h **
 ******************/

#define __init
#define __initdata
#define __exit

#define _SETUP_CONCAT(a, b) __##a##b
#define SETUP_CONCAT(a, b) _SETUP_CONCAT(a, b)
#define __setup(str, fn) void SETUP_CONCAT(fn, SETUP_SUFFIX)(void *addrs) { fn(addrs); }

/* these must be called by us during initialization */
#define  core_initcall(fn)   void core_##fn(void) { fn(); }
#define  subsys_initcall(fn) void subsys_##fn(void) { fn(); }
#define  fs_initcall(fn)     void fs_##fn(void) { fn(); }
#define  late_initcall(fn)   void late_##fn(void) { fn(); }


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


struct module;
#define module_init(fn) void module_##fn(void) { fn(); }
#define module_exit(fn) void module_exit_##fn(void) { exit_fn(); }


/*************************
 ** linux/moduleparam.h **
 *************************/

#define module_param(name, type, perm)
#define MODULE_PARM_DESC(_parm, desc)


/*********************
 ** linux/kconfig.h **
 *********************/

#define IS_ENABLED(x) x
#define CONFIG_DEFAULT_TCP_CONG "cubic"


/***************
 ** asm/bug.h **
 ***************/

#define BUG() do { \
	dde_kit_debug("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	while (1); \
} while (0)


/*******************
 ** linux/types.h **
 *******************/

typedef dde_kit_int8_t   int8_t;
typedef dde_kit_int16_t  int16_t;
typedef dde_kit_int32_t  int32_t;
typedef dde_kit_int64_t  int64_t;

typedef dde_kit_uint8_t  uint8_t;
typedef dde_kit_uint16_t uint16_t;
typedef dde_kit_uint32_t uint32_t;
typedef dde_kit_uint64_t uint64_t;
typedef dde_kit_size_t   size_t;

typedef uint32_t uint;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;


typedef int16_t  __s16;
typedef int32_t  __s32;

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;

typedef __u16 __be16;
typedef __u32 __be32;
typedef __u64 __be64;

typedef __u16 __sum16;
typedef __u32 __wsum;

typedef unsigned       gfp_t;
typedef unsigned long  dma_addr_t;
typedef long long      loff_t;
typedef size_t       __kernel_size_t;
typedef long         __kernel_time_t;
typedef long         __kernel_suseconds_t;
typedef int            pid_t;
typedef long           ssize_t;
typedef unsigned short umode_t;

#define __aligned_u64 __u64 __attribute__((aligned(8)))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, 8 * sizeof(long))
#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]

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

#define __bitwise


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

/*****************
 ** linux/err.h **
 *****************/

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)

static inline bool IS_ERR(void const *ptr) {
	return (unsigned long)(ptr) > (unsigned long)(-1000); }

static inline void * ERR_PTR(long error) {
	return (void *) error;
}

static inline long IS_ERR_OR_NULL(const void *ptr) {
	return !ptr || IS_ERR_VALUE((unsigned long)ptr); }

static inline long PTR_ERR(const void *ptr) { return (long) ptr; }


/*******************************
 ** asm-generic/scatterlist.h **
 *******************************/

struct scatterlist
{
};

/********************
 ** linux/printk.h **
 ********************/

#define KERN_WARNING "<4>"

#define pr_crit(fmt, ...)    printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)    printk(KERN_INFO fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)     printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_notice(fmt, ...)  printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_cont(fmt, ...)    printk(KERN_CONT fmt, ##__VA_ARGS__)

#if DEBUG_LEVEL
#define pr_debug(fmt, ...)   printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif

struct va_format
{
	const char *fmt;
	va_list *va;
};

static inline int _printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	dde_kit_vprintf(fmt, args);
	va_end(args);
	return 0;
}

#if DEBUG_PRINTK
#define printk  _printk
#define vprintk dde_kit_vprintf
#define panic   dde_kit_panic
#else
#define printk(...)
#define vprintk(...)
#define panic(...)
#endif


/******************
 ** linux/hash.h **
 ******************/

u32 hash_32(u32 val, unsigned int bits);


/****************************************************************
 ** arch/(*)/include/asm/(processor.h|segment.h|thread_info.h) **
 ****************************************************************/

typedef unsigned long mm_segment_t;


/*************************************
 ** linux/byteorder/little_endian.h **
 *************************************/

#include <uapi/linux/byteorder/little_endian.h>


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

#define cpu_to_be16  __cpu_to_be16
#define cpu_to_be32  __cpu_to_be32

#define be16_to_cpup __be16_to_cpup
#define be32_to_cpup __be32_to_cpup

#define htons(x) __cpu_to_be16(x)
#define ntohs(x) __be16_to_cpu(x)
#define htonl(x) __cpu_to_be32(x)
#define ntohl(x) __be32_to_cpu(x)


/*********************************
 ** linux/unaligned/access_ok.h **
 *********************************/

static inline u16 get_unaligned_be16(const void *p)
{
	return be16_to_cpup((__be16 *)p);
}

static inline u32 get_unaligned_be32(const void *p)
{
	return be32_to_cpup((__be32 *)p);
}


/*****************
 ** asm/param.h **
 *****************/

#define HZ 100UL


/*********************
 ** linux/stringify **
 *********************/

#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)


/********************
 ** linux/kernel.h **
 ********************/

#define KERN_DEBUG  "DEBUG: "
#define KERN_INFO   "INFO: "
#define KERN_ERR    "ERROR: "
#define KERN_CRIT   "CRTITCAL: "
#define KERN_NOTICE "NOTICE: "
#define KERN_EMERG  "EMERG: "
#define KERN_ALERT  "ALERT: "
#define KERN_CONT   ""


#define container_of(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})

/* normally provided by linux/stddef.h, needed by linux/list.h */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define ALIGN(x, a) x
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define USHRT_MAX ((u16)(~0U))
#define INT_MAX   ((int)(~0U>>1))
#define INT_MIN   (-INT_MAX - 1)
#define UINT_MAX  (~0U)

static inline size_t min(size_t a, size_t b) {
	return a < b ? a : b; }

#define max(x, y) ({                      \
	typeof(x) _max1 = (x);                  \
	typeof(y) _max2 = (y);                  \
	(void) (&_max1 == &_max2);              \
	_max1 > _max2 ? _max1 : _max2; })

#define min_t(type, x, y) ({ \
	type __min1 = (x); \
	type __min2 = (y); \
	__min1 < __min2 ? __min1: __min2; })

#define max_t(type, x, y) ({ \
	type __max1 = (x); \
	type __max2 = (y); \
	__max1 > __max2 ? __max1: __max2; })

#define swap(a, b) ({ \
	typeof(a) __tmp = (a); \
	(a) = (b); \
	(b) = __tmp;  })

#define PTR_ALIGN(p, a) ({              \
  unsigned long _p = (unsigned long)p;  \
  _p = (_p + a - 1) & ~(a - 1);         \
  p = (typeof(p))_p;                    \
	p;                                    \
})

#define BUILD_BUG_ON(condition)

#define _RET_IP_  (unsigned long)__builtin_return_address(0)

void might_sleep();
#define might_sleep_if(cond) do { if (cond) might_sleep(); } while (0)

enum { SPRINTF_STR_LEN = 64 };

int   snprintf(char *buf, size_t size, const char *fmt, ...);
int   sprintf(char *buf, const char *fmt, ...);
int   sscanf(const char *, const char *, ...);

#define sprintf(buf, fmt, ...) snprintf(buf, SPRINTF_STR_LEN, fmt, __VA_ARGS__)

#define kasprintf(gfp, fmt, ...) ({ \
	void *buf = kmalloc(SPRINTF_STR_LEN, 0); \
	sprintf(buf, fmt, __VA_ARGS__); \
	buf; \
})

//char *kasprintf(gfp_t gfp, const char *fmt, ...);

char *get_options(const char *str, int nints, int *ints);

/**************************
 ** uapi/linux/sysinfo.h **
 **************************/

struct sysinfo
{
	unsigned long totalram;
};


/*******************
 ** asm/cmpxchg.h **
 *******************/

#define cmpxchg(ptr, o, n) ({ \
	typeof(*ptr) prev = *ptr; \
	*ptr = (*ptr == o) ? n : *ptr; \
	prev;\
})


/******************
 ** asm/atomic.h **
 ******************/

#define ATOMIC_INIT(i) { (i) }

typedef struct atomic { unsigned long counter; } atomic_t;
typedef atomic_t atomic_long_t;

static inline unsigned int atomic_read(const atomic_t *p) { return p->counter; }
static inline void atomic_set(atomic_t *p, int i) { p->counter = i; }
static inline void atomic_sub(int i, atomic_t *p) { p->counter -= i; }
static inline int atomic_sub_return(int i, atomic_t *p) { p->counter -= i; return p->counter; }
static inline int atomic_sub_and_test(int i, atomic_t *p) { return atomic_sub_return(i, p) == 0; }

static inline int  atomic_dec_return(atomic_t *p) { return atomic_sub_return(1, p); }
static inline int  atomic_dec_and_test(atomic_t *p) { return atomic_sub_return(1, p) == 0; }
static inline void atomic_dec(atomic_t *p) { atomic_sub_return(1, p); }


static inline void atomic_inc(atomic_t *p) { p->counter++; }
static inline int  atomic_inc_return(atomic_t *p) { return p->counter++; }
static inline int atomic_inc_not_zero(atomic_t *p) { return p->counter ? p->counter++ : 0; }

static inline void atomic_add(int i, atomic_t *p) { p->counter += i; }


static inline void atomic_long_inc(atomic_long_t *p) { p->counter++; }
static inline void atomic_long_sub(int i, atomic_long_t *p) { atomic_sub(i, p); }
static inline long atomic_long_add_return(long i, atomic_long_t *p) {
	atomic_add(i, p); return p->counter; }
static inline long atomic_long_read(atomic_long_t *p) { return (long)atomic_read(p); }

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
	
	ret = v->counter;
	if (ret != u)
		v->counter += a;

	return ret != u;
}

/******************
 ** linux/kref.h **
 ******************/

struct kref { atomic_t refcount; };


/********************
 ** linux/uidgid.h **
 ********************/

typedef unsigned  kuid_t;
typedef unsigned  kgid_t;

#define GLOBAL_ROOT_UID (kuid_t)0

/***********************
 ** linux/kmemcheck.h **
 ***********************/

#define kmemcheck_bitfield_begin(name)
#define kmemcheck_bitfield_end(name)
#define kmemcheck_annotate_bitfield(ptr, name)

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

struct static_key { };
#define STATIC_KEY_INIT_FALSE ((struct static_key) {})

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

/**********************************
 ** linux/bitops.h, asm/bitops.h **
 **********************************/

enum { BITS_PER_LONG  = sizeof(long) * 8 };
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

static int get_bitmask_order(unsigned int count) {
 return __builtin_clz(count) ^ 0x1f; }


#define ffs(x) __builtin_ffs(x)
#define ffz(x) ffs(~(x))

static inline __u32 rol32(__u32 word, unsigned int shift)
{
	return (word << shift) | (word >> (32 - shift));
}


/*******************************
 ** asm-generic/bitops/find.h **
 *******************************/

unsigned long find_first_zero_bit(unsigned long const *addr, unsigned long size);


/****************************
 ** asm-generic/getorder.h **
 ****************************/

int get_order(unsigned long size);


/******************
 ** linux/log2.h **
 ******************/

unsigned long ilog2(unsigned long n);

#define roundup_pow_of_two(n) ( \
	1UL << (ilog2((n) - 1) + 1))


/****************
 ** asm/page.h **
 ****************/

/*
 * For now, hardcoded
 */
#define PAGE_SIZE 4096

enum {
	PAGE_SHIFT = 12,
};

#ifdef __cplusplus
#define private priv
#endif
struct page
{
	int       pfmemalloc;
	int       mapping;
	atomic_t _count;
	void     *addr;
	unsigned long private;
#ifdef __cplusplus
#undef priv
#endif
} __attribute((packed));


/*************************
 ** linux/res_counter.h **
 *************************/

enum { RES_USAGE };


/************************
 ** linux/memcontrol.h **
 ************************/

enum { UNDER_LIMIT, SOFT_LIMIT, OVER_LIMIT };


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


/****************
 ** linux/mm.h **
 ****************/

extern unsigned long totalram_pages;
extern unsigned long num_physpages;

static inline struct page *compound_head(struct page *page) { return page; }
static inline void *page_address(struct page *page) { return page->addr; };

void get_page(struct page *page);
void put_page(struct page *page);

struct page *virt_to_head_page(const void *x);
struct page *virt_to_page(const void *x);


/******************
 ** linux/swap.h **
 ******************/

unsigned long nr_free_buffer_pages(void);



/*****************
 ** linux/gfp.h **
 *****************/

enum {
	__GFP_DMA        = 0x01u,
	__GFP_WAIT       = 0x10u,
	__GFP_COLD       = 0x100u,
	__GFP_NOWARN     = 0x200u,
	__GFP_REPEAT     = 0x400u,
	__GFP_MEMALLOC   = 0x2000u,
	__GFP_ZERO       = 0x8000u,
	__GFP_COMP       = 0x4000u,
	__GFP_NOMEMALLOC = 0x10000u,
};

enum {
	GFP_DMA    = __GFP_DMA,
	GFP_KERNEL = 0x0u,
	GFP_USER   = 0x1u,
	GFP_ATOMIC = 0x20u,
};

struct page *alloc_pages_node(int nid, gfp_t gfp_mask,
                              unsigned int order);

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

unsigned long get_zeroed_page(gfp_t gfp_mask);
#define free_page(p) kfree((void *)p)

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
void *kzalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *krealloc(const void *, size_t, gfp_t);
void *kzalloc_node(size_t size, gfp_t flags, int node);
void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node);
void  kfree(const void *);
size_t ksize(const void *objp);

struct kmem_cache;

struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
                                     unsigned long, void (*)(void *));
void *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void *kmem_cache_alloc_node(struct kmem_cache *, gfp_t flags, int node);

void kmem_cache_free(struct kmem_cache *cache, void *objp);


/*********************
 ** linux/vmalloc.h **
 *********************/

void *vzalloc(unsigned long size);


/**********************
 ** linux/highmem.h  **
 **********************/

static inline void *kmap(struct page *page) { return page_address(page); }
static inline void *kmap_atomic(struct page *page) { return kmap(page); }


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


/**********************
 ** linux/spinlock.h **
 **********************/

typedef unsigned spinlock_t;
#define DEFINE_SPINLOCK(name) spinlock_t name = 0;
#define spin_lock_nested(lock, subclass)
static inline void spin_lock_init(spinlock_t *lock) { }

static inline void spin_lock(spinlock_t *lock) { }
static inline void spin_unlock(spinlock_t *lock) { }

static inline void spin_lock_bh(spinlock_t *lock) { }
static inline void spin_unlock_bh(spinlock_t *lock) { }

int spin_trylock(spinlock_t *lock);

/****************************
 ** linux/spinlock_types.h **
 ****************************/

#define __SPIN_LOCK_UNLOCKED(x) 0


/*******************
 ** linux/mutex.h **
 *******************/

struct mutex { };

#define DEFINE_MUTEX(n) struct mutex n;


/**************************
 ** linux/rwlock_types.h **
 **************************/

typedef unsigned rwlock_t;

#define DEFINE_RWLOCK(x) rwlock_t x;
#define __RW_LOCK_UNLOCKED(x) 0


/*******************
 ** linux/rwsem.h **
 *******************/

struct rw_semaphore { int dummy; };

#define __RWSEM_INITIALIZER(name) { 0 }


/*********************
 ** linux/seqlock.h **
 *********************/

typedef unsigned seqlock_t;

#define __SEQLOCK_UNLOCKED(x) 0

/*********************
 ** linux/jiffies.h **
 *********************/

extern volatile unsigned long jiffies;

enum { INITIAL_JIFFIES = 0 };

unsigned int jiffies_to_msecs(const unsigned long j);
unsigned long msecs_to_jiffies(const unsigned int m);

long time_after(long a, long b);
long time_after_eq(long a, long b);

static inline long time_before(long a, long b) { return time_after(b, a); }
static inline long time_before_eq(long a, long b) { return time_after_eq(b ,a); }


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
	NSEC_PER_MSEC = 1000L * 1000L,
	NSEC_PER_SEC  = MSEC_PER_SEC * NSEC_PER_MSEC,
	USEC,
	USEC_PER_MSEC = 1000L,
};

unsigned long get_seconds(void);


/*******************
 ** linux/ktime.h **
 *******************/

union ktime { 
	s64 tv64; 
};
 typedef union ktime ktime_t;

struct timeval ktime_to_timeval(const ktime_t);
struct timespec ktime_to_timespec(const ktime_t kt);

ktime_t ktime_sub(const ktime_t, const ktime_t);

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
	return (ktime_t) { .tv64 = jiffies * (1000 / HZ) * NSEC_PER_MSEC };
}

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
void setup_timer(struct timer_list *timer,void (*function)(unsigned long),
                 unsigned long data);
int  timer_pending(const struct timer_list * timer);
int  del_timer(struct timer_list *timer);


/*********************
 ** linux/hrtimer.h **
 *********************/

struct hrtimer { };


/*******************
 ** linux/delay.h **
 *******************/

 void msleep(unsigned int msecs);

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

long schedule_timeout_uninterruptible(signed long timeout);
signed long schedule_timeout(signed long timeout);

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

struct lock_class_key { };
struct lockdep_map { };


/*****************
 ** linux/smp.h **
 *****************/

#define smp_processor_id() 0

/**********************
 ** linux/rcupdate.h **
 **********************/

struct rcu_head { };

#define kfree_rcu(ptr, offset) kfree(ptr)

#define rcu_dereference(p) p
#define rcu_dereference_bh(p) p
#define rcu_dereference_check(p, c) p
#define rcu_dereference_protected(p, c) p
#define rcu_dereference_raw(p) p
#define rcu_dereference_rtnl(p) p
#define rcu_dereference_index_check(p, c) p

#define rcu_assign_pointer(p, v) p = v;
#define rcu_access_pointer(p) p

void rcu_read_lock(void);
void rcu_read_unlock(void);

static inline int rcu_read_lock_held(void) { return 1; }
static inline int rcu_read_lock_bh_held(void) { return 1; }

static inline void call_rcu(struct rcu_head *head,
                            void (*func)(struct rcu_head *head))
{ func(head); }

#define RCU_INIT_POINTER(p, v) p = (typeof(*v) *)v

/*********************
 ** linux/rculist.h **
 *********************/

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


/***************************
 ** linux/rculist_nulls.h **
 ***************************/

#define hlist_nulls_for_each_entry_rcu(tpos, pos, head, member) \
	hlist_nulls_for_each_entry(tpos, pos, head, member)


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

/********************
 ** linux/percpu.h **
 ********************/

void *__alloc_percpu(size_t size, size_t align);

#define alloc_percpu(type)      \
        (typeof(type) __percpu *)__alloc_percpu(sizeof(type), __alignof__(type))

#define per_cpu(var, cpu) var
#define per_cpu_ptr(ptr, cpu)   ({ (void)(cpu);(typeof(*(ptr)) *)(ptr); })
#define get_cpu_var(var) var
#define __get_cpu_var(var) var
#define __this_cpu_ptr(ptr) per_cpu_ptr(ptr, 0)
#define this_cpu_ptr(ptr) per_cpu_ptr(ptr, 0)

#define this_cpu_inc(pcp) pcp += 1
#define this_cpu_dec(pcp) pcp -= 1

#define __this_cpu_inc(pcp) this_cpu_inc(pcp)
#define __this_cpu_dec(pcp) this_cpu_dec(pcp)

#define this_cpu_add(pcp, val) pcp += val
#define __this_cpu_add(pcp, val) this_cpu_add(pcp, val)
#define get_cpu() 0


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


/*********************
 ** linux/kobject.h **
 *********************/

enum kobject_action
{
	KOBJ_ADD,
	KOBJ_REMOVE,
};

struct kobject { };


/***********************
 ** linux/interrupt.h **
 ***********************/

enum {
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	NET_SOFTIRQS,
};

struct softirq_action { };


/*************************
 ** linux/bottom_half.h **
 *************************/

void local_bh_disable(void);
void local_bh_enable(void);


/********************
 ** linux/string.h **
 ********************/

int    memcmp(const void *, const void *, size_t);
void  *memmove(void *, const void *, size_t);
void  *memset(void *s, int c, size_t n);
void  *kmemdup(const void *src, size_t len, gfp_t gfp);
char *kstrdup(const char *s, gfp_t gfp);
char  *strchr(const char *, int);
size_t strnlen(const char *s, size_t maxlen);
size_t strlen(const char *);
char  *strnchr(const char *, size_t, int);
char  *strcat(char *dest, const char *src);
int    strcmp(const char *, const char *);
int    strncmp(const char *, const char *, size_t);
char  *strcpy(char *to, const char *from);
char * strncpy(char *,const char *, __kernel_size_t);;
size_t strlcpy(char *dest, const char *src, size_t size);
char * strsep(char **,const char *);

void  *genode_memcpy(void *d, const void *s, size_t n);
static inline void  *memcpy(void *d, const void *s, size_t n) {
	return genode_memcpy(d, s, n); }


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


#define get_user(src, dst) ({ \
	src = *dst; \
	0; })

#define put_user(x, ptr) ({ \
	*ptr = x; \
	0; })

#define __copy_from_user copy_from_user


/*****************************
 ** uapi/linux/capability.h **
 *****************************/

enum {
	CAP_NET_BIND_SERVICE = 10,
	CAP_NET_ADMIN        = 12,
	CAP_NET_RAW          = 13,
};


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


/********************
 ** linux/device.h **
 ********************/

struct device_driver
{
	char const      *name;
};


struct device
{
	struct kobject                *kobj;
	struct device                 *parent;
	struct device_driver          *driver;
};

struct class_attribute { };

const char *dev_driver_string(const struct device *dev);
const char *dev_name(const struct device *dev);

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


/*****************
 ** linux/phy.h **
 *****************/

struct ethtool_ts_info;
struct phy_device;

struct phy_driver
{
	int (*ts_info)(struct phy_device *phydev, struct ethtool_ts_info *ti);
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


/******************
 ** linux/wait.h **
 ******************/

typedef struct wait_queue_head { int dummy; } wait_queue_head_t;
typedef struct wait_queue { } wait_queue_t;

#define DEFINE_WAIT(name) \
	wait_queue_t name;

#define __WAIT_QUEUE_HEAD_INITIALIZER(name) { 0 }

#define DECLARE_WAITQUEUE(name, tsk) \
        wait_queue_t name

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	wait_queue_head_t name = __WAIT_QUEUE_HEAD_INITIALIZER(name)

#define DEFINE_WAIT_FUNC(name, function) \
       wait_queue_t name

/******************
 ** linux/poll.h **
 ******************/

typedef struct poll_table_struct { } poll_table;

struct file;
void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p);

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


/*****************
 ** linux/aio.h **
 *****************/
#ifdef __cplusplus
#define private priv
#endif
struct kiocb
{
	void *private;
};

#ifdef __cplusplus
#undef private
#endif

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

struct sk_filter
{
	atomic_t        refcnt;
	struct rcu_head rcu;
};


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


/***********************
 ** linux/workquque.h **
 ***********************/

struct workqueue_struct { };

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	work_func_t func;
	struct list_head entry;
};

struct delayed_work
{
	struct work_struct work;
};

#define DECLARE_DELAYED_WORK(n, f)                                      \
        struct delayed_work n

#define __WORK_INITIALIZER(n, f) { \
	.entry  = { &(n).entry, &(n).entry }, \
	.func = (f) \
}

extern struct workqueue_struct *system_wq;


/**********************
 ** linux/inerrupt.h **
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


/**********************
 ** linx/rtnetlink.h **
 **********************/

struct net_device;

struct netdev_queue *dev_ingress_queue(struct net_device *dev);


/********************
 ** net/netevent.h **
 ********************/

enum netevent_notif_type {
	NETEVENT_NEIGH_UPDATE = 1, /* arg is struct neighbour ptr */
};


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


/*********************
 ** net/fib_rules.h **
 *********************/

enum { FIB_LOOKUP_NOREF = 1 };


/****************************
 ** linux/u64_stats_sync.h **
 ****************************/

struct u64_stats_sync { };


/*************************
 ** net/net_namespace.h **
 *************************/

#define new _new
#include <linux/list.h>
#undef new
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
	struct net_device       *loopback_dev;
	struct user_namespace   *user_ns;
	struct proc_dir_entry   *proc_net_stat;
	struct sock             *rtnl;
	struct netns_mib         mib;
	struct netns_ipv4        ipv4;
	atomic_t                 rt_genid;

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

struct net *get_net_ns_by_pid(pid_t pid);
struct net *get_net_ns_by_fd(int pid);

int register_pernet_subsys(struct pernet_operations *);
int register_pernet_device(struct pernet_operations *);

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


/**************************
 ** linux/seqlock.h **
 **************************/

typedef struct seqcount {
	unsigned sequence;
} seqcount_t;

unsigned read_seqbegin(const seqlock_t *sl);
unsigned read_seqretry(const seqlock_t *sl, unsigned start);

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

int raw_notifier_chain_register(struct raw_notifier_head *nh,
                                struct notifier_block *n);

int raw_notifier_call_chain(struct raw_notifier_head *nh,
                            unsigned long val, void *v);

int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                     struct notifier_block *n);

int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long val, void *v);

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


/*********************
 ** linux/if_vlan.h **
 *********************/

enum { VLAN_HLEN = 4 };

struct vlan_hdr
{
	__be16  h_vlan_encapsulated_proto;
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
struct sk_buff *__vlan_put_tag(struct sk_buff *skb, u16 vlan_tci);
struct sk_buff *vlan_untag(struct sk_buff *skb);


/********************
 ** net/checksum.h **
 ********************/

#define CSUM_MANGLED_0 ((__sum16)0xffff)

__wsum csum_and_copy_from_user(const void __user *src, void *dst,
                               int len, __wsum sum, int *err_ptr);
__wsum csum_add(__wsum csum, __wsum addend);
__wsum csum_block_add(__wsum csum, __wsum csum2, int offset);
__wsum csum_sub(__wsum csum, __wsum addend);

/*****************************
 ** uapi/linux/net_tstamp.h **
 *****************************/

#ifdef __cplusplus
#define class device_class
#endif
#include <uapi/linux/if_link.h>
#ifdef __cplusplus
#undef class
#endif
#include <net/netlink.h>

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

void rtmsg_ifinfo(int type, struct net_device *dev, unsigned change);


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


/********************************
 ** uapi/asm-generic/sockios.h **
 ********************************/

enum {
	SIOCATMARK   = 0x8905,
	SIOCGSTAMP   = 0x8906,
	SIOCGSTAMPNS = 0x8907,
};


/****************
 ** linux/ip.h **
 ****************/

struct ip_hdr;

struct iphdr *ip_hdr(const struct sk_buff *skb);


/********************************
 ** uapi/linux/netfilter_arp.h **
 ********************************/

enum {
	NF_ARP_IN  = 0,
	NF_ARP_OUT = 1,
};


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
 ** linux/inet_diag.h **
 ***********************/


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
};


/********************
 ** net/inet_ecn.h **
 ********************/

enum {
	INET_ECN_NOT_ECT = 0,
};



/*****************
 ** net/xfrm4.h **
 *****************/

int xfrm4_udp_encap_rcv(struct sock *sk, struct sk_buff *skb);


/*********************
 ** linux/netpoll.h **
 *********************/

struct napi_struct;

void *netpoll_poll_lock(struct napi_struct *napi);


/************************
 ** net/ethernet/eth.c **
 ************************/

extern const struct header_ops eth_header_ops;


/***********************
 ** linux/netfilter.h **
 ***********************/

#define NF_HOOK(pf, hook, skb, indev, outdev, okfn) (okfn)(skb)
#define NF_HOOK_COND(pf, hook, skb, indev, outdev, okfn, cond) (okfn)(skb)


/****************
 ** linux/in.h **
 ****************/

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

#include <uapi/linux/in6.h>


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


/**
 * Misc
 */

void set_sock_wait(struct socket *sock, unsigned long ptr);
int socket_check_state(struct socket *sock);
void log_sock(struct socket *sock);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _LX_EMUL_H_ */

