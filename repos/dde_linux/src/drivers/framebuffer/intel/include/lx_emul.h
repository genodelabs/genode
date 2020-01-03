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

#define DEBUG_LINUX_PRINTK 0

#include <lx_emul/compiler.h>
#include <lx_emul/printf.h>
#include <lx_emul/bug.h>
#include <lx_emul/atomic.h>

static inline void atomic_or(int i, atomic_t *v) {
	v->counter = v->counter | i; }
void atomic_andnot(int, atomic_t *);

#define smp_mb__before_atomic_inc() barrier()
#define smp_mb__before_atomic() barrier()

void atomic_set_mask(unsigned int mask, atomic_t *v);
#define atomic_set_release(v, i) atomic_set((v), (i))

#include <lx_emul/barrier.h>
#include <lx_emul/types.h>

typedef unsigned long kernel_ulong_t;
typedef unsigned int  u_int;
typedef long          ptrdiff_t;
typedef unsigned __bitwise slab_flags_t;

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]


/***************************
 ** asm-generic/barrier.h **
 ***************************/

#define smp_load_acquire(p)     *(p)


/************************
 ** uapi/linux/types.h **
 ************************/

typedef __u16 __le16;
typedef __u16 __be16;
typedef __u32 __le32;
typedef __u32 __be32;
typedef __u64 __le64;
typedef __u64 __be64;

typedef unsigned __poll_t;

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
struct page *virt_to_page(const void *addr);

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

/**********************
 ** linux/compiler.h **
 **********************/

#include <lx_emul/compiler.h>

#define prefetchw(x) __builtin_prefetch(x,1)

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
#include <lx_emul/kernel.h>

#define SIZE_MAX (~(size_t)0)
#define U64_MAX  ((u64)~0ULL)
#define U16_MAX  ((u16)~0U)

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
bool PageSlab(struct page *page);


/********************
 ** linux/printk.h **
 ********************/

int hex_dump_to_buffer(const void *buf, size_t len,
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
void preempt_enable(void);
void preempt_disable(void);


/**********************
 ** linux/irqflags.h **
 **********************/

/* needed bu i2c-core.c */
bool irqs_disabled();

void local_irq_enable();
void local_irq_disable();


/********************
 ** linux/kernel.h **
 ********************/

#define min3(x, y, z) min((typeof(x))min(x, y), z)

//	typecheck(u64, x); 
#define u64_to_user_ptr(x) ({ \
	(void __user *)(uintptr_t)x; \
})
char *kvasprintf(gfp_t, const char *, va_list);

#define U32_MAX  ((u32)~0U)

#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)

#define TAINT_MACHINE_CHECK 4
#define TAINT_WARN          9

/**********************
 ** sched/wait_bit.c **
 **********************/
void wake_up_bit(void *, int);
int wait_on_bit(unsigned long *, int, unsigned);
int wait_on_bit_timeout(unsigned long *, int, unsigned, unsigned long);


/******************
 ** linux/kref.h **
 ******************/

struct kref;
unsigned int kref_read(const struct kref*);



/*********************
 ** linux/jiffies.h **
 *********************/

#include <lx_emul/jiffies.h>

#define time_before(a,b)	time_after(b,a)
#define time_before_eq(a,b)	time_after_eq(b,a)

#define time_in_range(a,b,c) \
	(time_after_eq(a,b) &&   \
	 time_before_eq(a,c))

u64 nsecs_to_jiffies(u64);
extern u64 nsecs_to_jiffies64(u64 n);

static inline u64 get_jiffies_64(void) { return jiffies; }

#include <lx_emul/spinlock.h>
#include <lx_emul/semaphore.h>

#include <lx_emul/mutex.h>

LX_MUTEX_INIT_DECLARE(bridge_lock);
LX_MUTEX_INIT_DECLARE(core_lock);

#define bridge_lock LX_MUTEX(bridge_lock)
#define core_lock   LX_MUTEX(core_lock)


static inline int mutex_lock_interruptible(struct mutex *lock) {
	mutex_lock(lock);
	return 0;
}

void mutex_lock_nest_lock(struct mutex *, struct mutex *);

void might_lock(struct mutex *);


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
struct timespec ns_to_timespec(const s64 nsec);
s64 timespec_to_ns(const struct timespec *ts);


/*******************
 ** linux/timer.h **
 *******************/

typedef int clockid_t;

