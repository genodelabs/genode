/*
 * \brief  USB HID driver Linux emulation environment
 * \author Stefan Kalkowski
 * \date   2018-06-13
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SRC__DRIVERS__USB_HID__LX_EMUL_H_
#define _SRC__DRIVERS__USB_HID__LX_EMUL_H_

#include <base/fixed_stdint.h>
#include <stdarg.h>

#include <legacy/lx_emul/extern_c_begin.h>

#include <legacy/lx_emul/compiler.h>
#include <legacy/lx_emul/printf.h>
#include <legacy/lx_emul/types.h>
#include <legacy/lx_emul/kernel.h>

enum { HZ = 100UL };

#include <legacy/lx_emul/jiffies.h>
#include <legacy/lx_emul/time.h>
#include <legacy/lx_emul/bitops.h>

typedef int clockid_t;

#include <legacy/lx_emul/timer.h>
#include <legacy/lx_emul/spinlock.h>

#include <legacy/lx_emul/mutex.h>

LX_MUTEX_INIT_DECLARE(dquirks_lock);
LX_MUTEX_INIT_DECLARE(input_mutex);
LX_MUTEX_INIT_DECLARE(wacom_udev_list_lock);

#define dquirks_lock         LX_MUTEX(dquirks_lock)
#define input_mutex          LX_MUTEX(input_mutex)
#define wacom_udev_list_lock LX_MUTEX(wacom_udev_list_lock)


typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u64 __be64;

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]

#include <legacy/lx_emul/byteorder.h>
#include <legacy/lx_emul/atomic.h>
#include <legacy/lx_emul/work.h>
#include <legacy/lx_emul/bug.h>
#include <legacy/lx_emul/errno.h>
#include <legacy/lx_emul/module.h>
#include <legacy/lx_emul/gfp.h>

static inline void barrier() { asm volatile ("": : :"memory"); }

#define READ_ONCE(x) x

#include <legacy/lx_emul/list.h>
#include <legacy/lx_emul/string.h>
#include <legacy/lx_emul/kobject.h>
#include <legacy/lx_emul/completion.h>

struct file
{
	unsigned int f_flags;
	void * private_data;
};

struct device;

typedef unsigned long kernel_ulong_t;
typedef struct { __u8 b[16]; } uuid_le;

struct device * get_device(struct device *dev);
void            put_device(struct device *);
void * dev_get_drvdata(const struct device *dev);
int    dev_set_drvdata(struct device *dev, void *data);

#define dev_info(dev, format, arg...) lx_printf("dev_info: " format , ## arg)
#define dev_warn(dev, format, arg...) lx_printf("dev_warn: " format , ## arg)
#define dev_err( dev, format, arg...) lx_printf("dev_err: "  format , ## arg)
#define dev_dbg( dev, format, arg...)

#define pr_info(fmt, ...)       printk(KERN_INFO fmt,    ##__VA_ARGS__)
#define pr_err(fmt, ...)        printk(KERN_ERR  fmt,    ##__VA_ARGS__)
#define pr_warn(fmt, ...)       printk(KERN_WARN fmt,    ##__VA_ARGS__)

struct semaphore {};

struct device_driver
{
	char const      *name;
	struct bus_type *bus;
	struct module   *owner;
	const char      *mod_name;
};

typedef int devt;

struct class
{
	const char *name;
	char *(*devnode)(struct device *dev, mode_t *mode);
};

struct device
{
	char const               * name;
	struct device            * parent;
	struct kobject           * kobj;
	struct device_driver     * driver;
	struct bus_type          * bus;
	dev_t                      devt;
	struct class             * class;
	const struct device_type * type;
	void (*release)(struct device *dev);
	void                     * driver_data;
	unsigned                   ref;
};

void down(struct semaphore *sem);
void up(struct semaphore *sem);

#define module_driver(__driver, __register, __unregister, ...) \
	static int __init __driver##_init(void)                    \
	{                                                          \
		return __register(&(__driver) , ##__VA_ARGS__);        \
	}                                                          \
	module_init(__driver##_init);                              \
	static void __exit __driver##_exit(void)                   \
	{                                                          \
		__unregister(&(__driver) , ##__VA_ARGS__);             \
	}                                                          \
	module_exit(__driver##_exit);

void device_lock(struct device *dev);
void device_release_driver(struct device *dev);

#define KBUILD_MODNAME ""

struct attribute
{
	const char * name;
	mode_t       mode;
};

struct device_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *dev, struct device_attribute *attr,
	                char *buf);
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,
	                 const char *buf, size_t count);
};

#define __ATTR(_name,_mode,_show,_store) { \
	.attr  = {.name = #_name, .mode = _mode }, \
	.show  = _show, \
	.store = _store, \
}

#define DEVICE_ATTR(_name, _mode, _show, _store) \
struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)

int sprintf(char *buf, const char *fmt, ...);
int kstrtoul(const char *s, unsigned int base, unsigned long *res);

void device_unlock(struct device *dev);

struct kobj_uevent_env;

struct bus_type {
	const char * name;
	const struct attribute_group **dev_groups;
	const struct attribute_group **drv_groups;
	int (*match)(struct device *dev, struct device_driver *drv);
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
};

int bus_for_each_dev(struct bus_type *bus, struct device *start, void *data, int (*fn)(struct device *dev, void *data));
int bus_for_each_drv(struct bus_type *bus, struct device_driver *start, void *data, int (*fn)(struct device_driver *, void *));
int  driver_attach(struct device_driver *drv);

struct file_operations;
struct cdev { const struct file_operations * ops; };

typedef __s64 time64_t;

struct timespec64 {
	time64_t tv_sec;  /* seconds */
	long     tv_nsec; /* nanoseconds */
};

