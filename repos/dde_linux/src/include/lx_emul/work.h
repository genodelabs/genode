/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/***********************
 ** linux/workqueue.h **
 ***********************/

enum {
	WQ_UNBOUND       = 1 << 1,
	WQ_FREEZABLE     = 1 << 2,
	WQ_MEM_RECLAIM   = 1 << 3,
	WQ_HIGHPRI       = 1 << 4,
	WQ_CPU_INTENSIVE = 1 << 5,
};

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	atomic_long_t data;
	work_func_t func;
	struct list_head entry;
	struct workqueue_struct *wq;
};

struct workqueue_struct { void *task; };

struct delayed_work {
	struct timer_list timer;
	struct work_struct work;
	struct workqueue_struct *wq;
};

bool cancel_work_sync(struct work_struct *work);
bool cancel_delayed_work_sync(struct delayed_work *work);
bool cancel_delayed_work(struct delayed_work *dwork);
int  schedule_delayed_work(struct delayed_work *work, unsigned long delay);
int  schedule_work(struct work_struct *work);
void flush_scheduled_work(void);

bool flush_work(struct work_struct *work);
bool flush_work_sync(struct work_struct *work);

void delayed_work_timer_fn(struct timer_list *t);

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
                timer_setup(&(_work)->timer, delayed_work_timer_fn, 0); \
        } while (0)


/* dummy for queue_delayed_work call in storage/usb.c */
#define system_freezable_wq 0

struct workqueue_struct *create_singlethread_workqueue(const char *name);
struct workqueue_struct *alloc_ordered_workqueue(const char *fmt, unsigned int flags, ...) __printf(1, 3);
struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags,
                                         int max_active, ...) __printf(1, 4);
void destroy_workqueue(struct workqueue_struct *wq);
void flush_workqueue(struct workqueue_struct *wq);
bool queue_delayed_work(struct workqueue_struct *, struct delayed_work *, unsigned long);
bool flush_delayed_work(struct delayed_work *dwork);
bool queue_work(struct workqueue_struct *wq, struct work_struct *work);
struct work_struct *current_work(void);
void drain_workqueue(struct workqueue_struct *);

#define DECLARE_DELAYED_WORK(n, f) \
	struct delayed_work n = { .work = { .func = f }, .timer = { .function = 0 } }

bool mod_delayed_work(struct workqueue_struct *, struct delayed_work *,
                      unsigned long);

static inline struct delayed_work *to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}

extern struct workqueue_struct *system_wq;
extern struct workqueue_struct *system_unbound_wq;
extern struct workqueue_struct *system_long_wq;

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


/******************
 ** linux/wait.h **
 ******************/

typedef struct wait_queue_entry wait_queue_entry_t;
typedef int (*wait_queue_func_t)(wait_queue_entry_t *, unsigned, int, void *);
typedef struct wait_queue_head {
	spinlock_t  lock;
	void       *list;
	/*
	 * Reserve memory for a 'Wait_list' object, which needs to be
	 * freed together with the 'wait_queue_head_t' object.
	 *
	 * This implementation relies on the currently given fact that
	 * 'Wait_list' does not need to have a destructor called.
	 */
	char        wait_list_reserved[8];
} wait_queue_head_t;
struct wait_queue_entry {
	unsigned int		flags;
	void			*private;
	wait_queue_func_t	func;
	struct list_head entry;
};

void init_wait_entry(struct wait_queue_entry *, int);

#define DEFINE_WAIT(name) \
	wait_queue_entry_t name;

#define __WAIT_QUEUE_HEAD_INITIALIZER(name) { 0 }

#define DECLARE_WAITQUEUE(name, tsk) \
	wait_queue_entry_t name

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	wait_queue_head_t name = __WAIT_QUEUE_HEAD_INITIALIZER(name)

#define DEFINE_WAIT_FUNC(name, function) \
	wait_queue_entry_t name

/* simplified signature */
void __wake_up(wait_queue_head_t *q, bool all);

#define wake_up(x)                   __wake_up(x, false)
#define wake_up_all(x)               __wake_up(x, true)
#define wake_up_all_locked(x)        __wake_up(x, true)
#define wake_up_interruptible(x)     __wake_up(x, false)
#define wake_up_interruptible_all(x) __wake_up(x, true)

void init_waitqueue_head(wait_queue_head_t *);
int waitqueue_active(wait_queue_head_t *);

/* void wake_up_interruptible(wait_queue_head_t *); */
void wake_up_interruptible_sync_poll(wait_queue_head_t *, int);
void wake_up_interruptible_poll(wait_queue_head_t *, int);

void prepare_to_wait(wait_queue_head_t *, wait_queue_entry_t *, int);
void prepare_to_wait_exclusive(wait_queue_head_t *, wait_queue_entry_t *, int);
void finish_wait(wait_queue_head_t *, wait_queue_entry_t *);

int  autoremove_wake_function(wait_queue_entry_t *, unsigned, int, void *);
void add_wait_queue(wait_queue_head_t *, wait_queue_entry_t *);
void add_wait_queue_exclusive(wait_queue_head_t *, wait_queue_entry_t *);
void remove_wait_queue(wait_queue_head_t *, wait_queue_entry_t *);

/* our wait event implementation - it's okay as value */
void ___wait_event(wait_queue_head_t*);

#define __wait_event(wq) ___wait_event(&wq)
#define _wait_event(wq, condition) while (!(condition)) { __wait_event(wq); }
#define wait_event(wq, condition) ({ _wait_event(wq, condition); })
#define wait_event_interruptible(wq, condition) ({ _wait_event(wq, condition); 0; })
#define wait_event_interruptible_locked(wq, condition) ({ _wait_event(wq, condition); 0; })

#define _wait_event_timeout(wq, condition, timeout) \
	({ int res = 1;                                 \
		prepare_to_wait(&wq, 0, 0);                 \
		while (1) {                                 \
			if ((condition) || !res) {              \
				break;                              \
			}                                       \
			res = schedule_timeout(timeout);        \
		}                                           \
		finish_wait(&wq, 0);                        \
		res;                                        \
	})

#define wait_event_timeout(wq, condition, timeout)               \
	({                                                           \
		int ret = _wait_event_timeout(wq, (condition), timeout); \
		ret;                                                     \
	})