#include <lx_emul/timer.h>

#define del_singleshot_timer_sync(t) del_timer_sync(t)

void timer_setup(struct timer_list *, void (*func)(struct timer_list *), unsigned int);

#include <lx_emul/work.h>

extern bool flush_delayed_work(struct delayed_work *dwork);

extern unsigned long timespec_to_jiffies(const struct timespec *value);

#define wait_event_interruptible_timeout wait_event_timeout

#define setup_timer_on_stack setup_timer

unsigned long round_jiffies_up_relative(unsigned long j);

extern int mod_timer_pinned(struct timer_list *timer, unsigned long expires);

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

#define TIMER_IRQSAFE 0x00200000

/***********************
 ** linux/workqueue.h **
 ***********************/

enum {
	WORK_STRUCT_PENDING_BIT = 0,
};

#define work_data_bits(work) ((unsigned long *)(&(work)->data))

#define work_pending(work) \
        test_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))

#define delayed_work_pending(w) \
         work_pending(&(w)->work)

#ifndef CONFIG_DEBUG_OBJECTS_WORK
#define INIT_WORK_ONSTACK(_work, _func) while(0) { }
static inline void destroy_work_on_stack(struct work_struct *work) { }
#endif

void INIT_DELAYED_WORK_ONSTACK(void *, void *);
void destroy_delayed_work_on_stack(struct delayed_work *);

unsigned int work_busy(struct work_struct *);

/*******************
 ** linux/sched.h **
 *******************/

enum { TASK_COMM_LEN = 16 };

enum {
	TASK_RUNNING         = 0x0,
	TASK_INTERRUPTIBLE   = 0x1,
	TASK_UNINTERRUPTIBLE = 0x2,
	TASK_NORMAL          = TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE,
};

#define	MAX_SCHEDULE_TIMEOUT LONG_MAX

struct mm_struct;
struct task_struct {
	struct mm_struct *mm;
	char comm[16]; /* only for debug output */
	unsigned pid;
	int      prio;
	volatile long state;
};

signed long schedule_timeout(signed long timeout);
void __set_current_state(int state);
int signal_pending(struct task_struct *p);
void schedule(void);
int wake_up_process(struct task_struct *tsk);
int wake_up_state(struct task_struct *tsk, unsigned int state);

/* normally declared in linux/smp.h, included by sched.h */
int on_each_cpu(void (*func) (void *info), void *info, int wait);
#define get_cpu() 0
#define put_cpu()
#define smp_processor_id() 0

/* normally defined in asm/current.h, included by sched.h */
extern struct task_struct *current;

void yield(void);

extern signed long schedule_timeout_uninterruptible(signed long timeout);

extern u64 local_clock(void);
extern bool need_resched(void);
extern int signal_pending_state(long state, struct task_struct *p);

struct pid;
extern struct pid *task_pid(struct task_struct *task);

void  cond_resched(void);

void set_current_state(int);
long io_schedule_timeout(long);

struct sched_param {
	int sched_priority;
};

#define SCHED_FIFO 1

int sched_setscheduler_nocheck(struct task_struct *, int, const struct sched_param *);

/************************
 ** linux/completion.h **
 ************************/

#include <lx_emul/completion.h>

struct completion {
	unsigned done;
	void * task;
};

long __wait_completion(struct completion *work, unsigned long);

/*********************
 ** linux/raid/pq.h **
 *********************/

void cpu_relax(void); /* i915_dma.c */
#define cpu_relax_lowlatency() cpu_relax()


/*************************
 ** linux/bottom_half.h **
 *************************/

void local_bh_disable(void);
void local_bh_enable(void);


/*****************
 ** linux/panic **
 *****************/

enum lockdep_ok { LOCKDEP_STILL_OK };
void add_taint(unsigned, enum lockdep_ok);


/*******************
 ** linux/delay.h **
 *******************/

void msleep(unsigned int);
void udelay(unsigned long);
void mdelay(unsigned long);
void ndelay(unsigned long);

void usleep_range(unsigned long min, unsigned long max); /* intel_dp.c */


/*************************
 ** linux/scatterlist.h **
 *************************/

struct scatterlist;

#include <lx_emul/kobject.h>

enum kobject_action {
	KOBJ_CHANGE,
};

int kobject_uevent_env(struct kobject *kobj, enum kobject_action action, char *envp[]);