struct timespec64 ktime_to_timespec64(const s64 nsec);

enum {
	S_IWGRP = 20,
	S_IRGRP = 40,
	S_IRUGO = 444,
	S_IWUSR = 200,
	S_IRUSR = 400,
};

struct attribute_group
{
	const char            *name;
	struct attribute     **attrs;
	struct bin_attribute **bin_attrs;
};

void kfree(const void *);
unsigned int jiffies_to_usecs(const unsigned long j);
void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);
size_t strlen(const char *s);

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

int sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);
void sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp);
static inline void devm_kfree(struct device *dev, void *p) {}

#define module_param_array_named(name, array, type, nump, perm)

void *kmalloc(size_t size, gfp_t flags);
void *kzalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);

int snprintf(char *buf, size_t size, const char *fmt, ...);
const char *dev_name(const struct device *dev);
u16 get_unaligned_le16(const void *p);
u32  get_unaligned_le32(const void *p);

void *vzalloc(unsigned long size);
void vfree(void *addr);

struct pm_message {};
typedef struct pm_message pm_message_t;

struct task_struct
{
	char comm[16];
};

extern struct task_struct *current;

struct completion
{
	unsigned int done;
	void * task;
};

long __wait_completion(struct completion *work, unsigned long timeout);

struct notifier_block;

enum {
	EPOLLIN     = 0x00000001,
	EPOLLOUT    = 0x00000004,
	EPOLLERR    = 0x00000008,
	EPOLLHUP    = 0x00000010,
	EPOLLRDNORM = 0x00000040,
	EPOLLWRNORM = 0x00000100,
	EPOLLRDHUP  = 0x00002000,
	ESHUTDOWN   = 58,
};

enum led_brightness { LED_OFF = 0, LED_FULL = 255 };

struct led_classdev
{
	const char * name;
	enum led_brightness max_brightness;
	int flags;
	void (*brightness_set)(struct led_classdev *led_cdev,
	                       enum led_brightness brightness);
	int (*brightness_set_blocking)(struct led_classdev *led_cdev,
	                               enum led_brightness brightness);
	enum led_brightness (*brightness_get)(struct led_classdev *led_cdev);
	const char *default_trigger;
	struct led_trigger *trigger;
};

struct led_trigger
{
	const char * name;
};

int sscanf(const char *, const char *, ...);

#define hid_dump_input(a,b,c)       do { } while (0)
#define hid_dump_report(a, b, c, d) do { } while (0)
#define hid_debug_register(a, b)    do { } while (0)
#define hid_debug_unregister(a)     do { } while (0)
#define hid_debug_init()            do { } while (0)
#define hid_debug_exit()            do { } while (0)

struct hid_device;

static inline int hidraw_report_event(struct hid_device *hid, u8 *data, int len) { return 0; }

int  down_trylock(struct semaphore *sem);

struct bin_attribute {
	struct attribute attr;
	size_t           size;
	ssize_t        (*read)(struct file *, struct kobject *,
	                       struct bin_attribute *, char *, loff_t, size_t);
};

struct device *kobj_to_dev(struct kobject *kobj);

static inline int hidraw_connect(struct hid_device *hid) { return -1; }
static inline void hidraw_disconnect(struct hid_device *hid) { }

struct hidraw { u32 minor; };

int  device_create_file(struct device *device, const struct device_attribute *entry);
void device_remove_file(struct device *dev, const struct device_attribute *attr);

int  mutex_lock_killable(struct mutex *lock);

struct driver_attribute {
  struct attribute attr;
};