/************************
 ** asm/memory_model.h **
 ************************/

dma_addr_t page_to_pfn(struct page *page);
struct page * pfn_to_page(dma_addr_t);


/*********************
 ** linux/pagemap.h **
 *********************/

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

#define PAGE_KERNEL    ((pgprot_t) {0}) /* XXX */
#define PAGE_KERNEL_IO ((pgprot_t) {1}) /* XXX */

extern pgprot_t pgprot_writecombine(pgprot_t prot);
extern pgprot_t pgprot_decrypted(pgprot_t prot);


/*********************
 ** linux/kthread.h **
 *********************/

void kthread_parkme(void);
int kthread_park(struct task_struct *);
void kthread_unpark(struct task_struct *);
bool kthread_should_park(void);
bool kthread_should_stop(void);
int kthread_stop(struct task_struct *k);

struct task_struct *kthread_create_on_node(int (*threadfn)(void *data),
					   void *data,
					   int node,
					   const char namefmt[], ...);

/********************
 ** linux/mmzone.h **
 ********************/

#define MAX_ORDER 11

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

struct mm_struct { struct rw_semaphore mmap_sem; };


/**********************
 ** linux/shrinker.h **
 **********************/

struct shrinker { int DUMMY; };

/****************
 ** linux/mm.h **
 ****************/

enum {
	VM_FAULT_OOM    = 0x001,
	VM_FAULT_SIGBUS = 0x002,
	VM_FAULT_NOPAGE = 0x100,
	VM_PFNMAP       = 0x00000400,
	VM_IO           = 0x00004000,
	VM_DONTEXPAND   = 0x00040000,
	VM_NORESERVE    = 0x00200000,
	VM_DONTDUMP     = 0x04000000,
};

enum { FAULT_FLAG_WRITE = 0x1 };

enum { DEFAULT_SEEKS = 2 };

#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

#define offset_in_page(p) ((unsigned long)(p) & ~PAGE_MASK)

struct vm_fault {
	struct vm_area_struct *vma;
	unsigned int flags;
	unsigned long address;
};

int set_page_dirty(struct page *page);

void get_page(struct page *page);
void put_page(struct page *page);

extern unsigned long totalram_pages;


struct vm_area_struct;

struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	int (*fault)(struct vm_fault *vmf);
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

void *kvmalloc(size_t, gfp_t);
void *kvmalloc_array(size_t, size_t, gfp_t);

void unmap_mapping_range(struct address_space *, loff_t const, loff_t const, int);

unsigned long vma_pages(struct vm_area_struct *);

#include <asm/agp.h>


/***************
 ** asm/smp.h **
 ***************/

static inline void wbinvd() { }
static inline int wbinvd_on_all_cpus() { return 0; }


/**********************************
 ** x86/include/asm/set_memory.h **
 **********************************/

int set_memory_wb(unsigned long addr, int numpages);
int set_memory_uc(unsigned long addr, int numpages);
int set_pages_array_uc(struct page **pages, int addrinarray);

/*********************
 ** linux/vmalloc.h **
 *********************/

/* needed by agp/backend.c */
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
	SLAB_HWCACHE_ALIGN   = 0x00002000ul,
	SLAB_RECLAIM_ACCOUNT = 0x00020000ul,
	SLAB_PANIC           = 0x00040000ul,
	SLAB_TYPESAFE_BY_RCU = 0x00080000ul,
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

#define KMEM_CACHE(__struct, __flags) kmem_cache_create(#__struct,\
		sizeof(struct __struct), __alignof__(struct __struct),\
		(__flags), NULL)

void  *kmem_cache_alloc(struct kmem_cache *, gfp_t);


/**********************
 ** linux/kmemleak.h **
 **********************/

#ifndef CONFIG_DEBUG_KMEMLEAK
static inline void kmemleak_update_trace(const void *ptr) { }
static inline void kmemleak_alloc(const void *ptr, size_t size, int min_count,
                                  gfp_t gfp) { }
static inline void kmemleak_free(const void *ptr) { }
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

static inline void *kmap(struct page *page) { return page_address(page); }
static inline void *kmap_atomic(struct page *page) { return kmap(page); }
static inline void  kunmap(struct page *page) { return; }
static inline void  kunmap_atomic(void *addr) { return; }
struct page *kmap_to_page(void *);


#include <lx_emul/gfp.h>

void __free_pages(struct page *page, unsigned int order);

#define __free_page(page) __free_pages((page), 0)

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);

static inline void flush_kernel_dcache_page(struct page *page) { }

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

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);

#define __get_free_page(gfp_mask) \
		__get_free_pages((gfp_mask), 0)

void free_pages(unsigned long addr, unsigned int order);
#define free_page(addr) free_pages((addr), 0)

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
void dma_buf_put(struct dma_buf *);


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

struct dev_pm_domain;

#define SET_RUNTIME_PM_OPS(suspend_fn, resume_fn, idle_fn)
#define DPM_FLAG_NEVER_SKIP	BIT(0)

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
	struct dev_pm_info        power;
	struct dev_archdata       archdata;
	struct bus_type          *bus;
	struct device_node       *of_node;
	struct fwnode_handle     *fwnode;
};

struct device_attribute {
	struct attribute attr;
};

struct lock_class_key { int dummy; };

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
	const struct of_device_id*of_match_table;
	const struct acpi_device_id *acpi_match_table;
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

void dev_pm_set_driver_flags(struct device *, u32);

/* needed by linux/i2c.h */
struct acpi_device;

struct acpi_dev_node { struct acpi_device *companion; };


/*********************
 ** acpi/acpi_bus.h **
 *********************/

typedef char acpi_bus_id[8];
typedef char acpi_device_class[20];

struct acpi_bus_event {
	struct list_head node;
	acpi_device_class device_class;
	acpi_bus_id bus_id;
	u32 type;
	u32 data;
};


/****************
 ** linux/io.h **
 ****************/

enum {
	MEMREMAP_WB = 1 << 0,
};

#define writel_relaxed(v, a) writel(v, a)
#define iowrite32(v, addr)   writel((v), (addr))
#define ioread32(addr)       readl(addr)

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


/**************************************
 ** arch/x86/include/asm/string_64.h **
 **************************************/

#ifdef __x86_64__
static inline void *memset64(uint64_t *s, uint64_t v, size_t n)
{
	long d0, d1;
	asm volatile("rep\n\t"
		     "stosq"
		     : "=&c" (d0), "=&D" (d1)
		     : "a" (v), "1" (s), "0" (n)
		     : "memory");
	return s;
}
#else
static inline void *memset64(uint64_t *s, uint64_t v, size_t count)
{
	uint64_t *xs = s;

	while (count--)
		*xs++ = v;
	return s;
}
#endif


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

void pagefault_disable(void);
void pagefault_enable(void);

/*************************
 ** linux/dma-mapping.h **
 *************************/

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

int dma_set_coherent_mask(struct device *dev, u64 mask);


/************************
 ** linux/io-mapping.h **
 ************************/

#define pgprot_noncached(prot) prot

#include <lx_emul/mmio.h>


/********************
 ** linux/random.h **
 ********************/

unsigned int get_random_int(void);
unsigned long get_random_long(void);


/********************
 ** linux/ioport.h **
 ********************/

#include <lx_emul/ioport.h>

extern struct resource iomem_resource;

int request_resource(struct resource *root, struct resource *); /* intel-gtt.c */

int release_resource(struct resource *r);  /* i915_dma.c */

unsigned long resource_type(const struct resource *);

#define IORESOURCE_ROM_SHADOW (1<<1)
#define IORESOURCE_BITS 0x000000ff

/*****************
 ** linux/pci.h **
 *****************/

enum {
	PCI_STD_RESOURCE_END = 5,
	PCI_ROM_RESOURCE,
	PCI_NUM_RESOURCES,
	DEVICE_COUNT_RESOURCE = PCI_NUM_RESOURCES,
};

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
struct irq_data *irq_get_irq_data(unsigned int);


/**********************
 ** linux/irq_work.h **
 **********************/

struct irq_work { int DUMMY; };
bool irq_work_queue(struct irq_work *work);
void init_irq_work(struct irq_work *work, void (*func)(struct irq_work *));

/*********************
 ** linux/hardirq.h **
 *********************/

extern void synchronize_irq(unsigned int irq);

/*****************
 ** linux/irq.h **
 *****************/

#include <linux/irqhandler.h>

struct irq_chip { int DUMMY; };
void irqd_set_trigger_type(struct irq_data *, u32);
void irq_set_chip_and_handler(unsigned int, struct irq_chip *,
                              irq_flow_handler_t);