#define DRIVER_ATTR_WO(_name) \
	struct driver_attribute driver_attr_##_name = { .attr = { .name = NULL } }

void msleep(unsigned int);

long find_next_zero_bit_le(const void *addr, unsigned long size, unsigned long offset);

int  sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp);

int  down_interruptible(struct semaphore *sem);

int scnprintf(char *buf, size_t size, const char *fmt, ...);

enum { PAGE_SIZE = 4096, };

#define __ATTRIBUTE_GROUPS(_name)                                 \
static const struct attribute_group *_name##_groups[] = {       \
        &_name##_group,                                         \
        NULL,                                                   \
}

#define ATTRIBUTE_GROUPS(_name)                                 \
static const struct attribute_group _name##_group = {           \
        .attrs = _name##_attrs,                                 \
};                                                              \
__ATTRIBUTE_GROUPS(_name)

#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = { .attr = { .name = NULL } }

struct kobj_uevent_env;

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...);

int dev_set_name(struct device *dev, const char *name, ...);
int device_add(struct device *dev);

void device_initialize(struct device *dev);

void device_enable_async_suspend(struct device *dev);

void device_del(struct device *dev);

void sema_init(struct semaphore *sem, int val);

int  driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);
int  bus_register(struct bus_type *bus);
void bus_unregister(struct bus_type *bus);

enum { CLOCK_BOOTTIME  = 7, };

struct fasync_struct;
void kill_fasync(struct fasync_struct **, int, int);

enum { SIGIO =  29 };

enum {
	POLL_IN  = 1,
	POLL_HUP = 6,
};

enum tk_offsets { TK_OFFS_BOOT = 1, };

ktime_t ktime_mono_to_real(ktime_t mono);
ktime_t ktime_mono_to_any(ktime_t tmono, enum tk_offsets offs);

static inline void rcu_read_lock(void) { }
static inline void rcu_read_unlock(void) { }

#define rcu_dereference(p) p
#define rcu_assign_pointer(p,v) p = v
#define rcu_dereference_protected(p, c) p

static inline int hidraw_init(void) { return 0; }
static inline int hidraw_exit(void) { return 0; }
static inline int hidraw_debug_init(void) { return 0; }
static inline int hidraw_debug_exit(void) { return 0; }

#define list_for_each_entry_rcu(pos, head, member) \
	list_for_each_entry(pos, head, member)

int fasync_helper(int, struct file *, int, struct fasync_struct **);

typedef unsigned fl_owner_t;

struct scatterlist;

bool lockdep_is_held(void *);

struct power_supply;
void power_supply_changed(struct power_supply *psy);

static inline u16 get_unaligned_be16(const void *p) {
	return be16_to_cpup((__be16 *)p); }

static inline void synchronize_rcu(void) { }

static inline void list_add_tail_rcu(struct list_head *n,
                                     struct list_head *head) {
	list_add_tail(n, head); }

static inline void list_del_rcu(struct list_head *entry) {
	list_del(entry); }

int  mutex_lock_interruptible(struct mutex *m);

void kvfree(const void *addr);

struct inode
{
	struct cdev * i_cdev;
};

int nonseekable_open(struct inode *inode, struct file *filp);

enum { O_NONBLOCK = 0x4000 };

typedef unsigned __poll_t;

typedef struct poll_table_struct { } poll_table;

size_t copy_to_user(void *dst, void const *src, size_t len);
long copy_from_user(void *to, const void *from, unsigned long n);
#define put_user(x, ptr) ({ lx_printf("put_user not implemented"); (0);})

int devm_add_action(struct device *dev, void (*action)(void *), void *data);

void poll_wait(struct file *f, wait_queue_head_t *w, poll_table *p);

#define get_user(x, ptr) ({  x   = *ptr; 0; })

unsigned long clear_user(void *to, unsigned long n);

#define _IOC_NRSHIFT    0
#define _IOC_TYPESHIFT  (_IOC_NRSHIFT   + 8)
#define _IOC_SIZESHIFT  (_IOC_TYPESHIFT + 8)
#define _IOC_DIRSHIFT   (_IOC_SIZESHIFT + 14)

#define _IOC_WRITE 1U
#define _IOC_READ 2U
#define _IOC_SIZEMASK   ((1 << 14) - 1)
#define _IOC_DIR(cmd) cmd
#define _IOC(dir,type,nr,size) (type+nr+sizeof(size))
#define _IOR(type,nr,size)     (type+nr+sizeof(size)+10)
#define _IOW(type,nr,size)     (type+nr+sizeof(size)+20)
#define _IOC_SIZE(nr)  (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)
#define _IOC_TYPE(cmd) cmd
#define _IOC_NR(cmd) cmd

signed long schedule_timeout(signed long timeout);

int kstrtouint(const char *s, unsigned int base, unsigned int *res);

#define find_next_zero_bit find_next_zero_bit_le

int  device_set_wakeup_enable(struct device *dev, bool enable);

struct ida {};
#define DEFINE_IDA(name) struct ida name;

loff_t no_llseek(struct file *file, loff_t offset, int whence);

struct file_operations
{
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

#define MKDEV(major, minor) 0

unsigned long int_sqrt(unsigned long x);

u64 get_unaligned_le64(const void *p);

#define U8_MAX   ((u8)~0U)
#define S8_MAX   ((s8)(U8_MAX>>1))
#define S8_MIN   ((s8)(-S8_MAX - 1))
#define U16_MAX  ((u16)~0U)
#define S16_MAX  ((s16)(U16_MAX>>1))
#define S16_MIN  ((s16)(-S16_MAX - 1))
#define U32_MAX  ((u32)~0U)
#define S32_MAX  ((s32)(U32_MAX>>1))
#define S32_MIN  ((s32)(-S32_MAX - 1))

typedef void (*dr_release_t)(struct device *dev, void *res);
void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp);
void  devres_add(struct device *dev, void *res);
int devres_release_group(struct device *dev, void *id);
void * devres_open_group(struct device *dev, void *id, gfp_t gfp);
void devres_close_group(struct device *dev, void *id);

static inline void add_input_randomness(unsigned int type, unsigned int code, unsigned int value) { }

unsigned long find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset);
#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)

char *devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...);

int devm_led_trigger_register(struct device *dev, struct led_trigger *trigger);

int devm_add_action_or_reset(struct device *dev, void (*action)(void *), void *data);

int devm_led_classdev_register(struct device *parent, struct led_classdev *led_cdev);

enum { LED_HW_PLUGGABLE = 1 << 19 };

int kstrtou8(const char *s, unsigned int base, u8 *res);

struct kobj_attribute
{
	struct attribute attr;
	void * show;
	void * store;
};

int bitmap_subset(const unsigned long *src1, const unsigned long *src2, int nbits);

struct device_type {
	const char *name;
	const struct attribute_group **groups;
	void      (*release)(struct device *dev);
	int       (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	char     *(*devnode)(struct device *dev, mode_t *mode, kuid_t *, kgid_t *);
};

char *kasprintf(gfp_t gfp, const char *fmt, ...);

void cdev_init(struct cdev *c, const struct file_operations *fops);
int cdev_device_add(struct cdev *cdev, struct device *dev);
void cdev_device_del(struct cdev *cdev, struct device *dev);

#define MINOR(dev)   ((dev) & 0xff)

typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

void  devres_free(void *res);
int   devres_destroy(struct device *dev, dr_release_t release,  dr_match_t match, void *match_data);

static inline void dump_stack(void) { }

int bitmap_weight(const unsigned long *src, unsigned int nbits);

#define list_add_rcu       list_add

int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end, gfp_t gfp_mask);
void ida_simple_remove(struct ida *ida, unsigned int id);

int class_register(struct class *cls);
void class_unregister(struct class *cls);
int  register_chrdev_region(dev_t, unsigned, const char *);
void unregister_chrdev_region(dev_t, unsigned);

int sysfs_create_files(struct kobject *kobj, const struct attribute **ptr);

static inline void led_trigger_event(struct led_trigger *trigger,
                                     enum led_brightness event) {}

typedef unsigned slab_flags_t;

struct tasklet_struct
{
	void (*func)(unsigned long);
	unsigned long data;
};

struct __una_u16 { u16 x; } __attribute__((packed));
struct __una_u32 { u32 x; } __attribute__((packed));

void subsys_input_init();
int module_evdev_init();
int module_led_init();
int module_usbhid_init();
int module_hid_init();
int module_hid_generic_init();
int module_ch_driver_init();
int module_holtek_mouse_driver_init();
int module_apple_driver_init();
int module_ms_driver_init();
int module_mt_driver_init();
int module_wacom_driver_init();

struct input_handle;
void genode_evdev_event(struct input_handle *handle, unsigned int type, unsigned int code, int value);

struct usb_device;
extern int usb_get_configuration(struct usb_device *dev);
extern void usb_destroy_configuration(struct usb_device *dev);

struct usb_hcd { unsigned amd_resume_bug:1; };

bool usb_device_is_owned(struct usb_device *udev);

#include <legacy/lx_emul/extern_c_end.h>

#endif /* _SRC__DRIVERS__USB_HID__LX_EMUL_H_ */