void handle_simple_irq(struct irq_desc *);
extern struct irq_chip dummy_irq_chip;

/************************
 ** linux/capability.h **
 ************************/

#define CAP_SYS_ADMIN 21
#define CAP_SYS_NICE  23

bool capable(int);


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

enum {
	NOTIFY_DONE      = 0x0000,
	NOTIFY_OK        = 0x0001,
	NOTIFY_STOP_MASK = 0x8000,
	NOTIFY_BAD       = (NOTIFY_STOP_MASK|0x0002),
};

struct atomic_notifier_head { unsigned dummy; };

extern int atomic_notifier_chain_unregister(struct atomic_notifier_head *nh, struct notifier_block *nb);
extern int atomic_notifier_chain_register(struct atomic_notifier_head *nh, struct notifier_block *nb);

#define ATOMIC_INIT_NOTIFIER_HEAD(name) do { } while (0)

int atomic_notifier_call_chain(struct atomic_notifier_head *nh,
                               unsigned long val, void *v);

/*******************
 ** acpi/button.h **
 *******************/

int acpi_lid_open(void);
int acpi_lid_notifier_register(struct notifier_block *nb);
int acpi_lid_notifier_unregister(struct notifier_block *nb);


/*********************
 ** linux/console.h **
 *********************/

static inline bool vgacon_text_force(void) { return false; }


/******************
 ** acpi/video.h **
 ******************/

#define ACPI_VIDEO_CLASS "video"

int acpi_video_register(void);
void acpi_video_unregister(void);

enum acpi_backlight_type {
	acpi_backlight_native = 3,
};
enum acpi_backlight_type acpi_video_get_backlight_type(void);


/******************
 ** acpi/video.h **
 ******************/

int register_acpi_notifier(struct notifier_block *);
int unregister_acpi_notifier(struct notifier_block *);


/**********************
 ** linux/memremap.h **
 **********************/

void *memremap(resource_size_t offset, size_t size, unsigned long flags);
void memunmap(void *addr);


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
	atomic_long_t         f_count;
	struct inode         *f_inode;
	struct address_space *f_mapping;
	void                 *private_data;
};

struct poll_table_struct;
typedef struct poll_table_struct poll_table;

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
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*open) (struct inode *, struct file *);
	int (*release) (struct inode *, struct file *);
};

enum { PROT_READ  = 0x1, PROT_WRITE = 0x2 };

enum { MAP_SHARED = 0x1 };

loff_t noop_llseek(struct file *file, loff_t offset, int whence);

struct inode *file_inode(struct file *f);

unsigned long invalidate_mapping_pages(struct address_space *mapping,
		pgoff_t start, pgoff_t end);

int pagecache_write_begin(struct file *, struct address_space *, loff_t,
                          unsigned, unsigned, struct page **, void **);
int pagecache_write_end(struct file *, struct address_space *, loff_t,
                        unsigned, unsigned, struct page *, void *);


/******************************
 ** include/asm/set_memory.h **
 ******************************/

int set_pages_array_wc(struct page **, int);
int set_pages_array_wb(struct page **, int);


/**************************
 ** linux/stop_machine.h **
 **************************/
struct cpumask;

typedef int (*cpu_stop_fn_t)(void *arg);

int stop_machine(cpu_stop_fn_t, void *, const struct cpumask *);


/*****
 *
 */
struct rcu_head { int dummy; };

void clflush(volatile void *);
void clflushopt(volatile void *);


/*********************
 ** linux/vmalloc.h **
 *********************/

void vunmap(const void *);
void *vmap(struct page **, unsigned int, unsigned long, pgprot_t);


/*********************
 ** linux/seqlock.h **
 *********************/

typedef unsigned seqlock_t;

typedef struct seqcount {
	unsigned DUMMY;
} seqcount_t;

void seqlock_init (seqlock_t *);
unsigned __read_seqcount_begin(const seqcount_t *);
int __read_seqcount_retry(const seqcount_t *, unsigned);
unsigned raw_read_seqcount(const seqcount_t *);
int read_seqcount_retry(const seqcount_t *, unsigned);
void write_seqlock(seqlock_t *);
void write_sequnlock(seqlock_t *);
unsigned read_seqbegin(const seqlock_t *);
unsigned read_seqretry(const seqlock_t *, unsigned);

/*************************
 ** linux/reservation.h **
 *************************/

struct reservation_object_list {
	u32 shared_count;
	struct dma_fence *shared[];
};

struct reservation_object {
	seqcount_t seq;
	struct dma_fence *fence_excl;
	struct reservation_object_list *fence;
};
struct ww_acquire_ctx;
int reservation_object_lock(struct reservation_object *, struct ww_acquire_ctx *);
void reservation_object_unlock(struct reservation_object *);
struct dma_fence * reservation_object_get_excl_rcu(struct reservation_object *);
int reservation_object_get_fences_rcu(struct reservation_object *,
                                      struct dma_fence **, unsigned *,
                                      struct dma_fence ***);
bool reservation_object_trylock(struct reservation_object *);
void reservation_object_add_excl_fence(struct reservation_object *, struct dma_fence *);
void reservation_object_init(struct reservation_object *);
void reservation_object_fini(struct reservation_object *);
bool reservation_object_test_signaled_rcu(struct reservation_object *, bool);


/*******************
 * linux/swiotlb.h *
 *******************/

unsigned int swiotlb_max_segment(void);


/****************
 * linux/uuid.h *
 ****************/

#define	UUID_STRING_LEN 36


/**********************
 ** linux/shmem_fs.h **
 **********************/

extern void shmem_truncate_range(struct inode *inode, loff_t start, loff_t end);

extern struct page *shmem_read_mapping_page_gfp(struct address_space *mapping,
		pgoff_t index, gfp_t gfp_mask);
extern struct page *shmem_read_mapping_page( struct address_space *mapping, pgoff_t index);
struct file *shmem_file_setup(const char *, loff_t, unsigned long);
struct vfsmount;
struct file *shmem_file_setup_with_mnt(struct vfsmount *, const char *, loff_t, unsigned long);

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
	const struct backlight_ops * ops;
	struct intel_connector *connector;
};

static inline struct intel_connector* bl_get_data(struct backlight_device *bl_dev)
{
	if (bl_dev)
		return bl_dev->connector;
	return NULL;
}

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


/***********************
 ** linux/i2c-smbus.h **
 ***********************/

struct i2c_smbus_alert_setup;


/****************
 ** linux/of.h **
 ****************/

int of_alias_get_id(struct device_node *np, const char *stem);
void of_node_put(struct device_node *);
int of_property_match_string(const struct device_node *, const char *,
                             const char *);
int of_property_read_u32_index(const struct device_node *, const char *, u32,
                               u32 *);


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
void acpi_device_clear_enumerated(struct acpi_device *);
int acpi_reconfig_notifier_register(struct notifier_block *);
int acpi_reconfig_notifier_unregister(struct notifier_block *);

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

struct aperture {
	resource_size_t base;
	resource_size_t size;
};

struct apertures_struct {
	unsigned int count;
	struct aperture ranges[0];
};

static inline struct apertures_struct *alloc_apertures(unsigned int max_num) {
	void * p = kzalloc(sizeof(struct apertures_struct) + max_num *
	                   sizeof(struct aperture), GFP_KERNEL);
	struct apertures_struct *a = (struct apertures_struct *)p;
	if (!a)
		return NULL;
	a->count = max_num;
	return a;
}

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


/*******************
 ** drm/drm_pci.h **
 *******************/

struct drm_dma_handle *drm_pci_alloc(struct drm_device *, size_t, size_t);


/****************************
 ** drm/drm_modeset_lock.h **
 ****************************/

#include <video/videomode.h>

//extern struct ww_class crtc_ww_class;

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


/**********************
 ** linux/refcount.h **
 **********************/

bool refcount_dec_and_test(atomic_t *);


/*********************
 ** drm/drm_lease.h **
 *********************/

struct drm_file;
bool drm_lease_held(struct drm_file *, int);
bool _drm_lease_held(struct drm_file *, int);
uint32_t drm_lease_filter_crtcs(struct drm_file *, uint32_t);


/********************
 ** linux/time64.h **
 ********************/

typedef __s64 time64_t;
struct timespec64 {
	time64_t tv_sec;  /* seconds */
	long     tv_nsec; /* nanoseconds */
};


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
 ** linux/async.h **
 *******************/

typedef u64 async_cookie_t;
typedef void (*async_func_t) (void *data, async_cookie_t cookie);

extern async_cookie_t async_schedule(async_func_t func, void *data);
extern void async_synchronize_full(void);

/*******************
 ** linux/ktime.h **
 *******************/

#define ktime_to_ns(kt) kt

ktime_t ktime_get_raw(void);
u64 ktime_get_raw_ns(void);
extern struct timespec64 ns_to_timespec64(const s64 nsec);
#define ktime_to_timespec64(kt)		ns_to_timespec64((kt))
#define ktime_sub_ns(kt, nsval)		((kt) - (nsval))

#define ktime_sub(lhs, rhs) (lhs - rhs)

static inline s64 ktime_to_us(const ktime_t kt) {
	return kt / NSEC_PER_USEC; }

static inline s64 ktime_to_ms(const ktime_t kt) {
	return kt / NSEC_PER_MSEC; }

/*****************
 ** linux/pid.h **
 *****************/

enum pid_type { PIDTYPE_PID };

struct pid { atomic_t count; };

static inline struct pid *get_pid(struct pid *pid)
{
	if (pid)
		atomic_inc(&pid->count);
	return pid;
}
extern void put_pid(struct pid *pid);
struct pid *get_task_pid(struct task_struct *, enum pid_type);

typedef int pid_t;
pid_t pid_nr(struct pid *);


/***********************
 ** include/lockdep.h **
 ***********************/

#define MAX_LOCKDEP_SUBCLASSES 8UL
#define SINGLE_DEPTH_NESTING 1

#define lockdep_assert_held(l) do { (void)(l); } while (0)
#define lockdep_set_class_and_name(lock, key, name) \
	do { (void)(key); (void)(name); } while (0)


/**************************************
 ** arch/x86/include/asm/cpufeature* **
 **************************************/

#define cpu_has_pat 1
bool static_cpu_has(long);
bool boot_cpu_has(long);
#define X86_FEATURE_PAT			( 0*32+16) /* Page Attribute Table */
#define X86_FEATURE_CLFLUSH		( 0*32+19) /* CLFLUSH instruction */


/********************
 ** linux/bitmap.h **
 ********************/

extern void bitmap_set(unsigned long *map, unsigned int start, int len);
extern void bitmap_zero(unsigned long *dst, unsigned int nbits);
extern void bitmap_or(unsigned long *dst, const unsigned long *src1,
		const unsigned long *src2, unsigned int nbits);
void bitmap_clear(unsigned long *, unsigned int, unsigned int);

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
bool preemptible();


/***********************
 ** linux/interrupt.h **
 ***********************/

enum {
	TASKLET_STATE_SCHED,
};

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
void pwm_put(struct pwm_device *);
void pwm_apply_args(struct pwm_device *);


/********************
 * linux/property.h *
 ********************/

struct property_entry;
struct property_entry * property_entries_dup(const struct property_entry *);
int device_add_properties(struct device *, const struct property_entry *);
void device_remove_properties(struct device *);
int device_property_read_u32_array(struct device *, const char *, u32 *,
                                   size_t);
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

void rcu_read_lock(void);
void rcu_read_unlock(void);

void rcu_barrier(void);
void synchronize_rcu(void);
void call_rcu(struct rcu_head *, void (*)(struct rcu_head *));

#define RCU_INIT_POINTER(p, v) do { p = (typeof(*v) *)v; } while (0)

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


/************************
 ** linux/perf_event.h **
 ************************/

struct pmu { unsigned dummy; };

/*************************
 ** drm/drm_fb_helper.h **
 *************************/

struct drm_connector;
struct drm_fb_helper { unsigned dummy; };

int drm_fb_helper_remove_conflicting_framebuffers(struct apertures_struct *,
                                                  const char *, bool);
int drm_fb_helper_add_one_connector(struct drm_fb_helper *fb_helper,
                                    struct drm_connector *connector);
int drm_fb_helper_remove_one_connector(struct drm_fb_helper *fb_helper,
                                       struct drm_connector *connector);
void drm_fb_helper_set_suspend_unlocked(struct drm_fb_helper *fb_helper,
                                        bool suspend);

/********************
 ** drm/drm_file.h **
 ********************/

struct drm_file;


/**********************
 ** drm/drm_atomic.h **
 **********************/

struct drm_atomic_state;


/*********************
 ** drm/drm_sysfs.h **
 *********************/
void drm_sysfs_hotplug_event(struct drm_device *);


/*******************
 ** drm/drm_drv.h **
 *******************/

void drm_printk(const char *, unsigned int, const char *, ...);

/* XXX */
void put_unused_fd(unsigned int);
void fd_install(unsigned int, struct file *);
void fput(struct file *);
enum { O_CLOEXEC = 0xbadaffe };
int get_unused_fd_flags(unsigned);


/***********************
 ** linux/irqdomain.h **
 ***********************/

typedef unsigned long irq_hw_number_t;

struct irq_domain;
struct irq_domain_ops {
	int (*map)(struct irq_domain *, unsigned int, irq_hw_number_t);
};

unsigned int irq_find_mapping(struct irq_domain *, irq_hw_number_t);
unsigned int irq_create_mapping(struct irq_domain *, irq_hw_number_t);
void irq_dispose_mapping(unsigned int);
void irq_domain_remove(struct irq_domain *);

struct irq_domain *irq_domain_create_linear(struct fwnode_handle *,
                                            unsigned int,
                                            const struct irq_domain_ops *,
                                            void *);


/*********************
 ** linux/irqdesc.h **
 *********************/

int generic_handle_irq(unsigned int);


/*******************
 ** Configuration **
 *******************/

#define CONFIG_AGP                             1
#define CONFIG_AGP_INTEL                       1
#define CONFIG_BACKLIGHT_CLASS_DEVICE          1
#define CONFIG_DRM_I915                        1
#define CONFIG_DRM_I915_DEBUG                  0
#define CONFIG_DRM_I915_DEBUG_GEM              0
#define CONFIG_DRM_I915_PRELIMINARY_HW_SUPPORT 1
#define CONFIG_EXTRA_FIRMWARE                 ""
#define CONFIG_I2C                             1
#define CONFIG_I2C_BOARDINFO                   1
#define CONFIG_OF_DYNAMIC                      0
#define CONFIG_PCI                             1
#define CONFIG_BASE_SMALL                      0
#define CONFIG_DRM_LOAD_EDID_FIRMWARE          0
#define CONFIG_ARCH_HAS_SG_CHAIN               1
#define CONFIG_X86                             1


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

#define trace_intel_disable_plane(...)     while (0) { }
#define trace_intel_engine_notify(...)     while (0) { }
#define trace_intel_update_plane(...)      while (0) { }
#define trace_intel_cpu_fifo_underrun(...) while (0) { }
#define trace_intel_pch_fifo_underrun(...) while (0) { }

#define trace_intel_memory_cxsr(...)       while (0) { }
#define trace_vlv_wm(...)                  while (0) { }
#define trace_g4x_wm(...)                  while (0) { }
#define trace_vlv_fifo_size(...)           while (0) { }
#define trace_i915_gem_request_in(...)     while (0) { }
#define trace_i915_gem_request_out(...)    while (0) { }
#define trace_dma_fence_enable_signal(...) while (0) { }
#define trace_i915_gem_request_execute(...) while (0) { }
#define trace_i915_gem_request_submit(...)  while (0) { }

#define trace_dma_fence_init(...)           while (0) { }
#define trace_dma_fence_signaled(...)       while (0) { }
#define trace_dma_fence_destroy(...)        while (0) { }
#define trace_dma_fence_wait_start(...)     while (0) { }
#define trace_dma_fence_wait_end(...)       while (0) { }


/*********************
 ** linux/stringify **
 *********************/

#define __stringify(x...) #x


/******************
 ** linux/wait.h **
 ******************/

void __add_wait_queue_entry_tail(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry);
void __init_waitqueue_head(struct wait_queue_head *wq_head, const char *name, struct lock_class_key *);


/*******************
 ** linux/cache.h **
 *******************/
unsigned cache_line_size();


/***********************
 ** linux/interrupt.h **
 ***********************/

struct tasklet_struct
{
	unsigned long state;
	void (*func)(unsigned long);
	unsigned long data;
};

void tasklet_schedule(struct tasklet_struct *);
void tasklet_hi_schedule(struct tasklet_struct *);
void tasklet_kill(struct tasklet_struct *);
void tasklet_init(struct tasklet_struct *, void (*)(unsigned long), unsigned long);
void tasklet_enable(struct tasklet_struct *);
void tasklet_disable(struct tasklet_struct *);

void enable_irq(unsigned int);
void disable_irq(unsigned int);

#include <linux/math64.h>

#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_H_ */
